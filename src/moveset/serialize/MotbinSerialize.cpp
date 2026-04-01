// MotbinSerialize.cpp
// Converts state-3 raw motbin dump -> state-1 loader-compatible binary.
// Reference: OldTool2 jsonToBin.py
#include "moveset/serialize/MotbinSerialize.h"
#include <cstring>
#include <algorithm>

// -------------------------------------------------------------
//  Constants
// -------------------------------------------------------------

static constexpr size_t   kBase       = 0x318;
static constexpr size_t   kHdrSize    = kBase;
static constexpr uint64_t kSentinel   = 0xFFFFFFFFFFFFFFFFULL;

// -------------------------------------------------------------
//  XOR_KEYS encryption  (jsonToBin.py encrypt_value)
//
//  Each encrypted field occupies 0x20 bytes (8 ? uint32).
//  Slot [moveIdx % 8] stores (value ^ key[slot]).
//  All other slots store ((0x765 + moveIdx) ^ key[j]).
// -------------------------------------------------------------

static const uint32_t kXorKeys[8] = {
    0x964f5b9eU, 0xd88448a2U, 0xa84b71e0U, 0xa27d5221U,
    0x9b81329fU, 0xadfb76c8U, 0x7def1f1cU, 0x7ee2bc2cU,
};

static void XorEncrypt(uint8_t* moveBuf, size_t attrOff,
                       uint32_t value, uint32_t moveIdx)
{
    uint32_t blockSlot = moveIdx % 8;
    for (uint32_t j = 0; j < 8; ++j)
    {
        uint32_t toWrite = (j == blockSlot) ? value : (0x765u + moveIdx);
        uint32_t enc     = toWrite ^ kXorKeys[j];
        memcpy(moveBuf + attrOff + j * 4, &enc, 4);
    }
}

// -------------------------------------------------------------
//  Game-memory decryption  (validateAndTransform64BitValue)
//  Used to recover decrypted values from raw state-3 move blocks.
// -------------------------------------------------------------

static uint32_t CalcChecksum(uint32_t input_value, uint64_t key)
{
    uint32_t checksum     = 0;
    uint32_t shifted_input = input_value;
    for (int byte_shift = 0; byte_shift < 32; byte_shift += 8)
    {
        uint64_t temp_key   = key;
        int      shift_count = (byte_shift + 8) & 0xFF;
        for (int r = 0; r < shift_count; ++r)
            temp_key = (temp_key << 1) | (temp_key >> 63);
        checksum     ^= shifted_input ^ static_cast<uint32_t>(temp_key & 0xFFFFFFFF);
        shifted_input >>= 8;
    }
    return (checksum == 0) ? 1u : checksum;
}

static uint32_t ValidateAndDecrypt64(uint64_t enc, uint64_t key)
{
    if (enc == 0) return 0;
    uint32_t lo32 = static_cast<uint32_t>(enc & 0xFFFFFFFF);
    uint32_t ck   = CalcChecksum(lo32, key);
    if ((static_cast<uint64_t>(lo32) | (static_cast<uint64_t>(ck) << 32)) != enc)
        return 0;
    uint64_t scrambled = enc ^ 0x1D;
    int rot = static_cast<int>(scrambled & 0x1F);
    for (int r = 0; r < rot; ++r) key = (key << 1) | (key >> 63);
    key &= ~static_cast<uint64_t>(0x1F);
    key ^= scrambled;
    return static_cast<uint32_t>(key & 0xFFFFFFFF);
}

static uint32_t DecryptMoveBlock(const uint8_t* mb, size_t blockOff)
{
    uint64_t enc, key;
    memcpy(&enc, mb + blockOff,     8);
    memcpy(&key, mb + blockOff + 8, 8);
    return ValidateAndDecrypt64(enc, key);
}

uint32_t DecryptMotbinMoveKey(const uint8_t* moveBuf, size_t blockOff)
{
    return DecryptMoveBlock(moveBuf, blockOff);
}

// -------------------------------------------------------------
//  Pointer fixup  (absolute addresses -> file-relative offsets)
//  Mirrors MotbinData.cpp ApplyFixup.
// -------------------------------------------------------------

