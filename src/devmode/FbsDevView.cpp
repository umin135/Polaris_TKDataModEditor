#ifdef _DEBUG
// Developer-mode fbsdata view
// Imports/exports original FlatBuffers .bin files directly.

#include "devmode/FbsDevView.h"
#include "devmode/FbsBinaryIO.h"
#include "data/FieldNames.h"
#include "imgui/imgui.h"
#include <cstring>
#include <string>

static const float LIST_WIDTH = 290.0f;

// ─────────────────────────────────────────────────────────────────────────────
//  Top toolbar  (Export only — no Save/Load)
// ─────────────────────────────────────────────────────────────────────────────

void FbsDevView::RenderToolbar()
{
    ImGui::SetCursorPos(ImVec2(10.0f, 8.0f));

    // Dev mode badge
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.75f, 0.2f, 1.0f));
    ImGui::Text("[DEV]");
    ImGui::PopStyleColor();
    ImGui::SameLine(0, 10.0f);

    if (ImGui::Button("  Export (.bin)  "))
    {
        const std::string folder = FbsBinaryIO::OpenExportFolderDialog();
        if (!folder.empty())
        {
            bool allOk = true;
            for (const auto& bin : m_data.contents)
            {
                if (bin.type == BinType::None) continue;
                std::string outPath = folder + "\\" + bin.name;
                if (!FbsBinaryIO::ExportBin(outPath, bin))
                    allOk = false;
            }
            m_lastExportOk     = allOk && !m_data.contents.empty();
            m_statusMessage    = m_lastExportOk ? "Export (.bin) OK." : "Export (.bin) failed.";
            m_showExportResult = true;
            m_statusTimer      = 3.0f;
        }
    }

    ImGui::SameLine(0, 8.0f);

    if (ImGui::Button("  Export (.tsv)  "))
    {
        if (m_data.selectedIndex >= 0 &&
            m_data.selectedIndex < (int)m_data.contents.size())
        {
            const auto& bin      = m_data.contents[m_data.selectedIndex];
            std::string baseName = bin.name;
            // Replace .bin extension with .tsv for default filename
            if (baseName.size() > 4 &&
                baseName.substr(baseName.size() - 4) == ".bin")
                baseName = baseName.substr(0, baseName.size() - 4) + ".tsv";
            const std::string path = FbsBinaryIO::OpenTsvSaveDialog(baseName);
            if (!path.empty())
            {
                m_lastExportOk     = FbsBinaryIO::ExportTsv(path, bin);
                m_statusMessage    = m_lastExportOk ? "Export (.tsv) OK." : "Export (.tsv) failed.";
                m_showExportResult = true;
                m_statusTimer      = 3.0f;
            }
        }
    }

    ImGui::SameLine(0, 8.0f);

    if (ImGui::Button("  Import (.tsv)  "))
    {
        if (m_data.selectedIndex >= 0 &&
            m_data.selectedIndex < (int)m_data.contents.size())
        {
            const std::string path = FbsBinaryIO::OpenTsvOpenDialog();
            if (!path.empty())
            {
                auto& bin          = m_data.contents[m_data.selectedIndex];
                m_lastExportOk     = FbsBinaryIO::ImportTsv(path, bin);
                m_statusMessage    = m_lastExportOk ? "Import (.tsv) OK." : "Import (.tsv) failed.";
                m_showExportResult = true;
                m_statusTimer      = 3.0f;
            }
        }
    }

    if (m_showExportResult)
    {
        ImGui::SameLine(0, 16.0f);
        if (m_lastExportOk)
        {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.45f, 0.85f, 0.45f, 1.00f));
            ImGui::Text("%s", m_statusMessage.c_str());
        }
        else
        {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.90f, 0.35f, 0.35f, 1.00f));
            ImGui::Text("%s", m_statusMessage.c_str());
        }
        ImGui::PopStyleColor();

        m_statusTimer -= ImGui::GetIO().DeltaTime;
        if (m_statusTimer <= 0.0f) m_showExportResult = false;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Main render entry
// ─────────────────────────────────────────────────────────────────────────────

