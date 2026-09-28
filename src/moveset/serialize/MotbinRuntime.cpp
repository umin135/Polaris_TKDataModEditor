// MotbinRuntime.cpp
// state-1 (index) -> state-3 (absolute pointers) for live injection.
// Mirrors TK__populateMotbin; skips anim-manager lookup.
#include "moveset/serialize/MotbinRuntime.h"
#include "moveset/serialize/MotbinSerialize.h"
#include <cstring>
#include <algorithm>

static constexpr size_t   kBase     = 0x318;
static constexpr uint64_t kSentinel = 0xFFFFFFFFFFFFFFFFULL;

// -------------------------------------------------------------
//  Encryption (TK__encrypt32BitWith32BitChecksum)
// -------------------------------------------------------------

static uint32_t CalcChecksum(uint32_t input_value, uint64_t key)
{
    uint32_t checksum = 0;
    uint32_t shifted  = input_value;
    for (int byte_shift = 0; byte_shift < 32; byte_shift += 8)
    {
        uint64_t temp_key    = key;
        int      shift_count = (byte_shift + 8) & 0xFF;
        for (int r = 0; r < shift_count; ++r)
            temp_key = (temp_key << 1) | (temp_key >> 63);
        checksum ^= shifted ^ static_cast<uint32_t>(temp_key & 0xFFFFFFFF);
        shifted >>= 8;
    }
    return (checksum == 0) ? 1u : checksum;
}

