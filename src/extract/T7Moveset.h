#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "T7Constants.h"

// -------------------------------------------------------------
//  T7 runtime POD mirrors (gameAddr form) + extracted model.
// -------------------------------------------------------------

namespace T7 {

#pragma pack(push, 1)

struct Requirement {
    uint32_t condition;
    uint32_t param;
};

struct CancelExtra {
    uint32_t value;
};

struct Cancel {
    uint64_t command;
    uint64_t requirements_addr;
    uint64_t extradata_addr;
    uint32_t detection_start;
    uint32_t detection_end;
    uint32_t starting_frame;
    uint16_t move_id;
    uint16_t cancel_option;
};

struct PushbackExtra {
    uint16_t horizontal_offset;
};

struct Pushback {
    uint16_t duration;
    uint16_t displacement;
    uint32_t num_of_loops;
    uint64_t extradata_addr;
};

struct Reactions {
    uint64_t pushbacks[7];
    uint16_t front_direction;
    uint16_t back_direction;
    uint16_t left_side_direction;
    uint16_t right_side_direction;
    uint16_t front_counterhit_direction;
    uint16_t downed_direction;
    uint32_t _0x44;
    uint32_t _0x48;
    uint16_t vertical_pushback;
    uint16_t downed_rotation;   // misnamed standing_moveid in t7_struct.h
    uint16_t standing;
    uint16_t crouch;
    uint16_t ch;
    uint16_t crouch_ch;
    uint16_t left_side;
    uint16_t left_side_crouch;
    uint16_t right_side;
    uint16_t right_side_crouch;
    uint16_t back;
    uint16_t back_crouch;
    uint16_t block;
    uint16_t crouch_block;
    uint16_t wallslump;
    uint16_t downed;
    uint32_t _pad_0x6C;  // C padding to 0x70
};

struct HitCondition {
    uint64_t requirements_addr;
    uint32_t damage;
    uint32_t _0xC; // Padding can be removed.
    uint64_t reactions_addr;
};

struct Voiceclip {
    uint32_t id;
};

struct ExtraMoveProperty {
    uint32_t starting_frame;
    uint32_t id;
    uint32_t value;
};

struct OtherMoveProperty {
    uint64_t requirements_addr;
    uint32_t extraprop;
    uint32_t value;
};

struct Input {
    uint64_t command;
};

struct InputSequence {
    uint16_t input_window_frames;
    uint16_t input_amount;
    int32_t  _0x4; // Padding
    uint64_t input_addr;
};

struct Projectile {
    uint8_t raw[0xA8];
};

struct CameraData {
    uint32_t _0x0;
    uint16_t _0x4;
    uint16_t left_side_camera_data;
    uint16_t right_side_camera_data;
    uint16_t _0xA;
};

struct ThrowCamera {
    uint64_t side;
    uint64_t cameradata_addr;
};

struct UnknownParryRelated {
    uint32_t value;
};

struct Move {
    uint64_t name_addr;
    uint64_t anim_name_addr;
    uint64_t anim_addr;
    uint32_t vuln;
    uint32_t hitlevel;
    uint64_t cancel_addr;
    uint64_t _0x28_cancel_addr;
    int32_t  _0x30;
    int32_t  _0x34; // padding
    uint64_t _0x38_cancel_addr;
    int32_t  _0x40;
    int32_t  _0x44; // padding
    uint64_t _0x48_cancel_addr;
    uint32_t _0x50;
    uint16_t transition;
    int16_t  _0x56; // I think move_end_rotation
    uint16_t moveId_val1;
    uint16_t moveId_val2;
    int16_t  _0x5C;
    int16_t  _0x5E;
    uint64_t hit_condition_addr;
    uint32_t anim_len;
    uint32_t airborne_start;
    uint32_t airborne_end;
    uint32_t ground_fall;
    uint64_t voicelip_addr;
    uint64_t extra_move_property_addr;
    uint64_t move_start_extraprop_addr;
    uint64_t move_end_extraprop_addr;
    int32_t  _0x98; // u15
    uint32_t hitbox_location;
    uint32_t first_active_frame;
    uint32_t last_active_frame;
    int16_t  _0xA8;       // collision
    uint16_t distance;
    int32_t  _0xAC;
};

#pragma pack(pop)

static_assert(sizeof(Requirement) == 0x08, "T7 Requirement");
static_assert(sizeof(Cancel) == 0x28, "T7 Cancel");
static_assert(sizeof(Reactions) == 0x70, "T7 Reactions");
static_assert(sizeof(HitCondition) == 0x18, "T7 HitCondition");
static_assert(sizeof(ExtraMoveProperty) == 0x0C, "T7 ExtraMoveProperty");
static_assert(sizeof(OtherMoveProperty) == 0x10, "T7 OtherMoveProperty");
static_assert(sizeof(Move) == 0xB0, "T7 Move");
static_assert(sizeof(Voiceclip) == 0x04, "T7 Voiceclip");
static_assert(sizeof(InputSequence) == 0x10, "T7 InputSequence");
static_assert(sizeof(CameraData) == 0x0C, "T7 CameraData");
static_assert(sizeof(ThrowCamera) == 0x10, "T7 ThrowCamera");
static_assert(sizeof(Projectile) == 0xA8, "T7 Projectile");

struct Moveset {
    bool        valid = false;
    uintptr_t   baseAddr = 0;
    uint32_t    encodedCharId = 0;   // raw at +0x148
    uint32_t    fighterIdFromPlayer = 0;

