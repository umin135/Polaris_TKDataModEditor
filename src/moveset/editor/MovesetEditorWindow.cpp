// MovesetEditorWindow.cpp
// UI layout mirrors OldTool (TKMovesets2):
//   Left panel  -- searchable move list
//   Right panel -- collapsible property sections per move
#include "moveset/editor/MovesetEditorWindow.h"
#include "GameStatic.h"
#include "moveset/labels/LabelDB.h"
#include "moveset/data/EditorFieldLabel.h"
#include "moveset/live/GameLiveEdit.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include <cstdio>
#include <cstring>
#include <cctype>
#include <algorithm>
#include <windows.h>
#include <fstream>
#include <sstream>

extern ImFont* g_fontBold;

// -------------------------------------------------------------
//  Construction
// -------------------------------------------------------------

MovesetEditorWindow::MovesetEditorWindow(const std::string& folderPath,
                                         const std::string& movesetName,
                                         int uid)
{
    m_data = LoadMotbin(folderPath);
    LoadEditorDatas();

    char buf[256];
    snprintf(buf, sizeof(buf), "%s  -  Moveset Editor##msed%d",
             movesetName.c_str(), uid);
    m_windowTitle = buf;
}

// -------------------------------------------------------------
//  Main render
// -------------------------------------------------------------

bool MovesetEditorWindow::Render()
{
    if (!m_open) return false;

    ImGui::SetNextWindowSize(ImVec2(980.0f, 640.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(ImVec2(640.0f, 420.0f), ImVec2(FLT_MAX, FLT_MAX));

    constexpr ImGuiWindowFlags kWinFlags =
        ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_MenuBar;

    // Use a local copy so we can detect when the X button is clicked
    bool windowOpen = true;
    if (!ImGui::Begin(m_windowTitle.c_str(), &windowOpen, kWinFlags))
    {
        if (!windowOpen) RequestClose();
        ImGui::End();
        return m_open;
    }
    if (!windowOpen) RequestClose();

    if (!m_data.loaded)
    {
        ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.35f, 1.0f),
                           "Failed to load: %s", m_data.errorMsg.c_str());
        ImGui::End();
        return m_open;
    }

    // Keyboard shortcuts (only when this window or its children have focus)
    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows))
    {
        if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S, false))
            SaveToFile();
    }

    // -- Menu bar ------------------------------------------------
    RenderMenuBar();

    // -- Two-panel split ----------------------------------
    const float listW = 240.0f;

    ImGui::BeginChild("##mew_list", ImVec2(listW, 0.0f), true,
                      ImGuiWindowFlags_NoScrollbar);
    RenderMoveList();
    ImGui::EndChild();

    ImGui::SameLine();

    ImGui::BeginChild("##mew_props", ImVec2(0.0f, 0.0f), false,
                      ImGuiWindowFlags_HorizontalScrollbar);
    if (m_selectedIdx >= 0 &&
        m_selectedIdx < static_cast<int>(m_data.moves.size()))
    {
        RenderMoveProperties(m_selectedIdx);
    }
    else
    {
        const float ch = ImGui::GetContentRegionAvail().y;
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + ch * 0.45f);
        const float cw = ImGui::GetContentRegionAvail().x;
        const char* hint = "Select a move from the list.";
        ImGui::SetCursorPosX((cw - ImGui::CalcTextSize(hint).x) * 0.5f);
        ImGui::TextDisabled("%s", hint);
    }
    ImGui::EndChild();

    RenderSavePopups();

    ImGui::End();

    RenderReqViewWindow();
    RenderExtradataViewWindow();

    RenderSubWin_Requirements();
    RenderSubWin_Cancels();
    RenderSubWin_HitConditions();
    RenderSubWin_ReactionLists();
    RenderSubWin_Pushbacks();
    RenderSubWin_Voiceclips();
    RenderSubWin_Properties();
    RenderSubWin_Throws();
    RenderSubWin_Projectiles();
    RenderSubWin_InputSequences();
    RenderSubWin_ParryableMoves();

    RenderCloseConfirmModal();

    return m_open;
}

// -------------------------------------------------------------
//  Requirement viewer floating window
// -------------------------------------------------------------

void MovesetEditorWindow::RenderReqViewWindow()
{
    if (!m_reqView.open) return;

    ImGui::SetNextWindowSize(ImVec2(280.0f, 220.0f), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Requirements##reqviewer", &m_reqView.open,
                     ImGuiWindowFlags_NoCollapse))
    {
        if (m_reqView.reqs.empty())
        {
            ImGui::TextDisabled("(no requirements)");
        }
        else
        {
            constexpr ImGuiTableFlags kFlags =
                ImGuiTableFlags_BordersInnerH |
                ImGuiTableFlags_RowBg         |
                ImGuiTableFlags_SizingFixedFit;

            if (ImGui::BeginTable("##reqtbl", 3, kFlags))
            {
                ImGui::TableSetupColumn("#",     ImGuiTableColumnFlags_WidthFixed,  28.0f);
                ImGui::TableSetupColumn("req",   ImGuiTableColumnFlags_WidthFixed,  80.0f);
                ImGui::TableSetupColumn("param", ImGuiTableColumnFlags_WidthFixed,  80.0f);
                ImGui::TableHeadersRow();

                for (int i = 0; i < (int)m_reqView.reqs.size(); ++i)
                {
                    const ParsedRequirement& r = m_reqView.reqs[i];
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0); ImGui::TextDisabled("%d", i);
                    ImGui::TableSetColumnIndex(1); ImGui::Text("%u", r.req);
                    ImGui::TableSetColumnIndex(2); ImGui::Text("%u", r.param);
                }
                ImGui::EndTable();
            }
        }
    }
    ImGui::End();
}

// -------------------------------------------------------------
//  Extradata viewer floating window
// -------------------------------------------------------------

void MovesetEditorWindow::RenderExtradataViewWindow()
{
    if (!m_extradataView.open) return;

    ImGui::SetNextWindowSize(ImVec2(200.0f, 100.0f), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Cancel Extradata##edviewer", &m_extradataView.open,
                     ImGuiWindowFlags_NoCollapse))
    {
        if (m_extradataView.idx != 0xFFFFFFFF)
            ImGui::Text("index : %u", m_extradataView.idx);
        else
            ImGui::TextDisabled("index : (unknown)");

        ImGui::Text("value : %u", m_extradataView.value);
    }
    ImGui::End();
}

// -------------------------------------------------------------
//  Move list -- color classification (mirrors OldTool2 getMoveColor)
// -------------------------------------------------------------

enum class MoveCategory { None, Reaction, Attack, Throw, Generic };

static MoveCategory ClassifyMove(
    const ParsedMove& mv, uint32_t gid, int moveIdx,
    const std::vector<ParsedReactionList>& reacBlock)
{
    if (gid) return MoveCategory::Generic;

    // Throw: lower 12 bits of hitlevel == 0xA00
    if ((mv.hitlevel & 0xFFF) == 0xA00) return MoveCategory::Throw;

    // Attack: any hitbox slot with non-zero startup, recovery, or location
    for (int h = 0; h < 8; ++h)
    {
        if (mv.hitbox_active_start[h] || mv.hitbox_active_last[h] || mv.hitbox_location[h])
            return MoveCategory::Attack;
    }

    // Reaction: this move is referenced by any reaction list entry
    for (const auto& rl : reacBlock)
    {
        if (rl.standing          == (uint16_t)moveIdx ||
            rl.ch                == (uint16_t)moveIdx ||
            rl.crouch            == (uint16_t)moveIdx ||
            rl.crouch_ch         == (uint16_t)moveIdx ||
            rl.left_side         == (uint16_t)moveIdx ||
            rl.left_side_crouch  == (uint16_t)moveIdx ||
            rl.right_side        == (uint16_t)moveIdx ||
            rl.right_side_crouch == (uint16_t)moveIdx ||
            rl.back              == (uint16_t)moveIdx ||
            rl.back_crouch       == (uint16_t)moveIdx ||
            rl.block             == (uint16_t)moveIdx ||
            rl.crouch_block      == (uint16_t)moveIdx ||
            rl.wallslump         == (uint16_t)moveIdx ||
            rl.downed            == (uint16_t)moveIdx)
            return MoveCategory::Reaction;
    }

    return MoveCategory::None;
}

struct MoveRowColors { ImU32 bg; ImVec4 selected; };

static MoveRowColors GetMoveRowColors(MoveCategory cat)
{
    switch (cat)
    {
    case MoveCategory::Generic:
        return { IM_COL32( 30,  80, 160, 100), ImVec4(0.18f, 0.42f, 0.80f, 1.0f) };
    case MoveCategory::Throw:
        return { IM_COL32(120,  50, 160,  90), ImVec4(0.52f, 0.25f, 0.72f, 1.0f) };
    case MoveCategory::Attack:
        return { IM_COL32(160,  45,  45,  95), ImVec4(0.68f, 0.22f, 0.22f, 1.0f) };
    case MoveCategory::Reaction:
        return { IM_COL32(130, 100,  20,  90), ImVec4(0.58f, 0.46f, 0.12f, 1.0f) };
    default:
        return { 0, ImVec4(0.28f, 0.28f, 0.32f, 1.0f) };
    }
}

// Draw colored background rect for the current list item row
static void DrawMoveRowBg(MoveCategory cat)
{
    if (cat == MoveCategory::None) return;
    ImU32 col = GetMoveRowColors(cat).bg;
    ImVec2 p = ImGui::GetCursorScreenPos();
    float  w = ImGui::GetContentRegionAvail().x;
    float  h = ImGui::GetTextLineHeightWithSpacing();
    ImGui::GetWindowDrawList()->AddRectFilled(p, ImVec2(p.x + w, p.y + h), col);
}

// Draw selection indicator: white outline + left accent bar
static void DrawMoveRowSelected()
{
    ImVec2 mn = ImGui::GetItemRectMin();
    ImVec2 mx = ImGui::GetItemRectMax();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    // Outer outline
    dl->AddRect(mn, mx, IM_COL32(255, 255, 255, 180), 0.0f, 0, 1.5f);
    // Left accent bar (3px wide)
    dl->AddRectFilled(mn, ImVec2(mn.x + 3.0f, mx.y), IM_COL32(255, 255, 255, 230));
}

// -------------------------------------------------------------
//  Left panel -- move list
// -------------------------------------------------------------

void MovesetEditorWindow::RenderMoveList()
{
    ImGui::TextDisabled("%u moves", m_data.moveCount);
    ImGui::Spacing();

    ImGui::SetNextItemWidth(-1.0f);
    bool searchChanged = ImGui::InputText("##search", m_searchBuf, sizeof(m_searchBuf));
    (void)searchChanged;
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
        ImGui::SetTooltip("Filter by name or index");
    ImGui::Separator();

    char lowerSearch[128] = {};
    size_t slen = strlen(m_searchBuf);
    for (size_t k = 0; k < slen && k < sizeof(lowerSearch) - 1; ++k)
        lowerSearch[k] = static_cast<char>(tolower(static_cast<unsigned char>(m_searchBuf[k])));
    const bool hasFilter = (lowerSearch[0] != '\0');

    // Reserve space at the bottom for the 3 live-edit buttons (2 rows)
    const float kBtnAreaH = ImGui::GetFrameHeight() * 2.0f
                          + ImGui::GetStyle().ItemSpacing.y * 3.0f;
    ImGui::BeginChild("##mew_listinner",
                      ImVec2(0.0f, ImGui::GetContentRegionAvail().y - kBtnAreaH),
                      false);

    const int total = static_cast<int>(m_data.moves.size());

    if (!hasFilter)
    {
        ImGuiListClipper clipper;
        clipper.Begin(total);
        // Force the selected item to be included so we can call SetScrollHereY on it
        if (m_moveListScrollPending && m_selectedIdx >= 0 && m_selectedIdx < total)
            clipper.IncludeItemsByIndex(m_selectedIdx, m_selectedIdx + 1);
        while (clipper.Step())
        {
            for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i)
            {
                const ParsedMove& mv = m_data.moves[i];
                const uint32_t gid = (i < (int)m_data.moveToGenericId.size()) ? m_data.moveToGenericId[i] : 0u;
                char label[192];
                if (!mv.displayName.empty())
                {
                    if (gid) snprintf(label, sizeof(label), "#%04d  %s (%u)", i, mv.displayName.c_str(), gid);
                    else     snprintf(label, sizeof(label), "#%04d  %s", i, mv.displayName.c_str());
                }
                else
                {
                    if (gid) snprintf(label, sizeof(label), "#%04d (%u)", i, gid);
                    else     snprintf(label, sizeof(label), "#%04d", i);
                }

                MoveCategory cat = ClassifyMove(mv, gid, i, m_data.reactionListBlock);
                bool sel = (m_selectedIdx == i);
                DrawMoveRowBg(cat);
                ImGui::PushStyleColor(ImGuiCol_Header, GetMoveRowColors(cat).selected);
                if (cat == MoveCategory::Generic) ImGui::PushFont(g_fontBold);
                if (ImGui::Selectable(label, sel, ImGuiSelectableFlags_SpanAllColumns))
                    m_selectedIdx = i;
                if (cat == MoveCategory::Generic) ImGui::PopFont();
                ImGui::PopStyleColor();
                if (sel)
                {
                    DrawMoveRowSelected();
                    ImGui::SetItemDefaultFocus();
                    if (m_moveListScrollPending)
                    {
                        ImGui::SetScrollHereY(0.5f);
                        m_moveListScrollPending = false;
                    }
                }
            }
        }
        clipper.End();
    }
    else
    {
        for (int i = 0; i < total; ++i)
        {
            const ParsedMove& mv = m_data.moves[i];

            char idxStr[12];
            snprintf(idxStr, sizeof(idxStr), "%04d", i);

            auto containsLower = [&](const char* src) -> bool {
                char lsrc[256] = {};
                size_t n = strlen(src);
                for (size_t k = 0; k < n && k < sizeof(lsrc) - 1; ++k)
                    lsrc[k] = static_cast<char>(tolower(static_cast<unsigned char>(src[k])));
                return strstr(lsrc, lowerSearch) != nullptr;
            };

            bool match = containsLower(idxStr);
            if (!match && !mv.displayName.empty())
                match = containsLower(mv.displayName.c_str());
            if (!match) continue;

            const uint32_t gid = (i < (int)m_data.moveToGenericId.size()) ? m_data.moveToGenericId[i] : 0u;
            char label[192];
            if (!mv.displayName.empty())
            {
                if (gid) snprintf(label, sizeof(label), "#%04d  %s (%u)", i, mv.displayName.c_str(), gid);
                else     snprintf(label, sizeof(label), "#%04d  %s", i, mv.displayName.c_str());
            }
            else
            {
                if (gid) snprintf(label, sizeof(label), "#%04d (%u)", i, gid);
                else     snprintf(label, sizeof(label), "#%04d", i);
            }

            MoveCategory cat = ClassifyMove(mv, gid, i, m_data.reactionListBlock);
            bool sel = (m_selectedIdx == i);
            DrawMoveRowBg(cat);
            if (cat == MoveCategory::Generic) ImGui::PushFont(g_fontBold);
            if (ImGui::Selectable(label, sel, ImGuiSelectableFlags_SpanAllColumns))
                m_selectedIdx = i;
            if (cat == MoveCategory::Generic) ImGui::PopFont();
            if (sel)
            {
                DrawMoveRowSelected();
                ImGui::SetItemDefaultFocus();
                if (m_moveListScrollPending)
                {
                    ImGui::SetScrollHereY(0.5f);
                    m_moveListScrollPending = false;
                }
            }
        }
    }

    ImGui::EndChild();

    // Live-edit buttons -- always visible below the list
    ImGui::Spacing();
    const float halfW = (ImGui::GetContentRegionAvail().x
                         - ImGui::GetStyle().ItemSpacing.x) * 0.5f;

    // Resolve a raw game move ID: IDs >= 0x8000 are generic alias IDs that
    // must be looked up in original_aliases to get the real move index.
    auto ResolveGameMoveId = [&](int rawId) -> int
    {
        if (rawId < 0) return -1;
        if (rawId >= 0x8000)
        {
            int aliasIdx = rawId - 0x8000;
            if (aliasIdx < (int)m_data.originalAliases.size())
                return (int)m_data.originalAliases[aliasIdx];
            return -1; // alias out of range
        }
        return rawId;
    };

    if (ImGui::Button("Go to 1P move ID", ImVec2(halfW, 0.0f)))
    {
        int rawId = -1;
        if (GameLiveEdit::GetPlayerMoveId(0, rawId))
        {
            int id = ResolveGameMoveId(rawId);
            if (id >= 0 && id < (int)m_data.moves.size())
            {
                m_selectedIdx = id;
                m_moveListScrollPending = true;
            }
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Go to 2P move ID", ImVec2(-1.0f, 0.0f)))
    {
        int rawId = -1;
        if (GameLiveEdit::GetPlayerMoveId(1, rawId))
        {
            int id = ResolveGameMoveId(rawId);
            if (id >= 0 && id < (int)m_data.moves.size())
            {
                m_selectedIdx = id;
                m_moveListScrollPending = true;
            }
        }
    }

    if (ImGui::Button("Play Move (1P)", ImVec2(-1.0f, 0.0f)))
    {
        if (m_selectedIdx >= 0 && m_selectedIdx < (int)m_data.moves.size())
            GameLiveEdit::PlayMove(m_selectedIdx);
    }
}

// -------------------------------------------------------------
//  Right panel -- move property sections
// -------------------------------------------------------------

// -- Shared property table helpers ----------------------------

static bool BeginPropTable(const char* id = "##pt")
{
    constexpr ImGuiTableFlags kFlags =
        ImGuiTableFlags_BordersInnerH |
        ImGuiTableFlags_RowBg         |
        ImGuiTableFlags_SizingFixedFit;
    if (!ImGui::BeginTable(id, 2, kFlags)) return false;
    ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, 220.0f);
    ImGui::TableSetupColumn("Value",    ImGuiTableColumnFlags_WidthStretch);
    return true;
}

