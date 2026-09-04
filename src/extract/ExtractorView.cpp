// ExtractorView.cpp -- ImGui panel for game moveset extraction
#include "ExtractorView.h"
#include "imgui/imgui.h"
#include <cstdio>
#include <windows.h>
#include <shellapi.h>
#pragma comment(lib, "Shell32.lib")

ExtractorView::ExtractorView(const std::string& movesetRootDir)
    : m_destFolder(movesetRootDir)
{
}

void ExtractorView::OnGameTargetChanged()
{
    // Drop the inactive game connection so slot info stays consistent.
    if (m_gameTarget == GameTarget::Tekken8)
        m_t7Extractor.Disconnect();
    else
        m_extractor.Disconnect();
    m_lastMsg.clear();
    m_lastOk = false;
}

// -------------------------------------------------------------
//  CheckThread -- call each frame to pick up worker results
// -------------------------------------------------------------

void ExtractorView::CheckThread()
{
    if (!m_loading || !m_threadDone) return;
    if (m_thread.joinable()) m_thread.join();
    m_lastMsg = m_threadMsg;
    m_lastOk  = m_threadOk;
    m_loading = false;
    if (m_lastOk && m_onSuccess) m_onSuccess();
}

// -------------------------------------------------------------
//  StartExtract -- pre-check on main thread, then spawn worker
// -------------------------------------------------------------

void ExtractorView::StartExtract(int slotIndex)
{
    if (m_loading) return;

    m_lastMsg.clear();
    m_lastOk = false;

    if (m_destFolder.empty())
    {
        m_lastMsg = "Output folder not set. Configure Moveset Root Dir in Settings.";
        return;
    }

    if (m_gameTarget == GameTarget::Tekken7)
    {
        if (!m_t7Extractor.IsConnected())
        {
            if (!m_t7Extractor.Connect())
            {
                m_lastMsg = m_t7Extractor.GetStatusMsg();
                return;
            }
        }
        m_t7Extractor.RefreshSlots();
        const T7PlayerSlotInfo& slot = m_t7Extractor.GetSlot(slotIndex);
        if (!slot.valid)
        {
            m_lastMsg = std::string(slotIndex == 0 ? "P1" : "P2") +
                        ": no valid T7 moveset in memory. Is a character loaded?";
            return;
        }

        m_loading    = true;
        m_threadDone = false;
        m_thread = std::thread([this, slotIndex]()
        {
            std::string err;
            bool ok = m_t7Extractor.ExtractToFile(slotIndex, m_destFolder, err);
            m_threadMsg  = ok ? m_t7Extractor.GetStatusMsg() : err;
            m_threadOk   = ok;
            m_threadDone = true;
        });
        return;
    }

    // TEKKEN 8
    if (!m_extractor.IsConnected())
    {
        if (!m_extractor.Connect())
        {
            m_lastMsg = m_extractor.GetStatusMsg();
            return;
        }
    }

    m_extractor.RefreshSlots();

    const PlayerSlotInfo& slot = m_extractor.GetSlot(slotIndex);
    if (!slot.valid)
    {
        m_lastMsg = std::string(slotIndex == 0 ? "P1" : "P2") +
                    ": no valid moveset in memory. Is a character loaded?";
        return;
    }

    m_loading    = true;
    m_threadDone = false;
    m_thread = std::thread([this, slotIndex]()
    {
        std::string err;
        bool ok = m_extractor.ExtractToFile(slotIndex, m_destFolder, err);
        m_threadMsg  = ok ? m_extractor.GetStatusMsg() : err;
        m_threadOk   = ok;
        m_threadDone = true;
    });
}

// -------------------------------------------------------------
//  RenderButtons -- game dropdown + Extract P1 / Extract P2
// -------------------------------------------------------------

