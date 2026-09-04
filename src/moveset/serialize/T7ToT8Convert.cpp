// T7ToT8Convert.cpp — T7 Moveset → MotbinData (T8-compatible)
#include "T7ToT8Convert.h"
#include "extract/T7Constants.h"
#include "moveset/data/T7AliasDict.h"
#include "moveset/data/KamuiHash.h"
#include "moveset/serialize/MotbinSerialize.h"
#include <cstring>
#include <cstdio>
#include <cmath>

using namespace T7;

static constexpr size_t kHdr = 0x318;
static constexpr uint32_t kNullIdx = 0xFFFFFFFF;

static void StoreXorBlock(ParsedMove& m, size_t which, uint32_t value, uint32_t moveIdx)
{
    // which: 0=name, 1=anim, 2=vuln, 3=hitlevel, 4=ordinal2, 5=ordinal
    uint8_t block[0x20];
    MotbinXorEncryptBlock(block, value, moveIdx);
    auto copyNameStyle = [&](uint64_t& a, uint64_t& b, uint32_t* rel4) {
        memcpy(&a, block + 0, 8);
        memcpy(&b, block + 8, 8);
        for (int i = 0; i < 4; ++i)
            memcpy(&rel4[i], block + 16 + i * 4, 4);
    };
    switch (which) {
    case 0:
        copyNameStyle(m.encrypted_name_key, m.name_encryption_key, m.name_related);
        m.name_key = value;
        break;
    case 1:
        // XOR fills 0x20-0x3F; string offs at 0x40/0x48 go in anim_related[4..7]
        memcpy(&m.encrypted_anim_key, block + 0, 8);
        memcpy(&m.anim_encryption_key, block + 8, 8);
        for (int i = 0; i < 4; ++i)
            memcpy(&m.anim_related[i], block + 16 + i * 4, 4);
        m.anim_key = value;
        break;
    case 2:
        copyNameStyle(m.encrypted_vuln, m.vuln_encryption_key, m.vuln_related);
        m.vuln = value;
        break;
    case 3:
        copyNameStyle(m.encrypted_hitlevel, m.hitlevel_encryption_key, m.hitlevel_related);
        m.hitlevel = value;
        break;
    case 4:
        copyNameStyle(m.encrypted_ordinal_id2, m.ordinal_id2_enc_key, m.ordinal_id2_related);
        m.ordinal_id2 = value;
        break;
    case 5:
        copyNameStyle(m.encrypted_ordinal_id, m.ordinal_encryption_key, m.ordinal_related);
        m.moveId = value;
        break;
    }
}

static void SplitVoice(uint32_t t7, ParsedVoiceclip& vc)
{
    if (t7 == 0xFFFFFFFFu) {
        vc.val1 = vc.val2 = vc.val3 = 0xFFFFFFFFu;
        return;
    }
    vc.val1 = (t7 >> 24) & 0xFF;
    vc.val2 = 0;
    vc.val3 = t7 & 0xFFFF;
}

