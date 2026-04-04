// MotbinData.cpp -- binary parser for moveset.motbin
// Field offsets based on OldTool2 (TekkenMovesetExtractor TK8) t8_offsetTable.
// Move struct size = 0x448 bytes (FILE format, NOT in-memory).
#include "moveset/data/MotbinData.h"
#include "moveset/labels/LabelDB.h"
#include "GameStatic.h"
#include <windows.h>
#include <cstring>
#include <string>
#include <functional>

// -------------------------------------------------------------
//  Helpers
// -------------------------------------------------------------

static std::wstring Utf8ToWide(const std::string& s)
{
    if (s.empty()) return {};
    int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    std::wstring w(len > 0 ? len - 1 : 0, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &w[0], len);
    return w;
}

static std::vector<uint8_t> ReadFileBytes(const std::wstring& path)
{
    HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ,
                           nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return {};

    LARGE_INTEGER sz = {};
    GetFileSizeEx(h, &sz);

    if (sz.QuadPart == 0 || sz.QuadPart > 256LL * 1024 * 1024)
    {
        CloseHandle(h);
        return {};
    }

    std::vector<uint8_t> buf(static_cast<size_t>(sz.QuadPart));
    DWORD read = 0;
    ReadFile(h, buf.data(), static_cast<DWORD>(buf.size()), &read, nullptr);
    CloseHandle(h);

    if (static_cast<size_t>(read) != buf.size()) return {};
    return buf;
}

// Bounds-checked field reader -- returns zero-initialised T on out-of-bounds
template<typename T>
static T ReadAt(const uint8_t* base, size_t capacity, size_t offset)
{
    T val = {};
    if (offset + sizeof(T) <= capacity)
        memcpy(&val, base + offset, sizeof(T));
    return val;
}

// -------------------------------------------------------------
//  Index expansion -- converts index-format motbin to file-offset
//  format so all downstream parsers can work with direct offsets.
//
//  moveset.motbin is saved by the extractor in index format:
//    - Header block ptrs: (block_file_offset - 0x318)  (BASE-relative)
//    - Element pointer fields: element index into target block
//
//  ExpandIndexes reverses this:
//    - Header ptrs:  stored_val + 0x318  -> file offset
//    - Element ptrs: tgtBlockBase + idx * tgtStride  -> file offset
// -------------------------------------------------------------

static constexpr size_t kMotbinBase = 0x318;

struct ExpandPtrField {
    size_t elemOff;       // byte offset within element
    size_t tgtHdrPtrOff;  // header offset where target block's ptr is stored
    size_t tgtStride;     // element size of target block
};
struct ExpandBlockDesc {
    size_t ptrOff;   // header ptr offset of this block
    size_t cntOff;   // header cnt offset of this block
    size_t stride;   // element size of this block
    int    numPtrs;
    ExpandPtrField ptrs[8];
};

static const ExpandBlockDesc kExpandBlocks[] = {
    // reaction_list: 7 pushback pointers at +0x00..+0x30; +0x38 is front_direction (not a ptr)
    { 0x168, 0x178, 0x70, 7, {
        {0x00,0x1B0,0x10},{0x08,0x1B0,0x10},{0x10,0x1B0,0x10},{0x18,0x1B0,0x10},
        {0x20,0x1B0,0x10},{0x28,0x1B0,0x10},{0x30,0x1B0,0x10},
    }},
    { 0x180, 0x188, 0x14, 0, {} },  // requirements: no inner pointers
    // hit_conditions: req @0x00, reaction_list @0x10
    { 0x190, 0x198, 0x18, 2, { {0x00,0x180,0x14}, {0x10,0x168,0x70} }},
    // projectile: hit_cond @0x90, cancel @0x98
    { 0x1A0, 0x1A8, 0xE0, 2, { {0x90,0x190,0x18}, {0x98,0x1D0,0x28} }},
    // pushback: pushback_extra @0x08
    { 0x1B0, 0x1B8, 0x10, 1, { {0x08,0x1C0,0x02} }},
    { 0x1C0, 0x1C8, 0x02, 0, {} },  // pushback_extra: no inner pointers
    // cancel: req @0x08, cancel_extra @0x10
    { 0x1D0, 0x1D8, 0x28, 2, { {0x08,0x180,0x14}, {0x10,0x1F0,0x04} }},
    { 0x1E0, 0x1E8, 0x28, 2, { {0x08,0x180,0x14}, {0x10,0x1F0,0x04} }},  // group_cancel
    { 0x1F0, 0x1F8, 0x04, 0, {} },  // cancel_extra
    // extra_props (timed): req @0x08
    { 0x200, 0x208, 0x28, 1, { {0x08,0x180,0x14} }},
    // start/end props: req @0x00
    { 0x210, 0x218, 0x20, 1, { {0x00,0x180,0x14} }},
    { 0x220, 0x228, 0x20, 1, { {0x00,0x180,0x14} }},
    // moves: cancel @0x98, hitcond @0x110, voice @0x130, exprop @0x138, sprop @0x140, eprop @0x148
    // cancel2_addr @0xA0 is always 0 in file format (game-internal); not expanded.
    { 0x230, 0x238, 0x448, 6, {
        {0x98, 0x1D0,0x28}, {0x110,0x190,0x18},
        {0x130,0x240,0x0C}, {0x138,0x200,0x28},
        {0x140,0x210,0x20}, {0x148,0x220,0x20},
    }},
    { 0x240, 0x248, 0x0C, 0, {} },  // voiceclip
    // input_sequence: input_extra @0x08
    { 0x250, 0x258, 0x10, 1, { {0x08,0x260,0x08} }},
    { 0x260, 0x268, 0x08, 0, {} },  // input_extra
    { 0x270, 0x278, 0x04, 0, {} },  // parry
    { 0x280, 0x288, 0x0C, 0, {} },  // throw_extra
    // throws: throw_extra @0x08
    { 0x290, 0x298, 0x10, 1, { {0x08,0x280,0x0C} }},
    // dialogue: req @0x08  (0x10 is audio ID, not a block ptr)
    { 0x2A0, 0x2A8, 0x18, 1, { {0x08,0x180,0x14} }},
};

static const size_t kExpandHdrPtrs[] = {
    0x168,0x180,0x190,0x1A0,0x1B0,0x1C0,0x1D0,0x1E0,
    0x1F0,0x200,0x210,0x220,0x230,0x240,0x250,0x260,
    0x270,0x280,0x290,0x2A0,
};

// Expand index-format motbin to file-offset format in-place.
static void ExpandIndexes(std::vector<uint8_t>& v)
{
    uint8_t*     b  = v.data();
    const size_t sz = v.size();
    if (sz < kMotbinBase) return;

    // Pass 1: header block ptrs: BASE-relative -> file offset
    // NOTE: A BASE-relative value of 0 is valid -- it means the block starts at
    // exactly offset kMotbinBase (0x318) in the file (i.e. the first data block).
    // We must NOT skip val==0; doing so would leave that ptr at 0, causing the
    // block to be silently dropped during parsing (e.g. reaction_list in ant.motbin).
    for (size_t o : kExpandHdrPtrs) {
        if (o + 8 > sz) continue;
        uint64_t val; memcpy(&val, b + o, 8);
        val += kMotbinBase;
        memcpy(b + o, &val, 8);
    }

    // Pass 2: element pointer fields: index -> file offset
    for (const ExpandBlockDesc& bd : kExpandBlocks) {
        if (bd.numPtrs == 0) continue;
        if (bd.ptrOff + 8 > sz || bd.cntOff + 8 > sz) continue;
        uint64_t blockOff, cnt;
        memcpy(&blockOff, b + bd.ptrOff, 8);  // file offset after pass 1
        memcpy(&cnt,      b + bd.cntOff, 8);
        if (!blockOff || !cnt || blockOff + cnt * bd.stride > sz) continue;

        for (uint64_t i = 0; i < cnt; ++i) {
            size_t elem = static_cast<size_t>(blockOff + i * bd.stride);
            for (int p = 0; p < bd.numPtrs; ++p) {
                const ExpandPtrField& pf = bd.ptrs[p];
                size_t fOff = elem + pf.elemOff;
                if (fOff + 8 > sz) continue;
                uint64_t idx; memcpy(&idx, b + fOff, 8);
                if (idx == 0xFFFFFFFFFFFFFFFFULL) continue;  // index -1 = null pointer
                if (pf.tgtHdrPtrOff + 8 > sz) continue;
                uint64_t tgtBase; memcpy(&tgtBase, b + pf.tgtHdrPtrOff, 8);
                if (!tgtBase) continue;
                uint64_t fileOff = tgtBase + idx * pf.tgtStride;
                memcpy(b + fOff, &fileOff, 8);
            }
        }
    }
}

// -------------------------------------------------------------
//  XOR_KEYS decryption for index-format encrypted move fields.
//  Layout: 8 ? uint32 at blockOff+0 .. blockOff+0x1C
//  Slot [moveIdx%8] holds (rawValue ^ kXorKeys[slot]).
// -------------------------------------------------------------

static const uint32_t kXorKeys[8] = {
    0x964f5b9eU, 0xd88448a2U, 0xa84b71e0U, 0xa27d5221U,
    0x9b81329fU, 0xadfb76c8U, 0x7def1f1cU, 0x7ee2bc2cU,
};

static uint32_t XorDecryptMoveField(const uint8_t* mb, size_t blockOff, uint32_t moveIdx)
{
    uint32_t slot = moveIdx % 8;
    uint32_t enc;
    memcpy(&enc, mb + blockOff + slot * 4, 4);
    return enc ^ kXorKeys[slot];
}

// -------------------------------------------------------------
//  .motbin header constants (file format)
// -------------------------------------------------------------
static constexpr size_t kHdr_Signature  = 0x08;
static constexpr size_t kHdr_MovesPtr   = 0x230;
static constexpr size_t kHdr_MovesCount = 0x238;

// -------------------------------------------------------------
//  Move struct file offsets (OldTool2 t8_offsetTable)
//  Total size: 0x448 bytes
// -------------------------------------------------------------
static constexpr size_t kMove_Size = 0x448;

static constexpr size_t kMove_EncNameKey    = 0x00;
static constexpr size_t kMove_EncAnimKey    = 0x20;
static constexpr size_t kMove_AnimAddrEnc1  = 0x50;
static constexpr size_t kMove_AnimAddrEnc2  = 0x54;
static constexpr size_t kMove_EncVuln       = 0x58;
static constexpr size_t kMove_EncHitlevel   = 0x78;

