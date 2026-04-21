// MovesetEditorWindow.cpp
// UI layout mirrors OldTool (TKMovesets2):
//   Left panel  -- searchable move list
//   Right panel -- collapsible property sections per move
#include "moveset/editor/MovesetEditorWindow.h"
#include "GameStatic.h"
#include "moveset/labels/LabelDB.h"
#include "moveset/data/MovesetDataDict.h"
#include "moveset/data/EditorFieldLabel.h"
#include "moveset/labels/FieldTooltips.h"
#include "moveset/live/GameLiveEdit.h"
#include "moveset/data/KamuiHash.h"
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

    // Pre-create AnimMgr instance so anmbin can be parsed on first access.
    // Callbacks and member pointers are set in SetD3DContext(), which runs after
    // this object has been moved into its final location in the editor window list.
    m_animMgr = std::make_unique<AnimationManagerWindow>(
                    m_data.folderPath, m_movesetName, m_uid);
    // m_animMgrVisible remains false — window won't render until user opens it
}

// -------------------------------------------------------------
//  SetD3DContext
// -------------------------------------------------------------

void MovesetEditorWindow::SetD3DContext(ID3D11Device* dev, ID3D11DeviceContext* ctx)
{
    m_d3dDev = dev;
    m_d3dCtx = ctx;
    if (m_animMgr) {
        // Complete AnimMgr setup here — after MovesetEditorWindow has been moved
        // into its final location, so 'this' and all member addresses are stable.
        {
            std::vector<uint32_t> keys;
            keys.reserve(m_data.moves.size());
            for (const auto& mv : m_data.moves) keys.push_back(mv.anim_key);
            m_animMgr->SetMotbinAnimKeys(keys);
        }
        m_animMgr->SetAnimNameDB(&m_animNameDB);
        m_animMgr->SetCharaCode(m_data.charaCode);
        m_animMgr->SetMoves(&m_data.moves);
        m_animMgr->SetOnAnimAdded([this](int /*cat*/, const std::string& name, uint32_t crc32) {
            m_animNameDB.AddEntry(m_data.folderPath, name, crc32);
        });
        m_animMgr->SetOnAnimRemoved([this](uint32_t removedHash) {
            for (auto& mv : m_data.moves)
                if (mv.anim_key == removedHash) mv.anim_key = 0;
            m_dirty = true;
        });
        if (dev && ctx)
            m_animMgr->SetD3DContext(dev, ctx);
    }
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
    RenderSubWin_ReferenceFinder();
    RenderCommandCreator();

    // Animation Manager
    if (m_animMgr && m_animMgrVisible)
    {
        if (!m_animMgr->Render())
            m_animMgrVisible = false;  // closed via X; keep instance alive for data lookups
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
    // Nothing pending or showing → nothing to do.
    if (!m_removeConfirm.pending && !m_removeConfirm.showing &&
        !m_endInsertBlocked       && !m_showInsertBlocked)
        return;

    // Prefer the viewport of the sub-window that triggered the dialog;
    // fall back to the main editor viewport, then the host viewport.
    ImGuiViewport* edVp = nullptr;
    if ((m_removeConfirm.showing || m_removeConfirm.pending) && m_removeConfirm.callerViewportId != 0)
        edVp = ImGui::FindViewportByID(m_removeConfirm.callerViewportId);
    else if ((m_showInsertBlocked || m_endInsertBlocked) && m_insertBlockedViewportId != 0)
        edVp = ImGui::FindViewportByID(m_insertBlockedViewportId);
    if (!edVp && m_viewportId != 0)
        edVp = ImGui::FindViewportByID(m_viewportId);
    if (!edVp) edVp = ImGui::GetMainViewport();
    const ImVec2 vpPos  = edVp->Pos;
    const ImVec2 vpSize = edVp->Size;
    const ImVec2 center(vpPos.x + vpSize.x * 0.5f, vpPos.y + vpSize.y * 0.5f);

    // A 1×1 off-screen window on the correct viewport acts as the popup owner.
    // BeginPopupModal must be called from inside a Begin/End scope.
    constexpr ImGuiWindowFlags kHostFlags =
        ImGuiWindowFlags_NoTitleBar     | ImGuiWindowFlags_NoResize    |
        ImGuiWindowFlags_NoMove         | ImGuiWindowFlags_NoDocking   |
        ImGuiWindowFlags_NoSavedSettings| ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoNav          | ImGuiWindowFlags_NoFocusOnAppearing |
        ImGuiWindowFlags_NoBackground   | ImGuiWindowFlags_NoMouseInputs;
    ImGui::SetNextWindowViewport(edVp->ID);
    ImGui::SetNextWindowPos(ImVec2(vpPos.x - 100.f, vpPos.y - 100.f));
    ImGui::SetNextWindowSize(ImVec2(1.f, 1.f));
    ImGui::Begin(WinId("##dlg_host").c_str(), nullptr, kHostFlags);

    // Convert one-frame triggers → open the matching popup (same frame = immediate open).
    if (m_removeConfirm.pending) {
        ImGui::OpenPopup(WinId("##rm_confirm").c_str());
        m_removeConfirm.pending = false;
        m_removeConfirm.showing = true;
    }
    if (m_endInsertBlocked) {
        ImGui::OpenPopup(WinId("##ins_blocked").c_str());
        m_endInsertBlocked      = false;
        m_showInsertBlocked     = true;
    }

    const float pad   = ImGui::GetStyle().WindowPadding.x;
    const float lineH = ImGui::GetTextLineHeightWithSpacing();

    constexpr ImGuiWindowFlags kDlgFlags =
        ImGuiWindowFlags_NoTitleBar     | ImGuiWindowFlags_NoResize    |
        ImGuiWindowFlags_NoMove         | ImGuiWindowFlags_NoDocking   |
        ImGuiWindowFlags_NoSavedSettings| ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoNav;

    // ── Remove-confirmation modal ─────────────────────────────────────────
    {
        const float msgW = ImGui::CalcTextSize(m_removeConfirm.message, nullptr, false, 280.f).x;
        const float boxW = (msgW > 220.f ? msgW : 220.f) + pad * 2.f + 8.f;
        const float msgH = ImGui::CalcTextSize(m_removeConfirm.message, nullptr, false, 280.f).y;
        const float boxH = msgH + lineH * 2.f + pad * 2.f + 8.f;
        ImGui::SetNextWindowViewport(edVp->ID);
        ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(boxW, boxH), ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.96f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.f);
        ImGui::PushStyleColor(ImGuiCol_ModalWindowDimBg, ImVec4(0.f, 0.f, 0.f, 0.28f));
        if (ImGui::BeginPopupModal(WinId("##rm_confirm").c_str(), nullptr, kDlgFlags))
        {
            ImGui::PopStyleColor();
            ImGui::PopStyleVar();
            ImGui::SetCursorPosY(ImGui::GetStyle().WindowPadding.y + 4.f);
            ImGui::TextWrapped("%s", m_removeConfirm.message);
            ImGui::Spacing();
            if (ImGui::Button("Remove", ImVec2(100, 0)))
            {
                if (m_removeConfirm.onConfirm) m_removeConfirm.onConfirm();
                m_removeConfirm = {};
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(100, 0)))
            {
                m_removeConfirm = {};
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
        else
        {
            ImGui::PopStyleColor();
            ImGui::PopStyleVar();
            // Popup closed externally (e.g. ESC key) → clear state.
            if (m_removeConfirm.showing)
                m_removeConfirm = {};
        }
    }

    // ── "Cannot insert at [END]" modal ───────────────────────────────────
    {
        const char* msg  = "Cannot insert at an [END] item.";
        const float boxW = ImGui::CalcTextSize(msg).x + pad * 2.f + 16.f;
        const float boxH = lineH * 2.f + pad * 2.f + 8.f;
        ImGui::SetNextWindowViewport(edVp->ID);
        ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(boxW, boxH), ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.96f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.f);
        ImGui::PushStyleColor(ImGuiCol_ModalWindowDimBg, ImVec4(0.f, 0.f, 0.f, 0.28f));
        if (ImGui::BeginPopupModal(WinId("##ins_blocked").c_str(), nullptr, kDlgFlags))
        {
            ImGui::PopStyleColor();
            ImGui::PopStyleVar();
            ImGui::SetCursorPosY(ImGui::GetStyle().WindowPadding.y + 4.f);
            ImGui::TextUnformatted(msg);
            ImGui::Spacing();
            if (ImGui::Button("Close", ImVec2(80, 0)))
            {
                m_showInsertBlocked       = false;
                m_insertBlockedViewportId = 0;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
        else
        {
            ImGui::PopStyleColor();
            ImGui::PopStyleVar();
            if (m_showInsertBlocked) { m_showInsertBlocked = false; m_insertBlockedViewportId = 0; }
        }
    }

    ImGui::End(); // ##dlg_host
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
//  ImGui char-filter callback: blocks characters invalid in move names.
//  Blocks: space, !, ?, and Windows-filename-invalid chars: \ / : * " < > |
// -------------------------------------------------------------
static int MoveNameCharFilter(ImGuiInputTextCallbackData* data)
{
    ImWchar c = data->EventChar;
    // Block space and special chars
    static const ImWchar kBlocked[] = {
        ' ', '!', '?', '\\', '/', ':', '*', '"', '<', '>', '|', 0
    };
    for (int i = 0; kBlocked[i]; ++i)
        if (c == kBlocked[i]) return 1; // reject
    return 0;
}

// Generate a unique new move name that doesn't collide with existing displayNames.
// Pattern: baseName + "_NNNN" where NNNN increments until unique.
static std::string MakeUniqueName(const std::vector<ParsedMove>& moves, const std::string& baseName)
{
    // Check if baseName itself is free
    bool taken = false;
    for (const auto& mv : moves)
        if (mv.displayName == baseName) { taken = true; break; }
    if (!taken) return baseName;

    for (int n = 1; n < 9999; ++n)
    {
        char candidate[256];
        snprintf(candidate, sizeof(candidate), "%s_%04d", baseName.c_str(), n);
        taken = false;
        for (const auto& mv : moves)
            if (mv.displayName == candidate) { taken = true; break; }
        if (!taken) return candidate;
    }
    return baseName; // fallback (shouldn't reach here)
}

// Compute name_key and ordinal_id for a new move and write them into m.
// ordinal_id = KamuiHash(UPPER(charaCode) + "_" + name)
// name_key   = KamuiHash(name)
static void ApplyKamuiHashes(ParsedMove& m, const std::string& moveName, const std::string& charaCode)
{
    m.name_key = (uint32_t)KamuiHash::Compute(moveName);

    // Build "CHARCODE_MoveName" string (charaCode uppercased)
    std::string prefix = charaCode;
    for (char& c : prefix) c = (char)toupper((unsigned char)c);
    std::string ordStr = prefix + "_" + moveName;
    m.ordinal_id2 = (uint32_t)KamuiHash::Compute(ordStr);
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
            empty.isNew = true;
            { char base[32]; snprintf(base, sizeof(base), "NewMove_%04d", (int)m_data.moves.size());
              empty.displayName = MakeUniqueName(m_data.moves, base); }
            ApplyKamuiHashes(empty, empty.displayName, m_data.charaCode);
            m_data.moves.push_back(empty);
            m_dirty = true;
        }
        if (ImGui::MenuItem("Duplicate Current Move")) {
            if (m_selectedIdx >= 0 && m_selectedIdx < (int)m_data.moves.size()) {
                ParsedMove dup = m_data.moves[m_selectedIdx];
                dup.isNew = true;
                std::string baseName = dup.displayName + "_copy";
                dup.displayName = MakeUniqueName(m_data.moves, baseName);
                ApplyKamuiHashes(dup, dup.displayName, m_data.charaCode);
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

            // Check for duplicate name among other moves (warn-only)
            bool isDupeName = false;
            for (int di = 0; di < (int)m_data.moves.size(); ++di) {
                if (di != m_selectedIdx && m_data.moves[di].displayName == m.displayName) {
                    isDupeName = true; break;
                }
            }
            if (isDupeName)
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));

            // For new moves: filter out invalid chars; for original moves: allow free editing
            ImGuiInputTextFlags nameFlags = ImGuiInputTextFlags_None;
            if (m.isNew)
                nameFlags |= ImGuiInputTextFlags_CallbackCharFilter;

            ImGui::InputText("##move_name", nameBuf, sizeof(nameBuf), nameFlags,
                             m.isNew ? MoveNameCharFilter : nullptr);

            if (isDupeName) {
                ImGui::PopStyleColor();
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
                    ImGui::SetTooltip("Duplicate name — another move already uses this name.");
            }

            if (ImGui::IsItemDeactivatedAfterEdit())
            {
                m.displayName = nameBuf;
                // For new moves: recompute name_key and ordinal_id from the new name
                if (m.isNew && nameBuf[0] != '\0')
                    ApplyKamuiHashes(m, m.displayName, m_data.charaCode);
                if (nameBuf[0] != '\0') m_customNames[m_selectedIdx] = m.displayName;
                else                    m_customNames.erase(m_selectedIdx);
                SaveEditorDatas();
                dirty = true;
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
            const char* kamuiAnimName = LabelDB::Get().GetMoveName(m.anim_key);
            if (m_animKeyBufIdx != m_selectedIdx)
            {
                m_animKeyBufIdx = m_selectedIdx;
                if (kamuiAnimName)
                    snprintf(m_animKeyBuf, sizeof(m_animKeyBuf), "%s", kamuiAnimName);
                else if (db)
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
            const bool hasKamui  = (kamuiAnimName != nullptr);
            // kamui 이름이 있는 경우 버퍼가 정확히 그 이름과 일치해야만 유효
            const bool bufMatchesKamui = hasKamui && (strcmp(m_animKeyBuf, kamuiAnimName) == 0);
            const bool effectiveValid  = hasKamui ? bufMatchesKamui : animValid;
            const bool isComRef  = !hasKamui && db && !animValid
                                 && m_animKeyBuf[0] == '0'
                                 && (m_animKeyBuf[1] == 'x' || m_animKeyBuf[1] == 'X');
            static constexpr ImVec4 kYellow = {1.00f, 0.85f, 0.30f, 1.0f};
            const ImVec4& lblCol = effectiveValid ? kGreen : (!db ? kGray : (isComRef ? kYellow : kPink));

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
                const uint32_t prevAnimKey = m.anim_key;
                if (ImGui::InputText("##animkeyinput", m_animKeyBuf, sizeof(m_animKeyBuf)))
                {
                    uint32_t newKey = 0;
                    if (m_animNameDB.NameToAnimKey(m_animKeyBuf, newKey)) {
                        // Reject if the resolved key belongs to a kamui-recovered animation
                        // but the buffer doesn't match its kamui name — must use kamui name
                        const char* resolvedKamui = LabelDB::Get().GetMoveName(newKey);
                        if (!resolvedKamui || strcmp(m_animKeyBuf, resolvedKamui) == 0) {
                            m.anim_key = newKey; dirty = true;
                        }
                    } else {
                        // Try KamuiHash reverse lookup (kamui dict name typed directly)
                        uint32_t kh = (uint32_t)KamuiHash::Compute(m_animKeyBuf);
                        if (LabelDB::Get().GetMoveName(kh) != nullptr) {
                            m.anim_key = kh; dirty = true;
                        } else if (m_animKeyBuf[0]=='0' && (m_animKeyBuf[1]=='x'||m_animKeyBuf[1]=='X')) {
                            char* end;
                            uint32_t v = (uint32_t)strtoul(m_animKeyBuf + 2, &end, 16);
                            if (end != m_animKeyBuf + 2) { m.anim_key = v; dirty = true; }
                        }
                    }
                }
                if (m.anim_key != prevAnimKey && m_animMgr) {
                    int32_t tf = m_animMgr->GetTotalFramesForKey(m.anim_key);
                    if (tf >= 0) m.anim_len = tf + 1;
                }
            }
            else
            {
                char roBuf[32]; snprintf(roBuf, sizeof(roBuf), "0x%08X", m.anim_key);
                ImGui::InputText("##animkey_ro", roBuf, sizeof(roBuf), ImGuiInputTextFlags_ReadOnly);
            }
            ImGui::SameLine();
            if (GoButton("Go \xe2\x86\x92##animkey_go", effectiveValid))
            {
                m_animMgrVisible = true;
                if (m_animMgr) {
                    m_animMgr->Show();
                    if (m_selectedIdx >= 0 && m_selectedIdx < (int)m_data.moves.size())
                        m_animMgr->NavigateByMotbinKey(0, m_data.moves[m_selectedIdx].anim_key);
                }
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

        // 9. anim_len (auto-updated from PANM header when anim_key changes; game recalculates at runtime)
        FieldRow(AnimLen, FieldTT::Move::AnimLen);
        { char alBuf[16]; snprintf(alBuf, sizeof(alBuf), "%d", m.anim_len);
          ImGui::SetNextItemWidth(-1.0f);
          ImGui::InputText("##anim_len", alBuf, sizeof(alBuf), ImGuiInputTextFlags_ReadOnly); }

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
          ImGui::InputText("##t_char_id", buf, sizeof(buf), ImGuiInputTextFlags_ReadOnly); }

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
    // active_frame: move-level startup/recovery (0x158/0x15C).
    // OldTool2 calls these first_active_frame / last_active_frame.
    // The game reads 0x158 for hitbox timing; always equals hitbox1 active_start
    // in original movesets.
    if (BeginPropTable("##hb_active_frame", true))
    {
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextDisabled("%s", MoveLabel::ActiveFrame);
        ImGui::TableSetColumnIndex(1);
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.5f - 4.0f);
        { int tmp = (int)m.startup;
          if (ImGui::InputInt("##af_start", &tmp, 0, 0)) { m.startup = (uint32_t)tmp; dirty = true; } }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(-1.0f);
        { int tmp = (int)m.recovery;
          if (ImGui::InputInt("##af_last",  &tmp, 0, 0)) { m.recovery = (uint32_t)tmp; dirty = true; } }
        ImGui::EndTable();
    }
    ImGui::Spacing();

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
        uint32_t items = count;
        char lbl[48];
        snprintf(lbl, sizeof(lbl), "#%u  %u items##g%d", groups[gi].first, items, gi);
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
        if (ImGui::MenuItem("Animation Manager", nullptr, m_animMgrVisible)) {
            m_animMgrVisible = !m_animMgrVisible;
            if (m_animMgrVisible && m_animMgr) m_animMgr->Show();
        }
        if (ImGui::MenuItem("Reference Finder", nullptr, m_refFinder.open))
            m_refFinder.open = !m_refFinder.open;
        ImGui::Separator();
        if (ImGui::MenuItem("Command Creator")) {
            m_cmdCreator.pendingOpen      = true;
            m_cmdCreator.callerViewportId = ImGui::GetWindowViewport()->ID;
            m_cmdCreator.value            = 0;
            m_cmdCreator.target           = nullptr;
            m_cmdCreator.dirtyFlag        = nullptr;
            m_cmdCreator.activeTab        = 0;
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
    if (m_saveState == SaveState::Saving) return;
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
    const ImVec2 vpMax(vpPos.x + vpSize.x, vpPos.y + vpSize.y);
    const ImVec2 center(vpPos.x + vpSize.x * 0.5f, vpPos.y + vpSize.y * 0.5f);

    ImDrawList* dl = ImGui::GetForegroundDrawList(edVp);

    // Dim overlay drawn directly — never suppressed by ImGui window management.
    dl->AddRectFilled(vpPos, vpMax, IM_COL32(0, 0, 0, 72));

    const float pad   = ImGui::GetStyle().WindowPadding.x;
    const float lineH = ImGui::GetTextLineHeightWithSpacing();
    const float fontSize = ImGui::GetFontSize();

    if (m_saveState == SaveState::Saving)
    {
        bool done = m_saveFuture.valid() &&
                    m_saveFuture.wait_for(std::chrono::seconds(0)) == std::future_status::ready;

        static const char* kSpin = "|/-\\";
        const int spinIdx = done ? 0 : (static_cast<int>(ImGui::GetTime() * 8.0) & 3);
        char spinMsg[64];
        snprintf(spinMsg, sizeof(spinMsg), "%c  Saving...", kSpin[spinIdx]);

        // Draw Saving box via ImGui window (works fine on all frames).
        constexpr ImGuiWindowFlags kOvFlags =
            ImGuiWindowFlags_NoTitleBar     | ImGuiWindowFlags_NoResize    |
            ImGuiWindowFlags_NoMove         | ImGuiWindowFlags_NoDocking   |
            ImGuiWindowFlags_NoSavedSettings| ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoNav          | ImGuiWindowFlags_NoFocusOnAppearing |
            ImGuiWindowFlags_NoCollapse;
        const float boxW = ImGui::CalcTextSize(spinMsg).x + pad * 2.f + 16.f;
        ImGui::SetNextWindowViewport(edVp->ID);
        ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(boxW, lineH + pad * 2.f + 8.f), ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.96f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.f);
        ImGui::Begin(WinId("##sv_spin").c_str(), nullptr, kOvFlags);
        ImGui::PopStyleVar();
        ImGui::SetCursorPosY(ImGui::GetStyle().WindowPadding.y + 4.f);
        ImGui::Text("%s", spinMsg);
        ImGui::End();

        if (done)
        {
            try { m_saveFuture.get(); } catch (...) {}
            m_dirty        = false;
            m_saveState    = SaveState::Done;
            m_doneShowTime = ImGui::GetTime();
        }
    }
    else // SaveState::Done — draw entirely via foreground draw list (bypasses ImGui window state)
    {
        const char* msg  = "Saved";
        const char* hint = "(click to close)";
        const ImVec2 szMsg  = ImGui::CalcTextSize(msg);
        const ImVec2 szHint = ImGui::CalcTextSize(hint);
        const float innerW = (szMsg.x > szHint.x ? szMsg.x : szHint.x);
        const float boxW   = innerW + pad * 2.f + 16.f;
        const float boxH   = lineH * 2.f + pad * 2.f + 8.f;
        const ImVec2 bMin(center.x - boxW * 0.5f, center.y - boxH * 0.5f);
        const ImVec2 bMax(bMin.x + boxW, bMin.y + boxH);

        dl->AddRectFilled(bMin, bMax, IM_COL32(30, 30, 30, 245), 6.f);
        dl->AddRect      (bMin, bMax, IM_COL32(120, 120, 120, 200), 6.f);

        const float textX = bMin.x + (boxW - szMsg.x)  * 0.5f;
        const float hintX = bMin.x + (boxW - szHint.x) * 0.5f;
        const float textY = bMin.y + pad + 4.f;
        const float hintY = textY + lineH;

        dl->AddText(ImGui::GetFont(), fontSize,
                    ImVec2(textX, textY), IM_COL32(90, 255, 128, 255), msg);
        dl->AddText(ImGui::GetFont(), fontSize * 0.9f,
                    ImVec2(hintX, hintY), IM_COL32(160, 160, 160, 200), hint);

        // Skip first ~0.3 s to avoid closing on the same click that opened Done.
        if (ImGui::GetTime() - m_doneShowTime > 0.3 &&
            ImGui::IsMouseClicked(ImGuiMouseButton_Left))
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
            uint32_t items = groups[gi].second;
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
                char lbl[192];
                char reqBuf[32];
                if (r.req > 0x8000) {
                    snprintf(reqBuf, sizeof(reqBuf), "0x%.4X", r.req);
                } else {
                    snprintf(reqBuf, sizeof(reqBuf), "%u", r.req);
                }
                if (isTerm) {
                    snprintf(lbl, sizeof(lbl), "#%u  [END]##ri%u", k, idx);
                } else {
                    const MovesetDataDict::ReqEntry* de = MovesetDataDict::Get().GetReq(r.req);
                    if (de && !de->condition.empty()) {
                        snprintf(lbl, sizeof(lbl), "#%u  %s: %s##ri%u", k, reqBuf, de->condition.c_str(), idx);
                    } else {
                        snprintf(lbl, sizeof(lbl), "#%u  %s##ri%u", k, reqBuf, idx);
                    }
                }
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

            // --- Block 1: req + dictionary info ---
            static constexpr ImVec4 kGreen = { 0.30f, 0.88f, 0.42f, 1.00f };
            static constexpr ImVec4 kRed   = { 0.90f, 0.30f, 0.30f, 1.00f };
            float reqBlockH = ImGui::GetTextLineHeight()
                + sty.ItemSpacing.y + ImGui::GetFrameHeight()
                + sty.ItemSpacing.y + ImGui::GetTextLineHeightWithSpacing() * 2.0f
                + sty.WindowPadding.y * 2.0f;
            ImGui::PushStyleColor(ImGuiCol_ChildBg, kBlockBg);
            if (ImGui::BeginChild("##req_b1", ImVec2(-1.0f, reqBlockH), ImGuiChildFlags_Borders)) {
                bool isProp = r.req > 0x8000;
                const char* displayLabel = isProp ? ExtraPropLabel::Property : ReqLabel::Req;
                ImGui::TextDisabled("%s", displayLabel); ShowFieldTooltip(FieldTT::Req::Req);
                ImGui::SetNextItemWidth(-1.0f);
                int tmp = static_cast<int>(r.req);
                if (ImGui::InputInt("##req_val", &tmp, 0, 0))
                    { r.req = static_cast<uint32_t>(tmp); m_dirty = true; }

                const MovesetDataDict::ReqEntry* de = MovesetDataDict::Get().GetReq(r.req);
                if (de) {
                    ImGui::PushStyleColor(ImGuiCol_Text, kGreen);
                    const char* fmt = isProp ? "0x%X: %s" : "%u: %s";
                    ImGui::Text(fmt, r.req, de->condition.c_str());
                    ImGui::PopStyleColor();
                    if (!de->tooltip.empty() && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
                        ImGui::SetTooltip("%s", de->tooltip.c_str());
                    if (!de->param.empty())
                        ImGui::TextDisabled("param: %s", de->param.c_str());
                    else
                        ImGui::TextDisabled(" ");
                } else {
                    ImGui::PushStyleColor(ImGuiCol_Text, kRed);
                    const char* fmt = isProp ? "0x%X: Unknown" : "%u: Unknown";
                    ImGui::Text(fmt, r.req);
                    ImGui::PopStyleColor();
                    ImGui::TextDisabled(" ");
                }
            }
            ImGui::EndChild();
            ImGui::PopStyleColor();

            // --- Block 2: params ---
            float fieldRowH = ImGui::GetFrameHeight() + sty.ItemSpacing.y;
            float paramLabelRowH = ImGui::GetTextLineHeightWithSpacing();
            struct { int index; const char* label; uint32_t* val; const char* id; const FieldTooltip* tt; } rows[] = {
                { 0, ReqLabel::Param0, &r.param,  "##p0", &FieldTT::Req::Param0 },
                { 1, ReqLabel::Param1, &r.param2, "##p1", &FieldTT::Req::Param1 },
                { 2, ReqLabel::Param2, &r.param3, "##p2", &FieldTT::Req::Param2 },
                { 3, ReqLabel::Param3, &r.param4, "##p3", &FieldTT::Req::Param3 },
            };
            float paramBlockContentH = 0.0f;
            for (auto& row : rows) {
                paramBlockContentH += fieldRowH;
                const char* pl = MovesetDataDict::Get().GetParamLabel(r.req, row.index, *row.val);
                if (pl && pl[0])
                    paramBlockContentH += paramLabelRowH;
            }
            float paramBlockH = paramBlockContentH - sty.ItemSpacing.y + sty.WindowPadding.y * 2.0f;
            ImGui::PushStyleColor(ImGuiCol_ChildBg, kBlockBg);
            if (ImGui::BeginChild("##req_b2", ImVec2(-1.0f, paramBlockH), ImGuiChildFlags_Borders)) {
                constexpr ImGuiTableFlags kTF = ImGuiTableFlags_SizingFixedFit;
                if (ImGui::BeginTable("##req_params", 2, kTF, ImVec2(-1.0f, 0.0f))) {
                    ImGui::TableSetupColumn("##lbl", ImGuiTableColumnFlags_WidthFixed);
                    ImGui::TableSetupColumn("##val", ImGuiTableColumnFlags_WidthStretch);

                    for (auto& row : rows) {
                        const char* paramLabel = MovesetDataDict::Get().GetParamLabel(r.req, row.index, *row.val);
                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0); ImGui::TextDisabled("%s", row.label); ShowFieldTooltip(*row.tt);
                        ImGui::TableSetColumnIndex(1);
                        ImGui::SetNextItemWidth(-1.0f);
                        int tmp = static_cast<int>(*row.val);
                        if (ImGui::InputInt(row.id, &tmp, 0, 0))
                            { *row.val = static_cast<uint32_t>(tmp); m_dirty = true; }

                        if (paramLabel && paramLabel[0]) {
                            ImGui::TableNextRow();
                            ImGui::TableSetColumnIndex(1);
                            ImGui::PushStyleColor(ImGuiCol_Text, kGreen);
                            ImGui::TextUnformatted(paramLabel);
                            ImGui::PopStyleColor();
                        }
                    }
                    ImGui::EndTable();
                }
            }
            ImGui::EndChild();
            ImGui::PopStyleColor();

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
        // TK8: extradata pointer is always valid (game crashes on null); minimum valid index = 0
        bool extValid = (c.extradata_idx != 0xFFFFFFFF) &&
                        (c.extradata_idx < (uint32_t)m_data.cancelExtraBlock.size());
        ImGui::PushStyleColor(ImGuiCol_Text, extValid ? kGreen : kPink);
        ImGui::TextUnformatted(CancelLabel::Extradata); ShowFieldTooltip(FieldTT::Cancel::Extradata);
        ImGui::PopStyleColor();
        ImGui::SetNextItemWidth(-(kBtnW + sty.ItemSpacing.x));
        int extTmp = (c.extradata_idx == 0xFFFFFFFF) ? 0 : (int)c.extradata_idx;
        if (ImGui::InputInt("##ext_idx", &extTmp, 0, 0)) {
            c.extradata_idx = (extTmp < 0) ? 0 : (uint32_t)extTmp;
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
    c.move_id       = 0x8001;
    c.req_list_idx  = 0;
    c.extradata_idx = 0;  // TK8: always valid; index 0 = cancel_extra[0]
    c.group_cancel_list_idx = 0xFFFFFFFF;
    return c;
}

static ParsedCancel MakeGroupCancelTerminator()
{
    ParsedCancel c{};
    c.command       = (uint64_t)GameStatic::Get().data.groupCancelEnd;
    c.move_id       = 0x8000;
    c.req_list_idx  = 0xFFFFFFFF;
    c.extradata_idx = 0;  // TK8: always valid; index 0 = cancel_extra[0]
    c.group_cancel_list_idx = 0xFFFFFFFF;
    return c;
}

static ParsedCancel MakeEmptyCancel()
{
    ParsedCancel c{};
    c.command       = 0;
    c.move_id       = 0;
    c.req_list_idx  = 0xFFFFFFFF;
    c.extradata_idx = 0;  // TK8: always valid; index 0 = cancel_extra[0]
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
            uint32_t items = count;
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
                char lbl[128];
                if (isTerm)
                    snprintf(lbl, sizeof(lbl), "#%u  [END]##ci%u", k, idx);
                else if (c.command == GameStatic::Get().data.groupCancelStart)
                    snprintf(lbl, sizeof(lbl), "#%u  [GRP_START]##ci%u", k, idx);
                else if (c.command == GameStatic::Get().data.groupCancelEnd)
                    snprintf(lbl, sizeof(lbl), "#%u  [GRP_END]##ci%u", k, idx);
                else {
                    std::string moveName = "move_" + std::to_string(c.move_id);
                    if (c.move_id < 0x8000) {
                        if ((size_t)c.move_id < data.moves.size())
                            moveName = data.moves[c.move_id].displayName;
                    } else {
                        uint32_t aliasIdx = c.move_id - 0x8000u;
                        if (aliasIdx < data.originalAliases.size()) {
                            uint16_t res = data.originalAliases[aliasIdx];
                            if ((size_t)res < data.moves.size())
                                moveName = data.moves[res].displayName;
                        }
                    }
                    snprintf(lbl, sizeof(lbl), "#%u  ->%s (%u)##ci%u", k, moveName.c_str(), c.move_id, idx);
                }
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
    ImGui::SetNextWindowSize(ImVec2(960.0f, 600.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(WinId("Cancels##blkwin").c_str(), &m_cancelsWin.open, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking))
    { ImGui::End(); return; }
    if (cancelsBringFront) ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindow());

    const uint64_t gcEnd = GameStatic::Get().data.groupCancelEnd;
    auto isCancelTerm      = +[](const ParsedCancel& c)->bool { return c.command == 0x8000; };
    auto isGroupCancelTerm = [gcEnd](const ParsedCancel& c)->bool { return c.command == gcEnd; };
    auto cancelGroups      = ComputeGroups(m_data.cancelBlock,      isCancelTerm);
    auto groupCancelGroups = ComputeGroups(m_data.groupCancelBlock, isGroupCancelTerm);

    static constexpr float   kCexW   = 260.0f;
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
            const int pt = m_cancelsWin.pendingTab;
            m_cancelsWin.pendingTab = -1;
            ImGuiTabItemFlags f0 = (pt == 0) ? ImGuiTabItemFlags_SetSelected : 0;
            ImGuiTabItemFlags f1 = (pt == 1) ? ImGuiTabItemFlags_SetSelected : 0;

            if (ImGui::BeginTabItem("Cancel List", nullptr, f0))
            {
                RenderCancelSection("##co","##ci","##cd",
                    m_data.cancelBlock, cancelGroups, m_cancelsWin.cancelSel,
                    "Cancel Lists", 0.0f, 0x8000,
                    this, groupCancelGroups,
                    m_data, m_dirty, false);
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Group Cancel List", nullptr, f1))
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

                // Cancel-extra description from MovesetDataDict
                {
                    static constexpr ImVec4 kGreen = { 0.30f, 0.88f, 0.42f, 1.00f };
                    static constexpr ImVec4 kRed   = { 0.90f, 0.30f, 0.30f, 1.00f };
                    const char* desc = MovesetDataDict::Get().GetCancelExtra(cexBlk[i]);
                    if (desc && desc[0] != '\0') {
                        ImGui::PushStyleColor(ImGuiCol_Text, kGreen);
                        ImGui::TextUnformatted(desc);
                        ImGui::PopStyleColor();
                    } else {
                        ImGui::PushStyleColor(ImGuiCol_Text, kRed);
                        ImGui::TextUnformatted("Unknown");
                        ImGui::PopStyleColor();
                    }
                }
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
            uint32_t items = groups[gi].second;
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
                if (RowU16Edit("##pb_val1", PushbackLabel::LinearDuration,     p.val1, FieldTT::Pushback::LinearDuration))     m_dirty = true;
                if (RowU16Edit("##pb_val2", PushbackLabel::LinearDisplacement, p.val2, FieldTT::Pushback::LinearDisplacement)) m_dirty = true;
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
    // 0x860A-0x8613: hand animation/pose pool index
    AnimationManagerWindow*                        animMgr       = nullptr;
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

    static constexpr float kListW = 200.0f;

    // ---- Outer list ----
    ImGui::BeginChild(outerChildId, ImVec2(kListW, 0.0f), true);
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
            uint32_t items = groups[gi].second;
            char lbl[48]; snprintf(lbl, sizeof(lbl), "#%u  %u items##pg%d", groups[gi].first, items, gi);
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
    ImGui::BeginChild(innerChildId, ImVec2(kListW, 0.0f), true);
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
                ParsedExtraProp ne{};
                ne.req_list_idx = 0;        // requirements: index 0
                if (isExtraProp) ne.type = 32769; // frame: 32769; type!=0 prevents END trigger
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
                else {
                    const MovesetDataDict::PropEntry* de = MovesetDataDict::Get().GetPropEntry(e.id);
                    if (de && !de->function.empty()) {
                        snprintf(lbl, sizeof(lbl), "#%u  0x%.4X: %s##pi%u", k, e.id, de->function.c_str(), idx);
                    } else {
                        snprintf(lbl, sizeof(lbl), "#%u  0x%.4X ##pi%u", k, e.id, idx);
                    }
                }
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

            static constexpr ImVec4 kBlockBg = {0.14f, 0.14f, 0.18f, 1.00f};
            static constexpr ImVec4 kGreen   = {0.30f, 0.88f, 0.42f, 1.00f};
            static constexpr ImVec4 kPink    = {1.00f, 0.50f, 0.65f, 1.0f};
            static constexpr ImVec4 kRed     = {0.90f, 0.30f, 0.30f, 1.0f};

            const ImGuiStyle& sty = ImGui::GetStyle();

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

            // --- Block 1: frame / _0x4 / property / requirements ---
            // Each field: stacked label (TextLineH) + input (FrameH) + spacing*2
            const float fH1    = ImGui::GetTextLineHeight() + ImGui::GetFrameHeight() + sty.ItemSpacing.y * 2.0f;
            const int   b1Rows = isExtraProp ? 4 : 2;
            const float b1H    = fH1 * b1Rows - sty.ItemSpacing.y
                               + ImGui::GetTextLineHeightWithSpacing() * 2.0f
                               + sty.WindowPadding.y * 2.0f;

            std::string b1Id = std::string(detailChildId) + "_b1";
            ImGui::PushStyleColor(ImGuiCol_ChildBg, kBlockBg);
            if (ImGui::BeginChild(b1Id.c_str(), ImVec2(-1.0f, b1H), ImGuiChildFlags_Borders))
            {
                if (isExtraProp) {
                    // frame
                    ImGui::TextDisabled("%s", ExtraPropLabel::Frame); ShowFieldTooltip(FieldTT::ExtraProp::Frame);
                    ImGui::SetNextItemWidth(-1.0f);
                    int ftmp = (int)e.type;
                    if (ImGui::InputInt("##ep_type", &ftmp, 0, 0)) { e.type = (uint32_t)ftmp; dirty = true; }


                }

                // property
                ImGui::TextDisabled("%s", ExtraPropLabel::Property); ShowFieldTooltip(FieldTT::ExtraProp::Property);
                ImGui::SetNextItemWidth(-1.0f);
                char bufId[14]; snprintf(bufId, sizeof(bufId), "0x%X", e.id);
                ImGui::InputText("##ep_id", bufId, sizeof(bufId));
                if (ImGui::IsItemDeactivatedAfterEdit()) {
                    const char* p = bufId;
                    if (p[0]=='0' && (p[1]=='x'||p[1]=='X')) p += 2;
                    e.id = (uint32_t)strtoul(p, nullptr, 16); dirty = true;
                }

                // requirements (navigable)
                bool reqValid = (e.req_list_idx != 0xFFFFFFFF) && (e.req_list_idx < (uint32_t)reqBlk.size());
                ImGui::PushStyleColor(ImGuiCol_Text, reqValid ? kGreen : kPink);
                ImGui::TextUnformatted(ExtraPropLabel::Requirements);
                ImGui::PopStyleColor();
                ImGui::SetNextItemWidth(-(kBtnW + sty.ItemSpacing.x));
                int reqTmp = (e.req_list_idx == 0xFFFFFFFF) ? -1 : (int)e.req_list_idx;
                if (ImGui::InputInt("##prop_req_idx", &reqTmp, 0, 0)) {
                    e.req_list_idx = (reqTmp < 0) ? 0xFFFFFFFF : (uint32_t)reqTmp;
                    dirty = true;
                }
                ImGui::SameLine();
                if (GoButton("##req_go", reqValid)) {
                    auto grps = ComputeGroups(reqBlk, +[](const ParsedRequirement& rr)->bool{ return rr.req==GameStatic::Get().data.reqListEnd; });
                    int gi = FindGroupOuter(grps, e.req_list_idx);
                    if (gi >= 0) { reqWinSel.outer = gi; reqWinSel.inner = 0; reqWinSel.scrollOuter = true; }
                    reqWinOpen = true;
                }

                // property dict display
                const MovesetDataDict::PropEntry* pe = MovesetDataDict::Get().GetPropEntry(e.id);
                if (pe) {
                    ImGui::PushStyleColor(ImGuiCol_Text, kGreen);
                    ImGui::TextUnformatted(pe->function.c_str());
                    ImGui::PopStyleColor();
                    if (!pe->tooltip.empty() && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
                        ImGui::SetTooltip("%s", pe->tooltip.c_str());
                    if (!pe->param.empty())
                        ImGui::TextDisabled("param: %s", pe->param.c_str());
                    else
                        ImGui::TextDisabled(" ");
                } else {
                    ImGui::PushStyleColor(ImGuiCol_Text, kRed);
                    ImGui::TextUnformatted("Unknown");
                    ImGui::PopStyleColor();
                    ImGui::TextDisabled(" ");
                }
            }
            ImGui::EndChild();
            ImGui::PopStyleColor();

            ImGui::Spacing();

            // --- Block 2: params[0-4] ---
            // RowPU32: optional green param label row only when GetParamLabel is non-empty.
            const float fieldRowH = ImGui::GetFrameHeight() + sty.ItemSpacing.y;
            const float paramLabelRowH = ImGui::GetTextLineHeightWithSpacing();
            float b2ContentH = 0.0f;
            auto isHandAnimProp = [](uint32_t id) {
                return id >= 0x860F && id <= 0x8613;
            };
            auto isHandPoseProp = [](uint32_t id) {
                return id >= 0x860A && id <= 0x860D;
            };
            if (isHandAnimProp(e.id)) {
                b2ContentH += fieldRowH + paramLabelRowH;  // 입력행 + 이름 힌트행
            } else if (e.id == 0x877d || e.id == 0x827b || e.id == 0x868f) {
                b2ContentH += fieldRowH;
            } else if (isHandPoseProp(e.id)) {
                b2ContentH += 2 * fieldRowH + paramLabelRowH;  // pose combo + blend input + decoded label
            } else if (e.id == 0x860E) {
                b2ContentH += fieldRowH + paramLabelRowH;  // blend input + decoded label
            } else {
                b2ContentH += fieldRowH;
                const char* pl0 = MovesetDataDict::Get().GetParamLabel(e.id, 0, e.value);
                if (pl0 && pl0[0])
                    b2ContentH += paramLabelRowH;
            }
            const uint32_t* epVals[4] = { &e.value2, &e.value3, &e.value4, &e.value5 };
            for (int pi = 0; pi < 4; ++pi) {
                b2ContentH += fieldRowH;
                const char* pl = MovesetDataDict::Get().GetParamLabel(e.id, pi + 1, *epVals[pi]);
                if (pl && pl[0])
                    b2ContentH += paramLabelRowH;
            }
            const float b2H = b2ContentH - sty.ItemSpacing.y + sty.WindowPadding.y * 2.0f;

            std::string b2Id = std::string(detailChildId) + "_b2";
            ImGui::PushStyleColor(ImGuiCol_ChildBg, kBlockBg);
            if (ImGui::BeginChild(b2Id.c_str(), ImVec2(-1.0f, b2H), ImGuiChildFlags_Borders))
            {
                constexpr ImGuiTableFlags kTF = ImGuiTableFlags_SizingFixedFit;
                std::string tb2Id = std::string(detailChildId) + "_tb2";
                if (ImGui::BeginTable(tb2Id.c_str(), 2, kTF, ImVec2(-1.0f, 0.0f))) {
                    ImGui::TableSetupColumn("##pl", ImGuiTableColumnFlags_WidthFixed);
                    ImGui::TableSetupColumn("##pv", ImGuiTableColumnFlags_WidthStretch);

                    auto RowPU32 = [&](const char* id, const char* lbl, uint32_t& v, const FieldTooltip& tt = {}, int pIndex = 0) {
                        const char* paramLabel = MovesetDataDict::Get().GetParamLabel(e.id, pIndex, v);
                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0); ImGui::TextDisabled("%s", lbl); ShowFieldTooltip(tt);
                        ImGui::TableSetColumnIndex(1); ImGui::SetNextItemWidth(-1.0f);
                        int tmp = (int)v;
                        if (ImGui::InputInt(id, &tmp, 0, 0)) { v = (uint32_t)tmp; dirty = true; }

                        if (paramLabel && paramLabel[0]) {
                            ImGui::TableNextRow();
                            ImGui::TableSetColumnIndex(1);
                            ImGui::PushStyleColor(ImGuiCol_Text, kGreen);
                            ImGui::TextUnformatted(paramLabel);
                            ImGui::PopStyleColor();
                        }
                    };
                    auto RowPHex32 = [&](const char* id, const char* lbl, uint32_t& v, const FieldTooltip& tt = {}) {
                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0); ImGui::TextDisabled("%s", lbl); ShowFieldTooltip(tt);
                        ImGui::TableSetColumnIndex(1); ImGui::SetNextItemWidth(-1.0f);
                        char buf[14]; snprintf(buf, sizeof(buf), "0x%08X", v);
                        ImGui::InputText(id, buf, sizeof(buf));
                        if (ImGui::IsItemDeactivatedAfterEdit()) {
                            const char* p = buf;
                            if (p[0]=='0' && (p[1]=='x'||p[1]=='X')) p += 2;
                            v = (uint32_t)strtoul(p, nullptr, 16); dirty = true;
                        }
                    };

                    char p0lbl[64];
                    // if (e.id == 0x87f8)
                    // {
                    //     snprintf(p0lbl, sizeof(p0lbl), "%s   (Dialogues)", ExtraPropLabel::Param0);
                    //     RowPHex32("##ep_v1_dlg", p0lbl, e.value, FieldTT::ExtraProp::Param0);
                    // }
                    // else if (e.id == 0x877b)
                    // {
                    //     snprintf(p0lbl, sizeof(p0lbl), "%s   (Dialogues)", ExtraPropLabel::Param0);
                    //     RowPU32("##ep_v1_877b", p0lbl, e.value, FieldTT::ExtraProp::Param0);
                    // }
                    // else 
                    if (e.id == 0x877d)
                    {
                        RowPHex32("##ep_v1_877d", ExtraPropLabel::Param0, e.value, FieldTT::ExtraProp::Param0);
                    }
                    else if (e.id == 0x827b)
                    {
                        snprintf(p0lbl, sizeof(p0lbl), "%s   (Projectiles)", ExtraPropLabel::Param0);
                        bool projValid = navCtx.projBlk && (e.value < (uint32_t)navCtx.projBlk->size());
                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0); ImGui::TextDisabled("%s", p0lbl);
                        ImGui::TableSetColumnIndex(1); ImGui::SetNextItemWidth(-(kBtnW + sty.ItemSpacing.x));
                        int vtmp = (int)e.value;
                        if (ImGui::InputInt("##ep_v1_proj", &vtmp, 0, 0)) { e.value = (uint32_t)vtmp; dirty = true; }
                        ImGui::SameLine();
                        if (GoButton("##proj_go", projValid) && navCtx.projWinOpen) {
                            *navCtx.projWinSel = (int)e.value; *navCtx.projWinScroll = true; *navCtx.projWinOpen = true;
                        }
                    }
                    else if (e.id == 0x868f)
                    {
                        snprintf(p0lbl, sizeof(p0lbl), "%s   (Throws)", ExtraPropLabel::Param0);
                        bool teValid = navCtx.teGroups && !navCtx.teGroups->empty() &&
                                       (e.value < (uint32_t)navCtx.teGroups->back().first + navCtx.teGroups->back().second);
                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0); ImGui::TextDisabled("%s", p0lbl);
                        ImGui::TableSetColumnIndex(1); ImGui::SetNextItemWidth(-(kBtnW + sty.ItemSpacing.x));
                        int vtmp = (int)e.value;
                        if (ImGui::InputInt("##ep_v1_te", &vtmp, 0, 0)) { e.value = (uint32_t)vtmp; dirty = true; }
                        ImGui::SameLine();
                        if (GoButton("##te_go", teValid) && navCtx.throwExtraSel && navCtx.throwsWinOpen) {
                            int gi = FindGroupOuter(*navCtx.teGroups, e.value);
                            if (gi >= 0) { navCtx.throwExtraSel->outer = gi; navCtx.throwExtraSel->inner = 0; navCtx.throwExtraSel->scrollOuter = true; }
                            *navCtx.throwsWinOpen = true;
                        }
                    }
                    else if (isHandAnimProp(e.id))
                    {
                        bool hasMgr = navCtx.animMgr != nullptr;
                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0); ImGui::TextDisabled("Hand Anim");
                        ImGui::TableSetColumnIndex(1); ImGui::SetNextItemWidth(-(kBtnW + sty.ItemSpacing.x));
                        int vtmp = (int)e.value;
                        if (ImGui::InputInt("##ep_v1_hand", &vtmp, 0, 0)) { e.value = (uint32_t)vtmp; dirty = true; }
                        ImGui::SameLine();
                        if (GoButton("##hand_go", hasMgr)) {
                            if (win) win->OpenAnimationManager();
                            navCtx.animMgr->NavigateByHandKeyIdx((int)e.value);
                        }

                        // Name hint: resolve moveList[1][e.value] → pool[1] → name
                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(1);
                        std::string animName = hasMgr
                            ? navCtx.animMgr->GetNameForHandKeyIdx((int)e.value) : "";
                        bool nameValid = !animName.empty();
                        ImGui::PushStyleColor(ImGuiCol_Text, nameValid ? kGreen : kPink);
                        char decodedHand[128];
                        if (nameValid)
                            snprintf(decodedHand, sizeof(decodedHand), "Hand Key #%u : %s", e.value, animName.c_str());
                        else
                            snprintf(decodedHand, sizeof(decodedHand), "Hand Key #%u : (invalid index)", e.value);
                        ImGui::TextUnformatted(decodedHand);
                        ImGui::PopStyleColor();
                    }
                    else if (isHandPoseProp(e.id))
                    {
                        uint32_t poseIdx    = e.value >> 8;
                        uint32_t blendFrames = e.value & 0xFF;
                        const int kMaxPose  = ((int)poseIdx + 1 > 64) ? (int)poseIdx + 1 : 64;

                        // Row 1: pose index dropdown
                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0); ImGui::TextDisabled("Pose");
                        ImGui::TableSetColumnIndex(1); ImGui::SetNextItemWidth(-1.0f);
                        char posePreview[24];
                        snprintf(posePreview, sizeof(posePreview), "Pose #%u", poseIdx);
                        if (ImGui::BeginCombo("##ep_pose_idx", posePreview, ImGuiComboFlags_HeightLarge))
                        {
                            for (int i = 0; i < kMaxPose; ++i)
                            {
                                char item[24];
                                snprintf(item, sizeof(item), "Pose #%d", i);
                                bool sel = ((int)poseIdx == i);
                                if (ImGui::Selectable(item, sel))
                                {
                                    e.value = ((uint32_t)i << 8) | blendFrames;
                                    poseIdx = (uint32_t)i;
                                    dirty = true;
                                }
                                if (sel) ImGui::SetItemDefaultFocus();
                            }
                            ImGui::EndCombo();
                        }

                        // Row 2: blend frames input
                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0); ImGui::TextDisabled("Blend");
                        ImGui::TableSetColumnIndex(1); ImGui::SetNextItemWidth(-1.0f);
                        int btmp = (int)blendFrames;
                        if (ImGui::InputInt("##ep_blend_frames", &btmp, 1, 10))
                        {
                            btmp = btmp < 0 ? 0 : btmp > 255 ? 255 : btmp;
                            e.value = (poseIdx << 8) | (uint32_t)btmp;
                            blendFrames = (uint32_t)btmp;
                            dirty = true;
                        }

                        // Row 3: decoded summary
                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(1);
                        ImGui::PushStyleColor(ImGuiCol_Text, kGreen);
                        char decoded[64];
                        snprintf(decoded, sizeof(decoded), "Pose #%u  |  blend: %u frames", poseIdx, blendFrames);
                        ImGui::TextUnformatted(decoded);
                        ImGui::PopStyleColor();
                    }
                    else if (e.id == 0x860E)
                    {
                        // param is raw blend duration; poses are hardcoded (Left #1, Right #2)
                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0); ImGui::TextDisabled("Blend");
                        ImGui::TableSetColumnIndex(1); ImGui::SetNextItemWidth(-1.0f);
                        int btmp860e = (int)e.value;
                        if (ImGui::InputInt("##ep_860e_blend", &btmp860e, 1, 10))
                        {
                            btmp860e = btmp860e < 0 ? 0 : btmp860e > 255 ? 255 : btmp860e;
                            e.value = (uint32_t)btmp860e;
                            dirty = true;
                        }
                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(1);
                        ImGui::PushStyleColor(ImGuiCol_Text, kGreen);
                        char decoded860e[64];
                        snprintf(decoded860e, sizeof(decoded860e),
                                 "blend: %u frames  |  Left: Pose #1, Right: Pose #2", e.value);
                        ImGui::TextUnformatted(decoded860e);
                        ImGui::PopStyleColor();
                    }
                    else
                    {
                        RowPU32("##ep_v1", ExtraPropLabel::Param0, e.value, FieldTT::ExtraProp::Param0, 0);
                    }

                    RowPU32("##ep_v2", ExtraPropLabel::Param1, e.value2, FieldTT::ExtraProp::Param1, 1);
                    RowPU32("##ep_v3", ExtraPropLabel::Param2, e.value3, FieldTT::ExtraProp::Param2, 2);
                    RowPU32("##ep_v4", ExtraPropLabel::Param3, e.value4, FieldTT::ExtraProp::Param3, 3);
                    RowPU32("##ep_v5", ExtraPropLabel::Param4, e.value5, FieldTT::ExtraProp::Param4, 4);
                    ImGui::EndTable();
                }
            }
            ImGui::EndChild();
            ImGui::PopStyleColor();
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
    ImGui::SetNextWindowSize(ImVec2(960.0f, 600.0f), ImGuiCond_FirstUseEver);
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
    navCtx.animMgr       = m_animMgr.get();

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
                uint32_t items = teGroups[gi].second;
                char lbl[48]; snprintf(lbl, sizeof(lbl), "#%u  %u items##teg%d", teGroups[gi].first, items, gi);
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

    // Resolve ordinal_id hash → move displayName for labeling.
    auto ordinalToName = [&](uint32_t ordinal) -> const char* {
        for (const auto& mv : m_data.moves)
            if (mv.ordinal_id2 == ordinal && !mv.displayName.empty())
                return mv.displayName.c_str();
        return nullptr;
    };

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
            uint32_t items = groups[gi].second;
            char lbl[48]; snprintf(lbl, sizeof(lbl), "#%u  %u items##pmg%d", groups[gi].first, items, gi);
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
                char lbl[128];
                if (isEnd) {
                    snprintf(lbl, sizeof(lbl), "#%u  [END]##pmi%u", k, idx);
                } else {
                    const char* mvName = ordinalToName(pm.value);
                    if (mvName) snprintf(lbl, sizeof(lbl), "#%u  %s##pmi%u", k, mvName, idx);
                    else        snprintf(lbl, sizeof(lbl), "#%u  0x%X##pmi%u", k, pm.value, idx);
                }
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
            {
                const char* mvName = (!isEnd) ? ordinalToName(pm.value) : nullptr;
                if (mvName)
                    ImGui::TextDisabled("parryable_move #%u  (block[%u])  →  %s",
                        m_parryWinSel.inner, idx, mvName);
                else
                    ImGui::TextDisabled("parryable_move #%u  (block[%u])%s",
                        m_parryWinSel.inner, idx, isEnd ? "  [END]" : "");
            }
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
    ImGui::BeginChild("##dlg_list", ImVec2(200.0f, 0.0f), true);
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
            char lbl[128];
            const char* vcName = LabelDB::Get().GetMoveName(d.voiceclip_key);
            const char* dramaLabel = MovesetDataDict::Get().GetDramaLabel(d.type, d.id);
            if (dramaLabel) snprintf(lbl, sizeof(lbl), "#%d  %s##dlgi%d", i, dramaLabel, i);
            else        snprintf(lbl, sizeof(lbl), "#%d  t:%u id:%u##dlgi%d", i, d.type, d.id, i);
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
            {
                const char* vcName = LabelDB::Get().GetMoveName(d.voiceclip_key);
                if (vcName) {
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(1);
                    ImGui::TextDisabled("%s", vcName);
                }
            }
            if (RowU32Edit("##dlg_fanim", DialogueLabel::FacialAnimIdx, d.facial_anim_idx, FieldTT::Dialogue::FacialAnimIdx)) m_dirty = true;
            ImGui::EndTable();
        }
    }
    ImGui::EndChild();

    ImGui::End();
}

