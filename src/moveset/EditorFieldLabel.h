#pragma once
// -----------------------------------------------------------------------------
//  EditorFieldLabel.h
//  All field display label strings used across the moveset editor.
//  Edit this file to rename how fields appear in the editor UI.
//  Each namespace corresponds to a struct/block type in the motbin format.
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------
//  Move fields  (tk_move)
// -----------------------------------------------------------------------
namespace MoveLabel {
    // Overview
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

    // Unknown / raw section  (editable)
    static constexpr const char* U1 = "u1              [0xA8]";
    static constexpr const char* U2 = "u2              [0xB0]";
    static constexpr const char* U3 = "u3              [0xB8]";
    static constexpr const char* U4 = "u4              [0xC0]";

    // Unknown / raw section  (display-only encrypted blobs)
    static constexpr const char* EncNameKey     = "enc_name_key    [0x00]";
    static constexpr const char* NameEncKey     = "name_enc_key    [0x08]";
    static constexpr const char* EncAnimKey     = "enc_anim_key    [0x20]";
    static constexpr const char* AnimEncKey     = "anim_enc_key    [0x28]";
    static constexpr const char* AnmbinBodyIdx  = "anmbin_body_idx [0x50]";
    static constexpr const char* EncVuln        = "enc_vuln        [0x58]";
    static constexpr const char* VulnEncKey     = "vuln_enc_key    [0x60]";
    static constexpr const char* EncHitlevel    = "enc_hitlevel    [0x78]";
    static constexpr const char* HitlevelEncKey = "hitlevel_enc_key[0x80]";
    static constexpr const char* EncOrdinalId2  = "enc_ordinal_id2 [0xD0]";
    static constexpr const char* OrdinalId2Key  = "ordinal_id2_key [0xD8]";
    static constexpr const char* EncOrdinalId   = "enc_ordinal_id  [0xF0]";
    static constexpr const char* OrdinalEncKey  = "ordinal_enc_key [0xF8]";
} // namespace MoveLabel

// -----------------------------------------------------------------------
//  Hitbox fields  (tk_move_hitbox)
// -----------------------------------------------------------------------
namespace HitboxLabel {
    static constexpr const char* ActiveStart = "active_start";
    static constexpr const char* ActiveLast  = "active_last";
    static constexpr const char* Location    = "location";
    static constexpr const char* FloatFmt    = "float[%d]";  // format string — used with snprintf
} // namespace HitboxLabel

// -----------------------------------------------------------------------
//  Requirement fields  (tk_requirement)
// -----------------------------------------------------------------------
namespace ReqLabel {
    static constexpr const char* Req    = "req";
    static constexpr const char* Param0 = "params[0]";
    static constexpr const char* Param1 = "params[1]";
    static constexpr const char* Param2 = "params[2]";
    static constexpr const char* Param3 = "params[3]";
} // namespace ReqLabel

// -----------------------------------------------------------------------
//  Cancel fields  (tk_cancel)
// -----------------------------------------------------------------------
namespace CancelLabel {
    static constexpr const char* Command          = "command";
    static constexpr const char* Extradata        = "extradata";
    static constexpr const char* Requirements     = "requirements";
    static constexpr const char* InputWindowStart = "input_window_start";
    static constexpr const char* InputWindowEnd   = "input_window_end";
    static constexpr const char* StartingFrame    = "starting_frame";
    static constexpr const char* Move             = "move";
    static constexpr const char* GroupCancelIdx   = "group_cancel_list_idx";
    static constexpr const char* Option           = "option";
} // namespace CancelLabel

// -----------------------------------------------------------------------
//  HitCondition fields  (tk_hit_condition)
// -----------------------------------------------------------------------
namespace HitCondLabel {
    static constexpr const char* Requirements = "requirements";
    static constexpr const char* Damage       = "damage";
    static constexpr const char* F0x0C        = "_0x0C";
    static constexpr const char* ReactionList = "reaction_list";
} // namespace HitCondLabel