uint32_t TkDecrypt32(const uint8_t* src16)
{
    if (!src16) return 0;
    uint64_t enc = 0, key = 0;
    memcpy(&enc, src16, 8);
    memcpy(&key, src16 + 8, 8);
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

bool TkEncrypt32(uint8_t* dst16, uint32_t originalValue, uint64_t encKey)
{
    if (!dst16) return false;
    if (encKey == 0) encKey = kTkDefaultEncKey;

    uint64_t key = encKey;
    unsigned rot = originalValue & 0x1Fu;
    if (rot)
    {
        for (unsigned i = 0; i < rot; ++i)
            key = (key >> 63) + 2 * key;
    }
    uint64_t v5 = originalValue ^ (key & 0xFFFFFFE0ull) ^ 0x1Dull;
    uint32_t v4 = 0;
    uint64_t v6 = v5;
    for (unsigned i = 0; i < 32; i += 8)
    {
        uint64_t v8 = encKey;
        int v9 = static_cast<int>(static_cast<uint8_t>(i + 8));
        do {
            v8 = (v8 >> 63) + 2 * v8;
            --v9;
        } while (v9);
        v4 ^= static_cast<uint32_t>(v6) ^ static_cast<uint32_t>(v8);
        v6 >>= 8;
    }
    uint64_t ck = v4 ? v4 : 1u;
    uint64_t value = v5 + (ck << 32);
    memcpy(dst16, &value, 8);
    memcpy(dst16 + 8, &encKey, 8);
    // Zero trailing related decoy bytes (0x10..0x1F of the 0x20 region is caller's job
    // when writing a full 0x20 block; here we only write the 16-byte tk_encrypted.)
    return TkDecrypt32(dst16) == originalValue;
}

// -------------------------------------------------------------
//  Helpers
// -------------------------------------------------------------

template<typename T>
static T RAt(const uint8_t* b, size_t sz, size_t off)
{
    T v = {};
    if (off + sizeof(T) <= sz) memcpy(&v, b + off, sizeof(T));
    return v;
}

static void W32(uint8_t* b, size_t o, uint32_t v) { memcpy(b + o, &v, 4); }
static void W64(uint8_t* b, size_t o, uint64_t v) { memcpy(b + o, &v, 8); }

struct BL { uint64_t off, cnt; size_t stride; };

static BL ReadBL(const uint8_t* b, size_t sz, size_t ptrOff, size_t cntOff, size_t stride)
{
    BL bl = { 0, 0, stride };
    if (ptrOff + 8 <= sz) memcpy(&bl.off, b + ptrOff, 8);
    if (cntOff + 8 <= sz) memcpy(&bl.cnt,  b + cntOff, 8);
    // state-1 header ptrs are BASE-relative (file_offset - 0x318).
    // Relative 0 is VALID and means the block starts at file offset 0x318
    // (reactions is always first). Only treat as unused when count is 0.
    if (bl.cnt)
        bl.off += kBase;
    else
        bl.off = 0;
    return bl;
}

static bool IsNullIdx(uint64_t idx)
{
    // Index 0 is a valid first element. Only the file sentinel means null.
    return idx == kSentinel || idx == static_cast<uint64_t>(-1);
}

static uint64_t IdxToAbs(uint64_t idx, uint64_t blockAbs, size_t stride)
{
    if (IsNullIdx(idx)) return 0;
    return blockAbs + idx * stride;
}

static uint64_t IdxToAbsOpt(uint64_t idx, uint64_t blockAbs, size_t stride)
{
    if (IsNullIdx(idx)) return 0;
    return blockAbs + idx * stride;
}

// -------------------------------------------------------------
//  BuildRuntimeBlob
// -------------------------------------------------------------

std::vector<uint8_t> BuildRuntimeBlob(const std::vector<uint8_t>& state1,
                                      const MotbinData& model,
                                      uint64_t targetBase,
                                      uint32_t charId,
                                      std::string& err,
                                      const uint8_t* liveState3,
                                      size_t liveSize,
                                      uint64_t liveBase)
{
    err.clear();
    if (state1.size() < kBase)
    {
        err = "state-1 blob too small";
        return {};
    }

    // Append a single "?" string at the end for name/anim placeholders
    // (fallback when live string pointers are unavailable).
    const size_t qMarkOff = state1.size();
    std::vector<uint8_t> out(state1.size() + 2, 0); // "?" + NUL
    memcpy(out.data(), state1.data(), state1.size());
    out[qMarkOff] = '?';
    out[qMarkOff + 1] = 0;
    const uint64_t qMarkAbs = targetBase + qMarkOff;

    uint8_t* dst = out.data();
    const size_t sz = out.size();

    const bool haveLive = liveState3 && liveSize >= kBase && liveBase != 0;

    // -- Header flags -------------------------------------------------
    // skip_anim_lookup must be 0 — matches runtime-imported movesets.
    dst[0x00] = 0;
    dst[0x02] = 1;

    // Dummy character special-case
    if (charId == 116)
    {
        W32(dst, 0x160, 0x73FF8D);
        W32(dst, 0x164, 0x74FF8C);
    }

    // String placeholders: prefer the game's static "?" pointers from live.
    if (haveLive && liveSize >= 0x30)
    {
        memcpy(dst + 0x10, liveState3 + 0x10, 8);
        memcpy(dst + 0x18, liveState3 + 0x18, 8);
        memcpy(dst + 0x20, liveState3 + 0x20, 8);
        memcpy(dst + 0x28, liveState3 + 0x28, 8);
    }
    else
    {
        W64(dst, 0x10, qMarkAbs);
        W64(dst, 0x18, qMarkAbs);
        W64(dst, 0x20, qMarkAbs);
        W64(dst, 0x28, qMarkAbs);
    }
    W64(dst, 0x170, 0); // string_block_end_offset

    // Live moves array (absolute -> file-relative) for "?" string ptr reuse
    size_t liveMovesOff = 0;
    uint64_t liveMovesCnt = 0;
    if (haveLive && liveSize >= 0x240)
    {
        uint64_t absMoves = 0;
        memcpy(&absMoves, liveState3 + 0x230, 8);
        memcpy(&liveMovesCnt, liveState3 + 0x238, 8);
        if (absMoves >= liveBase && absMoves - liveBase + 0x448 <= liveSize)
            liveMovesOff = static_cast<size_t>(absMoves - liveBase);
        else
            liveMovesCnt = 0;
    }
    // -- Block layouts from state-1 header ----------------------------
    BL bl_react  = ReadBL(dst, sz, 0x168, 0x178, 0x70);
    BL bl_req    = ReadBL(dst, sz, 0x180, 0x188, 0x14);
    BL bl_hitc   = ReadBL(dst, sz, 0x190, 0x198, 0x18);
    BL bl_proj   = ReadBL(dst, sz, 0x1A0, 0x1A8, 0xE0);
    BL bl_push   = ReadBL(dst, sz, 0x1B0, 0x1B8, 0x10);
    BL bl_pushex = ReadBL(dst, sz, 0x1C0, 0x1C8, 0x02);
    BL bl_can    = ReadBL(dst, sz, 0x1D0, 0x1D8, 0x28);
    BL bl_gcan   = ReadBL(dst, sz, 0x1E0, 0x1E8, 0x28);
    BL bl_canex  = ReadBL(dst, sz, 0x1F0, 0x1F8, 0x04);
    BL bl_exprop = ReadBL(dst, sz, 0x200, 0x208, 0x28);
    BL bl_sprop  = ReadBL(dst, sz, 0x210, 0x218, 0x20);
    BL bl_eprop  = ReadBL(dst, sz, 0x220, 0x228, 0x20);
    BL bl_moves  = ReadBL(dst, sz, 0x230, 0x238, 0x448);
    BL bl_voice  = ReadBL(dst, sz, 0x240, 0x248, 0x0C);
    BL bl_inseq  = ReadBL(dst, sz, 0x250, 0x258, 0x10);
    BL bl_inex   = ReadBL(dst, sz, 0x260, 0x268, 0x08);
    BL bl_parry  = ReadBL(dst, sz, 0x270, 0x278, 0x04);
    BL bl_threx  = ReadBL(dst, sz, 0x280, 0x288, 0x0C);
    BL bl_thr    = ReadBL(dst, sz, 0x290, 0x298, 0x10);
    BL bl_dia    = ReadBL(dst, sz, 0x2A0, 0x2A8, 0x18);

    auto absOf = [&](const BL& bl) -> uint64_t {
        return bl.off ? (targetBase + bl.off) : 0;
    };

    const uint64_t a_react  = absOf(bl_react);
    const uint64_t a_req    = absOf(bl_req);
    const uint64_t a_hitc   = absOf(bl_hitc);
    const uint64_t a_proj   = absOf(bl_proj);
    const uint64_t a_push   = absOf(bl_push);
    const uint64_t a_pushex = absOf(bl_pushex);
    const uint64_t a_can    = absOf(bl_can);
    const uint64_t a_gcan   = absOf(bl_gcan);
    const uint64_t a_canex  = absOf(bl_canex);
    const uint64_t a_exprop = absOf(bl_exprop);
    const uint64_t a_sprop  = absOf(bl_sprop);
    const uint64_t a_eprop  = absOf(bl_eprop);
    const uint64_t a_moves  = absOf(bl_moves);
    const uint64_t a_voice  = absOf(bl_voice);
    const uint64_t a_inseq  = absOf(bl_inseq);
    const uint64_t a_inex   = absOf(bl_inex);
    const uint64_t a_parry  = absOf(bl_parry);
    const uint64_t a_threx  = absOf(bl_threx);
    const uint64_t a_thr    = absOf(bl_thr);
    const uint64_t a_dia    = absOf(bl_dia);

    // Header block pointers: targetBase + 0x318 + state1_value
    // (state1 value already converted to file-off via ReadBL; write abs)
    W64(dst, 0x168, a_react);
    W64(dst, 0x180, a_req);
    W64(dst, 0x190, a_hitc);
    W64(dst, 0x1A0, a_proj);
    W64(dst, 0x1B0, a_push);
    W64(dst, 0x1C0, a_pushex);
    W64(dst, 0x1D0, a_can);
    W64(dst, 0x1E0, a_gcan);
    W64(dst, 0x1F0, a_canex);
    W64(dst, 0x200, a_exprop);
    W64(dst, 0x210, a_sprop);
    W64(dst, 0x220, a_eprop);
    W64(dst, 0x230, a_moves);
    W64(dst, 0x240, a_voice);
    W64(dst, 0x250, a_inseq);
    W64(dst, 0x260, a_inex);
    W64(dst, 0x270, a_parry);
    W64(dst, 0x280, a_threx);
    W64(dst, 0x290, a_thr);
    W64(dst, 0x2A0, a_dia);

    // Null mota / arc pointers
    for (size_t o = 0x2B0; o <= 0x310; o += 8)
        W64(dst, o, 0);

    // Dominant anim_handle_hi for fallback on new moves
    uint32_t dominantHi = 0;
    for (const auto& m : model.moves)
    {
        if (m.anim_handle_hi)
        {
            dominantHi = m.anim_handle_hi;
            break;
        }
    }
    // Fallback: 0xEF00 | (char_id << 8) — char_id lives in bits 40..55 of the u64 handle
    if (!dominantHi && charId)
        dominantHi = 0xEF000000u | ((charId & 0xFFFFu) << 8);

    // -- reactions: 7 pushback ptrs -----------------------------------
    for (uint64_t i = 0; i < bl_react.cnt; ++i)
    {
        size_t e = static_cast<size_t>(bl_react.off + i * 0x70);
        if (e + 0x70 > state1.size()) break;
        for (int p = 0; p < 7; ++p)
        {
            uint64_t idx = RAt<uint64_t>(dst, sz, e + p * 8);
            W64(dst, e + p * 8, IdxToAbs(idx, a_push, 0x10));
        }
    }

    // -- hit_conditions -----------------------------------------------
    for (uint64_t i = 0; i < bl_hitc.cnt; ++i)
    {
        size_t e = static_cast<size_t>(bl_hitc.off + i * 0x18);
        if (e + 0x18 > state1.size()) break;
        W64(dst, e + 0x00, IdxToAbs(RAt<uint64_t>(dst, sz, e + 0x00), a_req, 0x14));
        W64(dst, e + 0x10, IdxToAbs(RAt<uint64_t>(dst, sz, e + 0x10), a_react, 0x70));
    }

    // -- cancels / group cancels --------------------------------------
    auto FixCancels = [&](const BL& bl, uint64_t aCan) {
        (void)aCan;
        for (uint64_t i = 0; i < bl.cnt; ++i)
        {
            size_t e = static_cast<size_t>(bl.off + i * 0x28);
            if (e + 0x28 > state1.size()) break;
            W64(dst, e + 0x08, IdxToAbs(RAt<uint64_t>(dst, sz, e + 0x08), a_req, 0x14));
            W64(dst, e + 0x10, IdxToAbs(RAt<uint64_t>(dst, sz, e + 0x10), a_canex, 0x04));
            // option @0x26 kept as-is
        }
    };
    FixCancels(bl_can,  a_can);
    FixCancels(bl_gcan, a_gcan);

    // -- extra_move_properties (req @0x08) ----------------------------
    for (uint64_t i = 0; i < bl_exprop.cnt; ++i)
    {
        size_t e = static_cast<size_t>(bl_exprop.off + i * 0x28);
        if (e + 0x28 > state1.size()) break;
        W64(dst, e + 0x08, IdxToAbs(RAt<uint64_t>(dst, sz, e + 0x08), a_req, 0x14));
    }

    // -- start/end props (req @0x00) ----------------------------------
    auto FixUntimed = [&](const BL& bl) {
        for (uint64_t i = 0; i < bl.cnt; ++i)
        {
            size_t e = static_cast<size_t>(bl.off + i * 0x20);
            if (e + 0x20 > state1.size()) break;
            W64(dst, e + 0x00, IdxToAbs(RAt<uint64_t>(dst, sz, e + 0x00), a_req, 0x14));
        }
    };
    FixUntimed(bl_sprop);
    FixUntimed(bl_eprop);

    // -- input_sequences ----------------------------------------------
    for (uint64_t i = 0; i < bl_inseq.cnt; ++i)
    {
        size_t e = static_cast<size_t>(bl_inseq.off + i * 0x10);
        if (e + 0x10 > state1.size()) break;
        W64(dst, e + 0x08, IdxToAbs(RAt<uint64_t>(dst, sz, e + 0x08), a_inex, 0x08));
    }

    // -- projectiles --------------------------------------------------
    // Game populateMotbin uses `if (idx)` — index 0 means null (unlike move
    // hit_condition where 0 is a valid first entry). State-1 stores null as 0
    // (ToIdx preserves abs 0) or as the FFFF.. sentinel.
    auto ProjIdxToAbs = [](uint64_t idx, uint64_t blockAbs, size_t stride) -> uint64_t {
        if (idx == 0 || IsNullIdx(idx)) return 0;
        return blockAbs + idx * stride;
    };
    for (uint64_t i = 0; i < bl_proj.cnt; ++i)
    {
        size_t e = static_cast<size_t>(bl_proj.off + i * 0xE0);
        if (e + 0xE0 > state1.size()) break;
        uint64_t hc = RAt<uint64_t>(dst, sz, e + 0x90);
        uint64_t cn = RAt<uint64_t>(dst, sz, e + 0x98);
        W64(dst, e + 0x90, ProjIdxToAbs(hc, a_hitc, 0x18));
        W64(dst, e + 0x98, ProjIdxToAbs(cn, a_can, 0x28));
        // Derived fields from populateMotbin
        uint32_t u2_3 = RAt<uint32_t>(dst, sz, e + 0xAC);
        uint32_t u2_4 = RAt<uint32_t>(dst, sz, e + 0xB0);
        if (!u2_3)
            W32(dst, e + 0xAC, 2u * RAt<uint32_t>(dst, sz, e + 0x30));
        if (!u2_4)
            W32(dst, e + 0xB0, RAt<uint32_t>(dst, sz, e + 0x34));
    }

    // -- pushbacks ----------------------------------------------------
    for (uint64_t i = 0; i < bl_push.cnt; ++i)
    {
        size_t e = static_cast<size_t>(bl_push.off + i * 0x10);
        if (e + 0x10 > state1.size()) break;
        W64(dst, e + 0x08, IdxToAbs(RAt<uint64_t>(dst, sz, e + 0x08), a_pushex, 0x02));
    }

    // -- throws -------------------------------------------------------
    for (uint64_t i = 0; i < bl_thr.cnt; ++i)
    {
        size_t e = static_cast<size_t>(bl_thr.off + i * 0x10);
        if (e + 0x10 > state1.size()) break;
        W64(dst, e + 0x08, IdxToAbs(RAt<uint64_t>(dst, sz, e + 0x08), a_threx, 0x0C));
    }

    // -- dialogues ----------------------------------------------------
    for (uint64_t i = 0; i < bl_dia.cnt; ++i)
    {
        size_t e = static_cast<size_t>(bl_dia.off + i * 0x18);
        if (e + 0x18 > state1.size()) break;
        W64(dst, e + 0x08, IdxToAbs(RAt<uint64_t>(dst, sz, e + 0x08), a_req, 0x14));
    }

    // -- moves --------------------------------------------------------
    for (uint64_t i = 0; i < bl_moves.cnt; ++i)
    {
        size_t e = static_cast<size_t>(bl_moves.off + i * 0x448);
        if (e + 0x448 > state1.size()) break;

        const ParsedMove* pm = (i < model.moves.size()) ? &model.moves[static_cast<size_t>(i)] : nullptr;

        const uint8_t* liveMove = nullptr;
        if (haveLive && i < liveMovesCnt)
        {
            size_t le = liveMovesOff + static_cast<size_t>(i * 0x448);
            if (le + 0x448 <= liveSize)
                liveMove = liveState3 + le;
        }

        // name/anim string ptrs: reuse live game static "?" when available
        if (liveMove)
        {
            memcpy(dst + e + 0x40, liveMove + 0x40, 8);
            memcpy(dst + e + 0x48, liveMove + 0x48, 8);
        }
        else
        {
            W64(dst, e + 0x40, qMarkAbs);
            W64(dst, e + 0x48, qMarkAbs);
        }

        // Encrypt six fields (value@+0x00, key@+0x08; related@+0x10.. zeroed).
        auto writeEnc = [&](size_t off, uint32_t value) {
            memset(dst + e + off, 0, 0x20);
            TkEncrypt32(dst + e + off, value);
        };

        if (pm)
        {
            writeEnc(0x00, pm->name_key);
            writeEnc(0x20, pm->anim_key);
            writeEnc(0x58, pm->vuln);
            writeEnc(0x78, pm->hitlevel);
            writeEnc(0xD0, pm->ordinal_id2);
            writeEnc(0xF0, pm->moveId);

            // Anim handle (live body id): lo@0x50 / hi@0x54 — NOT anim_key.
            // anim_key is only the encrypted name index at +0x20.
            uint32_t lo = pm->anim_handle_lo;
            uint32_t hi = pm->anim_handle_hi ? pm->anim_handle_hi : dominantHi;
            W32(dst, e + 0x50, lo);
            W32(dst, e + 0x54, hi);
            W32(dst, e + 0x120, static_cast<uint32_t>(pm->anim_len));
        }

        // Pointer conversions
        W64(dst, e + 0x110, IdxToAbs(RAt<uint64_t>(dst, sz, e + 0x110), a_hitc, 0x18));
        W64(dst, e + 0x98,  IdxToAbs(RAt<uint64_t>(dst, sz, e + 0x98),  a_can,  0x28));

        // cancel1/2/3: only when related > 0
        // Layout: cancel1_addr@0xA0 related@0xA8, cancel2@0xB0 related@0xB8, cancel3@0xC0 related@0xC8
        {
            int32_t r1 = RAt<int32_t>(dst, sz, e + 0xA8);
            int32_t r2 = RAt<int32_t>(dst, sz, e + 0xB8);
            uint32_t r3 = RAt<uint32_t>(dst, sz, e + 0xC8);
            if (r1 <= 0) W64(dst, e + 0xA0, 0);
            else W64(dst, e + 0xA0, IdxToAbsOpt(RAt<uint64_t>(dst, sz, e + 0xA0), a_can, 0x28));
            if (r2 <= 0) W64(dst, e + 0xB0, 0);
            else W64(dst, e + 0xB0, IdxToAbsOpt(RAt<uint64_t>(dst, sz, e + 0xB0), a_can, 0x28));
            if (static_cast<int32_t>(r3) <= 0) W64(dst, e + 0xC0, 0);
            else W64(dst, e + 0xC0, IdxToAbsOpt(RAt<uint64_t>(dst, sz, e + 0xC0), a_can, 0x28));
        }

        W64(dst, e + 0x130, IdxToAbsOpt(RAt<uint64_t>(dst, sz, e + 0x130), a_voice,  0x0C));
        W64(dst, e + 0x138, IdxToAbsOpt(RAt<uint64_t>(dst, sz, e + 0x138), a_exprop, 0x28));
        W64(dst, e + 0x140, IdxToAbsOpt(RAt<uint64_t>(dst, sz, e + 0x140), a_sprop,  0x20));
        W64(dst, e + 0x148, IdxToAbsOpt(RAt<uint64_t>(dst, sz, e + 0x148), a_eprop,  0x20));
    }

    (void)a_parry;
    (void)bl_parry;
    return out;
}

std::vector<uint8_t> RoundTripRuntimeBlob(const std::vector<uint8_t>& state3Live,
                                          uint64_t originalBase,
                                          const MotbinData* modelOrNull,
                                          uint32_t charId,
                                          std::string& err)
{
    auto state1 = ExportLoaderBin(state3Live, originalBase, nullptr);
    if (state1.empty())
    {
        err = "ExportLoaderBin failed";
        return {};
    }

    MotbinData synth;
    const MotbinData* model = modelOrNull;
    if (!model)
    {
        uint64_t movesOff = 0, movesCnt = 0;
        if (state1.size() >= 0x240)
        {
            memcpy(&movesOff, state1.data() + 0x230, 8);
            memcpy(&movesCnt, state1.data() + 0x238, 8);
        }
        size_t mOff = static_cast<size_t>(movesOff + kBase);
        size_t liveMoveOff = 0;
        {
            uint64_t absMoves = 0;
            if (state3Live.size() >= 0x238)
                memcpy(&absMoves, state3Live.data() + 0x230, 8);
            if (absMoves >= originalBase)
                liveMoveOff = static_cast<size_t>(absMoves - originalBase);
        }
        synth.moves.resize(static_cast<size_t>(movesCnt));
        for (uint64_t i = 0; i < movesCnt; ++i)
        {
            size_t e = mOff + static_cast<size_t>(i * 0x448);
            size_t le = liveMoveOff + static_cast<size_t>(i * 0x448);
            auto& pm = synth.moves[static_cast<size_t>(i)];
            if (le + 0x124 <= state3Live.size())
            {
                memcpy(&pm.anim_handle_lo, state3Live.data() + le + 0x50, 4);
                memcpy(&pm.anim_handle_hi, state3Live.data() + le + 0x54, 4);
                uint32_t alen = 0;
                memcpy(&alen, state3Live.data() + le + 0x120, 4);
                pm.anim_len = static_cast<int32_t>(alen);
            }
            if (e + 0x100 <= state1.size())
            {
                const uint8_t* mb = state1.data() + e;
                pm.name_key    = DecryptMotbinMoveKey(mb, 0x00);
                pm.anim_key    = DecryptMotbinMoveKey(mb, 0x20);
                pm.vuln        = DecryptMotbinMoveKey(mb, 0x58);
                pm.hitlevel    = DecryptMotbinMoveKey(mb, 0x78);
                pm.ordinal_id2 = DecryptMotbinMoveKey(mb, 0xD0);
                pm.moveId      = DecryptMotbinMoveKey(mb, 0xF0);
            }
        }
        model = &synth;
    }
    return BuildRuntimeBlob(state1, *model, originalBase, charId, err,
                            state3Live.data(), state3Live.size(), originalBase);
}