static constexpr size_t kMove_CancelAddr    = 0x98;
static constexpr size_t kMove_Cancel2Addr   = 0xA0;
static constexpr size_t kMove_U1            = 0xA8;
static constexpr size_t kMove_U2            = 0xB0;
static constexpr size_t kMove_U3            = 0xB8;
static constexpr size_t kMove_U4            = 0xC0;
static constexpr size_t kMove_U6            = 0xC8;
static constexpr size_t kMove_Transition    = 0xCC;
static constexpr size_t kMove_0xCE          = 0xCE;

static constexpr size_t kMove_EncCharId     = 0xD0;
static constexpr size_t kMove_EncOrdinalId  = 0xF0;

static constexpr size_t kMove_HitCondAddr   = 0x110;
static constexpr size_t kMove_0x118         = 0x118;
static constexpr size_t kMove_0x11C         = 0x11C;
static constexpr size_t kMove_AnimLen       = 0x120;
static constexpr size_t kMove_AirborneStart = 0x124;
static constexpr size_t kMove_AirborneEnd   = 0x128;
static constexpr size_t kMove_GroundFall    = 0x12C;
static constexpr size_t kMove_VoicelipAddr  = 0x130;
static constexpr size_t kMove_ExtraPropAddr = 0x138;
static constexpr size_t kMove_StartPropAddr = 0x140;
static constexpr size_t kMove_EndPropAddr   = 0x148;
static constexpr size_t kMove_U15           = 0x150;
static constexpr size_t kMove_0x154         = 0x154;
static constexpr size_t kMove_Startup       = 0x158;
static constexpr size_t kMove_Recovery      = 0x15C;

static constexpr size_t kMove_Hitbox0       = 0x160;
static constexpr size_t kMove_HitboxStride  = 0x30;

static constexpr size_t kMove_Collision     = 0x2E0;
static constexpr size_t kMove_Distance      = 0x2E2;

// -------------------------------------------------------------
//  Sub-struct sizes (TK8)
// -------------------------------------------------------------
static constexpr size_t kCancel_Size       = 0x28;
static constexpr size_t kHitCond_Size      = 0x18;
static constexpr size_t kExtraProp_Size    = 0x28;
static constexpr size_t kOtherMoveProp_Size = 0x20;  // start/end properties

// kReqListEnd is now loaded from GameStatic.ini
#define kReqListEnd (GameStatic::Get().data.reqListEnd)

// (Sub-struct parsers removed -- all blocks are now parsed globally in LoadMotbin)

// -------------------------------------------------------------
//  Public loader
// -------------------------------------------------------------

