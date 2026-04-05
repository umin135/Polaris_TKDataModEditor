// MovesetEditorWindow.cpp
// UI layout mirrors OldTool (TKMovesets2):
//   Left panel  -- searchable move list
//   Right panel -- collapsible property sections per move
#include "moveset/editor/MovesetEditorWindow.h"
#include "GameStatic.h"
#include "moveset/labels/LabelDB.h"
#include "moveset/data/EditorFieldLabel.h"
#include "moveset/labels/FieldTooltips.h"
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
//  List-action enum + generic +button popup helper
// -------------------------------------------------------------

enum class ListAction { None, Insert, Duplicate, Remove };

// Renders a "+" button that opens a popup with Insert / Duplicate / Remove items.
// The popup ID must be unique in the current window context.
static ListAction RenderListPlusMenu(const char* popupId, const char* typeName)
{
    if (ImGui::Button("+")) ImGui::OpenPopup(popupId);
    if (!ImGui::BeginPopup(popupId)) return ListAction::None;
    ListAction act = ListAction::None;
    char lbl[128];
    snprintf(lbl, sizeof(lbl), "Insert New %s", typeName);
    if (ImGui::MenuItem(lbl)) act = ListAction::Insert;
    snprintf(lbl, sizeof(lbl), "Duplicate Selected %s", typeName);
    if (ImGui::MenuItem(lbl)) act = ListAction::Duplicate;
    ImGui::Separator();
    snprintf(lbl, sizeof(lbl), "Remove Selected %s", typeName);
    if (ImGui::MenuItem(lbl)) act = ListAction::Remove;
    ImGui::EndPopup();
    return act;
}

// -------------------------------------------------------------
//  Construction
// -------------------------------------------------------------

MovesetEditorWindow::MovesetEditorWindow(const std::string& folderPath,
                                         const std::string& movesetName,
                                         int uid)
{
    m_data = LoadMotbin(folderPath);
    LoadEditorDatas();
    TryInitAnimNameDB();

    m_movesetName = movesetName;
    m_uid         = uid;

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

    // On first render: position just outside the main viewport so ImGui promotes
    // this window to a secondary viewport (independent OS window) immediately.
    // ImGuiCond_FirstUseEver means imgui.ini saves the user's chosen position after that.
    if (m_firstFrame)
    {
        m_firstFrame   = false;
        m_pendingFocus = true;
        const ImGuiViewport* mv = ImGui::GetMainViewport();
        // ImGuiCond_Always: override any imgui.ini saved position so the editor
        // always opens outside the main OS window and becomes a secondary viewport.
        ImGui::SetNextWindowPos(
            ImVec2(mv->Pos.x + mv->Size.x + 20.0f, mv->Pos.y + 60.0f),
            ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(1280.0f, 800.0f), ImGuiCond_Always);
    }
    else
    {
        ImGui::SetNextWindowSize(ImVec2(1280.0f, 800.0f), ImGuiCond_FirstUseEver);
    }
    ImGui::SetNextWindowSizeConstraints(ImVec2(640.0f, 420.0f), ImVec2(FLT_MAX, FLT_MAX));

    if (m_pendingMoveX >= 0.f)
    {
        ImGui::SetNextWindowPos(ImVec2(m_pendingMoveX, m_pendingMoveY), ImGuiCond_Always);
        m_pendingMoveX = m_pendingMoveY = -1.f;
    }

    constexpr ImGuiWindowFlags kWinFlags =
        ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_MenuBar;

    // Use a local copy so we can detect when the X button is clicked
    bool windowOpen = true;
    if (!ImGui::Begin(m_windowTitle.c_str(), &windowOpen, kWinFlags))
    {
        if (!windowOpen) RequestClose();
        ImGui::End();
        // Still render overlays so popups survive window collapse/minimize.
        RenderSavePopups();
        RenderCloseConfirmModal();
        RenderRemoveConfirmModal();
        return m_open;
    }
    if (!windowOpen) RequestClose();

    // Track which viewport this window is in so popups can be shown in the same OS window.
    m_viewportId = ImGui::GetWindowViewport()->ID;

    // Retry every frame until the secondary viewport OS window is actually created
    // (PlatformHandle becomes non-null). On first editor open this can take several
    // frames; with an existing editor it's usually ready by frame 2.
    if (m_pendingFocus)
    {
        HWND edHwnd = static_cast<HWND>(ImGui::GetWindowViewport()->PlatformHandle);
        if (edHwnd)
        {
            m_pendingFocus = false;
            HWND mainHwnd  = static_cast<HWND>(ImGui::GetMainViewport()->PlatformHandle);

            // Capture main window position before minimizing
            RECT mainRect = {};
            if (mainHwnd) ::GetWindowRect(mainHwnd, &mainRect);

            ::ShowWindow(edHwnd, SW_SHOW);
            ::SetForegroundWindow(edHwnd);

            // Flash taskbar button until the editor receives focus
            FLASHWINFO fi = {};
            fi.cbSize    = sizeof(fi);
            fi.hwnd      = edHwnd;
            fi.dwFlags   = FLASHW_ALL | FLASHW_TIMERNOFG;
            fi.uCount    = 0;
            fi.dwTimeout = 0;
            ::FlashWindowEx(&fi);

            if (mainHwnd) ::ShowWindow(mainHwnd, SW_MINIMIZE);

            // Schedule move to where the main window was via ImGui next frame
            // (direct SetWindowPos bypasses ImGui viewport tracking → broken mouse input)
            if (mainHwnd && (mainRect.right > mainRect.left))
            {
                m_pendingMoveX = (float)mainRect.left;
                m_pendingMoveY = (float)mainRect.top;
            }
        }
    }

    if (!m_data.loaded)
    {
        ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.35f, 1.0f),
                           "Failed to load: %s", m_data.errorMsg.c_str());
        ImGui::End();
        RenderSavePopups();
        RenderCloseConfirmModal();
        RenderRemoveConfirmModal();
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
    const float listW = 250.0f;

    ImGui::BeginChild("##mew_list", ImVec2(listW, 0.0f), true,
                      ImGuiWindowFlags_NoScrollbar);
    RenderMoveList();
    ImGui::EndChild();

    ImGui::SameLine();

    ImGui::BeginChild("##mew_props", ImVec2(0.0f, 0.0f), true,
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
    RenderSubWin_Throws();
    RenderSubWin_Projectiles();
    RenderSubWin_InputSequences();
    RenderSubWin_ParryableMoves();
    RenderSubWin_Dialogues();

    // Animation Manager
    if (m_animMgr)
    {
        if (!m_animMgr->Render())
            m_animMgr.reset();

        // Auto-process new pool entries discovered by Refresh:
        //   1. Rebuild anmbin first (embeds new files; uses current animNameDB for moveList patch).
        //   2. Only if rebuild succeeds: register stem → CRC32 in animNameDB.
        //   3. ForceReload AnimMgr from rebuilt anmbin (or from unchanged disk if rebuild failed).
        // Order matters: we rebuild BEFORE adding to animNameDB so that a failed rebuild
        // doesn't permanently mark the file as "registered" and block future Refresh attempts.
        if (m_animMgr)
        {
            auto newEntries = m_animMgr->TakePendingNewEntries();
            if (!newEntries.empty())
            {
                std::string anmbinErr;
                bool rebuilt = RebuildAnmbin(m_data.folderPath, m_animNameDB, m_data.moves, anmbinErr);

                if (rebuilt)
                {
                    // Embed succeeded — now persist the name→CRC32 mapping.
                    for (const auto& e : newEntries)
                        m_animNameDB.AddEntry(m_data.folderPath, e.name, e.hash);
                }
                else
                {
                    // Embed failed — show error and reset to clean disk state.
                    m_animMgr->SetRebuildError(!anmbinErr.empty() ? anmbinErr : "RebuildAnmbin returned false");
                }

                // Always ForceReload: on success this shows newly embedded entry;
                // on failure this clears the in-memory sentinel so Refresh can retry.
                m_animMgr->ForceReload();
            }
        }
    }

    // Overlay-style dialogs rendered LAST so they appear on top of all sub-windows.
    RenderSavePopups();
    RenderCloseConfirmModal();
    RenderRemoveConfirmModal();

    return m_open;
}

// -------------------------------------------------------------
//  Remove-confirmation modal + [END]-insert-blocked popup
// -------------------------------------------------------------

void MovesetEditorWindow::RenderRemoveConfirmModal()
{
    // Absorb one-frame triggers
    if (m_removeConfirm.pending) { m_removeConfirm.showing = true; m_removeConfirm.pending = false; }
    if (m_endInsertBlocked)      { m_showInsertBlocked = true; m_endInsertBlocked = false; }

    if (!m_removeConfirm.showing && !m_showInsertBlocked) return;

    // Prefer the viewport of the sub-window that triggered the dialog;
    // fall back to the main editor viewport, then the host viewport.
    ImGuiViewport* edVp = nullptr;
    if (m_removeConfirm.showing && m_removeConfirm.callerViewportId != 0)
        edVp = ImGui::FindViewportByID(m_removeConfirm.callerViewportId);
    else if (m_showInsertBlocked && m_insertBlockedViewportId != 0)
        edVp = ImGui::FindViewportByID(m_insertBlockedViewportId);
    if (!edVp && m_viewportId != 0)
        edVp = ImGui::FindViewportByID(m_viewportId);
    if (!edVp) edVp = ImGui::GetMainViewport();
    const ImVec2 vpPos  = edVp->Pos;
    const ImVec2 vpSize = edVp->Size;
    const ImVec2 center(vpPos.x + vpSize.x * 0.5f, vpPos.y + vpSize.y * 0.5f);

    constexpr ImGuiWindowFlags kOvFlags =
        ImGuiWindowFlags_NoTitleBar     | ImGuiWindowFlags_NoResize    |
        ImGuiWindowFlags_NoMove         | ImGuiWindowFlags_NoDocking   |
        ImGuiWindowFlags_NoSavedSettings| ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoNav          | ImGuiWindowFlags_NoFocusOnAppearing;

    ImGui::SetNextWindowViewport(edVp->ID);
    ImGui::SetNextWindowPos(vpPos);
    ImGui::SetNextWindowSize(vpSize);
    ImGui::SetNextWindowBgAlpha(0.28f);
    ImGui::Begin(WinId("##dlg_dim").c_str(), nullptr, kOvFlags);
    ImGui::End();

    const float pad   = ImGui::GetStyle().WindowPadding.x;
    const float lineH = ImGui::GetTextLineHeightWithSpacing();

    ImGui::SetNextWindowViewport(edVp->ID);
    ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowBgAlpha(0.96f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.f);

    if (m_removeConfirm.showing)
    {
        const float msgW = ImGui::CalcTextSize(m_removeConfirm.message, nullptr, false, 280.f).x;
        const float boxW = (msgW > 220.f ? msgW : 220.f) + pad * 2.f + 8.f;
        const float msgH = ImGui::CalcTextSize(m_removeConfirm.message, nullptr, false, 280.f).y;
        const float boxH = msgH + lineH * 2.f + pad * 2.f + 8.f;
        ImGui::SetNextWindowSize(ImVec2(boxW, boxH), ImGuiCond_Always);
        ImGui::Begin(WinId("##dlg_box").c_str(), nullptr, kOvFlags);
        ImGui::PopStyleVar();
        ImGui::SetCursorPosY(ImGui::GetStyle().WindowPadding.y + 4.f);
        ImGui::TextWrapped("%s", m_removeConfirm.message);
        ImGui::Spacing();
        if (ImGui::Button("Remove", ImVec2(100, 0)))
        {
            if (m_removeConfirm.onConfirm) m_removeConfirm.onConfirm();
            m_removeConfirm.onConfirm       = nullptr;
            m_removeConfirm.callerViewportId = 0;
            m_removeConfirm.showing          = false;
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(100, 0)))
        {
            m_removeConfirm.onConfirm       = nullptr;
            m_removeConfirm.callerViewportId = 0;
            m_removeConfirm.showing          = false;
        }
    }
    else // m_showInsertBlocked
    {
        const char* msg  = "Cannot insert at an [END] item.";
        const float boxW = ImGui::CalcTextSize(msg).x + pad * 2.f + 16.f;
        const float boxH = lineH * 2.f + pad * 2.f + 8.f;
        ImGui::SetNextWindowSize(ImVec2(boxW, boxH), ImGuiCond_Always);
        ImGui::Begin(WinId("##dlg_box").c_str(), nullptr, kOvFlags);
        ImGui::PopStyleVar();
        ImGui::SetCursorPosY(ImGui::GetStyle().WindowPadding.y + 4.f);
        ImGui::TextUnformatted(msg);
        ImGui::Spacing();
        if (ImGui::Button("Close", ImVec2(80, 0)))
        {
            m_showInsertBlocked       = false;
            m_insertBlockedViewportId = 0;
        }
    }
    ImGui::End();
}

// -------------------------------------------------------------
//  Requirement viewer floating window
// -------------------------------------------------------------

void MovesetEditorWindow::RenderReqViewWindow()
{
    if (!m_reqView.open) return;

    ImGui::SetNextWindowSize(ImVec2(280.0f, 220.0f), ImGuiCond_FirstUseEver);
    if (ImGui::Begin(WinId("Requirements##reqviewer").c_str(), &m_reqView.open,
                     ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking))
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
    if (ImGui::Begin(WinId("Cancel Extradata##edviewer").c_str(), &m_extradataView.open,
                     ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking))
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
    // + button: add new move
    if (ImGui::Button("+##addmove")) ImGui::OpenPopup("##AddMovePopup");
    ImGui::SameLine();
    ImGui::TextDisabled("Moves (%d)", (int)m_data.moves.size());

    if (ImGui::BeginPopup("##AddMovePopup")) {
        if (ImGui::MenuItem("Create Empty Move")) {
            ParsedMove empty{};
            empty.cancel_idx        = 0xFFFFFFFF;
            empty.cancel2_idx       = 0xFFFFFFFF;
            empty.hit_condition_idx = 0xFFFFFFFF;
            empty.voiceclip_idx     = 0xFFFFFFFF;
            empty.extra_prop_idx    = 0xFFFFFFFF;
            empty.start_prop_idx    = 0xFFFFFFFF;
            empty.end_prop_idx      = 0xFFFFFFFF;
            empty.displayName = "move_" + std::to_string(m_data.moves.size());
            m_data.moves.push_back(empty);
            m_dirty = true;
        }
        if (ImGui::MenuItem("Duplicate Current Move")) {
            if (m_selectedIdx >= 0 && m_selectedIdx < (int)m_data.moves.size()) {
                ParsedMove dup = m_data.moves[m_selectedIdx];
                dup.displayName = dup.displayName + "_copy";
                m_data.moves.push_back(dup);
                m_dirty = true;
            }
        }
        ImGui::EndPopup();
    }
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

static bool BeginPropTable(const char* id = "##pt", bool halfStretch = false)
{
    constexpr ImGuiTableFlags kFlags =
        ImGuiTableFlags_BordersInnerH |
        ImGuiTableFlags_RowBg         |
        ImGuiTableFlags_SizingFixedFit;
    if (!ImGui::BeginTable(id, 2, kFlags)) return false;
    if (halfStretch)
    {
        ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthStretch, 1.0f);
        ImGui::TableSetupColumn("Value",    ImGuiTableColumnFlags_WidthStretch, 1.0f);
    }
    else
    {
        ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, 220.0f);
        ImGui::TableSetupColumn("Value",    ImGuiTableColumnFlags_WidthStretch);
    }
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

static bool RowU32Edit(const char* id, const char* lbl, uint32_t& v,
                       const FieldTooltip& tt = {})
{
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0); ImGui::TextDisabled("%s", lbl); ShowFieldTooltip(tt);
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

static bool RowI32Edit(const char* id, const char* lbl, int32_t& v,
                       const FieldTooltip& tt = {})
{
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0); ImGui::TextDisabled("%s", lbl); ShowFieldTooltip(tt);
    ImGui::TableSetColumnIndex(1);
    ImGui::SetNextItemWidth(-1.0f);
    return ImGui::InputInt(id, &v, 0, 0);
}

static bool RowHex32Edit(const char* id, const char* lbl, uint32_t& v,
                         const FieldTooltip& tt = {})
{
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0); ImGui::TextDisabled("%s", lbl); ShowFieldTooltip(tt);
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

// Same as RowHex32Edit but omits leading zeros (e.g. 0x8000 instead of 0x00008000)
static bool RowHexCompact32Edit(const char* id, const char* lbl, uint32_t& v,
                                const FieldTooltip& tt = {})
{
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0); ImGui::TextDisabled("%s", lbl); ShowFieldTooltip(tt);
    ImGui::TableSetColumnIndex(1);
    ImGui::SetNextItemWidth(-1.0f);
    char buf[14]; snprintf(buf, sizeof(buf), "0x%X", v);
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

static bool RowHex64Edit(const char* id, const char* lbl, uint64_t& v,
                         const FieldTooltip& tt = {})
{
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0); ImGui::TextDisabled("%s", lbl); ShowFieldTooltip(tt);
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

static bool RowU16Edit(const char* id, const char* lbl, uint16_t& v,
                       const FieldTooltip& tt = {})
{
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0); ImGui::TextDisabled("%s", lbl); ShowFieldTooltip(tt);
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

static bool RowI16Edit(const char* id, const char* lbl, int16_t& v,
                       const FieldTooltip& tt = {})
{
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0); ImGui::TextDisabled("%s", lbl); ShowFieldTooltip(tt);
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

static bool RowF32Edit(const char* id, const char* lbl, float& v,
                       const FieldTooltip& tt = {})
{
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0); ImGui::TextDisabled("%s", lbl); ShowFieldTooltip(tt);
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
            ImGui::SeparatorText("General");
            ImGui::Spacing();
            RenderSection_Overview(m, m_dirty);
            ImGui::Spacing();
            ImGui::SeparatorText("Hitboxes");
            ImGui::Spacing();
            RenderSection_Hitboxes(m, m_dirty);
            ImGui::EndTabItem();
        }
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