    uint16_t    origAliases[kAliasCountT7] = {};
    uint16_t    currentAliases[kAliasCountT7] = {};
    uint16_t    unknownAliases[kUnknownAliasCount] = {};

    std::string characterName;      // from +0x2E8, brackets stripped
    std::string characterCreator;
    std::string date;
    std::string fullDate;

    std::vector<Reactions>          reactions;
    std::vector<Requirement>        requirements;
    std::vector<HitCondition>       hitConditions;
    std::vector<Projectile>         projectiles;
    std::vector<Pushback>           pushbacks;
    std::vector<PushbackExtra>      pushbackExtras;
    std::vector<Cancel>             cancels;
    std::vector<Cancel>             groupCancels;
    std::vector<CancelExtra>        cancelExtras;
    std::vector<ExtraMoveProperty>  extraMoveProperties;
    std::vector<OtherMoveProperty>  moveBeginningProps;
    std::vector<OtherMoveProperty>  moveEndingProps;
    std::vector<Move>               moves;
    std::vector<Voiceclip>          voiceclips;
    std::vector<InputSequence>      inputSequences;
    std::vector<Input>              inputs;
    std::vector<UnknownParryRelated> parryRelated;
    std::vector<CameraData>         cameraData;
    std::vector<ThrowCamera>        throwCameras;

    // Resolved pointer → index (0xFFFFFFFF = null)
    std::vector<uint32_t> cancelReqIdx;
    std::vector<uint32_t> cancelExtraIdx;
    std::vector<uint32_t> groupCancelReqIdx;
    std::vector<uint32_t> groupCancelExtraIdx;
    std::vector<uint32_t> hitCondReqIdx;
    std::vector<uint32_t> hitCondReactIdx;
    std::vector<uint32_t> pushbackExtraIdx;
    std::vector<uint32_t> reactPushbackIdx;   // reactions.size() * 7
    std::vector<uint32_t> inputSeqInputIdx;
    std::vector<uint32_t> throwCamDataIdx;
    std::vector<uint32_t> beginPropReqIdx;
    std::vector<uint32_t> endPropReqIdx;

    std::vector<std::string> moveNames;
    std::vector<std::string> moveAnimNames;
    std::vector<uint32_t> moveCancelIdx;
    std::vector<uint32_t> moveHitCondIdx;
    std::vector<uint32_t> moveVoiceIdx;
    std::vector<uint32_t> moveExtraPropIdx;
    std::vector<uint32_t> moveStartPropIdx;
    std::vector<uint32_t> moveEndPropIdx;
};

} // namespace T7