static void RowU32(const char* lbl, uint32_t v)
{
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0); ImGui::TextDisabled("%s", lbl);
    ImGui::TableSetColumnIndex(1); ImGui::Text("%u", v);
}

static void RowI32(const char* lbl, int32_t v)
{
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0); ImGui::TextDisabled("%s", lbl);
    ImGui::TableSetColumnIndex(1); ImGui::Text("%d", v);
}

static void RowHex32(const char* lbl, uint32_t v)
{
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0); ImGui::TextDisabled("%s", lbl);
    ImGui::TableSetColumnIndex(1); ImGui::Text("0x%08X", v);
}

static void RowHex64(const char* lbl, uint64_t v)
{
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0); ImGui::TextDisabled("%s", lbl);
    ImGui::TableSetColumnIndex(1);
    ImGui::Text("0x%016llX", static_cast<unsigned long long>(v));
}

static void RowU16(const char* lbl, uint16_t v)
{
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0); ImGui::TextDisabled("%s", lbl);
    ImGui::TableSetColumnIndex(1); ImGui::Text("%u", v);
}

static void RowPtr(const char* lbl, uint64_t v)
{
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0); ImGui::TextDisabled("%s", lbl);
    ImGui::TableSetColumnIndex(1);
    if (v == 0)
        ImGui::TextDisabled("null");
    else
        ImGui::Text("0x%08llX", static_cast<unsigned long long>(v));
}

static void RowMoveRef(const char* lbl, uint16_t moveId)
{
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0); ImGui::TextDisabled("%s", lbl);
    ImGui::TableSetColumnIndex(1); ImGui::Text("-> #%04u", moveId);
}

static void RowStr(const char* lbl, const char* v)
{
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0); ImGui::TextDisabled("%s", lbl);
    ImGui::TableSetColumnIndex(1);
    if (!v || !v[0])
        ImGui::TextDisabled("(empty)");
    else
        ImGui::TextUnformatted(v);
}

static void RowIdx(const char* lbl, uint32_t idx)
{
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0); ImGui::TextDisabled("%s", lbl);
    ImGui::TableSetColumnIndex(1);
    if (idx == 0xFFFFFFFF) ImGui::TextDisabled("--");
    else ImGui::Text("[%u]", idx);
}

static void RowI16(const char* lbl, int16_t v)
{
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0); ImGui::TextDisabled("%s", lbl);
    ImGui::TableSetColumnIndex(1); ImGui::Text("%d", static_cast<int>(v));
}

// Clickable index-reference row.
//   valid=true  -> light green label, "[N]" value, "Click to navigate" tooltip
//   valid=false -> pink label, "[N]" or "--" value, "Navigate target not found." tooltip
static bool RowIdxLink(const char* lbl, uint32_t idx, bool valid)
{
    static constexpr ImVec4 kGreen = {0.55f, 0.85f, 0.55f, 1.0f};
    static constexpr ImVec4 kPink  = {1.00f, 0.50f, 0.65f, 1.0f};

    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    if (valid)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, kGreen);
        bool clicked = ImGui::Selectable(lbl, false, ImGuiSelectableFlags_SpanAllColumns);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Click to navigate");
        ImGui::PopStyleColor();
        ImGui::TableSetColumnIndex(1);
        ImGui::Text("[%u]", idx);
        return clicked;
    }
    else
    {
        ImGui::PushStyleColor(ImGuiCol_Text, kPink);
        ImGui::TextUnformatted(lbl);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Navigate target not found.");
        ImGui::PopStyleColor();
        ImGui::TableSetColumnIndex(1);
        if (idx == 0xFFFFFFFF) ImGui::TextDisabled("--");
        else                   ImGui::Text("[%u]", idx);
        return false;
    }
}

// Clickable move-reference row for transition field (shows signed int value).
//   valid=true  -> light green + clickable
//   valid=false -> pink + non-clickable + tooltip
static bool RowTransitionLink(const char* lbl, int16_t val, bool valid)
{
    static constexpr ImVec4 kGreen = {0.55f, 0.85f, 0.55f, 1.0f};
    static constexpr ImVec4 kPink  = {1.00f, 0.50f, 0.65f, 1.0f};

    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    if (valid)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, kGreen);
        bool clicked = ImGui::Selectable(lbl, false, ImGuiSelectableFlags_SpanAllColumns);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Click to navigate");
        ImGui::PopStyleColor();
        ImGui::TableSetColumnIndex(1);
        ImGui::Text("%d", static_cast<int>(val));
        return clicked;
    }
    else
    {
        ImGui::PushStyleColor(ImGuiCol_Text, kPink);
        ImGui::TextUnformatted(lbl);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Navigate target not found.");
        ImGui::PopStyleColor();
        ImGui::TableSetColumnIndex(1);
        ImGui::Text("%d", static_cast<int>(val));
        return false;
    }
}

// Clickable move-reference row -- displays as #NNNN format.
//   valid=true  -> light green, "Click to navigate" tooltip
//   valid=false -> pink, "Navigate target not found." tooltip
static bool RowMoveLink(const char* lbl, uint16_t moveId, bool valid)
{
    static constexpr ImVec4 kGreen = {0.55f, 0.85f, 0.55f, 1.0f};
    static constexpr ImVec4 kPink  = {1.00f, 0.50f, 0.65f, 1.0f};
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    if (valid)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, kGreen);
        bool clicked = ImGui::Selectable(lbl, false, ImGuiSelectableFlags_SpanAllColumns);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Click to navigate");
        ImGui::PopStyleColor();
        ImGui::TableSetColumnIndex(1);
        ImGui::Text("#%04u", moveId);
        return clicked;
    }
    else
    {
        ImGui::PushStyleColor(ImGuiCol_Text, kPink);
        ImGui::TextUnformatted(lbl);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Navigate target not found.");
        ImGui::PopStyleColor();
        ImGui::TableSetColumnIndex(1);
        ImGui::Text("#%04u", moveId);
        return false;
    }
}

// Generic move ID (>=0x8000) link -- shows "0x%04X" with tooltip "(-> move #N)" when valid.
static bool RowGenericMoveLink(const char* lbl, uint16_t genericId, int resolvedIdx, bool valid)
{
    static constexpr ImVec4 kGreen = {0.55f, 0.85f, 0.55f, 1.0f};
    static constexpr ImVec4 kPink  = {1.00f, 0.50f, 0.65f, 1.0f};
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    if (valid)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, kGreen);
        char selId[64];
        snprintf(selId, sizeof(selId), "%s##gml", lbl);
        bool clicked = ImGui::Selectable(selId, false, ImGuiSelectableFlags_SpanAllColumns);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Click to navigate  (-> move #%d)", resolvedIdx);
        ImGui::PopStyleColor();
        ImGui::TableSetColumnIndex(1);
        ImGui::Text("%u", (unsigned)genericId);
        return clicked;
    }
    else
    {
        ImGui::PushStyleColor(ImGuiCol_Text, kPink);
        ImGui::TextUnformatted(lbl);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Navigate target not found.");
        ImGui::PopStyleColor();
        ImGui::TableSetColumnIndex(1);
        ImGui::Text("%u", (unsigned)genericId);
        return false;
    }
}

// -------------------------------------------------------------
//  Editable row helpers -- return true if the value changed.
//  The caller is responsible for setting m_dirty.
// -------------------------------------------------------------

static bool RowU32Edit(const char* id, const char* lbl, uint32_t& v)
{
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0); ImGui::TextDisabled("%s", lbl);
    ImGui::TableSetColumnIndex(1);
    ImGui::SetNextItemWidth(-1.0f);
    int tmp = static_cast<int>(v);
    if (ImGui::InputInt(id, &tmp, 0, 0))
    {
        v = static_cast<uint32_t>(tmp);
        return true;
    }
    return false;
}

static bool RowI32Edit(const char* id, const char* lbl, int32_t& v)
{
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0); ImGui::TextDisabled("%s", lbl);
    ImGui::TableSetColumnIndex(1);
    ImGui::SetNextItemWidth(-1.0f);
    return ImGui::InputInt(id, &v, 0, 0);
}

static bool RowHex32Edit(const char* id, const char* lbl, uint32_t& v)
{
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0); ImGui::TextDisabled("%s", lbl);
    ImGui::TableSetColumnIndex(1);
    ImGui::SetNextItemWidth(-1.0f);
    char buf[14]; snprintf(buf, sizeof(buf), "0x%08X", v);
    ImGui::InputText(id, buf, sizeof(buf));
    if (ImGui::IsItemDeactivatedAfterEdit())
    {
        const char* p = buf;
        if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) p += 2;
        v = static_cast<uint32_t>(strtoul(p, nullptr, 16));
        return true;
    }
    return false;
}

static bool RowHex64Edit(const char* id, const char* lbl, uint64_t& v)
{
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0); ImGui::TextDisabled("%s", lbl);
    ImGui::TableSetColumnIndex(1);
    ImGui::SetNextItemWidth(-1.0f);
    char buf[22]; snprintf(buf, sizeof(buf), "0x%016llX", static_cast<unsigned long long>(v));
    ImGui::InputText(id, buf, sizeof(buf));
    if (ImGui::IsItemDeactivatedAfterEdit())
    {
        const char* p = buf;
        if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) p += 2;
        v = static_cast<uint64_t>(strtoull(p, nullptr, 16));
        return true;
    }
    return false;
}

static bool RowU16Edit(const char* id, const char* lbl, uint16_t& v)
{
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0); ImGui::TextDisabled("%s", lbl);
    ImGui::TableSetColumnIndex(1);
    ImGui::SetNextItemWidth(-1.0f);
    int tmp = static_cast<int>(v);
    if (ImGui::InputInt(id, &tmp, 0, 0))
    {
        v = static_cast<uint16_t>(tmp);
        return true;
    }
    return false;
}

static bool RowI16Edit(const char* id, const char* lbl, int16_t& v)
{
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0); ImGui::TextDisabled("%s", lbl);
    ImGui::TableSetColumnIndex(1);
    ImGui::SetNextItemWidth(-1.0f);
    int tmp = static_cast<int>(v);
    if (ImGui::InputInt(id, &tmp, 0, 0))
    {
        v = static_cast<int16_t>(tmp);
        return true;
    }
    return false;
}

static bool RowF32Edit(const char* id, const char* lbl, float& v)
{
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0); ImGui::TextDisabled("%s", lbl);
    ImGui::TableSetColumnIndex(1);
    ImGui::SetNextItemWidth(-1.0f);
    return ImGui::InputFloat(id, &v, 0.0f, 0.0f, "%.6f");
}

// -------------------------------------------------------
//  Index-edit helpers: editable input + navigable label
//  Returns {changed, navigate}
// -------------------------------------------------------
struct IdxEditResult { bool changed; bool navigate; };

// Uint32 index: left col = colored clickable label (nav), right col = InputInt
static IdxEditResult RowIdxEditLink(const char* inputId, const char* lbl, uint32_t& v, bool valid)
{
    static constexpr ImVec4 kGreen = {0.55f, 0.85f, 0.55f, 1.0f};
    static constexpr ImVec4 kPink  = {1.00f, 0.50f, 0.65f, 1.0f};
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::PushStyleColor(ImGuiCol_Text, valid ? kGreen : kPink);
    char selId[96]; snprintf(selId, sizeof(selId), "%s##eilbl", lbl);
    bool nav = ImGui::Selectable(selId, false, ImGuiSelectableFlags_None);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip(v == 0xFFFFFFFF ? "null" : valid ? "Click to navigate" : "Index out of range");
    ImGui::PopStyleColor();
    ImGui::TableSetColumnIndex(1);
    ImGui::SetNextItemWidth(-1.0f);
    int tmp = (v == 0xFFFFFFFF) ? -1 : (int)v;
    bool chg = ImGui::InputInt(inputId, &tmp, 0, 0);
    if (chg) v = (tmp < 0) ? 0xFFFFFFFF : (uint32_t)tmp;
    return {chg, nav && valid};
}

// Uint16 move id: left col = colored clickable label, right col = InputInt
// resolvedIdx >= 0 means generic id was resolved; shown in tooltip.
static IdxEditResult RowMoveEditLink(const char* inputId, const char* lbl, uint16_t& v,
                                     bool valid, int resolvedIdx = -1, bool isGeneric = false)
{
    static constexpr ImVec4 kGreen = {0.55f, 0.85f, 0.55f, 1.0f};
    static constexpr ImVec4 kPink  = {1.00f, 0.50f, 0.65f, 1.0f};
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::PushStyleColor(ImGuiCol_Text, valid ? kGreen : kPink);
    char selId[96]; snprintf(selId, sizeof(selId), "%s##meilbl", lbl);
    bool nav = ImGui::Selectable(selId, false, ImGuiSelectableFlags_None);
    if (ImGui::IsItemHovered()) {
        if (valid && isGeneric && resolvedIdx >= 0)
            ImGui::SetTooltip("Click to navigate  (-> move #%d)", resolvedIdx);
        else
            ImGui::SetTooltip(valid ? "Click to navigate" : "Navigate target not found.");
    }
    ImGui::PopStyleColor();
    ImGui::TableSetColumnIndex(1);
    ImGui::SetNextItemWidth(-1.0f);
    int tmp = (int)(uint32_t)v;
    bool chg = ImGui::InputInt(inputId, &tmp, 0, 0);
    if (chg) v = (uint16_t)((uint32_t)tmp & 0xFFFFu);
    return {chg, nav && valid};
}

