// MovesetEditorWindow.cpp
// UI layout mirrors OldTool (TKMovesets2):
//   Left panel  -- searchable move list
//   Right panel -- collapsible property sections per move
#include "MovesetEditorWindow.h"
#include "LabelDB.h"
#include "MoveFieldLabels.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include <cstdio>
#include <cstring>
#include <cctype>
#include <algorithm>

extern ImFont* g_fontBold;

// -------------------------------------------------------------
//  Construction
// -------------------------------------------------------------

MovesetEditorWindow::MovesetEditorWindow(const std::string& folderPath,
                                         const std::string& movesetName,
                                         int uid)
{
    m_data = LoadMotbin(folderPath);

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

    if (!ImGui::Begin(m_windowTitle.c_str(), &m_open, kWinFlags))
    {
        ImGui::End();
        return m_open;
    }

    if (!m_data.loaded)
    {
        ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.35f, 1.0f),
                           "Failed to load: %s", m_data.errorMsg.c_str());
        ImGui::End();
        return m_open;
    }

    // -- Menu bar ------------------------------------------------
    RenderViewMenu();

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

    ImGui::BeginChild("##mew_listinner", ImVec2(0.0f, 0.0f), false);

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

// Finds the outer group whose absolute start index equals absIdx. Returns -1 if not found.
static int FindGroupOuter(const std::vector<std::pair<uint32_t,uint32_t>>& groups,
                          uint32_t absIdx)
{
    for (int i = 0; i < (int)groups.size(); ++i)
        if (groups[i].first == absIdx) return i;
    return -1;
}

// -------------------------------------------------------------

static void RenderSection_Hitboxes(const ParsedMove& m);

void MovesetEditorWindow::RenderMoveProperties(int idx)
{
    const ParsedMove& m = m_data.moves[idx];

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
            RenderSection_Overview(m);
            ImGui::EndTabItem();
        }
    }

    if (ImGui::BeginTabItem("Hitboxes###tab_hitboxes"))
    {
        ImGui::Spacing();
        RenderSection_Hitboxes(m);
        ImGui::EndTabItem();
    }

    if (ImGui::BeginTabItem("Debug###tab_debug"))
    {
        ImGui::Spacing();
        RenderSection_Unknown(m);
        ImGui::EndTabItem();
    }

    ImGui::EndTabBar();
}

// -------------------------------------------------------------
//  Group computation helpers (placed here so navigation handlers in
//  RenderSection_Overview can call them without forward-declaration issues)
// -------------------------------------------------------------