void ExtractorView::RenderButtons()
{
    const bool canExtract = !m_destFolder.empty();
    const bool busy = m_loading;

    if (busy) ImGui::BeginDisabled();

    // Game target dropdown (left of Extract buttons)
    {
        const char* items[] = { "TEKKEN 8", "TEKKEN 7" };
        int cur = static_cast<int>(m_gameTarget);
        ImGui::SetNextItemWidth(110.0f);
        if (ImGui::Combo("##GameTarget", &cur, items, 2))
        {
            GameTarget next = static_cast<GameTarget>(cur);
            if (next != m_gameTarget)
            {
                m_gameTarget = next;
                OnGameTargetChanged();
            }
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Select which game to extract from.");
    }

    ImGui::SameLine();

    if (!canExtract) ImGui::BeginDisabled();

    if (ImGui::Button("Extract P1", ImVec2(120.0f, 0.0f)))
        StartExtract(0);

    ImGui::SameLine();

    if (ImGui::Button("Extract P2", ImVec2(120.0f, 0.0f)))
        StartExtract(1);

    if (!canExtract)
    {
        ImGui::EndDisabled();
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
            ImGui::SetTooltip("Set Moveset Root Dir in Settings first.");
    }

    if (busy) ImGui::EndDisabled();

    // Open Directory button -- right-aligned on the same row
    {
        const float btnW = 120.0f;
        ImGui::SameLine(ImGui::GetContentRegionMax().x - btnW);
        if (!canExtract) ImGui::BeginDisabled();
        if (ImGui::Button("Open Directory", ImVec2(btnW, 0.0f)))
        {
            int wLen = MultiByteToWideChar(CP_UTF8, 0, m_destFolder.c_str(), -1, nullptr, 0);
            std::wstring wPath(wLen > 0 ? wLen - 1 : 0, L'\0');
            MultiByteToWideChar(CP_UTF8, 0, m_destFolder.c_str(), -1, &wPath[0], wLen);
            ShellExecuteW(nullptr, L"explore", wPath.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        }
        if (!canExtract) ImGui::EndDisabled();
    }
}

// -------------------------------------------------------------
//  RenderLog -- status, slot info, save-path hint
// -------------------------------------------------------------

void ExtractorView::RenderLog()
{
    if (!m_lastMsg.empty())
    {
        ImVec4 col = m_lastOk
            ? ImVec4(0.35f, 1.0f, 0.50f, 1.0f)
            : ImVec4(1.0f,  0.35f, 0.35f, 1.0f);
        ImGui::TextColored(col, "%s", m_lastMsg.c_str());
        ImGui::SameLine();
        if (ImGui::SmallButton("Copy"))
            ImGui::SetClipboardText(m_lastMsg.c_str());
    }

    if (m_loading) return;

    if (m_gameTarget == GameTarget::Tekken7)
    {
        if (m_t7Extractor.IsConnected())
        {
            ImGui::Spacing();
            for (int i = 0; i < 2; ++i)
            {
                const T7PlayerSlotInfo& slot = m_t7Extractor.GetSlot(i);
                ImGui::PushID(i);
                if (slot.valid)
                {
                    char info[128];
                    snprintf(info, sizeof(info), "%s  %s  (%u moves)",
                             i == 0 ? "P1" : "P2",
                             slot.charaName.c_str(),
                             slot.moveCount);
                    ImGui::TextDisabled("%s", info);
                    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
                    {
                        ImGui::BeginTooltip();
                        ImGui::Text("playerAddr  : 0x%llX", (unsigned long long)slot.playerAddr);
                        ImGui::Text("movesetAddr : 0x%llX", (unsigned long long)slot.movesetAddr);
                        ImGui::Text("fighterId   : %u",     slot.charaId);
                        ImGui::EndTooltip();
                    }
                }
                else
                {
                    ImGui::TextDisabled("%s  --  (no moveset)", i == 0 ? "P1" : "P2");
                }
                ImGui::PopID();
            }
        }
        ImGui::Spacing();
        ImGui::TextDisabled("Save path: <root>/TK7_<CharaName>/moveset.motbin  (T7->T8 convert; no .anmbin yet)");
        return;
    }

    if (m_extractor.IsConnected())
    {
        ImGui::Spacing();
        for (int i = 0; i < 2; ++i)
        {
            const PlayerSlotInfo& slot = m_extractor.GetSlot(i);
            ImGui::PushID(i);
            if (slot.valid)
            {
                char info[128];
                snprintf(info, sizeof(info), "%s  %s  (%u moves)",
                         i == 0 ? "P1" : "P2",
                         slot.charaName.c_str(),
                         slot.moveCount);
                ImGui::TextDisabled("%s", info);
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
                {
                    ImGui::BeginTooltip();
                    ImGui::Text("playerAddr : 0x%llX", (unsigned long long)slot.playerAddr);
                    ImGui::Text("motbinAddr : 0x%llX", (unsigned long long)slot.motbinAddr);
                    ImGui::Text("charaId    : %u",     slot.charaId);
                    ImGui::EndTooltip();
                }
            }
            else
            {
                ImGui::TextDisabled("%s  --  (no moveset)", i == 0 ? "P1" : "P2");
            }
            ImGui::PopID();
        }
    }

    ImGui::Spacing();
    ImGui::TextDisabled("Save path: <root>/TK8_<CharaName>/moveset.motbin  (.anmbin / .stllstb / .mvl if tkdata.bin found)");
}