MotbinData LoadMotbin(const std::string& folderPath)
{
    MotbinData result;

    std::string path = folderPath;
    if (!path.empty() && path.back() != '\\' && path.back() != '/')
        path += '\\';
    path += "moveset.motbin";

    auto bytes = ReadFileBytes(Utf8ToWide(path));
    if (bytes.empty())
    {
        result.errorMsg = "moveset.motbin not found or unreadable.";
        return result;
    }

    result.folderPath = folderPath;
    result.rawBytes   = bytes;  // save before expansion for later patching

    // File is index-format (saved by extractor via ExportLoaderBin).
    // Expand BASE-relative header ptrs and element indexes to file offsets.
    ExpandIndexes(bytes);

    const uint8_t* buf = bytes.data();
    const size_t   cap = bytes.size();

    // Validate TEK signature
    if (cap < 0x240 ||
        buf[kHdr_Signature]     != 'T' ||
        buf[kHdr_Signature + 1] != 'E' ||
        buf[kHdr_Signature + 2] != 'K')
    {
        result.errorMsg = "Invalid file signature (expected 'TEK').";
        return result;
    }

    const uint64_t movesOffset = ReadAt<uint64_t>(buf, cap, kHdr_MovesPtr);
    const uint64_t movesCount  = ReadAt<uint64_t>(buf, cap, kHdr_MovesCount);

    if (movesOffset == 0 || movesCount == 0 ||
        movesOffset >= cap ||
        movesCount > 32768 ||
        movesOffset + movesCount * kMove_Size > cap)
    {
        result.errorMsg = "Move array out of file bounds.";
        return result;
    }

    result.moveCount = static_cast<uint32_t>(movesCount);
    result.moves.reserve(static_cast<size_t>(movesCount));

    // -- original_aliases (header 0x30..0xA7, 60 ? uint16) ------------------
    // genericId - 0x8000 = index -> real move id
    result.originalAliases.resize(60);
    for (size_t ai = 0; ai < 60; ++ai)
        result.originalAliases[ai] = ReadAt<uint16_t>(buf, cap, 0x30 + ai * 2);

    // -- reverse map: move index -> first generic ID that references it --------
    result.moveToGenericId.assign(result.moveCount, 0u);
    for (size_t ai = 0; ai < result.originalAliases.size(); ++ai)
    {
        const uint32_t mid = result.originalAliases[ai];
        if (mid < result.moveCount && result.moveToGenericId[mid] == 0)
            result.moveToGenericId[mid] = static_cast<uint32_t>(0x8000u + ai);
    }

    // -- Build global requirement block -----------------------
    const uint64_t reqBase  = ReadAt<uint64_t>(buf, cap, 0x180);
    const uint64_t reqCount = ReadAt<uint64_t>(buf, cap, 0x188);
    const uint64_t reacBase = ReadAt<uint64_t>(buf, cap, 0x168);

    if (reqBase != 0 && reqCount > 0 && reqCount <= 0x100000)
    {
        result.requirementBlock.reserve(static_cast<size_t>(reqCount));
        for (uint64_t r = 0; r < reqCount; ++r)
        {
            size_t roff = static_cast<size_t>(reqBase) + static_cast<size_t>(r) * 0x14;
            if (roff + 0x14 > cap) break;
            ParsedRequirement pr;
            pr.req    = ReadAt<uint32_t>(buf, cap, roff + 0x00);
            pr.param  = ReadAt<uint32_t>(buf, cap, roff + 0x04);
            pr.param2 = ReadAt<uint32_t>(buf, cap, roff + 0x08);
            pr.param3 = ReadAt<uint32_t>(buf, cap, roff + 0x0C);
            pr.param4 = ReadAt<uint32_t>(buf, cap, roff + 0x10);
            result.requirementBlock.push_back(pr);
        }
    }

    // -- cancelExtraBlock ----------------------------------------
    {
        uint64_t base = ReadAt<uint64_t>(buf, cap, 0x1F0);
        uint64_t cnt  = ReadAt<uint64_t>(buf, cap, 0x1F8);
        if (base && cnt && cnt <= 0x100000)
        {
            result.cancelExtraBlock.reserve((size_t)cnt);
            for (uint64_t k = 0; k < cnt; ++k)
            {
                size_t o = (size_t)base + (size_t)k * 4;
                if (o + 4 > cap) break;
                result.cancelExtraBlock.push_back(ReadAt<uint32_t>(buf, cap, o));
            }
        }
    }

    // -- pushbackExtraBlock ---------------------------------------
    {
        uint64_t base = ReadAt<uint64_t>(buf, cap, 0x1C0);
        uint64_t cnt  = ReadAt<uint64_t>(buf, cap, 0x1C8);
        if (base && cnt && cnt <= 0x100000)
        {
            result.pushbackExtraBlock.reserve((size_t)cnt);
            for (uint64_t k = 0; k < cnt; ++k)
            {
                size_t o = (size_t)base + (size_t)k * 2;
                if (o + 2 > cap) break;
                ParsedPushbackExtra pe;
                pe.value = ReadAt<uint16_t>(buf, cap, o);
                result.pushbackExtraBlock.push_back(pe);
            }
        }
    }

    // -- pushbackBlock --------------------------------------------
    {
        uint64_t pbBase = ReadAt<uint64_t>(buf, cap, 0x1B0);
        uint64_t pbCnt  = ReadAt<uint64_t>(buf, cap, 0x1B8);
        uint64_t peBase = ReadAt<uint64_t>(buf, cap, 0x1C0);
        if (pbBase && pbCnt && pbCnt <= 0x100000)
        {
            result.pushbackBlock.reserve((size_t)pbCnt);
            for (uint64_t k = 0; k < pbCnt; ++k)
            {
                size_t o = (size_t)pbBase + (size_t)k * 0x10;
                if (o + 0x10 > cap) break;
                ParsedPushback pb;
                pb.val1       = ReadAt<uint16_t>(buf, cap, o + 0x00);
                pb.val2       = ReadAt<uint16_t>(buf, cap, o + 0x02);
                pb.val3       = ReadAt<uint32_t>(buf, cap, o + 0x04);
                pb.extra_addr = ReadAt<uint64_t>(buf, cap, o + 0x08);
                if (pb.extra_addr != 0 && pb.extra_addr < cap &&
                    peBase != 0 && pb.extra_addr >= peBase)
                    pb.pushback_extra_idx = (uint32_t)((pb.extra_addr - peBase) / 2);
                result.pushbackBlock.push_back(pb);
            }
        }
    }

    // -- reactionListBlock ----------------------------------------
    {
        uint64_t base   = ReadAt<uint64_t>(buf, cap, 0x168);
        uint64_t cnt    = ReadAt<uint64_t>(buf, cap, 0x178);
        uint64_t pbBase = ReadAt<uint64_t>(buf, cap, 0x1B0);
        if (base && cnt && cnt <= 0x100000)
        {
            result.reactionListBlock.reserve((size_t)cnt);
            for (uint64_t k = 0; k < cnt; ++k)
            {
                size_t o = (size_t)base + (size_t)k * 0x70;
                if (o + 0x70 > cap) break;
                ParsedReactionList rl;
                // 7 pushback pointers (already expanded to file offsets by ExpandIndexes)
                for (int p = 0; p < 7; ++p)
                {
                    rl.pushback_addr[p] = ReadAt<uint64_t>(buf, cap, o + p * 8);
                    rl.pushback_idx[p]  = 0xFFFFFFFF;
                    if (rl.pushback_addr[p] != 0 && rl.pushback_addr[p] < cap &&
                        pbBase != 0 && rl.pushback_addr[p] >= pbBase)
                        rl.pushback_idx[p] = (uint32_t)((rl.pushback_addr[p] - pbBase) / 0x10);
                }
                // directions
                rl.front_direction      = ReadAt<uint16_t>(buf, cap, o + 0x38);
                rl.back_direction       = ReadAt<uint16_t>(buf, cap, o + 0x3A);
                rl.left_side_direction  = ReadAt<uint16_t>(buf, cap, o + 0x3C);
                rl.right_side_direction = ReadAt<uint16_t>(buf, cap, o + 0x3E);
                rl.front_ch_direction   = ReadAt<uint16_t>(buf, cap, o + 0x40);
                rl.downed_direction     = ReadAt<uint16_t>(buf, cap, o + 0x42);
                // rotations
                rl.front_rotation       = ReadAt<uint16_t>(buf, cap, o + 0x44);
                rl.back_rotation        = ReadAt<uint16_t>(buf, cap, o + 0x46);
                rl.left_side_rotation   = ReadAt<uint16_t>(buf, cap, o + 0x48);
                rl.right_side_rotation  = ReadAt<uint16_t>(buf, cap, o + 0x4A);
                rl.vertical_pushback    = ReadAt<uint16_t>(buf, cap, o + 0x4C);
                rl.downed_rotation      = ReadAt<uint16_t>(buf, cap, o + 0x4E);
                // move indexes
                rl.standing             = ReadAt<uint16_t>(buf, cap, o + 0x50);
                rl.crouch               = ReadAt<uint16_t>(buf, cap, o + 0x52);
                rl.ch                   = ReadAt<uint16_t>(buf, cap, o + 0x54);
                rl.crouch_ch            = ReadAt<uint16_t>(buf, cap, o + 0x56);
                rl.left_side            = ReadAt<uint16_t>(buf, cap, o + 0x58);
                rl.left_side_crouch     = ReadAt<uint16_t>(buf, cap, o + 0x5A);
                rl.right_side           = ReadAt<uint16_t>(buf, cap, o + 0x5C);
                rl.right_side_crouch    = ReadAt<uint16_t>(buf, cap, o + 0x5E);
                rl.back                 = ReadAt<uint16_t>(buf, cap, o + 0x60);
                rl.back_crouch          = ReadAt<uint16_t>(buf, cap, o + 0x62);
                rl.block                = ReadAt<uint16_t>(buf, cap, o + 0x64);
                rl.crouch_block         = ReadAt<uint16_t>(buf, cap, o + 0x66);
                rl.wallslump            = ReadAt<uint16_t>(buf, cap, o + 0x68);
                rl.downed               = ReadAt<uint16_t>(buf, cap, o + 0x6A);
                result.reactionListBlock.push_back(rl);
            }
        }
    }

    // -- hitConditionBlock ----------------------------------------
    {
        uint64_t base   = ReadAt<uint64_t>(buf, cap, 0x190);
        uint64_t cnt    = ReadAt<uint64_t>(buf, cap, 0x198);
        uint64_t rlBase = ReadAt<uint64_t>(buf, cap, 0x168);
        if (base && cnt && cnt <= 0x100000)
        {
            result.hitConditionBlock.reserve((size_t)cnt);
            for (uint64_t k = 0; k < cnt; ++k)
            {
                size_t o = (size_t)base + (size_t)k * 0x18;
                if (o + 0x18 > cap) break;
                ParsedHitCondition h;
                h.requirement_addr   = ReadAt<uint64_t>(buf, cap, o + 0x00);
                h.damage             = ReadAt<uint32_t>(buf, cap, o + 0x08);
                h._0x0C              = ReadAt<uint32_t>(buf, cap, o + 0x0C);
                h.reaction_list_addr = ReadAt<uint64_t>(buf, cap, o + 0x10);
                h.req_list_idx       = 0xFFFFFFFF;
                h.reaction_list_idx  = 0xFFFFFFFF;
                if (h.requirement_addr && h.requirement_addr < cap && reqBase && h.requirement_addr >= reqBase)
                    h.req_list_idx = (uint32_t)((h.requirement_addr - reqBase) / 0x14);
                if (h.reaction_list_addr && h.reaction_list_addr < cap && rlBase && h.reaction_list_addr >= rlBase)
                    h.reaction_list_idx = (uint32_t)((h.reaction_list_addr - rlBase) / 0x70);
                result.hitConditionBlock.push_back(h);
            }
        }
    }

    // -- cancelBlock ----------------------------------------------
    {
        uint64_t base   = ReadAt<uint64_t>(buf, cap, 0x1D0);
        uint64_t cnt    = ReadAt<uint64_t>(buf, cap, 0x1D8);
        uint64_t exBase = ReadAt<uint64_t>(buf, cap, 0x1F0);
        if (base && cnt && cnt <= 0x200000)
        {
            result.cancelBlock.reserve((size_t)cnt);
            for (uint64_t k = 0; k < cnt; ++k)
            {
                size_t o = (size_t)base + (size_t)k * 0x28;
                if (o + 0x28 > cap) break;
                ParsedCancel c;
                c.command            = ReadAt<uint64_t>(buf, cap, o + 0x00);
                c.requirement_addr   = ReadAt<uint64_t>(buf, cap, o + 0x08);
                c.extradata_addr     = ReadAt<uint64_t>(buf, cap, o + 0x10);
                c.frame_window_start = ReadAt<uint32_t>(buf, cap, o + 0x18);
                c.frame_window_end   = ReadAt<uint32_t>(buf, cap, o + 0x1C);
                c.starting_frame     = ReadAt<uint32_t>(buf, cap, o + 0x20);
                c.move_id            = ReadAt<uint16_t>(buf, cap, o + 0x24);
                c.cancel_option      = ReadAt<uint16_t>(buf, cap, o + 0x26);
                c.req_list_idx       = 0xFFFFFFFF;
                c.extradata_idx      = 0xFFFFFFFF;
                c.extradata_value    = 0xFFFFFFFF;
                if (c.requirement_addr && c.requirement_addr < cap && reqBase && c.requirement_addr >= reqBase)
                    c.req_list_idx = (uint32_t)((c.requirement_addr - reqBase) / 0x14);
                if (c.extradata_addr && c.extradata_addr < cap)
                {
                    if (exBase && c.extradata_addr >= exBase)
                        c.extradata_idx = (uint32_t)((c.extradata_addr - exBase) / 4);
                    c.extradata_value = ReadAt<uint32_t>(buf, cap, (size_t)c.extradata_addr);
                }
                result.cancelBlock.push_back(c);
            }
        }
    }

    // -- groupCancelBlock -----------------------------------------
    {
        uint64_t base   = ReadAt<uint64_t>(buf, cap, 0x1E0);
        uint64_t cnt    = ReadAt<uint64_t>(buf, cap, 0x1E8);
        uint64_t exBase = ReadAt<uint64_t>(buf, cap, 0x1F0);
        if (base && cnt && cnt <= 0x200000)
        {
            result.groupCancelBlock.reserve((size_t)cnt);
            for (uint64_t k = 0; k < cnt; ++k)
            {
                size_t o = (size_t)base + (size_t)k * 0x28;
                if (o + 0x28 > cap) break;
                ParsedCancel c;
                c.command            = ReadAt<uint64_t>(buf, cap, o + 0x00);
                c.requirement_addr   = ReadAt<uint64_t>(buf, cap, o + 0x08);
                c.extradata_addr     = ReadAt<uint64_t>(buf, cap, o + 0x10);
                c.frame_window_start = ReadAt<uint32_t>(buf, cap, o + 0x18);
                c.frame_window_end   = ReadAt<uint32_t>(buf, cap, o + 0x1C);
                c.starting_frame     = ReadAt<uint32_t>(buf, cap, o + 0x20);
                c.move_id            = ReadAt<uint16_t>(buf, cap, o + 0x24);
                c.cancel_option      = ReadAt<uint16_t>(buf, cap, o + 0x26);
                c.req_list_idx       = 0xFFFFFFFF;
                c.extradata_idx      = 0xFFFFFFFF;
                c.extradata_value    = 0xFFFFFFFF;
                if (c.requirement_addr && c.requirement_addr < cap && reqBase && c.requirement_addr >= reqBase)
                    c.req_list_idx = (uint32_t)((c.requirement_addr - reqBase) / 0x14);
                if (c.extradata_addr && c.extradata_addr < cap)
                {
                    if (exBase && c.extradata_addr >= exBase)
                        c.extradata_idx = (uint32_t)((c.extradata_addr - exBase) / 4);
                    c.extradata_value = ReadAt<uint32_t>(buf, cap, (size_t)c.extradata_addr);
                }
                result.groupCancelBlock.push_back(c);
            }
        }
    }

    // -- extraPropBlock -------------------------------------------
    {
        uint64_t base = ReadAt<uint64_t>(buf, cap, 0x200);
        uint64_t cnt  = ReadAt<uint64_t>(buf, cap, 0x208);
        if (base && cnt && cnt <= 0x100000)
        {
            result.extraPropBlock.reserve((size_t)cnt);
            for (uint64_t k = 0; k < cnt; ++k)
            {
                size_t o = (size_t)base + (size_t)k * 0x28;
                if (o + 0x28 > cap) break;
                ParsedExtraProp e;
                e.type             = ReadAt<uint32_t>(buf, cap, o + 0x00);
                e._0x4             = ReadAt<uint32_t>(buf, cap, o + 0x04);
                e.requirement_addr = ReadAt<uint64_t>(buf, cap, o + 0x08);
                e.id               = ReadAt<uint32_t>(buf, cap, o + 0x10);
                e.value            = ReadAt<uint32_t>(buf, cap, o + 0x14);
                e.value2           = ReadAt<uint32_t>(buf, cap, o + 0x18);
                e.value3           = ReadAt<uint32_t>(buf, cap, o + 0x1C);
                e.value4           = ReadAt<uint32_t>(buf, cap, o + 0x20);
                e.value5           = ReadAt<uint32_t>(buf, cap, o + 0x24);
                e.req_list_idx     = 0xFFFFFFFF;
                if (e.requirement_addr && e.requirement_addr < cap && reqBase && e.requirement_addr >= reqBase)
                    e.req_list_idx = (uint32_t)((e.requirement_addr - reqBase) / 0x14);
                result.extraPropBlock.push_back(e);
            }
        }
    }

    // -- startPropBlock -------------------------------------------
    {
        uint64_t base = ReadAt<uint64_t>(buf, cap, 0x210);
        uint64_t cnt  = ReadAt<uint64_t>(buf, cap, 0x218);
        if (base && cnt && cnt <= 0x100000)
        {
            result.startPropBlock.reserve((size_t)cnt);
            for (uint64_t k = 0; k < cnt; ++k)
            {
                size_t o = (size_t)base + (size_t)k * 0x20;
                if (o + 0x20 > cap) break;
                ParsedExtraProp e = {};
                e.requirement_addr = ReadAt<uint64_t>(buf, cap, o + 0x00);
                e.id               = ReadAt<uint32_t>(buf, cap, o + 0x08);
                e.value            = ReadAt<uint32_t>(buf, cap, o + 0x0C);
                e.value2           = ReadAt<uint32_t>(buf, cap, o + 0x10);
                e.value3           = ReadAt<uint32_t>(buf, cap, o + 0x14);
                e.value4           = ReadAt<uint32_t>(buf, cap, o + 0x18);
                e.value5           = ReadAt<uint32_t>(buf, cap, o + 0x1C);
                e.req_list_idx     = 0xFFFFFFFF;
                if (e.requirement_addr && e.requirement_addr < cap && reqBase && e.requirement_addr >= reqBase)
                    e.req_list_idx = (uint32_t)((e.requirement_addr - reqBase) / 0x14);
                result.startPropBlock.push_back(e);
            }
        }
    }

    // -- endPropBlock ---------------------------------------------
    {
        uint64_t base = ReadAt<uint64_t>(buf, cap, 0x220);
        uint64_t cnt  = ReadAt<uint64_t>(buf, cap, 0x228);
        if (base && cnt && cnt <= 0x100000)
        {
            result.endPropBlock.reserve((size_t)cnt);
            for (uint64_t k = 0; k < cnt; ++k)
            {
                size_t o = (size_t)base + (size_t)k * 0x20;
                if (o + 0x20 > cap) break;
                ParsedExtraProp e = {};
                e.requirement_addr = ReadAt<uint64_t>(buf, cap, o + 0x00);
                e.id               = ReadAt<uint32_t>(buf, cap, o + 0x08);
                e.value            = ReadAt<uint32_t>(buf, cap, o + 0x0C);
                e.value2           = ReadAt<uint32_t>(buf, cap, o + 0x10);
                e.value3           = ReadAt<uint32_t>(buf, cap, o + 0x14);
                e.value4           = ReadAt<uint32_t>(buf, cap, o + 0x18);
                e.value5           = ReadAt<uint32_t>(buf, cap, o + 0x1C);
                e.req_list_idx     = 0xFFFFFFFF;
                if (e.requirement_addr && e.requirement_addr < cap && reqBase && e.requirement_addr >= reqBase)
                    e.req_list_idx = (uint32_t)((e.requirement_addr - reqBase) / 0x14);
                result.endPropBlock.push_back(e);
            }
        }
    }

    // -- inputBlock (hdr 0x260, stride 0x08) ----------------------
    {
        uint64_t base = ReadAt<uint64_t>(buf, cap, 0x260);
        uint64_t cnt  = ReadAt<uint64_t>(buf, cap, 0x268);
        if (base && cnt && cnt <= 0x100000)
        {
            result.inputBlock.reserve((size_t)cnt);
            for (uint64_t k = 0; k < cnt; ++k)
            {
                size_t o = (size_t)base + (size_t)k * 0x08;
                if (o + 0x08 > cap) break;
                ParsedInput inp;
                inp.command = ReadAt<uint64_t>(buf, cap, o);
                result.inputBlock.push_back(inp);
            }
        }
    }

    // -- inputSequenceBlock (hdr 0x250, stride 0x10) --------------
    {
        uint64_t base    = ReadAt<uint64_t>(buf, cap, 0x250);
        uint64_t cnt     = ReadAt<uint64_t>(buf, cap, 0x258);
        uint64_t inpBase = ReadAt<uint64_t>(buf, cap, 0x260);
        if (base && cnt && cnt <= 0x100000)
        {
            result.inputSequenceBlock.reserve((size_t)cnt);
            for (uint64_t k = 0; k < cnt; ++k)
            {
                size_t o = (size_t)base + (size_t)k * 0x10;
                if (o + 0x10 > cap) break;
                ParsedInputSequence is;
                is.input_window_frames = ReadAt<uint16_t>(buf, cap, o + 0x00);
                is.input_amount        = ReadAt<uint16_t>(buf, cap, o + 0x02);
                is._0x4                = ReadAt<uint32_t>(buf, cap, o + 0x04);
                is.inputs_addr         = ReadAt<uint64_t>(buf, cap, o + 0x08);
                is.input_start_idx     = 0xFFFFFFFF;
                if (is.inputs_addr && is.inputs_addr < cap &&
                    inpBase && is.inputs_addr >= inpBase)
                    is.input_start_idx = (uint32_t)((is.inputs_addr - inpBase) / 0x08);
                result.inputSequenceBlock.push_back(is);
            }
        }
    }

    // -- projectileBlock (hdr 0x1A0, stride 0xE0) -----------------
    {
        uint64_t base   = ReadAt<uint64_t>(buf, cap, 0x1A0);
        uint64_t cnt    = ReadAt<uint64_t>(buf, cap, 0x1A8);
        uint64_t hcBase = ReadAt<uint64_t>(buf, cap, 0x190);
        uint64_t cBase  = ReadAt<uint64_t>(buf, cap, 0x1D0);
        if (base && cnt && cnt <= 0x10000)
        {
            result.projectileBlock.reserve((size_t)cnt);
            for (uint64_t k = 0; k < cnt; ++k)
            {
                size_t o = (size_t)base + (size_t)k * 0xE0;
                if (o + 0xE0 > cap) break;
                ParsedProjectile p;
                for (int n = 0; n < 35; ++n)
                    p.u1[n] = ReadAt<uint32_t>(buf, cap, o + n * 4);
                p.hit_condition_addr = ReadAt<uint64_t>(buf, cap, o + 0x90);
                p.cancel_addr        = ReadAt<uint64_t>(buf, cap, o + 0x98);
                for (int n = 0; n < 16; ++n)
                    p.u2[n] = ReadAt<uint32_t>(buf, cap, o + 0xA0 + n * 4);
                p.hit_condition_idx = 0xFFFFFFFF;
                p.cancel_idx        = 0xFFFFFFFF;
                if (p.hit_condition_addr && p.hit_condition_addr < cap && hcBase && p.hit_condition_addr >= hcBase)
                    p.hit_condition_idx = (uint32_t)((p.hit_condition_addr - hcBase) / 0x18);
                if (p.cancel_addr && p.cancel_addr < cap && cBase && p.cancel_addr >= cBase)
                    p.cancel_idx = (uint32_t)((p.cancel_addr - cBase) / 0x28);
                result.projectileBlock.push_back(p);
            }
        }
    }

    // -- parryableMoveBlock (hdr 0x270, stride 0x04) --------------
    {
        uint64_t base = ReadAt<uint64_t>(buf, cap, 0x270);
        uint64_t cnt  = ReadAt<uint64_t>(buf, cap, 0x278);
        if (base && cnt && cnt <= 0x100000)
        {
            result.parryableMoveBlock.reserve((size_t)cnt);
            for (uint64_t k = 0; k < cnt; ++k)
            {
                size_t o = (size_t)base + (size_t)k * 0x04;
                if (o + 0x04 > cap) break;
                ParsedParryableMove pm;
                pm.value = ReadAt<uint32_t>(buf, cap, o);
                result.parryableMoveBlock.push_back(pm);
            }
        }
    }

    // -- throwExtraBlock (hdr 0x280, stride 0x0C) -----------------
    {
        uint64_t base = ReadAt<uint64_t>(buf, cap, 0x280);
        uint64_t cnt  = ReadAt<uint64_t>(buf, cap, 0x288);
        if (base && cnt && cnt <= 0x100000)
        {
            result.throwExtraBlock.reserve((size_t)cnt);
            for (uint64_t k = 0; k < cnt; ++k)
            {
                size_t o = (size_t)base + (size_t)k * 0x0C;
                if (o + 0x0C > cap) break;
                ParsedThrowExtra te;
                te.pick_probability       = ReadAt<uint32_t>(buf, cap, o + 0x00);
                te.camera_type            = ReadAt<uint16_t>(buf, cap, o + 0x04);
                te.left_side_camera_data  = ReadAt<uint16_t>(buf, cap, o + 0x06);
                te.right_side_camera_data = ReadAt<uint16_t>(buf, cap, o + 0x08);
                te.additional_rotation    = ReadAt<uint16_t>(buf, cap, o + 0x0A);
                result.throwExtraBlock.push_back(te);
            }
        }
    }

    // -- throwBlock (hdr 0x290, stride 0x10) ----------------------
    {
        uint64_t base   = ReadAt<uint64_t>(buf, cap, 0x290);
        uint64_t cnt    = ReadAt<uint64_t>(buf, cap, 0x298);
        uint64_t teBase = ReadAt<uint64_t>(buf, cap, 0x280);
        if (base && cnt && cnt <= 0x100000)
        {
            result.throwBlock.reserve((size_t)cnt);
            for (uint64_t k = 0; k < cnt; ++k)
            {
                size_t o = (size_t)base + (size_t)k * 0x10;
                if (o + 0x10 > cap) break;
                ParsedThrow t;
                t.side           = ReadAt<uint64_t>(buf, cap, o + 0x00);
                t.throwextra_addr = ReadAt<uint64_t>(buf, cap, o + 0x08);
                t.throwextra_idx = 0xFFFFFFFF;
                if (t.throwextra_addr != 0 && t.throwextra_addr < cap &&
                    teBase != 0 && t.throwextra_addr >= teBase)
                    t.throwextra_idx = (uint32_t)((t.throwextra_addr - teBase) / 0x0C);
                result.throwBlock.push_back(t);
            }
        }
    }

    // -- dialogueBlock (hdr 0x2A0, stride 0x18) -------------------
    {
        uint64_t base    = ReadAt<uint64_t>(buf, cap, 0x2A0);
        uint64_t cnt     = ReadAt<uint64_t>(buf, cap, 0x2A8);
        uint64_t reqBase = ReadAt<uint64_t>(buf, cap, 0x180);
        if (base && cnt && cnt <= 0x100000)
        {
            result.dialogueBlock.reserve((size_t)cnt);
            for (uint64_t k = 0; k < cnt; ++k)
            {
                size_t o = (size_t)base + (size_t)k * 0x18;
                if (o + 0x18 > cap) break;
                ParsedDialogue d;
                d.type             = ReadAt<uint16_t>(buf, cap, o + 0x00);
                d.id               = ReadAt<uint16_t>(buf, cap, o + 0x02);
                d._0x4             = ReadAt<uint32_t>(buf, cap, o + 0x04);
                d.requirement_addr = ReadAt<uint64_t>(buf, cap, o + 0x08);
                d.voiceclip_key    = ReadAt<uint32_t>(buf, cap, o + 0x10);
                d.facial_anim_idx  = ReadAt<uint32_t>(buf, cap, o + 0x14);
                d.req_list_idx     = 0xFFFFFFFF;
                if (d.requirement_addr && d.requirement_addr < cap &&
                    reqBase && d.requirement_addr >= reqBase)
                    d.req_list_idx = (uint32_t)((d.requirement_addr - reqBase) / 0x14);
                result.dialogueBlock.push_back(d);
            }
        }
    }

    // -- voiceclipBlock -------------------------------------------
    {
        uint64_t base = ReadAt<uint64_t>(buf, cap, 0x240);
        uint64_t cnt  = ReadAt<uint64_t>(buf, cap, 0x248);
        if (base && cnt && cnt <= 0x100000)
        {
            result.voiceclipBlock.reserve((size_t)cnt);
            for (uint64_t k = 0; k < cnt; ++k)
            {
                size_t o = (size_t)base + (size_t)k * 0x0C;
                if (o + 0x0C > cap) break;
                ParsedVoiceclip vc;
                vc.val1 = ReadAt<uint32_t>(buf, cap, o + 0x00);
                vc.val2 = ReadAt<uint32_t>(buf, cap, o + 0x04);
                vc.val3 = ReadAt<uint32_t>(buf, cap, o + 0x08);
                result.voiceclipBlock.push_back(vc);
            }
        }
    }

    for (uint64_t i = 0; i < movesCount; ++i)
    {
        const size_t   base = static_cast<size_t>(movesOffset + i * kMove_Size);
        const uint8_t* mb   = buf + base;

        ParsedMove m = {};

        // -- Encrypted blocks (raw storage) ------------------
        m.encrypted_name_key     = ReadAt<uint64_t>(mb, kMove_Size, kMove_EncNameKey);
        m.name_encryption_key    = ReadAt<uint64_t>(mb, kMove_Size, kMove_EncNameKey + 8);
        for (int k = 0; k < 4; ++k)
            m.name_related[k] = ReadAt<uint32_t>(mb, kMove_Size, kMove_EncNameKey + 16 + k * 4);

        m.encrypted_anim_key     = ReadAt<uint64_t>(mb, kMove_Size, kMove_EncAnimKey);
        m.anim_encryption_key    = ReadAt<uint64_t>(mb, kMove_Size, kMove_EncAnimKey + 8);
        for (int k = 0; k < 8; ++k)
            m.anim_related[k] = ReadAt<uint32_t>(mb, kMove_Size, kMove_EncAnimKey + 16 + k * 4);

        m.anmbin_body_idx     = ReadAt<uint32_t>(mb, kMove_Size, kMove_AnimAddrEnc1);
        m.anmbin_body_sub_idx = ReadAt<uint32_t>(mb, kMove_Size, kMove_AnimAddrEnc2);

        m.encrypted_vuln          = ReadAt<uint64_t>(mb, kMove_Size, kMove_EncVuln);
        m.vuln_encryption_key     = ReadAt<uint64_t>(mb, kMove_Size, kMove_EncVuln + 8);
        for (int k = 0; k < 4; ++k)
            m.vuln_related[k] = ReadAt<uint32_t>(mb, kMove_Size, kMove_EncVuln + 16 + k * 4);

        m.encrypted_hitlevel          = ReadAt<uint64_t>(mb, kMove_Size, kMove_EncHitlevel);
        m.hitlevel_encryption_key     = ReadAt<uint64_t>(mb, kMove_Size, kMove_EncHitlevel + 8);
        for (int k = 0; k < 4; ++k)
            m.hitlevel_related[k] = ReadAt<uint32_t>(mb, kMove_Size, kMove_EncHitlevel + 16 + k * 4);

        // -- Decrypt encrypted fields (XOR_KEYS format) ------
        uint32_t moveIdx = static_cast<uint32_t>(i);
        uint32_t nameKey = XorDecryptMoveField(mb, kMove_EncNameKey, moveIdx);
        m.name_key = nameKey;
        m.anim_key = XorDecryptMoveField(mb, kMove_EncAnimKey,  moveIdx);
        m.vuln     = XorDecryptMoveField(mb, kMove_EncVuln,     moveIdx);
        m.hitlevel = XorDecryptMoveField(mb, kMove_EncHitlevel, moveIdx);

        m.encrypted_ordinal_id2  = ReadAt<uint64_t>(mb, kMove_Size, kMove_EncCharId);
        m.ordinal_id2_enc_key    = ReadAt<uint64_t>(mb, kMove_Size, kMove_EncCharId + 8);
        for (int k = 0; k < 4; ++k)
            m.ordinal_id2_related[k] = ReadAt<uint32_t>(mb, kMove_Size, kMove_EncCharId + 16 + k * 4);

        m.encrypted_ordinal_id   = ReadAt<uint64_t>(mb, kMove_Size, kMove_EncOrdinalId);
        m.ordinal_encryption_key = ReadAt<uint64_t>(mb, kMove_Size, kMove_EncOrdinalId + 8);
        for (int k = 0; k < 4; ++k)
            m.ordinal_related[k] = ReadAt<uint32_t>(mb, kMove_Size, kMove_EncOrdinalId + 16 + k * 4);

        m.moveId            = XorDecryptMoveField(mb, kMove_EncOrdinalId, moveIdx);
        m.ordinal_id2       = XorDecryptMoveField(mb, kMove_EncCharId,    moveIdx);

        // -- Cancel pointers ----------------------------------
        m.cancel_addr  = ReadAt<uint64_t>(mb, kMove_Size, kMove_CancelAddr);
        m.cancel2_addr = ReadAt<uint64_t>(mb, kMove_Size, kMove_Cancel2Addr);
        m.u1           = ReadAt<uint64_t>(mb, kMove_Size, kMove_U1);
        m.u2           = ReadAt<uint64_t>(mb, kMove_Size, kMove_U2);
        m.u3           = ReadAt<uint64_t>(mb, kMove_Size, kMove_U3);
        m.u4           = ReadAt<uint64_t>(mb, kMove_Size, kMove_U4);
        m.u6           = ReadAt<uint32_t>(mb, kMove_Size, kMove_U6);
        m.transition   = ReadAt<uint16_t>(mb, kMove_Size, kMove_Transition);
        m._0xCE        = ReadAt<int16_t> (mb, kMove_Size, kMove_0xCE);

        // -- Plain fields -------------------------------------
        m.hit_condition_addr         = ReadAt<uint64_t>(mb, kMove_Size, kMove_HitCondAddr);
        m._0x118                     = ReadAt<uint32_t>(mb, kMove_Size, kMove_0x118);
        m._0x11C                     = ReadAt<uint32_t>(mb, kMove_Size, kMove_0x11C);
        m.anim_len                   = ReadAt<int32_t> (mb, kMove_Size, kMove_AnimLen);
        m.airborne_start             = ReadAt<uint32_t>(mb, kMove_Size, kMove_AirborneStart);
        m.airborne_end               = ReadAt<uint32_t>(mb, kMove_Size, kMove_AirborneEnd);
        m.ground_fall                = ReadAt<uint32_t>(mb, kMove_Size, kMove_GroundFall);
        m.voicelip_addr              = ReadAt<uint64_t>(mb, kMove_Size, kMove_VoicelipAddr);
        m.extra_move_property_addr   = ReadAt<uint64_t>(mb, kMove_Size, kMove_ExtraPropAddr);
        m.move_start_extraprop_addr  = ReadAt<uint64_t>(mb, kMove_Size, kMove_StartPropAddr);
        m.move_end_extraprop_addr    = ReadAt<uint64_t>(mb, kMove_Size, kMove_EndPropAddr);
        m.u15                        = ReadAt<uint32_t>(mb, kMove_Size, kMove_U15);
        m._0x154                     = ReadAt<uint32_t>(mb, kMove_Size, kMove_0x154);
        m.startup                    = ReadAt<uint32_t>(mb, kMove_Size, kMove_Startup);
        m.recovery                   = ReadAt<uint32_t>(mb, kMove_Size, kMove_Recovery);

        // -- Hitbox slots (8 ? 0x30 bytes) -------------------
        for (int h = 0; h < 8; ++h)
        {
            size_t hbase = kMove_Hitbox0 + h * kMove_HitboxStride;
            m.hitbox_active_start[h] = ReadAt<uint32_t>(mb, kMove_Size, hbase + 0x00);
            m.hitbox_active_last[h]  = ReadAt<uint32_t>(mb, kMove_Size, hbase + 0x04);
            m.hitbox_location[h]     = ReadAt<uint32_t>(mb, kMove_Size, hbase + 0x08);
            for (int f = 0; f < 9; ++f)
                m.hitbox_floats[h][f] = ReadAt<float>(mb, kMove_Size, hbase + 0x0C + f * 4);
        }

        // -- Collision / distance -----------------------------
        m.collision = ReadAt<uint16_t>(mb, kMove_Size, kMove_Collision);
        m.distance  = ReadAt<uint16_t>(mb, kMove_Size, kMove_Distance);
        m.u18       = ReadAt<uint32_t>(mb, kMove_Size, 0x444);

        // -- Compute block indexes from file addresses --------
        uint64_t cancelBase  = ReadAt<uint64_t>(buf, cap, 0x1D0);
        uint64_t hitCondBase = ReadAt<uint64_t>(buf, cap, 0x190);
        uint64_t voiceBase   = ReadAt<uint64_t>(buf, cap, 0x240);
        uint64_t exPropBase  = ReadAt<uint64_t>(buf, cap, 0x200);
        uint64_t stPropBase  = ReadAt<uint64_t>(buf, cap, 0x210);
        uint64_t enPropBase  = ReadAt<uint64_t>(buf, cap, 0x220);

        if (m.cancel_addr && m.cancel_addr < cap && cancelBase && m.cancel_addr >= cancelBase)
            m.cancel_idx = (uint32_t)((m.cancel_addr - cancelBase) / 0x28);
        if (m.cancel2_addr && m.cancel2_addr < cap && cancelBase && m.cancel2_addr >= cancelBase)
            m.cancel2_idx = (uint32_t)((m.cancel2_addr - cancelBase) / 0x28);
        if (m.hit_condition_addr && m.hit_condition_addr < cap && hitCondBase && m.hit_condition_addr >= hitCondBase)
            m.hit_condition_idx = (uint32_t)((m.hit_condition_addr - hitCondBase) / 0x18);
        if (m.voicelip_addr && m.voicelip_addr < cap && voiceBase && m.voicelip_addr >= voiceBase)
            m.voiceclip_idx = (uint32_t)((m.voicelip_addr - voiceBase) / 0x0C);
        if (m.extra_move_property_addr && m.extra_move_property_addr < cap && exPropBase && m.extra_move_property_addr >= exPropBase)
            m.extra_prop_idx = (uint32_t)((m.extra_move_property_addr - exPropBase) / 0x28);
        if (m.move_start_extraprop_addr && m.move_start_extraprop_addr < cap && stPropBase && m.move_start_extraprop_addr >= stPropBase)
            m.start_prop_idx = (uint32_t)((m.move_start_extraprop_addr - stPropBase) / 0x20);
        if (m.move_end_extraprop_addr && m.move_end_extraprop_addr < cap && enPropBase && m.move_end_extraprop_addr >= enPropBase)
            m.end_prop_idx = (uint32_t)((m.move_end_extraprop_addr - enPropBase) / 0x20);

        // -- Name lookup via name_keys.json -------------------
        // Supplement entries are placeholder strings of the form "nkXXXXXXXX___"
        // (nk + 8 uppercase hex digits + padding underscores).  These exist only
        // to produce correct string-block byte offsets in the binary; they are not
        // human-readable names, so we fall back to "move_N" for those.
        if (nameKey != 0)
        {
            const char* name = LabelDB::Get().GetMoveName(nameKey);
            if (name)
            {
                // Detect supplement placeholder: starts with "nk" followed by
                // exactly 8 hex digits (e.g. "nk0E8134F4__________")
                bool isPlaceholder = false;
                if (name[0] == 'n' && name[1] == 'k')
                {
                    isPlaceholder = true;
                    for (int hc = 0; hc < 8; ++hc)
                    {
                        char c = name[2 + hc];
                        if (!((c >= '0' && c <= '9') ||
                              (c >= 'A' && c <= 'F') ||
                              (c >= 'a' && c <= 'f')))
                        { isPlaceholder = false; break; }
                    }
                }
                if (!isPlaceholder)
                    m.displayName = name;
            }
        }
        if (m.displayName.empty())
            m.displayName = "move_" + std::to_string(static_cast<uint64_t>(i));

        result.moves.push_back(std::move(m));
    }

    result.loaded = true;
    return result;
}