template<typename T>
static std::vector<std::pair<uint32_t,uint32_t>>
ComputeGroups(const std::vector<T>& block,
              bool(*isTerminator)(const T&))
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
void MovesetEditorWindow::RenderSection_Overview(const ParsedMove& m)
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
                isTerm = (reqBlk[h.req_list_idx].req == 1100);
            if (isTerm) { hitCondGroups.push_back({start, i - start + 1}); start = i + 1; }
        }
        if (start < (uint32_t)blk.size())
            hitCondGroups.push_back({start, (uint32_t)blk.size() - start});
    }

    const auto epGroups = ComputeGroups(m_data.extraPropBlock,
        +[](const ParsedExtraProp& e)->bool{ return e.type == 0; });
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
        RowStr  (Name,         m.displayName.c_str());
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
        RowU32  (Vuln,         m.vuln);
        RowU32  (Hitlevel,     m.hitlevel);
        clickCancel    = RowIdxLink      (CancelIdx,    m.cancel_idx,        cancelValid);
        if (transIsGeneric)
            clickTrans = RowGenericMoveLink(Transition, transU16, transResolvedIdx, transValid);
        else
            clickTrans = RowTransitionLink(Transition, static_cast<int16_t>(transU16), transValid);
        RowI32  (AnimLen,      m.anim_len);
        clickHitCond   = RowIdxLink      (HitCondIdx,   m.hit_condition_idx, hitCondValid);
        clickVoiceclip = RowIdxLink      (VoiceclipIdx, m.voiceclip_idx,     voiceclipValid);
        clickExtraProp = RowIdxLink      (ExtraPropIdx, m.extra_prop_idx,    epValid);
        clickStartProp = RowIdxLink      (StartPropIdx, m.start_prop_idx,    spValid);
        clickEndProp   = RowIdxLink      (EndPropIdx,   m.end_prop_idx,      npValid);
        ImGui::EndTable();
    }
    ImGui::EndChild(); ImGui::SameLine();

    // -- Column 2 -------------------------------------------------------------
    ImGui::BeginChild("##ov_c2", ImVec2(0.0f, 0.0f), false);
    if (BeginPropTable("##ov2")) {
        RowI16  (CE,            m._0xCE);
        RowHex32(TCharId,       m.ordinal_id2);
        RowHex32(OrdinalId,     m.moveId);
        RowU32  (F0x118,        m._0x118);
        RowU32  (F0x11C,        m._0x11C);
        RowU32  (AirborneStart, m.airborne_start);
        RowU32  (AirborneEnd,   m.airborne_end);
        RowU32  (GroundFall,    m.ground_fall);
        RowU32  (F0x154,        m._0x154);
        RowU32  (U6,            m.u6);
        RowHex32(U15,           m.u15);
        RowI16  (Collision,     static_cast<int16_t>(m.collision));
        RowI16  (Distance,      static_cast<int16_t>(m.distance));
        RowU32  (U18,           m.u18);
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

void MovesetEditorWindow::RenderSection_Unknown(const ParsedMove& m)
{
    ImGui::TextDisabled("Raw 64-bit encrypted values as stored in file.");
    ImGui::Spacing();

    if (!BeginPropTable("##unk")) return;

    RowHex64("u1              [0xA8]", m.u1);
    RowHex64("u2              [0xB0]", m.u2);
    RowHex64("u3              [0xB8]", m.u3);
    RowHex64("u4              [0xC0]", m.u4);
    RowHex64("enc_name_key    [0x00]", m.encrypted_name_key);
    RowHex64("name_enc_key    [0x08]", m.name_encryption_key);
    RowHex64("enc_anim_key    [0x20]", m.encrypted_anim_key);
    RowHex64("anim_enc_key    [0x28]", m.anim_encryption_key);
    RowU32  ("anmbin_body_idx [0x50]", m.anmbin_body_idx);
    RowHex64("enc_vuln        [0x58]", m.encrypted_vuln);
    RowHex64("vuln_enc_key    [0x60]", m.vuln_encryption_key);
    RowHex64("enc_hitlevel    [0x78]", m.encrypted_hitlevel);
    RowHex64("hitlevel_enc_key[0x80]", m.hitlevel_encryption_key);
    RowHex64("enc_ordinal_id2 [0xD0]", m.encrypted_ordinal_id2);
    RowHex64("ordinal_id2_key [0xD8]", m.ordinal_id2_enc_key);
    RowHex64("enc_ordinal_id  [0xF0]", m.encrypted_ordinal_id);
    RowHex64("ordinal_enc_key [0xF8]", m.ordinal_encryption_key);
    RowU32  ("anmbin_body_idx [0x50]", m.anmbin_body_idx);
    ImGui::EndTable();

}

// -------------------------------------------------------------
//  Hitbox tab
// -------------------------------------------------------------

static void RenderSection_Hitboxes(const ParsedMove& m)
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

            ImGui::Text("Active   : %u - %u", start, last);
            ImGui::Text("Location : 0x%08X", loc);

            // Related floats in 3x3 grid
            ImGui::Text("Related Floats :  %.3f, %.3f, %.3f", fl[0], fl[1], fl[2]);
            float baseX  = ImGui::GetCursorPosX();
            float indent = ImGui::CalcTextSize("Related Floats :  ").x;
            ImGui::SetCursorPosX(baseX + indent);
            ImGui::Text("%.3f, %.3f, %.3f", fl[3], fl[4], fl[5]);
            ImGui::SetCursorPosX(baseX + indent);
            ImGui::Text("%.3f, %.3f, %.3f", fl[6], fl[7], fl[8]);

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

void MovesetEditorWindow::RenderViewMenu()
{
    if (!ImGui::BeginMenuBar()) return;
    if (ImGui::BeginMenu("View"))
    {
        if (ImGui::MenuItem("Requirements",   nullptr, m_reqWinOpen))      m_reqWinOpen      = !m_reqWinOpen;
        if (ImGui::MenuItem("Cancels",         nullptr, m_cancelsWin.open)) m_cancelsWin.open = !m_cancelsWin.open;
        if (ImGui::MenuItem("Hit Conditions",  nullptr, m_hitCondWinOpen))  m_hitCondWinOpen  = !m_hitCondWinOpen;
        if (ImGui::MenuItem("Reaction Lists",  nullptr, m_reacWin.open))    m_reacWin.open    = !m_reacWin.open;
        if (ImGui::MenuItem("Pushbacks",       nullptr, m_pushbackWin.open))m_pushbackWin.open= !m_pushbackWin.open;
        if (ImGui::MenuItem("Voiceclips",      nullptr, m_voiceclipWinOpen))m_voiceclipWinOpen= !m_voiceclipWinOpen;
        if (ImGui::MenuItem("Properties",      nullptr, m_propertiesWin.open))m_propertiesWin.open=!m_propertiesWin.open;
        ImGui::EndMenu();
    }
    ImGui::EndMenuBar();
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
    auto groups = ComputeGroups(block, +[](const ParsedRequirement& r)->bool{ return r.req==1100; });

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
            bool isTerm2 = (r.req == 1100);
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
            const ParsedRequirement& r = block[idx];
            ImGui::TextDisabled("requirement #%u  (block[%u])", m_reqWinSel.inner, idx);
            ImGui::Separator();
            if (BeginPropTable("##reqdt"))
            {
                RowU32("req",    r.req);
                RowU32("param",  r.param);
                RowU32("param2", r.param2);
                RowU32("param3", r.param3);
                RowU32("param4", r.param4);
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
    const ParsedCancel& c, int localIdx, uint32_t blockIdx,
    const std::vector<std::pair<uint32_t,uint32_t>>& gcGroups)
{
    ImGui::TextDisabled("cancel #%d  (block[%u])", localIdx, blockIdx);
    ImGui::Separator();
    if (!BeginPropTable("##cdt")) return;

    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0); ImGui::TextDisabled("command");
    ImGui::TableSetColumnIndex(1); ImGui::Text("0x%llX", (unsigned long long)c.command);

    // cancel-extra link
    {
        static constexpr ImVec4 kGreen = {0.55f, 0.85f, 0.55f, 1.0f};
        static constexpr ImVec4 kPink  = {1.00f, 0.50f, 0.65f, 1.0f};
        bool hasVal = (c.extradata_idx != 0xFFFFFFFF);
        bool valid  = hasVal && (c.extradata_idx < (uint32_t)m_data.cancelExtraBlock.size());
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        if (!hasVal) {
            ImGui::TextDisabled("cancel-extra");
            ImGui::TableSetColumnIndex(1); ImGui::TextDisabled("--");
        } else {
            ImGui::PushStyleColor(ImGuiCol_Text, valid ? kGreen : kPink);
            bool clicked = ImGui::Selectable("cancel-extra##cxlbl", false, ImGuiSelectableFlags_SpanAllColumns);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip(valid ? "Click to navigate" : "Navigate target not found.");
            ImGui::PopStyleColor();
            ImGui::TableSetColumnIndex(1);
            ImGui::Text("[%u]", c.extradata_idx);
            if (clicked && valid) {
                m_cancelsWin.extradataSel       = (int)c.extradata_idx;
                m_cancelsWin.extraScrollPending = true;
            }
        }
    }

    // requirement link
    {
        static constexpr ImVec4 kGreen = {0.55f, 0.85f, 0.55f, 1.0f};
        static constexpr ImVec4 kPink  = {1.00f, 0.50f, 0.65f, 1.0f};
        bool hasVal = (c.req_list_idx != 0xFFFFFFFF);
        bool valid  = hasVal && (c.req_list_idx < (uint32_t)m_data.requirementBlock.size());
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        if (!hasVal) {
            ImGui::TextDisabled("requirement");
            ImGui::TableSetColumnIndex(1); ImGui::TextDisabled("--");
        } else {
            ImGui::PushStyleColor(ImGuiCol_Text, valid ? kGreen : kPink);
            bool clicked = ImGui::Selectable("requirement##rqlbl", false, ImGuiSelectableFlags_SpanAllColumns);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip(valid ? "Click to navigate" : "Navigate target not found.");
            ImGui::PopStyleColor();
            ImGui::TableSetColumnIndex(1);
            ImGui::Text("[%u]", c.req_list_idx);
            if (clicked && valid) {
                const auto& blk = m_data.requirementBlock;
                auto grps = ComputeGroups(blk, +[](const ParsedRequirement& r)->bool{ return r.req==1100; });
                int gi = FindGroupOuter(grps, c.req_list_idx);
                if (gi >= 0) {
                    m_reqWinSel.outer       = gi;
                    m_reqWinSel.inner       = 0;
                    m_reqWinSel.scrollOuter = true;
                }
                m_reqWinOpen = true;
            }
        }
    }

    RowU32("frame_window_start", c.frame_window_start);
    RowU32("frame_window_end",   c.frame_window_end);
    RowU32("starting_frame",     c.starting_frame);

    // move_id: move reference normally, group-cancel list index when command == 0x8012
    if (c.command == 0x8012)
    {
        const bool valid   = (FindGroupOuter(gcGroups, (uint32_t)c.move_id) >= 0);
        const bool clicked = RowIdxLink("group_cancel_list_idx", (uint32_t)c.move_id, valid);
        if (clicked)
        {
            int gi = FindGroupOuter(gcGroups, (uint32_t)c.move_id);
            if (gi >= 0)
            {
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
        // Terminator: command is the list-end sentinel AND move_id is also 0x8000
        const bool isTerm = ((c.command == 0x8000 || c.command == 0x8013) && c.move_id == 0x8000);
        if (!isTerm && c.move_id >= 0x8000)
        {
            // Generic move ID -- resolve via original_aliases
            const uint32_t aliasIdx = c.move_id - 0x8000u;
            int resolvedIdx = -1;
            if (aliasIdx < m_data.originalAliases.size())
            {
                const uint16_t resolved = m_data.originalAliases[aliasIdx];
                if ((size_t)resolved < m_data.moves.size())
                    resolvedIdx = static_cast<int>(resolved);
            }
            const bool clicked = RowGenericMoveLink("move", c.move_id, resolvedIdx, resolvedIdx >= 0);
            if (clicked) { m_selectedIdx = resolvedIdx; m_moveListScrollPending = true; }
        }
        else
        {
            const bool valid   = !isTerm && (c.move_id < (uint16_t)m_data.moves.size());
            const bool clicked = RowMoveLink("move", c.move_id, valid);
            if (clicked) { m_selectedIdx = (int)c.move_id; m_moveListScrollPending = true; }
        }
    }

    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0); ImGui::TextDisabled("option");
    ImGui::TableSetColumnIndex(1); ImGui::Text("0x%04X", c.cancel_option);
    ImGui::EndTable();
}

static void RenderCancelSection(
    const char* outerChildId, const char* innerChildId, const char* detailChildId,
    const std::vector<ParsedCancel>& block,
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
            else if (c.command == 0x8012)
                snprintf(lbl, sizeof(lbl), "#%u  [GRP_START]##ci%u", k, idx);
            else if (c.command == 0x8013)
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

    auto isCancelTerm      = +[](const ParsedCancel& c)->bool { return c.command == 0x8000; };
    auto isGroupCancelTerm = +[](const ParsedCancel& c)->bool { return c.command == 0x8013; };
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
            "group-cancel-lists", 0.0f, 0x8013,
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
            ImGui::TableSetColumnIndex(1); ImGui::Text("%u", exb[i]);
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
                isTerm = (reqBlk[h.req_list_idx].req == 1100);
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
                isTerm = (reqBlk[h.req_list_idx].req == 1100);
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
            const ParsedHitCondition& h = block[idx];
            ImGui::TextDisabled("hit-condition #%d  (block[%u])", m_hitCondWinSel.inner, idx);
            ImGui::Separator();
            if (BeginPropTable("##hcdt"))
            {
                // requirement link
                {
                    static constexpr ImVec4 kGreen = {0.55f, 0.85f, 0.55f, 1.0f};
                    static constexpr ImVec4 kPink  = {1.00f, 0.50f, 0.65f, 1.0f};
                    bool hasVal = (h.req_list_idx != 0xFFFFFFFF);
                    bool valid  = hasVal && (h.req_list_idx < (uint32_t)m_data.requirementBlock.size());
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    if (!hasVal) {
                        ImGui::TextDisabled("requirement");
                        ImGui::TableSetColumnIndex(1); ImGui::TextDisabled("--");
                    } else {
                        ImGui::PushStyleColor(ImGuiCol_Text, valid ? kGreen : kPink);
                        bool clicked = ImGui::Selectable("requirement##hcrqlbl", false, ImGuiSelectableFlags_SpanAllColumns);
                        if (ImGui::IsItemHovered()) ImGui::SetTooltip(valid ? "Click to navigate" : "Navigate target not found.");
                        ImGui::PopStyleColor();
                        ImGui::TableSetColumnIndex(1); ImGui::Text("[%u]", h.req_list_idx);
                        if (clicked && valid) {
                            const auto& blk = m_data.requirementBlock;
                            auto grps = ComputeGroups(blk, +[](const ParsedRequirement& r)->bool{ return r.req==1100; });
                            int gi = FindGroupOuter(grps, h.req_list_idx);
                            if (gi >= 0) { m_reqWinSel.outer = gi; m_reqWinSel.inner = 0; m_reqWinSel.scrollOuter = true; }
                            m_reqWinOpen = true;
                        }
                    }
                }
                RowU32("damage", h.damage);
                // reaction_list link
                {
                    static constexpr ImVec4 kGreen = {0.55f, 0.85f, 0.55f, 1.0f};
                    static constexpr ImVec4 kPink  = {1.00f, 0.50f, 0.65f, 1.0f};
                    bool hasVal = (h.reaction_list_idx != 0xFFFFFFFF);
                    bool valid  = hasVal && (h.reaction_list_idx < (uint32_t)m_data.reactionListBlock.size());
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    if (!hasVal) {
                        ImGui::TextDisabled("reaction_list");
                        ImGui::TableSetColumnIndex(1); ImGui::TextDisabled("--");
                    } else {
                        ImGui::PushStyleColor(ImGuiCol_Text, valid ? kGreen : kPink);
                        bool clicked = ImGui::Selectable("reaction_list##hcrllbl", false, ImGuiSelectableFlags_SpanAllColumns);
                        if (ImGui::IsItemHovered()) ImGui::SetTooltip(valid ? "Click to navigate" : "Navigate target not found.");
                        ImGui::PopStyleColor();
                        ImGui::TableSetColumnIndex(1); ImGui::Text("[%u]", h.reaction_list_idx);
                        if (clicked && valid) {
                            m_reacWin.open          = true;
                            m_reacWin.selectedIdx   = (int)h.reaction_list_idx;
                            m_reacWin.scrollPending = true;
                        }
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
        const ParsedReactionList& rl = block[m_reacWin.selectedIdx];
        ImGui::TextDisabled("ReactionList #%d", m_reacWin.selectedIdx);
        ImGui::Separator();

        float thirdW = ImGui::GetContentRegionAvail().x / 3.0f - ImGui::GetStyle().ItemSpacing.x;

        // Helper: resolve uint16 move id and emit a navigable link
        auto RlMoveRow = [&](const char* lbl, uint16_t id) -> bool {
            bool clicked = false;
            if (id >= 0x8000) {
                const uint32_t ai = id - 0x8000u;
                int resolved = -1;
                if (ai < m_data.originalAliases.size()) {
                    uint16_t r = m_data.originalAliases[ai];
                    if ((size_t)r < m_data.moves.size()) resolved = (int)r;
                }
                clicked = RowGenericMoveLink(lbl, id, resolved, resolved >= 0);
            } else {
                bool valid = (id < (uint16_t)m_data.moves.size());
                clicked = RowMoveLink(lbl, id, valid);
            }
            return clicked;
        };

        ImGui::BeginChild("##rl_mv", ImVec2(thirdW, 0.0f), true);
        ImGui::TextDisabled("reaction moves"); ImGui::Separator();
        if (BeginPropTable("##rlm")) {
            const uint16_t* ids[] = {
                &rl.standing, &rl.ch, &rl.crouch, &rl.crouch_ch,
                &rl.left_side, &rl.left_side_crouch, &rl.right_side, &rl.right_side_crouch,
                &rl.back, &rl.back_crouch, &rl.block, &rl.crouch_block,
                &rl.wallslump, &rl.downed
            };
            const char* names[] = {
                "default (standing)", "ch", "crouch", "crouch_ch",
                "left_side", "left_side_crouch", "right_side", "right_side_crouch",
                "back", "back_crouch", "block", "crouch_block",
                "wallslump", "downed"
            };
            for (int r = 0; r < 14; ++r) {
                if (RlMoveRow(names[r], *ids[r])) {
                    uint16_t id = *ids[r];
                    if (id >= 0x8000) {
                        const uint32_t ai = id - 0x8000u;
                        if (ai < m_data.originalAliases.size()) {
                            uint16_t res = m_data.originalAliases[ai];
                            if ((size_t)res < m_data.moves.size()) {
                                m_selectedIdx = (int)res;
                                m_moveListScrollPending = true;
                            }
                        }
                    } else if (id < (uint16_t)m_data.moves.size()) {
                        m_selectedIdx = (int)id;
                        m_moveListScrollPending = true;
                    }
                }
            }
            ImGui::EndTable();
        }
        ImGui::EndChild(); ImGui::SameLine();

        static const char* kPbN[7] = {
            "front_pushback","back_pushback","left_side_pushback",
            "right_side_pushback","front_ch_pushback","downed_pushback","block_pushback"
        };
        ImGui::BeginChild("##rl_pb", ImVec2(thirdW, 0.0f), true);
        ImGui::TextDisabled("Pushbacks"); ImGui::Separator();
        if (BeginPropTable("##rlp")) {
            static constexpr ImVec4 kGreen = {0.55f, 0.85f, 0.55f, 1.0f};
            static constexpr ImVec4 kPink  = {1.00f, 0.50f, 0.65f, 1.0f};
            for (int p = 0; p < 7; ++p) {
                bool hasVal = (rl.pushback_idx[p] != 0xFFFFFFFF);
                bool valid  = hasVal && (rl.pushback_idx[p] < (uint32_t)m_data.pushbackBlock.size());
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                if (!hasVal) {
                    ImGui::TextDisabled("%s", kPbN[p]);
                    ImGui::TableSetColumnIndex(1); ImGui::TextDisabled("--");
                } else {
                    char lnk[40]; snprintf(lnk, sizeof(lnk), "%s##rlpblbl%d", kPbN[p], p);
                    ImGui::PushStyleColor(ImGuiCol_Text, valid ? kGreen : kPink);
                    bool clicked = ImGui::Selectable(lnk, false, ImGuiSelectableFlags_SpanAllColumns);
                    if (ImGui::IsItemHovered()) ImGui::SetTooltip(valid ? "Click to navigate" : "Navigate target not found.");
                    ImGui::PopStyleColor();
                    ImGui::TableSetColumnIndex(1);
                    ImGui::Text("[%u]", rl.pushback_idx[p]);
                    if (clicked && valid) {
                        m_pushbackWin.open            = true;
                        m_pushbackWin.pushbackSel     = (int)rl.pushback_idx[p];
                        m_pushbackWin.pbScrollPending = true;
                    }
                }
            }
            ImGui::EndTable();
        }
        ImGui::EndChild(); ImGui::SameLine();

        ImGui::BeginChild("##rl_oth", ImVec2(0.0f, 0.0f), true);
        ImGui::TextDisabled("others"); ImGui::Separator();
        if (BeginPropTable("##rlo")) {
            RowU16("front_direction",      rl.front_direction);
            RowU16("back_direction",       rl.back_direction);
            RowU16("left_side_direction",  rl.left_side_direction);
            RowU16("right_side_direction", rl.right_side_direction);
            RowU16("front_ch_direction",   rl.front_ch_direction);
            RowU16("downed_direction",     rl.downed_direction);
            RowU16("front_rotation",       rl.front_rotation);
            RowU16("back_rotation",        rl.back_rotation);
            RowU16("left_side_rotation",   rl.left_side_rotation);
            RowU16("right_side_rotation",  rl.right_side_rotation);
            RowU16("vertical_pushback",    rl.vertical_pushback);
            RowU16("downed_rotation",      rl.downed_rotation);
            ImGui::EndTable();
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
            const ParsedPushback& p = pb[m_pushbackWin.pushbackSel];
            ImGui::TextDisabled("Pushback #%d", m_pushbackWin.pushbackSel); ImGui::Separator();
            if (BeginPropTable("##pbdt")) {
                RowU16("duration",         p.val1);
                RowU16("displacement",     p.val2);
                RowU32("nonlinear_frames", p.val3);
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0); ImGui::TextDisabled("pushback-extra");
                ImGui::TableSetColumnIndex(1);
                if (p.pushback_extra_idx == 0xFFFFFFFF) {
                    ImGui::TextDisabled("--");
                } else {
                    bool valid = (p.pushback_extra_idx < (uint32_t)pe.size());
                    char lnk[32]; snprintf(lnk, sizeof(lnk), "[%u]##pelnk", p.pushback_extra_idx);
                    if (valid) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.40f, 0.85f, 0.40f, 1.0f));
                    else       ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.85f, 0.40f, 0.40f, 1.0f));
                    if (ImGui::Selectable(lnk, false, ImGuiSelectableFlags_None) && valid) {
                        m_pushbackWin.extraSel = (int)p.pushback_extra_idx;
                        m_pushbackWin.extraScrollPending = true;
                    }
                    ImGui::PopStyleColor();
                    if (valid && ImGui::IsItemHovered())
                        ImGui::SetTooltip("Click to navigate");
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
        ImGui::TableSetupColumn("Index", ImGuiTableColumnFlags_WidthFixed, 45.0f);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
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
            ImGui::TableSetColumnIndex(1); ImGui::Text("%u", (uint32_t)pe[i].value);
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

    ImGui::BeginChild("##vc_master", ImVec2(110.0f, 0.0f), true);
    ImGui::TextDisabled("%d entries", total); ImGui::Separator();
    for (int i = 0; i < total; ++i) {
        char lbl[24]; snprintf(lbl, sizeof(lbl), "#%d", i);
        bool sel = (m_voiceclipWinSel == i);
        if (ImGui::Selectable(lbl, sel)) m_voiceclipWinSel = i;
        if (sel && m_voiceclipWinScroll) { ImGui::SetScrollHereY(0.5f); m_voiceclipWinScroll = false; }
    }
    ImGui::EndChild();

    ImGui::SameLine();
    ImGui::BeginChild("##vc_detail", ImVec2(0.0f, 0.0f), false);
    if (m_voiceclipWinSel >= 0 && m_voiceclipWinSel < total) {
        const ParsedVoiceclip& vc = block[m_voiceclipWinSel];
        ImGui::TextDisabled("Voiceclip #%d", m_voiceclipWinSel); ImGui::Separator();
        if (BeginPropTable("##vcdt")) {
            RowU32("val1", vc.val1);
            RowU32("val2", vc.val2);
            RowU32("val3", vc.val3);
            ImGui::EndTable();
        }
    }
    ImGui::EndChild();
    ImGui::End();
}

// -------------------------------------------------------------
//  Properties subwindow  (3-panel for each of extraProp/startProp/endProp)
//  Outer groups: extraProp ends with id==0; start/endProp end with req==1100 terminator
// -------------------------------------------------------------

static void RenderPropSection(
    const char* outerChildId, const char* innerChildId, const char* detailChildId,
    const std::vector<ParsedExtraProp>& block,
    const std::vector<ParsedRequirement>& reqBlk,
    const std::vector<std::pair<uint32_t,uint32_t>>& groups,
    MovesetEditorWindow::TwoLevelSel& sel,
    const char* listLabel,
    bool isExtraProp,
    MovesetEditorWindow::TwoLevelSel& reqWinSel,
    bool& reqWinOpen)
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
            bool isTerm = isExtraProp ? (e.type == 0) :
                (e.requirement_addr == 0 ||
                 (e.req_list_idx != 0xFFFFFFFF &&
                  e.req_list_idx < (uint32_t)reqBlk.size() &&
                  reqBlk[e.req_list_idx].req == 1100));
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
            const ParsedExtraProp& e = block[idx];
            ImGui::TextDisabled("prop #%d  (block[%u])", sel.inner, idx);
            ImGui::Separator();
            if (BeginPropTable("##prdt"))
            {
                if (isExtraProp)
                    RowU32("starting_frame", e.type);
                RowU32("id", e.id);
                // requirement link
                {
                    static constexpr ImVec4 kGreen = {0.55f, 0.85f, 0.55f, 1.0f};
                    static constexpr ImVec4 kPink  = {1.00f, 0.50f, 0.65f, 1.0f};
                    bool hasVal = (e.req_list_idx != 0xFFFFFFFF);
                    bool valid  = hasVal && (e.req_list_idx < (uint32_t)reqBlk.size());
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    if (!hasVal) {
                        ImGui::TextDisabled("requirement");
                        ImGui::TableSetColumnIndex(1); ImGui::TextDisabled("--");
                    } else {
                        ImGui::PushStyleColor(ImGuiCol_Text, valid ? kGreen : kPink);
                        bool clicked = ImGui::Selectable("requirement##proplbl", false, ImGuiSelectableFlags_SpanAllColumns);
                        if (ImGui::IsItemHovered()) ImGui::SetTooltip(valid ? "Click to navigate" : "Navigate target not found.");
                        ImGui::PopStyleColor();
                        ImGui::TableSetColumnIndex(1); ImGui::Text("[%u]", e.req_list_idx);
                        if (clicked && valid) {
                            auto grps = ComputeGroups(reqBlk, +[](const ParsedRequirement& r)->bool{ return r.req==1100; });
                            int gi = FindGroupOuter(grps, e.req_list_idx);
                            if (gi >= 0) { reqWinSel.outer = gi; reqWinSel.inner = 0; reqWinSel.scrollOuter = true; }
                            reqWinOpen = true;
                        }
                    }
                }
                RowU32("value1", e.value);
                RowU32("value2", e.value2);
                RowU32("value3", e.value3);
                RowU32("value4", e.value4);
                RowU32("value5", e.value5);
                ImGui::EndTable();
            }
        }
    }
    ImGui::EndChild();
}