struct FBDesc { size_t ptrOff, cntOff, stride; int n; size_t ep[8]; };

static const FBDesc kFBBlocks[] = {
    { 0x168, 0x178, 0x70, 7, { 0x00,0x08,0x10,0x18,0x20,0x28,0x30 } },
    { 0x180, 0x188, 0x14, 0, {} },
    { 0x190, 0x198, 0x18, 2, { 0x00, 0x10 } },
    { 0x1A0, 0x1A8, 0xE0, 2, { 0x90, 0x98 } },
    { 0x1B0, 0x1B8, 0x10, 1, { 0x08 } },
    { 0x1C0, 0x1C8, 0x02, 0, {} },
    { 0x1D0, 0x1D8, 0x28, 2, { 0x08, 0x10 } },
    { 0x1E0, 0x1E8, 0x28, 2, { 0x08, 0x10 } },
    { 0x1F0, 0x1F8, 0x04, 0, {} },
    { 0x200, 0x208, 0x28, 1, { 0x08 } },   // extra_props:  req @0x08
    { 0x210, 0x218, 0x20, 1, { 0x00 } },   // start_props:  req @0x00
    { 0x220, 0x228, 0x20, 1, { 0x00 } },   // end_props:    req @0x00
    { 0x230, 0x238, 0x448, 8, { 0x98,0xA0,0xA8,0xB0,0xB8,0xC0,0x110,0x130 } },
    { 0x240, 0x248, 0x0C, 0, {} },
    { 0x250, 0x258, 0x10, 1, { 0x08 } },
    { 0x260, 0x268, 0x08, 0, {} },
    { 0x270, 0x278, 0x04, 0, {} },
    { 0x280, 0x288, 0x0C, 0, {} },
    { 0x290, 0x298, 0x10, 1, { 0x08 } },
    { 0x2A0, 0x2A8, 0x18, 1, { 0x08 } },   // dialogue: req @0x08 (0x10 is audio ID, not a ptr)
};
static const size_t kFBHdrPtrs[] = {
    0x168,0x180,0x190,0x1A0,0x1B0,0x1C0,0x1D0,0x1E0,
    0x1F0,0x200,0x210,0x220,0x230,0x240,0x250,0x260,
    0x270,0x280,0x290,0x2A0,
};
static const size_t kFBMoveExPtrs[] = { 0x138, 0x140, 0x148 };

static void ApplyFixup(std::vector<uint8_t>& v, uint64_t base)
{
    uint8_t*     b  = v.data();
    const size_t sz = v.size();
    auto fix = [&](size_t off) {
        if (off + 8 > sz) return;
        uint64_t val; memcpy(&val, b + off, 8);
        if (!val || val < base || val > base + sz) return;
        val -= base; memcpy(b + off, &val, 8);
    };
    for (size_t o : kFBHdrPtrs) fix(o);
    for (const FBDesc& d : kFBBlocks) {
        if (d.ptrOff + 8 > sz || d.cntOff + 8 > sz) continue;
        uint64_t bOff, cnt;
        memcpy(&bOff, b + d.ptrOff, 8);
        memcpy(&cnt,  b + d.cntOff, 8);
        size_t blockOff = static_cast<size_t>(bOff);
        if (!blockOff || !cnt || blockOff + cnt * d.stride > sz) continue;
        for (uint64_t i = 0; i < cnt; ++i) {
            size_t e = blockOff + static_cast<size_t>(i * d.stride);
            for (int p = 0; p < d.n; ++p) fix(e + d.ep[p]);
        }
    }
    uint64_t mOff, mCnt;
    memcpy(&mOff, b + 0x230, 8); memcpy(&mCnt, b + 0x238, 8);
    size_t mOff_ = static_cast<size_t>(mOff);
    if (mOff_ && mCnt && mOff_ + mCnt * 0x448 <= sz)
        for (uint64_t i = 0; i < mCnt; ++i) {
            size_t e = mOff_ + static_cast<size_t>(i * 0x448);
            for (size_t p : kFBMoveExPtrs) fix(e + p);
        }
}

// -------------------------------------------------------------
//  Block layout helpers
// -------------------------------------------------------------

struct BL { uint64_t off, cnt; size_t stride; };