// -------------------------------------------------------------
//  RebuildMotbinBytes
//  Fully rebuilds the index-format motbin binary from parsed data.
//  This approach supports added/removed items in any block.
// -------------------------------------------------------------

static std::vector<uint8_t> RebuildMotbinBytes(MotbinData& data)
{
    const auto& raw = data.rawBytes;
    if (raw.size() < kMotbinBase) return {};

    // WriteIdx64 helper: writes index (or 0xFFFFFFFFFFFFFFFF if null) at elem+off
    auto WriteIdx64 = [](uint8_t* elem, size_t off, uint32_t idx) {
        uint64_t v = (idx == 0xFFFFFFFF) ? 0xFFFFFFFFFFFFFFFFULL : (uint64_t)idx;
        memcpy(elem + off, &v, 8);
    };

    // Helper: read BASE-relative ptr from rawBytes header -> file offset in rawBytes
    auto RawBlockBase = [&](size_t hdrOff) -> size_t {
        if (hdrOff + 8 > raw.size()) return 0;
        uint64_t val; memcpy(&val, raw.data() + hdrOff, 8);
        return (size_t)(val + kMotbinBase);
    };

    // Start output: copy header (0x318 bytes)
    std::vector<uint8_t> out(raw.begin(), raw.begin() + kMotbinBase);

    // Helper: update header entry in out
    auto UpdateHeader = [&](size_t ptrOff, size_t cntOff, size_t blockStart, size_t cnt) {
        uint64_t relOff = (uint64_t)(blockStart - kMotbinBase);
        memcpy(out.data() + ptrOff, &relOff, 8);
        uint64_t c64 = (uint64_t)cnt;
        memcpy(out.data() + cntOff, &c64, 8);
    };

    // EmitBlock: appends block bytes to out, then updates header.
    // For existing elements (i < origCnt): copies stride bytes from rawBytes, then patches.
    // For new elements (i >= origCnt): zero-fills stride bytes, then patches.
    auto EmitBlock = [&](size_t ptrOff, size_t cntOff, size_t stride,
                         size_t parsedCount,
                         std::function<void(uint8_t*, size_t)> patchFn) -> size_t
    {
        size_t blockStart = out.size();
        size_t origBase   = RawBlockBase(ptrOff);
        size_t origCnt    = 0;
        if (cntOff + 8 <= raw.size()) {
            uint64_t c; memcpy(&c, raw.data() + cntOff, 8);
            origCnt = (size_t)c;
        }

        for (size_t i = 0; i < parsedCount; ++i)
        {
            size_t elemStart = out.size();
            if (i < origCnt && origBase != 0 && origBase + i * stride + stride <= raw.size()) {
                const uint8_t* src = raw.data() + origBase + i * stride;
                out.insert(out.end(), src, src + stride);
            } else {
                out.insert(out.end(), stride, 0u);
            }
            patchFn(out.data() + elemStart, i);
        }

        UpdateHeader(ptrOff, cntOff, blockStart, parsedCount);
        return blockStart;
    };

    // -- reactionListBlock (0x168/0x178, stride 0x70) --
    EmitBlock(0x168, 0x178, 0x70, data.reactionListBlock.size(),
        [&](uint8_t* e, size_t i) {
            const auto& r = data.reactionListBlock[i];
            for (int p = 0; p < 7; ++p) WriteIdx64(e, p * 8, r.pushback_idx[p]);
            auto W16 = [](uint8_t* b, size_t o, uint16_t v){ memcpy(b+o,&v,2); };
            W16(e,0x38,r.front_direction);      W16(e,0x3A,r.back_direction);
            W16(e,0x3C,r.left_side_direction);  W16(e,0x3E,r.right_side_direction);
            W16(e,0x40,r.front_ch_direction);   W16(e,0x42,r.downed_direction);
            W16(e,0x44,r.front_rotation);       W16(e,0x46,r.back_rotation);
            W16(e,0x48,r.left_side_rotation);   W16(e,0x4A,r.right_side_rotation);
            W16(e,0x4C,r.vertical_pushback);    W16(e,0x4E,r.downed_rotation);
            W16(e,0x50,r.standing);             W16(e,0x52,r.crouch);
            W16(e,0x54,r.ch);                   W16(e,0x56,r.crouch_ch);
            W16(e,0x58,r.left_side);            W16(e,0x5A,r.left_side_crouch);
            W16(e,0x5C,r.right_side);           W16(e,0x5E,r.right_side_crouch);
            W16(e,0x60,r.back);                 W16(e,0x62,r.back_crouch);
            W16(e,0x64,r.block);                W16(e,0x66,r.crouch_block);
            W16(e,0x68,r.wallslump);            W16(e,0x6A,r.downed);
        });

    // -- requirementBlock (0x180/0x188, stride 0x14) --
    EmitBlock(0x180, 0x188, 0x14, data.requirementBlock.size(),
        [&](uint8_t* e, size_t i) {
            const auto& r = data.requirementBlock[i];
            memcpy(e+0x00,&r.req,    4); memcpy(e+0x04,&r.param,  4);
            memcpy(e+0x08,&r.param2, 4); memcpy(e+0x0C,&r.param3, 4);
            memcpy(e+0x10,&r.param4, 4);
        });

    // -- hitConditionBlock (0x190/0x198, stride 0x18) --
    EmitBlock(0x190, 0x198, 0x18, data.hitConditionBlock.size(),
        [&](uint8_t* e, size_t i) {
            const auto& h = data.hitConditionBlock[i];
            WriteIdx64(e, 0x00, h.req_list_idx);
            memcpy(e+0x08,&h.damage,4); memcpy(e+0x0C,&h._0x0C,4);
            WriteIdx64(e, 0x10, h.reaction_list_idx);
        });

    // -- projectileBlock (0x1A0/0x1A8, stride 0xE0) --
    EmitBlock(0x1A0, 0x1A8, 0xE0, data.projectileBlock.size(),
        [&](uint8_t* e, size_t i) {
            const auto& p = data.projectileBlock[i];
            for (int n = 0; n < 35; ++n) memcpy(e + n*4, &p.u1[n], 4);
            WriteIdx64(e, 0x90, p.hit_condition_idx);
            WriteIdx64(e, 0x98, p.cancel_idx);
            for (int n = 0; n < 16; ++n) memcpy(e + 0xA0 + n*4, &p.u2[n], 4);
        });

    // -- pushbackBlock (0x1B0/0x1B8, stride 0x10) --
    EmitBlock(0x1B0, 0x1B8, 0x10, data.pushbackBlock.size(),
        [&](uint8_t* e, size_t i) {
            const auto& p = data.pushbackBlock[i];
            memcpy(e+0x00,&p.val1,2); memcpy(e+0x02,&p.val2,2);
            memcpy(e+0x04,&p.val3,4);
            WriteIdx64(e, 0x08, p.pushback_extra_idx);
        });

    // -- pushbackExtraBlock (0x1C0/0x1C8, stride 0x02) --
    EmitBlock(0x1C0, 0x1C8, 0x02, data.pushbackExtraBlock.size(),
        [&](uint8_t* e, size_t i) {
            memcpy(e, &data.pushbackExtraBlock[i].value, 2);
        });

    // -- cancelBlock (0x1D0/0x1D8, stride 0x28) --
    EmitBlock(0x1D0, 0x1D8, 0x28, data.cancelBlock.size(),
        [&](uint8_t* e, size_t i) {
            const auto& c = data.cancelBlock[i];
            memcpy(e+0x00,&c.command,8);
            WriteIdx64(e, 0x08, c.req_list_idx);
            WriteIdx64(e, 0x10, c.extradata_idx);
            memcpy(e+0x18,&c.frame_window_start,4); memcpy(e+0x1C,&c.frame_window_end,4);
            memcpy(e+0x20,&c.starting_frame,4);
            memcpy(e+0x24,&c.move_id,2);            memcpy(e+0x26,&c.cancel_option,2);
        });

    // -- groupCancelBlock (0x1E0/0x1E8, stride 0x28) --
    EmitBlock(0x1E0, 0x1E8, 0x28, data.groupCancelBlock.size(),
        [&](uint8_t* e, size_t i) {
            const auto& c = data.groupCancelBlock[i];
            memcpy(e+0x00,&c.command,8);
            WriteIdx64(e, 0x08, c.req_list_idx);
            WriteIdx64(e, 0x10, c.extradata_idx);
            memcpy(e+0x18,&c.frame_window_start,4); memcpy(e+0x1C,&c.frame_window_end,4);
            memcpy(e+0x20,&c.starting_frame,4);
            memcpy(e+0x24,&c.move_id,2);            memcpy(e+0x26,&c.cancel_option,2);
        });

    // -- cancelExtraBlock (0x1F0/0x1F8, stride 0x04) --
    EmitBlock(0x1F0, 0x1F8, 0x04, data.cancelExtraBlock.size(),
        [&](uint8_t* e, size_t i) {
            memcpy(e, &data.cancelExtraBlock[i], 4);
        });

    // -- extraPropBlock (0x200/0x208, stride 0x28) --
    EmitBlock(0x200, 0x208, 0x28, data.extraPropBlock.size(),
        [&](uint8_t* e, size_t i) {
            const auto& ep = data.extraPropBlock[i];
            memcpy(e+0x00,&ep.type,4);  memcpy(e+0x04,&ep._0x4,4);
            WriteIdx64(e, 0x08, ep.req_list_idx);
            memcpy(e+0x10,&ep.id,4);    memcpy(e+0x14,&ep.value,4);
            memcpy(e+0x18,&ep.value2,4); memcpy(e+0x1C,&ep.value3,4);
            memcpy(e+0x20,&ep.value4,4); memcpy(e+0x24,&ep.value5,4);
        });

    // -- startPropBlock (0x210/0x218, stride 0x20) --
    EmitBlock(0x210, 0x218, 0x20, data.startPropBlock.size(),
        [&](uint8_t* e, size_t i) {
            const auto& ep = data.startPropBlock[i];
            WriteIdx64(e, 0x00, ep.req_list_idx);
            memcpy(e+0x08,&ep.id,4);     memcpy(e+0x0C,&ep.value,4);
            memcpy(e+0x10,&ep.value2,4); memcpy(e+0x14,&ep.value3,4);
            memcpy(e+0x18,&ep.value4,4); memcpy(e+0x1C,&ep.value5,4);
        });

    // -- endPropBlock (0x220/0x228, stride 0x20) --
    EmitBlock(0x220, 0x228, 0x20, data.endPropBlock.size(),
        [&](uint8_t* e, size_t i) {
            const auto& ep = data.endPropBlock[i];
            WriteIdx64(e, 0x00, ep.req_list_idx);
            memcpy(e+0x08,&ep.id,4);     memcpy(e+0x0C,&ep.value,4);
            memcpy(e+0x10,&ep.value2,4); memcpy(e+0x14,&ep.value3,4);
            memcpy(e+0x18,&ep.value4,4); memcpy(e+0x1C,&ep.value5,4);
        });

    // -- moves (0x230/0x238, stride 0x448) --
    // For moves: copy full 0x448 bytes from rawBytes for existing entries
    // (preserves encrypted fields, hitboxes, etc.), then patch pointer+scalar fields.
    EmitBlock(0x230, 0x238, kMove_Size, data.moves.size(),
        [&](uint8_t* e, size_t i) {
            const ParsedMove& m = data.moves[i];
            uint32_t slot = (uint32_t)(i % 8);

            // Encrypted fields: re-XOR with the same key
            { uint32_t enc = m.vuln        ^ kXorKeys[slot]; memcpy(e + kMove_EncVuln      + slot*4, &enc, 4); }
            { uint32_t enc = m.hitlevel    ^ kXorKeys[slot]; memcpy(e + kMove_EncHitlevel  + slot*4, &enc, 4); }
            { uint32_t enc = m.ordinal_id2 ^ kXorKeys[slot]; memcpy(e + kMove_EncCharId    + slot*4, &enc, 4); }
            { uint32_t enc = m.moveId      ^ kXorKeys[slot]; memcpy(e + kMove_EncOrdinalId + slot*4, &enc, 4); }
            { uint32_t enc = m.anim_key    ^ kXorKeys[slot]; memcpy(e + kMove_EncAnimKey   + slot*4, &enc, 4); }

            // anmbin fields
            memcpy(e + kMove_AnimAddrEnc1, &m.anmbin_body_idx,     4);
            memcpy(e + kMove_AnimAddrEnc2, &m.anmbin_body_sub_idx, 4);

            // Plain scalar fields
            memcpy(e + kMove_U1,            &m.u1,             8);
            memcpy(e + kMove_U2,            &m.u2,             8);
            memcpy(e + kMove_U3,            &m.u3,             8);
            memcpy(e + kMove_U4,            &m.u4,             8);
            memcpy(e + kMove_U6,            &m.u6,             4);
            memcpy(e + kMove_Transition,    &m.transition,     2);
            memcpy(e + kMove_0xCE,          &m._0xCE,          2);
            memcpy(e + kMove_0x118,         &m._0x118,         4);
            memcpy(e + kMove_0x11C,         &m._0x11C,         4);
            memcpy(e + kMove_AnimLen,       &m.anim_len,       4);
            memcpy(e + kMove_AirborneStart, &m.airborne_start, 4);
            memcpy(e + kMove_AirborneEnd,   &m.airborne_end,   4);
            memcpy(e + kMove_GroundFall,    &m.ground_fall,    4);
            memcpy(e + kMove_U15,           &m.u15,            4);
            memcpy(e + kMove_0x154,         &m._0x154,         4);
            memcpy(e + kMove_Startup,       &m.startup,        4);
            memcpy(e + kMove_Recovery,      &m.recovery,       4);
            memcpy(e + kMove_Collision,     &m.collision,      2);
            memcpy(e + kMove_Distance,      &m.distance,       2);
            memcpy(e + 0x444,               &m.u18,            4);

            // Hitboxes (8 slots x 0x30 bytes)
            for (int h = 0; h < 8; ++h) {
                size_t hb = kMove_Hitbox0 + h * kMove_HitboxStride;
                memcpy(e + hb + 0x00, &m.hitbox_active_start[h], 4);
                memcpy(e + hb + 0x04, &m.hitbox_active_last[h],  4);
                memcpy(e + hb + 0x08, &m.hitbox_location[h],     4);
                for (int f = 0; f < 9; ++f)
                    memcpy(e + hb + 0x0C + f*4, &m.hitbox_floats[h][f], 4);
            }

            // Pointer fields (stored as indexes in index-format)
            WriteIdx64(e, kMove_CancelAddr,    m.cancel_idx);
            WriteIdx64(e, kMove_Cancel2Addr,   m.cancel2_idx);
            WriteIdx64(e, kMove_HitCondAddr,   m.hit_condition_idx);
            WriteIdx64(e, kMove_VoicelipAddr,  m.voiceclip_idx);
            WriteIdx64(e, kMove_ExtraPropAddr, m.extra_prop_idx);
            WriteIdx64(e, kMove_StartPropAddr, m.start_prop_idx);
            WriteIdx64(e, kMove_EndPropAddr,   m.end_prop_idx);
        });

    // -- voiceclipBlock (0x240/0x248, stride 0x0C) --
    EmitBlock(0x240, 0x248, 0x0C, data.voiceclipBlock.size(),
        [&](uint8_t* e, size_t i) {
            const auto& v = data.voiceclipBlock[i];
            memcpy(e+0x00,&v.val1,4); memcpy(e+0x04,&v.val2,4); memcpy(e+0x08,&v.val3,4);
        });

    // -- inputSequenceBlock (0x250/0x258, stride 0x10) --
    EmitBlock(0x250, 0x258, 0x10, data.inputSequenceBlock.size(),
        [&](uint8_t* e, size_t i) {
            const auto& s = data.inputSequenceBlock[i];
            memcpy(e+0x00,&s.input_window_frames,2); memcpy(e+0x02,&s.input_amount,2);
            memcpy(e+0x04,&s._0x4,4);
            WriteIdx64(e, 0x08, s.input_start_idx);
        });

    // -- inputBlock (0x260/0x268, stride 0x08) --
    EmitBlock(0x260, 0x268, 0x08, data.inputBlock.size(),
        [&](uint8_t* e, size_t i) {
            memcpy(e, &data.inputBlock[i].command, 8);
        });

    // -- parryableMoveBlock (0x270/0x278, stride 0x04) --
    EmitBlock(0x270, 0x278, 0x04, data.parryableMoveBlock.size(),
        [&](uint8_t* e, size_t i) {
            memcpy(e, &data.parryableMoveBlock[i].value, 4);
        });

    // -- throwExtraBlock (0x280/0x288, stride 0x0C) --
    EmitBlock(0x280, 0x288, 0x0C, data.throwExtraBlock.size(),
        [&](uint8_t* e, size_t i) {
            const auto& te = data.throwExtraBlock[i];
            memcpy(e+0x00,&te.pick_probability,4);
            memcpy(e+0x04,&te.camera_type,2);
            memcpy(e+0x06,&te.left_side_camera_data,2);
            memcpy(e+0x08,&te.right_side_camera_data,2);
            memcpy(e+0x0A,&te.additional_rotation,2);
        });

    // -- throwBlock (0x290/0x298, stride 0x10) --
    EmitBlock(0x290, 0x298, 0x10, data.throwBlock.size(),
        [&](uint8_t* e, size_t i) {
            const auto& t = data.throwBlock[i];
            memcpy(e+0x00,&t.side,8);
            WriteIdx64(e, 0x08, t.throwextra_idx);
        });

    // -- dialogueBlock (0x2A0/0x2A8, stride 0x18) --
    EmitBlock(0x2A0, 0x2A8, 0x18, data.dialogueBlock.size(),
        [&](uint8_t* e, size_t i) {
            const auto& d = data.dialogueBlock[i];
            auto W16 = [](uint8_t* b, size_t o, uint16_t v){ memcpy(b+o,&v,2); };
            auto W32 = [](uint8_t* b, size_t o, uint32_t v){ memcpy(b+o,&v,4); };
            W16(e, 0x00, d.type);
            W16(e, 0x02, d.id);
            W32(e, 0x04, d._0x4);
            WriteIdx64(e, 0x08, d.req_list_idx);
            W32(e, 0x10, d.voiceclip_key);
            W32(e, 0x14, d.facial_anim_idx);
        });

    return out;
}

