#pragma once
#include <cstdint>
#include <string>
#include <vector>

// -------------------------------------------------------------
//  Sub-struct: Requirement  (stride=0x14 in file)
// -------------------------------------------------------------
struct ParsedRequirement {
    uint32_t req;    // +0x00  requirement ID  (1100 = list terminator)
    uint32_t param;  // +0x04  parameter value
    uint32_t param2; // +0x08
    uint32_t param3; // +0x0C
    uint32_t param4; // +0x10
};

// -------------------------------------------------------------
//  Sub-struct: PushbackExtra  (stride=0x02 in file)
// -------------------------------------------------------------
struct ParsedPushbackExtra {
    uint16_t value; // +0x00
};

// -------------------------------------------------------------
//  Sub-struct: Pushback  (stride=0x10 in file)
// -------------------------------------------------------------
struct ParsedPushback {
    uint16_t val1;              // +0x00  duration
    uint16_t val2;              // +0x02  displacement
    uint32_t val3;              // +0x04  nonlinear_frames
    uint64_t extra_addr;        // +0x08  -> PushbackExtra block entry
    uint32_t pushback_extra_idx = 0xFFFFFFFF;  // index into global pushbackExtraBlock
};

// -------------------------------------------------------------
//  Sub-struct: ReactionList  (stride=0x70 in file)
// -------------------------------------------------------------
struct ParsedReactionList {
    // 7 pushback indexes (at +0x00..+0x30, 7 ? uint64 after expand)
    uint64_t pushback_addr[7];           // file offsets after expand
    uint32_t pushback_idx[7];            // indexes into pushbackBlock (0xFFFFFFFF = null)

    // Directions (+0x38..+0x42, all uint16)
    uint16_t front_direction;
    uint16_t back_direction;
    uint16_t left_side_direction;
    uint16_t right_side_direction;
    uint16_t front_ch_direction;
    uint16_t downed_direction;

    // Rotations (+0x44..+0x4A, all uint16)
    uint16_t front_rotation;
    uint16_t back_rotation;
    uint16_t left_side_rotation;
    uint16_t right_side_rotation;

    // +0x4C uint32 (vertical_pushback -- 4 bytes, but OldTool2 reads as 2+2)
    uint16_t vertical_pushback;  // +0x4C  (only lower 2 bytes used)
    uint16_t downed_rotation;    // +0x4E

    // Move indexes (+0x50..+0x6A, all uint16)
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
};

// -------------------------------------------------------------
//  Sub-struct: Cancel  (TK8 file format, 0x28 bytes)
//  Fields from OldTool2 motbinExport.py t8_offsetTable
// -------------------------------------------------------------
struct ParsedCancel {
    uint64_t command;             // 0x00  direction/input bitmask
    uint64_t requirement_addr;    // 0x08  -> Requirement list
    uint64_t extradata_addr;      // 0x10  -> CancelExtradata block entry
    uint32_t frame_window_start;  // 0x18  activation window start frame
    uint32_t frame_window_end;    // 0x1C  activation window end frame
    uint32_t starting_frame;      // 0x20  target frame in destination move
    uint16_t move_id;             // 0x24  destination move index (0x8000 = end)
    uint16_t cancel_option;       // 0x26  flags

    // -- Resolved values (populated during parse) -------------
    uint32_t req_list_idx    = 0xFFFFFFFF;  // index into global requirementBlock where req list starts
    uint32_t extradata_idx   = 0xFFFFFFFF;  // index into cancel_extra block (0xFFFF = null)
    uint32_t extradata_value = 0xFFFFFFFF;  // value at that index
};

// -------------------------------------------------------------
//  Sub-struct: HitCondition  (TK8 file format, 0x18 bytes)
// -------------------------------------------------------------
struct ParsedHitCondition {
    uint64_t requirement_addr;   // 0x00  -> Requirement list (null = list end)
    uint32_t damage;             // 0x08  damage value
    uint32_t _0x0C;              // 0x0C  unknown
    uint64_t reaction_list_addr; // 0x10  -> ReactionList

