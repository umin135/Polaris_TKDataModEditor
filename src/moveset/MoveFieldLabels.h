#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Move field display labels.
//  Edit this file to rename how fields appear in the editor.
//
//  Offset  | Field key              | OldTool2 fmt   | Notes
//  --------|------------------------|----------------|---------------------------
//  0x00    | name_key               | hex            | decrypted name index
//  0x20    | anim_key               | hex            | decrypted anim index
//  0x54    | skeleton_id            | hex            | anmbin sub-index (runtime)
//  0x58    | vuln                   | int            | decrypted vulnerability
//  0x78    | hitlevel               | int            | decrypted hit level
//  0x98    | cancel_idx             | positive_index | index into cancelBlock
//  0xCC    | transition             | short          | next move ID
//  0x120   | anim_len               | int            | total animation frames
//  0x110   | hit_condition_idx      | positive_index | index into hitCondBlock
//  0x130   | voiceclip_idx          | index          | index into voiceclipBlock
//  0x138   | extra_properties_idx   | index          | index into extraPropBlock
//  0x140   | start_prop_idx         | index          | index into startPropBlock
//  0x148   | end_prop_idx           | index          | index into endPropBlock
//  0x168   | hitbox_location        | hex            | alias = hitbox1 location
//  0x158   | first_active_frame     | int            | move startup frame
//  0x15C   | last_active_frame      | int            | move last active frame
//  0x160+  | hitbox1..4             | hex/int        | per-hitbox slots (1-indexed)
//  0xCE    | _0xCE                  | short          | unknown int16
//  0xD0    | t_char_id              | hex            | decrypted ordinal_id2
//  0xF0    | ordinal_id             | hex            | decrypted move ordinal ID
//  0x118   | _0x118                 | int            | unknown
//  0x11C   | _0x11C                 | int            | unknown
//  0x124   | airborne_start         | int            | airborne start frame
//  0x128   | airborne_end           | int            | airborne end frame
//  0x12C   | ground_fall            | int            | ground fall frame
//  0x154   | _0x154                 | int            | unknown
//  0xC8    | u6                     | int            | unknown
//  0x150   | u15                    | hex            | unknown
//  0x2E0   | collision?             | short (u16)    | collision-related
//  0x2E2   | distance               | short (u17)    | distance-related
//  0x444   | u18                    | int            | unknown
// ─────────────────────────────────────────────────────────────────────────────
namespace MoveLabel {
    static constexpr const char* Name         = "name";
    static constexpr const char* NameKey      = "name_key";
    static constexpr const char* AnimKey      = "anim_key";
    static constexpr const char* SkeletonId   = "skeleton_id";
    static constexpr const char* Vuln         = "vuln";
    static constexpr const char* Hitlevel     = "hitlevel";
    static constexpr const char* CancelIdx    = "cancel_idx";
    static constexpr const char* Transition   = "transition";
    static constexpr const char* AnimLen      = "anim_len";
    static constexpr const char* HitCondIdx   = "hit_condition_idx";
    static constexpr const char* VoiceclipIdx = "voiceclip_idx";
    static constexpr const char* ExtraPropIdx = "extra_properties_idx";
    static constexpr const char* StartPropIdx = "start_prop_idx";
    static constexpr const char* EndPropIdx   = "end_prop_idx";
    static constexpr const char* HitboxLoc    = "hitbox_location";
    static constexpr const char* FirstActive  = "first_active_frame";
    static constexpr const char* LastActive   = "last_active_frame";
    // per-hitbox: labels built dynamically as "hitbox%d", "hitbox%d first", "hitbox%d last"
    static constexpr const char* CE           = "_0xCE";
    static constexpr const char* TCharId      = "t_char_id";
    static constexpr const char* OrdinalId    = "ordinal_id";
    static constexpr const char* F0x118       = "_0x118";
    static constexpr const char* F0x11C       = "_0x11C";
    static constexpr const char* AirborneStart= "airborne_start";
    static constexpr const char* AirborneEnd  = "airborne_end";
    static constexpr const char* GroundFall   = "ground_fall";
    static constexpr const char* F0x154       = "_0x154";
    static constexpr const char* U6           = "u6";
    static constexpr const char* U15          = "u15";
    static constexpr const char* Collision    = "collision?";
    static constexpr const char* Distance     = "distance";
    static constexpr const char* U18          = "u18";
} // namespace MoveLabel