// -- Section: Overview (3-column block grid) -----------------------------------
void MovesetEditorWindow::RenderSection_Overview(ParsedMove& m, bool& dirty)
{
    using namespace MoveLabel;

    // -- Pre-compute groups (used for validity checks and click navigation) ----
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

    // -- Validity flags -------------------------------------------------------
    const uint16_t transU16       = m.transition;
    const bool     transIsGeneric = (transU16 >= 0x8000);
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
    const bool transValid     = (transResolvedIdx >= 0);
    const bool cancelValid    = (m.cancel_idx        != 0xFFFFFFFF) && (FindGroupOuter(cancelGroups,   m.cancel_idx)        >= 0);
    const bool hitCondValid   = (m.hit_condition_idx != 0xFFFFFFFF) && (FindGroupOuter(hitCondGroups,  m.hit_condition_idx) >= 0);
    const bool voiceclipValid = (m.voiceclip_idx     != 0xFFFFFFFF) && (m.voiceclip_idx < m_data.voiceclipBlock.size());
    const bool epValid        = (m.extra_prop_idx    != 0xFFFFFFFF) && (FindGroupOuter(epGroups,       m.extra_prop_idx)    >= 0);
    const bool spValid        = (m.start_prop_idx    != 0xFFFFFFFF) && (FindGroupOuter(spGroups,       m.start_prop_idx)    >= 0);
    const bool npValid        = (m.end_prop_idx      != 0xFFFFFFFF) && (FindGroupOuter(npGroups,       m.end_prop_idx)      >= 0);

    // -- Navigation click flags -----------------------------------------------
    bool clickCancel    = false, clickTrans     = false, clickHitCond  = false;
    bool clickVoiceclip = false, clickExtraProp = false;
    bool clickStartProp = false, clickEndProp   = false;

    // -- 2-column layout helpers ----------------------------------------------
    static constexpr ImVec4 kGreen = {0.55f, 0.85f, 0.55f, 1.0f};
    static constexpr ImVec4 kPink  = {1.00f, 0.50f, 0.65f, 1.0f};
    static constexpr ImVec4 kGray  = {0.50f, 0.50f, 0.50f, 1.0f};

    // Colored label text for navigation fields (non-clickable; navigation via Go button)
    auto NavLabel = [&](const char* lbl, bool valid, const FieldTooltip& tt = {})
    {
        ImGui::PushStyleColor(ImGuiCol_Text, valid ? kGreen : kPink);
        ImGui::TextUnformatted(lbl);
        ImGui::PopStyleColor();
        ShowFieldTooltip(tt);
    };

    // "Go →" button drawn with ImDrawList (font-independent arrow triangle).
    const float kArrowW = ImGui::GetFrameHeight() * 0.38f;
    const float kBtnW   = ImGui::CalcTextSize("Go ").x + kArrowW
                        + ImGui::GetStyle().FramePadding.x * 2.0f + 4.0f;

    // "Go →" button: always rendered; disabled (greyed) when !valid.
    // Returns true only when clicked and valid.
    auto GoButton = [&](const char* id, bool valid) -> bool
    {
        if (!valid) ImGui::BeginDisabled();

        const float  h  = ImGui::GetFrameHeight();
        bool clicked    = ImGui::InvisibleButton(id, ImVec2(kBtnW, h));
        const ImVec2 p0 = ImGui::GetItemRectMin();
        const ImVec2 p1 = ImGui::GetItemRectMax();
        const bool   hov= ImGui::IsItemHovered();
        const bool   act= ImGui::IsItemActive();

        // Button background (GetColorU32 respects BeginDisabled alpha automatically)
        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->AddRectFilled(p0, p1,
            ImGui::GetColorU32(act ? ImGuiCol_ButtonActive
                                   : hov ? ImGuiCol_ButtonHovered
                                         : ImGuiCol_Button),
            ImGui::GetStyle().FrameRounding);
        dl->AddRect(p0, p1,
            ImGui::GetColorU32(ImGuiCol_Border),
            ImGui::GetStyle().FrameRounding);

        // "Go " text + right-pointing triangle, centred in button
        const ImU32  col    = ImGui::GetColorU32(ImGuiCol_Text);
        const char*  txt    = "Go ";
        const ImVec2 txtSz  = ImGui::CalcTextSize(txt);
        const float  totalW = txtSz.x + kArrowW;
        const float  startX = p0.x + (kBtnW - totalW) * 0.5f;
        const float  midY   = (p0.y + p1.y) * 0.5f;
        const float  arrowH = h * 0.30f;

        dl->AddText(ImVec2(startX, midY - txtSz.y * 0.5f), col, txt);
        dl->AddTriangleFilled(
            ImVec2(startX + txtSz.x,          midY - arrowH * 0.5f),
            ImVec2(startX + txtSz.x,          midY + arrowH * 0.5f),
            ImVec2(startX + txtSz.x + kArrowW, midY),
            col);

        if (!valid) ImGui::EndDisabled();
        return clicked && valid;
    };

    // -- 2-column layout: left col (fields 1-15), right col (fields 16-28) -----
    // Outer table splits the area into two equal halves.
    // Each half uses an inner table with label(50%) | input(50%).
    constexpr ImGuiTableFlags kOuterF = ImGuiTableFlags_PadOuterX;
    constexpr ImGuiTableFlags kInnerF = ImGuiTableFlags_BordersInnerH
                                      | ImGuiTableFlags_RowBg
                                      | ImGuiTableFlags_SizingStretchSame;

    if (!ImGui::BeginTable("##gen_outer", 2, kOuterF)) return;
    ImGui::TableSetupColumn("left",  ImGuiTableColumnFlags_WidthStretch, 1.0f);
    ImGui::TableSetupColumn("right", ImGuiTableColumnFlags_WidthStretch, 1.0f);
    ImGui::TableNextRow();

    // Helper: begin a row in the current inner table, put label in col0, set col1 active.
    // Returns with the cursor in the input column (1).
    auto FieldRow = [](const char* lbl, const FieldTooltip& tt = {})
    {
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextDisabled("%s", lbl);
        ShowFieldTooltip(tt);
        ImGui::TableSetColumnIndex(1);
    };

    // =====================================================================
    // LEFT COLUMN — fields 1-15
    // =====================================================================
    ImGui::TableNextColumn();
    if (ImGui::BeginTable("##gen_left", 2, kInnerF))
    {
        ImGui::TableSetupColumn("lbl", ImGuiTableColumnFlags_WidthStretch, 1.0f);
        ImGui::TableSetupColumn("inp", ImGuiTableColumnFlags_WidthStretch, 1.0f);

        // 1. name
        FieldRow(Name, FieldTT::Move::Name);
        {
            char nameBuf[256]; snprintf(nameBuf, sizeof(nameBuf), "%s", m.displayName.c_str());
            ImGui::SetNextItemWidth(-1.0f);
            ImGui::InputText("##move_name", nameBuf, sizeof(nameBuf));
            if (ImGui::IsItemDeactivatedAfterEdit())
            {
                m.displayName = nameBuf;
                if (nameBuf[0] != '\0') m_customNames[m_selectedIdx] = m.displayName;
                else                    m_customNames.erase(m_selectedIdx);
                SaveEditorDatas();
            }
        }

        // 2. name_key
        FieldRow(NameKey, FieldTT::Move::NameKey);
        { char buf[14]; snprintf(buf, sizeof(buf), "0x%08X", m.name_key);
          ImGui::SetNextItemWidth(-1.0f);
          ImGui::InputText("##namekey_ro", buf, sizeof(buf), ImGuiInputTextFlags_ReadOnly); }

        // 3. anim_key
        {
            const bool db = m_animNameDB.IsLoaded();
            if (m_animKeyBufIdx != m_selectedIdx)
            {
                m_animKeyBufIdx = m_selectedIdx;
                if (db)
                {
                    std::string animName = m_animNameDB.AnimKeyToName(m.anim_key);
                    if (!animName.empty())
                        snprintf(m_animKeyBuf, sizeof(m_animKeyBuf), "%s", animName.c_str());
                    else
                        snprintf(m_animKeyBuf, sizeof(m_animKeyBuf), "0x%08X", m.anim_key);
                }
                else
                    snprintf(m_animKeyBuf, sizeof(m_animKeyBuf), "0x%08X", m.anim_key);
            }
            uint32_t resolvedKey = 0;
            const bool animValid = db && m_animNameDB.NameToAnimKey(m_animKeyBuf, resolvedKey);
            const bool isComRef  = db && !animValid
                                 && m_animKeyBuf[0] == '0'
                                 && (m_animKeyBuf[1] == 'x' || m_animKeyBuf[1] == 'X');
            static constexpr ImVec4 kYellow = {1.00f, 0.85f, 0.30f, 1.0f};
            const ImVec4& lblCol = !db ? kGray : (animValid ? kGreen : (isComRef ? kYellow : kPink));

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::PushStyleColor(ImGuiCol_Text, lblCol);
            ImGui::TextUnformatted(AnimKey);
            ImGui::PopStyleColor();
            ShowFieldTooltip(FieldTT::Move::AnimKey);
            ImGui::TableSetColumnIndex(1);
            ImGui::SetNextItemWidth(-(kBtnW + ImGui::GetStyle().ItemSpacing.x));
            if (db)
            {
                if (ImGui::InputText("##animkeyinput", m_animKeyBuf, sizeof(m_animKeyBuf)))
                {
                    uint32_t newKey = 0;
                    if (m_animNameDB.NameToAnimKey(m_animKeyBuf, newKey)) {
                        m.anim_key = newKey; dirty = true;
                    } else if (m_animKeyBuf[0]=='0' && (m_animKeyBuf[1]=='x'||m_animKeyBuf[1]=='X')) {
                        char* end;
                        uint32_t v = (uint32_t)strtoul(m_animKeyBuf + 2, &end, 16);
                        if (end != m_animKeyBuf + 2) { m.anim_key = v; dirty = true; }
                    }
                }
            }
            else
            {
                char roBuf[32]; snprintf(roBuf, sizeof(roBuf), "0x%08X", m.anim_key);
                ImGui::InputText("##animkey_ro", roBuf, sizeof(roBuf), ImGuiInputTextFlags_ReadOnly);
            }
            ImGui::SameLine();
            if (GoButton("Go \xe2\x86\x92##animkey_go", animValid))
            {
                if (!m_animMgr)
                {
                    m_animMgr = std::make_unique<AnimationManagerWindow>(
                                    m_data.folderPath, m_movesetName, m_uid);
                    std::vector<uint32_t> keys;
                    keys.reserve(m_data.moves.size());
                    for (const auto& mv : m_data.moves) keys.push_back(mv.anim_key);
                    m_animMgr->SetMotbinAnimKeys(keys);
                    m_animMgr->SetAnimNameDB(&m_animNameDB);
                    m_animMgr->SetCharaCode(m_data.charaCode);
                }
                if (m_selectedIdx >= 0 && m_selectedIdx < (int)m_data.moves.size())
                    m_animMgr->NavigateByMotbinKey(0, m_data.moves[m_selectedIdx].anim_key);
            }
        }

        // 4. skeleton_id
        FieldRow(SkeletonId, FieldTT::Move::SkeletonId);
        { char buf[14]; snprintf(buf, sizeof(buf), "0x%08X", m.anmbin_body_sub_idx);
          ImGui::SetNextItemWidth(-1.0f);
          ImGui::InputText("##skelid_ro", buf, sizeof(buf), ImGuiInputTextFlags_ReadOnly); }

        // 5. vuln
        FieldRow(Vuln, FieldTT::Move::Vuln);
        { ImGui::SetNextItemWidth(-1.0f);
          int tmp = (int)m.vuln;
          if (ImGui::InputInt("##vuln", &tmp, 0, 0)) { m.vuln = (uint32_t)tmp; dirty = true; } }

        // 6. hitlevel
        FieldRow(Hitlevel, FieldTT::Move::Hitlevel);
        { ImGui::SetNextItemWidth(-1.0f);
          int tmp = (int)m.hitlevel;
          if (ImGui::InputInt("##hitlevel", &tmp, 0, 0)) { m.hitlevel = (uint32_t)tmp; dirty = true; } }

        // 7. cancel_idx
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0); NavLabel(CancelIdx, cancelValid, FieldTT::Move::CancelIdx);
        ImGui::TableSetColumnIndex(1);
        { ImGui::SetNextItemWidth(-(kBtnW + ImGui::GetStyle().ItemSpacing.x));
          int tmp = (m.cancel_idx == 0xFFFFFFFF) ? -1 : (int)m.cancel_idx;
          if (ImGui::InputInt("##cancel_idx", &tmp, 0, 0)) { m.cancel_idx = (tmp < 0) ? 0xFFFFFFFF : (uint32_t)tmp; dirty = true; }
          ImGui::SameLine(); if (GoButton("Go \xe2\x86\x92##cancel_go", cancelValid)) clickCancel = true; }

        // 8. transition
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0); NavLabel(Transition, transValid, FieldTT::Move::Transition);
        ImGui::TableSetColumnIndex(1);
        { ImGui::SetNextItemWidth(-(kBtnW + ImGui::GetStyle().ItemSpacing.x));
          int tmp = (int)(uint32_t)m.transition;
          if (ImGui::InputInt("##transition", &tmp, 0, 0)) { m.transition = (uint16_t)((uint32_t)tmp & 0xFFFFu); dirty = true; }
          ImGui::SameLine(); if (GoButton("Go \xe2\x86\x92##trans_go", transValid)) clickTrans = true; }

        // 9. anim_len
        FieldRow(AnimLen, FieldTT::Move::AnimLen);
        { ImGui::SetNextItemWidth(-1.0f);
          if (ImGui::InputInt("##anim_len", &m.anim_len, 0, 0)) dirty = true; }

        // 10. hit_condition_idx
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0); NavLabel(HitCondIdx, hitCondValid, FieldTT::Move::HitCondIdx);
        ImGui::TableSetColumnIndex(1);
        { ImGui::SetNextItemWidth(-(kBtnW + ImGui::GetStyle().ItemSpacing.x));
          int tmp = (m.hit_condition_idx == 0xFFFFFFFF) ? -1 : (int)m.hit_condition_idx;
          if (ImGui::InputInt("##hitcond_idx", &tmp, 0, 0)) { m.hit_condition_idx = (tmp < 0) ? 0xFFFFFFFF : (uint32_t)tmp; dirty = true; }
          ImGui::SameLine(); if (GoButton("Go \xe2\x86\x92##hc_go", hitCondValid)) clickHitCond = true; }

        // 11. voiceclip_idx
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0); NavLabel(VoiceclipIdx, voiceclipValid, FieldTT::Move::VoiceclipIdx);
        ImGui::TableSetColumnIndex(1);
        { ImGui::SetNextItemWidth(-(kBtnW + ImGui::GetStyle().ItemSpacing.x));
          int tmp = (m.voiceclip_idx == 0xFFFFFFFF) ? -1 : (int)m.voiceclip_idx;
          if (ImGui::InputInt("##voice_idx", &tmp, 0, 0)) { m.voiceclip_idx = (tmp < 0) ? 0xFFFFFFFF : (uint32_t)tmp; dirty = true; }
          ImGui::SameLine(); if (GoButton("Go \xe2\x86\x92##vc_go", voiceclipValid)) clickVoiceclip = true; }

        // 12. extra_prop_idx
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0); NavLabel(ExtraPropIdx, epValid, FieldTT::Move::ExtraPropIdx);
        ImGui::TableSetColumnIndex(1);
        { ImGui::SetNextItemWidth(-(kBtnW + ImGui::GetStyle().ItemSpacing.x));
          int tmp = (m.extra_prop_idx == 0xFFFFFFFF) ? -1 : (int)m.extra_prop_idx;
          if (ImGui::InputInt("##eprop_idx", &tmp, 0, 0)) { m.extra_prop_idx = (tmp < 0) ? 0xFFFFFFFF : (uint32_t)tmp; dirty = true; }
          ImGui::SameLine(); if (GoButton("Go \xe2\x86\x92##ep_go", epValid)) clickExtraProp = true; }

        // 13. start_prop_idx
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0); NavLabel(StartPropIdx, spValid, FieldTT::Move::StartPropIdx);
        ImGui::TableSetColumnIndex(1);
        { ImGui::SetNextItemWidth(-(kBtnW + ImGui::GetStyle().ItemSpacing.x));
          int tmp = (m.start_prop_idx == 0xFFFFFFFF) ? -1 : (int)m.start_prop_idx;
          if (ImGui::InputInt("##sprop_idx", &tmp, 0, 0)) { m.start_prop_idx = (tmp < 0) ? 0xFFFFFFFF : (uint32_t)tmp; dirty = true; }
          ImGui::SameLine(); if (GoButton("Go \xe2\x86\x92##sp_go", spValid)) clickStartProp = true; }

        // 14. end_prop_idx
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0); NavLabel(EndPropIdx, npValid, FieldTT::Move::EndPropIdx);
        ImGui::TableSetColumnIndex(1);
        { ImGui::SetNextItemWidth(-(kBtnW + ImGui::GetStyle().ItemSpacing.x));
          int tmp = (m.end_prop_idx == 0xFFFFFFFF) ? -1 : (int)m.end_prop_idx;
          if (ImGui::InputInt("##nprop_idx", &tmp, 0, 0)) { m.end_prop_idx = (tmp < 0) ? 0xFFFFFFFF : (uint32_t)tmp; dirty = true; }
          ImGui::SameLine(); if (GoButton("Go \xe2\x86\x92##np_go", npValid)) clickEndProp = true; }

        // 15. _0xCE
        FieldRow(CE, FieldTT::Move::CE);
        { ImGui::SetNextItemWidth(-1.0f);
          int tmp = (int)m._0xCE;
          if (ImGui::InputInt("##0xCE", &tmp, 0, 0)) { m._0xCE = (int16_t)tmp; dirty = true; } }

        ImGui::EndTable();
    } // end left column inner table

    // =====================================================================
    // RIGHT COLUMN — fields 16-28
    // =====================================================================
    ImGui::TableNextColumn();
    if (ImGui::BeginTable("##gen_right", 2, kInnerF))
    {
        ImGui::TableSetupColumn("lbl", ImGuiTableColumnFlags_WidthStretch, 1.0f);
        ImGui::TableSetupColumn("inp", ImGuiTableColumnFlags_WidthStretch, 1.0f);

        // 16. t_char_id
        FieldRow(TCharId, FieldTT::Move::TCharId);
        { char buf[14]; snprintf(buf, sizeof(buf), "0x%08X", m.ordinal_id2);
          ImGui::SetNextItemWidth(-1.0f);
          ImGui::InputText("##t_char_id", buf, sizeof(buf));
          if (ImGui::IsItemDeactivatedAfterEdit())
          {
              const char* p = buf; if (p[0]=='0' && (p[1]=='x'||p[1]=='X')) p += 2;
              m.ordinal_id2 = (uint32_t)strtoul(p, nullptr, 16); dirty = true;
          } }

        // 17. ordinal_id
        FieldRow(OrdinalId, FieldTT::Move::OrdinalId);
        { char buf[14]; snprintf(buf, sizeof(buf), "0x%08X", m.moveId);
          ImGui::SetNextItemWidth(-1.0f);
          ImGui::InputText("##ordinal_id", buf, sizeof(buf));
          if (ImGui::IsItemDeactivatedAfterEdit())
          {
              const char* p = buf; if (p[0]=='0' && (p[1]=='x'||p[1]=='X')) p += 2;
              m.moveId = (uint32_t)strtoul(p, nullptr, 16); dirty = true;
          } }

        // 18. _0x118
        FieldRow(F0x118, FieldTT::Move::F0x118);
        { ImGui::SetNextItemWidth(-1.0f);
          int tmp = (int)m._0x118;
          if (ImGui::InputInt("##0x118", &tmp, 0, 0)) { m._0x118 = (uint32_t)tmp; dirty = true; } }

        // 19. _0x11C
        FieldRow(F0x11C, FieldTT::Move::F0x11C);
        { ImGui::SetNextItemWidth(-1.0f);
          int tmp = (int)m._0x11C;
          if (ImGui::InputInt("##0x11C", &tmp, 0, 0)) { m._0x11C = (uint32_t)tmp; dirty = true; } }

        // 20. airborne_start
        FieldRow(AirborneStart, FieldTT::Move::AirborneStart);
        { ImGui::SetNextItemWidth(-1.0f);
          int tmp = (int)m.airborne_start;
          if (ImGui::InputInt("##airborne_start", &tmp, 0, 0)) { m.airborne_start = (uint32_t)tmp; dirty = true; } }

        // 21. airborne_end
        FieldRow(AirborneEnd, FieldTT::Move::AirborneEnd);
        { ImGui::SetNextItemWidth(-1.0f);
          int tmp = (int)m.airborne_end;
          if (ImGui::InputInt("##airborne_end", &tmp, 0, 0)) { m.airborne_end = (uint32_t)tmp; dirty = true; } }

        // 22. ground_fall
        FieldRow(GroundFall, FieldTT::Move::GroundFall);
        { ImGui::SetNextItemWidth(-1.0f);
          int tmp = (int)m.ground_fall;
          if (ImGui::InputInt("##ground_fall", &tmp, 0, 0)) { m.ground_fall = (uint32_t)tmp; dirty = true; } }

        // 23. _0x154
        FieldRow(F0x154, FieldTT::Move::F0x154);
        { ImGui::SetNextItemWidth(-1.0f);
          int tmp = (int)m._0x154;
          if (ImGui::InputInt("##0x154", &tmp, 0, 0)) { m._0x154 = (uint32_t)tmp; dirty = true; } }

        // 24. u6
        FieldRow(U6, FieldTT::Move::U6);
        { ImGui::SetNextItemWidth(-1.0f);
          int tmp = (int)m.u6;
          if (ImGui::InputInt("##u6", &tmp, 0, 0)) { m.u6 = (uint32_t)tmp; dirty = true; } }

        // 25. u15
        FieldRow(U15, FieldTT::Move::U15);
        { char buf[14]; snprintf(buf, sizeof(buf), "0x%08X", m.u15);
          ImGui::SetNextItemWidth(-1.0f);
          ImGui::InputText("##u15", buf, sizeof(buf));
          if (ImGui::IsItemDeactivatedAfterEdit())
          {
              const char* p = buf; if (p[0]=='0' && (p[1]=='x'||p[1]=='X')) p += 2;
              m.u15 = (uint32_t)strtoul(p, nullptr, 16); dirty = true;
          } }

        // 26. collision
        FieldRow(Collision, FieldTT::Move::Collision);
        { ImGui::SetNextItemWidth(-1.0f);
          int16_t col16 = static_cast<int16_t>(m.collision);
          int tmp = (int)col16;
          if (ImGui::InputInt("##collision", &tmp, 0, 0)) { m.collision = static_cast<uint16_t>((int16_t)tmp); dirty = true; } }

        // 27. distance
        FieldRow(Distance, FieldTT::Move::Distance);
        { ImGui::SetNextItemWidth(-1.0f);
          int16_t dist16 = static_cast<int16_t>(m.distance);
          int tmp = (int)dist16;
          if (ImGui::InputInt("##distance", &tmp, 0, 0)) { m.distance = static_cast<uint16_t>((int16_t)tmp); dirty = true; } }

        // 28. u18
        FieldRow(U18, FieldTT::Move::U18);
        { ImGui::SetNextItemWidth(-1.0f);
          int tmp = (int)m.u18;
          if (ImGui::InputInt("##u18", &tmp, 0, 0)) { m.u18 = (uint32_t)tmp; dirty = true; } }

        ImGui::EndTable();
    } // end right column inner table

    ImGui::EndTable();


    // -- Index navigation handlers (reuse pre-computed groups) ----------------
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
        m_selectedIdx           = transResolvedIdx;
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

    // Read-only row helpers — visually identical to editable InputText but non-editable
    auto RowHex64RO = [](const char* id, const char* lbl, uint64_t v) {
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0); ImGui::TextDisabled("%s", lbl);
        ImGui::TableSetColumnIndex(1);
        ImGui::SetNextItemWidth(-1.0f);
        char buf[22]; snprintf(buf, sizeof(buf), "0x%016llX", static_cast<unsigned long long>(v));
        ImGui::InputText(id, buf, sizeof(buf), ImGuiInputTextFlags_ReadOnly);
    };
    auto RowU32RO = [](const char* id, const char* lbl, uint32_t v) {
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0); ImGui::TextDisabled("%s", lbl);
        ImGui::TableSetColumnIndex(1);
        ImGui::SetNextItemWidth(-1.0f);
        char buf[16]; snprintf(buf, sizeof(buf), "%u", v);
        ImGui::InputText(id, buf, sizeof(buf), ImGuiInputTextFlags_ReadOnly);
    };

    if (RowHex64Edit("##u1", MoveLabel::U1, m.u1)) dirty = true;
    if (RowHex64Edit("##u2", MoveLabel::U2, m.u2)) dirty = true;
    if (RowHex64Edit("##u3", MoveLabel::U3, m.u3)) dirty = true;
    if (RowHex64Edit("##u4", MoveLabel::U4, m.u4)) dirty = true;
    RowHex64RO("##enc_namekey_ro",  MoveLabel::EncNameKey,     m.encrypted_name_key);
    RowHex64RO("##name_enckey_ro",  MoveLabel::NameEncKey,     m.name_encryption_key);
    RowHex64RO("##enc_animkey_ro",  MoveLabel::EncAnimKey,     m.encrypted_anim_key);
    RowHex64RO("##anim_enckey_ro",  MoveLabel::AnimEncKey,     m.anim_encryption_key);
    RowU32RO  ("##anmbin_idx_ro",   MoveLabel::AnmbinBodyIdx,  m.anmbin_body_idx);
    RowHex64RO("##enc_vuln_ro",     MoveLabel::EncVuln,        m.encrypted_vuln);
    RowHex64RO("##vuln_enckey_ro",  MoveLabel::VulnEncKey,     m.vuln_encryption_key);
    RowHex64RO("##enc_hitlev_ro",   MoveLabel::EncHitlevel,    m.encrypted_hitlevel);
    RowHex64RO("##hitlev_enckey_ro",MoveLabel::HitlevelEncKey, m.hitlevel_encryption_key);
    RowHex64RO("##enc_ordid2_ro",   MoveLabel::EncOrdinalId2,  m.encrypted_ordinal_id2);
    RowHex64RO("##ordid2_key_ro",   MoveLabel::OrdinalId2Key,  m.ordinal_id2_enc_key);
    RowHex64RO("##enc_ordid_ro",    MoveLabel::EncOrdinalId,   m.encrypted_ordinal_id);
    RowHex64RO("##ordid_enckey_ro", MoveLabel::OrdinalEncKey,  m.ordinal_encryption_key);
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
            "RELATED FLOATS: %.3f, %.3f, %.3f, %.3f, %.3f, %.3f, %.3f, %.3f, %.3f###hb%d",
            h + 1, start, last, loc,
            fl[0], fl[1], fl[2], fl[3], fl[4], fl[5], fl[6], fl[7], fl[8],
            h);

        if (ImGui::CollapsingHeader(header))
        {
            ImGui::Indent(16.0f);

            if (BeginPropTable("##hbdt", true))
            {
                char id[32];
                snprintf(id, sizeof(id), "##hb%d_start", h);
                if (RowU32Edit(id, HitboxLabel::ActiveStart, m.hitbox_active_start[h], FieldTT::Hitbox::ActiveStart)) dirty = true;
                snprintf(id, sizeof(id), "##hb%d_last", h);
                if (RowU32Edit(id, HitboxLabel::ActiveLast,  m.hitbox_active_last[h],  FieldTT::Hitbox::ActiveLast))  dirty = true;
                snprintf(id, sizeof(id), "##hb%d_loc", h);
                if (RowHex32Edit(id, HitboxLabel::Location,  m.hitbox_location[h],     FieldTT::Hitbox::Location))    dirty = true;
                for (int f = 0; f < 9; ++f)
                {
                    snprintf(id, sizeof(id), "##hb%d_f%d", h, f);
                    char flbl[16]; snprintf(flbl, sizeof(flbl), HitboxLabel::FloatFmt, f);
                    if (RowF32Edit(id, flbl, m.hitbox_floats[h][f], FieldTT::Hitbox::Float[f])) dirty = true;
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
            if (ImGui::MenuItem("Dialogues",        nullptr, m_dialogueWinOpen))   m_dialogueWinOpen     = !m_dialogueWinOpen;
            ImGui::EndMenu();
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Tools"))
    {
        bool animMgrOpen = m_animMgr != nullptr;
        if (ImGui::MenuItem("Animation Manager", nullptr, animMgrOpen))
        {
            if (!m_animMgr)
            {
                m_animMgr = std::make_unique<AnimationManagerWindow>(
                                m_data.folderPath, m_movesetName, m_uid);
                std::vector<uint32_t> keys;
                keys.reserve(m_data.moves.size());
                for (const auto& mv : m_data.moves) keys.push_back(mv.anim_key);
                m_animMgr->SetMotbinAnimKeys(keys);
                m_animMgr->SetAnimNameDB(&m_animNameDB);
                m_animMgr->SetCharaCode(m_data.charaCode);
            }
            else
                m_animMgr.reset();
        }
        ImGui::EndMenu();
    }

    ImGui::EndMenuBar();
}

// -------------------------------------------------------------
//  Editor-only persistent data (.tkedit/editor_datas.json)
// -------------------------------------------------------------

// -------------------------------------------------------------
//  TryInitAnimNameDB
//  Load .tkedit/anim_names.json if it exists, otherwise build
//  it from moveset.anmbin + motbin anim_keys and save.
// -------------------------------------------------------------

void MovesetEditorWindow::TryInitAnimNameDB()
{
    if (!m_data.loaded) return;

    std::string anmbinPath = m_data.folderPath;
    if (!anmbinPath.empty() && anmbinPath.back() != '\\' && anmbinPath.back() != '/')
        anmbinPath += '\\';
    anmbinPath += "moveset.anmbin";

    if (m_animNameDB.Load(m_data.folderPath))
    {
        // JSON loaded. Scan pool[0] for embedded entries not yet in DB.
        // This recovers custom animations (e.g. "testanim") that were added via Refresh
        // but not persisted to JSON due to a prior bug (Save() excluded non-anim_N names).
        // Recovered entries get a pool-index fallback name ("anim_K") which is functional;
        // newly added files will use the correct custom name via the fixed Save().
        AnmbinData anmbin = LoadAnmbin(anmbinPath);
        if (anmbin.loaded)
        {
            const auto& pool = anmbin.pool[0];
            for (int j = 0; j < (int)pool.size(); ++j)
            {
                if (pool[j].animDataPtr == 0) continue; // com ref, already named

                // Build expected name for this pool index.
                char name[64];
                if (!m_data.charaCode.empty())
                    snprintf(name, sizeof(name), "anim_%s_%d", m_data.charaCode.c_str(), j);
                else
                    snprintf(name, sizeof(name), "anim_%d", j);

                // If the index-based name is already in DB, this entry is covered.
                // (Do NOT check by hash here — pool animKey != motbin anim_key for original
                //  game entries, so an hash-based check would always miss and corrupt the JSON.)
                uint32_t existingKey;
                if (m_animNameDB.NameToAnimKey(name, existingKey)) continue;

                // Also skip if the hash is already known under a custom name (e.g. "mytest").
                uint32_t h = static_cast<uint32_t>(pool[j].animKey & 0xFFFFFFFF);
                if (!m_animNameDB.AnimKeyToName(h).empty()) continue;

                // Genuinely missing entry (custom file not yet in DB): add as fallback.
                m_animNameDB.AddEntry(m_data.folderPath, name, h);
            }
        }
        return;
    }

    // JSON not found — build it from anmbin + motbin anim_keys and save.
    AnmbinData anmbin = LoadAnmbin(anmbinPath);
    if (anmbin.loaded)
    {
        std::vector<uint32_t> keys;
        keys.reserve(m_data.moves.size());
        for (const auto& mv : m_data.moves)
            keys.push_back(mv.anim_key);
        m_animNameDB.BuildAndSave(m_data.folderPath, anmbin, keys, m_data.charaCode);
    }
}

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
    m_saveState = SaveState::Saving;

    // Run save on a background thread so the UI keeps rendering.
    // The dim overlay blocks user interaction while the future runs.
    m_saveFuture = std::async(std::launch::async, [this]()
    {
        SaveMotbin(m_data);
        if (m_animNameDB.IsLoaded())
        {
            std::string anmbinErr;
            RebuildAnmbin(m_data.folderPath, m_animNameDB, m_data.moves, anmbinErr);
        }
    });
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
    if (!m_pendingClose) return;

    ImGuiViewport* edVp = (m_viewportId != 0) ? ImGui::FindViewportByID(m_viewportId) : nullptr;
    if (!edVp) edVp = ImGui::GetMainViewport();
    const ImVec2 vpPos  = edVp->Pos;
    const ImVec2 vpSize = edVp->Size;
    const ImVec2 center(vpPos.x + vpSize.x * 0.5f, vpPos.y + vpSize.y * 0.5f);

    constexpr ImGuiWindowFlags kOvFlags =
        ImGuiWindowFlags_NoTitleBar     | ImGuiWindowFlags_NoResize    |
        ImGuiWindowFlags_NoMove         | ImGuiWindowFlags_NoDocking   |
        ImGuiWindowFlags_NoSavedSettings| ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoNav          | ImGuiWindowFlags_NoFocusOnAppearing;

    ImGui::SetNextWindowViewport(edVp->ID);
    ImGui::SetNextWindowPos(vpPos);
    ImGui::SetNextWindowSize(vpSize);
    ImGui::SetNextWindowBgAlpha(0.28f);
    ImGui::Begin(WinId("##cls_dim").c_str(), nullptr, kOvFlags);
    ImGui::End();

    const float pad   = ImGui::GetStyle().WindowPadding.x;
    const float lineH = ImGui::GetTextLineHeightWithSpacing();
    const float boxW  = 260.f;
    const float boxH  = lineH * 4.f + pad * 2.f + 8.f;

    ImGui::SetNextWindowViewport(edVp->ID);
    ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(boxW, boxH), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.96f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.f);
    ImGui::Begin(WinId("##cls_box").c_str(), nullptr, kOvFlags);
    ImGui::PopStyleVar();
    ImGui::SetCursorPosY(ImGui::GetStyle().WindowPadding.y + 4.f);
    ImGui::Text("You have unsaved changes.");
    ImGui::Text("Save before closing?");
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    if (ImGui::Button("Save", ImVec2(76, 0)))
    {
        if (SaveMotbin(m_data)) m_dirty = false;
        if (m_animNameDB.IsLoaded())
        {
            std::string anmbinErr;
            RebuildAnmbin(m_data.folderPath, m_animNameDB, m_data.moves, anmbinErr);
        }
        m_open = false;
        m_pendingClose = false;
    }
    ImGui::SameLine();
    if (ImGui::Button("Don't Save", ImVec2(76, 0)))
    {
        m_open = false;
        m_pendingClose = false;
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(76, 0)))
        m_pendingClose = false;
    ImGui::End();
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

    ImGuiViewport* edVp = (m_viewportId != 0) ? ImGui::FindViewportByID(m_viewportId) : nullptr;
    if (!edVp) edVp = ImGui::GetMainViewport();
    const ImVec2 vpPos  = edVp->Pos;
    const ImVec2 vpSize = edVp->Size;
    const ImVec2 center(vpPos.x + vpSize.x * 0.5f, vpPos.y + vpSize.y * 0.5f);

    constexpr ImGuiWindowFlags kOvFlags =
        ImGuiWindowFlags_NoTitleBar     | ImGuiWindowFlags_NoResize    |
        ImGuiWindowFlags_NoMove         | ImGuiWindowFlags_NoDocking   |
        ImGuiWindowFlags_NoSavedSettings| ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoNav          | ImGuiWindowFlags_NoFocusOnAppearing;

    // -- Dim background (blocks input while saving/done) --
    ImGui::SetNextWindowViewport(edVp->ID);
    ImGui::SetNextWindowPos(vpPos);
    ImGui::SetNextWindowSize(vpSize);
    ImGui::SetNextWindowBgAlpha(0.28f);
    ImGui::Begin(WinId("##sv_dim").c_str(), nullptr, kOvFlags);
    ImGui::End();

    const float pad   = ImGui::GetStyle().WindowPadding.x;
    const float lineH = ImGui::GetTextLineHeightWithSpacing();

    if (m_saveState == SaveState::Saving)
    {
        // Poll the background save task; animate a spinner while waiting.
        bool done = m_saveFuture.valid() &&
                    m_saveFuture.wait_for(std::chrono::seconds(0)) == std::future_status::ready;

        static const char* kSpin = "|/-\\";
        const int spinIdx = done ? 0 : (static_cast<int>(ImGui::GetTime() * 8.0) & 3);
        char spinMsg[64];
        snprintf(spinMsg, sizeof(spinMsg), "%c  Saving...", kSpin[spinIdx]);
        const float boxW = ImGui::CalcTextSize(spinMsg).x + pad * 2.f + 16.f;

        ImGui::SetNextWindowViewport(edVp->ID);
        ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(boxW, lineH + pad * 2.f + 8.f), ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.96f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.f);
        ImGui::Begin(WinId("##sv_box").c_str(), nullptr, kOvFlags);
        ImGui::PopStyleVar();
        ImGui::SetCursorPosY(ImGui::GetStyle().WindowPadding.y + 4.f);
        ImGui::Text("%s", spinMsg);
        ImGui::End();

        if (done)
        {
            m_saveFuture.get(); // consume the future
            m_dirty           = false;
            m_saveState       = SaveState::Done;
            m_donePoppedFirst = true;
        }
    }
    else // SaveState::Done
    {
        const char* msg  = "Saved";
        const char* hint = "(click to close)";
        const float wMsg = ImGui::CalcTextSize(msg).x, wHint = ImGui::CalcTextSize(hint).x;
        const float textW = wMsg > wHint ? wMsg : wHint;
        const float boxW  = textW + pad * 2.f + 16.f;
        const float boxH  = lineH * 2.f + pad * 2.f + 8.f;

        ImGui::SetNextWindowViewport(edVp->ID);
        ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(boxW, boxH), ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.96f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.f);
        ImGui::Begin(WinId("##sv_box").c_str(), nullptr, kOvFlags);
        ImGui::PopStyleVar();
        ImGui::SetCursorPosY(ImGui::GetStyle().WindowPadding.y + 4.f);
        ImGui::TextColored(ImVec4(0.35f, 1.f, 0.50f, 1.f), "%s", msg);
        ImGui::TextDisabled("%s", hint);
        ImGui::End();

        if (m_donePoppedFirst)
            m_donePoppedFirst = false;
        else if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
            m_saveState = SaveState::Idle;
    }
}

