// MotbinData.cpp -- binary parser for moveset.motbin
// Field offsets based on OldTool2 (TekkenMovesetExtractor TK8) t8_offsetTable.
// Move struct size = 0x448 bytes (FILE format, NOT in-memory).
#include "MotbinData.h"
#include "LabelDB.h"
#include "../GameStatic.h"
#include <windows.h>
#include <cstring>
#include <string>

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
//  SaveMotbin
//  Patches scalar (non-pointer) fields back into the original
//  index-format rawBytes, then writes to moveset.motbin.
//  Pointer fields (requirement_addr, extradata_addr, etc.) are
//  intentionally left unchanged -- block relationships are fixed.
// -------------------------------------------------------------

bool SaveMotbin(MotbinData& data)
{
    if (data.rawBytes.empty() || data.folderPath.empty()) return false;

    auto out = data.rawBytes; // work on a copy of original index-format bytes
    uint8_t*     b  = out.data();
    const size_t sz = out.size();

    // Scalar fields have the same byte offsets in index-format and expanded-format,
    // because ExpandIndexes only modifies pointer (uint64) fields.

    auto PatchAt = [&](size_t off, const void* src, size_t n)
    {
        if (off + n <= sz) memcpy(b + off, src, n);
    };

    auto WriteIdx64 = [&](size_t off, uint32_t idx) {
        uint64_t v = (idx == 0xFFFFFFFF) ? 0xFFFFFFFFFFFFFFFFULL : (uint64_t)idx;
        PatchAt(off, &v, 8);
    };

    // Helper: read BASE-relative header ptr and add kMotbinBase to get file offset.
    auto BlockBase = [&](size_t hdrOff) -> size_t
    {
        if (hdrOff + 8 > sz) return 0;
        uint64_t val; memcpy(&val, b + hdrOff, 8);
        return (size_t)(val + kMotbinBase); // kMotbinBase = 0x318
    };

    // -- requirements (hdr 0x180, stride 0x14) --
    {
        size_t base = BlockBase(0x180);
        for (size_t i = 0; i < data.requirementBlock.size(); ++i)
        {
            const auto& r = data.requirementBlock[i];
            size_t o = base + i * 0x14;
            PatchAt(o + 0x00, &r.req,    4);
            PatchAt(o + 0x04, &r.param,  4);
            PatchAt(o + 0x08, &r.param2, 4);
            PatchAt(o + 0x0C, &r.param3, 4);
            PatchAt(o + 0x10, &r.param4, 4);
        }
    }

    // -- cancelExtra (hdr 0x1F0, stride 4) --
    {
        size_t base = BlockBase(0x1F0);
        for (size_t i = 0; i < data.cancelExtraBlock.size(); ++i)
        {
            size_t o = base + i * 4;
            PatchAt(o, &data.cancelExtraBlock[i], 4);
        }
    }

    // -- cancelBlock (hdr 0x1D0, stride 0x28) -- scalar fields only --
    {
        size_t base = BlockBase(0x1D0);
        for (size_t i = 0; i < data.cancelBlock.size(); ++i)
        {
            const auto& c = data.cancelBlock[i];
            size_t o = base + i * 0x28;
            PatchAt(o + 0x00, &c.command,            8); // uint64 input value
            WriteIdx64(o + 0x08, c.req_list_idx);
            WriteIdx64(o + 0x10, c.extradata_idx);
            PatchAt(o + 0x18, &c.frame_window_start, 4);
            PatchAt(o + 0x1C, &c.frame_window_end,   4);
            PatchAt(o + 0x20, &c.starting_frame,     4);
            PatchAt(o + 0x24, &c.move_id,            2);
            PatchAt(o + 0x26, &c.cancel_option,      2);
        }
    }

    // -- groupCancelBlock (hdr 0x1E0, stride 0x28) --
    {
        size_t base = BlockBase(0x1E0);
        for (size_t i = 0; i < data.groupCancelBlock.size(); ++i)
        {
            const auto& c = data.groupCancelBlock[i];
            size_t o = base + i * 0x28;
            PatchAt(o + 0x00, &c.command,            8);
            WriteIdx64(o + 0x08, c.req_list_idx);
            WriteIdx64(o + 0x10, c.extradata_idx);
            PatchAt(o + 0x18, &c.frame_window_start, 4);
            PatchAt(o + 0x1C, &c.frame_window_end,   4);
            PatchAt(o + 0x20, &c.starting_frame,     4);
            PatchAt(o + 0x24, &c.move_id,            2);
            PatchAt(o + 0x26, &c.cancel_option,      2);
        }
    }

    // -- hitConditionBlock (hdr 0x190, stride 0x18) -- scalars only --
    {
        size_t base = BlockBase(0x190);
        for (size_t i = 0; i < data.hitConditionBlock.size(); ++i)
        {
            const auto& h = data.hitConditionBlock[i];
            size_t o = base + i * 0x18;
            WriteIdx64(o + 0x00, h.req_list_idx);
            PatchAt(o + 0x08, &h.damage, 4);
            PatchAt(o + 0x0C, &h._0x0C,  4);
            WriteIdx64(o + 0x10, h.reaction_list_idx);
        }
    }

    // -- pushbackExtraBlock (hdr 0x1C0, stride 2) --
    {
        size_t base = BlockBase(0x1C0);
        for (size_t i = 0; i < data.pushbackExtraBlock.size(); ++i)
        {
            size_t o = base + i * 2;
            PatchAt(o, &data.pushbackExtraBlock[i].value, 2);
        }
    }

    // -- pushbackBlock (hdr 0x1B0, stride 0x10) --
    {
        size_t base = BlockBase(0x1B0);
        for (size_t i = 0; i < data.pushbackBlock.size(); ++i)
        {
            const auto& p = data.pushbackBlock[i];
            size_t o = base + i * 0x10;
            PatchAt(o + 0x00, &p.val1, 2);
            PatchAt(o + 0x02, &p.val2, 2);
            PatchAt(o + 0x04, &p.val3, 4);
            WriteIdx64(o + 0x08, p.pushback_extra_idx);
        }
    }

    // -- reactionListBlock (hdr 0x168, stride 0x70) -- scalars only --
    {
        size_t base = BlockBase(0x168);
        for (size_t i = 0; i < data.reactionListBlock.size(); ++i)
        {
            const auto& rl = data.reactionListBlock[i];
            size_t o = base + i * 0x70;
            for (int pp = 0; pp < 7; ++pp)
                WriteIdx64(o + pp * 8, rl.pushback_idx[pp]);
            PatchAt(o + 0x38, &rl.front_direction,      2);
            PatchAt(o + 0x3A, &rl.back_direction,       2);
            PatchAt(o + 0x3C, &rl.left_side_direction,  2);
            PatchAt(o + 0x3E, &rl.right_side_direction, 2);
            PatchAt(o + 0x40, &rl.front_ch_direction,   2);
            PatchAt(o + 0x42, &rl.downed_direction,     2);
            PatchAt(o + 0x44, &rl.front_rotation,       2);
            PatchAt(o + 0x46, &rl.back_rotation,        2);
            PatchAt(o + 0x48, &rl.left_side_rotation,   2);
            PatchAt(o + 0x4A, &rl.right_side_rotation,  2);
            PatchAt(o + 0x4C, &rl.vertical_pushback,    2);
            PatchAt(o + 0x4E, &rl.downed_rotation,      2);
            PatchAt(o + 0x50, &rl.standing,             2);
            PatchAt(o + 0x52, &rl.crouch,               2);
            PatchAt(o + 0x54, &rl.ch,                   2);
            PatchAt(o + 0x56, &rl.crouch_ch,            2);
            PatchAt(o + 0x58, &rl.left_side,            2);
            PatchAt(o + 0x5A, &rl.left_side_crouch,     2);
            PatchAt(o + 0x5C, &rl.right_side,           2);
            PatchAt(o + 0x5E, &rl.right_side_crouch,    2);
            PatchAt(o + 0x60, &rl.back,                 2);
            PatchAt(o + 0x62, &rl.back_crouch,          2);
            PatchAt(o + 0x64, &rl.block,                2);
            PatchAt(o + 0x66, &rl.crouch_block,         2);
            PatchAt(o + 0x68, &rl.wallslump,            2);
            PatchAt(o + 0x6A, &rl.downed,               2);
        }
    }

    // -- extraPropBlock (hdr 0x200, stride 0x28) --
    {
        size_t base = BlockBase(0x200);
        for (size_t i = 0; i < data.extraPropBlock.size(); ++i)
        {
            const auto& e = data.extraPropBlock[i];
            size_t o = base + i * 0x28;
            PatchAt(o + 0x00, &e.type,   4); // starting_frame for extraProp
            PatchAt(o + 0x04, &e._0x4,   4);
            WriteIdx64(o + 0x08, e.req_list_idx);
            PatchAt(o + 0x10, &e.id,     4);
            PatchAt(o + 0x14, &e.value,  4);
            PatchAt(o + 0x18, &e.value2, 4);
            PatchAt(o + 0x1C, &e.value3, 4);
            PatchAt(o + 0x20, &e.value4, 4);
            PatchAt(o + 0x24, &e.value5, 4);
        }
    }

    // -- startPropBlock (hdr 0x210, stride 0x20) --
    {
        size_t base = BlockBase(0x210);
        for (size_t i = 0; i < data.startPropBlock.size(); ++i)
        {
            const auto& e = data.startPropBlock[i];
            size_t o = base + i * 0x20;
            WriteIdx64(o + 0x00, e.req_list_idx);
            PatchAt(o + 0x08, &e.id,     4);
            PatchAt(o + 0x0C, &e.value,  4);
            PatchAt(o + 0x10, &e.value2, 4);
            PatchAt(o + 0x14, &e.value3, 4);
            PatchAt(o + 0x18, &e.value4, 4);
            PatchAt(o + 0x1C, &e.value5, 4);
        }
    }

    // -- endPropBlock (hdr 0x220, stride 0x20) --
    {
        size_t base = BlockBase(0x220);
        for (size_t i = 0; i < data.endPropBlock.size(); ++i)
        {
            const auto& e = data.endPropBlock[i];
            size_t o = base + i * 0x20;
            WriteIdx64(o + 0x00, e.req_list_idx);
            PatchAt(o + 0x08, &e.id,     4);
            PatchAt(o + 0x0C, &e.value,  4);
            PatchAt(o + 0x10, &e.value2, 4);
            PatchAt(o + 0x14, &e.value3, 4);
            PatchAt(o + 0x18, &e.value4, 4);
            PatchAt(o + 0x1C, &e.value5, 4);
        }
    }

    // -- inputBlock (hdr 0x260, stride 0x08) --
    {
        size_t base = BlockBase(0x260);
        for (size_t i = 0; i < data.inputBlock.size(); ++i)
        {
            size_t o = base + i * 0x08;
            PatchAt(o, &data.inputBlock[i].command, 8);
        }
    }

    // -- inputSequenceBlock (hdr 0x250, stride 0x10) --
    {
        size_t base = BlockBase(0x250);
        for (size_t i = 0; i < data.inputSequenceBlock.size(); ++i)
        {
            const auto& is = data.inputSequenceBlock[i];
            size_t o = base + i * 0x10;
            PatchAt(o + 0x00, &is.input_window_frames, 2);
            PatchAt(o + 0x02, &is.input_amount,        2);
            PatchAt(o + 0x04, &is._0x4,                4);
            WriteIdx64(o + 0x08, is.input_start_idx);
        }
    }

    // -- projectileBlock (hdr 0x1A0, stride 0xE0) --
    {
        size_t base = BlockBase(0x1A0);
        for (size_t i = 0; i < data.projectileBlock.size(); ++i)
        {
            const auto& p = data.projectileBlock[i];
            size_t o = base + i * 0xE0;
            for (int n = 0; n < 35; ++n)
                PatchAt(o + n * 4, &p.u1[n], 4);
            WriteIdx64(o + 0x90, p.hit_condition_idx);
            WriteIdx64(o + 0x98, p.cancel_idx);
            for (int n = 0; n < 16; ++n)
                PatchAt(o + 0xA0 + n * 4, &p.u2[n], 4);
        }
    }

    // -- parryableMoveBlock (hdr 0x270, stride 0x04) --
    {
        size_t base = BlockBase(0x270);
        for (size_t i = 0; i < data.parryableMoveBlock.size(); ++i)
        {
            size_t o = base + i * 0x04;
            PatchAt(o, &data.parryableMoveBlock[i].value, 4);
        }
    }

    // -- throwExtraBlock (hdr 0x280, stride 0x0C) --
    {
        size_t base = BlockBase(0x280);
        for (size_t i = 0; i < data.throwExtraBlock.size(); ++i)
        {
            const auto& te = data.throwExtraBlock[i];
            size_t o = base + i * 0x0C;
            PatchAt(o + 0x00, &te.pick_probability,       4);
            PatchAt(o + 0x04, &te.camera_type,            2);
            PatchAt(o + 0x06, &te.left_side_camera_data,  2);
            PatchAt(o + 0x08, &te.right_side_camera_data, 2);
            PatchAt(o + 0x0A, &te.additional_rotation,    2);
        }
    }

    // -- throwBlock (hdr 0x290, stride 0x10) --
    {
        size_t base = BlockBase(0x290);
        for (size_t i = 0; i < data.throwBlock.size(); ++i)
        {
            const auto& t = data.throwBlock[i];
            size_t o = base + i * 0x10;
            PatchAt(o + 0x00, &t.side, 8);
            WriteIdx64(o + 0x08, t.throwextra_idx);
        }
    }

    // -- voiceclipBlock (hdr 0x240, stride 0x0C) --
    {
        size_t base = BlockBase(0x240);
        for (size_t i = 0; i < data.voiceclipBlock.size(); ++i)
        {
            const auto& v = data.voiceclipBlock[i];
            size_t o = base + i * 0x0C;
            PatchAt(o + 0x00, &v.val1, 4);
            PatchAt(o + 0x04, &v.val2, 4);
            PatchAt(o + 0x08, &v.val3, 4);
        }
    }

    // -- moves (hdr 0x230, stride 0x448) --
    {
        size_t movesBase = BlockBase(0x230);
        for (size_t i = 0; i < data.moves.size(); ++i)
        {
            const ParsedMove& m = data.moves[i];
            size_t o = movesBase + i * kMove_Size;
            uint32_t slot = (uint32_t)(i % 8);

            // Encrypted fields: re-XOR with the same key used during decryption.
            // vuln @ kMove_EncVuln (0x58), hitlevel @ kMove_EncHitlevel (0x78)
            {
                uint32_t enc = m.vuln ^ kXorKeys[slot];
                PatchAt(o + kMove_EncVuln + slot * 4, &enc, 4);
            }
            {
                uint32_t enc = m.hitlevel ^ kXorKeys[slot];
                PatchAt(o + kMove_EncHitlevel + slot * 4, &enc, 4);
            }
            {
                uint32_t enc = m.ordinal_id2 ^ kXorKeys[slot];
                PatchAt(o + kMove_EncCharId + slot * 4, &enc, 4);
            }
            {
                uint32_t enc = m.moveId ^ kXorKeys[slot];
                PatchAt(o + kMove_EncOrdinalId + slot * 4, &enc, 4);
            }

            // Plain scalar fields
            PatchAt(o + kMove_U1,            &m.u1,           8);
            PatchAt(o + kMove_U2,            &m.u2,           8);
            PatchAt(o + kMove_U3,            &m.u3,           8);
            PatchAt(o + kMove_U4,            &m.u4,           8);
            PatchAt(o + kMove_U6,            &m.u6,           4);
            PatchAt(o + kMove_Transition,    &m.transition,   2);
            PatchAt(o + kMove_0xCE,          &m._0xCE,        2);
            PatchAt(o + kMove_0x118,         &m._0x118,       4);
            PatchAt(o + kMove_0x11C,         &m._0x11C,       4);
            PatchAt(o + kMove_AnimLen,       &m.anim_len,     4);
            PatchAt(o + kMove_AirborneStart, &m.airborne_start, 4);
            PatchAt(o + kMove_AirborneEnd,   &m.airborne_end,   4);
            PatchAt(o + kMove_GroundFall,    &m.ground_fall,    4);
            PatchAt(o + kMove_U15,           &m.u15,            4);
            PatchAt(o + kMove_0x154,         &m._0x154,         4);
            PatchAt(o + kMove_Startup,       &m.startup,        4);
            PatchAt(o + kMove_Recovery,      &m.recovery,       4);
            PatchAt(o + kMove_Collision,     &m.collision,      2);
            PatchAt(o + kMove_Distance,      &m.distance,       2);
            PatchAt(o + 0x444,               &m.u18,            4);

            // Hitboxes (8 slots x 0x30 bytes)
            for (int h = 0; h < 8; ++h)
            {
                size_t hb = o + kMove_Hitbox0 + h * kMove_HitboxStride;
                PatchAt(hb + 0x00, &m.hitbox_active_start[h], 4);
                PatchAt(hb + 0x04, &m.hitbox_active_last[h],  4);
                PatchAt(hb + 0x08, &m.hitbox_location[h],     4);
                for (int f = 0; f < 9; ++f)
                    PatchAt(hb + 0x0C + f * 4, &m.hitbox_floats[h][f], 4);
            }

            WriteIdx64(o + kMove_CancelAddr,    m.cancel_idx);
            WriteIdx64(o + kMove_HitCondAddr,   m.hit_condition_idx);
            WriteIdx64(o + kMove_VoicelipAddr,  m.voiceclip_idx);
            WriteIdx64(o + kMove_ExtraPropAddr, m.extra_prop_idx);
            WriteIdx64(o + kMove_StartPropAddr, m.start_prop_idx);
            WriteIdx64(o + kMove_EndPropAddr,   m.end_prop_idx);
        }
    }

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