static BL ReadBL(const uint8_t* b, size_t sz,
                 size_t ptrOff, size_t cntOff, size_t stride)
{
    BL bl = { 0, 0, stride };
    if (ptrOff + 8 <= sz) memcpy(&bl.off, b + ptrOff, 8);
    if (cntOff + 8 <= sz) memcpy(&bl.cnt, b + cntOff, 8);
    return bl;
}

// Convert file offset -> element index within a block.
// Null (0) and kSentinel pass through unchanged.
// Values outside the block's range also pass through unchanged -- they are
// game-internal sentinels (e.g. 0xEAABEAABEAABEAAB in reaction_list) that
// survive ApplyFixup and must be preserved as-is in the state-1 file.
static uint64_t ToIdx(uint64_t fileOff, const BL& bl)
{
    if (fileOff == 0 || fileOff == kSentinel) return fileOff;
    if (!bl.off || !bl.stride || !bl.cnt) return 0;
    if (fileOff < bl.off || fileOff >= bl.off + bl.cnt * bl.stride)
        return fileOff;  // out-of-range sentinel: pass through unchanged
    return (fileOff - bl.off) / bl.stride;
}

template<typename T>
static T RAt(const uint8_t* b, size_t sz, size_t off)
{
    T v = {};
    if (off + sizeof(T) <= sz) memcpy(&v, b + off, sizeof(T));
    return v;
}

static void W16(uint8_t* b, size_t o, uint16_t v) { memcpy(b+o, &v, 2); }
static void W32(uint8_t* b, size_t o, uint32_t v) { memcpy(b+o, &v, 4); }
static void W64(uint8_t* b, size_t o, uint64_t v) { memcpy(b+o, &v, 8); }

// -------------------------------------------------------------
//  ExportLoaderBin
// -------------------------------------------------------------