bool ConvertT7ToMotbin(const Moveset& src, uint32_t t7FighterId,
                       MotbinData& out, std::string& errorMsg,
                       std::string* optionalWarn)
{
    out = {};
    if (!src.valid || src.moves.empty()) {
        errorMsg = "Empty or invalid T7 moveset.";
        return false;
    }

    const T7AliasDict& al = T7AliasDict::Get();
    int unmappedReq = 0, unmappedProp = 0;

    // ---- Requirements 0x08 → 0x14 ----
    out.requirementBlock.reserve(src.requirements.size());
    for (const auto& r : src.requirements) {
        ParsedRequirement pr = {};
        uint32_t mapped = 0;
        if (al.MapRequirement(r.condition, mapped))
            pr.req = mapped;
        else {
            pr.req = r.condition; // keep T7 id if no alias entry
            ++unmappedReq;
        }
        pr.param = r.param;
        if (al.IsCharacterIdReq(r.condition))
            pr.param = al.MapCharacterId(r.param);
        out.requirementBlock.push_back(pr);
    }

    // ---- Cancels ----
    auto convertCancels = [&](const std::vector<Cancel>& srcC,
                              const std::vector<uint32_t>& reqIdx,
                              const std::vector<uint32_t>& exIdx,
                              std::vector<ParsedCancel>& dst) {
        dst.reserve(srcC.size());
        for (size_t i = 0; i < srcC.size(); ++i) {
            ParsedCancel c = {};
            c.command            = al.MapCancelCommand(srcC[i].command);
            c.requirement_addr   = 0;
            c.extradata_addr     = 0;
            c.frame_window_start = srcC[i].detection_start;
            c.frame_window_end   = srcC[i].detection_end;
            c.starting_frame     = srcC[i].starting_frame;
            c.move_id            = srcC[i].move_id;
            c.cancel_option      = srcC[i].cancel_option;
            c.req_list_idx       = (i < reqIdx.size()) ? reqIdx[i] : kNullIdx;
            c.extradata_idx      = (i < exIdx.size())  ? exIdx[i]  : kNullIdx;
            if (c.extradata_idx != kNullIdx && c.extradata_idx < src.cancelExtras.size())
                c.extradata_value = src.cancelExtras[c.extradata_idx].value;
            dst.push_back(c);
        }
    };
    convertCancels(src.cancels, src.cancelReqIdx, src.cancelExtraIdx, out.cancelBlock);
    convertCancels(src.groupCancels, src.groupCancelReqIdx, src.groupCancelExtraIdx, out.groupCancelBlock);

    out.cancelExtraBlock.reserve(src.cancelExtras.size());
    for (const auto& e : src.cancelExtras)
        out.cancelExtraBlock.push_back(e.value);

    // ---- Pushbacks ----
    out.pushbackExtraBlock.reserve(src.pushbackExtras.size());
    for (const auto& e : src.pushbackExtras)
        out.pushbackExtraBlock.push_back({ e.horizontal_offset });

    out.pushbackBlock.reserve(src.pushbacks.size());
    for (size_t i = 0; i < src.pushbacks.size(); ++i) {
        ParsedPushback pb = {};
        pb.val1 = src.pushbacks[i].duration;
        pb.val2 = src.pushbacks[i].displacement;
        pb.val3 = src.pushbacks[i].num_of_loops;
        pb.pushback_extra_idx = (i < src.pushbackExtraIdx.size()) ? src.pushbackExtraIdx[i] : kNullIdx;
        out.pushbackBlock.push_back(pb);
    }

    // ---- Reactions (1:1) ----
    out.reactionListBlock.reserve(src.reactions.size());
    for (size_t i = 0; i < src.reactions.size(); ++i) {
        const Reactions& r = src.reactions[i];
        ParsedReactionList rl = {};
        for (int p = 0; p < 7; ++p) {
            rl.pushback_addr[p] = 0;
            rl.pushback_idx[p] = (i * 7 + p < src.reactPushbackIdx.size())
                ? src.reactPushbackIdx[i * 7 + p] : kNullIdx;
        }
        rl.front_direction      = r.front_direction;
        rl.back_direction       = r.back_direction;
        rl.left_side_direction  = r.left_side_direction;
        rl.right_side_direction = r.right_side_direction;
        rl.front_ch_direction   = r.front_counterhit_direction;
        rl.downed_direction     = r.downed_direction;
        {
            uint16_t rot[4];
            memcpy(rot, &r._0x44, 8);
            rl.front_rotation      = rot[0];
            rl.back_rotation       = rot[1];
            rl.left_side_rotation  = rot[2];
            rl.right_side_rotation = rot[3];
        }
        rl.vertical_pushback = r.vertical_pushback;
        rl.downed_rotation   = r.downed_rotation;
        rl.standing          = r.standing;
        rl.crouch            = r.crouch;
        rl.ch                = r.ch;
        rl.crouch_ch         = r.crouch_ch;
        rl.left_side         = r.left_side;
        rl.left_side_crouch  = r.left_side_crouch;
        rl.right_side        = r.right_side;
        rl.right_side_crouch = r.right_side_crouch;
        rl.back              = r.back;
        rl.back_crouch       = r.back_crouch;
        rl.block             = r.block;
        rl.crouch_block      = r.crouch_block;
        rl.wallslump         = r.wallslump;
        rl.downed            = r.downed;
        out.reactionListBlock.push_back(rl);
    }

    // ---- Hit conditions ----
    out.hitConditionBlock.reserve(src.hitConditions.size());
    for (size_t i = 0; i < src.hitConditions.size(); ++i) {
        ParsedHitCondition h = {};
        h.damage = src.hitConditions[i].damage;
        h._0x0C  = src.hitConditions[i]._0xC;
        h.req_list_idx = (i < src.hitCondReqIdx.size()) ? src.hitCondReqIdx[i] : kNullIdx;
        h.reaction_list_idx = (i < src.hitCondReactIdx.size()) ? src.hitCondReactIdx[i] : kNullIdx;
        out.hitConditionBlock.push_back(h);
    }

    // ---- Timed extraprops 0x0C → 0x28 ----
    out.extraPropBlock.reserve(src.extraMoveProperties.size());
    for (const auto& e : src.extraMoveProperties) {
        ParsedExtraProp ep = {};
        ep.type = e.starting_frame;
        ep._0x4 = 0;
        ep.requirement_addr = 0;
        ep.req_list_idx = 0; // always 0 — T7 had no per-item req list
        uint32_t mapped = 0;
        if (al.MapRequirement(e.id, mapped))
            ep.id = mapped;
        else {
            ep.id = 0;
            ++unmappedProp;
        }
        if (al.IsSoundProp(e.id)) {
            al.MapSoundParams(e.value, ep.value, ep.value2, ep.value3, ep.value4, ep.value5);
        } else {
            ep.value = e.value;
            ep.value2 = ep.value3 = ep.value4 = ep.value5 = 0;
        }
        out.extraPropBlock.push_back(ep);
    }

    // ---- OtherMoveProperty start/end 0x10 → 0x20 ----
    auto convertOther = [&](const std::vector<OtherMoveProperty>& srcP,
                            const std::vector<uint32_t>& reqIdx,
                            std::vector<ParsedExtraProp>& dst) {
        dst.reserve(srcP.size());
        for (size_t i = 0; i < srcP.size(); ++i) {
            ParsedExtraProp ep = {};
            ep.req_list_idx = (i < reqIdx.size()) ? reqIdx[i] : kNullIdx;
            uint32_t mapped = 0;
            if (al.MapRequirement(srcP[i].extraprop, mapped))
                ep.id = mapped;
            else {
                ep.id = 0;
                ++unmappedProp;
            }
            ep.value  = srcP[i].value;
            ep.value2 = ep.value3 = ep.value4 = ep.value5 = 0;
            dst.push_back(ep);
        }
    };
    convertOther(src.moveBeginningProps, src.beginPropReqIdx, out.startPropBlock);
    convertOther(src.moveEndingProps, src.endPropReqIdx, out.endPropBlock);

    // ---- Voiceclips ----
    out.voiceclipBlock.reserve(src.voiceclips.size());
    for (const auto& v : src.voiceclips) {
        ParsedVoiceclip vc = {};
        SplitVoice(v.id, vc);
        out.voiceclipBlock.push_back(vc);
    }

    // ---- Inputs / sequences ----
    out.inputBlock.reserve(src.inputs.size());
    for (const auto& in : src.inputs)
        out.inputBlock.push_back({ in.command });

    out.inputSequenceBlock.reserve(src.inputSequences.size());
    for (size_t i = 0; i < src.inputSequences.size(); ++i) {
        ParsedInputSequence is = {};
        is.input_window_frames = src.inputSequences[i].input_window_frames;
        is.input_amount        = src.inputSequences[i].input_amount;
        is._0x4                = static_cast<uint32_t>(src.inputSequences[i]._0x4);
        is.input_start_idx     = (i < src.inputSeqInputIdx.size()) ? src.inputSeqInputIdx[i] : kNullIdx;
        out.inputSequenceBlock.push_back(is);
    }

    // ---- Parry (copy-through) ----
    out.parryableMoveBlock.reserve(src.parryRelated.size());
    for (const auto& p : src.parryRelated)
        out.parryableMoveBlock.push_back({ p.value });

    // ---- Projectiles: empty ----
    out.projectileBlock.clear();

    // ---- Throws / camera ----
    out.throwExtraBlock.reserve(src.cameraData.size());
    for (const auto& c : src.cameraData) {
        ParsedThrowExtra te = {};
        te.pick_probability       = c._0x0;
        te.camera_type            = c._0x4;
        te.left_side_camera_data  = c.left_side_camera_data;
        te.right_side_camera_data = c.right_side_camera_data;
        te.additional_rotation    = c._0xA;
        out.throwExtraBlock.push_back(te);
    }
    out.throwBlock.reserve(src.throwCameras.size());
    for (size_t i = 0; i < src.throwCameras.size(); ++i) {
        ParsedThrow t = {};
        t.side = src.throwCameras[i].side;
        t.throwextra_idx = (i < src.throwCamDataIdx.size()) ? src.throwCamDataIdx[i] : kNullIdx;
        out.throwBlock.push_back(t);
    }

    // ---- Character ID for _0xD0 ----
    uint32_t decodedFromHeader = DecodeCharId(src.encodedCharId);
    uint32_t t7IdForMap = decodedFromHeader ? decodedFromHeader : t7FighterId;
    uint32_t t8CharId = al.MapCharacterId(t7IdForMap);
    uint32_t encodedOrdinal2 = EncodeCharId(t8CharId);

    // ---- String block offsets ----
    std::string charName = src.characterName.empty() ? "CHAR" : src.characterName;
    if (charName.size() < 2 || charName.substr(charName.size() - 2) != "_n")
        charName += "_n";
    std::string creator = src.characterCreator.empty() ? "Polaris7" : src.characterCreator;
    std::string date    = src.date.empty() ? "00000000.000000" : src.date;
    std::string fullDate= src.fullDate.empty() ? (date + " 00:00:00.000") : src.fullDate;

    uint64_t strCur = 0;
    auto addStr = [&](const std::string& s) -> uint64_t {
        uint64_t off = strCur;
        strCur += s.size() + 1;
        return off;
    };
    addStr(charName);
    uint64_t creatorOff  = addStr(creator);
    uint64_t dateOff     = addStr(date);
    uint64_t fullDateOff = addStr(fullDate);

    std::vector<uint64_t> nameOffs(src.moves.size());
    std::vector<uint64_t> animOffs(src.moves.size());

    // ---- Moves ----
    out.moves.reserve(src.moves.size());
    out.moveCount = static_cast<uint32_t>(src.moves.size());
    for (size_t i = 0; i < src.moves.size(); ++i) {
        const Move& tm = src.moves[i];
        ParsedMove m = {};
        uint32_t mi = static_cast<uint32_t>(i);

        std::string nStr = (i < src.moveNames.size()) ? src.moveNames[i] : "";
        std::string aStr = (i < src.moveAnimNames.size()) ? src.moveAnimNames[i] : "";
        nameOffs[i] = addStr(nStr);
        animOffs[i] = addStr(aStr);

        uint32_t nameKey = static_cast<uint32_t>(KamuiHash::Compute(nStr));
        uint32_t animKey = static_cast<uint32_t>(KamuiHash::Compute(aStr));

        StoreXorBlock(m, 0, nameKey, mi);
        StoreXorBlock(m, 1, animKey, mi);
        StoreXorBlock(m, 2, tm.vuln, mi);
        StoreXorBlock(m, 3, tm.hitlevel, mi);
        StoreXorBlock(m, 4, encodedOrdinal2, mi);
        StoreXorBlock(m, 5, mi, mi); // ordinal_id = move index

        // String offsets into anim_related[4..7]
        m.anim_related[4] = static_cast<uint32_t>(nameOffs[i] & 0xFFFFFFFF);
        m.anim_related[5] = static_cast<uint32_t>(nameOffs[i] >> 32);
        m.anim_related[6] = static_cast<uint32_t>(animOffs[i] & 0xFFFFFFFF);
        m.anim_related[7] = static_cast<uint32_t>(animOffs[i] >> 32);

        m.anmbin_body_idx = mi;
        m.anmbin_body_sub_idx = 0;
        m.displayName = nStr.empty() ? ("move_" + std::to_string(i)) : nStr;

        m.cancel_idx = (i < src.moveCancelIdx.size()) ? src.moveCancelIdx[i] : kNullIdx;
        m.cancel2_idx = kNullIdx;
        m.hit_condition_idx = (i < src.moveHitCondIdx.size()) ? src.moveHitCondIdx[i] : kNullIdx;
        m.voiceclip_idx = (i < src.moveVoiceIdx.size()) ? src.moveVoiceIdx[i] : kNullIdx;
        m.extra_prop_idx = (i < src.moveExtraPropIdx.size()) ? src.moveExtraPropIdx[i] : kNullIdx;
        m.start_prop_idx = (i < src.moveStartPropIdx.size()) ? src.moveStartPropIdx[i] : kNullIdx;
        m.end_prop_idx = (i < src.moveEndPropIdx.size()) ? src.moveEndPropIdx[i] : kNullIdx;

        m.transition = tm.transition;
        m._0xCE = tm._0x56;
        m._0x118 = 0xFFFF;
        m._0x11C = 1;
        m.anim_len = static_cast<int32_t>(tm.anim_len);
        m.airborne_start = tm.airborne_start;
        m.airborne_end = tm.airborne_end;
        m.ground_fall = tm.ground_fall;
        m.u15 = static_cast<uint32_t>(tm._0x98);
        m._0x154 = 0;
        m.startup = tm.first_active_frame;
        m.recovery = tm.last_active_frame;
        m.collision = static_cast<uint16_t>(tm._0xA8);
        m.distance = tm.distance;

        // Hitboxes: u32 → 4 bytes
        uint8_t hb[4] = {
            static_cast<uint8_t>(tm.hitbox_location & 0xFF),
            static_cast<uint8_t>((tm.hitbox_location >> 8) & 0xFF),
            static_cast<uint8_t>((tm.hitbox_location >> 16) & 0xFF),
            static_cast<uint8_t>((tm.hitbox_location >> 24) & 0xFF),
        };
        for (int h = 0; h < 8; ++h) {
            m.hitbox_active_start[h] = 0;
            m.hitbox_active_last[h] = 0;
            m.hitbox_location[h] = 0;
            for (int f = 0; f < 9; ++f) m.hitbox_floats[h][f] = 0.f;
        }
        for (int h = 0; h < 4; ++h)
            m.hitbox_location[h] = al.MapHitbox(hb[h]);
        m.hitbox_active_start[0] = tm.first_active_frame;
        m.hitbox_active_last[0]  = tm.last_active_frame;
        m.hitbox_active_start[1] = tm.first_active_frame;
        m.hitbox_active_last[1]  = tm.last_active_frame;

        // unk5: 8 items × 11 u32
        for (int item = 0; item < 8; ++item) {
            int base = item * 11;
            auto putF = [&](int o, float f) {
                uint32_t bits; memcpy(&bits, &f, 4);
                m.unk5[base + o] = bits;
            };
            m.unk5[base + 0] = 0xFFFFFFFFu;
            m.unk5[base + 1] = 0xFFFFFFFFu;
            m.unk5[base + 2] = 0xFFFFFFFFu;
            putF(3, -1.f); putF(4, -1.f); putF(5, -1.f);
            m.unk5[base + 6] = 0;
            putF(7, 0.f); putF(8, 0.f); putF(9, 0.f);
            m.unk5[base + 10] = 0;
        }

        out.moves.push_back(std::move(m));
    }

    // ---- Synthetic header ----
    out.rawBytes.assign(kHdr, 0);
    uint8_t* h = out.rawBytes.data();

    // TEK signature
    h[0x08] = 'T'; h[0x09] = 'E'; h[0x0A] = 'K'; h[0x0B] = '\0';

    // String offsets
    auto w64 = [&](size_t off, uint64_t v) { memcpy(h + off, &v, 8); };
    auto w16 = [&](size_t off, uint16_t v) { memcpy(h + off, &v, 2); };
    w64(0x10, 0);
    w64(0x18, creatorOff);
    w64(0x20, dateOff);
    w64(0x28, fullDateOff);
    w64(0x170, strCur);

    // Aliases: copy 56, pad 4 with aliases[1]
    uint16_t idle = src.origAliases[1];
    for (size_t i = 0; i < kAliasCountT7; ++i)
        w16(0x30 + i * 2, src.origAliases[i]);
    for (size_t i = kAliasCountT7; i < kAliasCountT8; ++i)
        w16(0x30 + i * 2, idle);
    for (size_t i = 0; i < kAliasCountT7; ++i)
        w16(0xA8 + i * 2, src.currentAliases[i]);
    for (size_t i = kAliasCountT7; i < kAliasCountT8; ++i)
        w16(0xA8 + i * 2, idle);
    for (size_t i = 0; i < kUnknownAliasCount; ++i)
        w16(0x120 + i * 2, src.unknownAliases[i]);

    out.originalAliases.resize(60);
    for (size_t i = 0; i < 60; ++i)
        memcpy(&out.originalAliases[i], h + 0x30 + i * 2, 2);

    // Block counts in header (ptrs left 0 — EmitBlock uses counts)
    auto setCnt = [&](size_t cntOff, uint64_t c) { w64(cntOff, c); };
    setCnt(0x178, out.reactionListBlock.size());
    setCnt(0x188, out.requirementBlock.size());
    setCnt(0x198, out.hitConditionBlock.size());
    setCnt(0x1A8, out.projectileBlock.size());
    setCnt(0x1B8, out.pushbackBlock.size());
    setCnt(0x1C8, out.pushbackExtraBlock.size());
    setCnt(0x1D8, out.cancelBlock.size());
    setCnt(0x1E8, out.groupCancelBlock.size());
    setCnt(0x1F8, out.cancelExtraBlock.size());
    setCnt(0x208, out.extraPropBlock.size());
    setCnt(0x218, out.startPropBlock.size());
    setCnt(0x228, out.endPropBlock.size());
    setCnt(0x238, out.moves.size());
    setCnt(0x248, out.voiceclipBlock.size());
    setCnt(0x258, out.inputSequenceBlock.size());
    setCnt(0x268, out.inputBlock.size());
    setCnt(0x278, out.parryableMoveBlock.size());
    setCnt(0x288, out.throwExtraBlock.size());
    setCnt(0x298, out.throwBlock.size());
    setCnt(0x2A8, out.dialogueBlock.size());

    out.loaded = true;
    out.charaCode = charName;

    if (optionalWarn) {
        char buf[128];
        snprintf(buf, sizeof(buf),
                 "unmapped req=%d prop=%d projectiles dropped=%zu charId=%u→%u",
                 unmappedReq, unmappedProp, src.projectiles.size(),
                 t7IdForMap, t8CharId);
        *optionalWarn = buf;
    }
    (void)errorMsg;
    return true;
}