    // Resolved indexes into global blocks
    uint32_t req_list_idx      = 0xFFFFFFFF;  // index into global requirementBlock where req list starts
    uint32_t reaction_list_idx = 0xFFFFFFFF;  // index into global reaction_list block
};

// -------------------------------------------------------------
//  Sub-struct: ExtraMoveProperty  (TK8 file format, 0x28 bytes)
//  Used for extra_properties, start_properties, end_properties
// -------------------------------------------------------------
struct ParsedExtraProp {
    uint32_t type;               // 0x00  property type
    uint32_t _0x4;               // 0x04  unknown
    uint64_t requirement_addr;   // 0x08  -> Requirement list
    uint32_t id;                 // 0x10  property ID (0 = list end)
    uint32_t value;              // 0x14
    uint32_t value2;             // 0x18
    uint32_t value3;             // 0x1C
    uint32_t value4;             // 0x20
    uint32_t value5;             // 0x24

    uint32_t req_list_idx = 0xFFFFFFFF;  // index into global requirementBlock
};

// -------------------------------------------------------------
//  Sub-struct: Voiceclip  (stride=0x0C in file)
// -------------------------------------------------------------
struct ParsedVoiceclip {
    uint32_t val1; // +0x00
    uint32_t val2; // +0x04
    uint32_t val3; // +0x08
};

// -------------------------------------------------------------
//  ParsedMove
//  Field offsets based on OldTool2 (TekkenMovesetExtractor TK8)
//  t8_offsetTable -- these are .motbin FILE format offsets.
//  Move struct size = 0x448 bytes.
//
//  NOTE: vuln/hitlevel/name_key/anim_key/ordinal_id are stored
//  encrypted in the file. We store the raw encrypted 64-bit
//  value; decryption is applied separately if needed.
// -------------------------------------------------------------
struct ParsedMove {
    // -- 0x00-0x4F  Encrypted name/anim key blocks ---------
    uint64_t encrypted_name_key;      // 0x00  -> move name index (encrypted)
    uint64_t name_encryption_key;     // 0x08
    uint32_t name_related[4];         // 0x10
    uint64_t encrypted_anim_key;      // 0x20  -> anim name index (encrypted)
    uint64_t anim_encryption_key;     // 0x28
    uint32_t anim_related[8];         // 0x30  (0x30-0x4F, 8 entries)
    uint32_t anmbin_body_idx;         // 0x50  index into anmbin bodyKeysCount array (set to move_idx by extractor)
    uint32_t anmbin_body_sub_idx;     // 0x54  sub-index (0 in file; skeleton_id populated at runtime)

    // -- 0x58-0x97  Encrypted vuln / hitlevel blocks --------
    uint64_t encrypted_vuln;          // 0x58  -> vuln value (encrypted)
    uint64_t vuln_encryption_key;     // 0x60
    uint32_t vuln_related[4];         // 0x68
    uint64_t encrypted_hitlevel;      // 0x78  -> hit level (encrypted)
    uint64_t hitlevel_encryption_key; // 0x80
    uint32_t hitlevel_related[4];     // 0x88

    // -- 0x98-0xCB  Cancel addresses & unknowns ------------
    uint64_t cancel_addr;   // 0x98  primary cancel list
    uint64_t cancel2_addr;  // 0xA0  secondary cancel list
    uint64_t u1;            // 0xA8
    uint64_t u2;            // 0xB0
    uint64_t u3;            // 0xB8
    uint64_t u4;            // 0xC0
    uint32_t u6;            // 0xC8
    uint16_t transition;    // 0xCC  next move ID
    int16_t  _0xCE;         // 0xCE

    // -- 0xD0-0x10F  Encrypted char_id / ordinal_id --------
    uint64_t encrypted_ordinal_id2;    // 0xD0  -> ordinal_id2 / t_char_id (encrypted)
    uint64_t ordinal_id2_enc_key;      // 0xD8
    uint32_t ordinal_id2_related[4];   // 0xE0
    uint64_t encrypted_ordinal_id;    // 0xF0  -> move ordinal ID (encrypted)
    uint64_t ordinal_encryption_key;  // 0xF8
    uint32_t ordinal_related[4];      // 0x100