std::vector<uint8_t> ExportLoaderBin(const std::vector<uint8_t>& rawBytes,
                                      uint64_t motbinBase,
                                      const MotbinNameData* names)
{
    if (rawBytes.size() < kHdrSize) return {};

    // Work on a copy; apply fixup so all pointers become file-relative offsets
    std::vector<uint8_t> fixed = rawBytes;
    if (motbinBase != 0) ApplyFixup(fixed, motbinBase);

    const uint8_t* src     = fixed.data();
    const size_t   srcSize = fixed.size();

    // -- Read block layouts from fixed header --------------------------
    BL bl_react  = ReadBL(src, srcSize, 0x168, 0x178, 0x70);
    BL bl_req    = ReadBL(src, srcSize, 0x180, 0x188, 0x14);
    BL bl_hitc   = ReadBL(src, srcSize, 0x190, 0x198, 0x18);
    BL bl_proj   = ReadBL(src, srcSize, 0x1A0, 0x1A8, 0xE0);
    BL bl_push   = ReadBL(src, srcSize, 0x1B0, 0x1B8, 0x10);
    BL bl_pushex = ReadBL(src, srcSize, 0x1C0, 0x1C8, 0x02);
    BL bl_can    = ReadBL(src, srcSize, 0x1D0, 0x1D8, 0x28);
    BL bl_gcan   = ReadBL(src, srcSize, 0x1E0, 0x1E8, 0x28);
    BL bl_canex  = ReadBL(src, srcSize, 0x1F0, 0x1F8, 0x04);
    BL bl_exprop = ReadBL(src, srcSize, 0x200, 0x208, 0x28);
    BL bl_sprop  = ReadBL(src, srcSize, 0x210, 0x218, 0x20);
    BL bl_eprop  = ReadBL(src, srcSize, 0x220, 0x228, 0x20);
    BL bl_moves  = ReadBL(src, srcSize, 0x230, 0x238, 0x448);
    BL bl_voice  = ReadBL(src, srcSize, 0x240, 0x248, 0x0C);
    BL bl_inseq  = ReadBL(src, srcSize, 0x250, 0x258, 0x10);
    BL bl_inex   = ReadBL(src, srcSize, 0x260, 0x268, 0x08);
    BL bl_parry  = ReadBL(src, srcSize, 0x270, 0x278, 0x04);
    BL bl_threx  = ReadBL(src, srcSize, 0x280, 0x288, 0x0C);
    BL bl_thr    = ReadBL(src, srcSize, 0x290, 0x298, 0x10);
    BL bl_dia    = ReadBL(src, srcSize, 0x2A0, 0x2A8, 0x18);

    // -- Virtual string-block offsets ---------------------------------
    // charName is always at offset 0 in the virtual block (header 0x10 = 0).
    // creatorOff / dateOff / fullDateOff -> header 0x18 / 0x20 / 0x28.
    // totalSize -> header 0x170 (string_block_end_offset).
    // nameOff[i] / animOff[i] -> move[i]+0x040 / move[i]+0x048.
    struct VStrBlock {
        uint64_t creatorOff  = 0;
        uint64_t dateOff     = 0;
        uint64_t fullDateOff = 0;
        uint64_t totalSize   = 0;
        std::vector<uint64_t> nameOff;
        std::vector<uint64_t> animOff;
    } vsb;
    if (names) {
        uint64_t cur = 0;
        auto addStr = [&](const std::string& s) -> uint64_t {
            uint64_t off = cur;
            cur += static_cast<uint64_t>(s.size()) + 1;
            return off;
        };
        addStr(names->charName);            // always at 0; advance cur past it
        vsb.creatorOff  = addStr(names->charCreator);
        vsb.dateOff     = addStr(names->date);
        vsb.fullDateOff = addStr(names->fullDate);
        const size_t nMoves = names->moves.size();
        vsb.nameOff.resize(nMoves);
        vsb.animOff.resize(nMoves);
        for (size_t mi = 0; mi < nMoves; ++mi) {
            vsb.nameOff[mi] = addStr(names->moves[mi].name);
            vsb.animOff[mi] = addStr(names->moves[mi].anim);
        }
        vsb.totalSize = cur;
    }

    // -- Compute output layout (blocks start at kBase = 0x318) ---------
    size_t outSz = kBase;
    size_t o_react  = outSz; outSz += static_cast<size_t>(bl_react.cnt)  * 0x70;
    size_t o_req    = outSz; outSz += static_cast<size_t>(bl_req.cnt)    * 0x14;
    size_t o_hitc   = outSz; outSz += static_cast<size_t>(bl_hitc.cnt)   * 0x18;
    size_t o_proj   = outSz; outSz += static_cast<size_t>(bl_proj.cnt)   * 0xE0;
    size_t o_push   = outSz; outSz += static_cast<size_t>(bl_push.cnt)   * 0x10;
    size_t o_pushex = outSz; outSz += static_cast<size_t>(bl_pushex.cnt) * 0x02;
    outSz = (outSz + 3) & ~size_t(3);  // align to 4
    size_t o_can    = outSz; outSz += static_cast<size_t>(bl_can.cnt)    * 0x28;
    size_t o_gcan   = outSz; outSz += static_cast<size_t>(bl_gcan.cnt)   * 0x28;
    size_t o_canex  = outSz; outSz += static_cast<size_t>(bl_canex.cnt)  * 0x04;
    size_t o_exprop = outSz; outSz += static_cast<size_t>(bl_exprop.cnt) * 0x28;
    size_t o_sprop  = outSz; outSz += static_cast<size_t>(bl_sprop.cnt)  * 0x20;
    size_t o_eprop  = outSz; outSz += static_cast<size_t>(bl_eprop.cnt)  * 0x20;
    size_t o_moves  = outSz; outSz += static_cast<size_t>(bl_moves.cnt)  * 0x448;
    size_t o_voice  = outSz; outSz += static_cast<size_t>(bl_voice.cnt)  * 0x0C;
    size_t o_inseq  = outSz; outSz += static_cast<size_t>(bl_inseq.cnt)  * 0x10;
    size_t o_inex   = outSz; outSz += static_cast<size_t>(bl_inex.cnt)   * 0x08;
    size_t o_parry  = outSz; outSz += static_cast<size_t>(bl_parry.cnt)  * 0x04;
    size_t o_threx  = outSz; outSz += static_cast<size_t>(bl_threx.cnt)  * 0x0C;
    size_t o_thr    = outSz; outSz += static_cast<size_t>(bl_thr.cnt)    * 0x10;
    size_t o_dia    = outSz; outSz += static_cast<size_t>(bl_dia.cnt)    * 0x18;

    std::vector<uint8_t> out(outSz, 0);
    uint8_t* dst = out.data();

    // -- Header --------------------------------------------------------
    // 0x00: disable_anim_lookup = 0
    // 0x04: _0x4 (copy from src)
    W32(dst, 0x04, RAt<uint32_t>(src, srcSize, 0x04));
    // 0x08: TEK signature (copy from src)
    W32(dst, 0x08, RAt<uint32_t>(src, srcSize, 0x08));
    // 0x30-0x167: aliases (copy from src)
    {
        size_t aliasEnd = std::min(srcSize, size_t(0x168));
        if (aliasEnd > 0x30)
            memcpy(dst + 0x30, src + 0x30, aliasEnd - 0x30);
    }
    // 0x168: reaction_list ptr (BASE-relative)
    // 0x170: string_block_end_offset (total virtual string-block size)
    // 0x178: reaction_list count
    // 0x18/0x20/0x28: char_creator / date / fulldate offsets in virtual string block
    //   (0x10 = charName offset = 0, already zeroed by default)
    W64(dst, 0x168, static_cast<uint64_t>(o_react - kBase));
    W64(dst, 0x170, names ? vsb.totalSize : UINT64_C(0));
    W64(dst, 0x178, bl_react.cnt);
    if (names) {
        W64(dst, 0x18, vsb.creatorOff);
        W64(dst, 0x20, vsb.dateOff);
        W64(dst, 0x28, vsb.fullDateOff);
    }
    // Remaining blocks: pairs of (ptr, count) at 0x180 onward
    struct { size_t hdrOff; size_t outOff; uint64_t cnt; } blkHdrs[] = {
        { 0x180, o_req,    bl_req.cnt    },
        { 0x190, o_hitc,   bl_hitc.cnt   },
        { 0x1A0, o_proj,   bl_proj.cnt   },
        { 0x1B0, o_push,   bl_push.cnt   },
        { 0x1C0, o_pushex, bl_pushex.cnt },
        { 0x1D0, o_can,    bl_can.cnt    },
        { 0x1E0, o_gcan,   bl_gcan.cnt   },
        { 0x1F0, o_canex,  bl_canex.cnt  },
        { 0x200, o_exprop, bl_exprop.cnt },
        { 0x210, o_sprop,  bl_sprop.cnt  },
        { 0x220, o_eprop,  bl_eprop.cnt  },
        { 0x230, o_moves,  bl_moves.cnt  },
        { 0x240, o_voice,  bl_voice.cnt  },
        { 0x250, o_inseq,  bl_inseq.cnt  },
        { 0x260, o_inex,   bl_inex.cnt   },
        { 0x270, o_parry,  bl_parry.cnt  },
        { 0x280, o_threx,  bl_threx.cnt  },
        { 0x290, o_thr,    bl_thr.cnt    },
        { 0x2A0, o_dia,    bl_dia.cnt    },
    };
    for (auto& h : blkHdrs) {
        W64(dst, h.hdrOff,     static_cast<uint64_t>(h.outOff - kBase));
        W64(dst, h.hdrOff + 8, h.cnt);
    }
    // 0x2B0-0x317: mota pointers (zero)

    // -- Helper: safe block copy ---------------------------------------
    auto CopyBlock = [&](size_t dstOff, const BL& bl, size_t elemSz) {
        if (!bl.off || !bl.cnt) return;
        size_t blkSz = static_cast<size_t>(bl.cnt * elemSz);
        if (bl.off + blkSz <= srcSize)
            memcpy(dst + dstOff, src + bl.off, blkSz);
    };

    // -- reaction_list -------------------------------------------------
    // 7 ? uint64 pushback pointers at +0x00..+0x30; +0x38 is front_direction (uint16, not a ptr)
    for (uint64_t i = 0; i < bl_react.cnt; ++i) {
        size_t si = static_cast<size_t>(bl_react.off + i * 0x70);
        size_t di = o_react + static_cast<size_t>(i * 0x70);
        if (si + 0x70 > srcSize) break;
        memcpy(dst + di, src + si, 0x70);
        for (int p = 0; p < 7; ++p)
            W64(dst, di + p * 8, ToIdx(RAt<uint64_t>(src, srcSize, si + p * 8), bl_push));
    }

    // -- requirements (no inner pointers) -----------------------------
    CopyBlock(o_req, bl_req, 0x14);

    // -- hit_conditions ------------------------------------------------
    // [0x00] requirement_idx (uint64)
    // [0x08] damage (written as uint64 in jsonToBin)
    // [0x10] reaction_list_idx (uint64)
    for (uint64_t i = 0; i < bl_hitc.cnt; ++i) {
        size_t si = static_cast<size_t>(bl_hitc.off + i * 0x18);
        size_t di = o_hitc + static_cast<size_t>(i * 0x18);
        if (si + 0x18 > srcSize) break;
        W64(dst, di + 0x00, ToIdx(RAt<uint64_t>(src, srcSize, si + 0x00), bl_req));
        W64(dst, di + 0x08, static_cast<uint64_t>(RAt<uint32_t>(src, srcSize, si + 0x08)));
        W64(dst, di + 0x10, ToIdx(RAt<uint64_t>(src, srcSize, si + 0x10), bl_react));
    }

    // -- projectiles ---------------------------------------------------
    // Pointer fields: hit_condition_idx @0x90, cancel_idx @0x98
    // Runtime fields: 0xAC (u32), 0xB0 (u64)
    //   0xAC: game computes this at load time (= 0x30 * 2) for normal projectiles
    //         (type field @0x00 without bit 0x800). Special-type projectiles
    //         (bit 0x800 set) preserve their file value, so we keep the game-memory
    //         value for those (it matches the original file).
    for (uint64_t i = 0; i < bl_proj.cnt; ++i) {
        size_t si = static_cast<size_t>(bl_proj.off + i * 0xE0);
        size_t di = o_proj + static_cast<size_t>(i * 0xE0);
        if (si + 0xE0 > srcSize) break;
        memcpy(dst + di, src + si, 0xE0);
        W64(dst, di + 0x90, ToIdx(RAt<uint64_t>(src, srcSize, si + 0x90), bl_hitc));
        W64(dst, di + 0x98, ToIdx(RAt<uint64_t>(src, srcSize, si + 0x98), bl_can));
        // 0xAC: zero only for normal projectile types (no 0x800 bit in type @0x00).
        // Special types (0x800 bit) preserve the file value -- game doesn't overwrite.
        if (!(RAt<uint32_t>(src, srcSize, si + 0x00) & 0x800u))
            W32(dst, di + 0xAC, 0);
        W64(dst, di + 0xB0, 0);  // always runtime, always 0 in file format
    }

    // -- pushbacks  (pointer field: pushbackextra_idx @0x08) ----------
    for (uint64_t i = 0; i < bl_push.cnt; ++i) {
        size_t si = static_cast<size_t>(bl_push.off + i * 0x10);
        size_t di = o_push + static_cast<size_t>(i * 0x10);
        if (si + 0x10 > srcSize) break;
        memcpy(dst + di, src + si, 0x10);
        W64(dst, di + 0x08, ToIdx(RAt<uint64_t>(src, srcSize, si + 0x08), bl_pushex));
    }

    // -- pushback_extras (no inner pointers) --------------------------
    CopyBlock(o_pushex, bl_pushex, 0x02);

    // -- cancels  (requirement_idx @0x08, extradata_idx @0x10) ---------
    auto WriteCancels = [&](size_t dstBase, const BL& bl_src) {
        for (uint64_t i = 0; i < bl_src.cnt; ++i) {
            size_t si = static_cast<size_t>(bl_src.off + i * 0x28);
            size_t di = dstBase + static_cast<size_t>(i * 0x28);
            if (si + 0x28 > srcSize) break;
            memcpy(dst + di, src + si, 0x28);
            W64(dst, di + 0x08, ToIdx(RAt<uint64_t>(src, srcSize, si + 0x08), bl_req));
            W64(dst, di + 0x10, ToIdx(RAt<uint64_t>(src, srcSize, si + 0x10), bl_canex));
            // cancel_option [0x26]: runtime bits set by the game engine at load time.
            // The original file-format motbin always stores 0x0000 here, but we preserve it
            // W16(dst, di + 0x26, 0);
        }
    };
    WriteCancels(o_can,  bl_can);
    WriteCancels(o_gcan, bl_gcan);

    // -- cancel_extradata (no inner pointers) -------------------------
    CopyBlock(o_canex, bl_canex, 0x04);

    // -- extra_move_properties (timed) -- requirement_idx @0x08 --------
    for (uint64_t i = 0; i < bl_exprop.cnt; ++i) {
        size_t si = static_cast<size_t>(bl_exprop.off + i * 0x28);
        size_t di = o_exprop + static_cast<size_t>(i * 0x28);
        if (si + 0x28 > srcSize) break;
        memcpy(dst + di, src + si, 0x28);
        W64(dst, di + 0x08, ToIdx(RAt<uint64_t>(src, srcSize, si + 0x08), bl_req));
    }

    // -- move_start/end_props (untimed) -- requirement_idx @0x00 -------
    auto WriteUntimed = [&](size_t dstBase, const BL& bl_src) {
        for (uint64_t i = 0; i < bl_src.cnt; ++i) {
            size_t si = static_cast<size_t>(bl_src.off + i * 0x20);
            size_t di = dstBase + static_cast<size_t>(i * 0x20);
            if (si + 0x20 > srcSize) break;
            memcpy(dst + di, src + si, 0x20);
            W64(dst, di + 0x00, ToIdx(RAt<uint64_t>(src, srcSize, si + 0x00), bl_req));
        }
    };
    WriteUntimed(o_sprop, bl_sprop);
    WriteUntimed(o_eprop, bl_eprop);

    // -- moves (complex: re-encrypt + convert pointer fields) ----------
    for (uint64_t i = 0; i < bl_moves.cnt; ++i) {
        size_t    si  = static_cast<size_t>(bl_moves.off + i * 0x448);
        size_t    di  = o_moves + static_cast<size_t>(i * 0x448);
        uint32_t  idx = static_cast<uint32_t>(i);
        if (si + 0x448 > srcSize) break;

        // Copy raw bytes first (non-pointer, non-encrypted fields inherit)
        memcpy(dst + di, src + si, 0x448);

        // name_idx_abs (0x040) / anim_idx_abs (0x048): virtual string-block byte offsets.
        // In state-3 these are absolute game-memory pointers into a separate allocation.
        // When MotbinNameData is provided we write the correct file-format offsets;
        // otherwise zero (loader will not find name/anim strings).
        {
            const size_t mi = static_cast<size_t>(i);
            if (names && mi < vsb.nameOff.size()) {
                W64(dst, di + 0x40, vsb.nameOff[mi]);
                W64(dst, di + 0x48, vsb.animOff[mi]);
            } else {
                W64(dst, di + 0x40, 0);
                W64(dst, di + 0x48, 0);
            }
        }

        // anim_len (0x120): populated at runtime from the anmbin asset.
        // The state-3 game memory dump already contains the live value, so we
        // preserve it from src.  (The original file-format reference has 0 here
        // because it was built offline without the runtime value; live extraction
        // gives us the real frame count.)
        W32(dst, di + 0x120, RAt<uint32_t>(src, srcSize, si + 0x120));

        // Re-encrypt 6 fields: decrypt with game-memory scheme, re-encrypt with XOR_KEYS
        const uint8_t* mb = src + si;
        XorEncrypt(dst + di, 0x00, DecryptMoveBlock(mb, 0x00), idx);  // name_key
        XorEncrypt(dst + di, 0x20, DecryptMoveBlock(mb, 0x20), idx);  // anim_key
        XorEncrypt(dst + di, 0x58, DecryptMoveBlock(mb, 0x58), idx);  // vuln
        XorEncrypt(dst + di, 0x78, DecryptMoveBlock(mb, 0x78), idx);  // hitlevel
        XorEncrypt(dst + di, 0xD0, DecryptMoveBlock(mb, 0xD0), idx);  // t_char_id
        XorEncrypt(dst + di, 0xF0, DecryptMoveBlock(mb, 0xF0), idx);  // ordinal_id

        // anim_addr_enc1/2: mode=0 -> (move_idx, 0)
        W32(dst, di + 0x50, idx);
        W32(dst, di + 0x54, 0);

        // Convert pointer fields -> indices
        auto cvt = [&](size_t off, const BL& bl) {
            W64(dst, di + off, ToIdx(RAt<uint64_t>(src, srcSize, si + off), bl));
        };
        // Optional pointer fields: null (0) in state-3 -> sentinel (0xFFFFFFFFFFFFFFFF)
        // in index format.  The original working motbin uses sentinel for "no data".
        auto cvtOpt = [&](size_t off, const BL& bl) {
            uint64_t fileOff = RAt<uint64_t>(src, srcSize, si + off);
            W64(dst, di + off, (fileOff == 0) ? kSentinel : ToIdx(fileOff, bl));
        };
        cvt(0x98,  bl_can);     // cancel_addr  -> regular cancel index
        W64(dst, di + 0xA0, 0); // cancel2_addr -> 0 (game-internal, always 0 in file format)
        cvt(0x110, bl_hitc);    // hit_condition_addr
        // u1/u2/u3/u4 @0xA8-0xC0: pointer fields fixed by ApplyFixup; target block
        // unknown.  Zero them (matches motbinImport.py behaviour for 0xA0/0xA8).
        W64(dst, di + 0xA8, 0);
        W64(dst, di + 0xB0, 0);
        W64(dst, di + 0xB8, 0);
        W64(dst, di + 0xC0, 0);
        // Optional fields: null -> sentinel so loader recognises "not present"
        cvtOpt(0x130, bl_voice);
        cvtOpt(0x138, bl_exprop);
        cvtOpt(0x140, bl_sprop);
        cvtOpt(0x148, bl_eprop);
    }

    // -- voiceclips (no inner pointers) -------------------------------
    CopyBlock(o_voice, bl_voice, 0x0C);

    // -- input_sequences  (extradata_idx @0x08) -----------------------
    for (uint64_t i = 0; i < bl_inseq.cnt; ++i) {
        size_t si = static_cast<size_t>(bl_inseq.off + i * 0x10);
        size_t di = o_inseq + static_cast<size_t>(i * 0x10);
        if (si + 0x10 > srcSize) break;
        memcpy(dst + di, src + si, 0x10);
        W64(dst, di + 0x08, ToIdx(RAt<uint64_t>(src, srcSize, si + 0x08), bl_inex));
    }

    // -- input_extradata (no inner pointers) --------------------------
    CopyBlock(o_inex, bl_inex, 0x08);

    // -- parry_related (no inner pointers) ----------------------------
    CopyBlock(o_parry, bl_parry, 0x04);

    // -- throw_extras (no inner pointers) -----------------------------
    CopyBlock(o_threx, bl_threx, 0x0C);

    // -- throws  (throwextra_idx @0x08) --------------------------------
    for (uint64_t i = 0; i < bl_thr.cnt; ++i) {
        size_t si = static_cast<size_t>(bl_thr.off + i * 0x10);
        size_t di = o_thr + static_cast<size_t>(i * 0x10);
        if (si + 0x10 > srcSize) break;
        memcpy(dst + di, src + si, 0x10);
        W64(dst, di + 0x08, ToIdx(RAt<uint64_t>(src, srcSize, si + 0x08), bl_threx));
    }

    // -- dialogues  (requirement_idx @0x08, audio-ID @0x10 is not a block ptr) --
    for (uint64_t i = 0; i < bl_dia.cnt; ++i) {
        size_t si = static_cast<size_t>(bl_dia.off + i * 0x18);
        size_t di = o_dia + static_cast<size_t>(i * 0x18);
        if (si + 0x18 > srcSize) break;
        memcpy(dst + di, src + si, 0x18);
        // 0x08: requirement pointer -> index (confirmed from original file analysis)
        W64(dst, di + 0x08, ToIdx(RAt<uint64_t>(src, srcSize, si + 0x08), bl_req));
        // 0x10: audio/voice ID -- not a block pointer, memcpy'd as-is above
    }

    return out;
}