// -------------------------------------------------------------
//  Requirements subwindow  (3-panel: outer groups | inner items | detail)
//  Outer groups: sequences ending with req==1100
// -------------------------------------------------------------

void MovesetEditorWindow::RenderSubWin_Requirements()
{
    if (!m_reqWinOpen) return;
    ImGui::SetNextWindowSize(ImVec2(650.0f, 400.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(WinId("Requirements##blkwin").c_str(), &m_reqWinOpen, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking))
    { ImGui::End(); return; }

    auto& blk = m_data.requirementBlock;
    const uint32_t reqEnd = GameStatic::Get().data.reqListEnd;
    auto mkGroups = [&]{ return ComputeGroups(blk, +[](const ParsedRequirement& r)->bool{ return r.req==GameStatic::Get().data.reqListEnd; }); };
    auto groups = mkGroups();
    static constexpr float kListW = 180.0f;

    // Helper: render right-aligned + button, return its action
    auto ListHeader = [](const char* popupId, const char* typeName,
                         const char* label, int count) -> ListAction
    {
        float btnW = ImGui::CalcTextSize("+").x + ImGui::GetStyle().FramePadding.x * 2.0f;
        float rightX = ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - btnW;
        ImGui::TextDisabled("%s (%d)", label, count);
        ImGui::SameLine(rightX);
        return RenderListPlusMenu(popupId, typeName);
    };

    // ---- Outer list ----
    ImGui::BeginChild("##req_outer", ImVec2(kListW, 0.0f), true);
    {
        bool hasOuter = (m_reqWinSel.outer < (int)groups.size());
        ListAction outerAct = ListHeader("##req_om", "Requirement-list",
                                         "Requirement Lists", (int)groups.size());
        ImGui::Separator();

        if (outerAct == ListAction::Insert) {
            uint32_t insertPos = hasOuter
                ? groups[m_reqWinSel.outer].first + groups[m_reqWinSel.outer].second
                : (uint32_t)blk.size();
            ParsedRequirement term{}; term.req = reqEnd;
            blk.insert(blk.begin() + insertPos, term);
            FixupRef_Requirement(m_data, insertPos, true);
            m_reqWinSel.outer = hasOuter ? m_reqWinSel.outer + 1 : 0;
            m_reqWinSel.inner = 0; m_dirty = true;
            groups = mkGroups();
        } else if (outerAct == ListAction::Duplicate && hasOuter) {
            uint32_t gf = groups[m_reqWinSel.outer].first, gc = groups[m_reqWinSel.outer].second;
            for (uint32_t k = 0; k < gc; ++k) blk.push_back(blk[gf + k]);
            m_dirty = true; groups = mkGroups();
        } else if (outerAct == ListAction::Remove && hasOuter) {
            uint32_t gf = groups[m_reqWinSel.outer].first, gc = groups[m_reqWinSel.outer].second;
            uint32_t refs = CountRefs_Requirement(m_data, gf);
            int co = m_reqWinSel.outer;
            auto doRem = [this, gf, gc, co]() {
                for (int i = (int)gc - 1; i >= 0; --i) {
                    uint32_t pos = gf + (uint32_t)i;
                    FixupRef_Requirement(m_data, pos, false);
                    m_data.requirementBlock.erase(m_data.requirementBlock.begin() + pos);
                }
                m_reqWinSel.outer = (std::max)(0, co - 1); m_reqWinSel.inner = 0; m_dirty = true;
            };
            if (refs > 0) {
                snprintf(m_removeConfirm.message, sizeof(m_removeConfirm.message),
                    "Requirement-list at index %u is referenced by %u location(s).\nRemove anyway?", gf, refs);
                m_removeConfirm.onConfirm = doRem; m_removeConfirm.callerViewportId = ImGui::GetWindowViewport()->ID; m_removeConfirm.pending = true;
            } else { doRem(); groups = mkGroups(); }
        }

        ImGui::BeginChild("##req_outer_sl", ImVec2(0, 0), false);
        for (int gi = 0; gi < (int)groups.size(); ++gi) {
            uint32_t items = groups[gi].second > 0 ? groups[gi].second - 1 : 0;
            char lbl[48]; snprintf(lbl, sizeof(lbl), "#%u  %u items##rg%d", groups[gi].first, items, gi);
            bool s = (m_reqWinSel.outer == gi);
            if (ImGui::Selectable(lbl, s)) { m_reqWinSel.outer = gi; m_reqWinSel.inner = 0; }
            if (s && m_reqWinSel.scrollOuter) { ImGui::SetScrollHereY(0.5f); m_reqWinSel.scrollOuter = false; }
        }
        ImGui::EndChild();
    }
    ImGui::EndChild(); ImGui::SameLine();

    groups = mkGroups();

    // ---- Inner list ----
    ImGui::BeginChild("##req_inner", ImVec2(kListW, 0.0f), true);
    {
        bool hasOuter = (m_reqWinSel.outer < (int)groups.size());
        bool isEndSel = false;
        uint32_t innerAbsIdx = 0xFFFFFFFF;
        if (hasOuter) {
            innerAbsIdx = groups[m_reqWinSel.outer].first + (uint32_t)m_reqWinSel.inner;
            if (innerAbsIdx < (uint32_t)blk.size())
                isEndSel = (blk[innerAbsIdx].req == reqEnd);
        }

        int innerCount = hasOuter ? (int)groups[m_reqWinSel.outer].second : 0;
        ListAction innerAct = ListHeader("##req_im", "Requirement",
                                          "Requirements", innerCount);
        ImGui::Separator();

        // isOnlyEnd: group has only [END] → allow insert before it
        bool isOnlyEnd = isEndSel && hasOuter && (groups[m_reqWinSel.outer].second == 1);
        if (innerAct == ListAction::Insert && isEndSel && !isOnlyEnd) { innerAct = ListAction::None; m_endInsertBlocked = true; m_insertBlockedViewportId = ImGui::GetWindowViewport()->ID; }

        if (hasOuter && innerAct != ListAction::None && innerAbsIdx != 0xFFFFFFFF) {
            uint32_t gf = groups[m_reqWinSel.outer].first, gc = groups[m_reqWinSel.outer].second;
            if (innerAct == ListAction::Insert) {
                uint32_t ipos = isOnlyEnd ? innerAbsIdx : innerAbsIdx + 1;
                ParsedRequirement nr{}; nr.req = 0;
                blk.insert(blk.begin() + ipos, nr);
                FixupRef_Requirement(m_data, ipos, true);
                if (!isOnlyEnd) m_reqWinSel.inner++;
                m_dirty = true; groups = mkGroups();
            } else if (innerAct == ListAction::Duplicate && !isEndSel) {
                uint32_t ipos = gf + gc - 1; // before [END]
                blk.insert(blk.begin() + ipos, blk[innerAbsIdx]);
                FixupRef_Requirement(m_data, ipos, true);
                m_dirty = true; groups = mkGroups();
            } else if (innerAct == ListAction::Remove && !isEndSel) {
                uint32_t cai = innerAbsIdx;
                auto doRem = [this, cai]() {
                    FixupRef_Requirement(m_data, cai, false);
                    m_data.requirementBlock.erase(m_data.requirementBlock.begin() + cai);
                    if (m_reqWinSel.inner > 0) m_reqWinSel.inner--;
                    m_dirty = true;
                };
                uint32_t refs = CountRefs_Requirement(m_data, cai);
                if (refs > 0) {
                    snprintf(m_removeConfirm.message, sizeof(m_removeConfirm.message),
                        "Requirement at index %u is referenced by %u location(s).\nRemove anyway?", cai, refs);
                    m_removeConfirm.onConfirm = doRem; m_removeConfirm.callerViewportId = ImGui::GetWindowViewport()->ID; m_removeConfirm.pending = true;
                } else { doRem(); groups = mkGroups(); }
            }
        }

        ImGui::BeginChild("##req_inner_sl", ImVec2(0, 0), false);
        hasOuter = (m_reqWinSel.outer < (int)groups.size());
        if (hasOuter) {
            uint32_t start = groups[m_reqWinSel.outer].first;
            uint32_t count = groups[m_reqWinSel.outer].second;
            for (uint32_t k = 0; k < count; ++k) {
                uint32_t idx = start + k;
                if (idx >= (uint32_t)blk.size()) break;
                const ParsedRequirement& r = blk[idx];
                bool isTerm = (r.req == reqEnd);
                char lbl[48];
                if (isTerm) snprintf(lbl, sizeof(lbl), "#%u  [END]##ri%u", k, idx);
                else        snprintf(lbl, sizeof(lbl), "#%u  req=%u##ri%u", k, r.req, idx);
                if (isTerm) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f,0.5f,0.5f,1.0f));
                bool sel = (m_reqWinSel.inner == (int)k);
                if (ImGui::Selectable(lbl, sel)) m_reqWinSel.inner = (int)k;
                if (isTerm) ImGui::PopStyleColor();
            }
        }
        ImGui::EndChild();
    }
    ImGui::EndChild(); ImGui::SameLine();

    groups = mkGroups();

    // ---- Detail ----
    ImGui::BeginChild("##req_detail", ImVec2(0.0f, 0.0f), false);
    if (m_reqWinSel.outer < (int)groups.size()) {
        uint32_t start = groups[m_reqWinSel.outer].first;
        uint32_t idx = start + (uint32_t)m_reqWinSel.inner;
        if (idx < (uint32_t)blk.size()) {
            ParsedRequirement& r = blk[idx];
            const ImGuiStyle& sty = ImGui::GetStyle();
            static constexpr ImVec4 kBlockBg = {0.14f, 0.14f, 0.18f, 1.00f};
            static constexpr ImVec4 kTTCol   = {0.40f, 0.75f, 1.00f, 1.00f};

            // --- Block 1: req ---
            float reqBlockH = ImGui::GetTextLineHeight()
                + sty.ItemSpacing.y + ImGui::GetFrameHeight()
                + sty.WindowPadding.y * 2.0f;
            ImGui::PushStyleColor(ImGuiCol_ChildBg, kBlockBg);
            if (ImGui::BeginChild("##req_b1", ImVec2(-1.0f, reqBlockH), ImGuiChildFlags_Borders)) {
                ImGui::TextDisabled("%s", ReqLabel::Req); ShowFieldTooltip(FieldTT::Req::Req);
                ImGui::SetNextItemWidth(-1.0f);
                int tmp = static_cast<int>(r.req);
                if (ImGui::InputInt("##req_val", &tmp, 0, 0))
                    { r.req = static_cast<uint32_t>(tmp); m_dirty = true; }
            }
            ImGui::EndChild();
            ImGui::PopStyleColor();

            ImGui::Spacing();

            // --- Block 2: params ---
            float rowH   = ImGui::GetFrameHeight() + sty.ItemSpacing.y;
            float paramBlockH = rowH * 4.0f - sty.ItemSpacing.y + sty.WindowPadding.y * 2.0f;
            ImGui::PushStyleColor(ImGuiCol_ChildBg, kBlockBg);
            if (ImGui::BeginChild("##req_b2", ImVec2(-1.0f, paramBlockH), ImGuiChildFlags_Borders)) {
                constexpr ImGuiTableFlags kTF = ImGuiTableFlags_SizingFixedFit;
                if (ImGui::BeginTable("##req_params", 2, kTF, ImVec2(-1.0f, 0.0f))) {
                    ImGui::TableSetupColumn("##lbl", ImGuiTableColumnFlags_WidthFixed);
                    ImGui::TableSetupColumn("##val", ImGuiTableColumnFlags_WidthStretch);

                    struct { const char* label; uint32_t* val; const char* id; const FieldTooltip* tt; } rows[] = {
                        { ReqLabel::Param0, &r.param,  "##p0", &FieldTT::Req::Param0 },
                        { ReqLabel::Param1, &r.param2, "##p1", &FieldTT::Req::Param1 },
                        { ReqLabel::Param2, &r.param3, "##p2", &FieldTT::Req::Param2 },
                        { ReqLabel::Param3, &r.param4, "##p3", &FieldTT::Req::Param3 },
                    };
                    for (auto& row : rows) {
                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0); ImGui::TextDisabled("%s", row.label); ShowFieldTooltip(*row.tt);
                        ImGui::TableSetColumnIndex(1);
                        ImGui::SetNextItemWidth(-1.0f);
                        int tmp = static_cast<int>(*row.val);
                        if (ImGui::InputInt(row.id, &tmp, 0, 0))
                            { *row.val = static_cast<uint32_t>(tmp); m_dirty = true; }
                    }
                    ImGui::EndTable();
                }
            }
            ImGui::EndChild();
            ImGui::PopStyleColor();

            // --- Tooltip area (plain, no box) ---
            ImGui::Spacing();
            ImGui::TextColored(kTTCol, " ");  // placeholder — tooltip text goes here
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
    const ImGuiStyle& sty = ImGui::GetStyle();
    static constexpr ImVec4 kBlockBg = {0.14f, 0.14f, 0.18f, 1.00f};
    static constexpr ImVec4 kGreen   = {0.55f, 0.85f, 0.55f, 1.0f};
    static constexpr ImVec4 kPink    = {1.00f, 0.50f, 0.65f, 1.0f};
    static constexpr ImVec4 kTTCol   = {0.40f, 0.75f, 1.00f, 1.00f};

    // GoButton: custom InvisibleButton rendering identical to RenderSection_Overview
    const float kArrowW = ImGui::GetFrameHeight() * 0.38f;
    const float kBtnW   = ImGui::CalcTextSize("Go ").x + kArrowW
                        + sty.FramePadding.x * 2.0f + 4.0f;

    auto GoButton = [&](const char* id, bool valid) -> bool {
        if (!valid) ImGui::BeginDisabled();
        const float  h   = ImGui::GetFrameHeight();
        bool clicked     = ImGui::InvisibleButton(id, ImVec2(kBtnW, h));
        const ImVec2 p0  = ImGui::GetItemRectMin();
        const ImVec2 p1  = ImGui::GetItemRectMax();
        const bool   hov = ImGui::IsItemHovered();
        const bool   act = ImGui::IsItemActive();
        ImDrawList*  dl  = ImGui::GetWindowDrawList();
        dl->AddRectFilled(p0, p1,
            ImGui::GetColorU32(act ? ImGuiCol_ButtonActive : hov ? ImGuiCol_ButtonHovered : ImGuiCol_Button),
            sty.FrameRounding);
        dl->AddRect(p0, p1, ImGui::GetColorU32(ImGuiCol_Border), sty.FrameRounding);
        const ImU32  col    = ImGui::GetColorU32(ImGuiCol_Text);
        const char*  txt    = "Go ";
        const ImVec2 txtSz  = ImGui::CalcTextSize(txt);
        const float  totalW = txtSz.x + kArrowW;
        const float  startX = p0.x + (kBtnW - totalW) * 0.5f;
        const float  midY   = (p0.y + p1.y) * 0.5f;
        const float  arrowH = h * 0.30f;
        dl->AddText(ImVec2(startX, midY - txtSz.y * 0.5f), col, txt);
        dl->AddTriangleFilled(
            ImVec2(startX + txtSz.x,           midY - arrowH * 0.5f),
            ImVec2(startX + txtSz.x,           midY + arrowH * 0.5f),
            ImVec2(startX + txtSz.x + kArrowW, midY), col);
        if (!valid) ImGui::EndDisabled();
        return clicked && valid;
    };

    // --- Block 1: command / extradata / requirements / move ---
    // Height: 4 × (TextLineH + FrameH + spacing×2) − spacing + padding×2
    float fH1     = ImGui::GetTextLineHeight() + ImGui::GetFrameHeight() + sty.ItemSpacing.y * 2.0f;
    float block1H = fH1 * 4.0f - sty.ItemSpacing.y + sty.WindowPadding.y * 2.0f;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, kBlockBg);
    if (ImGui::BeginChild("##cdt_b1", ImVec2(-1.0f, block1H), ImGuiChildFlags_Borders))
    {
        // command (hex64, no navigation)
        ImGui::TextDisabled("%s", CancelLabel::Command); ShowFieldTooltip(FieldTT::Cancel::Command);
        ImGui::SetNextItemWidth(-1.0f);
        char cmdBuf[22]; snprintf(cmdBuf, sizeof(cmdBuf), "0x%016llX", (unsigned long long)c.command);
        ImGui::InputText("##cmd", cmdBuf, sizeof(cmdBuf));
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            const char* p = cmdBuf;
            if (p[0]=='0' && (p[1]=='x'||p[1]=='X')) p += 2;
            c.command = (uint64_t)strtoull(p, nullptr, 16);
            m_dirty = true;
        }

        // extradata (uint32_t, navigate to cancel-extra)
        bool extValid = (c.extradata_idx != 0xFFFFFFFF) &&
                        (c.extradata_idx < (uint32_t)m_data.cancelExtraBlock.size());
        ImGui::PushStyleColor(ImGuiCol_Text, extValid ? kGreen : kPink);
        ImGui::TextUnformatted(CancelLabel::Extradata); ShowFieldTooltip(FieldTT::Cancel::Extradata);
        ImGui::PopStyleColor();
        ImGui::SetNextItemWidth(-(kBtnW + sty.ItemSpacing.x));
        int extTmp = (c.extradata_idx == 0xFFFFFFFF) ? -1 : (int)c.extradata_idx;
        if (ImGui::InputInt("##ext_idx", &extTmp, 0, 0)) {
            c.extradata_idx = (extTmp < 0) ? 0xFFFFFFFF : (uint32_t)extTmp;
            m_dirty = true;
        }
        ImGui::SameLine();
        if (GoButton("##ext_go", extValid)) {
            m_cancelsWin.extradataSel       = (int)c.extradata_idx;
            m_cancelsWin.extraScrollPending = true;
        }

        // requirements (uint32_t, navigate to requirement window)
        bool reqValid = (c.req_list_idx != 0xFFFFFFFF) &&
                        (c.req_list_idx < (uint32_t)m_data.requirementBlock.size());
        ImGui::PushStyleColor(ImGuiCol_Text, reqValid ? kGreen : kPink);
        ImGui::TextUnformatted(CancelLabel::Requirements); ShowFieldTooltip(FieldTT::Cancel::Requirements);
        ImGui::PopStyleColor();
        ImGui::SetNextItemWidth(-(kBtnW + sty.ItemSpacing.x));
        int reqTmp = (c.req_list_idx == 0xFFFFFFFF) ? -1 : (int)c.req_list_idx;
        if (ImGui::InputInt("##req_idx", &reqTmp, 0, 0)) {
            c.req_list_idx = (reqTmp < 0) ? 0xFFFFFFFF : (uint32_t)reqTmp;
            m_dirty = true;
        }
        ImGui::SameLine();
        if (GoButton("##req_go", reqValid)) {
            const auto& blk = m_data.requirementBlock;
            auto grps = ComputeGroups(blk, +[](const ParsedRequirement& rr)->bool{
                return rr.req == GameStatic::Get().data.reqListEnd; });
            int gi = FindGroupOuter(grps, c.req_list_idx);
            if (gi >= 0) { m_reqWinSel.outer = gi; m_reqWinSel.inner = 0; m_reqWinSel.scrollOuter = true; }
            m_reqWinOpen = true;
        }

        // move (uint16_t, navigate — same logic as before)
        const bool isTerm = ((c.command == 0x8000 || c.command == GameStatic::Get().data.groupCancelEnd)
                             && c.move_id == 0x8000);
        bool moveValid  = false;
        int  resolvedIdx = -1;
        const char* moveLbl = CancelLabel::Move;

        if (c.command == GameStatic::Get().data.groupCancelStart) {
            moveLbl   = CancelLabel::GroupCancelIdx;
            moveValid = (FindGroupOuter(gcGroups, (uint32_t)c.move_id) >= 0);
        } else if (!isTerm && c.move_id >= 0x8000) {
            const uint32_t aliasIdx = c.move_id - 0x8000u;
            if (aliasIdx < m_data.originalAliases.size()) {
                const uint16_t res = m_data.originalAliases[aliasIdx];
                if ((size_t)res < m_data.moves.size()) { resolvedIdx = (int)res; moveValid = true; }
            }
        } else {
            moveValid = !isTerm && (c.move_id < (uint16_t)m_data.moves.size());
        }

        ImGui::PushStyleColor(ImGuiCol_Text, moveValid ? kGreen : kPink);
        ImGui::TextUnformatted(moveLbl);
        ShowFieldTooltip(c.command == GameStatic::Get().data.groupCancelStart ? FieldTT::Cancel::GroupCancelIdx : FieldTT::Cancel::Move);
        ImGui::PopStyleColor();
        ImGui::SetNextItemWidth(-(kBtnW + sty.ItemSpacing.x));
        int moveTmp = (int)(uint16_t)c.move_id;
        if (ImGui::InputInt("##mv_idx", &moveTmp, 0, 0)) {
            c.move_id = (uint16_t)(moveTmp < 0 ? 0 : moveTmp > 0xFFFF ? 0xFFFF : moveTmp);
            m_dirty = true;
        }
        ImGui::SameLine();
        if (GoButton("##mv_go", moveValid)) {
            if (c.command == GameStatic::Get().data.groupCancelStart) {
                int gi = FindGroupOuter(gcGroups, (uint32_t)c.move_id);
                if (gi >= 0) {
                    m_cancelsWin.groupCancelSel.outer       = gi;
                    m_cancelsWin.groupCancelSel.inner       = 0;
                    m_cancelsWin.groupCancelSel.scrollOuter = true;
                }
                m_cancelsWin.open = true; m_cancelsWin.pendingFocus = true;
            } else if (!isTerm && c.move_id >= 0x8000) {
                if (resolvedIdx >= 0) { m_selectedIdx = resolvedIdx; m_moveListScrollPending = true; }
            } else {
                m_selectedIdx = (int)c.move_id; m_moveListScrollPending = true;
            }
        }
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::Spacing();

    // --- Block 2: input_window_start / end / starting_frame / option ---
    float fH2     = ImGui::GetFrameHeight() + sty.ItemSpacing.y;
    float block2H = fH2 * 4.0f - sty.ItemSpacing.y + sty.WindowPadding.y * 2.0f;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, kBlockBg);
    if (ImGui::BeginChild("##cdt_b2", ImVec2(-1.0f, block2H), ImGuiChildFlags_Borders))
    {
        constexpr ImGuiTableFlags kTF = ImGuiTableFlags_SizingFixedFit;
        if (ImGui::BeginTable("##cdt_tbl2", 2, kTF, ImVec2(-1.0f, 0.0f))) {
            ImGui::TableSetupColumn("##l2", ImGuiTableColumnFlags_WidthFixed);
            ImGui::TableSetupColumn("##v2", ImGuiTableColumnFlags_WidthStretch);

            auto Row32 = [&](const char* id, const char* lbl, uint32_t& v, const FieldTooltip& tt = {}) {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0); ImGui::TextDisabled("%s", lbl); ShowFieldTooltip(tt);
                ImGui::TableSetColumnIndex(1); ImGui::SetNextItemWidth(-1.0f);
                int tmp = (int)v;
                if (ImGui::InputInt(id, &tmp, 0, 0)) { v = (uint32_t)tmp; m_dirty = true; }
            };
            auto Row16 = [&](const char* id, const char* lbl, uint16_t& v, const FieldTooltip& tt = {}) {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0); ImGui::TextDisabled("%s", lbl); ShowFieldTooltip(tt);
                ImGui::TableSetColumnIndex(1); ImGui::SetNextItemWidth(-1.0f);
                int tmp = (int)v;
                if (ImGui::InputInt(id, &tmp, 0, 0)) { v = (uint16_t)tmp; m_dirty = true; }
            };

            Row32("##fws", CancelLabel::InputWindowStart, c.frame_window_start, FieldTT::Cancel::InputWindowStart);
            Row32("##fwe", CancelLabel::InputWindowEnd,   c.frame_window_end,   FieldTT::Cancel::InputWindowEnd);
            Row32("##sf",  CancelLabel::StartingFrame,    c.starting_frame,     FieldTT::Cancel::StartingFrame);
            Row16("##opt", CancelLabel::Option,           c.cancel_option,      FieldTT::Cancel::Option);
            ImGui::EndTable();
        }
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// -------------------------------------------------------------
//  Cancel list helpers: make terminator/empty cancel entries
// -------------------------------------------------------------