    // -- 0x110-0x157  Pointers & timing --------------------
    uint64_t hit_condition_addr;              // 0x110
    uint32_t _0x118;                          // 0x118
    uint32_t _0x11C;                          // 0x11C
    int32_t  anim_len;                        // 0x120  total animation frames
    uint32_t airborne_start;                  // 0x124
    uint32_t airborne_end;                    // 0x128
    uint32_t ground_fall;                     // 0x12C
    uint64_t voicelip_addr;                   // 0x130
    uint64_t extra_move_property_addr;        // 0x138
    uint64_t move_start_extraprop_addr;       // 0x140
    uint64_t move_end_extraprop_addr;         // 0x148
    uint32_t u15;                             // 0x150
    uint32_t _0x154;                          // 0x154
    uint32_t startup;                         // 0x158  startup frames
    uint32_t recovery;                        // 0x15C  recovery frames

    // -- 0x160-0x2DF  Hitbox slots (8 ? 0x30 bytes) --------
    uint32_t hitbox_active_start[8];
    uint32_t hitbox_active_last[8];
    uint32_t hitbox_location[8];
    float    hitbox_floats[8][9];

    // -- 0x2E0-0x447  Collision / distance / unknowns ------
    uint16_t collision;   // 0x2E0
    uint16_t distance;    // 0x2E2
    uint32_t unk5[88];    // 0x2E4
    uint32_t u18;         // 0x444

    // -- Decrypted / display values (not stored in file) ---
    uint32_t vuln              = 0;
    uint32_t hitlevel          = 0;
    uint32_t moveId            = 0;
    uint32_t name_key          = 0;   // decrypted name index
    uint32_t anim_key          = 0;   // decrypted anim index
    uint32_t ordinal_id2       = 0;   // decrypted ordinal_id2 (0xD0)
    std::string displayName;

    // -- Block indexes (into MotbinData global blocks) ----------
    uint32_t cancel_idx        = 0xFFFFFFFF;  // first Cancel entry for this move
    uint32_t hit_condition_idx = 0xFFFFFFFF;  // first HitCondition entry
    uint32_t voiceclip_idx     = 0xFFFFFFFF;  // Voiceclip entry
    uint32_t extra_prop_idx    = 0xFFFFFFFF;  // first ExtraProp entry
    uint32_t start_prop_idx    = 0xFFFFFFFF;  // first StartProp entry
    uint32_t end_prop_idx      = 0xFFFFFFFF;  // first EndProp entry
};

struct MotbinData {
    bool        loaded   = false;
    std::string errorMsg;

    uint32_t moveCount = 0;
    std::vector<ParsedMove>   moves;
    std::vector<uint16_t>     originalAliases;  // header[0x30..0xA7], 60 entries: genericId-0x8000 -> real move index
    std::vector<uint32_t>     moveToGenericId;  // per-move: 0 = not a generic target, else = the generic ID (0x8000+n)

    // Global flat arrays parsed from each file block
    std::vector<ParsedRequirement>   requirementBlock;
    std::vector<ParsedCancel>        cancelBlock;
    std::vector<ParsedCancel>        groupCancelBlock;
    std::vector<uint32_t>            cancelExtraBlock;
    std::vector<ParsedHitCondition>  hitConditionBlock;
    std::vector<ParsedReactionList>  reactionListBlock;
    std::vector<ParsedPushback>      pushbackBlock;
    std::vector<ParsedPushbackExtra> pushbackExtraBlock;
    std::vector<ParsedExtraProp>     extraPropBlock;
    std::vector<ParsedExtraProp>     startPropBlock;
    std::vector<ParsedExtraProp>     endPropBlock;
    std::vector<ParsedVoiceclip>     voiceclipBlock;
};

// Loads moveset.motbin from the given moveset folder path.
MotbinData LoadMotbin(const std::string& folderPath);
