// ExtractorView.cpp -- ImGui panel for game moveset extraction
#include "ExtractorView.h"
#include "imgui/imgui.h"
#include <cstdio>

ExtractorView::ExtractorView(const std::string& movesetRootDir)
    : m_destFolder(movesetRootDir)
{
}

// -------------------------------------------------------------
//  TryExtract -- one-click: connect -> refresh slot -> extract
// -------------------------------------------------------------

void ExtractorView::TryExtract(int slotIndex)
{
    m_lastMsg.clear();
    m_lastOk = false;

    if (m_destFolder.empty())
    {
        m_lastMsg = "Output folder not set. Configure Moveset Root Dir in Settings.";
        return;
    }

    // (Re)connect if needed
    if (!m_extractor.IsConnected())
    {
        if (!m_extractor.Connect())
        {
            m_lastMsg = m_extractor.GetStatusMsg();
            return;
        }
    }

    // Refresh slot info
    m_extractor.RefreshSlots();

    const PlayerSlotInfo& slot = m_extractor.GetSlot(slotIndex);
    if (!slot.valid)
    {
        m_lastMsg = std::string(slotIndex == 0 ? "P1" : "P2") +
                    ": no valid moveset in memory. Is a character loaded?";
        return;
    }

    // Extract
    std::string err;
    if (m_extractor.ExtractToFile(slotIndex, m_destFolder, err))
    {
        m_lastMsg = m_extractor.GetStatusMsg();
        m_lastOk  = true;
        if (m_onSuccess) m_onSuccess();
    }
    else
    {
        m_lastMsg = err;
    }
}

// -------------------------------------------------------------
//  RenderButtons -- Extract P1 / Extract P2 only
// -------------------------------------------------------------

void ExtractorView::RenderButtons()
{
    const bool canExtract = !m_destFolder.empty();

    if (!canExtract) ImGui::BeginDisabled();

    if (ImGui::Button("Extract P1", ImVec2(120.0f, 0.0f)))
        TryExtract(0);

    ImGui::SameLine();

    if (ImGui::Button("Extract P2", ImVec2(120.0f, 0.0f)))
        TryExtract(1);

    if (!canExtract)
    {
        ImGui::EndDisabled();
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
            ImGui::SetTooltip("Set Moveset Root Dir in Settings first.");
    }
}

// -------------------------------------------------------------
//  RenderLog -- status, slot info, save-path hint
// -------------------------------------------------------------

void ExtractorView::RenderLog()
{
    // -- Status message ---------------------------------------
    if (!m_lastMsg.empty())
    {
        ImVec4 col = m_lastOk
            ? ImVec4(0.35f, 1.0f, 0.50f, 1.0f)
            : ImVec4(1.0f,  0.35f, 0.35f, 1.0f);
        ImGui::TextColored(col, "%s", m_lastMsg.c_str());
    }

    // -- Slot info (shown after a successful connect) ---------
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

    // -- Output path hint -------------------------------------
    ImGui::Spacing();
    ImGui::TextDisabled("Save path: <root>/TK8_<CharaName>/moveset.motbin");
}
