#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  FieldTooltips.h
//  Tooltip text for every editable field label in the moveset editor.
//
//  Usage:
//    • Set text = "" to show no tooltip on hover.
//    • Set text = "..." to show a popup when the label is hovered.
//    • Tooltip text is shown as-is; use '\n' for line breaks.
//
//  Namespaces mirror EditorFieldLabel.h (one entry per label constant).
//  Do NOT modify FieldTooltip or ShowFieldTooltip.
// ─────────────────────────────────────────────────────────────────────────────
#include "imgui/imgui.h"

// ─── Core type ───────────────────────────────────────────────────────────────

struct FieldTooltip {
    const char* text = ""; // "" = no tooltip
};

// Call immediately after the label ImGui item is rendered (TextDisabled etc.).
// No-op when text is empty.
// Tooltip is anchored above the center of the hovered item (not at the mouse cursor).
inline void ShowFieldTooltip(const FieldTooltip& tt)
{
    if (!tt.text || tt.text[0] == '\0') return;
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal | ImGuiHoveredFlags_NoSharedDelay))
    {
        const ImVec2 itemMin = ImGui::GetItemRectMin();
        const ImVec2 itemMax = ImGui::GetItemRectMax();
        // Position: center-bottom of tooltip aligns with top-center of the item
        const ImVec2 pos = ImVec2(
            (itemMin.x + itemMax.x) * 0.5f,
            itemMin.y - ImGui::GetStyle().ItemSpacing.y
        );
        ImGui::SetNextWindowPos(pos, ImGuiCond_Always, ImVec2(0.5f, 1.0f));
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 28.0f);
        ImGui::TextUnformatted(tt.text);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Tooltip declarations
//  Organised to mirror EditorFieldLabel.h namespaces.
//  Leave text = "" to suppress the tooltip for that field.
// ─────────────────────────────────────────────────────────────────────────────
namespace FieldTT
{
    // ── Move / General ───────────────────────────────────────────────────────
    namespace Move {
        static constexpr FieldTooltip Name          = { "This is Move Name!" };
        static constexpr FieldTooltip NameKey       = { "" };
        static constexpr FieldTooltip AnimKey       = { "" };
        static constexpr FieldTooltip SkeletonId    = { "" };
        static constexpr FieldTooltip Vuln          = { "" };
        static constexpr FieldTooltip Hitlevel      = { "" };
        static constexpr FieldTooltip CancelIdx     = { "" };
        static constexpr FieldTooltip Transition    = { "" };
        static constexpr FieldTooltip AnimLen       = { "" };
        static constexpr FieldTooltip HitCondIdx    = { "" };
        static constexpr FieldTooltip VoiceclipIdx  = { "" };
        static constexpr FieldTooltip ExtraPropIdx  = { "" };
        static constexpr FieldTooltip StartPropIdx  = { "" };
        static constexpr FieldTooltip EndPropIdx    = { "" };
        static constexpr FieldTooltip CE            = { "" };
        static constexpr FieldTooltip TCharId       = { "" };
        static constexpr FieldTooltip OrdinalId     = { "" };
        static constexpr FieldTooltip F0x118        = { "" };
        static constexpr FieldTooltip F0x11C        = { "" };
        static constexpr FieldTooltip AirborneStart = { "" };
        static constexpr FieldTooltip AirborneEnd   = { "" };
        static constexpr FieldTooltip GroundFall    = { "" };
        static constexpr FieldTooltip F0x154        = { "" };
        static constexpr FieldTooltip U6            = { "" };
        static constexpr FieldTooltip U15           = { "" };
        static constexpr FieldTooltip Collision     = { "" };
        static constexpr FieldTooltip Distance      = { "" };
        static constexpr FieldTooltip U18           = { "" };
    } // namespace Move

    // ── Hitbox ───────────────────────────────────────────────────────────────
    namespace Hitbox {
        static constexpr FieldTooltip ActiveStart        = { "" };
        static constexpr FieldTooltip ActiveLast         = { "" };
        static constexpr FieldTooltip Location           = { "" };
        // Float[0..8]
        static constexpr FieldTooltip Float[9] = { {""},{""},{""},{""},{""},{""},{""},{""},{""} };
    } // namespace Hitbox

    // ── Requirement ──────────────────────────────────────────────────────────
    namespace Req {
        static constexpr FieldTooltip Req    = { "" };
        static constexpr FieldTooltip Param0 = { "" };
        static constexpr FieldTooltip Param1 = { "" };
        static constexpr FieldTooltip Param2 = { "" };
        static constexpr FieldTooltip Param3 = { "" };
    } // namespace Req

    // ── Cancel ───────────────────────────────────────────────────────────────
    namespace Cancel {
        static constexpr FieldTooltip Command          = { "" };
        static constexpr FieldTooltip Extradata        = { "" };
        static constexpr FieldTooltip Requirements     = { "" };
        static constexpr FieldTooltip InputWindowStart = { "" };
        static constexpr FieldTooltip InputWindowEnd   = { "" };
        static constexpr FieldTooltip StartingFrame    = { "" };
        static constexpr FieldTooltip Move             = { "" };
        static constexpr FieldTooltip GroupCancelIdx   = { "" };
        static constexpr FieldTooltip Option           = { "" };
    } // namespace Cancel

    // ── HitCondition ─────────────────────────────────────────────────────────
    namespace HitCond {
        static constexpr FieldTooltip Requirements = { "" };
        static constexpr FieldTooltip Damage       = { "" };
        static constexpr FieldTooltip F0x0C        = { "" };
        static constexpr FieldTooltip ReactionList = { "" };
    } // namespace HitCond