// -------------------------------------------------------------
//  Reference Finder
// -------------------------------------------------------------

void MovesetEditorWindow::RenderSubWin_ReferenceFinder()
{
    if (!m_refFinder.open) return;

    ImGui::SetNextWindowSize(ImVec2(480.0f, 380.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(WinId("Reference Finder##blkwin").c_str(), &m_refFinder.open,
                      ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking))
    { ImGui::End(); return; }

    if (ImGui::BeginTabBar("##rftabs"))
    {
        if (ImGui::BeginTabItem("Move"))
        {
            ImGui::SetNextItemWidth(200.0f);
            bool doFind = ImGui::InputText("##rfmoveinput", m_refFinder.inputBuf,
                                           sizeof(m_refFinder.inputBuf),
                                           ImGuiInputTextFlags_EnterReturnsTrue);
            ImGui::SameLine();
            doFind |= ImGui::Button("Find##rffind");

            if (doFind)
            {
                m_refFinder.results.clear();
                m_refFinder.searched   = true;
                m_refFinder.targetMove = -1;

                const char* inp = m_refFinder.inputBuf;
                char* end = nullptr;
                long v = strtol(inp, &end, 10);
                if (end != inp && v >= 0 && (size_t)v < m_data.moves.size())
                {
                    m_refFinder.targetMove = (int)v;
                }
                else
                {
                    std::string query(inp);
                    for (char& ch : query) ch = (char)tolower((unsigned char)ch);
                    for (int i = 0; i < (int)m_data.moves.size(); ++i) {
                        std::string dn = m_data.moves[i].displayName;
                        for (char& ch : dn) ch = (char)tolower((unsigned char)ch);
                        if (dn == query) { m_refFinder.targetMove = i; break; }
                    }
                }

                if (m_refFinder.targetMove >= 0)
                {
                    const uint16_t tgt           = (uint16_t)m_refFinder.targetMove;
                    const uint64_t cancelEnd      = 0x8000;
                    const uint64_t groupCancelEnd = GameStatic::Get().data.groupCancelEnd;

                    auto resolveMove = [&](uint16_t mid) -> int {
                        if (mid < 0x8000) return (int)mid;
                        uint32_t aliasIdx = mid - 0x8000u;
                        if (aliasIdx < m_data.originalAliases.size())
                            return (int)m_data.originalAliases[aliasIdx];
                        return -1;
                    };

                    {
                        uint32_t g = 0, gStart = 0;
                        for (uint32_t i = 0; i < (uint32_t)m_data.cancelBlock.size(); ++i) {
                            const auto& c = m_data.cancelBlock[i];
                            if (c.command == cancelEnd) { ++g; gStart = i + 1; continue; }
                            if (resolveMove(c.move_id) == (int)tgt)
                                m_refFinder.results.push_back({RefFinderState::Hit::Type::Cancel, i, g, gStart});
                        }
                    }

                    {
                        uint32_t g = 0, gStart = 0;
                        for (uint32_t i = 0; i < (uint32_t)m_data.groupCancelBlock.size(); ++i) {
                            const auto& c = m_data.groupCancelBlock[i];
                            if (c.command == groupCancelEnd) { ++g; gStart = i + 1; continue; }
                            if (resolveMove(c.move_id) == (int)tgt)
                                m_refFinder.results.push_back({RefFinderState::Hit::Type::GroupCancel, i, g, gStart});
                        }
                    }

                    // reactionListBlock — check all 14 move fields per entry
                    {
                        for (uint32_t i = 0; i < (uint32_t)m_data.reactionListBlock.size(); ++i) {
                            const auto& rl = m_data.reactionListBlock[i];
                            const uint16_t fields[] = {
                                rl.standing, rl.crouch, rl.ch, rl.crouch_ch,
                                rl.left_side, rl.left_side_crouch, rl.right_side, rl.right_side_crouch,
                                rl.back, rl.back_crouch, rl.block, rl.crouch_block,
                                rl.wallslump, rl.downed
                            };
                            for (uint16_t mid : fields) {
                                if (resolveMove(mid) == (int)tgt) {
                                    m_refFinder.results.push_back({RefFinderState::Hit::Type::ReactionList, i, i, i});
                                    break;
                                }
                            }
                        }
                    }

                    // Post-process: compute owningMoves for each Cancel/GroupCancel hit.
                    std::vector<uint32_t> cGStart(m_data.cancelBlock.size(), 0);
                    {
                        uint32_t gs = 0;
                        for (uint32_t i = 0; i < (uint32_t)m_data.cancelBlock.size(); ++i) {
                            cGStart[i] = gs;
                            if (m_data.cancelBlock[i].command == cancelEnd) gs = i + 1;
                        }
                    }
                    std::unordered_map<uint32_t, std::vector<int>> movesByCGStart;
                    for (int mi = 0; mi < (int)m_data.moves.size(); ++mi) {
                        const auto& mv = m_data.moves[mi];
                        if (mv.cancel_idx  != 0xFFFFFFFF) movesByCGStart[mv.cancel_idx ].push_back(mi);
                        if (mv.cancel2_idx != 0xFFFFFFFF) movesByCGStart[mv.cancel2_idx].push_back(mi);
                    }
                    for (auto& hit : m_refFinder.results) {
                        using HT2 = RefFinderState::Hit::Type;
                        if (hit.type == HT2::Cancel) {
                            auto it = movesByCGStart.find(hit.groupFirst);
                            if (it != movesByCGStart.end()) hit.owningMoves = it->second;
                        } else if (hit.type == HT2::GroupCancel) {
                            for (uint32_t ci = 0; ci < (uint32_t)m_data.cancelBlock.size(); ++ci) {
                                if (m_data.cancelBlock[ci].group_cancel_list_idx != hit.groupFirst) continue;
                                uint32_t cgStart = cGStart[ci];
                                auto it = movesByCGStart.find(cgStart);
                                if (it == movesByCGStart.end()) continue;
                                for (int mi : it->second) {
                                    bool dup = false;
                                    for (int x : hit.owningMoves) { if (x == mi) { dup = true; break; } }
                                    if (!dup) hit.owningMoves.push_back(mi);
                                }
                            }
                        }
                    }
                }
            }

            ImGui::Separator();

            if (!m_refFinder.searched)
            {
                ImGui::TextDisabled("Enter a move index or name and press Find.");
            }
            else if (m_refFinder.targetMove < 0)
            {
                ImGui::TextDisabled("Invalid input.");
            }
            else if (m_refFinder.results.empty())
            {
                ImGui::TextDisabled("No references found for move #%d.", m_refFinder.targetMove);
            }
            else
            {
                ImGui::BeginChild("##rfresults", ImVec2(0.0f, 0.0f), false);

                using HT = RefFinderState::Hit::Type;
                HT lastType = (HT)0xFF;

                for (int ri = 0; ri < (int)m_refFinder.results.size(); ++ri)
                {
                    const auto& h = m_refFinder.results[ri];

                    if (h.type != lastType)
                    {
                        if (lastType != (HT)0xFF) ImGui::Spacing();
                        switch (h.type) {
                        case HT::Cancel:       ImGui::TextDisabled("--- Cancels ---");        break;
                        case HT::GroupCancel:  ImGui::TextDisabled("--- Group Cancels ---");  break;
                        case HT::ReactionList: ImGui::TextDisabled("--- Reaction Lists ---"); break;
                        }
                        lastType = h.type;
                    }

                    char goId[24]; snprintf(goId, sizeof(goId), "Go >##rfgo%d", ri);

                    if (h.type == HT::ReactionList)
                    {
                        ImGui::Text("  Reaction List ID : %u", h.blockIdx);
                        ImGui::SameLine();
                        if (ImGui::SmallButton(goId)) {
                            m_reacWin.selectedIdx   = (int)h.blockIdx;
                            m_reacWin.scrollPending = true;
                            m_reacWin.open          = true;
                        }
                    }
                    else
                    {
                        const uint32_t A = h.groupFirst;
                        const uint32_t B = h.blockIdx - h.groupFirst;
                        const uint32_t C = h.blockIdx;

                        const char* typeName = (h.type == HT::Cancel) ? "Cancel" : "Group Cancel";
                        ImGui::Text("  %s List ID : %u | Item Idx : %u | Absolute ID : %u",
                                    typeName, A, B, C);
                        ImGui::SameLine();
                        if (ImGui::SmallButton(goId))
                        {
                            if (h.type == HT::Cancel) {
                                m_cancelsWin.cancelSel.outer       = (int)h.groupOuter;
                                m_cancelsWin.cancelSel.inner       = (int)B;
                                m_cancelsWin.cancelSel.scrollOuter = true;
                            } else {
                                m_cancelsWin.groupCancelSel.outer       = (int)h.groupOuter;
                                m_cancelsWin.groupCancelSel.inner       = (int)B;
                                m_cancelsWin.groupCancelSel.scrollOuter = true;
                            }
                            m_cancelsWin.open         = true;
                            m_cancelsWin.pendingFocus = true;
                            m_cancelsWin.pendingTab   = (h.type == HT::Cancel) ? 0 : 1;
                        }

                        if (!h.owningMoves.empty()) {
                            ImGui::Indent(16.0f);
                            ImGui::TextDisabled("-> Referenced Move :");
                            for (int mi : h.owningMoves) {
                                const std::string& dname = (mi >= 0 && mi < (int)m_data.moves.size())
                                                           ? m_data.moves[mi].displayName : "?";
                                ImGui::Text("    - %s", dname.c_str());
                                ImGui::SameLine();
                                char mvGoId[32]; snprintf(mvGoId, sizeof(mvGoId), "Go >##rfmv%d_%d", ri, mi);
                                if (ImGui::SmallButton(mvGoId)) {
                                    m_selectedIdx = mi;
                                    m_moveListScrollPending = true;
                                }
                            }
                            ImGui::Unindent(16.0f);
                        }
                    }
                }

                ImGui::EndChild();
            }

            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    ImGui::End();
}

// =============================================================
//  RenderCommandCreator
//  Modal popup for building cancel command values visually.
// =============================================================

// -------------------------------------------------------------
//  Command label helpers
// -------------------------------------------------------------

static void FillBtnStr(char* dst, int dstSz, uint64_t mask)
{
    static const char*   kBtnNames[] = { "1","2","3","4","H","SS","RA" };
    static const uint64_t kBtnBits[] = { 1,   2,  4,  8, 16,  32,  64 };
    bool first = true;
    for (int i = 0; i < 7; ++i) {
        if (mask & kBtnBits[i]) {
            int len = (int)strlen(dst);
            snprintf(dst + len, dstSz - len, "%s%s", first ? "" : "+", kBtnNames[i]);
            first = false;
        }
    }
}

// Converts a full 64-bit cancel command to a short human-readable label.
// Tries exact LabelDB match first, then decomposes direction + button bits.
static void CmdToLabel(char* out, int outSz, uint64_t cmd)
{
    const char* exact = LabelDB::Get().Cmd(cmd);
    if (exact) { snprintf(out, outSz, "%s", exact); return; }

    uint64_t dirBits = cmd & 0x3FEULL;
    uint64_t pressed = (cmd >> 32) & 0x7F;
    uint64_t held    = (cmd >> 40) & 0x7F;
    uint64_t rel     = (cmd >> 48) & 0x7F;

    char dirBuf[16] = "Any";
    if (dirBits) {
        const char* dl = LabelDB::Get().Cmd(dirBits);
        if (dl) snprintf(dirBuf, sizeof(dirBuf), "%s", dl);
        else    snprintf(dirBuf, sizeof(dirBuf), "0x%llX", (unsigned long long)dirBits);
    }

    char pressBuf[32] = {}; FillBtnStr(pressBuf, sizeof(pressBuf), pressed);
    char heldBuf[32]  = {}; FillBtnStr(heldBuf,  sizeof(heldBuf),  held);
    char relBuf[32]   = {}; FillBtnStr(relBuf,   sizeof(relBuf),   rel);

    snprintf(out, outSz, "%s", dirBuf);
    if (pressBuf[0]) { int l = (int)strlen(out); snprintf(out+l, outSz-l, "+%s",   pressBuf); }
    if (heldBuf[0])  { int l = (int)strlen(out); snprintf(out+l, outSz-l, " h:%s", heldBuf);  }
    if (relBuf[0])   { int l = (int)strlen(out); snprintf(out+l, outSz-l, " r:%s", relBuf);   }
}

static constexpr uint64_t kCCModeMask    = 0xFF00000000000000ULL;
static constexpr uint64_t kCCModeReg     = 0x4000000000000000ULL;
static constexpr uint64_t kCCModePartial = 0x2000000000000000ULL;
static constexpr uint64_t kCCModeDirOnly = 0x8000000000000000ULL;
static constexpr uint64_t kCCDirMask     = 0x00000000000003FEULL;
static constexpr uint64_t kCCBtnMaskAll  =
    (uint64_t(0x7F) << 32) | (uint64_t(0x7F) << 40) | (uint64_t(0x7F) << 48);

void MovesetEditorWindow::RenderCommandCreator()
{
    if (!m_cmdCreator.pendingOpen && !m_cmdCreator.open) return;

    // Resolve target viewport
    ImGuiViewport* vp = nullptr;
    if (m_cmdCreator.callerViewportId)
        vp = ImGui::FindViewportByID(m_cmdCreator.callerViewportId);
    if (!vp && m_viewportId)
        vp = ImGui::FindViewportByID(m_viewportId);
    if (!vp) vp = ImGui::GetMainViewport();
    const ImVec2 vpCenter(vp->Pos.x + vp->Size.x * 0.5f, vp->Pos.y + vp->Size.y * 0.5f);

    // 1×1 invisible host window on the correct viewport
    constexpr ImGuiWindowFlags kHostF =
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoDocking  | ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoScrollbar| ImGuiWindowFlags_NoNav |
        ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoBackground |
        ImGuiWindowFlags_NoMouseInputs;
    ImGui::SetNextWindowViewport(vp->ID);
    ImGui::SetNextWindowPos(ImVec2(vp->Pos.x - 100.f, vp->Pos.y - 100.f));
    ImGui::SetNextWindowSize(ImVec2(1.f, 1.f));
    std::string hostId   = WinId("##cc_host");
    std::string popupId  = WinId("Command Creator##cc");
    ImGui::Begin(hostId.c_str(), nullptr, kHostF);

    if (m_cmdCreator.pendingOpen) {
        ImGui::OpenPopup(popupId.c_str());
        m_cmdCreator.pendingOpen = false;
        m_cmdCreator.open        = true;
    }

    ImGui::SetNextWindowSize(ImVec2(530.f, 670.f), ImGuiCond_Always);
    ImGui::SetNextWindowPos(vpCenter, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    bool keepOpen = true;
    bool popupVis = ImGui::BeginPopupModal(popupId.c_str(), &keepOpen,
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings);

    if (!popupVis) {
        if (!keepOpen) m_cmdCreator.open = false;
        ImGui::End(); // host window
        return;
    }
    if (!keepOpen) {
        m_cmdCreator.open = false;
        ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
        ImGui::End(); // host window
        return;
    }

    uint64_t&    val  = m_cmdCreator.value;
    ImGuiStyle&  sty  = ImGui::GetStyle();
    const ImVec4 kGreen (0.20f, 1.00f, 0.40f, 1.0f);
    const ImVec4 kBlue  (0.40f, 0.80f, 1.00f, 1.0f);
    const ImVec4 kPurple(0.50f, 0.15f, 0.80f, 1.0f);
    const ImVec4 kBtnOn (0.15f, 0.35f, 0.70f, 1.0f);

    // ---- Standalone banner ----
    if (!m_cmdCreator.target) {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.25f, 0.15f, 0.0f, 1.0f));
        if (ImGui::BeginChild("##cc_banner", ImVec2(-1, 42), ImGuiChildFlags_Borders)) {
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4);
            ImGui::TextColored(ImVec4(1.f, 0.85f, 0.f, 1.f), "Standalone Mode:");
            ImGui::SameLine();
            ImGui::TextUnformatted("Clicking \"Save\" will copy the command value to your clipboard.");
        }
        ImGui::EndChild();
        ImGui::PopStyleColor();
        ImGui::Spacing();
    }

    // ---- Raw value display ----
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.10f, 0.10f, 0.12f, 1.0f));
    if (ImGui::BeginChild("##cc_hdr", ImVec2(-1, 98), ImGuiChildFlags_Borders)) {
        float avail = ImGui::GetContentRegionAvail().x;

        // "RAW VALUE (HEX)" left, "Mode: 0xXX" right
        ImGui::TextDisabled("RAW VALUE (HEX)");
        uint8_t modeByte = (uint8_t)(val >> 56);
        char modeBuf[16]; snprintf(modeBuf, sizeof(modeBuf), "Mode: 0x%02X", modeByte);
        float modeW = ImGui::CalcTextSize(modeBuf).x;
        ImGui::SameLine(avail - modeW);
        ImGui::TextDisabled("%s", modeBuf);

        // Editable hex (green, centered)
        char hexBuf[20]; snprintf(hexBuf, sizeof(hexBuf), "%016llX", (unsigned long long)val);
        float hexW = 300.0f;
        ImGui::SetCursorPosX((avail - hexW) * 0.5f + sty.WindowPadding.x);
        ImGui::PushStyleColor(ImGuiCol_Text, kGreen);
        ImGui::SetNextItemWidth(hexW);
        if (ImGui::InputText("##cc_hexinput", hexBuf, sizeof(hexBuf),
            ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_AutoSelectAll))
            val = (uint64_t)strtoull(hexBuf, nullptr, 16);
        ImGui::PopStyleColor();

        // Decimal
        char decBuf[40]; snprintf(decBuf, sizeof(decBuf), "Decimal: %llu", (unsigned long long)val);
        float decW = ImGui::CalcTextSize(decBuf).x;
        ImGui::SetCursorPosX((avail - decW) * 0.5f + sty.WindowPadding.x);
        ImGui::TextDisabled("%s", decBuf);

        // Command label
        const char* cmdLbl = LabelDB::Get().Cmd(val);
        char cmdLblBuf[64]; snprintf(cmdLblBuf, sizeof(cmdLblBuf), "Command: %s", cmdLbl ? cmdLbl : "Any");
        float cmdLblW = ImGui::CalcTextSize(cmdLblBuf).x;
        ImGui::SetCursorPosX((avail - cmdLblW) * 0.5f + sty.WindowPadding.x);
        ImGui::TextColored(kBlue, "%s", cmdLblBuf);
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::Spacing();

    // ---- Main content area (tabs), leave room for footer ----
    const float footerH = ImGui::GetFrameHeight() + sty.ItemSpacing.y * 2 + sty.WindowPadding.y;
    if (ImGui::BeginChild("##cc_main", ImVec2(-1, ImGui::GetContentRegionAvail().y - footerH),
        false, ImGuiWindowFlags_NoScrollbar))
    {
        if (ImGui::BeginTabBar("##cc_tabs"))
        {
            // =================== LINEAR INPUT ===================
            if (ImGui::BeginTabItem("Linear Input"))
            {
                if (ImGui::BeginChild("##cc_linear", ImVec2(-1, -1), false))
                {
                    // Input Mode
                    ImGui::TextUnformatted("INPUT MODE");
                    ImGui::Spacing();
                    struct ModeOpt { const char* label; const char* hexStr; uint64_t bits; };
                    static const ModeOpt kModes[3] = {
                        { "Regular",        "(0x40)", kCCModeReg     },
                        { "Partial",        "(0x20)", kCCModePartial },
                        { "Direction Only", "(0x80)", kCCModeDirOnly },
                    };
                    float modeW3 = (ImGui::GetContentRegionAvail().x - sty.ItemSpacing.x * 2) / 3.0f;
                    uint64_t curMode = val & kCCModeMask;
                    for (int m = 0; m < 3; ++m) {
                        if (m > 0) ImGui::SameLine();
                        bool active = (curMode == kModes[m].bits);
                        if (active) ImGui::PushStyleColor(ImGuiCol_Button, kPurple);
                        char bid[32]; snprintf(bid, sizeof(bid), "%s\n%s##mode%d",
                            kModes[m].label, kModes[m].hexStr, m);
                        if (ImGui::Button(bid, ImVec2(modeW3, 42)))
                            val = (val & ~kCCModeMask) | kModes[m].bits;
                        if (active) ImGui::PopStyleColor();
                    }
                    ImGui::TextDisabled("Select a mode to define strictness.");
                    ImGui::Spacing();
                    ImGui::Separator();
                    ImGui::Spacing();

                    // Direction + Buttons side by side
                    float colW = (ImGui::GetContentRegionAvail().x - sty.ItemSpacing.x) * 0.5f;

                    // Direction column
                    ImGui::BeginGroup();
                    ImGui::TextColored(kBlue, "Direction");
                    {
                        float cx = ImGui::GetCursorPosX() + colW
                                   - ImGui::CalcTextSize("Clear").x
                                   - sty.FramePadding.x * 2 - sty.ItemSpacing.x;
                        ImGui::SameLine(cx);
                    }
                    if (ImGui::SmallButton("Clear##dirclr")) val &= ~kCCDirMask;
                    ImGui::Separator();
                    ImGui::Spacing();

                    struct DirDef { const char* lbl; uint64_t bit; };
                    static const DirDef kDirGrid[3][3] = {
                        {{"ub",0x80ULL},  {"u", 0x100ULL}, {"uf",0x200ULL}},
                        {{"b", 0x10ULL},  {"n", 0x020ULL}, {"f", 0x040ULL}},
                        {{"db",0x02ULL},  {"d", 0x004ULL}, {"df",0x008ULL}},
                    };
                    float dirBtnW = (colW - sty.ItemSpacing.x * 2) / 3.0f;
                    float dirBtnH = dirBtnW * 0.65f;
                    for (int row = 0; row < 3; ++row) {
                        for (int col = 0; col < 3; ++col) {
                            if (col > 0) ImGui::SameLine();
                            const DirDef& de = kDirGrid[row][col];
                            bool on = (val & de.bit) != 0;
                            if (on) ImGui::PushStyleColor(ImGuiCol_Button, kBtnOn);
                            char bid[16]; snprintf(bid, sizeof(bid), "%s##d%d%d", de.lbl, row, col);
                            if (ImGui::Button(bid, ImVec2(dirBtnW, dirBtnH))) {
                                if (on) val &= ~de.bit; else val |= de.bit;
                            }
                            if (on) ImGui::PopStyleColor();
                        }
                    }
                    ImGui::EndGroup();
                    ImGui::SameLine();

                    // Buttons column
                    ImGui::BeginGroup();
                    ImGui::TextColored(ImVec4(1.f, 0.4f, 0.4f, 1.f), "Buttons");
                    {
                        float cx = ImGui::GetCursorPosX() + colW
                                   - ImGui::CalcTextSize("Clear").x
                                   - sty.FramePadding.x * 2 - sty.ItemSpacing.x;
                        ImGui::SameLine(cx);
                    }
                    if (ImGui::SmallButton("Clear##btnclr")) val &= ~kCCBtnMaskAll;
                    ImGui::Separator();
                    ImGui::Spacing();

                    struct BtnDef { const char* lbl; uint64_t bit; };
                    static const BtnDef kBtns[7] = {
                        {"1",0x01},{"2",0x02},{"3",0x04},{"4",0x08},
                        {"H",0x10},{"SS",0x20},{"RA",0x40},
                    };
                    static const int   kShifts[3]    = { 32, 40, 48 };
                    static const char* kSecLbls[3]   = { "PRESSED","HELD","NOT HELD (RELEASE)" };

                    float smBtnW = (colW - sty.ItemSpacing.x * 4) / 5.0f;
                    for (int s = 0; s < 3; ++s) {
                        ImGui::TextDisabled("%s", kSecLbls[s]);
                        int shift = kShifts[s];
                        for (int b = 0; b < 5; ++b) {
                            if (b > 0) ImGui::SameLine();
                            bool on = ((val >> shift) & kBtns[b].bit) != 0;
                            if (on) ImGui::PushStyleColor(ImGuiCol_Button, kBtnOn);
                            char bid[16]; snprintf(bid, sizeof(bid), "%s##b%d%d", kBtns[b].lbl, s, b);
                            if (ImGui::Button(bid, ImVec2(smBtnW, 0))) {
                                uint64_t mask = uint64_t(kBtns[b].bit) << shift;
                                if (on) val &= ~mask; else val |= mask;
                            }
                            if (on) ImGui::PopStyleColor();
                        }
                        for (int b = 5; b < 7; ++b) {
                            if (b > 5) ImGui::SameLine();
                            bool on = ((val >> shift) & kBtns[b].bit) != 0;
                            if (on) ImGui::PushStyleColor(ImGuiCol_Button, kBtnOn);
                            char bid[16]; snprintf(bid, sizeof(bid), "%s##b%d%d", kBtns[b].lbl, s, b);
                            if (ImGui::Button(bid, ImVec2(smBtnW * 1.6f, 0))) {
                                uint64_t mask = uint64_t(kBtns[b].bit) << shift;
                                if (on) val &= ~mask; else val |= mask;
                            }
                            if (on) ImGui::PopStyleColor();
                        }
                        if (s < 2) ImGui::Spacing();
                    }
                    ImGui::EndGroup();
                }
                ImGui::EndChild();
                ImGui::EndTabItem();
            }

            // =================== PRESETS ===================
            if (ImGui::BeginTabItem("Presets"))
            {
                if (ImGui::BeginChild("##cc_presets", ImVec2(-1, -1), false))
                {
                    struct CmdPreset { const char* name; const char* hexStr; uint64_t value; };
                    static const CmdPreset kPresets[] = {
                        { "End of List (EOL)",  "0x8000", 0x8000ULL },
                        { "Double Tap F",       "0x8001", 0x8001ULL },
                        { "Double Tap B",       "0x8002", 0x8002ULL },
                        { "Double Tap F (Alt)", "0x8003", 0x8003ULL },
                        { "Double Tap B (Alt)", "0x8004", 0x8004ULL },
                        { "Double Tap U",       "0x8005", 0x8005ULL },
                        { "Double Tap D",       "0x8006", 0x8006ULL },
                        { "Double Tap UF",      "0x8007", 0x8007ULL },
                        { "Double Tap DB",      "0x8008", 0x8008ULL },
                        { "Double Tap UB",      "0x8009", 0x8009ULL },
                        { "Double Tap DF",      "0x800A", 0x800AULL },
                        { "Group Cancel Start", "0x8012", 0x8012ULL },
                        { "Group Cancel End",   "0x8013", 0x8013ULL },
                    };
                    static const int kPresetCount = (int)(sizeof(kPresets) / sizeof(kPresets[0]));

                    float cardW = (ImGui::GetContentRegionAvail().x - sty.ItemSpacing.x) * 0.5f;
                    for (int i = 0; i < kPresetCount; ++i) {
                        if (i % 2 != 0) ImGui::SameLine();
                        bool selected = (val == kPresets[i].value);
                        ImGui::PushID(i + 1000);
                        ImGui::PushStyleColor(ImGuiCol_ChildBg,
                            selected ? ImVec4(0.10f, 0.28f, 0.10f, 1.0f)
                                     : ImVec4(0.14f, 0.14f, 0.14f, 1.0f));
                        if (ImGui::BeginChild("##preset_card", ImVec2(cardW, 52), ImGuiChildFlags_Borders)) {
                            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4);
                            ImGui::TextUnformatted(kPresets[i].name);
                            ImGui::TextDisabled("%s", kPresets[i].hexStr);
                        }
                        ImGui::EndChild();
                        if (ImGui::IsItemClicked()) val = kPresets[i].value;
                        ImGui::PopStyleColor();
                        ImGui::PopID();
                    }
                }
                ImGui::EndChild();
                ImGui::EndTabItem();
            }

            // =================== INPUT SEQUENCES ===================
            if (ImGui::BeginTabItem("Input Sequences"))
            {
                if (ImGui::BeginChild("##cc_inseq", ImVec2(-1, -1), false))
                {
                    ImGui::TextWrapped(
                        "Select an Input Sequence from the list below. "
                        "This will set the command to reference the sequence index.");
                    ImGui::Spacing();

                    uint32_t seqBase = GameStatic::Get().data.inputSeqStart;
                    const std::vector<ParsedInputSequence>& seqs   = m_data.inputSequenceBlock;
                    const std::vector<ParsedInput>&         inputs = m_data.inputBlock;

                    if (seqs.empty()) {
                        ImGui::TextDisabled("No input sequences in this moveset.");
                    } else {
                        for (size_t i = 0; i < seqs.size(); ++i) {
                            const ParsedInputSequence& seq = seqs[i];
                            uint64_t seqCmd = (uint64_t)(seqBase + (uint32_t)i);
                            bool selected   = (val == seqCmd);

                            // Build "step > step" description
                            char desc[256] = {};
                            int  descLen   = 0;
                            if (seq.input_start_idx != 0xFFFFFFFF) {
                                for (uint16_t k = 0; k < seq.input_amount && k < 8; ++k) {
                                    uint32_t idx = seq.input_start_idx + k;
                                    if (idx >= (uint32_t)inputs.size()) break;
                                    char lbl[48] = {};
                                    CmdToLabel(lbl, sizeof(lbl), inputs[idx].command);
                                    char step[56];
                                    snprintf(step, sizeof(step), "%s%s", k > 0 ? " > " : "", lbl);
                                    int sl = (int)strlen(step);
                                    if (descLen + sl < (int)sizeof(desc) - 1) {
                                        memcpy(desc + descLen, step, sl);
                                        descLen += sl;
                                    }
                                }
                            }
                            if (descLen == 0) snprintf(desc, sizeof(desc), "(empty)");

                            ImGui::PushID((int)i + 2000);
                            ImGui::PushStyleColor(ImGuiCol_ChildBg,
                                selected ? ImVec4(0.10f, 0.25f, 0.10f, 1.0f)
                                         : ImVec4(0.12f, 0.12f, 0.12f, 1.0f));
                            if (ImGui::BeginChild("##seq_row", ImVec2(-1, 32), ImGuiChildFlags_Borders)) {
                                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 6);
                                ImGui::TextColored(kGreen, "#%d", (int)i);
                                ImGui::SameLine(48);
                                ImGui::TextDisabled("0x%04X", (unsigned)(seqBase + i));
                                ImGui::SameLine(110);
                                ImGui::TextColored(kGreen, "%s", desc);
                            }
                            ImGui::EndChild();
                            if (ImGui::IsItemClicked()) val = seqCmd;
                            ImGui::PopStyleColor();
                            ImGui::PopID();
                        }
                    }
                }
                ImGui::EndChild();
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }
    }
    ImGui::EndChild(); // ##cc_main

    // ---- Footer ----
    ImGui::Separator();
    float saveBtnW   = 130.0f;
    float cancelBtnW = 80.0f;
    float btnX = ImGui::GetContentRegionAvail().x - saveBtnW - cancelBtnW - sty.ItemSpacing.x;
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + btnX);
    if (ImGui::Button("Cancel##cc_cancel", ImVec2(cancelBtnW, 0))) {
        m_cmdCreator.open = false;
        ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.15f, 0.40f, 0.90f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.55f, 1.00f, 1.0f));
    if (ImGui::Button("Save Command##cc_save", ImVec2(saveBtnW, 0))) {
        if (m_cmdCreator.target) {
            *m_cmdCreator.target = val;
            if (m_cmdCreator.dirtyFlag) *m_cmdCreator.dirtyFlag = true;
        } else {
            char clip[22]; snprintf(clip, sizeof(clip), "0x%016llX", (unsigned long long)val);
            ImGui::SetClipboardText(clip);
        }
        m_cmdCreator.open = false;
        ImGui::CloseCurrentPopup();
    }
    ImGui::PopStyleColor(2);

    ImGui::EndPopup();
    ImGui::End(); // host window
}