static ParsedCancel MakeCancelTerminator()
{
    ParsedCancel c{};
    c.command       = 0x8000;
    c.move_id       = 0x8000;
    c.req_list_idx  = 0xFFFFFFFF;
    c.extradata_idx = 0xFFFFFFFF;
    c.group_cancel_list_idx = 0xFFFFFFFF;
    return c;
}

static ParsedCancel MakeGroupCancelTerminator()
{
    ParsedCancel c{};
    c.command       = (uint64_t)GameStatic::Get().data.groupCancelEnd;
    c.move_id       = 0x8000;
    c.req_list_idx  = 0xFFFFFFFF;
    c.extradata_idx = 0xFFFFFFFF;
    c.group_cancel_list_idx = 0xFFFFFFFF;
    return c;
}

static ParsedCancel MakeEmptyCancel()
{
    ParsedCancel c{};
    c.command       = 0;
    c.move_id       = 0;
    c.req_list_idx  = 0xFFFFFFFF;
    c.extradata_idx = 0xFFFFFFFF;
    c.group_cancel_list_idx = 0xFFFFFFFF;
    return c;
}

static void RenderCancelSection(
    const char* outerChildId, const char* innerChildId, const char* detailChildId,
    std::vector<ParsedCancel>& block,
    std::vector<std::pair<uint32_t,uint32_t>> groups,  // passed by value, recomputed inside
    MovesetEditorWindow::TwoLevelSel& sel,
    const char* listLabel,
    float sectionH,
    uint64_t terminatorCmd,
    MovesetEditorWindow* win,
    const std::vector<std::pair<uint32_t,uint32_t>>& gcGroups,
    MotbinData& data,
    bool& dirty,
    bool isGroupCancel)
{
    static constexpr float kColW = 180.0f;

    // Right-aligned + button header: "Label (N)  [+]"
    auto ListHdr = [](const char* popupId, const char* typeName,
                      const char* label, int count) -> ListAction
    {
        float btnW  = ImGui::CalcTextSize("+").x + ImGui::GetStyle().FramePadding.x * 2.0f;
        float rightX = ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - btnW;
        ImGui::TextDisabled("%s (%d)", label, count);
        ImGui::SameLine(rightX);
        return RenderListPlusMenu(popupId, typeName);
    };

    auto recompGroups = [&]() {
        if (isGroupCancel) {
            const uint64_t gcEnd = GameStatic::Get().data.groupCancelEnd;
            groups = ComputeGroups(block, [gcEnd](const ParsedCancel& c){ return c.command == gcEnd; });
        } else {
            groups = ComputeGroups(block, +[](const ParsedCancel& c){ return c.command == 0x8000; });
        }
    };

    // ---- Outer panel ----
    ImGui::BeginChild(outerChildId, ImVec2(kColW, sectionH), true);
    {
        bool hasOuter = (sel.outer < (int)groups.size());
        char outerMenuId[64]; snprintf(outerMenuId, sizeof(outerMenuId), "##cListMenu%s", outerChildId);
        ListAction outerAct = ListHdr(outerMenuId, isGroupCancel ? "Group-Cancel-list" : "Cancel-list",
                                      listLabel, (int)groups.size());
        ImGui::Separator();

        if (outerAct == ListAction::Insert) {
            uint32_t insertPos = hasOuter
                ? groups[sel.outer].first + groups[sel.outer].second
                : (uint32_t)block.size();
            ParsedCancel term = isGroupCancel ? MakeGroupCancelTerminator() : MakeCancelTerminator();
            block.insert(block.begin() + insertPos, term);
            if (isGroupCancel) FixupRef_GroupCancel(data, insertPos, true);
            else               FixupRef_Cancel(data, insertPos, true);
            sel.outer = hasOuter ? sel.outer + 1 : 0; sel.inner = 0; dirty = true;
            recompGroups();
        } else if (outerAct == ListAction::Duplicate && hasOuter) {
            uint32_t gf = groups[sel.outer].first, gc = groups[sel.outer].second;
            for (uint32_t k = 0; k < gc; ++k) block.push_back(block[gf + k]);
            dirty = true; recompGroups();
        } else if (outerAct == ListAction::Remove && hasOuter) {
            uint32_t gf = groups[sel.outer].first, gc = groups[sel.outer].second;
            uint32_t refs = isGroupCancel ? CountRefs_GroupCancel(data, gf) : CountRefs_Cancel(data, gf);
            int co = sel.outer;
            auto doRem = [&block, &data, &dirty, &sel, isGroupCancel, gf, gc, co]() {
                for (int i = (int)gc - 1; i >= 0; --i) {
                    uint32_t pos = gf + (uint32_t)i;
                    if (isGroupCancel) FixupRef_GroupCancel(data, pos, false);
                    else               FixupRef_Cancel(data, pos, false);
                    block.erase(block.begin() + pos);
                }
                sel.outer = (std::max)(0, co - 1); sel.inner = 0; dirty = true;
            };
            if (refs > 0) {
                snprintf(win->m_removeConfirm.message, sizeof(win->m_removeConfirm.message),
                    "Cancel-list at index %u is referenced by %u location(s).\nRemove anyway?", gf, refs);
                win->m_removeConfirm.onConfirm = doRem; win->m_removeConfirm.callerViewportId = ImGui::GetWindowViewport()->ID; win->m_removeConfirm.pending = true;
            } else { doRem(); recompGroups(); }
        }

        ImGui::BeginChild((std::string(outerChildId) + "_sl").c_str(), ImVec2(0, 0), false);
        for (int gi = 0; gi < (int)groups.size(); ++gi) {
            uint32_t count = groups[gi].second;
            uint32_t items = count > 0 ? count - 1 : 0;
            char lbl[48]; snprintf(lbl, sizeof(lbl), "#%u  %u items##g%d", groups[gi].first, items, gi);
            bool s = (sel.outer == gi);
            if (ImGui::Selectable(lbl, s)) { sel.outer = gi; sel.inner = 0; }
            if (s && sel.scrollOuter) { ImGui::SetScrollHereY(0.5f); sel.scrollOuter = false; }
        }
        ImGui::EndChild();
    }
    ImGui::EndChild();

    recompGroups();

    ImGui::SameLine();

    // ---- Inner panel ----
    ImGui::BeginChild(innerChildId, ImVec2(kColW, sectionH), true);
    {
        bool hasOuter = (sel.outer < (int)groups.size());
        bool isEndSel = false;
        uint32_t innerAbsIdx = 0xFFFFFFFF;
        if (hasOuter) {
            innerAbsIdx = groups[sel.outer].first + (uint32_t)sel.inner;
            if (innerAbsIdx < (uint32_t)block.size())
                isEndSel = (block[innerAbsIdx].command == terminatorCmd);
        }

        int innerCount = hasOuter ? (int)groups[sel.outer].second : 0;
        char innerMenuId[64]; snprintf(innerMenuId, sizeof(innerMenuId), "##cItemMenu%s", innerChildId);
        ListAction innerAct = ListHdr(innerMenuId, isGroupCancel ? "Group-Cancel" : "Cancel",
                                      "Cancels", innerCount);
        ImGui::Separator();

        bool isOnlyEnd = isEndSel && hasOuter && (groups[sel.outer].second == 1);
        if (innerAct == ListAction::Insert && isEndSel && !isOnlyEnd) { innerAct = ListAction::None; win->m_endInsertBlocked = true; win->m_insertBlockedViewportId = ImGui::GetWindowViewport()->ID; }

        if (hasOuter && innerAct != ListAction::None && innerAbsIdx != 0xFFFFFFFF) {
            uint32_t gf = groups[sel.outer].first, gc = groups[sel.outer].second;
            if (innerAct == ListAction::Insert) {
                uint32_t ipos = isOnlyEnd ? innerAbsIdx : innerAbsIdx + 1;
                block.insert(block.begin() + ipos, MakeEmptyCancel());
                if (isGroupCancel) FixupRef_GroupCancel(data, ipos, true);
                else               FixupRef_Cancel(data, ipos, true);
                if (!isOnlyEnd) sel.inner++;
                dirty = true; recompGroups();
            } else if (innerAct == ListAction::Duplicate && !isEndSel) {
                uint32_t ipos = gf + gc - 1; // before [END]
                block.insert(block.begin() + ipos, block[innerAbsIdx]);
                if (isGroupCancel) FixupRef_GroupCancel(data, ipos, true);
                else               FixupRef_Cancel(data, ipos, true);
                dirty = true; recompGroups();
            } else if (innerAct == ListAction::Remove && !isEndSel) {
                uint32_t cai = innerAbsIdx;
                uint32_t refs = isGroupCancel ? CountRefs_GroupCancel(data, cai) : CountRefs_Cancel(data, cai);
                auto doRem = [&block, &data, &dirty, &sel, isGroupCancel, cai]() {
                    if (isGroupCancel) FixupRef_GroupCancel(data, cai, false);
                    else               FixupRef_Cancel(data, cai, false);
                    block.erase(block.begin() + cai);
                    if (sel.inner > 0) sel.inner--;
                    dirty = true;
                };
                if (refs > 0) {
                    snprintf(win->m_removeConfirm.message, sizeof(win->m_removeConfirm.message),
                        "Cancel at index %u is referenced by %u location(s).\nRemove anyway?", cai, refs);
                    win->m_removeConfirm.onConfirm = doRem; win->m_removeConfirm.callerViewportId = ImGui::GetWindowViewport()->ID; win->m_removeConfirm.pending = true;
                } else { doRem(); recompGroups(); }
            }
        }

        ImGui::BeginChild((std::string(innerChildId) + "_sl").c_str(), ImVec2(0, 0), false);
        hasOuter = (sel.outer < (int)groups.size());
        if (hasOuter) {
            uint32_t start = groups[sel.outer].first;
            uint32_t count = groups[sel.outer].second;
            for (uint32_t k = 0; k < count; ++k) {
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
    }
    ImGui::EndChild();

    ImGui::SameLine();
    ImGui::BeginChild(detailChildId, ImVec2(0.0f, sectionH), false);
    if (sel.outer < (int)groups.size()) {
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
    ImGui::SetNextWindowSize(ImVec2(700.0f, 400.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(WinId("Cancels##blkwin").c_str(), &m_cancelsWin.open, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking))
    { ImGui::End(); return; }
    if (cancelsBringFront) ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindow());

    const uint64_t gcEnd = GameStatic::Get().data.groupCancelEnd;
    auto isCancelTerm      = +[](const ParsedCancel& c)->bool { return c.command == 0x8000; };
    auto isGroupCancelTerm = [gcEnd](const ParsedCancel& c)->bool { return c.command == gcEnd; };
    auto cancelGroups      = ComputeGroups(m_data.cancelBlock,      isCancelTerm);
    auto groupCancelGroups = ComputeGroups(m_data.groupCancelBlock, isGroupCancelTerm);

    static constexpr float   kCexW   = 180.0f;
    static constexpr ImVec4  kBlockBg = {0.14f, 0.14f, 0.18f, 1.00f};
    static constexpr ImVec4  kSelBg   = {0.22f, 0.22f, 0.30f, 1.00f};
    static constexpr ImVec4  kTTCol   = {0.40f, 0.75f, 1.00f, 1.00f};
    static constexpr float   kTTH     = 20.0f;

    float leftW = ImGui::GetContentRegionAvail().x - kCexW - ImGui::GetStyle().ItemSpacing.x;

    // Left: tab bar (Cancel List | Group Cancel List)
    ImGui::BeginChild("##canleft", ImVec2(leftW, 0.0f), false);
    {
        if (ImGui::BeginTabBar("##cantabs"))
        {
            if (ImGui::BeginTabItem("Cancel List"))
            {
                RenderCancelSection("##co","##ci","##cd",
                    m_data.cancelBlock, cancelGroups, m_cancelsWin.cancelSel,
                    "Cancel Lists", 0.0f, 0x8000,
                    this, groupCancelGroups,
                    m_data, m_dirty, false);
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Group Cancel List"))
            {
                RenderCancelSection("##gco","##gci","##gcd",
                    m_data.groupCancelBlock, groupCancelGroups, m_cancelsWin.groupCancelSel,
                    "Group Cancel Lists", 0.0f, (uint64_t)GameStatic::Get().data.groupCancelEnd,
                    this, {},
                    m_data, m_dirty, true);
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
    }
    ImGui::EndChild();

    // Right: cancel-extra (card style, always visible)
    ImGui::SameLine();
    ImGui::BeginChild("##cexcol", ImVec2(kCexW, 0.0f), true);
    {
        auto& cexBlk  = m_data.cancelExtraBlock;
        int   cexTotal = (int)cexBlk.size();

        // Header: "Cancel Extras (N)  [+]"
        float btnW  = ImGui::CalcTextSize("+").x + ImGui::GetStyle().FramePadding.x * 2.0f;
        float rightX = ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - btnW;
        ImGui::TextDisabled("Cancel Extras (%d)", cexTotal);
        ImGui::SameLine(rightX);
        ListAction cexAct = RenderListPlusMenu("##cex_pm", "Cancel-extra");
        ImGui::Separator();

        bool hasCexSel = (m_cancelsWin.extradataSel >= 0 && m_cancelsWin.extradataSel < cexTotal);
        if (cexAct == ListAction::Insert) {
            uint32_t ipos = hasCexSel ? (uint32_t)m_cancelsWin.extradataSel + 1 : (uint32_t)cexTotal;
            cexBlk.insert(cexBlk.begin() + ipos, 0u);
            FixupRef_CancelExtra(m_data, ipos, true);
            m_cancelsWin.extradataSel = (int)ipos; m_dirty = true;
        } else if (cexAct == ListAction::Duplicate && hasCexSel) {
            cexBlk.push_back(cexBlk[m_cancelsWin.extradataSel]);
            m_dirty = true;
        } else if (cexAct == ListAction::Remove && hasCexSel) {
            uint32_t pos  = (uint32_t)m_cancelsWin.extradataSel;
            uint32_t refs = CountRefs_CancelExtra(m_data, pos);
            auto doRem = [this, pos]() {
                FixupRef_CancelExtra(m_data, pos, false);
                m_data.cancelExtraBlock.erase(m_data.cancelExtraBlock.begin() + pos);
                if (m_cancelsWin.extradataSel >= (int)m_data.cancelExtraBlock.size())
                    m_cancelsWin.extradataSel = (std::max)(0, (int)m_data.cancelExtraBlock.size() - 1);
                m_dirty = true;
            };
            if (refs > 0) {
                snprintf(m_removeConfirm.message, sizeof(m_removeConfirm.message),
                    "Cancel-extra at index %u is referenced by %u location(s).\nRemove anyway?", pos, refs);
                m_removeConfirm.onConfirm = doRem; m_removeConfirm.callerViewportId = ImGui::GetWindowViewport()->ID; m_removeConfirm.pending = true;
            } else { doRem(); }
        }

        // Card-style items
        const ImGuiStyle& sty = ImGui::GetStyle();
        float cardH = ImGui::GetTextLineHeight() + sty.ItemSpacing.y
                    + ImGui::GetFrameHeight() + sty.ItemSpacing.y
                    + kTTH + sty.WindowPadding.y * 2.0f;

        ImGui::BeginChild("##cex_scroll", ImVec2(0.0f, 0.0f), false);
        for (int i = 0; i < cexTotal; ++i)
        {
            bool sel = (m_cancelsWin.extradataSel == i);
            ImGui::PushStyleColor(ImGuiCol_ChildBg, sel ? kSelBg : kBlockBg);
            char cardId[32]; snprintf(cardId, sizeof(cardId), "##cex_%d", i);
            if (ImGui::BeginChild(cardId, ImVec2(-1.0f, cardH), ImGuiChildFlags_Borders))
            {
                // Click anywhere in the card to select
                if (ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem)
                    && ImGui::IsMouseClicked(0))
                    m_cancelsWin.extradataSel = i;

                ImGui::TextDisabled("#%d", i);

                // "value" label + InputInt on same row (2-col table)
                constexpr ImGuiTableFlags kTF = ImGuiTableFlags_SizingFixedFit;
                if (ImGui::BeginTable("##cex_row", 2, kTF, ImVec2(-1.0f, 0.0f))) {
                    ImGui::TableSetupColumn("##cxl", ImGuiTableColumnFlags_WidthFixed);
                    ImGui::TableSetupColumn("##cxv", ImGuiTableColumnFlags_WidthStretch);
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0); ImGui::TextDisabled("value");
                    ImGui::TableSetColumnIndex(1); ImGui::SetNextItemWidth(-1.0f);
                    char cxid[32]; snprintf(cxid, sizeof(cxid), "##cxv%d", i);
                    int cxtmp = (int)cexBlk[i];
                    if (ImGui::InputInt(cxid, &cxtmp, 0, 0)) { cexBlk[i] = (uint32_t)cxtmp; m_dirty = true; }
                    ImGui::EndTable();
                }

                // Tooltip placeholder
                ImGui::TextColored(kTTCol, " ");
            }
            ImGui::EndChild();
            // 카드가 ##cex_scroll의 last item이 된 시점에서 스크롤 적용
            if (sel && m_cancelsWin.extraScrollPending) {
                ImGui::SetScrollHereY(0.5f);
                m_cancelsWin.extraScrollPending = false;
            }
            ImGui::PopStyleColor();
            ImGui::Spacing();
        }
        ImGui::EndChild();
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
    ImGui::SetNextWindowSize(ImVec2(650.0f, 400.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(WinId("Hit Conditions##blkwin").c_str(), &m_hitCondWinOpen, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking))
    { ImGui::End(); return; }
    if (hitCondBringFront) ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindow());

    auto& blk = m_data.hitConditionBlock;
    const auto& reqBlk = m_data.requirementBlock;

    auto isHcEnd = [&](const ParsedHitCondition& h) -> bool {
        if (h.requirement_addr == 0) return true;
        if (h.req_list_idx != 0xFFFFFFFF && h.req_list_idx < (uint32_t)reqBlk.size())
            return (reqBlk[h.req_list_idx].req == GameStatic::Get().data.reqListEnd);
        return false;
    };
    auto mkGroups = [&]{ return ComputeGroups(blk, isHcEnd); };
    auto groups = mkGroups();
    static constexpr float kListW = 180.0f;

    auto ListHdr = [](const char* popupId, const char* typeName,
                      const char* label, int count) -> ListAction
    {
        float btnW  = ImGui::CalcTextSize("+").x + ImGui::GetStyle().FramePadding.x * 2.0f;
        float rightX = ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - btnW;
        ImGui::TextDisabled("%s (%d)", label, count);
        ImGui::SameLine(rightX);
        return RenderListPlusMenu(popupId, typeName);
    };

    // ---- Outer list ----
    ImGui::BeginChild("##hc_outer", ImVec2(kListW, 0.0f), true);
    {
        bool hasOuter = (m_hitCondWinSel.outer < (int)groups.size());
        ListAction outerAct = ListHdr("##hc_om", "Hit-condition-list",
                                       "Hit Condition Lists", (int)groups.size());
        ImGui::Separator();

        if (outerAct == ListAction::Insert) {
            uint32_t insertPos = hasOuter
                ? groups[m_hitCondWinSel.outer].first + groups[m_hitCondWinSel.outer].second
                : (uint32_t)blk.size();
            ParsedHitCondition term{};
            blk.insert(blk.begin() + insertPos, term);
            FixupRef_HitCond(m_data, insertPos, true);
            m_hitCondWinSel.outer = hasOuter ? m_hitCondWinSel.outer + 1 : 0;
            m_hitCondWinSel.inner = 0; m_dirty = true;
            groups = mkGroups();
        } else if (outerAct == ListAction::Duplicate && hasOuter) {
            uint32_t gf = groups[m_hitCondWinSel.outer].first, gc = groups[m_hitCondWinSel.outer].second;
            for (uint32_t k = 0; k < gc; ++k) blk.push_back(blk[gf + k]);
            m_dirty = true; groups = mkGroups();
        } else if (outerAct == ListAction::Remove && hasOuter) {
            uint32_t gf = groups[m_hitCondWinSel.outer].first, gc = groups[m_hitCondWinSel.outer].second;
            uint32_t refs = CountRefs_HitCond(m_data, gf);
            int co = m_hitCondWinSel.outer;
            auto doRem = [this, gf, gc, co]() {
                for (int i = (int)gc - 1; i >= 0; --i) {
                    uint32_t pos = gf + (uint32_t)i;
                    FixupRef_HitCond(m_data, pos, false);
                    m_data.hitConditionBlock.erase(m_data.hitConditionBlock.begin() + pos);
                }
                m_hitCondWinSel.outer = (std::max)(0, co - 1); m_hitCondWinSel.inner = 0; m_dirty = true;
            };
            if (refs > 0) {
                snprintf(m_removeConfirm.message, sizeof(m_removeConfirm.message),
                    "Hit-condition-list at index %u is referenced by %u location(s).\nRemove anyway?", gf, refs);
                m_removeConfirm.onConfirm = doRem; m_removeConfirm.callerViewportId = ImGui::GetWindowViewport()->ID; m_removeConfirm.pending = true;
            } else { doRem(); groups = mkGroups(); }
        }

        ImGui::BeginChild("##hc_outer_sl", ImVec2(0, 0), false);
        for (int gi = 0; gi < (int)groups.size(); ++gi) {
            uint32_t items = groups[gi].second > 0 ? groups[gi].second - 1 : 0;
            char lbl[48]; snprintf(lbl, sizeof(lbl), "#%u  %u items##hcg%d", groups[gi].first, items, gi);
            bool s = (m_hitCondWinSel.outer == gi);
            if (ImGui::Selectable(lbl, s)) { m_hitCondWinSel.outer = gi; m_hitCondWinSel.inner = 0; }
            if (s && m_hitCondWinSel.scrollOuter) { ImGui::SetScrollHereY(0.5f); m_hitCondWinSel.scrollOuter = false; }
        }
        ImGui::EndChild();
    }
    ImGui::EndChild(); ImGui::SameLine();

    groups = mkGroups();

    // ---- Inner list ----
    ImGui::BeginChild("##hc_inner", ImVec2(kListW, 0.0f), true);
    {
        bool hasOuter = (m_hitCondWinSel.outer < (int)groups.size());
        bool isEndSel = false;
        uint32_t innerAbsIdx = 0xFFFFFFFF;
        if (hasOuter) {
            innerAbsIdx = groups[m_hitCondWinSel.outer].first + (uint32_t)m_hitCondWinSel.inner;
            if (innerAbsIdx < (uint32_t)blk.size())
                isEndSel = isHcEnd(blk[innerAbsIdx]);
        }

        int innerCount = hasOuter ? (int)groups[m_hitCondWinSel.outer].second : 0;
        ListAction innerAct = ListHdr("##hc_im", "Hit-condition",
                                       "Hit Conditions", innerCount);
        ImGui::Separator();

        bool isOnlyEnd = isEndSel && hasOuter && (groups[m_hitCondWinSel.outer].second == 1);
        if (innerAct == ListAction::Insert && isEndSel && !isOnlyEnd) { innerAct = ListAction::None; m_endInsertBlocked = true; m_insertBlockedViewportId = ImGui::GetWindowViewport()->ID; }

        if (hasOuter && innerAct != ListAction::None && innerAbsIdx != 0xFFFFFFFF) {
            uint32_t gf = groups[m_hitCondWinSel.outer].first, gc = groups[m_hitCondWinSel.outer].second;
            if (innerAct == ListAction::Insert) {
                uint32_t ipos = isOnlyEnd ? innerAbsIdx : innerAbsIdx + 1;
                ParsedHitCondition nh{};
                nh.requirement_addr = 1;
                nh.req_list_idx = !reqBlk.empty() ? 0u : 0xFFFFFFFFu;
                blk.insert(blk.begin() + ipos, nh);
                FixupRef_HitCond(m_data, ipos, true);
                if (!isOnlyEnd) m_hitCondWinSel.inner++;
                m_dirty = true; groups = mkGroups();
            } else if (innerAct == ListAction::Duplicate && !isEndSel) {
                uint32_t ipos = gf + gc - 1;
                blk.insert(blk.begin() + ipos, blk[innerAbsIdx]);
                FixupRef_HitCond(m_data, ipos, true);
                m_dirty = true; groups = mkGroups();
            } else if (innerAct == ListAction::Remove && !isEndSel) {
                uint32_t cai = innerAbsIdx;
                auto doRem = [this, cai]() {
                    FixupRef_HitCond(m_data, cai, false);
                    m_data.hitConditionBlock.erase(m_data.hitConditionBlock.begin() + cai);
                    if (m_hitCondWinSel.inner > 0) m_hitCondWinSel.inner--;
                    m_dirty = true;
                };
                uint32_t refs = CountRefs_HitCond(m_data, cai);
                if (refs > 0) {
                    snprintf(m_removeConfirm.message, sizeof(m_removeConfirm.message),
                        "Hit-condition at index %u is referenced by %u location(s).\nRemove anyway?", cai, refs);
                    m_removeConfirm.onConfirm = doRem; m_removeConfirm.callerViewportId = ImGui::GetWindowViewport()->ID; m_removeConfirm.pending = true;
                } else { doRem(); groups = mkGroups(); }
            }
        }

        ImGui::BeginChild("##hc_inner_sl", ImVec2(0, 0), false);
        hasOuter = (m_hitCondWinSel.outer < (int)groups.size());
        if (hasOuter) {
            uint32_t start = groups[m_hitCondWinSel.outer].first;
            uint32_t count = groups[m_hitCondWinSel.outer].second;
            for (uint32_t k = 0; k < count; ++k) {
                uint32_t idx = start + k;
                if (idx >= (uint32_t)blk.size()) break;
                const ParsedHitCondition& h = blk[idx];
                bool isTerm = isHcEnd(h);
                char lbl[48];
                if (isTerm) snprintf(lbl, sizeof(lbl), "#%u  [END]##hci%u",   k, idx);
                else        snprintf(lbl, sizeof(lbl), "#%u  dmg=%u##hci%u",  k, h.damage, idx);
                if (isTerm) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f,0.5f,0.5f,1.0f));
                bool sel = (m_hitCondWinSel.inner == (int)k);
                if (ImGui::Selectable(lbl, sel)) m_hitCondWinSel.inner = (int)k;
                if (isTerm) ImGui::PopStyleColor();
            }
        }
        ImGui::EndChild();
    }
    ImGui::EndChild(); ImGui::SameLine();

    groups = mkGroups();

    // ---- Detail ----
    ImGui::BeginChild("##hc_detail", ImVec2(0.0f, 0.0f), false);
    if (m_hitCondWinSel.outer < (int)groups.size())
    {
        uint32_t start = groups[m_hitCondWinSel.outer].first;
        uint32_t idx   = start + (uint32_t)m_hitCondWinSel.inner;
        if (idx < (uint32_t)blk.size())
        {
            ParsedHitCondition& h = m_data.hitConditionBlock[idx];
            const ImGuiStyle& sty = ImGui::GetStyle();
            static constexpr ImVec4 kBlockBg = {0.14f, 0.14f, 0.18f, 1.00f};
            static constexpr ImVec4 kGreen   = {0.55f, 0.85f, 0.55f, 1.0f};
            static constexpr ImVec4 kPink    = {1.00f, 0.50f, 0.65f, 1.0f};
            static constexpr ImVec4 kTTCol   = {0.40f, 0.75f, 1.00f, 1.00f};
            static constexpr float  kTTH     = 34.0f;

            // GoButton helper
            const float kArrowW = ImGui::GetFrameHeight() * 0.38f;
            const float kBtnW   = ImGui::CalcTextSize("Go ").x + kArrowW
                                + sty.FramePadding.x * 2.0f + 4.0f;
            auto GoButton = [&](const char* id, bool valid) -> bool {
                if (!valid) ImGui::BeginDisabled();
                const float  hh  = ImGui::GetFrameHeight();
                bool clicked     = ImGui::InvisibleButton(id, ImVec2(kBtnW, hh));
                const ImVec2 p0  = ImGui::GetItemRectMin();
                const ImVec2 p1  = ImGui::GetItemRectMax();
                const bool   hov = ImGui::IsItemHovered();
                const bool   act = ImGui::IsItemActive();
                ImDrawList*  dl  = ImGui::GetWindowDrawList();
                dl->AddRectFilled(p0, p1,
                    ImGui::GetColorU32(act ? ImGuiCol_ButtonActive : hov ? ImGuiCol_ButtonHovered : ImGuiCol_Button),
                    sty.FrameRounding);
                dl->AddRect(p0, p1, ImGui::GetColorU32(ImGuiCol_Border), sty.FrameRounding);
                const ImU32  col    = ImGui::GetColorU32(ImGuiCol_Text);
                const char*  txt    = "Go ";
                const ImVec2 txtSz  = ImGui::CalcTextSize(txt);
                const float  totalW = txtSz.x + kArrowW;
                const float  startX = p0.x + (kBtnW - totalW) * 0.5f;
                const float  midY   = (p0.y + p1.y) * 0.5f;
                const float  arrowH = hh * 0.30f;
                dl->AddText(ImVec2(startX, midY - txtSz.y * 0.5f), col, txt);
                dl->AddTriangleFilled(
                    ImVec2(startX + txtSz.x,           midY - arrowH * 0.5f),
                    ImVec2(startX + txtSz.x,           midY + arrowH * 0.5f),
                    ImVec2(startX + txtSz.x + kArrowW, midY), col);
                if (!valid) ImGui::EndDisabled();
                return clicked && valid;
            };

            // --- Block 1: requirements ---
            // Height: TextLineH + FrameH + kTTH + spacing×2 + padding×2
            float b1H = ImGui::GetTextLineHeight() + ImGui::GetFrameHeight() + kTTH
                      + sty.ItemSpacing.y * 2.0f + sty.WindowPadding.y * 2.0f;
            ImGui::PushStyleColor(ImGuiCol_ChildBg, kBlockBg);
            if (ImGui::BeginChild("##hcdt_b1", ImVec2(-1.0f, b1H), ImGuiChildFlags_Borders))
            {
                bool reqValid = (h.req_list_idx != 0xFFFFFFFF) &&
                                (h.req_list_idx < (uint32_t)m_data.requirementBlock.size());
                ImGui::PushStyleColor(ImGuiCol_Text, reqValid ? kGreen : kPink);
                ImGui::TextUnformatted(HitCondLabel::Requirements); ShowFieldTooltip(FieldTT::HitCond::Requirements);
                ImGui::PopStyleColor();
                ImGui::SetNextItemWidth(-(kBtnW + sty.ItemSpacing.x));
                int reqTmp = (h.req_list_idx == 0xFFFFFFFF) ? -1 : (int)h.req_list_idx;
                if (ImGui::InputInt("##hcreq_val", &reqTmp, 0, 0)) {
                    h.req_list_idx = (reqTmp < 0) ? 0xFFFFFFFF : (uint32_t)reqTmp;
                    m_dirty = true;
                }
                ImGui::SameLine();
                if (GoButton("##hcreq_go", reqValid)) {
                    const auto& rblk = m_data.requirementBlock;
                    auto grps = ComputeGroups(rblk, +[](const ParsedRequirement& rr)->bool{
                        return rr.req == GameStatic::Get().data.reqListEnd; });
                    int gi = FindGroupOuter(grps, h.req_list_idx);
                    if (gi >= 0) { m_reqWinSel.outer = gi; m_reqWinSel.inner = 0; m_reqWinSel.scrollOuter = true; }
                    m_reqWinOpen = true;
                }
                // Tooltip area inside card
                ImGui::TextColored(kTTCol, " ");
            }
            ImGui::EndChild();
            ImGui::PopStyleColor();

            ImGui::Spacing();

            // --- Block 2: damage / _0x0C / reaction_list ---
            // Height: FrameH×3 + TextLineH + spacing×3 + padding×2
            float b2H = ImGui::GetFrameHeight() * 3.0f + ImGui::GetTextLineHeight()
                      + sty.ItemSpacing.y * 3.0f + sty.WindowPadding.y * 2.0f;
            ImGui::PushStyleColor(ImGuiCol_ChildBg, kBlockBg);
            if (ImGui::BeginChild("##hcdt_b2", ImVec2(-1.0f, b2H), ImGuiChildFlags_Borders))
            {
                // damage + _0x0C as 2-col label/input rows
                constexpr ImGuiTableFlags kTF = ImGuiTableFlags_SizingFixedFit;
                if (ImGui::BeginTable("##hcdt_tbl", 2, kTF, ImVec2(-1.0f, 0.0f))) {
                    ImGui::TableSetupColumn("##hl", ImGuiTableColumnFlags_WidthFixed);
                    ImGui::TableSetupColumn("##hv", ImGuiTableColumnFlags_WidthStretch);

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0); ImGui::TextDisabled("%s", HitCondLabel::Damage); ShowFieldTooltip(FieldTT::HitCond::Damage);
                    ImGui::TableSetColumnIndex(1); ImGui::SetNextItemWidth(-1.0f);
                    { int tmp = (int)h.damage; if (ImGui::InputInt("##hc_dmg", &tmp, 0, 0)) { h.damage = (uint32_t)tmp; m_dirty = true; } }

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0); ImGui::TextDisabled("%s", HitCondLabel::F0x0C); ShowFieldTooltip(FieldTT::HitCond::F0x0C);
                    ImGui::TableSetColumnIndex(1); ImGui::SetNextItemWidth(-1.0f);
                    { int tmp = (int)h._0x0C; if (ImGui::InputInt("##hc_f0c", &tmp, 0, 0)) { h._0x0C = (uint32_t)tmp; m_dirty = true; } }

                    ImGui::EndTable();
                }

                // reaction_list: label on top + input+Go below
                bool rlValid = (h.reaction_list_idx != 0xFFFFFFFF) &&
                               (h.reaction_list_idx < (uint32_t)m_data.reactionListBlock.size());
                ImGui::PushStyleColor(ImGuiCol_Text, rlValid ? kGreen : kPink);
                ImGui::TextUnformatted(HitCondLabel::ReactionList); ShowFieldTooltip(FieldTT::HitCond::ReactionList);
                ImGui::PopStyleColor();
                ImGui::SetNextItemWidth(-(kBtnW + sty.ItemSpacing.x));
                int rlTmp = (h.reaction_list_idx == 0xFFFFFFFF) ? -1 : (int)h.reaction_list_idx;
                if (ImGui::InputInt("##hcrl_val", &rlTmp, 0, 0)) {
                    h.reaction_list_idx = (rlTmp < 0) ? 0xFFFFFFFF : (uint32_t)rlTmp;
                    m_dirty = true;
                }
                ImGui::SameLine();
                if (GoButton("##hcrl_go", rlValid)) {
                    m_reacWin.open          = true;
                    m_reacWin.selectedIdx   = (int)h.reaction_list_idx;
                    m_reacWin.scrollPending = true;
                }
            }
            ImGui::EndChild();
            ImGui::PopStyleColor();

            // Plain tooltip space at bottom (no box)
            ImGui::Spacing();
            ImGui::TextColored(kTTCol, " ");
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
    if (!ImGui::Begin(WinId("Reaction Lists##blkwin").c_str(), &m_reacWin.open, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking))
    { ImGui::End(); return; }

    auto& block = m_data.reactionListBlock;
    int total = (int)block.size();

    ImGui::BeginChild("##rl_master", ImVec2(110.0f, 0.0f), true);
    {
        ListAction act = RenderListPlusMenu("##rl_pm", "Reaction-list");
        ImGui::SameLine(); ImGui::TextDisabled("%d", total);
        ImGui::Separator();

        bool hasSel = (m_reacWin.selectedIdx >= 0 && m_reacWin.selectedIdx < total);
        if (act == ListAction::Insert) {
            uint32_t ipos = hasSel ? (uint32_t)m_reacWin.selectedIdx + 1 : (uint32_t)total;
            ParsedReactionList nr{};
            for (int p = 0; p < 7; ++p) nr.pushback_idx[p] = 0xFFFFFFFF;
            block.insert(block.begin() + ipos, nr);
            FixupRef_ReactionList(m_data, ipos, true);
            m_reacWin.selectedIdx = (int)ipos;
            total = (int)block.size(); m_dirty = true;
        } else if (act == ListAction::Duplicate && hasSel) {
            block.push_back(block[m_reacWin.selectedIdx]);
            total = (int)block.size(); m_dirty = true;
        } else if (act == ListAction::Remove && hasSel) {
            uint32_t pos = (uint32_t)m_reacWin.selectedIdx;
            uint32_t refs = CountRefs_ReactionList(m_data, pos);
            auto doRem = [this, pos]() {
                FixupRef_ReactionList(m_data, pos, false);
                m_data.reactionListBlock.erase(m_data.reactionListBlock.begin() + pos);
                if (m_reacWin.selectedIdx >= (int)m_data.reactionListBlock.size())
                    m_reacWin.selectedIdx = (std::max)(0, (int)m_data.reactionListBlock.size() - 1);
                m_dirty = true;
            };
            if (refs > 0) {
                snprintf(m_removeConfirm.message, sizeof(m_removeConfirm.message),
                    "Reaction-list at index %u is referenced by %u location(s).\nRemove anyway?", pos, refs);
                m_removeConfirm.onConfirm = doRem; m_removeConfirm.callerViewportId = ImGui::GetWindowViewport()->ID; m_removeConfirm.pending = true;
            } else { doRem(); total = (int)block.size(); }
        }

        ImGui::BeginChild("##rl_master_sl", ImVec2(0, 0), false);
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
                if (RowU16Edit("##rl_fd",  ReactionLabel::FrontDirection,           rlm.front_direction,      FieldTT::Reaction::FrontDirection))           m_dirty = true;
                if (RowU16Edit("##rl_bd",  ReactionLabel::BackDirection,            rlm.back_direction,       FieldTT::Reaction::BackDirection))            m_dirty = true;
                if (RowU16Edit("##rl_lsd", ReactionLabel::LeftSideDirection,        rlm.left_side_direction,  FieldTT::Reaction::LeftSideDirection))        m_dirty = true;
                if (RowU16Edit("##rl_rsd", ReactionLabel::RightSideDirection,       rlm.right_side_direction, FieldTT::Reaction::RightSideDirection))       m_dirty = true;
                if (RowU16Edit("##rl_fcd", ReactionLabel::FrontCounterhitDirection, rlm.front_ch_direction,   FieldTT::Reaction::FrontCounterhitDirection)) m_dirty = true;
                if (RowU16Edit("##rl_dd",  ReactionLabel::DownedDirection,          rlm.downed_direction,     FieldTT::Reaction::DownedDirection))          m_dirty = true;
                if (RowU16Edit("##rl_fr",  ReactionLabel::FrontRotation,            rlm.front_rotation,       FieldTT::Reaction::FrontRotation))            m_dirty = true;
                if (RowU16Edit("##rl_br",  ReactionLabel::BackRotation,             rlm.back_rotation,        FieldTT::Reaction::BackRotation))             m_dirty = true;
                if (RowU16Edit("##rl_lsr", ReactionLabel::LeftSideRotation,         rlm.left_side_rotation,   FieldTT::Reaction::LeftSideRotation))         m_dirty = true;
                if (RowU16Edit("##rl_rsr", ReactionLabel::RightSideRotation,        rlm.right_side_rotation,  FieldTT::Reaction::RightSideRotation))        m_dirty = true;
                if (RowU16Edit("##rl_vp",  ReactionLabel::VerticalPushback,         rlm.vertical_pushback,    FieldTT::Reaction::VerticalPushback))          m_dirty = true;
                if (RowU16Edit("##rl_dr",  ReactionLabel::DownedRotation,           rlm.downed_rotation,      FieldTT::Reaction::DownedRotation))           m_dirty = true;
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
    if (!ImGui::Begin(WinId("Pushbacks##blkwin").c_str(), &m_pushbackWin.open, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking))
    { ImGui::End(); return; }

    auto& pb = m_data.pushbackBlock;
    auto& pe = m_data.pushbackExtraBlock;
    float rightW   = 160.0f;
    float leftW    = ImGui::GetContentRegionAvail().x - rightW - ImGui::GetStyle().ItemSpacing.x;

    ImGui::BeginChild("##pb_lm", ImVec2(leftW, 0.0f), false);
    {
        ImGui::BeginChild("##pblist", ImVec2(120.0f, 0.0f), true);
        {
            int pbTotal = (int)pb.size();
            ListAction pbAct = RenderListPlusMenu("##pb_pm", "Pushback");
            ImGui::SameLine(); ImGui::TextDisabled("(%d)", pbTotal);
            ImGui::Separator();

            bool hasPbSel = (m_pushbackWin.pushbackSel >= 0 && m_pushbackWin.pushbackSel < pbTotal);
            if (pbAct == ListAction::Insert) {
                uint32_t ipos = hasPbSel ? (uint32_t)m_pushbackWin.pushbackSel + 1 : (uint32_t)pbTotal;
                ParsedPushback np{}; np.pushback_extra_idx = 0xFFFFFFFF;
                pb.insert(pb.begin() + ipos, np);
                FixupRef_Pushback(m_data, ipos, true);
                m_pushbackWin.pushbackSel = (int)ipos;
                m_dirty = true;
            } else if (pbAct == ListAction::Duplicate && hasPbSel) {
                pb.push_back(pb[m_pushbackWin.pushbackSel]);
                m_dirty = true;
            } else if (pbAct == ListAction::Remove && hasPbSel) {
                uint32_t pos = (uint32_t)m_pushbackWin.pushbackSel;
                uint32_t refs = CountRefs_Pushback(m_data, pos);
                auto doRem = [this, pos]() {
                    FixupRef_Pushback(m_data, pos, false);
                    m_data.pushbackBlock.erase(m_data.pushbackBlock.begin() + pos);
                    if (m_pushbackWin.pushbackSel >= (int)m_data.pushbackBlock.size())
                        m_pushbackWin.pushbackSel = (std::max)(0, (int)m_data.pushbackBlock.size() - 1);
                    m_dirty = true;
                };
                if (refs > 0) {
                    snprintf(m_removeConfirm.message, sizeof(m_removeConfirm.message),
                        "Pushback at index %u is referenced by %u location(s).\nRemove anyway?", pos, refs);
                    m_removeConfirm.onConfirm = doRem; m_removeConfirm.callerViewportId = ImGui::GetWindowViewport()->ID; m_removeConfirm.pending = true;
                } else { doRem(); }
            }

            ImGui::BeginChild("##pblist_sl", ImVec2(0, 0), false);
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
        }
        ImGui::EndChild();

        ImGui::SameLine();
        ImGui::BeginChild("##pbdetail", ImVec2(0.0f, 0.0f), false);
        if (m_pushbackWin.pushbackSel >= 0 && m_pushbackWin.pushbackSel < (int)pb.size()) {
            ParsedPushback& p = m_data.pushbackBlock[m_pushbackWin.pushbackSel];
            ImGui::TextDisabled("Pushback #%d", m_pushbackWin.pushbackSel); ImGui::Separator();
            if (BeginPropTable("##pbdt")) {
                if (RowU16Edit("##pb_val1", PushbackLabel::NonLinearDisplacement, p.val1, FieldTT::Pushback::NonLinearDisplacement)) m_dirty = true;
                if (RowU16Edit("##pb_val2", PushbackLabel::NonLinearDistance,     p.val2, FieldTT::Pushback::NonLinearDistance))     m_dirty = true;
                if (RowU32Edit("##pb_val3", PushbackLabel::NumOfExtraPushbacks,   p.val3, FieldTT::Pushback::NumOfExtraPushbacks))   m_dirty = true;
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
    {
        ListAction peAct = RenderListPlusMenu("##pe_pm", "Pushback-extra");
        ImGui::SameLine(); ImGui::TextDisabled("(%d)", (int)pe.size());
        ImGui::Separator();

        bool hasPeSel = (m_pushbackWin.extraSel >= 0 && m_pushbackWin.extraSel < (int)pe.size());
        if (peAct == ListAction::Insert) {
            uint32_t ipos = hasPeSel ? (uint32_t)m_pushbackWin.extraSel + 1 : (uint32_t)pe.size();
            pe.insert(pe.begin() + ipos, ParsedPushbackExtra{});
            FixupRef_PushbackExtra(m_data, ipos, true);
            m_pushbackWin.extraSel = (int)ipos;
            m_dirty = true;
        } else if (peAct == ListAction::Duplicate && hasPeSel) {
            pe.push_back(pe[m_pushbackWin.extraSel]);
            m_dirty = true;
        } else if (peAct == ListAction::Remove && hasPeSel) {
            uint32_t pos = (uint32_t)m_pushbackWin.extraSel;
            uint32_t refs = CountRefs_PushbackExtra(m_data, pos);
            auto doRem = [this, pos]() {
                FixupRef_PushbackExtra(m_data, pos, false);
                m_data.pushbackExtraBlock.erase(m_data.pushbackExtraBlock.begin() + pos);
                if (m_pushbackWin.extraSel >= (int)m_data.pushbackExtraBlock.size())
                    m_pushbackWin.extraSel = (std::max)(0, (int)m_data.pushbackExtraBlock.size() - 1);
                m_dirty = true;
            };
            if (refs > 0) {
                snprintf(m_removeConfirm.message, sizeof(m_removeConfirm.message),
                    "Pushback-extra at index %u is referenced by %u location(s).\nRemove anyway?", pos, refs);
                m_removeConfirm.onConfirm = doRem; m_removeConfirm.callerViewportId = ImGui::GetWindowViewport()->ID; m_removeConfirm.pending = true;
            } else { doRem(); }
        }

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
    if (!ImGui::Begin(WinId("Voiceclips##blkwin").c_str(), &m_voiceclipWinOpen, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking))
    { ImGui::End(); return; }
    if (voiceclipBringFront) ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindow());

    auto& block = m_data.voiceclipBlock;
    int total = (int)block.size();
    auto vcIsEnd = [](const ParsedVoiceclip& vc) {
        return vc.val1 == 0xFFFFFFFFu && vc.val2 == 0xFFFFFFFFu && vc.val3 == 0xFFFFFFFFu;
    };

    ImGui::BeginChild("##vc_master", ImVec2(130.0f, 0.0f), true);
    {
        ListAction act = RenderListPlusMenu("##vc_pm", "Voiceclip");
        ImGui::SameLine(); ImGui::TextDisabled("%d", total);
        ImGui::Separator();

        bool hasSel = (m_voiceclipWinSel >= 0 && m_voiceclipWinSel < total);
        bool isEndSel = hasSel && vcIsEnd(block[m_voiceclipWinSel]);
        // isOnlyEnd: this [END] has no non-END items before it in its group → allow insert before it
        bool isOnlyEnd = isEndSel && (m_voiceclipWinSel == 0 || vcIsEnd(block[m_voiceclipWinSel - 1]));
        if (act == ListAction::Insert && isEndSel && !isOnlyEnd) { act = ListAction::None; m_endInsertBlocked = true; m_insertBlockedViewportId = ImGui::GetWindowViewport()->ID; }

        if (act == ListAction::Insert) {
            uint32_t ipos = isOnlyEnd ? (uint32_t)m_voiceclipWinSel
                          : hasSel    ? (uint32_t)m_voiceclipWinSel + 1
                          :             (uint32_t)total;
            ParsedVoiceclip nv{}; nv.val1 = 0; nv.val2 = 0; nv.val3 = 0;
            block.insert(block.begin() + ipos, nv);
            FixupRef_Voiceclip(m_data, ipos, true);
            m_voiceclipWinSel = (int)ipos;
            total = (int)block.size(); m_dirty = true;
        } else if (act == ListAction::Duplicate && hasSel && !isEndSel) {
            block.push_back(block[m_voiceclipWinSel]);
            total = (int)block.size(); m_dirty = true;
        } else if (act == ListAction::Remove && hasSel) {
            uint32_t pos = (uint32_t)m_voiceclipWinSel;
            uint32_t refs = CountRefs_Voiceclip(m_data, pos);
            auto doRem = [this, pos]() {
                FixupRef_Voiceclip(m_data, pos, false);
                m_data.voiceclipBlock.erase(m_data.voiceclipBlock.begin() + pos);
                if (m_voiceclipWinSel >= (int)m_data.voiceclipBlock.size())
                    m_voiceclipWinSel = (std::max)(0, (int)m_data.voiceclipBlock.size() - 1);
                m_dirty = true;
            };
            if (refs > 0) {
                snprintf(m_removeConfirm.message, sizeof(m_removeConfirm.message),
                    "Voiceclip at index %u is referenced by %u location(s).\nRemove anyway?", pos, refs);
                m_removeConfirm.onConfirm = doRem; m_removeConfirm.callerViewportId = ImGui::GetWindowViewport()->ID; m_removeConfirm.pending = true;
            } else { doRem(); total = (int)block.size(); }
        }

        ImGui::BeginChild("##vc_master_sl", ImVec2(0, 0), false);
        for (int i = 0; i < total; ++i) {
            const ParsedVoiceclip& vc = block[i];
            const bool isEnd = vcIsEnd(vc);
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
              if (RowI32Edit("##vc_val1", VoiceclipLabel::Folder, tmp, FieldTT::Voiceclip::Folder)) { vc.val1 = static_cast<uint32_t>(tmp); m_dirty = true; } }
            { int32_t tmp = static_cast<int32_t>(vc.val2);
              if (RowI32Edit("##vc_val2", VoiceclipLabel::Val2, tmp, FieldTT::Voiceclip::Val2)) { vc.val2 = static_cast<uint32_t>(tmp); m_dirty = true; } }
            { int32_t tmp = static_cast<int32_t>(vc.val3);
              if (RowI32Edit("##vc_val3", VoiceclipLabel::Clip, tmp, FieldTT::Voiceclip::Clip)) { vc.val3 = static_cast<uint32_t>(tmp); m_dirty = true; } }
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

// Navigation context passed to RenderPropSection for property-specific param links.
struct PropNavCtx {
    // 0x827b: projectile index
    const std::vector<ParsedProjectile>*           projBlk       = nullptr;
    bool*                                          projWinOpen   = nullptr;
    int*                                           projWinSel    = nullptr;
    bool*                                          projWinScroll = nullptr;
    // 0x868f: throw-extra index
    const std::vector<std::pair<uint32_t,uint32_t>>* teGroups    = nullptr;
    MovesetEditorWindow::TwoLevelSel*              throwExtraSel = nullptr;
    bool*                                          throwsWinOpen = nullptr;
};

static void RenderPropSection(
    const char* outerChildId, const char* innerChildId, const char* detailChildId,
    std::vector<ParsedExtraProp>& block,
    const std::vector<ParsedRequirement>& reqBlk,
    MovesetEditorWindow::TwoLevelSel& sel,
    const char* listLabel,
    bool isExtraProp,
    MovesetEditorWindow::TwoLevelSel& reqWinSel,
    bool& reqWinOpen,
    bool& dirty,
    PropNavCtx& navCtx,
    MotbinData& data,
    MovesetEditorWindow* win,
    void(*fixupFn)(MotbinData&, uint32_t, bool),
    uint32_t(*countFn)(const MotbinData&, uint32_t))
{
    const uint32_t reqEnd = GameStatic::Get().data.reqListEnd;
    auto isPropEnd = [&](const ParsedExtraProp& e) -> bool {
        return isExtraProp ? (e.type == 0 && e.id == 0) : (e.id == reqEnd);
    };
    auto mkGroups = [&]{ return ComputeGroups(block, isPropEnd); };
    auto groups = mkGroups();

    float colW = ImGui::GetContentRegionAvail().x / 3.0f - ImGui::GetStyle().ItemSpacing.x;

    // ---- Outer list ----
    ImGui::BeginChild(outerChildId, ImVec2(colW, 0.0f), true);
    {
        bool hasOuter = (sel.outer < (int)groups.size());
        char outerMenuId[80]; snprintf(outerMenuId, sizeof(outerMenuId), "##pop%s", outerChildId);
        ListAction outerAct = RenderListPlusMenu(outerMenuId, isExtraProp ? "Property-list" : "Fl-property-list");
        ImGui::SameLine(); ImGui::TextDisabled("%s (%d)", listLabel, (int)groups.size());
        ImGui::Separator();

        if (outerAct == ListAction::Insert) {
            uint32_t insertPos = hasOuter
                ? groups[sel.outer].first + groups[sel.outer].second
                : (uint32_t)block.size();
            ParsedExtraProp term{};
            if (!isExtraProp) term.id = reqEnd;
            block.insert(block.begin() + insertPos, term);
            fixupFn(data, insertPos, true);
            sel.outer = hasOuter ? sel.outer + 1 : 0; sel.inner = 0; dirty = true;
            groups = mkGroups();
        } else if (outerAct == ListAction::Duplicate && hasOuter) {
            uint32_t gf = groups[sel.outer].first, gc = groups[sel.outer].second;
            for (uint32_t k = 0; k < gc; ++k) block.push_back(block[gf + k]);
            dirty = true; groups = mkGroups();
        } else if (outerAct == ListAction::Remove && hasOuter) {
            uint32_t gf = groups[sel.outer].first, gc = groups[sel.outer].second;
            uint32_t refs = countFn(data, gf);
            int co = sel.outer;
            auto doRem = [&block, &data, &dirty, &sel, fixupFn, gf, gc, co]() {
                for (int i = (int)gc - 1; i >= 0; --i) {
                    uint32_t pos = gf + (uint32_t)i;
                    fixupFn(data, pos, false);
                    block.erase(block.begin() + pos);
                }
                sel.outer = (std::max)(0, co - 1); sel.inner = 0; dirty = true;
            };
            if (refs > 0) {
                snprintf(win->m_removeConfirm.message, sizeof(win->m_removeConfirm.message),
                    "Property-list at index %u is referenced by %u location(s).\nRemove anyway?", gf, refs);
                win->m_removeConfirm.onConfirm = doRem; win->m_removeConfirm.callerViewportId = ImGui::GetWindowViewport()->ID; win->m_removeConfirm.pending = true;
            } else { doRem(); groups = mkGroups(); }
        }

        ImGui::BeginChild((std::string(outerChildId) + "_sl").c_str(), ImVec2(0, 0), false);
        for (int gi = 0; gi < (int)groups.size(); ++gi) {
            uint32_t items = groups[gi].second > 0 ? groups[gi].second - 1 : 0;
            char lbl[48]; snprintf(lbl, sizeof(lbl), "[%u]  (%u)##pg%d", groups[gi].first, items, gi);
            bool s = (sel.outer == gi);
            if (ImGui::Selectable(lbl, s)) { sel.outer = gi; sel.inner = 0; }
            if (s && sel.scrollOuter) { ImGui::SetScrollHereY(0.5f); sel.scrollOuter = false; }
        }
        ImGui::EndChild();
    }
    ImGui::EndChild();

    ImGui::SameLine();
    groups = mkGroups();

    // ---- Inner list ----
    ImGui::BeginChild(innerChildId, ImVec2(colW, 0.0f), true);
    {
        bool hasOuter = (sel.outer < (int)groups.size());
        bool isEndSel = false;
        uint32_t innerAbsIdx = 0xFFFFFFFF;
        if (hasOuter) {
            innerAbsIdx = groups[sel.outer].first + (uint32_t)sel.inner;
            if (innerAbsIdx < (uint32_t)block.size())
                isEndSel = isPropEnd(block[innerAbsIdx]);
        }

        char innerMenuId[80]; snprintf(innerMenuId, sizeof(innerMenuId), "##pip%s", innerChildId);
        ListAction innerAct = RenderListPlusMenu(innerMenuId, "Property");
        ImGui::SameLine(); ImGui::TextDisabled("items");
        ImGui::Separator();

        bool isOnlyEnd = isEndSel && hasOuter && (groups[sel.outer].second == 1);
        if (innerAct == ListAction::Insert && isEndSel && !isOnlyEnd) { innerAct = ListAction::None; win->m_endInsertBlocked = true; win->m_insertBlockedViewportId = ImGui::GetWindowViewport()->ID; }

        if (hasOuter && innerAct != ListAction::None && innerAbsIdx != 0xFFFFFFFF) {
            uint32_t gf = groups[sel.outer].first, gc = groups[sel.outer].second;
            if (innerAct == ListAction::Insert) {
                uint32_t ipos = isOnlyEnd ? innerAbsIdx : innerAbsIdx + 1;
                ParsedExtraProp ne{}; ne.req_list_idx = 0xFFFFFFFF;
                if (isExtraProp) ne.id = 1; // non-zero id so (type==0,id==0) doesn't trigger END
                block.insert(block.begin() + ipos, ne);
                fixupFn(data, ipos, true);
                if (!isOnlyEnd) sel.inner++;
                dirty = true; groups = mkGroups();
            } else if (innerAct == ListAction::Duplicate && !isEndSel) {
                uint32_t ipos = gf + gc - 1;
                block.insert(block.begin() + ipos, block[innerAbsIdx]);
                fixupFn(data, ipos, true);
                dirty = true; groups = mkGroups();
            } else if (innerAct == ListAction::Remove && !isEndSel) {
                uint32_t cai = innerAbsIdx;
                uint32_t refs = countFn(data, cai);
                auto doRem = [&block, &data, &dirty, &sel, fixupFn, cai]() {
                    fixupFn(data, cai, false);
                    block.erase(block.begin() + cai);
                    if (sel.inner > 0) sel.inner--;
                    dirty = true;
                };
                if (refs > 0) {
                    snprintf(win->m_removeConfirm.message, sizeof(win->m_removeConfirm.message),
                        "Property at index %u is referenced by %u location(s).\nRemove anyway?", cai, refs);
                    win->m_removeConfirm.onConfirm = doRem; win->m_removeConfirm.callerViewportId = ImGui::GetWindowViewport()->ID; win->m_removeConfirm.pending = true;
                } else { doRem(); groups = mkGroups(); }
            }
        }

        ImGui::BeginChild((std::string(innerChildId) + "_sl").c_str(), ImVec2(0, 0), false);
        hasOuter = (sel.outer < (int)groups.size());
        if (hasOuter)
        {
            uint32_t start = groups[sel.outer].first;
            uint32_t count = groups[sel.outer].second;
            for (uint32_t k = 0; k < count; ++k)
            {
                uint32_t idx = start + k;
                if (idx >= (uint32_t)block.size()) break;
                const ParsedExtraProp& e = block[idx];
                bool isTerm = isPropEnd(e);
                char lbl[48];
                if (isTerm)
                    snprintf(lbl, sizeof(lbl), "#%u  [END]##pi%u", k, idx);
                else
                    snprintf(lbl, sizeof(lbl), "#%u  id=0x%X##pi%u", k, e.id, idx);
                if (isTerm) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f,0.5f,0.5f,1.0f));
                bool s = (sel.inner == (int)k);
                if (ImGui::Selectable(lbl, s)) sel.inner = (int)k;
                if (isTerm) ImGui::PopStyleColor();
            }
        }
        ImGui::EndChild();
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
                    if (RowU32Edit  ("##ep_type", ExtraPropLabel::Frame, e.type, FieldTT::ExtraProp::Frame)) dirty = true;
                    if (RowHex32Edit("##ep_0x4",  ExtraPropLabel::F0x4,  e._0x4, FieldTT::ExtraProp::F0x4)) dirty = true;
                }
                if (RowHexCompact32Edit("##ep_id", ExtraPropLabel::Property, e.id, FieldTT::ExtraProp::Property)) dirty = true;
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
                // params[0]: property-specific display; params[1-4]: decimal
                {
                    char p0lbl[64];
                    if (e.id == 0x87f8)
                    {
                        // hex input, format 0xAAAABBBB (AAAA=type, BBBB=id), 8-digit padded
                        snprintf(p0lbl, sizeof(p0lbl), "%s   (Dialogues)", ExtraPropLabel::Param0);
                        if (RowHex32Edit("##ep_v1_dlg", p0lbl, e.value, FieldTT::ExtraProp::Param0)) dirty = true;
                    }
                    else if (e.id == 0x877b)
                    {
                        // decimal index into dialogues (same concept as 0x87f8)
                        snprintf(p0lbl, sizeof(p0lbl), "%s   (Dialogues)", ExtraPropLabel::Param0);
                        if (RowU32Edit("##ep_v1_877b", p0lbl, e.value, FieldTT::ExtraProp::Param0)) dirty = true;
                    }
                    else if (e.id == 0x877d)
                    {
                        // hex value, full 8-digit padded (0x00000000)
                        snprintf(p0lbl, sizeof(p0lbl), "%s", ExtraPropLabel::Param0);
                        if (RowHex32Edit("##ep_v1_877d", p0lbl, e.value, FieldTT::ExtraProp::Param0)) dirty = true;
                    }
                    else if (e.id == 0x827b)
                    {
                        // projectile index
                        snprintf(p0lbl, sizeof(p0lbl), "%s   (Projectiles)", ExtraPropLabel::Param0);
                        bool valid = navCtx.projBlk && (e.value < (uint32_t)navCtx.projBlk->size());
                        auto r = RowIdxEditLink("##ep_v1_proj", p0lbl, e.value, valid);
                        if (r.changed) dirty = true;
                        if (r.navigate && navCtx.projWinOpen) {
                            *navCtx.projWinSel    = (int)e.value;
                            *navCtx.projWinScroll = true;
                            *navCtx.projWinOpen   = true;
                        }
                    }
                    else if (e.id == 0x868f)
                    {
                        // throw-extra index
                        snprintf(p0lbl, sizeof(p0lbl), "%s   (Throws)", ExtraPropLabel::Param0);
                        bool valid = navCtx.teGroups &&
                                     !navCtx.teGroups->empty() &&
                                     (e.value < (uint32_t)navCtx.teGroups->back().first + navCtx.teGroups->back().second);
                        auto r = RowIdxEditLink("##ep_v1_te", p0lbl, e.value, valid);
                        if (r.changed) dirty = true;
                        if (r.navigate && navCtx.throwExtraSel && navCtx.throwsWinOpen) {
                            int gi = FindGroupOuter(*navCtx.teGroups, e.value);
                            if (gi >= 0) { navCtx.throwExtraSel->outer = gi; navCtx.throwExtraSel->inner = 0; navCtx.throwExtraSel->scrollOuter = true; }
                            *navCtx.throwsWinOpen = true;
                        }
                    }
                    else
                    {
                        if (RowU32Edit("##ep_v1", ExtraPropLabel::Param0, e.value, FieldTT::ExtraProp::Param0)) dirty = true;
                    }
                }
                if (RowU32Edit("##ep_v2", ExtraPropLabel::Param1, e.value2, FieldTT::ExtraProp::Param1)) dirty = true;
                if (RowU32Edit("##ep_v3", ExtraPropLabel::Param2, e.value3, FieldTT::ExtraProp::Param2)) dirty = true;
                if (RowU32Edit("##ep_v4", ExtraPropLabel::Param3, e.value4, FieldTT::ExtraProp::Param3)) dirty = true;
                if (RowU32Edit("##ep_v5", ExtraPropLabel::Param4, e.value5, FieldTT::ExtraProp::Param4)) dirty = true;
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
    if (!ImGui::Begin(WinId("Properties##blkwin").c_str(), &m_propertiesWin.open, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking))
    { ImGui::End(); return; }
    if (propBringFront) ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindow());

    const auto& reqBlk = m_data.requirementBlock;

    // Pre-compute throw-extra groups for 0x868f navigation
    auto isTeEnd = +[](const ParsedThrowExtra& t) -> bool {
        return t.pick_probability == 0 && t.camera_type == 0 &&
               t.left_side_camera_data == 0 && t.right_side_camera_data == 0 &&
               t.additional_rotation == 0;
    };
    auto teGroups = ComputeGroups(m_data.throwExtraBlock, isTeEnd);

    PropNavCtx navCtx;
    navCtx.projBlk       = &m_data.projectileBlock;
    navCtx.projWinOpen   = &m_projectileWin.open;
    navCtx.projWinSel    = &m_projectileWin.selectedIdx;
    navCtx.projWinScroll = &m_projectileWin.scrollPending;
    navCtx.teGroups      = &teGroups;
    navCtx.throwExtraSel = &m_throwsWin.extraSel;
    navCtx.throwsWinOpen = &m_throwsWin.open;

    if (!ImGui::BeginTabBar("##prop_tabs")) { ImGui::End(); return; }

    // Consume pendingTab once so the flag fires for exactly one frame
    const int pendingTab = m_propertiesWin.pendingTab;
    m_propertiesWin.pendingTab = -1;

    if (ImGui::BeginTabItem("Extra", nullptr,
            pendingTab == 0 ? ImGuiTabItemFlags_SetSelected : 0))
    {
        RenderPropSection("##epo","##epi","##epd",
            m_data.extraPropBlock, reqBlk, m_propertiesWin.epSel,
            "property-lists", true, m_reqWinSel, m_reqWinOpen, m_dirty, navCtx,
            m_data, this, FixupRef_ExtraProp, CountRefs_ExtraProp);
        ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("Start", nullptr,
            pendingTab == 1 ? ImGuiTabItemFlags_SetSelected : 0))
    {
        RenderPropSection("##spo","##spi","##spd",
            m_data.startPropBlock, reqBlk, m_propertiesWin.spSel,
            "start-property-lists", false, m_reqWinSel, m_reqWinOpen, m_dirty, navCtx,
            m_data, this, FixupRef_StartProp, CountRefs_StartProp);
        ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("End", nullptr,
            pendingTab == 2 ? ImGuiTabItemFlags_SetSelected : 0))
    {
        RenderPropSection("##npo","##npi","##npd",
            m_data.endPropBlock, reqBlk, m_propertiesWin.npSel,
            "end-property-lists", false, m_reqWinSel, m_reqWinOpen, m_dirty, navCtx,
            m_data, this, FixupRef_EndProp, CountRefs_EndProp);
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
    if (!ImGui::Begin(WinId("Throws##blkwin").c_str(), &m_throwsWin.open, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking))
    { ImGui::End(); return; }

    auto& th  = m_data.throwBlock;
    auto& te  = m_data.throwExtraBlock;

    auto isTeEnd = +[](const ParsedThrowExtra& t) -> bool {
        return t.pick_probability == 0 && t.camera_type == 0 &&
               t.left_side_camera_data == 0 && t.right_side_camera_data == 0 &&
               t.additional_rotation == 0;
    };
    auto mkTeGroups = [&]{ return ComputeGroups(te, isTeEnd); };
    auto teGroups = mkTeGroups();

    const float spacing = ImGui::GetStyle().ItemSpacing.x;
    const float totalW  = ImGui::GetContentRegionAvail().x;
    const float leftW   = totalW * 0.25f;
    const float rightW  = totalW - leftW - spacing;
    const float listH   = 160.0f;

    // ---- Left: throw list + detail ----
    ImGui::BeginChild("##th_left", ImVec2(leftW, 0.0f), false);
    {
        ImGui::BeginChild("##thlist", ImVec2(0.0f, listH), true);
        {
            int thTotal = (int)th.size();
            ListAction thAct = RenderListPlusMenu("##th_pm", "Throw");
            ImGui::SameLine(); ImGui::TextDisabled("(%d)", thTotal);
            ImGui::Separator();

            bool hasThSel = (m_throwsWin.throwSel >= 0 && m_throwsWin.throwSel < thTotal);
            if (thAct == ListAction::Insert) {
                uint32_t ipos = hasThSel ? (uint32_t)m_throwsWin.throwSel + 1 : (uint32_t)thTotal;
                ParsedThrow nt{}; nt.throwextra_idx = 0xFFFFFFFF;
                th.insert(th.begin() + ipos, nt);
                m_throwsWin.throwSel = (int)ipos; m_dirty = true;
            } else if (thAct == ListAction::Duplicate && hasThSel) {
                th.push_back(th[m_throwsWin.throwSel]);
                m_dirty = true;
            } else if (thAct == ListAction::Remove && hasThSel) {
                uint32_t pos = (uint32_t)m_throwsWin.throwSel;
                // throws have no dedicated CountRefs — they're not indexed by other blocks
                auto doRem = [this, pos]() {
                    m_data.throwBlock.erase(m_data.throwBlock.begin() + pos);
                    if (m_throwsWin.throwSel >= (int)m_data.throwBlock.size())
                        m_throwsWin.throwSel = (std::max)(0, (int)m_data.throwBlock.size() - 1);
                    m_dirty = true;
                };
                doRem();
            }

            ImGui::BeginChild("##thlist_sl", ImVec2(0, 0), false);
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
        }
        ImGui::EndChild();

        if (m_throwsWin.throwSel >= 0 && m_throwsWin.throwSel < (int)th.size())
        {
            ParsedThrow& t = m_data.throwBlock[m_throwsWin.throwSel];
            ImGui::TextDisabled("throw #%d", m_throwsWin.throwSel); ImGui::Separator();
            if (BeginPropTable("##thdt"))
            {
                if (RowHex64Edit("##th_side", ThrowLabel::Side, t.side, FieldTT::Throw::Side)) m_dirty = true;
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
        {
            bool hasOuter = (m_throwsWin.extraSel.outer < (int)teGroups.size());
            ListAction outerAct = RenderListPlusMenu("##te_om", "ThrowExtra-list");
            ImGui::SameLine(); ImGui::TextDisabled("throw_extra lists (%d)", (int)teGroups.size());
            ImGui::Separator();

            if (outerAct == ListAction::Insert) {
                uint32_t insertPos = hasOuter
                    ? teGroups[m_throwsWin.extraSel.outer].first + teGroups[m_throwsWin.extraSel.outer].second
                    : (uint32_t)te.size();
                te.insert(te.begin() + insertPos, ParsedThrowExtra{});  // all zero → [END]
                FixupRef_ThrowExtra(m_data, insertPos, true);
                m_throwsWin.extraSel.outer = hasOuter ? m_throwsWin.extraSel.outer + 1 : 0;
                m_throwsWin.extraSel.inner = 0; m_dirty = true;
                teGroups = mkTeGroups();
            } else if (outerAct == ListAction::Duplicate && hasOuter) {
                uint32_t gf = teGroups[m_throwsWin.extraSel.outer].first, gc = teGroups[m_throwsWin.extraSel.outer].second;
                for (uint32_t k = 0; k < gc; ++k) te.push_back(te[gf + k]);
                m_dirty = true; teGroups = mkTeGroups();
            } else if (outerAct == ListAction::Remove && hasOuter) {
                uint32_t gf = teGroups[m_throwsWin.extraSel.outer].first, gc = teGroups[m_throwsWin.extraSel.outer].second;
                uint32_t refs = CountRefs_ThrowExtra(m_data, gf);
                int co = m_throwsWin.extraSel.outer;
                auto doRem = [this, gf, gc, co]() {
                    for (int i = (int)gc - 1; i >= 0; --i) {
                        uint32_t pos = gf + (uint32_t)i;
                        FixupRef_ThrowExtra(m_data, pos, false);
                        m_data.throwExtraBlock.erase(m_data.throwExtraBlock.begin() + pos);
                    }
                    m_throwsWin.extraSel.outer = (std::max)(0, co - 1); m_throwsWin.extraSel.inner = 0; m_dirty = true;
                };
                if (refs > 0) {
                    snprintf(m_removeConfirm.message, sizeof(m_removeConfirm.message),
                        "ThrowExtra-list at index %u is referenced by %u location(s).\nRemove anyway?", gf, refs);
                    m_removeConfirm.onConfirm = doRem; m_removeConfirm.callerViewportId = ImGui::GetWindowViewport()->ID; m_removeConfirm.pending = true;
                } else { doRem(); teGroups = mkTeGroups(); }
            }

            ImGui::BeginChild("##te_outer_sl", ImVec2(0, 0), false);
            for (int gi = 0; gi < (int)teGroups.size(); ++gi) {
                uint32_t items = teGroups[gi].second > 0 ? teGroups[gi].second - 1 : 0;
                char lbl[48]; snprintf(lbl, sizeof(lbl), "[%u]  (%u)##teg%d", teGroups[gi].first, items, gi);
                bool s = (m_throwsWin.extraSel.outer == gi);
                if (ImGui::Selectable(lbl, s)) { m_throwsWin.extraSel.outer = gi; m_throwsWin.extraSel.inner = 0; }
                if (s && m_throwsWin.extraSel.scrollOuter) { ImGui::SetScrollHereY(0.5f); m_throwsWin.extraSel.scrollOuter = false; }
            }
            ImGui::EndChild();
        }
        ImGui::EndChild();

        ImGui::SameLine();
        teGroups = mkTeGroups();

        // Inner items
        ImGui::BeginChild("##te_inner", ImVec2(colW, 0.0f), true);
        {
            bool hasOuter = (m_throwsWin.extraSel.outer < (int)teGroups.size());
            bool isEndSel = false;
            uint32_t innerAbsIdx = 0xFFFFFFFF;
            if (hasOuter) {
                innerAbsIdx = teGroups[m_throwsWin.extraSel.outer].first + (uint32_t)m_throwsWin.extraSel.inner;
                if (innerAbsIdx < (uint32_t)te.size())
                    isEndSel = isTeEnd(te[innerAbsIdx]);
            }

            ListAction innerAct = RenderListPlusMenu("##te_im", "ThrowExtra");
            ImGui::SameLine(); ImGui::TextDisabled("items");
            ImGui::Separator();

            bool isOnlyEnd = isEndSel && hasOuter && (teGroups[m_throwsWin.extraSel.outer].second == 1);
            if (innerAct == ListAction::Insert && isEndSel && !isOnlyEnd) { innerAct = ListAction::None; m_endInsertBlocked = true; m_insertBlockedViewportId = ImGui::GetWindowViewport()->ID; }

            if (hasOuter && innerAct != ListAction::None && innerAbsIdx != 0xFFFFFFFF) {
                uint32_t gf = teGroups[m_throwsWin.extraSel.outer].first, gc = teGroups[m_throwsWin.extraSel.outer].second;
                if (innerAct == ListAction::Insert) {
                    uint32_t ipos = isOnlyEnd ? innerAbsIdx : innerAbsIdx + 1;
                    ParsedThrowExtra nte{}; nte.pick_probability = 1; // non-zero so not [END]
                    te.insert(te.begin() + ipos, nte);
                    FixupRef_ThrowExtra(m_data, ipos, true);
                    if (!isOnlyEnd) m_throwsWin.extraSel.inner++;
                    m_dirty = true; teGroups = mkTeGroups();
                } else if (innerAct == ListAction::Duplicate && !isEndSel) {
                    uint32_t ipos = gf + gc - 1;
                    te.insert(te.begin() + ipos, te[innerAbsIdx]);
                    FixupRef_ThrowExtra(m_data, ipos, true);
                    m_dirty = true; teGroups = mkTeGroups();
                } else if (innerAct == ListAction::Remove && !isEndSel) {
                    uint32_t cai = innerAbsIdx;
                    uint32_t refs = CountRefs_ThrowExtra(m_data, cai);
                    auto doRem = [this, cai]() {
                        FixupRef_ThrowExtra(m_data, cai, false);
                        m_data.throwExtraBlock.erase(m_data.throwExtraBlock.begin() + cai);
                        if (m_throwsWin.extraSel.inner > 0) m_throwsWin.extraSel.inner--;
                        m_dirty = true;
                    };
                    if (refs > 0) {
                        snprintf(m_removeConfirm.message, sizeof(m_removeConfirm.message),
                            "ThrowExtra at index %u is referenced by %u location(s).\nRemove anyway?", cai, refs);
                        m_removeConfirm.onConfirm = doRem; m_removeConfirm.callerViewportId = ImGui::GetWindowViewport()->ID; m_removeConfirm.pending = true;
                    } else { doRem(); teGroups = mkTeGroups(); }
                }
            }

            ImGui::BeginChild("##te_inner_sl", ImVec2(0, 0), false);
            hasOuter = (m_throwsWin.extraSel.outer < (int)teGroups.size());
            if (hasOuter)
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
                    if (RowU32Edit("##te_prob", ThrowExtraLabel::PickProbability,     tex.pick_probability,       FieldTT::ThrowExtra::PickProbability))     m_dirty = true;
                    if (RowU16Edit("##te_cam",  ThrowExtraLabel::CameraType,          tex.camera_type,            FieldTT::ThrowExtra::CameraType))          m_dirty = true;
                    if (RowU16Edit("##te_lsc",  ThrowExtraLabel::LeftSideCameraData,  tex.left_side_camera_data,  FieldTT::ThrowExtra::LeftSideCameraData))  m_dirty = true;
                    if (RowU16Edit("##te_rsc",  ThrowExtraLabel::RightSideCameraData, tex.right_side_camera_data, FieldTT::ThrowExtra::RightSideCameraData)) m_dirty = true;
                    if (RowU16Edit("##te_rot",  ThrowExtraLabel::AdditionalRotation,  tex.additional_rotation,    FieldTT::ThrowExtra::AdditionalRotation))  m_dirty = true;
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
    if (!ImGui::Begin(WinId("Projectiles##blkwin").c_str(), &m_projectileWin.open, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking))
    { ImGui::End(); return; }

    auto& block = m_data.projectileBlock;
    const float leftW = 110.0f;

    ImGui::BeginChild("##pjlist", ImVec2(leftW, 0.0f), true);
    {
        int pjTotal = (int)block.size();
        ListAction act = RenderListPlusMenu("##pj_pm", "Projectile");
        ImGui::SameLine(); ImGui::TextDisabled("(%d)", pjTotal);
        ImGui::Separator();

        bool hasSel = (m_projectileWin.selectedIdx >= 0 && m_projectileWin.selectedIdx < pjTotal);
        if (act == ListAction::Insert) {
            uint32_t ipos = hasSel ? (uint32_t)m_projectileWin.selectedIdx + 1 : (uint32_t)pjTotal;
            ParsedProjectile np{}; np.hit_condition_idx = 0xFFFFFFFF; np.cancel_idx = 0xFFFFFFFF;
            block.insert(block.begin() + ipos, np);
            // Projectiles are not referenced by fixup functions (only projectile refs cancel/hitcond)
            m_projectileWin.selectedIdx = (int)ipos; m_dirty = true;
        } else if (act == ListAction::Duplicate && hasSel) {
            block.push_back(block[m_projectileWin.selectedIdx]);
            m_dirty = true;
        } else if (act == ListAction::Remove && hasSel) {
            uint32_t pos = (uint32_t)m_projectileWin.selectedIdx;
            // projectiles are not referenced by other blocks
            block.erase(block.begin() + pos);
            if (m_projectileWin.selectedIdx >= (int)block.size())
                m_projectileWin.selectedIdx = (std::max)(0, (int)block.size() - 1);
            m_dirty = true;
        }

        ImGui::BeginChild("##pjlist_sl", ImVec2(0, 0), false);
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
    if (!ImGui::Begin(WinId("Input Sequences##blkwin").c_str(), &m_inputSeqWin.open, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking))
    { ImGui::End(); return; }

    auto& seqs = m_data.inputSequenceBlock;
    auto& inps = m_data.inputBlock;

    const float spacing = ImGui::GetStyle().ItemSpacing.x;
    const float totalW  = ImGui::GetContentRegionAvail().x;
    const float leftW   = totalW * 0.40f;
    const float rightW  = totalW - leftW - spacing;
    const float listH   = 150.0f;

    // ---- Left: sequence list + detail ----
    ImGui::BeginChild("##iseq_left", ImVec2(leftW, 0.0f), false);
    {
        ImGui::BeginChild("##iseqlist", ImVec2(0.0f, listH), true);
        {
            int seqTotal = (int)seqs.size();
            ListAction act = RenderListPlusMenu("##iseq_pm", "Input-sequence");
            ImGui::SameLine(); ImGui::TextDisabled("(%d)", seqTotal);
            ImGui::Separator();

            bool hasSel = (m_inputSeqWin.sel.outer >= 0 && m_inputSeqWin.sel.outer < seqTotal);
            if (act == ListAction::Insert) {
                uint32_t ipos = hasSel ? (uint32_t)m_inputSeqWin.sel.outer + 1 : (uint32_t)seqTotal;
                ParsedInputSequence ns{}; ns.input_start_idx = 0xFFFFFFFF;
                seqs.insert(seqs.begin() + ipos, ns);
                m_inputSeqWin.sel.outer = (int)ipos; m_inputSeqWin.sel.inner = 0;
                m_dirty = true;
            } else if (act == ListAction::Duplicate && hasSel) {
                seqs.push_back(seqs[m_inputSeqWin.sel.outer]);
                m_dirty = true;
            } else if (act == ListAction::Remove && hasSel) {
                seqs.erase(seqs.begin() + m_inputSeqWin.sel.outer);
                if (m_inputSeqWin.sel.outer >= (int)seqs.size())
                    m_inputSeqWin.sel.outer = (std::max)(0, (int)seqs.size() - 1);
                m_inputSeqWin.sel.inner = 0; m_dirty = true;
            }

            ImGui::BeginChild("##iseqlist_sl", ImVec2(0, 0), false);
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
        }
        ImGui::EndChild();

        if (m_inputSeqWin.sel.outer >= 0 && m_inputSeqWin.sel.outer < (int)seqs.size())
        {
            ParsedInputSequence& s = m_data.inputSequenceBlock[m_inputSeqWin.sel.outer];
            ImGui::TextDisabled("input_sequence #%d", m_inputSeqWin.sel.outer); ImGui::Separator();
            if (BeginPropTable("##iseqdt"))
            {
                if (RowU16Edit("##is_wf",  InputSeqLabel::InputWindowFrames, s.input_window_frames, FieldTT::InputSeq::InputWindowFrames)) m_dirty = true;
                if (RowU16Edit("##is_amt", InputSeqLabel::InputAmount,       s.input_amount,        FieldTT::InputSeq::InputAmount))       m_dirty = true;
                if (RowU32Edit("##is_0x4", InputSeqLabel::F0x4,              s._0x4,                FieldTT::InputSeq::F0x4))              m_dirty = true;
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
        ParsedInputSequence& s = seqs[m_inputSeqWin.sel.outer];

        // + button for inputs
        ListAction inpAct = RenderListPlusMenu("##inp_pm", "Input");
        ImGui::SameLine();
        ImGui::TextDisabled("inputs  (seq #%d,  amount=%u)", m_inputSeqWin.sel.outer, s.input_amount);
        ImGui::Separator();

        bool hasInpSel = (m_inputSeqWin.sel.inner >= 0 && s.input_start_idx != 0xFFFFFFFF &&
                          (uint32_t)m_inputSeqWin.sel.inner < s.input_amount);
        if (inpAct == ListAction::Insert) {
            uint32_t ipos = (s.input_start_idx != 0xFFFFFFFF)
                ? s.input_start_idx + (hasInpSel ? (uint32_t)m_inputSeqWin.sel.inner + 1 : s.input_amount)
                : (uint32_t)inps.size();
            if (s.input_start_idx == 0xFFFFFFFF) s.input_start_idx = ipos;
            inps.insert(inps.begin() + ipos, ParsedInput{});
            FixupRef_Input(m_data, ipos, true);
            s.input_amount++; m_dirty = true;
            if (hasInpSel) m_inputSeqWin.sel.inner++;
        } else if (inpAct == ListAction::Duplicate && hasInpSel) {
            uint32_t absSrc = s.input_start_idx + (uint32_t)m_inputSeqWin.sel.inner;
            uint32_t ipos = s.input_start_idx + s.input_amount;
            inps.insert(inps.begin() + ipos, inps[absSrc]);
            FixupRef_Input(m_data, ipos, true);
            s.input_amount++; m_dirty = true;
        } else if (inpAct == ListAction::Remove && hasInpSel) {
            uint32_t absPos = s.input_start_idx + (uint32_t)m_inputSeqWin.sel.inner;
            inps.erase(inps.begin() + absPos);
            // Manual fixup: decrement start indices > absPos; don't nullify == absPos
            // (erasing first input keeps input_start_idx pointing to the now-shifted next item)
            for (auto& sq : m_data.inputSequenceBlock) {
                if (sq.input_start_idx != 0xFFFFFFFF && sq.input_start_idx > absPos)
                    sq.input_start_idx--;
            }
            s.input_amount--;
            if (m_inputSeqWin.sel.inner > 0 && (uint32_t)m_inputSeqWin.sel.inner >= s.input_amount)
                m_inputSeqWin.sel.inner--;
            m_dirty = true;
        }

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
    if (!ImGui::Begin(WinId("Parryable Moves##blkwin").c_str(), &m_parryWinOpen, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking))
    { ImGui::End(); return; }

    auto& block = m_data.parryableMoveBlock;
    auto isPmEnd = +[](const ParsedParryableMove& pm) -> bool { return pm.value == 0; };
    auto mkPmGroups = [&]{ return ComputeGroups(block, isPmEnd); };
    auto groups = mkPmGroups();

    float colW = ImGui::GetContentRegionAvail().x / 3.0f - ImGui::GetStyle().ItemSpacing.x;

    // Outer list
    ImGui::BeginChild("##pm_outer", ImVec2(colW, 0.0f), true);
    {
        bool hasOuter = (m_parryWinSel.outer < (int)groups.size());
        ListAction outerAct = RenderListPlusMenu("##pm_om", "ParryableMove-list");
        ImGui::SameLine(); ImGui::TextDisabled("lists (%d)", (int)groups.size());
        ImGui::Separator();

        if (outerAct == ListAction::Insert) {
            uint32_t insertPos = hasOuter
                ? groups[m_parryWinSel.outer].first + groups[m_parryWinSel.outer].second
                : (uint32_t)block.size();
            ParsedParryableMove term{}; term.value = 0;
            block.insert(block.begin() + insertPos, term);
            m_parryWinSel.outer = hasOuter ? m_parryWinSel.outer + 1 : 0;
            m_parryWinSel.inner = 0; m_dirty = true;
            groups = mkPmGroups();
        } else if (outerAct == ListAction::Duplicate && hasOuter) {
            uint32_t gf = groups[m_parryWinSel.outer].first, gc = groups[m_parryWinSel.outer].second;
            for (uint32_t k = 0; k < gc; ++k) block.push_back(block[gf + k]);
            m_dirty = true; groups = mkPmGroups();
        } else if (outerAct == ListAction::Remove && hasOuter) {
            uint32_t gf = groups[m_parryWinSel.outer].first, gc = groups[m_parryWinSel.outer].second;
            int co = m_parryWinSel.outer;
            // parryable moves not referenced by other blocks
            for (int i = (int)gc - 1; i >= 0; --i)
                block.erase(block.begin() + gf + (uint32_t)i);
            m_parryWinSel.outer = (std::max)(0, co - 1); m_parryWinSel.inner = 0; m_dirty = true;
            groups = mkPmGroups();
        }

        ImGui::BeginChild("##pm_outer_sl", ImVec2(0, 0), false);
        for (int gi = 0; gi < (int)groups.size(); ++gi) {
            uint32_t items = groups[gi].second > 0 ? groups[gi].second - 1 : 0;
            char lbl[48]; snprintf(lbl, sizeof(lbl), "[%u]  (%u)##pmg%d", groups[gi].first, items, gi);
            bool s = (m_parryWinSel.outer == gi);
            if (ImGui::Selectable(lbl, s)) { m_parryWinSel.outer = gi; m_parryWinSel.inner = 0; }
            if (s && m_parryWinSel.scrollOuter) { ImGui::SetScrollHereY(0.5f); m_parryWinSel.scrollOuter = false; }
        }
        ImGui::EndChild();
    }
    ImGui::EndChild();

    ImGui::SameLine();
    groups = mkPmGroups();

    // Inner list
    ImGui::BeginChild("##pm_inner", ImVec2(colW, 0.0f), true);
    {
        bool hasOuter = (m_parryWinSel.outer < (int)groups.size());
        bool isEndSel = false;
        uint32_t innerAbsIdx = 0xFFFFFFFF;
        if (hasOuter) {
            innerAbsIdx = groups[m_parryWinSel.outer].first + (uint32_t)m_parryWinSel.inner;
            if (innerAbsIdx < (uint32_t)block.size())
                isEndSel = isPmEnd(block[innerAbsIdx]);
        }

        ListAction innerAct = RenderListPlusMenu("##pm_im", "ParryableMove");
        ImGui::SameLine(); ImGui::TextDisabled("items");
        ImGui::Separator();

        bool isOnlyEnd = isEndSel && hasOuter && (groups[m_parryWinSel.outer].second == 1);
        if (innerAct == ListAction::Insert && isEndSel && !isOnlyEnd) { innerAct = ListAction::None; m_endInsertBlocked = true; m_insertBlockedViewportId = ImGui::GetWindowViewport()->ID; }

        if (hasOuter && innerAct != ListAction::None && innerAbsIdx != 0xFFFFFFFF) {
            uint32_t gf = groups[m_parryWinSel.outer].first, gc = groups[m_parryWinSel.outer].second;
            if (innerAct == ListAction::Insert) {
                uint32_t ipos = isOnlyEnd ? innerAbsIdx : innerAbsIdx + 1;
                ParsedParryableMove nm{}; nm.value = 1; // non-zero so not [END]
                block.insert(block.begin() + ipos, nm);
                if (!isOnlyEnd) m_parryWinSel.inner++;
                m_dirty = true; groups = mkPmGroups();
            } else if (innerAct == ListAction::Duplicate && !isEndSel) {
                uint32_t ipos = gf + gc - 1;
                block.insert(block.begin() + ipos, block[innerAbsIdx]);
                m_dirty = true; groups = mkPmGroups();
            } else if (innerAct == ListAction::Remove && !isEndSel) {
                block.erase(block.begin() + innerAbsIdx);
                if (m_parryWinSel.inner > 0) m_parryWinSel.inner--;
                m_dirty = true; groups = mkPmGroups();
            }
        }

        ImGui::BeginChild("##pm_inner_sl", ImVec2(0, 0), false);
        hasOuter = (m_parryWinSel.outer < (int)groups.size());
        if (hasOuter)
        {
            uint32_t start = groups[m_parryWinSel.outer].first;
            uint32_t count = groups[m_parryWinSel.outer].second;
            for (uint32_t k = 0; k < count; ++k)
            {
                uint32_t idx = start + k;
                if (idx >= (uint32_t)block.size()) break;
                const ParsedParryableMove& pm = block[idx];
                const bool isEnd = isPmEnd(pm);
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
                if (RowU32Edit("##pm_val", ParryableMoveLabel::Value, pm.value, FieldTT::Parry::Value)) m_dirty = true;
                ImGui::EndTable();
            }
        }
    }
    ImGui::EndChild();

    ImGui::End();
}

// -------------------------------------------------------------
//  Dialogues subwindow  (flat list)
//  tk_dialogue: type / id / _0x4 / requirements / voiceclip_key / facial_anim_idx
// -------------------------------------------------------------

void MovesetEditorWindow::RenderSubWin_Dialogues()
{
    if (!m_dialogueWinOpen) return;
    ImGui::SetNextWindowSize(ImVec2(520.0f, 400.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(WinId("Dialogues##blkwin").c_str(), &m_dialogueWinOpen,
                      ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking))
    { ImGui::End(); return; }

    auto& block = m_data.dialogueBlock;
    int total = (int)block.size();

    // Left: flat list
    ImGui::BeginChild("##dlg_list", ImVec2(130.0f, 0.0f), true);
    {
        ListAction act = RenderListPlusMenu("##dlg_pm", "Dialogue");
        ImGui::SameLine(); ImGui::TextDisabled("%d", total);
        ImGui::Separator();

        bool hasSel = (m_dialogueSel >= 0 && m_dialogueSel < total);
        if (act == ListAction::Insert) {
            uint32_t ipos = hasSel ? (uint32_t)m_dialogueSel + 1 : (uint32_t)total;
            ParsedDialogue nd{}; nd.req_list_idx = 0xFFFFFFFF;
            block.insert(block.begin() + ipos, nd);
            m_dialogueSel = (int)ipos;
            total = (int)block.size(); m_dirty = true;
        } else if (act == ListAction::Duplicate && hasSel) {
            block.push_back(block[m_dialogueSel]);
            total = (int)block.size(); m_dirty = true;
        } else if (act == ListAction::Remove && hasSel) {
            uint32_t pos = (uint32_t)m_dialogueSel;
            // dialogues are not referenced by other blocks
            block.erase(block.begin() + pos);
            if (m_dialogueSel >= (int)block.size())
                m_dialogueSel = (std::max)(0, (int)block.size() - 1);
            total = (int)block.size(); m_dirty = true;
        }

        ImGui::BeginChild("##dlg_list_sl", ImVec2(0, 0), false);
        for (int i = 0; i < total; ++i) {
            const ParsedDialogue& d = block[i];
            char lbl[48];
            snprintf(lbl, sizeof(lbl), "#%d  t:%u id:%u##dlgi%d", i, d.type, d.id, i);
            bool sel = (m_dialogueSel == i);
            if (ImGui::Selectable(lbl, sel)) m_dialogueSel = i;
        }
        ImGui::EndChild();
    }
    ImGui::EndChild();

    ImGui::SameLine();

    // Right: detail
    ImGui::BeginChild("##dlg_detail", ImVec2(0.0f, 0.0f), false);
    if (m_dialogueSel >= 0 && m_dialogueSel < total) {
        ParsedDialogue& d = m_data.dialogueBlock[m_dialogueSel];
        ImGui::TextDisabled("dialogue #%d", m_dialogueSel); ImGui::Separator();
        if (BeginPropTable("##dlgdt")) {
            { uint32_t tmp = d.type;
              if (RowU32Edit("##dlg_type", DialogueLabel::Type, tmp, FieldTT::Dialogue::Type)) { d.type = (uint16_t)tmp; m_dirty = true; } }
            { uint32_t tmp = d.id;
              if (RowU32Edit("##dlg_id", DialogueLabel::Id, tmp, FieldTT::Dialogue::Id)) { d.id = (uint16_t)tmp; m_dirty = true; } }
            if (RowU32Edit("##dlg_0x4", DialogueLabel::F0x4, d._0x4, FieldTT::Dialogue::F0x4)) m_dirty = true;
            {
                bool valid = (d.req_list_idx != 0xFFFFFFFF) &&
                             (d.req_list_idx < (uint32_t)m_data.requirementBlock.size());
                auto r = RowIdxEditLink("##dlg_req", DialogueLabel::Requirements, d.req_list_idx, valid);
                if (r.changed) m_dirty = true;
                if (r.navigate) {
                    const auto& blk = m_data.requirementBlock;
                    auto grps = ComputeGroups(blk, +[](const ParsedRequirement& rr)->bool{ return rr.req==GameStatic::Get().data.reqListEnd; });
                    int gi = FindGroupOuter(grps, d.req_list_idx);
                    if (gi >= 0) { m_reqWinSel.outer = gi; m_reqWinSel.inner = 0; m_reqWinSel.scrollOuter = true; }
                    m_reqWinOpen = true;
                }
            }
            if (RowU32Edit("##dlg_vckey", DialogueLabel::VoiceclipKey, d.voiceclip_key,   FieldTT::Dialogue::VoiceclipKey))  m_dirty = true;
            if (RowU32Edit("##dlg_fanim", DialogueLabel::FacialAnimIdx, d.facial_anim_idx, FieldTT::Dialogue::FacialAnimIdx)) m_dirty = true;
            ImGui::EndTable();
        }
    }
    ImGui::EndChild();

    ImGui::End();
}
