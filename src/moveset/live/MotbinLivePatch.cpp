// MotbinLivePatch.cpp
// Patch individual elements into an already-injected runtime motbin.
#include "moveset/live/MotbinLivePatch.h"
#include "moveset/live/MovesetInjector.h"
#include "moveset/serialize/MotbinRuntime.h"
#include "extract/GameProcess.h"
#include <cstring>
#include <vector>

namespace {

struct BlockBases {
    uint64_t react  = 0;
    uint64_t req    = 0;
    uint64_t hitc   = 0;
    uint64_t proj   = 0;
    uint64_t push   = 0;
    uint64_t pushex = 0;
    uint64_t can    = 0;
    uint64_t gcan   = 0;
    uint64_t canex  = 0;
    uint64_t exprop = 0;
    uint64_t sprop  = 0;
    uint64_t eprop  = 0;
    uint64_t moves  = 0;
    uint64_t voice  = 0;
    uint64_t inseq  = 0;
    uint64_t inex   = 0;
    uint64_t parry  = 0;
    uint64_t threx  = 0;
    uint64_t thr    = 0;
    uint64_t dia    = 0;
};

struct PatchSession {
    GameProcessInfo gp;
    uintptr_t       inject = 0;
    BlockBases      b;
};

static bool OpenSession(int playerId, PatchSession& s)
{
    s.inject = MovesetInjector::GetInjectedAddr(playerId);
    if (!s.inject) return false;
    if (!FindGameProcess(s.gp)) return false;

    auto rd = [&](uintptr_t off, uint64_t& out) -> bool {
        return ReadGameValue(s.gp, s.inject + off, out);
    };
    if (!rd(0x168, s.b.react)  || !rd(0x180, s.b.req)    || !rd(0x190, s.b.hitc) ||
        !rd(0x1A0, s.b.proj)   || !rd(0x1B0, s.b.push)   || !rd(0x1C0, s.b.pushex) ||
        !rd(0x1D0, s.b.can)    || !rd(0x1E0, s.b.gcan)   || !rd(0x1F0, s.b.canex) ||
        !rd(0x200, s.b.exprop) || !rd(0x210, s.b.sprop)  || !rd(0x220, s.b.eprop) ||
        !rd(0x230, s.b.moves)  || !rd(0x240, s.b.voice)  || !rd(0x250, s.b.inseq) ||
        !rd(0x260, s.b.inex)   || !rd(0x270, s.b.parry)  || !rd(0x280, s.b.threx) ||
        !rd(0x290, s.b.thr)    || !rd(0x2A0, s.b.dia))
    {
        CloseGameProcess(s.gp);
        return false;
    }
    return true;
}

static void CloseSession(PatchSession& s)
{
    CloseGameProcess(s.gp);
}

static uint64_t IdxToAbs(uint32_t idx, uint64_t blockAbs, size_t stride)
{
    if (idx == 0xFFFFFFFFu || !blockAbs) return 0;
    return blockAbs + static_cast<uint64_t>(idx) * stride;
}

static uint64_t ProjIdxToAbs(uint32_t idx, uint64_t blockAbs, size_t stride)
{
    if (idx == 0 || idx == 0xFFFFFFFFu || !blockAbs) return 0;
    return blockAbs + static_cast<uint64_t>(idx) * stride;
}

static void W32(uint8_t* p, size_t off, uint32_t v) { memcpy(p + off, &v, 4); }
static void W16(uint8_t* p, size_t off, uint16_t v) { memcpy(p + off, &v, 2); }
static void W64(uint8_t* p, size_t off, uint64_t v) { memcpy(p + off, &v, 8); }

static bool WriteBytes(PatchSession& s, uintptr_t addr, const void* buf, size_t n)
{
    return WriteGameMemory(s.gp, addr, buf, n);
}

static bool WriteAt(PatchSession& s, uint64_t blockAbs, uint32_t idx, size_t stride,
                    const void* buf, size_t n)
{
    if (!blockAbs) return false;
    const uintptr_t addr = static_cast<uintptr_t>(blockAbs + static_cast<uint64_t>(idx) * stride);
    return WriteBytes(s, addr, buf, n);
}

static bool ReadElem(PatchSession& s, uint64_t blockAbs, uint32_t idx, size_t stride,
                     void* buf, size_t n)
{
    if (!blockAbs) return false;
    const uintptr_t addr = static_cast<uintptr_t>(blockAbs + static_cast<uint64_t>(idx) * stride);
    return ReadGameMemory(s.gp, addr, buf, n);
}

static void WriteEnc(uint8_t* dst, size_t off, uint32_t value)
{
    memset(dst + off, 0, 0x20);
    TkEncrypt32(dst + off, value);
}

static void PackCancel(uint8_t* e, const ParsedCancel& c, uint64_t a_req, uint64_t a_canex)
{
    memset(e, 0, 0x28);
    W64(e, 0x00, c.command);
    W64(e, 0x08, IdxToAbs(c.req_list_idx, a_req, 0x14));
    W64(e, 0x10, IdxToAbs(c.extradata_idx, a_canex, 0x04));
    W32(e, 0x18, c.frame_window_start);
    W32(e, 0x1C, c.frame_window_end);
    W32(e, 0x20, c.starting_frame);
    W16(e, 0x24, c.move_id);
    W16(e, 0x26, c.cancel_option);
}

static void PackExtraProp28(uint8_t* e, const ParsedExtraProp& p, uint64_t a_req)
{
    memset(e, 0, 0x28);
    W32(e, 0x00, p.type);
    W32(e, 0x04, p._0x4);
    W64(e, 0x08, IdxToAbs(p.req_list_idx, a_req, 0x14));
    W32(e, 0x10, p.id);
    W32(e, 0x14, p.value);
    W32(e, 0x18, p.value2);
    W32(e, 0x1C, p.value3);
    W32(e, 0x20, p.value4);
    W32(e, 0x24, p.value5);
}

static void PackExtraProp20(uint8_t* e, const ParsedExtraProp& p, uint64_t a_req)
{
    memset(e, 0, 0x20);
    W64(e, 0x00, IdxToAbs(p.req_list_idx, a_req, 0x14));
    W32(e, 0x08, p.id);
    W32(e, 0x0C, p.value);
    W32(e, 0x10, p.value2);
    W32(e, 0x14, p.value3);
    W32(e, 0x18, p.value4);
    W32(e, 0x1C, p.value5);
}

} // namespace