// Finds the outer group whose absolute start index equals absIdx. Returns -1 if not found.
static int FindGroupOuter(const std::vector<std::pair<uint32_t,uint32_t>>& groups,
                          uint32_t absIdx)
{
    for (int i = 0; i < (int)groups.size(); ++i)
        if (groups[i].first == absIdx) return i;
    return -1;
}

// -------------------------------------------------------------

static void RenderSection_Hitboxes(ParsedMove& m, bool& dirty);

void MovesetEditorWindow::RenderMoveProperties(int idx)
{
    ParsedMove& m = m_data.moves[idx];

    // -- Title + summary strip --------------------------------
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.85f, 0.90f, 1.00f, 1.00f));
    if (!m.displayName.empty())
        ImGui::Text("Move  #%04d   %s", idx, m.displayName.c_str());
    else
        ImGui::Text("Move  #%04d", idx);
    ImGui::PopStyleColor();

    ImGui::Separator();

    // -- Tab bar ----------------------------------------------
    if (!ImGui::BeginTabBar("##move_tabs")) return;

    {
        char overviewLabel[256];
        snprintf(overviewLabel, sizeof(overviewLabel),
                 "Move: %s###tab_overview", m.displayName.c_str());
        if (ImGui::BeginTabItem(overviewLabel))
        {
            ImGui::Spacing();
            RenderSection_Overview(m, m_dirty);
            ImGui::EndTabItem();
        }
    }

    if (ImGui::BeginTabItem("Hitboxes###tab_hitboxes"))
    {
        ImGui::Spacing();
        RenderSection_Hitboxes(m, m_dirty);
        ImGui::EndTabItem();
    }

    if (ImGui::BeginTabItem("Debug###tab_debug"))
    {
        ImGui::Spacing();
        RenderSection_Unknown(m, m_dirty);
        ImGui::EndTabItem();
    }

    ImGui::EndTabBar();
}

// -------------------------------------------------------------
//  Group computation helpers (placed here so navigation handlers in
//  RenderSection_Overview can call them without forward-declaration issues)
// -------------------------------------------------------------

template<typename T, typename Pred>
static std::vector<std::pair<uint32_t,uint32_t>>
ComputeGroups(const std::vector<T>& block, Pred isTerminator)
{
    std::vector<std::pair<uint32_t,uint32_t>> groups;
    if (block.empty()) return groups;
    uint32_t start = 0;
    for (uint32_t i = 0; i < (uint32_t)block.size(); ++i)
    {
        if (isTerminator(block[i]))
        {
            groups.push_back({start, i - start + 1});
            start = i + 1;
        }
    }
    if (start < (uint32_t)block.size())
        groups.push_back({start, (uint32_t)block.size() - start});
    return groups;
}

// Forward declaration -- full definition lives near RenderSubWin_Properties
static std::vector<std::pair<uint32_t,uint32_t>>
ComputeOtherPropGroups(const std::vector<ParsedExtraProp>&,
                       const std::vector<ParsedRequirement>&);

// -- Section: Overview (3-column layout matching OldTool2 field order) ---------
void MovesetEditorWindow::RenderSection_Overview(ParsedMove& m, bool& dirty)
{
    using namespace MoveLabel;
    const float spacing = ImGui::GetStyle().ItemSpacing.x;
    const float colW    = (ImGui::GetContentRegionAvail().x - spacing * 2.0f) / 3.0f;

    // -- Pre-compute groups (used for both validity checks and click navigation) -
    const auto cancelGroups = ComputeGroups(m_data.cancelBlock,
        +[](const ParsedCancel& c)->bool{ return c.command == 0x8000; });

    std::vector<std::pair<uint32_t,uint32_t>> hitCondGroups;
    {
        const auto& blk    = m_data.hitConditionBlock;
        const auto& reqBlk = m_data.requirementBlock;
        uint32_t start = 0;
        for (uint32_t i = 0; i < (uint32_t)blk.size(); ++i)
        {
            const auto& h = blk[i];
            bool isTerm = (h.requirement_addr == 0);
            if (!isTerm && h.req_list_idx != 0xFFFFFFFF &&
                h.req_list_idx < (uint32_t)reqBlk.size())
                isTerm = (reqBlk[h.req_list_idx].req == GameStatic::Get().data.reqListEnd);
            if (isTerm) { hitCondGroups.push_back({start, i - start + 1}); start = i + 1; }
        }
        if (start < (uint32_t)blk.size())
            hitCondGroups.push_back({start, (uint32_t)blk.size() - start});
    }

    const auto epGroups = ComputeGroups(m_data.extraPropBlock,
        +[](const ParsedExtraProp& e)->bool{ return e.type == 0 && e.id == 0; });
    const auto spGroups = ComputeOtherPropGroups(m_data.startPropBlock, m_data.requirementBlock);
    const auto npGroups = ComputeOtherPropGroups(m_data.endPropBlock,   m_data.requirementBlock);

    // -- Validity flags --------------------------------------------------------
    const uint16_t transU16        = m.transition;
    const bool     transIsGeneric  = (transU16 >= 0x8000);
    // Resolve transition: generic IDs use original_aliases; direct IDs check move count
    int transResolvedIdx = -1;
    if (transIsGeneric)
    {
        const uint32_t aliasIdx = transU16 - 0x8000u;
        if (aliasIdx < m_data.originalAliases.size())
        {
            const uint16_t resolved = m_data.originalAliases[aliasIdx];
            if (resolved < static_cast<uint16_t>(m_data.moves.size()))
                transResolvedIdx = static_cast<int>(resolved);
        }
    }
    else
    {
        if (transU16 < static_cast<uint16_t>(m_data.moves.size()))
            transResolvedIdx = static_cast<int>(transU16);
    }
    const bool transValid = (transResolvedIdx >= 0);
    const bool cancelValid    = (m.cancel_idx        != 0xFFFFFFFF) && (FindGroupOuter(cancelGroups,   m.cancel_idx)        >= 0);
    const bool hitCondValid   = (m.hit_condition_idx != 0xFFFFFFFF) && (FindGroupOuter(hitCondGroups,  m.hit_condition_idx) >= 0);
    const bool voiceclipValid = (m.voiceclip_idx     != 0xFFFFFFFF) && (m.voiceclip_idx < m_data.voiceclipBlock.size());
    const bool epValid        = (m.extra_prop_idx    != 0xFFFFFFFF) && (FindGroupOuter(epGroups,       m.extra_prop_idx)    >= 0);
    const bool spValid        = (m.start_prop_idx    != 0xFFFFFFFF) && (FindGroupOuter(spGroups,       m.start_prop_idx)    >= 0);
    const bool npValid        = (m.end_prop_idx      != 0xFFFFFFFF) && (FindGroupOuter(npGroups,       m.end_prop_idx)      >= 0);

    // -- Column 1 -------------------------------------------------------------
    bool clickCancel    = false, clickTrans    = false, clickHitCond   = false;
    bool clickVoiceclip = false, clickExtraProp = false;
    bool clickStartProp = false, clickEndProp   = false;

    ImGui::BeginChild("##ov_c1", ImVec2(colW, 0.0f), false);
    if (BeginPropTable("##ov1")) {
        // Editable name (stored in editor_datas.json, not in motbin)
        {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0); ImGui::TextDisabled("%s", MoveLabel::Name);
            ImGui::TableSetColumnIndex(1);
            ImGui::SetNextItemWidth(-1.0f);
            char nameBuf[256];
            snprintf(nameBuf, sizeof(nameBuf), "%s", m.displayName.c_str());
            ImGui::InputText("##move_name", nameBuf, sizeof(nameBuf));
            if (ImGui::IsItemDeactivatedAfterEdit())
            {
                m.displayName = nameBuf;
                if (nameBuf[0] != '\0')
                    m_customNames[m_selectedIdx] = m.displayName;
                else
                    m_customNames.erase(m_selectedIdx);
                SaveEditorDatas();
            }
        }
        RowHex32(NameKey,      m.name_key);
        // anim_key: value + anim name lookup
        {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0); ImGui::TextDisabled("%s", AnimKey);
            ImGui::TableSetColumnIndex(1);
            const char* animName = LabelDB::Get().GetAnimName(m.anim_key);
            if (animName) ImGui::Text("0x%08X  (%s)", m.anim_key, animName);
            else          ImGui::Text("0x%08X", m.anim_key);
        }
        RowHex32(SkeletonId,   m.anmbin_body_sub_idx);
        if (RowU32Edit ("##vuln",     Vuln,     m.vuln))     dirty = true;
        if (RowU32Edit ("##hitlevel", Hitlevel, m.hitlevel)) dirty = true;
        { auto r = RowIdxEditLink("##cancel_idx", CancelIdx, m.cancel_idx, cancelValid);
          if (r.changed) dirty = true;  clickCancel = r.navigate; }
        { auto r = RowMoveEditLink("##transition", Transition, m.transition, transValid, transResolvedIdx, transIsGeneric);
          if (r.changed) dirty = true;  clickTrans = r.navigate; }
        if (RowI32Edit ("##anim_len", AnimLen,  m.anim_len)) dirty = true;
        { auto r = RowIdxEditLink("##hitcond_idx", HitCondIdx, m.hit_condition_idx, hitCondValid);
          if (r.changed) dirty = true;  clickHitCond = r.navigate; }
        { auto r = RowIdxEditLink("##voice_idx", VoiceclipIdx, m.voiceclip_idx, voiceclipValid);
          if (r.changed) dirty = true;  clickVoiceclip = r.navigate; }
        { auto r = RowIdxEditLink("##eprop_idx", ExtraPropIdx, m.extra_prop_idx, epValid);
          if (r.changed) dirty = true;  clickExtraProp = r.navigate; }
        { auto r = RowIdxEditLink("##sprop_idx", StartPropIdx, m.start_prop_idx, spValid);
          if (r.changed) dirty = true;  clickStartProp = r.navigate; }
        { auto r = RowIdxEditLink("##nprop_idx", EndPropIdx, m.end_prop_idx, npValid);
          if (r.changed) dirty = true;  clickEndProp = r.navigate; }
        ImGui::EndTable();
    }
    ImGui::EndChild(); ImGui::SameLine();

    // -- Column 2 -------------------------------------------------------------
    ImGui::BeginChild("##ov_c2", ImVec2(0.0f, 0.0f), false);
    if (BeginPropTable("##ov2")) {
        if (RowI16Edit   ("##0xCE",      CE,        m._0xCE))      dirty = true;
        if (RowHex32Edit ("##t_char_id", TCharId,   m.ordinal_id2)) dirty = true;
        if (RowHex32Edit ("##ordinal_id",OrdinalId, m.moveId))      dirty = true;
        if (RowU32Edit ("##0x118",         F0x118,        m._0x118))         dirty = true;
        if (RowU32Edit ("##0x11C",         F0x11C,        m._0x11C))         dirty = true;
        if (RowU32Edit ("##airborne_start",AirborneStart, m.airborne_start)) dirty = true;
        if (RowU32Edit ("##airborne_end",  AirborneEnd,   m.airborne_end))   dirty = true;
        if (RowU32Edit ("##ground_fall",   GroundFall,    m.ground_fall))    dirty = true;
        if (RowU32Edit ("##0x154",         F0x154,        m._0x154))         dirty = true;
        if (RowU32Edit ("##u6",            U6,            m.u6))             dirty = true;
        if (RowHex32Edit("##u15",          U15,           m.u15))            dirty = true;
        {
            // collision stored as uint16, display/edit as int16
            int16_t col16 = static_cast<int16_t>(m.collision);
            if (RowI16Edit("##collision", Collision, col16)) { m.collision = static_cast<uint16_t>(col16); dirty = true; }
        }
        {
            int16_t dist16 = static_cast<int16_t>(m.distance);
            if (RowI16Edit("##distance", Distance, dist16)) { m.distance = static_cast<uint16_t>(dist16); dirty = true; }
        }
        if (RowU32Edit ("##u18",           U18,           m.u18))            dirty = true;
        ImGui::EndTable();
    }
    ImGui::EndChild();

    // -- Index navigation handlers (reuse pre-computed groups) -----------------
    if (clickCancel)
    {
        int gi = FindGroupOuter(cancelGroups, m.cancel_idx);
        if (gi >= 0)
        {
            m_cancelsWin.cancelSel.outer       = gi;
            m_cancelsWin.cancelSel.inner       = 0;
            m_cancelsWin.cancelSel.scrollOuter = true;
        }
        m_cancelsWin.open         = true;
        m_cancelsWin.pendingFocus = true;
    }
    if (clickTrans)
    {
        m_selectedIdx = transResolvedIdx; // already resolved (direct or via original_aliases)
        m_moveListScrollPending = true;
    }
    if (clickHitCond)
    {
        int gi = FindGroupOuter(hitCondGroups, m.hit_condition_idx);
        if (gi >= 0)
        {
            m_hitCondWinSel.outer       = gi;
            m_hitCondWinSel.inner       = 0;
            m_hitCondWinSel.scrollOuter = true;
        }
        m_hitCondWinOpen  = true;
        m_hitCondWinFocus = true;
    }
    if (clickVoiceclip)
    {
        m_voiceclipWinSel    = (int)m.voiceclip_idx;
        m_voiceclipWinOpen   = true;
        m_voiceclipWinFocus  = true;
        m_voiceclipWinScroll = true;
    }
    if (clickExtraProp)
    {
        int gi = FindGroupOuter(epGroups, m.extra_prop_idx);
        if (gi >= 0)
        {
            m_propertiesWin.epSel.outer       = gi;
            m_propertiesWin.epSel.inner       = 0;
            m_propertiesWin.epSel.scrollOuter = true;
        }
        m_propertiesWin.open         = true;
        m_propertiesWin.pendingFocus = true;
        m_propertiesWin.pendingTab   = 0;
    }
    if (clickStartProp)
    {
        int gi = FindGroupOuter(spGroups, m.start_prop_idx);
        if (gi >= 0)
        {
            m_propertiesWin.spSel.outer       = gi;
            m_propertiesWin.spSel.inner       = 0;
            m_propertiesWin.spSel.scrollOuter = true;
        }
        m_propertiesWin.open         = true;
        m_propertiesWin.pendingFocus = true;
        m_propertiesWin.pendingTab   = 1;
    }
    if (clickEndProp)
    {
        int gi = FindGroupOuter(npGroups, m.end_prop_idx);
        if (gi >= 0)
        {
            m_propertiesWin.npSel.outer       = gi;
            m_propertiesWin.npSel.inner       = 0;
            m_propertiesWin.npSel.scrollOuter = true;
        }
        m_propertiesWin.open         = true;
        m_propertiesWin.pendingFocus = true;
        m_propertiesWin.pendingTab   = 2;
    }
}

// -- Section: Debug (raw encrypted blocks) --------------------

void MovesetEditorWindow::RenderSection_Unknown(ParsedMove& m, bool& dirty)
{
    ImGui::TextDisabled("Raw 64-bit encrypted values as stored in file.");
    ImGui::Spacing();

    if (!BeginPropTable("##unk")) return;

    if (RowHex64Edit("##u1", MoveLabel::U1, m.u1)) dirty = true;
    if (RowHex64Edit("##u2", MoveLabel::U2, m.u2)) dirty = true;
    if (RowHex64Edit("##u3", MoveLabel::U3, m.u3)) dirty = true;
    if (RowHex64Edit("##u4", MoveLabel::U4, m.u4)) dirty = true;
    RowHex64(MoveLabel::EncNameKey,     m.encrypted_name_key);
    RowHex64(MoveLabel::NameEncKey,     m.name_encryption_key);
    RowHex64(MoveLabel::EncAnimKey,     m.encrypted_anim_key);
    RowHex64(MoveLabel::AnimEncKey,     m.anim_encryption_key);
    RowU32  (MoveLabel::AnmbinBodyIdx,  m.anmbin_body_idx);
    RowHex64(MoveLabel::EncVuln,        m.encrypted_vuln);
    RowHex64(MoveLabel::VulnEncKey,     m.vuln_encryption_key);
    RowHex64(MoveLabel::EncHitlevel,    m.encrypted_hitlevel);
    RowHex64(MoveLabel::HitlevelEncKey, m.hitlevel_encryption_key);
    RowHex64(MoveLabel::EncOrdinalId2,  m.encrypted_ordinal_id2);
    RowHex64(MoveLabel::OrdinalId2Key,  m.ordinal_id2_enc_key);
    RowHex64(MoveLabel::EncOrdinalId,   m.encrypted_ordinal_id);
    RowHex64(MoveLabel::OrdinalEncKey,  m.ordinal_encryption_key);
    RowU32  (MoveLabel::AnmbinBodyIdx,  m.anmbin_body_idx);
    ImGui::EndTable();

}