void FbsDevView::Render()
{
    RenderToolbar();
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4.0f);
    ImGui::Separator();
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 2.0f);

    const float totalH = ImGui::GetContentRegionAvail().y;
    const float totalW = ImGui::GetContentRegionAvail().x;
    const float editorW = totalW - LIST_WIDTH - 1.0f;

    ImGui::BeginChild("##DevEditor", ImVec2(editorW, totalH), false,
                      ImGuiWindowFlags_NoScrollbar);
    RenderEditorArea();
    ImGui::EndChild();

    ImGui::SameLine(0, 0);

    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.20f, 0.20f, 0.28f, 1.00f));
    ImGui::BeginChild("##DevDivider", ImVec2(1.0f, totalH), false);
    ImGui::PopStyleColor();
    ImGui::EndChild();

    ImGui::SameLine(0, 0);

    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.09f, 0.09f, 0.12f, 1.00f));
    ImGui::BeginChild("##DevList", ImVec2(LIST_WIDTH, totalH), false,
                      ImGuiWindowFlags_NoScrollbar);
    ImGui::PopStyleColor();
    RenderContentsList(LIST_WIDTH);
    ImGui::EndChild();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Editor area
// ─────────────────────────────────────────────────────────────────────────────

void FbsDevView::RenderEditorArea()
{
    if (m_data.selectedIndex < 0 ||
        m_data.selectedIndex >= (int)m_data.contents.size())
    {
        const float cw  = ImGui::GetContentRegionAvail().x;
        const float ch  = ImGui::GetContentRegionAvail().y;
        const char* msg = "Import a .bin from the Contents List on the right.";
        ImGui::SetCursorPos(ImVec2((cw - ImGui::CalcTextSize(msg).x) * 0.5f, ch * 0.45f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.36f, 0.36f, 0.48f, 1.00f));
        ImGui::Text("%s", msg);
        ImGui::PopStyleColor();
        return;
    }

    ContentsBinData& bin = m_data.contents[m_data.selectedIndex];
    ImGui::SetCursorPos(ImVec2(10.0f, 6.0f));

    switch (bin.type)
    {
    case BinType::CustomizeItemCommonList:
        RenderCustomizeItemCommonEditor(bin);
        break;
    default:
        ImGui::TextDisabled("No editor available for this bin type.");
        break;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  customize_item_common_list editor (all 26 fields)
// ─────────────────────────────────────────────────────────────────────────────

void FbsDevView::RenderCustomizeItemCommonEditor(ContentsBinData& bin)
{
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.65f, 0.82f, 1.00f, 1.00f));
    ImGui::Text("customize_item_common_list.bin");
    ImGui::PopStyleColor();
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.42f, 0.42f, 0.54f, 1.00f));
    ImGui::Text("(%d entries)", (int)bin.commonEntries.size());
    ImGui::PopStyleColor();

    const float addBtnW = 100.0f;
    ImGui::SameLine(ImGui::GetContentRegionAvail().x - addBtnW + ImGui::GetCursorPosX());
    if (ImGui::Button("+ Add Entry", ImVec2(addBtnW, 0)))
        bin.commonEntries.push_back(CustomizeItemCommonEntry{});

    ImGui::Separator();

    constexpr ImGuiTableFlags tFlags =
        ImGuiTableFlags_ScrollX       |
        ImGuiTableFlags_ScrollY       |
        ImGuiTableFlags_RowBg         |
        ImGuiTableFlags_BordersOuter  |
        ImGuiTableFlags_BordersInnerV |
        ImGuiTableFlags_Resizable     |
        ImGuiTableFlags_Reorderable   |
        ImGuiTableFlags_Hideable      |
        ImGuiTableFlags_SizingFixedFit;

    // 1 control column + 26 field columns = 27
    if (!ImGui::BeginTable("##DevCICLTable", 27, tFlags,
                           ImGui::GetContentRegionAvail()))
        return;

    ImGui::TableSetupScrollFreeze(1, 1);

    // Column widths indexed by schema id (same order as FieldNames::CommonItem)
    static const float k_ColWidths[26] = {
        95.0f,  // id  0
        80.0f,  // id  1
       195.0f,  // id  2
        95.0f,  // id  3
        95.0f,  // id  4
       215.0f,  // id  5
       115.0f,  // id  6
       115.0f,  // id  7
        80.0f,  // id  8
        95.0f,  // id  9
        78.0f,  // id 10
        80.0f,  // id 11
        82.0f,  // id 12
        60.0f,  // id 13
        95.0f,  // id 14
        95.0f,  // id 15
        60.0f,  // id 16
        80.0f,  // id 17
        95.0f,  // id 18
        80.0f,  // id 19
        80.0f,  // id 20
        80.0f,  // id 21
        80.0f,  // id 22
        95.0f,  // id 23
        62.0f,  // id 24
        82.0f,  // id 25
    };
    ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, 52.0f);
    for (int fi = 0; fi < FieldNames::CommonItemCount; ++fi)
        ImGui::TableSetupColumn(FieldNames::CommonItem[fi],
                                ImGuiTableColumnFlags_WidthFixed, k_ColWidths[fi]);
    ImGui::TableHeadersRow();

    int deleteIdx = -1;

    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(bin.commonEntries.size()));
    while (clipper.Step())
    {
        for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i)
        {
            auto& e = bin.commonEntries[i];
            ImGui::TableNextRow();
            ImGui::PushID(i);

            ImGui::TableSetColumnIndex(0);
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.50f, 0.50f, 0.60f, 1.00f));
            ImGui::Text("%d", i + 1);
            ImGui::PopStyleColor();
            ImGui::SameLine(0, 4.0f);
            ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.55f, 0.12f, 0.12f, 1.00f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.78f, 0.18f, 0.18f, 1.00f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.65f, 0.15f, 0.15f, 1.00f));
            if (ImGui::SmallButton("X")) deleteIdx = i;
            ImGui::PopStyleColor(3);

            auto U32Cell = [](const char* id, uint32_t& v) {
                ImGui::SetNextItemWidth(-FLT_MIN);
                ImGui::InputScalar(id, ImGuiDataType_U32, &v);
            };
            auto I32Cell = [](const char* id, int32_t& v) {
                ImGui::SetNextItemWidth(-FLT_MIN);
                ImGui::InputScalar(id, ImGuiDataType_S32, &v);
            };
            auto StrCell = [](const char* id, char* buf, size_t sz) {
                ImGui::SetNextItemWidth(-FLT_MIN);
                ImGui::InputText(id, buf, sz);
            };
            auto BoolCell = [](const char* id, bool& v) {
                ImGui::Checkbox(id, &v);
            };

            ImGui::TableSetColumnIndex(1);  U32Cell("##iid",  e.item_id);
            ImGui::TableSetColumnIndex(2);  I32Cell("##ino",  e.item_no);
            ImGui::TableSetColumnIndex(3);  StrCell("##ic",   e.item_code,      sizeof(e.item_code));
            ImGui::TableSetColumnIndex(4);  U32Cell("##h0",   e.hash_0);
            ImGui::TableSetColumnIndex(5);  U32Cell("##h1",   e.hash_1);
            ImGui::TableSetColumnIndex(6);  StrCell("##tk",   e.text_key,       sizeof(e.text_key));
            ImGui::TableSetColumnIndex(7);  StrCell("##pkid", e.package_id,     sizeof(e.package_id));
            ImGui::TableSetColumnIndex(8);  StrCell("##pksu", e.package_sub_id, sizeof(e.package_sub_id));
            ImGui::TableSetColumnIndex(9);  U32Cell("##u8",   e.unk_8);
            ImGui::TableSetColumnIndex(10); I32Cell("##ssid", e.shop_sort_id);
            ImGui::TableSetColumnIndex(11); BoolCell("##enb", e.is_enabled);
            ImGui::TableSetColumnIndex(12); U32Cell("##u11",  e.unk_11);
            ImGui::TableSetColumnIndex(13); I32Cell("##prc",  e.price);
            ImGui::TableSetColumnIndex(14); BoolCell("##u13", e.unk_13);
            ImGui::TableSetColumnIndex(15); I32Cell("##cno",  e.category_no);
            ImGui::TableSetColumnIndex(16); U32Cell("##h2",   e.hash_2);
            ImGui::TableSetColumnIndex(17); BoolCell("##u16", e.unk_16);
            ImGui::TableSetColumnIndex(18); U32Cell("##u17",  e.unk_17);
            ImGui::TableSetColumnIndex(19); U32Cell("##h3",   e.hash_3);
            ImGui::TableSetColumnIndex(20); U32Cell("##u19",  e.unk_19);
            ImGui::TableSetColumnIndex(21); U32Cell("##u20",  e.unk_20);
            ImGui::TableSetColumnIndex(22); U32Cell("##u21",  e.unk_21);
            ImGui::TableSetColumnIndex(23); U32Cell("##u22",  e.unk_22);
            ImGui::TableSetColumnIndex(24); U32Cell("##h4",   e.hash_4);
            ImGui::TableSetColumnIndex(25); I32Cell("##rar",  e.rarity);
            ImGui::TableSetColumnIndex(26); I32Cell("##sgrp", e.sort_group);

            ImGui::PopID();
        }
    }
    clipper.End();

    if (deleteIdx >= 0)
        bin.commonEntries.erase(bin.commonEntries.begin() + deleteIdx);

    ImGui::EndTable();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Contents list panel  (Import .bin instead of Add)