namespace MotbinLivePatch {

bool WriteRequirement(const MotbinData& data, int playerId, uint32_t idx)
{
    if (idx >= data.requirementBlock.size()) return false;
    PatchSession s;
    if (!OpenSession(playerId, s)) return false;
    const auto& r = data.requirementBlock[idx];
    uint8_t buf[0x14] = {};
    W32(buf, 0x00, r.req);
    W32(buf, 0x04, r.param);
    W32(buf, 0x08, r.param2);
    W32(buf, 0x0C, r.param3);
    W32(buf, 0x10, r.param4);
    const bool ok = WriteAt(s, s.b.req, idx, 0x14, buf, 0x14);
    CloseSession(s);
    return ok;
}

bool WriteCancel(const MotbinData& data, int playerId, uint32_t idx)
{
    if (idx >= data.cancelBlock.size()) return false;
    PatchSession s;
    if (!OpenSession(playerId, s)) return false;
    uint8_t buf[0x28];
    PackCancel(buf, data.cancelBlock[idx], s.b.req, s.b.canex);
    const bool ok = WriteAt(s, s.b.can, idx, 0x28, buf, 0x28);
    CloseSession(s);
    return ok;
}

bool WriteGroupCancel(const MotbinData& data, int playerId, uint32_t idx)
{
    if (idx >= data.groupCancelBlock.size()) return false;
    PatchSession s;
    if (!OpenSession(playerId, s)) return false;
    uint8_t buf[0x28];
    PackCancel(buf, data.groupCancelBlock[idx], s.b.req, s.b.canex);
    const bool ok = WriteAt(s, s.b.gcan, idx, 0x28, buf, 0x28);
    CloseSession(s);
    return ok;
}

bool WriteCancelExtra(const MotbinData& data, int playerId, uint32_t idx)
{
    if (idx >= data.cancelExtraBlock.size()) return false;
    PatchSession s;
    if (!OpenSession(playerId, s)) return false;
    uint32_t v = data.cancelExtraBlock[idx];
    const bool ok = WriteAt(s, s.b.canex, idx, 0x04, &v, 4);
    CloseSession(s);
    return ok;
}

bool WriteHitCondition(const MotbinData& data, int playerId, uint32_t idx)
{
    if (idx >= data.hitConditionBlock.size()) return false;
    PatchSession s;
    if (!OpenSession(playerId, s)) return false;
    const auto& h = data.hitConditionBlock[idx];
    uint8_t buf[0x18] = {};
    W64(buf, 0x00, IdxToAbs(h.req_list_idx, s.b.req, 0x14));
    W32(buf, 0x08, h.damage);
    W32(buf, 0x0C, h._0x0C);
    W64(buf, 0x10, IdxToAbs(h.reaction_list_idx, s.b.react, 0x70));
    const bool ok = WriteAt(s, s.b.hitc, idx, 0x18, buf, 0x18);
    CloseSession(s);
    return ok;
}

bool WriteReaction(const MotbinData& data, int playerId, uint32_t idx)
{
    if (idx >= data.reactionListBlock.size()) return false;
    PatchSession s;
    if (!OpenSession(playerId, s)) return false;
    const auto& r = data.reactionListBlock[idx];
    uint8_t buf[0x70] = {};
    for (int i = 0; i < 7; ++i)
        W64(buf, i * 8, IdxToAbs(r.pushback_idx[i], s.b.push, 0x10));
    W16(buf, 0x38, r.front_direction);
    W16(buf, 0x3A, r.back_direction);
    W16(buf, 0x3C, r.left_side_direction);
    W16(buf, 0x3E, r.right_side_direction);
    W16(buf, 0x40, r.front_ch_direction);
    W16(buf, 0x42, r.downed_direction);
    W16(buf, 0x44, r.front_rotation);
    W16(buf, 0x46, r.back_rotation);
    W16(buf, 0x48, r.left_side_rotation);
    W16(buf, 0x4A, r.right_side_rotation);
    W16(buf, 0x4C, r.vertical_pushback);
    W16(buf, 0x4E, r.downed_rotation);
    W16(buf, 0x50, r.standing);
    W16(buf, 0x52, r.crouch);
    W16(buf, 0x54, r.ch);
    W16(buf, 0x56, r.crouch_ch);
    W16(buf, 0x58, r.left_side);
    W16(buf, 0x5A, r.left_side_crouch);
    W16(buf, 0x5C, r.right_side);
    W16(buf, 0x5E, r.right_side_crouch);
    W16(buf, 0x60, r.back);
    W16(buf, 0x62, r.back_crouch);
    W16(buf, 0x64, r.block);
    W16(buf, 0x66, r.crouch_block);
    W16(buf, 0x68, r.wallslump);
    W16(buf, 0x6A, r.downed);
    const bool ok = WriteAt(s, s.b.react, idx, 0x70, buf, 0x70);
    CloseSession(s);
    return ok;
}

bool WritePushback(const MotbinData& data, int playerId, uint32_t idx)
{
    if (idx >= data.pushbackBlock.size()) return false;
    PatchSession s;
    if (!OpenSession(playerId, s)) return false;
    const auto& p = data.pushbackBlock[idx];
    uint8_t buf[0x10] = {};
    W16(buf, 0x00, p.val1);
    W16(buf, 0x02, p.val2);
    W32(buf, 0x04, p.val3);
    W64(buf, 0x08, IdxToAbs(p.pushback_extra_idx, s.b.pushex, 0x02));
    const bool ok = WriteAt(s, s.b.push, idx, 0x10, buf, 0x10);
    CloseSession(s);
    return ok;
}

bool WritePushbackExtra(const MotbinData& data, int playerId, uint32_t idx)
{
    if (idx >= data.pushbackExtraBlock.size()) return false;
    PatchSession s;
    if (!OpenSession(playerId, s)) return false;
    uint16_t v = data.pushbackExtraBlock[idx].value;
    const bool ok = WriteAt(s, s.b.pushex, idx, 0x02, &v, 2);
    CloseSession(s);
    return ok;
}

bool WriteExtraProp(const MotbinData& data, int playerId, uint32_t idx)
{
    if (idx >= data.extraPropBlock.size()) return false;
    PatchSession s;
    if (!OpenSession(playerId, s)) return false;
    uint8_t buf[0x28];
    PackExtraProp28(buf, data.extraPropBlock[idx], s.b.req);
    const bool ok = WriteAt(s, s.b.exprop, idx, 0x28, buf, 0x28);
    CloseSession(s);
    return ok;
}

bool WriteStartProp(const MotbinData& data, int playerId, uint32_t idx)
{
    if (idx >= data.startPropBlock.size()) return false;
    PatchSession s;
    if (!OpenSession(playerId, s)) return false;
    uint8_t buf[0x20];
    PackExtraProp20(buf, data.startPropBlock[idx], s.b.req);
    const bool ok = WriteAt(s, s.b.sprop, idx, 0x20, buf, 0x20);
    CloseSession(s);
    return ok;
}

bool WriteEndProp(const MotbinData& data, int playerId, uint32_t idx)
{
    if (idx >= data.endPropBlock.size()) return false;
    PatchSession s;
    if (!OpenSession(playerId, s)) return false;
    uint8_t buf[0x20];
    PackExtraProp20(buf, data.endPropBlock[idx], s.b.req);
    const bool ok = WriteAt(s, s.b.eprop, idx, 0x20, buf, 0x20);
    CloseSession(s);
    return ok;
}

bool WriteMove(const MotbinData& data, int playerId, uint32_t idx)
{
    if (idx >= data.moves.size()) return false;
    PatchSession s;
    if (!OpenSession(playerId, s)) return false;

    uint8_t buf[0x448];
    if (!ReadElem(s, s.b.moves, idx, 0x448, buf, 0x448))
    {
        CloseSession(s);
        return false;
    }

    // Preserve +0x40/+0x48 (string ptrs) and +0x50/+0x54 (anim handles) from live.
    uint8_t preserved[0x18];
    memcpy(preserved, buf + 0x40, 0x18);

    const ParsedMove& m = data.moves[idx];

    WriteEnc(buf, 0x00, m.name_key);
    WriteEnc(buf, 0x20, m.anim_key);
    WriteEnc(buf, 0x58, m.vuln);
    WriteEnc(buf, 0x78, m.hitlevel);
    WriteEnc(buf, 0xD0, m.ordinal_id2);
    WriteEnc(buf, 0xF0, m.moveId);

    // Restore string ptrs + anim handles (enc wipes do not cover +0x40..+0x57,
    // but keep this explicit so we never overwrite inject-time values).
    memcpy(buf + 0x40, preserved, 0x18);
    W16(buf, 0xCC, m.transition);
    W16(buf, 0xCE, static_cast<uint16_t>(m._0xCE));
    W32(buf, 0x118, m._0x118);
    W32(buf, 0x11C, m._0x11C);
    W32(buf, 0x120, static_cast<uint32_t>(m.anim_len));
    W32(buf, 0x124, m.airborne_start);
    W32(buf, 0x128, m.airborne_end);
    W32(buf, 0x12C, m.ground_fall);
    W32(buf, 0x150, m.u15);
    W32(buf, 0x154, m._0x154);
    W32(buf, 0x158, m.startup);
    W32(buf, 0x15C, m.recovery);

    for (int h = 0; h < 8; ++h)
    {
        size_t hb = 0x160 + h * 0x30;
        W32(buf, hb + 0x00, m.hitbox_active_start[h]);
        W32(buf, hb + 0x04, m.hitbox_active_last[h]);
        W32(buf, hb + 0x08, m.hitbox_location[h]);
        for (int f = 0; f < 9; ++f)
            memcpy(buf + hb + 0x0C + f * 4, &m.hitbox_floats[h][f], 4);
    }
    W16(buf, 0x2E0, m.collision);
    W16(buf, 0x2E2, m.distance);
    memcpy(buf + 0x2E4, m.unk5, sizeof(m.unk5));
    W32(buf, 0x444, m.u18);

    // Pointers from editor indices
    W64(buf, 0x98, IdxToAbs(m.cancel_idx, s.b.can, 0x28));
    W64(buf, 0x110, IdxToAbs(m.hit_condition_idx, s.b.hitc, 0x18));
    W64(buf, 0x130, IdxToAbs(m.voiceclip_idx, s.b.voice, 0x0C));
    W64(buf, 0x138, IdxToAbs(m.extra_prop_idx, s.b.exprop, 0x28));
    W64(buf, 0x140, IdxToAbs(m.start_prop_idx, s.b.sprop, 0x20));
    W64(buf, 0x148, IdxToAbs(m.end_prop_idx, s.b.eprop, 0x20));

    // cancel1 @0xA0: use cancel2_idx; null when related@0xA8 <= 0
    {
        int32_t r1 = 0;
        memcpy(&r1, buf + 0xA8, 4);
        if (r1 <= 0)
            W64(buf, 0xA0, 0);
        else
            W64(buf, 0xA0, IdxToAbs(m.cancel2_idx, s.b.can, 0x28));
    }
    // cancel2/3: keep live related; null ptr if related <= 0
    {
        int32_t r2 = 0;
        memcpy(&r2, buf + 0xB8, 4);
        if (r2 <= 0) W64(buf, 0xB0, 0);
        // else leave existing abs at 0xB0 (no model index)

        int32_t r3 = 0;
        memcpy(&r3, buf + 0xC8, 4);
        if (r3 <= 0) W64(buf, 0xC0, 0);
    }

    const bool ok = WriteAt(s, s.b.moves, idx, 0x448, buf, 0x448);
    CloseSession(s);
    return ok;
}

bool WriteVoiceclip(const MotbinData& data, int playerId, uint32_t idx)
{
    if (idx >= data.voiceclipBlock.size()) return false;
    PatchSession s;
    if (!OpenSession(playerId, s)) return false;
    const auto& v = data.voiceclipBlock[idx];
    uint8_t buf[0x0C] = {};
    W32(buf, 0x00, v.val1);
    W32(buf, 0x04, v.val2);
    W32(buf, 0x08, v.val3);
    const bool ok = WriteAt(s, s.b.voice, idx, 0x0C, buf, 0x0C);
    CloseSession(s);
    return ok;
}

bool WriteInput(const MotbinData& data, int playerId, uint32_t idx)
{
    if (idx >= data.inputBlock.size()) return false;
    PatchSession s;
    if (!OpenSession(playerId, s)) return false;
    uint64_t cmd = data.inputBlock[idx].command;
    const bool ok = WriteAt(s, s.b.inex, idx, 0x08, &cmd, 8);
    CloseSession(s);
    return ok;
}

bool WriteInputSequence(const MotbinData& data, int playerId, uint32_t idx)
{
    if (idx >= data.inputSequenceBlock.size()) return false;
    PatchSession s;
    if (!OpenSession(playerId, s)) return false;
    const auto& q = data.inputSequenceBlock[idx];
    uint8_t buf[0x10] = {};
    W16(buf, 0x00, q.input_window_frames);
    W16(buf, 0x02, q.input_amount);
    W32(buf, 0x04, q._0x4);
    W64(buf, 0x08, IdxToAbs(q.input_start_idx, s.b.inex, 0x08));
    const bool ok = WriteAt(s, s.b.inseq, idx, 0x10, buf, 0x10);
    CloseSession(s);
    return ok;
}

bool WriteProjectile(const MotbinData& data, int playerId, uint32_t idx)
{
    if (idx >= data.projectileBlock.size()) return false;
    PatchSession s;
    if (!OpenSession(playerId, s)) return false;

    uint8_t buf[0xE0];
    if (!ReadElem(s, s.b.proj, idx, 0xE0, buf, 0xE0))
    {
        CloseSession(s);
        return false;
    }

    const auto& p = data.projectileBlock[idx];
    // u1 occupies +0x00.. — model stores 36×u32; file uses 35 + pad before ptrs
    memcpy(buf, p.u1, sizeof(p.u1) <= 0x90 ? sizeof(p.u1) : 0x90);
    W64(buf, 0x90, ProjIdxToAbs(p.hit_condition_idx, s.b.hitc, 0x18));
    W64(buf, 0x98, ProjIdxToAbs(p.cancel_idx, s.b.can, 0x28));
    memcpy(buf + 0xA0, p.u2, sizeof(p.u2));

    uint32_t u2_3 = 0, u2_4 = 0, v30 = 0, v34 = 0;
    memcpy(&u2_3, buf + 0xAC, 4);
    memcpy(&u2_4, buf + 0xB0, 4);
    memcpy(&v30, buf + 0x30, 4);
    memcpy(&v34, buf + 0x34, 4);
    if (!u2_3) W32(buf, 0xAC, 2u * v30);
    if (!u2_4) W32(buf, 0xB0, v34);

    const bool ok = WriteAt(s, s.b.proj, idx, 0xE0, buf, 0xE0);
    CloseSession(s);
    return ok;
}

bool WriteThrowExtra(const MotbinData& data, int playerId, uint32_t idx)
{
    if (idx >= data.throwExtraBlock.size()) return false;
    PatchSession s;
    if (!OpenSession(playerId, s)) return false;
    const auto& t = data.throwExtraBlock[idx];
    uint8_t buf[0x0C] = {};
    W32(buf, 0x00, t.pick_probability);
    W16(buf, 0x04, t.camera_type);
    W16(buf, 0x06, t.left_side_camera_data);
    W16(buf, 0x08, t.right_side_camera_data);
    W16(buf, 0x0A, t.additional_rotation);
    const bool ok = WriteAt(s, s.b.threx, idx, 0x0C, buf, 0x0C);
    CloseSession(s);
    return ok;
}

bool WriteThrow(const MotbinData& data, int playerId, uint32_t idx)
{
    if (idx >= data.throwBlock.size()) return false;
    PatchSession s;
    if (!OpenSession(playerId, s)) return false;
    const auto& t = data.throwBlock[idx];
    uint8_t buf[0x10] = {};
    W64(buf, 0x00, t.side);
    W64(buf, 0x08, IdxToAbs(t.throwextra_idx, s.b.threx, 0x0C));
    const bool ok = WriteAt(s, s.b.thr, idx, 0x10, buf, 0x10);
    CloseSession(s);
    return ok;
}

bool WriteParryable(const MotbinData& data, int playerId, uint32_t idx)
{
    if (idx >= data.parryableMoveBlock.size()) return false;
    PatchSession s;
    if (!OpenSession(playerId, s)) return false;
    uint32_t v = data.parryableMoveBlock[idx].value;
    const bool ok = WriteAt(s, s.b.parry, idx, 0x04, &v, 4);
    CloseSession(s);
    return ok;
}

bool WriteDialogue(const MotbinData& data, int playerId, uint32_t idx)
{
    if (idx >= data.dialogueBlock.size()) return false;
    PatchSession s;
    if (!OpenSession(playerId, s)) return false;
    const auto& d = data.dialogueBlock[idx];
    uint8_t buf[0x18] = {};
    W16(buf, 0x00, d.type);
    W16(buf, 0x02, d.id);
    W32(buf, 0x04, d._0x4);
    W64(buf, 0x08, IdxToAbs(d.req_list_idx, s.b.req, 0x14));
    W32(buf, 0x10, d.voiceclip_key);
    W32(buf, 0x14, d.facial_anim_idx);
    const bool ok = WriteAt(s, s.b.dia, idx, 0x18, buf, 0x18);
    CloseSession(s);
    return ok;
}

} // namespace MotbinLivePatch