// -------------------------------------------------------------
//  Hitbox tab
// -------------------------------------------------------------

static void RenderSection_Hitboxes(ParsedMove& m, bool& dirty)
{
    for (int h = 0; h < 8; ++h)
    {
        const uint32_t start = m.hitbox_active_start[h];
        const uint32_t last  = m.hitbox_active_last[h];
        const uint32_t loc   = m.hitbox_location[h];
        const float*   fl    = m.hitbox_floats[h];

        // Collapsed header label: one line summary
        char header[256];
        snprintf(header, sizeof(header),
            "Hitbox %d   ACTIVE: %u -> %u   LOC: 0x%08X   "
            "RELATED FLOATS: %.3f, %.3f, %.3f, %.3f, %.3f, %.3f, %.3f, %.3f, %.3f##hb%d",
            h + 1, start, last, loc,
            fl[0], fl[1], fl[2], fl[3], fl[4], fl[5], fl[6], fl[7], fl[8],
            h);

        if (ImGui::CollapsingHeader(header))
        {
            ImGui::Indent(16.0f);

            if (BeginPropTable("##hbdt"))
            {
                char id[32];
                snprintf(id, sizeof(id), "##hb%d_start", h);
                if (RowU32Edit(id, HitboxLabel::ActiveStart, m.hitbox_active_start[h])) dirty = true;
                snprintf(id, sizeof(id), "##hb%d_last", h);
                if (RowU32Edit(id, HitboxLabel::ActiveLast,  m.hitbox_active_last[h]))  dirty = true;
                snprintf(id, sizeof(id), "##hb%d_loc", h);
                if (RowHex32Edit(id, HitboxLabel::Location,  m.hitbox_location[h]))     dirty = true;
                for (int f = 0; f < 9; ++f)
                {
                    snprintf(id, sizeof(id), "##hb%d_f%d", h, f);
                    char flbl[16]; snprintf(flbl, sizeof(flbl), HitboxLabel::FloatFmt, f);
                    if (RowF32Edit(id, flbl, m.hitbox_floats[h][f])) dirty = true;
                }
                ImGui::EndTable();
            }

            ImGui::Unindent(16.0f);
            ImGui::Spacing();
        }
    }
}

// -------------------------------------------------------------
//  2-level list helper: renders outer list in current child window.
//  Returns true if the selected inner item changed.
// -------------------------------------------------------------

static void Render2LevelOuterList(
    const std::vector<std::pair<uint32_t,uint32_t>>& groups,
    MovesetEditorWindow::TwoLevelSel& sel,
    const char* listLabel)
{
    ImGui::TextDisabled("%s (%d)", listLabel, (int)groups.size());
    ImGui::Separator();
    for (int gi = 0; gi < (int)groups.size(); ++gi)
    {
        uint32_t count = groups[gi].second;
        uint32_t items = (count > 0) ? count - 1 : 0; // exclude terminator from count
        char lbl[48];
        snprintf(lbl, sizeof(lbl), "[%u]  (%u)##g%d", groups[gi].first, items, gi);
        bool sel_ = (sel.outer == gi);
        if (ImGui::Selectable(lbl, sel_))
        {
            sel.outer = gi;
            sel.inner = 0;
        }
        if (sel_ && sel.scrollOuter)
        {
            ImGui::SetScrollHereY(0.5f);
            sel.scrollOuter = false;
        }
    }
}

// -------------------------------------------------------------
//  View menu
// -------------------------------------------------------------