// ─────────────────────────────────────────────────────────────────────────────

void FbsDevView::RenderContentsList(float listWidth)
{
    ImGui::SetCursorPos(ImVec2(10.0f, 8.0f));

    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.65f, 0.82f, 1.00f, 1.00f));
    ImGui::Text("Contents");
    ImGui::PopStyleColor();

    ImGui::SameLine(listWidth - 100.0f);

    if (ImGui::Button("Import (.bin)"))
    {
        const std::string path = FbsBinaryIO::OpenImportDialog();
        if (!path.empty())
        {
            // Derive bin name from filename
            std::string name = path;
            const size_t sl = name.find_last_of("/\\");
            if (sl != std::string::npos) name = name.substr(sl + 1);

            // Don't add duplicates
            if (!m_data.HasBinByName(name))
            {
                ContentsBinData bin;
                if (FbsBinaryIO::ImportBin(path, bin))
                {
                    m_data.selectedIndex = static_cast<int>(m_data.contents.size());
                    m_data.contents.push_back(std::move(bin));
                }
            }
            else
            {
                // Reload existing bin
                for (auto& b : m_data.contents)
                {
                    if (b.name == name)
                    {
                        ContentsBinData fresh;
                        if (FbsBinaryIO::ImportBin(path, fresh))
                            b = std::move(fresh);
                        break;
                    }
                }
            }
        }
    }

    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4.0f);
    ImGui::Separator();
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4.0f);

    const float availH = ImGui::GetContentRegionAvail().y;
    ImGui::BeginChild("##DevBinList", ImVec2(0.0f, availH), false);

    int removeIdx = -1;

    for (int i = 0; i < (int)m_data.contents.size(); ++i)
    {
        const auto& bin     = m_data.contents[i];
        const bool  selected = (i == m_data.selectedIndex);

        ImGui::PushID(i);

        if (selected)
        {
            ImGui::PushStyleColor(ImGuiCol_Header,        ImVec4(0.22f, 0.40f, 0.72f, 1.00f));
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.28f, 0.48f, 0.82f, 1.00f));
            ImGui::PushStyleColor(ImGuiCol_HeaderActive,  ImVec4(0.25f, 0.44f, 0.78f, 1.00f));
        }

        if (ImGui::Selectable(bin.name.c_str(), selected,
                              ImGuiSelectableFlags_AllowOverlap, ImVec2(0.0f, 32.0f)))
            m_data.selectedIndex = i;

        if (ImGui::BeginPopupContextItem("##DevBinCtx"))
        {
            if (ImGui::MenuItem("Remove")) removeIdx = i;
            ImGui::EndPopup();
        }

        if (selected) ImGui::PopStyleColor(3);
        ImGui::PopID();
    }

    if (removeIdx >= 0)
    {
        m_data.contents.erase(m_data.contents.begin() + removeIdx);
        if (m_data.selectedIndex >= (int)m_data.contents.size())
            m_data.selectedIndex = (int)m_data.contents.size() - 1;
    }

    ImGui::EndChild();
}

#endif // _DEBUG