static std::vector<std::pair<uint32_t,uint32_t>>
ComputeOtherPropGroups(const std::vector<ParsedExtraProp>& block,
                       const std::vector<ParsedRequirement>& reqBlk)
{
    std::vector<std::pair<uint32_t,uint32_t>> groups;
    uint32_t start = 0;
    for (uint32_t i = 0; i < (uint32_t)block.size(); ++i)
    {
        const auto& e = block[i];
        bool isTerm = (e.requirement_addr == 0);
        if (!isTerm && e.req_list_idx != 0xFFFFFFFF &&
            e.req_list_idx < (uint32_t)reqBlk.size())
            isTerm = (reqBlk[e.req_list_idx].req == 1100);
        if (isTerm)
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
        +[](const ParsedExtraProp& e)->bool{ return e.type == 0; });
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
            "property-lists", true, m_reqWinSel, m_reqWinOpen);
        ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("Start", nullptr,
            pendingTab == 1 ? ImGuiTabItemFlags_SetSelected : 0))
    {
        RenderPropSection("##spo","##spi","##spd",
            m_data.startPropBlock, reqBlk, spGroups, m_propertiesWin.spSel,
            "start-property-lists", false, m_reqWinSel, m_reqWinOpen);
        ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("End", nullptr,
            pendingTab == 2 ? ImGuiTabItemFlags_SetSelected : 0))
    {
        RenderPropSection("##npo","##npi","##npd",
            m_data.endPropBlock, reqBlk, npGroups, m_propertiesWin.npSel,
            "end-property-lists", false, m_reqWinSel, m_reqWinOpen);
        ImGui::EndTabItem();
    }

    ImGui::EndTabBar();
    ImGui::End();
}