void MovesetEditorWindow::RenderMenuBar()
{
    if (!ImGui::BeginMenuBar()) return;

    if (ImGui::BeginMenu("File"))
    {
        if (ImGui::MenuItem("Save", "Ctrl+S", false, m_dirty))
            SaveToFile();
        ImGui::MenuItem("Save As...", nullptr, false, false); // not yet implemented
        ImGui::Separator();
        if (ImGui::MenuItem("Close Editor"))
            RequestClose();
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Edit"))
    {
        // reserved for future features
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("View"))
    {
        if (ImGui::BeginMenu("Layout"))
        {
            if (ImGui::MenuItem("Requirements",   nullptr, m_reqWinOpen))        m_reqWinOpen        = !m_reqWinOpen;
            if (ImGui::MenuItem("Cancels",         nullptr, m_cancelsWin.open))   m_cancelsWin.open   = !m_cancelsWin.open;
            if (ImGui::MenuItem("Hit Conditions",  nullptr, m_hitCondWinOpen))    m_hitCondWinOpen    = !m_hitCondWinOpen;
            if (ImGui::MenuItem("Reaction Lists",  nullptr, m_reacWin.open))      m_reacWin.open      = !m_reacWin.open;
            if (ImGui::MenuItem("Pushbacks",       nullptr, m_pushbackWin.open))  m_pushbackWin.open  = !m_pushbackWin.open;
            if (ImGui::MenuItem("Voiceclips",      nullptr, m_voiceclipWinOpen))  m_voiceclipWinOpen  = !m_voiceclipWinOpen;
            if (ImGui::MenuItem("Properties",      nullptr, m_propertiesWin.open))m_propertiesWin.open= !m_propertiesWin.open;
            if (ImGui::MenuItem("Throws",          nullptr, m_throwsWin.open))      m_throwsWin.open      = !m_throwsWin.open;
            if (ImGui::MenuItem("Projectiles",      nullptr, m_projectileWin.open)) m_projectileWin.open  = !m_projectileWin.open;
            if (ImGui::MenuItem("Input Sequences",  nullptr, m_inputSeqWin.open))  m_inputSeqWin.open    = !m_inputSeqWin.open;
            if (ImGui::MenuItem("Parryable Moves",  nullptr, m_parryWinOpen))      m_parryWinOpen        = !m_parryWinOpen;
            ImGui::EndMenu();
        }
        ImGui::EndMenu();
    }

    ImGui::EndMenuBar();
}

// -------------------------------------------------------------
//  Editor-only persistent data (.tkedit/editor_datas.json)
// -------------------------------------------------------------

void MovesetEditorWindow::LoadEditorDatas()
{
    std::string path = m_data.folderPath;
    if (!path.empty() && path.back() != '\\' && path.back() != '/')
        path += '\\';
    path += ".tkedit\\editor_datas.json";

    std::ifstream f(path);
    if (!f.is_open()) return;

    std::string content((std::istreambuf_iterator<char>(f)),
                         std::istreambuf_iterator<char>());

    // Minimal hand-rolled JSON parse for "custom_names": { "0": "name", ... }
    auto findKey = [&](const std::string& s, const std::string& key) -> size_t {
        std::string quoted = "\"" + key + "\"";
        return s.find(quoted);
    };

    size_t cnPos = findKey(content, "custom_names");
    if (cnPos == std::string::npos) return;

    size_t braceOpen = content.find('{', cnPos);
    if (braceOpen == std::string::npos) return;
    size_t braceClose = content.find('}', braceOpen);
    if (braceClose == std::string::npos) return;

    std::string obj = content.substr(braceOpen + 1, braceClose - braceOpen - 1);

    // Parse "idx": "name" pairs
    size_t pos = 0;
    while (pos < obj.size())
    {
        size_t q1 = obj.find('"', pos);
        if (q1 == std::string::npos) break;
        size_t q2 = obj.find('"', q1 + 1);
        if (q2 == std::string::npos) break;
        std::string idxStr = obj.substr(q1 + 1, q2 - q1 - 1);

        size_t colon = obj.find(':', q2 + 1);
        if (colon == std::string::npos) break;
        size_t q3 = obj.find('"', colon + 1);
        if (q3 == std::string::npos) break;
        size_t q4 = obj.find('"', q3 + 1);
        if (q4 == std::string::npos) break;
        std::string nameStr = obj.substr(q3 + 1, q4 - q3 - 1);

        int idx = std::stoi(idxStr);
        m_customNames[idx] = nameStr;
        if (idx >= 0 && idx < (int)m_data.moves.size())
            m_data.moves[idx].displayName = nameStr;

        pos = q4 + 1;
    }
}

void MovesetEditorWindow::SaveEditorDatas()
{
    std::string dir = m_data.folderPath;
    if (!dir.empty() && dir.back() != '\\' && dir.back() != '/')
        dir += '\\';
    dir += ".tkedit";

    CreateDirectoryA(dir.c_str(), nullptr);

    std::string path = dir + "\\editor_datas.json";
    std::ofstream f(path);
    if (!f.is_open()) return;

    f << "{\n  \"custom_names\": {";
    bool first = true;
    for (auto& kv : m_customNames)
    {
        if (!first) f << ",";
        // Escape backslash and quote in name
        std::string escaped;
        for (char c : kv.second)
        {
            if (c == '\\' || c == '"') escaped += '\\';
            escaped += c;
        }
        f << "\n    \"" << kv.first << "\": \"" << escaped << "\"";
        first = false;
    }
    f << "\n  }\n}\n";
}

void MovesetEditorWindow::SaveToFile()
{
    if (!m_dirty || m_saveState != SaveState::Idle) return;
    m_saveState       = SaveState::Saving;
    m_openSavingPopup = true;
    m_doSaveThisFrame = false;
}

void MovesetEditorWindow::RequestClose()
{
    if (m_dirty)
        m_pendingClose = true;
    else
        m_open = false;
}

void MovesetEditorWindow::RenderCloseConfirmModal()
{
    if (m_pendingClose)
    {
        ImGui::OpenPopup("Unsaved Changes##closemodal");
        m_pendingClose = false;
    }

    const ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal("Unsaved Changes##closemodal", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse))
    {
        ImGui::Text("You have unsaved changes.");
        ImGui::Text("Save before closing?");
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (ImGui::Button("Save", ImVec2(90, 0)))
        {
            if (SaveMotbin(m_data)) m_dirty = false;
            m_open = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Don't Save", ImVec2(90, 0)))
        {
            m_open = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(90, 0)))
        {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

// -------------------------------------------------------------
//  Save overlay + popup pair
//  Two-frame sequence:
//    Frame 0: overlay shown, "저장 중..." modal opened (save NOT done yet)
//    Frame 1: inside modal, actual save executed, modal closed
//    Frame 2: "저장 완료" modal opened, overlay gone
//             closes on any left-click (skips first frame to avoid same-click)
// -------------------------------------------------------------

void MovesetEditorWindow::RenderSavePopups()
{
    if (m_saveState == SaveState::Idle) return;

    const ImVec2 wpos   = ImGui::GetWindowPos();
    const ImVec2 wsz    = ImGui::GetWindowSize();
    const ImVec2 center = ImVec2(wpos.x + wsz.x * 0.5f, wpos.y + wsz.y * 0.5f);

    // -- Dim overlay (only while saving, not after) ---------------
    if (m_saveState == SaveState::Saving)
    {
        ImGui::GetWindowDrawList()->AddRectFilled(
            wpos, ImVec2(wpos.x + wsz.x, wpos.y + wsz.y),
            IM_COL32(0, 0, 0, 110));
    }

    // -- Open popup triggers (must be before BeginPopupModal) -----
    if (m_openSavingPopup) { ImGui::OpenPopup("##saving_modal"); m_openSavingPopup = false; }
    if (m_openDonePopup)   { ImGui::OpenPopup("##savedone_modal"); m_openDonePopup = false; }

    // -- "Saving..." modal ----------------------------------------
    ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    constexpr ImGuiWindowFlags kModalFlags =
        ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoTitleBar       |
        ImGuiWindowFlags_NoMove;

    if (ImGui::BeginPopupModal("##saving_modal", nullptr, kModalFlags))
    {
        ImGui::Spacing();
        ImGui::Text("  Saving...  ");
        ImGui::Spacing();

        if (m_doSaveThisFrame)
        {
            // Second frame: actually save
            if (SaveMotbin(m_data)) m_dirty = false;
            m_doSaveThisFrame = false;
            m_saveState       = SaveState::Done;
            m_openDonePopup   = true;
            m_donePoppedFirst = true;
            ImGui::CloseCurrentPopup();
        }
        else
        {
            // First frame: just show text, defer save to next frame
            m_doSaveThisFrame = true;
        }
        ImGui::EndPopup();
    }

    // -- "Saved" modal --------------------------------------------
    ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal("##savedone_modal", nullptr, kModalFlags))
    {
        ImGui::Spacing();
        ImGui::Text("  Saved  ");
        ImGui::Spacing();
        ImGui::TextDisabled("  (click anywhere to close)  ");
        ImGui::Spacing();

        if (m_donePoppedFirst)
        {
            m_donePoppedFirst = false; // skip the frame the popup first opened
        }
        else if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            m_saveState = SaveState::Idle;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

// -------------------------------------------------------------
//  Requirements subwindow  (3-panel: outer groups | inner items | detail)
//  Outer groups: sequences ending with req==1100
// -------------------------------------------------------------

void MovesetEditorWindow::RenderSubWin_Requirements()
{
    if (!m_reqWinOpen) return;
    ImGui::SetNextWindowSize(ImVec2(560.0f, 420.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Requirements##blkwin", &m_reqWinOpen, ImGuiWindowFlags_NoCollapse))
    { ImGui::End(); return; }

    const auto& block = m_data.requirementBlock;
    auto groups = ComputeGroups(block, +[](const ParsedRequirement& r)->bool{ return r.req==GameStatic::Get().data.reqListEnd; });

    float colW = ImGui::GetContentRegionAvail().x / 3.0f - ImGui::GetStyle().ItemSpacing.x;

    // Outer list
    ImGui::BeginChild("##req_outer", ImVec2(colW, 0.0f), true);
    Render2LevelOuterList(groups, m_reqWinSel, "requirement Lists");
    ImGui::EndChild();

    ImGui::SameLine();

    // Inner list
    ImGui::BeginChild("##req_inner", ImVec2(colW, 0.0f), true);
    ImGui::TextDisabled("items");
    ImGui::Separator();
    if (m_reqWinSel.outer < (int)groups.size())
    {
        uint32_t start = groups[m_reqWinSel.outer].first;
        uint32_t count = groups[m_reqWinSel.outer].second;
        for (uint32_t k = 0; k < count; ++k)
        {
            uint32_t idx = start + k;
            if (idx >= (uint32_t)block.size()) break;
            const ParsedRequirement& r = block[idx];
            bool isTerm2 = (r.req == GameStatic::Get().data.reqListEnd);
            char lbl[48];
            if (isTerm2)
                snprintf(lbl, sizeof(lbl), "#%u  [END]##ri%u", k, idx);
            else
                snprintf(lbl, sizeof(lbl), "#%u  req=%u##ri%u", k, r.req, idx);
            if (isTerm2) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f,0.5f,0.5f,1.0f));
            bool sel = (m_reqWinSel.inner == (int)k);
            if (ImGui::Selectable(lbl, sel)) m_reqWinSel.inner = (int)k;
            if (isTerm2) ImGui::PopStyleColor();
        }
    }
    ImGui::EndChild();

    ImGui::SameLine();

    // Detail
    ImGui::BeginChild("##req_detail", ImVec2(0.0f, 0.0f), false);
    if (m_reqWinSel.outer < (int)groups.size())
    {
        uint32_t start = groups[m_reqWinSel.outer].first;
        uint32_t idx = start + (uint32_t)m_reqWinSel.inner;
        if (idx < (uint32_t)block.size())
        {
            ParsedRequirement& r = m_data.requirementBlock[idx];
            ImGui::TextDisabled("requirement #%u  (block[%u])", m_reqWinSel.inner, idx);
            ImGui::Separator();
            if (BeginPropTable("##reqdt"))
            {
                if (RowU32Edit("##req",    ReqLabel::Req,    r.req))    m_dirty = true;
                if (RowU32Edit("##param",  ReqLabel::Param0, r.param))  m_dirty = true;
                if (RowU32Edit("##param2", ReqLabel::Param1, r.param2)) m_dirty = true;
                if (RowU32Edit("##param3", ReqLabel::Param2, r.param3)) m_dirty = true;
                if (RowU32Edit("##param4", ReqLabel::Param3, r.param4)) m_dirty = true;
                ImGui::EndTable();
            }
        }
    }
    ImGui::EndChild();

    ImGui::End();
}

// -------------------------------------------------------------
//  Cancels subwindow  (3-panel cancel + 3-panel group-cancel + extradata column)
// -------------------------------------------------------------

void MovesetEditorWindow::RenderCancelInnerDetail(
    ParsedCancel& c, int localIdx, uint32_t blockIdx,
    const std::vector<std::pair<uint32_t,uint32_t>>& gcGroups)
{
    ImGui::TextDisabled("cancel #%d  (block[%u])", localIdx, blockIdx);
    ImGui::Separator();
    if (!BeginPropTable("##cdt")) return;

    if (RowHex64Edit("##command", CancelLabel::Command, c.command)) m_dirty = true;

    // cancel-extra link (editable)
    {
        static constexpr ImVec4 kGreen = {0.55f, 0.85f, 0.55f, 1.0f};
        static constexpr ImVec4 kPink  = {1.00f, 0.50f, 0.65f, 1.0f};
        bool valid = (c.extradata_idx != 0xFFFFFFFF) &&
                     (c.extradata_idx < (uint32_t)m_data.cancelExtraBlock.size());
        auto r = RowIdxEditLink("##cxl_idx", CancelLabel::Extradata, c.extradata_idx, valid);
        if (r.changed) m_dirty = true;
        if (r.navigate) {
            m_cancelsWin.extradataSel       = (int)c.extradata_idx;
            m_cancelsWin.extraScrollPending = true;
        }
    }

    // requirement link (editable)
    {
        bool valid = (c.req_list_idx != 0xFFFFFFFF) &&
                     (c.req_list_idx < (uint32_t)m_data.requirementBlock.size());
        auto r = RowIdxEditLink("##creq_idx", CancelLabel::Requirements, c.req_list_idx, valid);
        if (r.changed) m_dirty = true;
        if (r.navigate) {
            const auto& blk = m_data.requirementBlock;
            auto grps = ComputeGroups(blk, +[](const ParsedRequirement& rr)->bool{ return rr.req==GameStatic::Get().data.reqListEnd; });
            int gi = FindGroupOuter(grps, c.req_list_idx);
            if (gi >= 0) { m_reqWinSel.outer = gi; m_reqWinSel.inner = 0; m_reqWinSel.scrollOuter = true; }
            m_reqWinOpen = true;
        }
    }

    if (RowU32Edit("##fws", CancelLabel::InputWindowStart, c.frame_window_start)) m_dirty = true;
    if (RowU32Edit("##fwe", CancelLabel::InputWindowEnd,   c.frame_window_end))   m_dirty = true;
    if (RowU32Edit("##sf",  CancelLabel::StartingFrame,    c.starting_frame))     m_dirty = true;

    // move_id (editable + navigable)
    if (c.command == GameStatic::Get().data.groupCancelStart)
    {
        bool valid = (FindGroupOuter(gcGroups, (uint32_t)c.move_id) >= 0);
        auto r = RowMoveEditLink("##cmid_gc", CancelLabel::GroupCancelIdx, c.move_id, valid);
        if (r.changed) m_dirty = true;
        if (r.navigate) {
            int gi = FindGroupOuter(gcGroups, (uint32_t)c.move_id);
            if (gi >= 0) {
                m_cancelsWin.groupCancelSel.outer       = gi;
                m_cancelsWin.groupCancelSel.inner       = 0;
                m_cancelsWin.groupCancelSel.scrollOuter = true;
            }
            m_cancelsWin.open         = true;
            m_cancelsWin.pendingFocus = true;
        }
    }
    else
    {
        const bool isTerm = ((c.command == 0x8000 || c.command == GameStatic::Get().data.groupCancelEnd) && c.move_id == 0x8000);
        if (!isTerm && c.move_id >= 0x8000)
        {
            const uint32_t aliasIdx = c.move_id - 0x8000u;
            int resolvedIdx = -1;
            if (aliasIdx < m_data.originalAliases.size()) {
                const uint16_t resolved = m_data.originalAliases[aliasIdx];
                if ((size_t)resolved < m_data.moves.size()) resolvedIdx = (int)resolved;
            }
            bool valid = (resolvedIdx >= 0);
            auto r = RowMoveEditLink("##cmid_gen", CancelLabel::Move, c.move_id, valid, resolvedIdx, true);
            if (r.changed) m_dirty = true;
            if (r.navigate) { m_selectedIdx = resolvedIdx; m_moveListScrollPending = true; }
        }
        else
        {
            bool valid = !isTerm && (c.move_id < (uint16_t)m_data.moves.size());
            auto r = RowMoveEditLink("##cmid_dir", CancelLabel::Move, c.move_id, valid);
            if (r.changed) m_dirty = true;
            if (r.navigate) { m_selectedIdx = (int)c.move_id; m_moveListScrollPending = true; }
        }
    }

    if (RowU16Edit("##cancel_option", CancelLabel::Option, c.cancel_option)) m_dirty = true;
    ImGui::EndTable();
}

static void RenderCancelSection(
    const char* outerChildId, const char* innerChildId, const char* detailChildId,
    std::vector<ParsedCancel>& block,
    const std::vector<std::pair<uint32_t,uint32_t>>& groups,
    MovesetEditorWindow::TwoLevelSel& sel,
    const char* listLabel,
    float sectionH,
    uint64_t terminatorCmd,
    MovesetEditorWindow* win,
    const std::vector<std::pair<uint32_t,uint32_t>>& gcGroups)
{
    float colW = ImGui::GetContentRegionAvail().x / 3.0f - ImGui::GetStyle().ItemSpacing.x;

    ImGui::BeginChild(outerChildId, ImVec2(colW, sectionH), true);
    Render2LevelOuterList(groups, sel, listLabel);
    ImGui::EndChild();

    ImGui::SameLine();
    ImGui::BeginChild(innerChildId, ImVec2(colW, sectionH), true);
    ImGui::TextDisabled("items");
    ImGui::Separator();
    if (sel.outer < (int)groups.size())
    {
        uint32_t start = groups[sel.outer].first;
        uint32_t count = groups[sel.outer].second;
        for (uint32_t k = 0; k < count; ++k)
        {
            uint32_t idx = start + k;
            if (idx >= (uint32_t)block.size()) break;
            const ParsedCancel& c = block[idx];
            bool isTerm = (c.command == terminatorCmd);
            char lbl[64];
            if (isTerm)
                snprintf(lbl, sizeof(lbl), "#%u  [END]##ci%u", k, idx);
            else if (c.command == GameStatic::Get().data.groupCancelStart)
                snprintf(lbl, sizeof(lbl), "#%u  [GRP_START]##ci%u", k, idx);
            else if (c.command == GameStatic::Get().data.groupCancelEnd)
                snprintf(lbl, sizeof(lbl), "#%u  [GRP_END]##ci%u", k, idx);
            else
                snprintf(lbl, sizeof(lbl), "#%u  ->%u##ci%u", k, c.move_id, idx);
            if (isTerm) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f,0.5f,0.5f,1.0f));
            bool s = (sel.inner == (int)k);
            if (ImGui::Selectable(lbl, s)) sel.inner = (int)k;
            if (isTerm) ImGui::PopStyleColor();
        }
    }
    ImGui::EndChild();

    ImGui::SameLine();
    ImGui::BeginChild(detailChildId, ImVec2(0.0f, sectionH), false);
    if (sel.outer < (int)groups.size())
    {
        uint32_t start = groups[sel.outer].first;
        uint32_t idx = start + (uint32_t)sel.inner;
        if (idx < (uint32_t)block.size())
            win->RenderCancelInnerDetail(block[idx], sel.inner, idx, gcGroups);
    }
    ImGui::EndChild();
}

void MovesetEditorWindow::RenderSubWin_Cancels()
{
    if (!m_cancelsWin.open) return;
    const bool cancelsBringFront = m_cancelsWin.pendingFocus;
    if (cancelsBringFront) m_cancelsWin.pendingFocus = false;
    ImGui::SetNextWindowSize(ImVec2(900.0f, 600.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Cancels##blkwin", &m_cancelsWin.open, ImGuiWindowFlags_NoCollapse))
    { ImGui::End(); return; }
    if (cancelsBringFront) ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindow());

    const uint64_t gcEnd = GameStatic::Get().data.groupCancelEnd;
    auto isCancelTerm      = +[](const ParsedCancel& c)->bool { return c.command == 0x8000; };
    auto isGroupCancelTerm = [gcEnd](const ParsedCancel& c)->bool { return c.command == gcEnd; };
    auto cancelGroups      = ComputeGroups(m_data.cancelBlock,      isCancelTerm);
    auto groupCancelGroups = ComputeGroups(m_data.groupCancelBlock, isGroupCancelTerm);

    const auto& exb = m_data.cancelExtraBlock;
    float rightW   = 160.0f;
    float availW   = ImGui::GetContentRegionAvail().x - rightW - ImGui::GetStyle().ItemSpacing.x;
    float availH   = ImGui::GetContentRegionAvail().y;
    float halfH    = (availH - ImGui::GetStyle().ItemSpacing.y) * 0.5f;

    // Left+Middle: stacked cancel / group-cancel sections
    ImGui::BeginChild("##cancelmid", ImVec2(availW, 0.0f), false);
    {
        // Top: cancel lists
        ImGui::BeginChild("##ctop", ImVec2(0.0f, halfH), false);
        RenderCancelSection("##co","##ci","##cd",
            m_data.cancelBlock, cancelGroups, m_cancelsWin.cancelSel,
            "cancel-lists", 0.0f, 0x8000,
            this, groupCancelGroups);
        ImGui::EndChild();

        // Bottom: group-cancel lists
        ImGui::BeginChild("##cgbot", ImVec2(0.0f, 0.0f), false);
        RenderCancelSection("##gco","##gci","##gcd",
            m_data.groupCancelBlock, groupCancelGroups, m_cancelsWin.groupCancelSel,
            "group-cancel-lists", 0.0f, (uint64_t)GameStatic::Get().data.groupCancelEnd,
            this, {});
        ImGui::EndChild();
    }
    ImGui::EndChild();

    // Right: cancel-extra flat list
    ImGui::SameLine();
    ImGui::BeginChild("##cexcol", ImVec2(rightW, 0.0f), true);
    ImGui::TextDisabled("cancel-extra (%d)", (int)exb.size());
    ImGui::Separator();
    constexpr ImGuiTableFlags kTf =
        ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit;
    if (ImGui::BeginTable("##cextbl", 2, kTf))
    {
        ImGui::TableSetupColumn("Index", ImGuiTableColumnFlags_WidthFixed, 45.0f);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();
        for (int i = 0; i < (int)exb.size(); ++i)
        {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            bool sel = (m_cancelsWin.extradataSel == i);
            char lbl[16]; snprintf(lbl, sizeof(lbl), "%d##cx%d", i, i);
            if (ImGui::Selectable(lbl, sel, ImGuiSelectableFlags_SpanAllColumns))
                m_cancelsWin.extradataSel = i;
            if (sel && m_cancelsWin.extraScrollPending) {
                ImGui::SetScrollHereY(0.5f);
                m_cancelsWin.extraScrollPending = false;
            }
            ImGui::TableSetColumnIndex(1);
            ImGui::SetNextItemWidth(-1.0f);
            char cxid[32]; snprintf(cxid, sizeof(cxid), "##cxv%d", i);
            int cxtmp = (int)m_data.cancelExtraBlock[i];
            if (ImGui::InputInt(cxid, &cxtmp, 0, 0)) { m_data.cancelExtraBlock[i] = (uint32_t)cxtmp; m_dirty = true; }
        }
        ImGui::EndTable();
    }
    ImGui::EndChild();

    ImGui::End();
}

// -------------------------------------------------------------
//  Hit Conditions subwindow  (3-panel)
//  Outer groups: sequences ending with req==1100 terminator
// -------------------------------------------------------------

void MovesetEditorWindow::RenderSubWin_HitConditions()
{
    if (!m_hitCondWinOpen) return;
    const bool hitCondBringFront = m_hitCondWinFocus;
    if (hitCondBringFront) m_hitCondWinFocus = false;
    ImGui::SetNextWindowSize(ImVec2(560.0f, 420.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Hit Conditions##blkwin", &m_hitCondWinOpen, ImGuiWindowFlags_NoCollapse))
    { ImGui::End(); return; }
    if (hitCondBringFront) ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindow());

    const auto& block = m_data.hitConditionBlock;
    const auto& reqBlk = m_data.requirementBlock;

    // Compute groups with proper check using reqBlk context
    std::vector<std::pair<uint32_t,uint32_t>> groups;
    {
        uint32_t start = 0;
        for (uint32_t i = 0; i < (uint32_t)block.size(); ++i)
        {
            const auto& h = block[i];
            bool isTerm = (h.requirement_addr == 0);
            if (!isTerm && h.req_list_idx != 0xFFFFFFFF &&
                h.req_list_idx < (uint32_t)reqBlk.size())
                isTerm = (reqBlk[h.req_list_idx].req == GameStatic::Get().data.reqListEnd);
            if (isTerm)
            {
                groups.push_back({start, i - start + 1});
                start = i + 1;
            }
        }
        if (start < (uint32_t)block.size())
            groups.push_back({start, (uint32_t)block.size() - start});
    }

    float colW = ImGui::GetContentRegionAvail().x / 3.0f - ImGui::GetStyle().ItemSpacing.x;

    // Outer
    ImGui::BeginChild("##hc_outer", ImVec2(colW, 0.0f), true);
    Render2LevelOuterList(groups, m_hitCondWinSel, "hit-condition-lists");
    ImGui::EndChild();

    ImGui::SameLine();

    // Inner
    ImGui::BeginChild("##hc_inner", ImVec2(colW, 0.0f), true);
    ImGui::TextDisabled("items");
    ImGui::Separator();
    if (m_hitCondWinSel.outer < (int)groups.size())
    {
        uint32_t start = groups[m_hitCondWinSel.outer].first;
        uint32_t count = groups[m_hitCondWinSel.outer].second;
        for (uint32_t k = 0; k < count; ++k)
        {
            uint32_t idx = start + k;
            if (idx >= (uint32_t)block.size()) break;
            const ParsedHitCondition& h = block[idx];
            bool isTerm = (h.requirement_addr == 0);
            if (!isTerm && h.req_list_idx != 0xFFFFFFFF &&
                h.req_list_idx < (uint32_t)reqBlk.size())
                isTerm = (reqBlk[h.req_list_idx].req == GameStatic::Get().data.reqListEnd);
            char lbl[48];
            if (isTerm)
                snprintf(lbl, sizeof(lbl), "#%u  [END]##hci%u", k, idx);
            else
                snprintf(lbl, sizeof(lbl), "#%u  dmg=%u##hci%u", k, h.damage, idx);
            if (isTerm) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f,0.5f,0.5f,1.0f));
            bool sel = (m_hitCondWinSel.inner == (int)k);
            if (ImGui::Selectable(lbl, sel)) m_hitCondWinSel.inner = (int)k;
            if (isTerm) ImGui::PopStyleColor();
        }
    }
    ImGui::EndChild();

    ImGui::SameLine();

    // Detail
    ImGui::BeginChild("##hc_detail", ImVec2(0.0f, 0.0f), false);
    if (m_hitCondWinSel.outer < (int)groups.size())
    {
        uint32_t start = groups[m_hitCondWinSel.outer].first;
        uint32_t idx = start + (uint32_t)m_hitCondWinSel.inner;
        if (idx < (uint32_t)block.size())
        {
            ParsedHitCondition& h = m_data.hitConditionBlock[idx];
            ImGui::TextDisabled("hit-condition #%d  (block[%u])", m_hitCondWinSel.inner, idx);
            ImGui::Separator();
            if (BeginPropTable("##hcdt"))
            {
                // requirement link (editable)
                {
                    bool valid = (h.req_list_idx != 0xFFFFFFFF) &&
                                 (h.req_list_idx < (uint32_t)m_data.requirementBlock.size());
                    auto r = RowIdxEditLink("##hcreq_idx", HitCondLabel::Requirements, h.req_list_idx, valid);
                    if (r.changed) m_dirty = true;
                    if (r.navigate) {
                        const auto& blk = m_data.requirementBlock;
                        auto grps = ComputeGroups(blk, +[](const ParsedRequirement& rr)->bool{ return rr.req==GameStatic::Get().data.reqListEnd; });
                        int gi = FindGroupOuter(grps, h.req_list_idx);
                        if (gi >= 0) { m_reqWinSel.outer = gi; m_reqWinSel.inner = 0; m_reqWinSel.scrollOuter = true; }
                        m_reqWinOpen = true;
                    }
                }
                if (RowU32Edit("##hc_damage", HitCondLabel::Damage, h.damage)) m_dirty = true;
                if (RowU32Edit("##hc_0x0C",  HitCondLabel::F0x0C,  h._0x0C))  m_dirty = true;
                // reaction_list link (editable)
                {
                    bool valid = (h.reaction_list_idx != 0xFFFFFFFF) &&
                                 (h.reaction_list_idx < (uint32_t)m_data.reactionListBlock.size());
                    auto r = RowIdxEditLink("##hcrl_idx", HitCondLabel::ReactionList, h.reaction_list_idx, valid);
                    if (r.changed) m_dirty = true;
                    if (r.navigate) {
                        m_reacWin.open          = true;
                        m_reacWin.selectedIdx   = (int)h.reaction_list_idx;
                        m_reacWin.scrollPending = true;
                    }
                }
                ImGui::EndTable();
            }
        }
    }
    ImGui::EndChild();

    ImGui::End();
}

// -------------------------------------------------------------
//  ReactionLists subwindow  (master + 3-column detail)
//  (unchanged from before -- no 2-level needed, each entry is standalone)
// -------------------------------------------------------------

void MovesetEditorWindow::RenderSubWin_ReactionLists()
{
    if (!m_reacWin.open) return;
    ImGui::SetNextWindowSize(ImVec2(700.0f, 480.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Reaction Lists##blkwin", &m_reacWin.open, ImGuiWindowFlags_NoCollapse))
    { ImGui::End(); return; }

    const auto& block = m_data.reactionListBlock;
    const int total = (int)block.size();

    ImGui::BeginChild("##rl_master", ImVec2(110.0f, 0.0f), true);
    ImGui::TextDisabled("%d entries", total);
    ImGui::Separator();
    for (int i = 0; i < total; ++i)
    {
        char lbl[24]; snprintf(lbl, sizeof(lbl), "#%d", i);
        bool sel = (m_reacWin.selectedIdx == i);
        if (ImGui::Selectable(lbl, sel)) m_reacWin.selectedIdx = i;
        if (sel && m_reacWin.scrollPending) {
            ImGui::SetScrollHereY(0.5f);
            m_reacWin.scrollPending = false;
        }
    }
    ImGui::EndChild();

    ImGui::SameLine();
    ImGui::BeginChild("##rl_detail", ImVec2(0.0f, 0.0f), false);
    if (m_reacWin.selectedIdx >= 0 && m_reacWin.selectedIdx < total)
    {
        ParsedReactionList& rlm = m_data.reactionListBlock[m_reacWin.selectedIdx];
        ImGui::TextDisabled("ReactionList #%d", m_reacWin.selectedIdx);
        ImGui::Separator();

        float thirdW = ImGui::GetContentRegionAvail().x / 3.0f - ImGui::GetStyle().ItemSpacing.x;

        ImGui::BeginChild("##rl_mv", ImVec2(thirdW, 0.0f), true);
        ImGui::TextDisabled("reaction moves"); ImGui::Separator();
        if (BeginPropTable("##rlm")) {
            auto RlMoveEdit = [&](const char* inputId, const char* lbl, uint16_t& v) {
                bool gen = (v >= 0x8000);
                int res = -1;
                if (gen) { uint32_t ai = v-0x8000u; if(ai<m_data.originalAliases.size()){uint16_t rv=m_data.originalAliases[ai]; if((size_t)rv<m_data.moves.size()) res=(int)rv;} }
                bool valid = gen ? (res>=0) : (v < (uint16_t)m_data.moves.size());
                auto r = RowMoveEditLink(inputId, lbl, v, valid, res, gen);
                if (r.changed) m_dirty = true;
                if (r.navigate) { m_selectedIdx = gen ? res : (int)v; m_moveListScrollPending = true; }
            };
            RlMoveEdit("##rl_standing",         ReactionLabel::Standing,        rlm.standing);
            RlMoveEdit("##rl_ch",               ReactionLabel::Ch,              rlm.ch);
            RlMoveEdit("##rl_crouch",           ReactionLabel::Crouch,          rlm.crouch);
            RlMoveEdit("##rl_crouch_ch",        ReactionLabel::CrouchCh,        rlm.crouch_ch);
            RlMoveEdit("##rl_left_side",        ReactionLabel::LeftSide,        rlm.left_side);
            RlMoveEdit("##rl_left_side_crouch", ReactionLabel::LeftSideCrouch,  rlm.left_side_crouch);
            RlMoveEdit("##rl_right_side",       ReactionLabel::RightSide,       rlm.right_side);
            RlMoveEdit("##rl_right_side_crouch",ReactionLabel::RightSideCrouch, rlm.right_side_crouch);
            RlMoveEdit("##rl_back",             ReactionLabel::Back,            rlm.back);
            RlMoveEdit("##rl_back_crouch",      ReactionLabel::BackCrouch,   rlm.back_crouch);
            RlMoveEdit("##rl_block",            ReactionLabel::Block,        rlm.block);
            RlMoveEdit("##rl_crouch_block",     ReactionLabel::CrouchBlock,  rlm.crouch_block);
            RlMoveEdit("##rl_wallslump",        ReactionLabel::Wallslump,    rlm.wallslump);
            RlMoveEdit("##rl_downed",           ReactionLabel::Downed,       rlm.downed);
            ImGui::EndTable();
        }
        ImGui::EndChild(); ImGui::SameLine();

        static const char* kPbN[7] = {
            ReactionLabel::FrontPushback,
            ReactionLabel::BackturnedPushback,
            ReactionLabel::LeftSidePushback,
            ReactionLabel::RightSidePushback,
            ReactionLabel::FrontCounterhitPushback,
            ReactionLabel::DownedPushback,
            ReactionLabel::BlockPushback
        };
        ImGui::BeginChild("##rl_pb", ImVec2(thirdW, 0.0f), true);
        ImGui::TextDisabled("Pushbacks"); ImGui::Separator();
        if (BeginPropTable("##rlp")) {
            for (int p = 0; p < 7; ++p) {
                bool valid = (rlm.pushback_idx[p] != 0xFFFFFFFF) && (rlm.pushback_idx[p] < (uint32_t)m_data.pushbackBlock.size());
                char inputId[32]; snprintf(inputId, sizeof(inputId), "##rl_pb%d", p);
                auto r = RowIdxEditLink(inputId, kPbN[p], rlm.pushback_idx[p], valid);
                if (r.changed) m_dirty = true;
                if (r.navigate) { m_pushbackWin.open = true; m_pushbackWin.pushbackSel = (int)rlm.pushback_idx[p]; m_pushbackWin.pbScrollPending = true; }
            }
            ImGui::EndTable();
        }
        ImGui::EndChild(); ImGui::SameLine();

        ImGui::BeginChild("##rl_oth", ImVec2(0.0f, 0.0f), true);
        ImGui::TextDisabled("others"); ImGui::Separator();
        {
            if (BeginPropTable("##rlo")) {
                if (RowU16Edit("##rl_fd",  ReactionLabel::FrontDirection,           rlm.front_direction))      m_dirty = true;
                if (RowU16Edit("##rl_bd",  ReactionLabel::BackDirection,            rlm.back_direction))       m_dirty = true;
                if (RowU16Edit("##rl_lsd", ReactionLabel::LeftSideDirection,        rlm.left_side_direction))  m_dirty = true;
                if (RowU16Edit("##rl_rsd", ReactionLabel::RightSideDirection,       rlm.right_side_direction)) m_dirty = true;
                if (RowU16Edit("##rl_fcd", ReactionLabel::FrontCounterhitDirection, rlm.front_ch_direction))   m_dirty = true;
                if (RowU16Edit("##rl_dd",  ReactionLabel::DownedDirection,          rlm.downed_direction))     m_dirty = true;
                if (RowU16Edit("##rl_fr",  ReactionLabel::FrontRotation,            rlm.front_rotation))       m_dirty = true;
                if (RowU16Edit("##rl_br",  ReactionLabel::BackRotation,             rlm.back_rotation))        m_dirty = true;
                if (RowU16Edit("##rl_lsr", ReactionLabel::LeftSideRotation,         rlm.left_side_rotation))   m_dirty = true;
                if (RowU16Edit("##rl_rsr", ReactionLabel::RightSideRotation,        rlm.right_side_rotation))  m_dirty = true;
                if (RowU16Edit("##rl_vp",  ReactionLabel::VerticalPushback,         rlm.vertical_pushback))    m_dirty = true;
                if (RowU16Edit("##rl_dr",  ReactionLabel::DownedRotation,           rlm.downed_rotation))      m_dirty = true;
                ImGui::EndTable();
            }
        }
        ImGui::EndChild();
    }
    ImGui::EndChild();
    ImGui::End();
}

// -------------------------------------------------------------
//  Pushbacks subwindow  (master list + detail + pushback-extra column)
//  (no 2-level needed -- each pushback entry is standalone)
// -------------------------------------------------------------

void MovesetEditorWindow::RenderSubWin_Pushbacks()
{
    if (!m_pushbackWin.open) return;
    ImGui::SetNextWindowSize(ImVec2(580.0f, 420.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Pushbacks##blkwin", &m_pushbackWin.open, ImGuiWindowFlags_NoCollapse))
    { ImGui::End(); return; }

    const auto& pb = m_data.pushbackBlock;
    const auto& pe = m_data.pushbackExtraBlock;
    float rightW   = 160.0f;
    float leftW    = ImGui::GetContentRegionAvail().x - rightW - ImGui::GetStyle().ItemSpacing.x;

    ImGui::BeginChild("##pb_lm", ImVec2(leftW, 0.0f), false);
    {
        ImGui::BeginChild("##pblist", ImVec2(120.0f, 0.0f), true);
        ImGui::TextDisabled("pushbacks (%d)", (int)pb.size()); ImGui::Separator();
        for (int i = 0; i < (int)pb.size(); ++i) {
            char lbl[24]; snprintf(lbl, sizeof(lbl), "#%d", i);
            bool sel = (m_pushbackWin.pushbackSel == i);
            if (ImGui::Selectable(lbl, sel)) m_pushbackWin.pushbackSel = i;
            if (sel && m_pushbackWin.pbScrollPending) {
                ImGui::SetScrollHereY(0.5f);
                m_pushbackWin.pbScrollPending = false;
            }
        }
        ImGui::EndChild();

        ImGui::SameLine();
        ImGui::BeginChild("##pbdetail", ImVec2(0.0f, 0.0f), false);
        if (m_pushbackWin.pushbackSel >= 0 && m_pushbackWin.pushbackSel < (int)pb.size()) {
            ParsedPushback& p = m_data.pushbackBlock[m_pushbackWin.pushbackSel];
            ImGui::TextDisabled("Pushback #%d", m_pushbackWin.pushbackSel); ImGui::Separator();
            if (BeginPropTable("##pbdt")) {
                if (RowU16Edit("##pb_val1", PushbackLabel::NonLinearDisplacement, p.val1)) m_dirty = true;
                if (RowU16Edit("##pb_val2", PushbackLabel::NonLinearDistance,     p.val2)) m_dirty = true;
                if (RowU32Edit("##pb_val3", PushbackLabel::NumOfExtraPushbacks,   p.val3)) m_dirty = true;
                {
                    bool valid = (p.pushback_extra_idx != 0xFFFFFFFF) && (p.pushback_extra_idx < (uint32_t)pe.size());
                    auto r = RowIdxEditLink("##pb_extra_idx", PushbackLabel::PushbackExtradata, p.pushback_extra_idx, valid);
                    if (r.changed) m_dirty = true;
                    if (r.navigate) { m_pushbackWin.extraSel = (int)p.pushback_extra_idx; m_pushbackWin.extraScrollPending = true; }
                }
                ImGui::EndTable();
            }
        }
        ImGui::EndChild();
    }
    ImGui::EndChild();

    ImGui::SameLine();
    ImGui::BeginChild("##pecol", ImVec2(rightW, 0.0f), true);
    ImGui::TextDisabled("pushback-extra (%d)", (int)pe.size()); ImGui::Separator();
    constexpr ImGuiTableFlags kTf = ImGuiTableFlags_BordersInnerH|ImGuiTableFlags_RowBg|ImGuiTableFlags_SizingFixedFit;
    if (ImGui::BeginTable("##petbl", 2, kTf)) {
        ImGui::TableSetupColumn("Index",        ImGuiTableColumnFlags_WidthFixed, 45.0f);
        ImGui::TableSetupColumn(PushbackExtraLabel::Displacement, ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();
        for (int i = 0; i < (int)pe.size(); ++i) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            bool sel = (m_pushbackWin.extraSel == i);
            char lbl[16]; snprintf(lbl, sizeof(lbl), "%d##pe%d", i, i);
            if (ImGui::Selectable(lbl, sel, ImGuiSelectableFlags_SpanAllColumns)) m_pushbackWin.extraSel = i;
            if (sel && m_pushbackWin.extraScrollPending) {
                ImGui::SetScrollHereY(0.5f);
                m_pushbackWin.extraScrollPending = false;
            }
            ImGui::TableSetColumnIndex(1);
            ImGui::SetNextItemWidth(-1.0f);
            char peid[32]; snprintf(peid, sizeof(peid), "##pev%d", i);
            int petmp = (int)static_cast<int16_t>(m_data.pushbackExtraBlock[i].value);
            if (ImGui::InputInt(peid, &petmp, 0, 0)) { m_data.pushbackExtraBlock[i].value = static_cast<uint16_t>(static_cast<int16_t>(petmp)); m_dirty = true; }
        }
        ImGui::EndTable();
    }
    ImGui::EndChild();
    ImGui::End();
}

// -------------------------------------------------------------
//  Voiceclips subwindow
// -------------------------------------------------------------

void MovesetEditorWindow::RenderSubWin_Voiceclips()
{
    if (!m_voiceclipWinOpen) return;
    const bool voiceclipBringFront = m_voiceclipWinFocus;
    if (voiceclipBringFront) m_voiceclipWinFocus = false;
    ImGui::SetNextWindowSize(ImVec2(380.0f, 360.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Voiceclips##blkwin", &m_voiceclipWinOpen, ImGuiWindowFlags_NoCollapse))
    { ImGui::End(); return; }
    if (voiceclipBringFront) ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindow());

    const auto& block = m_data.voiceclipBlock;
    const int total = (int)block.size();

    ImGui::BeginChild("##vc_master", ImVec2(130.0f, 0.0f), true);
    ImGui::TextDisabled("%d entries", total); ImGui::Separator();
    for (int i = 0; i < total; ++i) {
        const ParsedVoiceclip& vc = block[i];
        const bool isEnd = (vc.val1 == 0xFFFFFFFFu && vc.val2 == 0xFFFFFFFFu && vc.val3 == 0xFFFFFFFFu);
        char lbl[32];
        if (isEnd) snprintf(lbl, sizeof(lbl), "#%d  [END]##vci%d", i, i);
        else       snprintf(lbl, sizeof(lbl), "#%d##vci%d", i, i);
        bool sel = (m_voiceclipWinSel == i);
        if (isEnd) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f,0.5f,0.5f,1.0f));
        if (ImGui::Selectable(lbl, sel)) m_voiceclipWinSel = i;
        if (isEnd) ImGui::PopStyleColor();
        if (sel && m_voiceclipWinScroll) { ImGui::SetScrollHereY(0.5f); m_voiceclipWinScroll = false; }
    }
    ImGui::EndChild();

    ImGui::SameLine();
    ImGui::BeginChild("##vc_detail", ImVec2(0.0f, 0.0f), false);
    if (m_voiceclipWinSel >= 0 && m_voiceclipWinSel < total) {
        ParsedVoiceclip& vc = m_data.voiceclipBlock[m_voiceclipWinSel];
        const bool isEnd = (vc.val1 == 0xFFFFFFFFu && vc.val2 == 0xFFFFFFFFu && vc.val3 == 0xFFFFFFFFu);
        ImGui::TextDisabled("Voiceclip #%d%s", m_voiceclipWinSel, isEnd ? "  [END]" : ""); ImGui::Separator();
        if (BeginPropTable("##vcdt")) {
            // tk_voiceclip fields are 'int' (signed) -- display as signed int32
            { int32_t tmp = static_cast<int32_t>(vc.val1);
              if (RowI32Edit("##vc_val1", VoiceclipLabel::Folder, tmp)) { vc.val1 = static_cast<uint32_t>(tmp); m_dirty = true; } }
            { int32_t tmp = static_cast<int32_t>(vc.val2);
              if (RowI32Edit("##vc_val2", VoiceclipLabel::Val2, tmp)) { vc.val2 = static_cast<uint32_t>(tmp); m_dirty = true; } }
            { int32_t tmp = static_cast<int32_t>(vc.val3);
              if (RowI32Edit("##vc_val3", VoiceclipLabel::Clip, tmp)) { vc.val3 = static_cast<uint32_t>(tmp); m_dirty = true; } }
            ImGui::EndTable();
        }
    }
    ImGui::EndChild();
    ImGui::End();
}

// -------------------------------------------------------------
//  Properties subwindow  (3-panel for each of extraProp/startProp/endProp)
//  extraProp (tk_extraprops): terminator = type==0 && id==0
//  start/endProp (tk_fl_extraprops): terminator = id==reqListEnd (1100)
// -------------------------------------------------------------

static void RenderPropSection(
    const char* outerChildId, const char* innerChildId, const char* detailChildId,
    std::vector<ParsedExtraProp>& block,
    const std::vector<ParsedRequirement>& reqBlk,
    const std::vector<std::pair<uint32_t,uint32_t>>& groups,
    MovesetEditorWindow::TwoLevelSel& sel,
    const char* listLabel,
    bool isExtraProp,
    MovesetEditorWindow::TwoLevelSel& reqWinSel,
    bool& reqWinOpen,
    bool& dirty)
{
    float colW = ImGui::GetContentRegionAvail().x / 3.0f - ImGui::GetStyle().ItemSpacing.x;

    ImGui::BeginChild(outerChildId, ImVec2(colW, 0.0f), true);
    Render2LevelOuterList(groups, sel, listLabel);
    ImGui::EndChild();

    ImGui::SameLine();
    ImGui::BeginChild(innerChildId, ImVec2(colW, 0.0f), true);
    ImGui::TextDisabled("items"); ImGui::Separator();
    if (sel.outer < (int)groups.size())
    {
        uint32_t start = groups[sel.outer].first;
        uint32_t count = groups[sel.outer].second;
        for (uint32_t k = 0; k < count; ++k)
        {
            uint32_t idx = start + k;
            if (idx >= (uint32_t)block.size()) break;
            const ParsedExtraProp& e = block[idx];
            bool isTerm = isExtraProp
                ? (e.type == 0 && e.id == 0)
                : (e.id == GameStatic::Get().data.reqListEnd);
            char lbl[48];
            if (isTerm)
                snprintf(lbl, sizeof(lbl), "#%u  [END]##pi%u", k, idx);
            else
                snprintf(lbl, sizeof(lbl), "#%u  id=0x%04X##pi%u", k, e.id, idx);
            if (isTerm) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f,0.5f,0.5f,1.0f));
            bool s = (sel.inner == (int)k);
            if (ImGui::Selectable(lbl, s)) sel.inner = (int)k;
            if (isTerm) ImGui::PopStyleColor();
        }
    }
    ImGui::EndChild();

    ImGui::SameLine();
    ImGui::BeginChild(detailChildId, ImVec2(0.0f, 0.0f), false);
    if (sel.outer < (int)groups.size())
    {
        uint32_t start = groups[sel.outer].first;
        uint32_t idx = start + (uint32_t)sel.inner;
        if (idx < (uint32_t)block.size())
        {
            ParsedExtraProp& e = block[idx];
            ImGui::TextDisabled("prop #%d  (block[%u])", sel.inner, idx);
            ImGui::Separator();
            if (BeginPropTable("##prdt"))
            {
                if (isExtraProp) {
                    if (RowU32Edit  ("##ep_type", ExtraPropLabel::Frame, e.type)) dirty = true;
                    if (RowHex32Edit("##ep_0x4",  ExtraPropLabel::F0x4,  e._0x4)) dirty = true;
                }
                if (RowHex32Edit("##ep_id", ExtraPropLabel::Property, e.id)) dirty = true;
                // requirement link (editable)
                {
                    bool valid = (e.req_list_idx != 0xFFFFFFFF) && (e.req_list_idx < (uint32_t)reqBlk.size());
                    auto r = RowIdxEditLink("##prop_req_idx", ExtraPropLabel::Requirements, e.req_list_idx, valid);
                    if (r.changed) dirty = true;
                    if (r.navigate) {
                        auto grps = ComputeGroups(reqBlk, +[](const ParsedRequirement& rr)->bool{ return rr.req==GameStatic::Get().data.reqListEnd; });
                        int gi = FindGroupOuter(grps, e.req_list_idx);
                        if (gi >= 0) { reqWinSel.outer = gi; reqWinSel.inner = 0; reqWinSel.scrollOuter = true; }
                        reqWinOpen = true;
                    }
                }
                if (RowHex32Edit("##ep_v1", ExtraPropLabel::Param0, e.value))  dirty = true;
                if (RowHex32Edit("##ep_v2", ExtraPropLabel::Param1, e.value2)) dirty = true;
                if (RowHex32Edit("##ep_v3", ExtraPropLabel::Param2, e.value3)) dirty = true;
                if (RowHex32Edit("##ep_v4", ExtraPropLabel::Param3, e.value4)) dirty = true;
                if (RowHex32Edit("##ep_v5", ExtraPropLabel::Param4, e.value5)) dirty = true;
                ImGui::EndTable();
            }
        }
    }
    ImGui::EndChild();
}

static std::vector<std::pair<uint32_t,uint32_t>>
ComputeOtherPropGroups(const std::vector<ParsedExtraProp>& block,
                       const std::vector<ParsedRequirement>&)
{
    std::vector<std::pair<uint32_t,uint32_t>> groups;
    uint32_t start = 0;
    for (uint32_t i = 0; i < (uint32_t)block.size(); ++i)
    {
        if (block[i].id == GameStatic::Get().data.reqListEnd)  // id == reqListEnd (1100) is the terminator for fl_extraprops (start/end props)
        {
            groups.push_back({start, i - start + 1});
            start = i + 1;
        }
    }
    if (start < (uint32_t)block.size())
        groups.push_back({start, (uint32_t)block.size() - start});
    return groups;
}

void MovesetEditorWindow::RenderSubWin_Properties()
{
    if (!m_propertiesWin.open) return;
    const bool propBringFront = m_propertiesWin.pendingFocus;
    if (propBringFront) m_propertiesWin.pendingFocus = false;
    ImGui::SetNextWindowSize(ImVec2(620.0f, 460.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Properties##blkwin", &m_propertiesWin.open, ImGuiWindowFlags_NoCollapse))
    { ImGui::End(); return; }
    if (propBringFront) ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindow());

    const auto& reqBlk = m_data.requirementBlock;
    auto epGroups = ComputeGroups(m_data.extraPropBlock,
        +[](const ParsedExtraProp& e)->bool{ return e.type == 0 && e.id == 0; });
    auto spGroups = ComputeOtherPropGroups(m_data.startPropBlock, reqBlk);
    auto npGroups = ComputeOtherPropGroups(m_data.endPropBlock,   reqBlk);

    if (!ImGui::BeginTabBar("##prop_tabs")) { ImGui::End(); return; }

    // Consume pendingTab once so the flag fires for exactly one frame
    const int pendingTab = m_propertiesWin.pendingTab;
    m_propertiesWin.pendingTab = -1;

    if (ImGui::BeginTabItem("Extra", nullptr,
            pendingTab == 0 ? ImGuiTabItemFlags_SetSelected : 0))
    {
        RenderPropSection("##epo","##epi","##epd",
            m_data.extraPropBlock, reqBlk, epGroups, m_propertiesWin.epSel,
            "property-lists", true, m_reqWinSel, m_reqWinOpen, m_dirty);
        ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("Start", nullptr,
            pendingTab == 1 ? ImGuiTabItemFlags_SetSelected : 0))
    {
        RenderPropSection("##spo","##spi","##spd",
            m_data.startPropBlock, reqBlk, spGroups, m_propertiesWin.spSel,
            "start-property-lists", false, m_reqWinSel, m_reqWinOpen, m_dirty);
        ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("End", nullptr,
            pendingTab == 2 ? ImGuiTabItemFlags_SetSelected : 0))
    {
        RenderPropSection("##npo","##npi","##npd",
            m_data.endPropBlock, reqBlk, npGroups, m_propertiesWin.npSel,
            "end-property-lists", false, m_reqWinSel, m_reqWinOpen, m_dirty);
        ImGui::EndTabItem();
    }

    ImGui::EndTabBar();
    ImGui::End();
}

// -------------------------------------------------------------
//  Throws subwindow
//  Left panel  : throw list + detail  (side, throwextra link)
//  Right panel : throw_extra list + detail  (5 fields)
// -------------------------------------------------------------

void MovesetEditorWindow::RenderSubWin_Throws()
{
    if (!m_throwsWin.open) return;
    ImGui::SetNextWindowSize(ImVec2(760.0f, 420.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Throws##blkwin", &m_throwsWin.open, ImGuiWindowFlags_NoCollapse))
    { ImGui::End(); return; }

    const auto& th  = m_data.throwBlock;
    const auto& te  = m_data.throwExtraBlock;

    auto isTeEnd = +[](const ParsedThrowExtra& t) -> bool {
        return t.pick_probability == 0 && t.camera_type == 0 &&
               t.left_side_camera_data == 0 && t.right_side_camera_data == 0 &&
               t.additional_rotation == 0;
    };
    auto teGroups = ComputeGroups(te, isTeEnd);

    const float spacing = ImGui::GetStyle().ItemSpacing.x;
    const float totalW  = ImGui::GetContentRegionAvail().x;
    const float leftW   = totalW * 0.25f;
    const float rightW  = totalW - leftW - spacing;
    const float listH   = 160.0f;

    // ---- Left: throw list + detail ----
    ImGui::BeginChild("##th_left", ImVec2(leftW, 0.0f), false);
    {
        ImGui::BeginChild("##thlist", ImVec2(0.0f, listH), true);
        ImGui::TextDisabled("throws (%d)", (int)th.size()); ImGui::Separator();
        for (int i = 0; i < (int)th.size(); ++i)
        {
            char lbl[48]; snprintf(lbl, sizeof(lbl), "#%d  0x%llX", i, (unsigned long long)th[i].side);
            bool sel = (m_throwsWin.throwSel == i);
            if (ImGui::Selectable(lbl, sel)) m_throwsWin.throwSel = i;
            if (sel && m_throwsWin.throwScrollPending)
            {
                ImGui::SetScrollHereY(0.5f);
                m_throwsWin.throwScrollPending = false;
            }
        }
        ImGui::EndChild();

        if (m_throwsWin.throwSel >= 0 && m_throwsWin.throwSel < (int)th.size())
        {
            ParsedThrow& t = m_data.throwBlock[m_throwsWin.throwSel];
            ImGui::TextDisabled("throw #%d", m_throwsWin.throwSel); ImGui::Separator();
            if (BeginPropTable("##thdt"))
            {
                if (RowHex64Edit("##th_side", ThrowLabel::Side, t.side)) m_dirty = true;
                {
                    bool valid = (t.throwextra_idx != 0xFFFFFFFF) &&
                                 (t.throwextra_idx < (uint32_t)te.size());
                    auto r = RowIdxEditLink("##th_extra_idx", ThrowLabel::ThrowExtra, t.throwextra_idx, valid);
                    if (r.changed) m_dirty = true;
                    if (r.navigate)
                    {
                        // navigate to the group that contains throwextra_idx
                        for (int gi = 0; gi < (int)teGroups.size(); ++gi)
                        {
                            uint32_t gs = teGroups[gi].first;
                            uint32_t gc = teGroups[gi].second;
                            if (t.throwextra_idx >= gs && t.throwextra_idx < gs + gc)
                            {
                                m_throwsWin.extraSel.outer       = gi;
                                m_throwsWin.extraSel.inner       = (int)(t.throwextra_idx - gs);
                                m_throwsWin.extraSel.scrollOuter = true;
                                break;
                            }
                        }
                    }
                }
                ImGui::EndTable();
            }
        }
    }
    ImGui::EndChild();

    ImGui::SameLine();

    // ---- Right: throw_extra 3-panel (outer groups | inner items | detail) ----
    ImGui::BeginChild("##te_right", ImVec2(rightW, 0.0f), false);
    {
        float colW = ImGui::GetContentRegionAvail().x / 3.0f - spacing;

        // Outer groups
        ImGui::BeginChild("##te_outer", ImVec2(colW, 0.0f), true);
        Render2LevelOuterList(teGroups, m_throwsWin.extraSel, "throw_extra lists");
        ImGui::EndChild();

        ImGui::SameLine();

        // Inner items
        ImGui::BeginChild("##te_inner", ImVec2(colW, 0.0f), true);
        ImGui::TextDisabled("items"); ImGui::Separator();
        if (m_throwsWin.extraSel.outer < (int)teGroups.size())
        {
            uint32_t start = teGroups[m_throwsWin.extraSel.outer].first;
            uint32_t count = teGroups[m_throwsWin.extraSel.outer].second;
            for (uint32_t k = 0; k < count; ++k)
            {
                uint32_t idx = start + k;
                if (idx >= (uint32_t)te.size()) break;
                const bool isEnd = isTeEnd(te[idx]);
                char lbl[32];
                if (isEnd) snprintf(lbl, sizeof(lbl), "#%u  [END]##tei%u", k, idx);
                else        snprintf(lbl, sizeof(lbl), "#%u##tei%u", k, idx);
                bool sel = (m_throwsWin.extraSel.inner == (int)k);
                if (isEnd) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
                if (ImGui::Selectable(lbl, sel)) m_throwsWin.extraSel.inner = (int)k;
                if (isEnd) ImGui::PopStyleColor();
            }
        }
        ImGui::EndChild();

        ImGui::SameLine();

        // Detail
        ImGui::BeginChild("##te_detail", ImVec2(0.0f, 0.0f), false);
        if (m_throwsWin.extraSel.outer < (int)teGroups.size())
        {
            uint32_t start = teGroups[m_throwsWin.extraSel.outer].first;
            uint32_t idx   = start + (uint32_t)m_throwsWin.extraSel.inner;
            if (idx < (uint32_t)te.size())
            {
                ParsedThrowExtra& tex = m_data.throwExtraBlock[idx];
                const bool isEnd = isTeEnd(tex);
                ImGui::TextDisabled("throw_extra #%u  (block[%u])%s",
                    m_throwsWin.extraSel.inner, idx, isEnd ? "  [END]" : "");
                ImGui::Separator();
                if (BeginPropTable("##tedt"))
                {
                    if (RowU32Edit("##te_prob", ThrowExtraLabel::PickProbability,     tex.pick_probability))       m_dirty = true;
                    if (RowU16Edit("##te_cam",  ThrowExtraLabel::CameraType,          tex.camera_type))            m_dirty = true;
                    if (RowU16Edit("##te_lsc",  ThrowExtraLabel::LeftSideCameraData,  tex.left_side_camera_data))  m_dirty = true;
                    if (RowU16Edit("##te_rsc",  ThrowExtraLabel::RightSideCameraData, tex.right_side_camera_data)) m_dirty = true;
                    if (RowU16Edit("##te_rot",  ThrowExtraLabel::AdditionalRotation,  tex.additional_rotation))    m_dirty = true;
                    ImGui::EndTable();
                }
            }
        }
        ImGui::EndChild();
    }
    ImGui::EndChild();

    ImGui::End();
}

// -------------------------------------------------------------
//  Projectiles subwindow
//  Left panel  : projectile list
//  Right panel : detail (hit_condition link, cancel link, u1[35], u2[16])
// -------------------------------------------------------------

void MovesetEditorWindow::RenderSubWin_Projectiles()
{
    if (!m_projectileWin.open) return;
    ImGui::SetNextWindowSize(ImVec2(540.0f, 480.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Projectiles##blkwin", &m_projectileWin.open, ImGuiWindowFlags_NoCollapse))
    { ImGui::End(); return; }

    const auto& block = m_data.projectileBlock;
    const float leftW = 110.0f;

    ImGui::BeginChild("##pjlist", ImVec2(leftW, 0.0f), true);
    ImGui::TextDisabled("projectiles (%d)", (int)block.size()); ImGui::Separator();
    for (int i = 0; i < (int)block.size(); ++i)
    {
        char lbl[20]; snprintf(lbl, sizeof(lbl), "#%d", i);
        bool sel = (m_projectileWin.selectedIdx == i);
        if (ImGui::Selectable(lbl, sel)) m_projectileWin.selectedIdx = i;
        if (sel && m_projectileWin.scrollPending)
        {
            ImGui::SetScrollHereY(0.5f);
            m_projectileWin.scrollPending = false;
        }
    }
    ImGui::EndChild();

    ImGui::SameLine();

    ImGui::BeginChild("##pjdetail", ImVec2(0.0f, 0.0f), false);
    if (m_projectileWin.selectedIdx >= 0 && m_projectileWin.selectedIdx < (int)block.size())
    {
        ParsedProjectile& p = m_data.projectileBlock[m_projectileWin.selectedIdx];
        ImGui::TextDisabled("projectile #%d", m_projectileWin.selectedIdx); ImGui::Separator();

        if (BeginPropTable("##pjdt"))
        {
            // hit_condition link
            {
                bool valid = (p.hit_condition_idx != 0xFFFFFFFF) &&
                             (p.hit_condition_idx < (uint32_t)m_data.hitConditionBlock.size());
                auto r = RowIdxEditLink("##pj_hc_idx", ProjectileLabel::HitCondition, p.hit_condition_idx, valid);
                if (r.changed) m_dirty = true;
                if (r.navigate)
                {
                    auto grps = ComputeGroups(m_data.hitConditionBlock,
                        +[](const ParsedHitCondition& h)->bool{ return h.requirement_addr == 0; });
                    int gi = FindGroupOuter(grps, p.hit_condition_idx);
                    if (gi >= 0) { m_hitCondWinSel.outer = gi; m_hitCondWinSel.inner = 0; m_hitCondWinSel.scrollOuter = true; }
                    m_hitCondWinOpen  = true;
                    m_hitCondWinFocus = true;
                }
            }
            // cancel link
            {
                bool valid = (p.cancel_idx != 0xFFFFFFFF) &&
                             (p.cancel_idx < (uint32_t)m_data.cancelBlock.size());
                auto r = RowIdxEditLink("##pj_c_idx", ProjectileLabel::Cancel, p.cancel_idx, valid);
                if (r.changed) m_dirty = true;
                if (r.navigate)
                {
                    auto grps = ComputeGroups(m_data.cancelBlock,
                        +[](const ParsedCancel& c)->bool{ return c.command == 0x8000; });
                    int gi = FindGroupOuter(grps, p.cancel_idx);
                    if (gi >= 0) { m_cancelsWin.cancelSel.outer = gi; m_cancelsWin.cancelSel.inner = 0; m_cancelsWin.cancelSel.scrollOuter = true; }
                    m_cancelsWin.open         = true;
                    m_cancelsWin.pendingFocus = true;
                }
            }
            // u1[0]-u1[34]
            for (int n = 0; n < 35; ++n)
            {
                char id[20], lbl[24];
                snprintf(id,  sizeof(id),  "##pj_u1_%d", n);
                snprintf(lbl, sizeof(lbl), ProjectileLabel::U1Fmt, n, n * 4);
                if (RowHex32Edit(id, lbl, p.u1[n])) m_dirty = true;
            }
            // u2[0]-u2[15]
            for (int n = 0; n < 16; ++n)
            {
                char id[20], lbl[24];
                snprintf(id,  sizeof(id),  "##pj_u2_%d", n);
                snprintf(lbl, sizeof(lbl), ProjectileLabel::U2Fmt, n, 0xA0 + n * 4);
                if (RowHex32Edit(id, lbl, p.u2[n])) m_dirty = true;
            }
            ImGui::EndTable();
        }
    }
    ImGui::EndChild();

    ImGui::End();
}

// -------------------------------------------------------------
//  Input Sequences subwindow
//  Left  : sequence list + sequence detail (input_window_frames, input_amount, _0x4)
//  Right : inputs table for selected sequence (command hex64, inline editable)
// -------------------------------------------------------------

void MovesetEditorWindow::RenderSubWin_InputSequences()
{
    if (!m_inputSeqWin.open) return;
    ImGui::SetNextWindowSize(ImVec2(580.0f, 400.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Input Sequences##blkwin", &m_inputSeqWin.open, ImGuiWindowFlags_NoCollapse))
    { ImGui::End(); return; }

    const auto& seqs = m_data.inputSequenceBlock;
    const auto& inps = m_data.inputBlock;

    const float spacing = ImGui::GetStyle().ItemSpacing.x;
    const float totalW  = ImGui::GetContentRegionAvail().x;
    const float leftW   = totalW * 0.40f;
    const float rightW  = totalW - leftW - spacing;
    const float listH   = 150.0f;

    // ---- Left: sequence list + detail ----
    ImGui::BeginChild("##iseq_left", ImVec2(leftW, 0.0f), false);
    {
        ImGui::BeginChild("##iseqlist", ImVec2(0.0f, listH), true);
        ImGui::TextDisabled("input_sequences (%d)", (int)seqs.size()); ImGui::Separator();
        for (int i = 0; i < (int)seqs.size(); ++i)
        {
            char lbl[40];
            snprintf(lbl, sizeof(lbl), "#%d  (amount=%u)", i, seqs[i].input_amount);
            bool sel = (m_inputSeqWin.sel.outer == i);
            if (ImGui::Selectable(lbl, sel)) { m_inputSeqWin.sel.outer = i; m_inputSeqWin.sel.inner = 0; }
            if (sel && m_inputSeqWin.sel.scrollOuter)
            {
                ImGui::SetScrollHereY(0.5f);
                m_inputSeqWin.sel.scrollOuter = false;
            }
        }
        ImGui::EndChild();

        if (m_inputSeqWin.sel.outer >= 0 && m_inputSeqWin.sel.outer < (int)seqs.size())
        {
            ParsedInputSequence& s = m_data.inputSequenceBlock[m_inputSeqWin.sel.outer];
            ImGui::TextDisabled("input_sequence #%d", m_inputSeqWin.sel.outer); ImGui::Separator();
            if (BeginPropTable("##iseqdt"))
            {
                if (RowU16Edit("##is_wf",  InputSeqLabel::InputWindowFrames, s.input_window_frames)) m_dirty = true;
                if (RowU16Edit("##is_amt", InputSeqLabel::InputAmount,       s.input_amount))        m_dirty = true;
                if (RowU32Edit("##is_0x4", InputSeqLabel::F0x4,              s._0x4))                m_dirty = true;
                ImGui::EndTable();
            }
        }
    }
    ImGui::EndChild();

    ImGui::SameLine();

    // ---- Right: inputs table for selected sequence ----
    ImGui::BeginChild("##iseq_right", ImVec2(rightW, 0.0f), true);
    if (m_inputSeqWin.sel.outer >= 0 && m_inputSeqWin.sel.outer < (int)seqs.size())
    {
        const ParsedInputSequence& s = seqs[m_inputSeqWin.sel.outer];
        ImGui::TextDisabled("inputs  (seq #%d,  amount=%u)", m_inputSeqWin.sel.outer, s.input_amount);
        ImGui::Separator();

        constexpr ImGuiTableFlags kTf =
            ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit;
        if (ImGui::BeginTable("##inptbl", 2, kTf))
        {
            ImGui::TableSetupColumn("Index",              ImGuiTableColumnFlags_WidthFixed,   45.0f);
            ImGui::TableSetupColumn(InputLabel::Command,  ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableHeadersRow();

            if (s.input_start_idx != 0xFFFFFFFF)
            {
                const uint32_t end = s.input_start_idx + s.input_amount;
                for (uint32_t k = s.input_start_idx; k < end && k < (uint32_t)inps.size(); ++k)
                {
                    const int localIdx = (int)(k - s.input_start_idx);
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    bool sel = (m_inputSeqWin.sel.inner == localIdx);
                    char selLbl[16]; snprintf(selLbl, sizeof(selLbl), "%d##inp%u", localIdx, k);
                    if (ImGui::Selectable(selLbl, sel, ImGuiSelectableFlags_SpanAllColumns))
                        m_inputSeqWin.sel.inner = localIdx;

                    ImGui::TableSetColumnIndex(1);
                    ImGui::SetNextItemWidth(-1.0f);
                    char inputId[24]; snprintf(inputId, sizeof(inputId), "##inp_cmd%u", k);
                    // hex64 inline edit
                    char hexBuf[20];
                    snprintf(hexBuf, sizeof(hexBuf), "0x%016llX",
                             (unsigned long long)m_data.inputBlock[k].command);
                    if (ImGui::InputText(inputId, hexBuf, sizeof(hexBuf),
                                         ImGuiInputTextFlags_CharsHexadecimal))
                    {
                        // parse on deactivate
                    }
                    if (ImGui::IsItemDeactivatedAfterEdit())
                    {
                        const char* p = hexBuf;
                        if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) p += 2;
                        uint64_t val = 0;
                        for (; *p; ++p)
                        {
                            val <<= 4;
                            if (*p >= '0' && *p <= '9') val |= (uint64_t)(*p - '0');
                            else if (*p >= 'a' && *p <= 'f') val |= (uint64_t)(*p - 'a' + 10);
                            else if (*p >= 'A' && *p <= 'F') val |= (uint64_t)(*p - 'A' + 10);
                        }
                        m_data.inputBlock[k].command = val;
                        m_dirty = true;
                    }
                }
            }
            ImGui::EndTable();
        }
    }
    ImGui::EndChild();

    ImGui::End();
}

// -------------------------------------------------------------
//  Parryable Moves subwindow
//  3-panel: outer groups | inner items | detail
//  Terminator: move_id == 0
// -------------------------------------------------------------

void MovesetEditorWindow::RenderSubWin_ParryableMoves()
{
    if (!m_parryWinOpen) return;
    ImGui::SetNextWindowSize(ImVec2(500.0f, 380.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Parryable Moves##blkwin", &m_parryWinOpen, ImGuiWindowFlags_NoCollapse))
    { ImGui::End(); return; }

    const auto& block = m_data.parryableMoveBlock;
    auto groups = ComputeGroups(block,
        +[](const ParsedParryableMove& pm) -> bool { return pm.value == 0; });

    float colW = ImGui::GetContentRegionAvail().x / 3.0f - ImGui::GetStyle().ItemSpacing.x;

    // Outer list
    ImGui::BeginChild("##pm_outer", ImVec2(colW, 0.0f), true);
    Render2LevelOuterList(groups, m_parryWinSel, "parryable-move lists");
    ImGui::EndChild();

    ImGui::SameLine();

    // Inner list
    ImGui::BeginChild("##pm_inner", ImVec2(colW, 0.0f), true);
    ImGui::TextDisabled("items"); ImGui::Separator();
    if (m_parryWinSel.outer < (int)groups.size())
    {
        uint32_t start = groups[m_parryWinSel.outer].first;
        uint32_t count = groups[m_parryWinSel.outer].second;
        for (uint32_t k = 0; k < count; ++k)
        {
            uint32_t idx = start + k;
            if (idx >= (uint32_t)block.size()) break;
            const ParsedParryableMove& pm = block[idx];
            const bool isEnd = (pm.value == 0);
            char lbl[48];
            if (isEnd) snprintf(lbl, sizeof(lbl), "#%u  [END]##pmi%u", k, idx);
            else        snprintf(lbl, sizeof(lbl), "#%u  0x%X##pmi%u", k, pm.value, idx);
            bool sel = (m_parryWinSel.inner == (int)k);
            if (isEnd) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
            if (ImGui::Selectable(lbl, sel)) m_parryWinSel.inner = (int)k;
            if (isEnd) ImGui::PopStyleColor();
        }
    }
    ImGui::EndChild();

    ImGui::SameLine();

    // Detail
    ImGui::BeginChild("##pm_detail", ImVec2(0.0f, 0.0f), false);
    if (m_parryWinSel.outer < (int)groups.size())
    {
        uint32_t start = groups[m_parryWinSel.outer].first;
        uint32_t idx   = start + (uint32_t)m_parryWinSel.inner;
        if (idx < (uint32_t)block.size())
        {
            ParsedParryableMove& pm = m_data.parryableMoveBlock[idx];
            const bool isEnd = (pm.value == 0);
            ImGui::TextDisabled("parryable_move #%u  (block[%u])%s",
                m_parryWinSel.inner, idx, isEnd ? "  [END]" : "");
            ImGui::Separator();
            if (BeginPropTable("##pmdt"))
            {
                if (RowU32Edit("##pm_val", ParryableMoveLabel::Value, pm.value)) m_dirty = true;
                ImGui::EndTable();
            }
        }
    }
    ImGui::EndChild();

    ImGui::End();
}