// -------------------------------------------------------------
//  SaveMotbin
//  Fully rebuilds the index-format motbin binary, then writes it.
// -------------------------------------------------------------

bool SaveMotbin(MotbinData& data)
{
    if (data.rawBytes.empty() || data.folderPath.empty()) return false;

    auto out = RebuildMotbinBytes(data);
    if (out.empty()) return false;

    // Write to moveset.motbin
    std::string savePath = data.folderPath;
    if (!savePath.empty() && savePath.back() != '\\' && savePath.back() != '/')
        savePath += '\\';
    savePath += "moveset.motbin";

    HANDLE h = CreateFileW(Utf8ToWide(savePath).c_str(), GENERIC_WRITE, 0,
                           nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;

    DWORD written = 0;
    bool ok = WriteFile(h, out.data(), (DWORD)out.size(), &written, nullptr) != FALSE
              && written == (DWORD)out.size();
    CloseHandle(h);

    if (ok) data.rawBytes = std::move(out); // update baseline so future saves are correct
    return ok;
}

// -------------------------------------------------------------
//  FixupCancelInsert / FixupGroupCancelInsert
//  Shift indexes in all moves/cancels after an insert into a block.
// -------------------------------------------------------------

void FixupCancelInsert(MotbinData& data, uint32_t insertPos, uint32_t delta)
{
    for (auto& m : data.moves) {
        if (m.cancel_idx  != 0xFFFFFFFF && m.cancel_idx  >= insertPos) m.cancel_idx  += delta;
        if (m.cancel2_idx != 0xFFFFFFFF && m.cancel2_idx >= insertPos) m.cancel2_idx += delta;
    }
    // Also shift projectile cancel indexes
    for (auto& p : data.projectileBlock) {
        if (p.cancel_idx != 0xFFFFFFFF && p.cancel_idx >= insertPos) p.cancel_idx += delta;
    }
}

void FixupGroupCancelInsert(MotbinData& data, uint32_t insertPos, uint32_t delta)
{
    uint64_t gcStart = GameStatic::Get().data.groupCancelStart;
    for (auto& c : data.cancelBlock) {
        if (c.command == gcStart && c.move_id != 0xFFFF && c.move_id >= insertPos)
            c.move_id = (uint16_t)(c.move_id + delta);
    }
    for (auto& c : data.groupCancelBlock) {
        if (c.command == gcStart && c.move_id != 0xFFFF && c.move_id >= insertPos)
            c.move_id = (uint16_t)(c.move_id + delta);
    }
}

// -------------------------------------------------------------
//  FixupRef_*  (unified single-element insert/remove fixup)
// -------------------------------------------------------------

void FixupRef_Requirement(MotbinData& d, uint32_t pos, bool ins) {
    for (auto& c  : d.cancelBlock)       AdjRef(c.req_list_idx,  pos, ins);
    for (auto& c  : d.groupCancelBlock)  AdjRef(c.req_list_idx,  pos, ins);
    for (auto& h  : d.hitConditionBlock) AdjRef(h.req_list_idx,  pos, ins);
    for (auto& e  : d.extraPropBlock)    AdjRef(e.req_list_idx,  pos, ins);
    for (auto& e  : d.startPropBlock)    AdjRef(e.req_list_idx,  pos, ins);
    for (auto& e  : d.endPropBlock)      AdjRef(e.req_list_idx,  pos, ins);
    for (auto& dl : d.dialogueBlock)     AdjRef(dl.req_list_idx, pos, ins);
}
void FixupRef_Cancel(MotbinData& d, uint32_t pos, bool ins) {
    for (auto& m : d.moves) { AdjRef(m.cancel_idx, pos, ins); AdjRef(m.cancel2_idx, pos, ins); }
    for (auto& p : d.projectileBlock) AdjRef(p.cancel_idx, pos, ins);
}
void FixupRef_GroupCancel(MotbinData& d, uint32_t pos, bool ins) {
    // group_cancel_list_idx is always 0xFFFFFFFF (never populated, never saved).
    // The real file field is move_id for cancels with command == groupCancelStart.
    uint64_t gcStart = GameStatic::Get().data.groupCancelStart;
    auto adj = [&](ParsedCancel& c) {
        if (c.command != gcStart) return;
        uint32_t mid = c.move_id;
        AdjRef(mid, pos, ins);
        c.move_id = (uint16_t)mid;
    };
    for (auto& c : d.cancelBlock)      adj(c);
    for (auto& c : d.groupCancelBlock) adj(c);
}
void FixupRef_CancelExtra(MotbinData& d, uint32_t pos, bool ins) {
    for (auto& c : d.cancelBlock)      AdjRef(c.extradata_idx, pos, ins);
    for (auto& c : d.groupCancelBlock) AdjRef(c.extradata_idx, pos, ins);
}
void FixupRef_HitCond(MotbinData& d, uint32_t pos, bool ins) {
    for (auto& m : d.moves)           AdjRef(m.hit_condition_idx, pos, ins);
    for (auto& p : d.projectileBlock) AdjRef(p.hit_condition_idx, pos, ins);
}
void FixupRef_ReactionList(MotbinData& d, uint32_t pos, bool ins) {
    for (auto& h : d.hitConditionBlock) AdjRef(h.reaction_list_idx, pos, ins);
}
void FixupRef_Pushback(MotbinData& d, uint32_t pos, bool ins) {
    for (auto& r : d.reactionListBlock)
        for (int i = 0; i < 7; ++i) AdjRef(r.pushback_idx[i], pos, ins);
}
void FixupRef_PushbackExtra(MotbinData& d, uint32_t pos, bool ins) {
    for (auto& p : d.pushbackBlock) AdjRef(p.pushback_extra_idx, pos, ins);
}
void FixupRef_ExtraProp(MotbinData& d, uint32_t pos, bool ins) {
    for (auto& m : d.moves) AdjRef(m.extra_prop_idx, pos, ins);
}
void FixupRef_StartProp(MotbinData& d, uint32_t pos, bool ins) {
    for (auto& m : d.moves) AdjRef(m.start_prop_idx, pos, ins);
}
void FixupRef_EndProp(MotbinData& d, uint32_t pos, bool ins) {
    for (auto& m : d.moves) AdjRef(m.end_prop_idx, pos, ins);
}
void FixupRef_Voiceclip(MotbinData& d, uint32_t pos, bool ins) {
    for (auto& m : d.moves) AdjRef(m.voiceclip_idx, pos, ins);
}
void FixupRef_ThrowExtra(MotbinData& d, uint32_t pos, bool ins) {
    for (auto& t : d.throwBlock) AdjRef(t.throwextra_idx, pos, ins);
}
void FixupRef_Input(MotbinData& d, uint32_t pos, bool ins) {
    for (auto& s : d.inputSequenceBlock) AdjRef(s.input_start_idx, pos, ins);
}

// -------------------------------------------------------------
//  CountRefs_*
// -------------------------------------------------------------

uint32_t CountRefs_Requirement(const MotbinData& d, uint32_t pos) {
    uint32_t n = 0;
    for (auto& c  : d.cancelBlock)       if (c.req_list_idx  == pos) ++n;
    for (auto& c  : d.groupCancelBlock)  if (c.req_list_idx  == pos) ++n;
    for (auto& h  : d.hitConditionBlock) if (h.req_list_idx  == pos) ++n;
    for (auto& e  : d.extraPropBlock)    if (e.req_list_idx  == pos) ++n;
    for (auto& e  : d.startPropBlock)    if (e.req_list_idx  == pos) ++n;
    for (auto& e  : d.endPropBlock)      if (e.req_list_idx  == pos) ++n;
    for (auto& dl : d.dialogueBlock)     if (dl.req_list_idx == pos) ++n;
    return n;
}
uint32_t CountRefs_Cancel(const MotbinData& d, uint32_t pos) {
    uint32_t n = 0;
    for (auto& m : d.moves) { if (m.cancel_idx == pos) ++n; if (m.cancel2_idx == pos) ++n; }
    for (auto& p : d.projectileBlock) if (p.cancel_idx == pos) ++n;
    return n;
}
uint32_t CountRefs_GroupCancel(const MotbinData& d, uint32_t pos) {
    uint64_t gcStart = GameStatic::Get().data.groupCancelStart;
    uint32_t n = 0;
    for (auto& c : d.cancelBlock)      if (c.command == gcStart && (uint32_t)c.move_id == pos) ++n;
    for (auto& c : d.groupCancelBlock) if (c.command == gcStart && (uint32_t)c.move_id == pos) ++n;
    return n;
}
uint32_t CountRefs_CancelExtra(const MotbinData& d, uint32_t pos) {
    uint32_t n = 0;
    for (auto& c : d.cancelBlock)      if (c.extradata_idx == pos) ++n;
    for (auto& c : d.groupCancelBlock) if (c.extradata_idx == pos) ++n;
    return n;
}
uint32_t CountRefs_HitCond(const MotbinData& d, uint32_t pos) {
    uint32_t n = 0;
    for (auto& m : d.moves)           if (m.hit_condition_idx == pos) ++n;
    for (auto& p : d.projectileBlock) if (p.hit_condition_idx == pos) ++n;
    return n;
}
uint32_t CountRefs_ReactionList(const MotbinData& d, uint32_t pos) {
    uint32_t n = 0;
    for (auto& h : d.hitConditionBlock) if (h.reaction_list_idx == pos) ++n;
    return n;
}
uint32_t CountRefs_Pushback(const MotbinData& d, uint32_t pos) {
    uint32_t n = 0;
    for (auto& r : d.reactionListBlock)
        for (int i = 0; i < 7; ++i) if (r.pushback_idx[i] == pos) ++n;
    return n;
}
uint32_t CountRefs_PushbackExtra(const MotbinData& d, uint32_t pos) {
    uint32_t n = 0;
    for (auto& p : d.pushbackBlock) if (p.pushback_extra_idx == pos) ++n;
    return n;
}
uint32_t CountRefs_ExtraProp(const MotbinData& d, uint32_t pos) {
    uint32_t n = 0;
    for (auto& m : d.moves) if (m.extra_prop_idx == pos) ++n;
    return n;
}
uint32_t CountRefs_StartProp(const MotbinData& d, uint32_t pos) {
    uint32_t n = 0;
    for (auto& m : d.moves) if (m.start_prop_idx == pos) ++n;
    return n;
}
uint32_t CountRefs_EndProp(const MotbinData& d, uint32_t pos) {
    uint32_t n = 0;
    for (auto& m : d.moves) if (m.end_prop_idx == pos) ++n;
    return n;
}
uint32_t CountRefs_Voiceclip(const MotbinData& d, uint32_t pos) {
    uint32_t n = 0;
    for (auto& m : d.moves) if (m.voiceclip_idx == pos) ++n;
    return n;
}
uint32_t CountRefs_ThrowExtra(const MotbinData& d, uint32_t pos) {
    uint32_t n = 0;
    for (auto& t : d.throwBlock) if (t.throwextra_idx == pos) ++n;
    return n;
}
uint32_t CountRefs_Input(const MotbinData& d, uint32_t pos) {
    uint32_t n = 0;
    for (auto& s : d.inputSequenceBlock) if (s.input_start_idx == pos) ++n;
    return n;
}