    // ── ReactionList ─────────────────────────────────────────────────────────
    namespace Reaction {
        static constexpr FieldTooltip FrontPushback           = { "" };
        static constexpr FieldTooltip BackturnedPushback      = { "" };
        static constexpr FieldTooltip LeftSidePushback        = { "" };
        static constexpr FieldTooltip RightSidePushback       = { "" };
        static constexpr FieldTooltip FrontCounterhitPushback = { "" };
        static constexpr FieldTooltip DownedPushback          = { "" };
        static constexpr FieldTooltip BlockPushback           = { "" };
        static constexpr FieldTooltip Standing                = { "" };
        static constexpr FieldTooltip Ch                      = { "" };
        static constexpr FieldTooltip Crouch                  = { "" };
        static constexpr FieldTooltip CrouchCh                = { "" };
        static constexpr FieldTooltip LeftSide                = { "" };
        static constexpr FieldTooltip LeftSideCrouch          = { "" };
        static constexpr FieldTooltip RightSide               = { "" };
        static constexpr FieldTooltip RightSideCrouch         = { "" };
        static constexpr FieldTooltip Back                    = { "" };
        static constexpr FieldTooltip BackCrouch              = { "" };
        static constexpr FieldTooltip Block                   = { "" };
        static constexpr FieldTooltip CrouchBlock             = { "" };
        static constexpr FieldTooltip Wallslump               = { "" };
        static constexpr FieldTooltip Downed                  = { "" };
        static constexpr FieldTooltip FrontDirection           = { "" };
        static constexpr FieldTooltip BackDirection            = { "" };
        static constexpr FieldTooltip LeftSideDirection        = { "" };
        static constexpr FieldTooltip RightSideDirection       = { "" };
        static constexpr FieldTooltip FrontCounterhitDirection = { "" };
        static constexpr FieldTooltip DownedDirection          = { "" };
        static constexpr FieldTooltip FrontRotation            = { "" };
        static constexpr FieldTooltip BackRotation             = { "" };
        static constexpr FieldTooltip LeftSideRotation         = { "" };
        static constexpr FieldTooltip RightSideRotation        = { "" };
        static constexpr FieldTooltip VerticalPushback         = { "" };
        static constexpr FieldTooltip DownedRotation           = { "" };
    } // namespace Reaction

    // ── Pushback ─────────────────────────────────────────────────────────────
    namespace Pushback {
        static constexpr FieldTooltip LinearDisplacement = { "" };
        static constexpr FieldTooltip LinearDistance     = { "" };
        static constexpr FieldTooltip NumOfExtraPushbacks   = { "" };
        static constexpr FieldTooltip PushbackExtradata     = { "" };
    } // namespace Pushback

    namespace PushbackExtra {
        static constexpr FieldTooltip Displacement = { "" };
    } // namespace PushbackExtra

    // ── Voiceclip ────────────────────────────────────────────────────────────
    namespace Voiceclip {
        static constexpr FieldTooltip Folder = { "" };
        static constexpr FieldTooltip Val2   = { "" };
        static constexpr FieldTooltip Clip   = { "" };
    } // namespace Voiceclip

    // ── Input / InputSequence ─────────────────────────────────────────────────
    namespace Input {
        static constexpr FieldTooltip Command = { "" };
    } // namespace Input

    namespace InputSeq {
        static constexpr FieldTooltip InputWindowFrames = { "" };
        static constexpr FieldTooltip InputAmount       = { "" };
        static constexpr FieldTooltip F0x4              = { "" };
    } // namespace InputSeq

    // ── Projectile ───────────────────────────────────────────────────────────
    namespace Projectile {
        static constexpr FieldTooltip HitCondition = { "" };
        static constexpr FieldTooltip Cancel       = { "" };
        // U1[N] / U2[N] use dynamic snprintf labels; no per-index entries here.
    } // namespace Projectile

    // ── Throw ─────────────────────────────────────────────────────────────────
    namespace Throw {
        static constexpr FieldTooltip Side       = { "" };
        static constexpr FieldTooltip ThrowExtra = { "" };
    } // namespace Throw

    namespace ThrowExtra {
        static constexpr FieldTooltip PickProbability     = { "" };
        static constexpr FieldTooltip CameraType          = { "" };
        static constexpr FieldTooltip LeftSideCameraData  = { "" };
        static constexpr FieldTooltip RightSideCameraData = { "" };
        static constexpr FieldTooltip AdditionalRotation  = { "" };
    } // namespace ThrowExtra

    // ── ParryableMove ────────────────────────────────────────────────────────
    namespace Parry {
        static constexpr FieldTooltip Value = { "" };
    } // namespace Parry

    // ── Dialogue ─────────────────────────────────────────────────────────────
    namespace Dialogue {
        static constexpr FieldTooltip Type          = { "" };
        static constexpr FieldTooltip Id            = { "" };
        static constexpr FieldTooltip F0x4          = { "" };
        static constexpr FieldTooltip Requirements  = { "" };
        static constexpr FieldTooltip VoiceclipKey  = { "" };
        static constexpr FieldTooltip FacialAnimIdx = { "" };
    } // namespace Dialogue

    // ── ExtraProp / StartProp / EndProp ──────────────────────────────────────
    namespace ExtraProp {
        static constexpr FieldTooltip Frame        = { "" };
        static constexpr FieldTooltip F0x4         = { "" };
        static constexpr FieldTooltip Property     = { "" };
        static constexpr FieldTooltip Requirements = { "" };
        static constexpr FieldTooltip Param0       = { "" };
        static constexpr FieldTooltip Param1       = { "" };
        static constexpr FieldTooltip Param2       = { "" };
        static constexpr FieldTooltip Param3       = { "" };
        static constexpr FieldTooltip Param4       = { "" };
    } // namespace ExtraProp

} // namespace FieldTT