// -----------------------------------------------------------------------
//  ReactionList fields  (tk_reaction)
// -----------------------------------------------------------------------
namespace ReactionLabel {
    // Pushback pointer links
    static constexpr const char* FrontPushback          = "front_pushback";
    static constexpr const char* BackturnedPushback     = "backturned_pushback";
    static constexpr const char* LeftSidePushback       = "left_side_pushback";
    static constexpr const char* RightSidePushback      = "right_side_pushback";
    static constexpr const char* FrontCounterhitPushback= "front_counterhit_pushback";
    static constexpr const char* DownedPushback         = "downed_pushback";
    static constexpr const char* BlockPushback          = "block_pushback";

    // Reaction move IDs
    static constexpr const char* Standing        = "default (standing)";
    static constexpr const char* Ch              = "ch";
    static constexpr const char* Crouch          = "crouch";
    static constexpr const char* CrouchCh        = "crouch_ch";
    static constexpr const char* LeftSide        = "left_side";
    static constexpr const char* LeftSideCrouch  = "left_side_crouch";
    static constexpr const char* RightSide       = "right_side";
    static constexpr const char* RightSideCrouch = "right_side_crouch";
    static constexpr const char* Back            = "back";
    static constexpr const char* BackCrouch      = "back_crouch";
    static constexpr const char* Block           = "block";
    static constexpr const char* CrouchBlock     = "crouch_block";
    static constexpr const char* Wallslump       = "wallslump";
    static constexpr const char* Downed          = "downed";

    // Direction fields
    static constexpr const char* FrontDirection           = "front_direction";
    static constexpr const char* BackDirection            = "back_direction";
    static constexpr const char* LeftSideDirection        = "left_side_direction";
    static constexpr const char* RightSideDirection       = "right_side_direction";
    static constexpr const char* FrontCounterhitDirection = "front_counterhit_direction";
    static constexpr const char* DownedDirection          = "downed_direction";

    // Rotation fields
    static constexpr const char* FrontRotation      = "front_rotation";
    static constexpr const char* BackRotation       = "back_rotation";
    static constexpr const char* LeftSideRotation   = "left_side_rotation";
    static constexpr const char* RightSideRotation  = "right_side_rotation";
    static constexpr const char* VerticalPushback   = "vertical_pushback";
    static constexpr const char* DownedRotation     = "downed_rotation";
} // namespace ReactionLabel

// -----------------------------------------------------------------------
//  Pushback fields  (tk_pushback)
// -----------------------------------------------------------------------
namespace PushbackLabel {
    static constexpr const char* NonLinearDisplacement = "non_linear_displacement";
    static constexpr const char* NonLinearDistance     = "non_linear_distance";
    static constexpr const char* NumOfExtraPushbacks   = "num_of_extra_pushbacks";
    static constexpr const char* PushbackExtradata     = "pushback_extradata";
} // namespace PushbackLabel

// -----------------------------------------------------------------------
//  PushbackExtra fields  (tk_pushback_extradata)
// -----------------------------------------------------------------------
namespace PushbackExtraLabel {
    static constexpr const char* Displacement = "displacement";
} // namespace PushbackExtraLabel

// -----------------------------------------------------------------------
//  Voiceclip fields  (tk_voiceclip)
// -----------------------------------------------------------------------
namespace VoiceclipLabel {
    static constexpr const char* Folder = "folder";
    static constexpr const char* Val2   = "val2";
    static constexpr const char* Clip   = "clip";
} // namespace VoiceclipLabel

// -----------------------------------------------------------------------
//  ExtraProp / StartProp / EndProp fields
//  (tk_extraprops / tk_fl_extraprops — both use ParsedExtraProp)
// -----------------------------------------------------------------------
namespace ExtraPropLabel {
    static constexpr const char* Frame        = "frame";        // tk_extraprops only
    static constexpr const char* F0x4         = "_0x4";         // tk_extraprops only
    static constexpr const char* Property     = "property";
    static constexpr const char* Requirements = "requirements";
    static constexpr const char* Param0       = "params[0]";
    static constexpr const char* Param1       = "params[1]";
    static constexpr const char* Param2       = "params[2]";
    static constexpr const char* Param3       = "params[3]";
    static constexpr const char* Param4       = "params[4]";
} // namespace ExtraPropLabel
