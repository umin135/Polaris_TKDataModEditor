// FbsData editor view implementation
#include "fbsdata/editor/FbsDataView.h"
#include "fbsdata/data/FieldNames.h"
#include "fbsdata/data/DefaultValues.h"
#include "fbsdata/editor/ColumnWidths.h"
#include "fbsdata/io/TkmodIO.h"
#include "fbsdata/editor/BinVisibility.h"
#include "FbsDataDict.h"
#include "imgui/imgui.h"
#include <cstring>
#include <cctype>
#include <algorithm>
#define NOMINMAX
#include <windows.h>
#include <commdlg.h>
#pragma comment(lib, "comdlg32.lib")
#include <cstdio>
#include <string>
#include <sstream>
#include <unordered_map>

// -----------------------------------------------------------------------------
//  All fbsdata bin files with support status
// -----------------------------------------------------------------------------

struct BinInfo
{
    const char* filename;
    BinType     type;
    bool        supported;   // true = editor implemented, false = greyed out
    const char* category;    // submenu group label
};

static const BinInfo k_AllBins[] =
{
    // -- Supported ------------------------------------------------------------
    { "arcade_cpu_list.bin",                     BinType::ArcadeCpuList,                  true,  "Supported"     },
    { "area_list.bin",                           BinType::AreaList,                        true,  "Supported"     },
    { "assist_input_list.bin",                   BinType::AssistInputList,                 true,  "Supported"     },
    { "ball_property_list.bin",                  BinType::BallPropertyList,                true,  "Supported"     },
    { "ball_recommend_list.bin",                 BinType::BallRecommendList,               true,  "Supported"     },
    { "ball_setting_list.bin",                   BinType::BallSettingList,                 true,  "Supported"     },
    { "battle_common_list.bin",                  BinType::BattleCommonList,                true,  "Supported"     },
    { "battle_cpu_list.bin",                     BinType::BattleCpuList,                   true,  "Supported"     },
    { "battle_motion_list.bin",                  BinType::BattleMotionList,                true,  "Supported"     },
    { "battle_subtitle_info.bin",                BinType::BattleSubtitleInfoList,          true,  "Supported"     },
    { "body_cylinder_data_list.bin",             BinType::BodyCylinderDataList,            true,  "Supported"     },
    { "character_list.bin",                      BinType::CharacterList,                   true,  "Supported"     },
    { "character_select_list.bin",               BinType::CharacterSelectList,             true,  "Supported"     },
    { "customize_item_common_list.bin",          BinType::CustomizeItemCommonList,         true,  "Supported"     },
    { "customize_item_exclusive_list.bin",       BinType::CustomizeItemExclusiveList,      true,  "Supported"     },
    { "customize_item_prohibit_drama_list.bin",  BinType::CustomizeItemProhibitDramaList,  true,  "Supported"     },
    { "customize_item_unique_list.bin",          BinType::CustomizeItemUniqueList,         true,  "Supported"     },
    { "drama_player_start_list.bin",             BinType::DramaPlayerStartList,            true,  "Supported"     },
    { "fate_drama_player_start_list.bin",        BinType::FateDramaPlayerStartList,        true,  "Supported"     },
    { "jukebox_list.bin",                        BinType::JukeboxList,                     true,  "Supported"     },
    { "rank_list.bin",                           BinType::RankList,                        true,  "Supported"     },
    { "series_list.bin",                         BinType::SeriesList,                      true,  "Supported"     },
    { "stage_list.bin",                          BinType::StageList,                       true,  "Supported"     },
    { "tam_mission_list.bin",                    BinType::TamMissionList,                  true,  "Supported"     },

    // -- Not Supported ---------------------------------------------------------
    { "button_help_list.bin",                    BinType::None,                            false, "Not Supported" },
    { "button_image_list.bin",                   BinType::None,                            false, "Not Supported" },
    { "character_episode_list.bin",              BinType::None,                            false, "Not Supported" },
    { "character_panel_list.bin",                BinType::None,                            false, "Not Supported" },
    { "chat_window_list.bin",                    BinType::None,                            false, "Not Supported" },
    { "common_dialog_details_list.bin",          BinType::None,                            false, "Not Supported" },
    { "cosmos_country_code_list.bin",            BinType::None,                            false, "Not Supported" },
    { "cosmos_language_code_game_list.bin",      BinType::None,                            false, "Not Supported" },
    { "cosmos_language_code_list.bin",           BinType::None,                            false, "Not Supported" },
    { "customize_gauge_list.bin",                BinType::None,                            false, "Not Supported" },
    { "customize_item_acc_parameter_list.bin",   BinType::None,                            false, "Not Supported" },
    { "customize_item_color_palette_list.bin",   BinType::None,                            false, "Not Supported" },
    { "customize_item_color_slot_list.bin",      BinType::None,                            false, "Not Supported" },
    { "customize_item_shop_camera_list.bin",     BinType::None,                            false, "Not Supported" },
    { "customize_model_viewer_list.bin",         BinType::None,                            false, "Not Supported" },
    { "customize_panel_list.bin",                BinType::CustomizePanelList,              true,  "Supported"     },
    { "customize_set_list.bin",                  BinType::None,                            false, "Not Supported" },
    { "customize_shogo_bg_list.bin",             BinType::None,                            false, "Not Supported" },
    { "customize_shogo_list.bin",                BinType::None,                            false, "Not Supported" },
    { "customize_stage_light_list.bin",          BinType::None,                            false, "Not Supported" },
    { "customize_unique_exclusion_list.bin",     BinType::None,                            false, "Not Supported" },
    { "drama_voice_change_list.bin",             BinType::None,                            false, "Not Supported" },
    { "gallery_illust_list.bin",                 BinType::None,                            false, "Not Supported" },
    { "gallery_movie_list.bin",                  BinType::None,                            false, "Not Supported" },
    { "gallery_title_list.bin",                  BinType::None,                            false, "Not Supported" },
    { "game_camera_data_list.bin",               BinType::None,                            false, "Not Supported" },
    { "ghost_vs_ghost_battle_property_list.bin", BinType::None,                            false, "Not Supported" },
    { "ghost_vs_ghost_property_list.bin",        BinType::None,                            false, "Not Supported" },
    { "help_dialog_list.bin",                    BinType::None,                            false, "Not Supported" },
    { "lobby_menu_help_list.bin",                BinType::None,                            false, "Not Supported" },
    { "movie_vibration_list.bin",                BinType::None,                            false, "Not Supported" },
    { "option_settings_list.bin",                BinType::None,                            false, "Not Supported" },
    { "parameter_camera_data_list.bin",          BinType::None,                            false, "Not Supported" },
    { "per_fighter_basic_info_list.bin",         BinType::None,                            false, "Not Supported" },
    { "per_fighter_battle_info_list.bin",        BinType::None,                            false, "Not Supported" },
    { "per_fighter_motion_info_list.bin",        BinType::None,                            false, "Not Supported" },
    { "per_fighter_voice_info_list.bin",         BinType::None,                            false, "Not Supported" },
    { "photo_mode_list.bin",                     BinType::None,                            false, "Not Supported" },
    { "player_profile_stage_light_list.bin",     BinType::None,                            false, "Not Supported" },
    { "playing_stats_table.bin",                 BinType::None,                            false, "Not Supported" },
    { "practice_position_reset_list.bin",        BinType::None,                            false, "Not Supported" },
    { "quake_camera_data_list.bin",              BinType::None,                            false, "Not Supported" },
    { "region_list.bin",                         BinType::None,                            false, "Not Supported" },
    { "replace_text_list.bin",                   BinType::None,                            false, "Not Supported" },
    { "rom_ghost_info_list.bin",                 BinType::None,                            false, "Not Supported" },
    { "rt_drama_player_start_list.bin",          BinType::None,                            false, "Not Supported" },
    { "scene_bgm_list.bin",                      BinType::None,                            false, "Not Supported" },
    { "scene_setting_list.bin",                  BinType::None,                            false, "Not Supported" },
    { "software_keyboard_list.bin",              BinType::None,                            false, "Not Supported" },
    { "sound_parameter_list.bin",                BinType::None,                            false, "Not Supported" },
    { "staffroll_list.bin",                      BinType::None,                            false, "Not Supported" },
    { "store_customize_item_exclusive_list.bin", BinType::None,                            false, "Not Supported" },
    { "store_stage_light_list.bin",              BinType::None,                            false, "Not Supported" },
    { "story_battle_voice_list.bin",             BinType::None,                            false, "Not Supported" },
    { "story_iw_battle_event_list.bin",          BinType::None,                            false, "Not Supported" },
    { "story_settings_info_list.bin",            BinType::None,                            false, "Not Supported" },
    { "subtitle_list.bin",                       BinType::None,                            false, "Not Supported" },
    { "tam_battle_navi_list.bin",                BinType::None,                            false, "Not Supported" },
    { "tam_help_dialog_list.bin",                BinType::None,                            false, "Not Supported" },
    { "tam_message_list.bin",                    BinType::None,                            false, "Not Supported" },
    { "tam_npc_list.bin",                        BinType::None,                            false, "Not Supported" },
    { "tam_tips_command_list.bin",               BinType::None,                            false, "Not Supported" },
    { "tips_list.bin",                           BinType::None,                            false, "Not Supported" },
    { "unlock_list.bin",                         BinType::None,                            false, "Not Supported" },
    { "vibration_pattern_list.bin",              BinType::None,                            false, "Not Supported" },
    { "yellow_book_battle_voice_list.bin",       BinType::None,                            false, "Not Supported" },
    { "yellow_book_settings_list.bin",           BinType::None,                            false, "Not Supported" },
};
static constexpr int k_AllBinsCount = (int)(sizeof(k_AllBins) / sizeof(k_AllBins[0]));

static const float LIST_WIDTH = 290.0f;  // Contents List panel width

// -----------------------------------------------------------------------------
//  Top toolbar
// -----------------------------------------------------------------------------

// Forward declarations (defined after AssembleItemId, further below)
static void FixCommonItemIds(std::vector<CustomizeItemCommonEntry>& entries);
static void FixUniqueItemIds(std::vector<CustomizeItemUniqueEntry>& entries);

static bool IsValidGtbManifestName(const std::string& name)
{
    if (name.size() <= 4) return false;
    if (name.rfind("GTB_", 0) != 0) return false;
    for (char c : name)
    {
        const bool ok = (c >= 'A' && c <= 'Z')
                     || (c >= 'a' && c <= 'z')
                     || (c >= '0' && c <= '9')
                     || c == '_';
        if (!ok) return false;
    }
    return true;
}

void FbsDataView::DoSave()
{
    for (auto& bin : m_data.contents)
    {
        FixCommonItemIds(bin.commonEntries);
        FixUniqueItemIds(bin.customizeItemUniqueEntries);
    }
    if (!m_currentFilePath.empty())
    {
        m_lastSaveOk = TkmodIO::SaveToPath(m_data, m_currentFilePath);
    }
    else
    {
        std::string outPath;
        m_lastSaveOk = TkmodIO::SaveDialog(m_data, outPath);
        if (m_lastSaveOk) m_currentFilePath = outPath;
    }
    m_showSaveResult = true;
    m_statusTimer    = 3.0f;
}

void FbsDataView::DoSaveAs()
{
    for (auto& bin : m_data.contents)
    {
        FixCommonItemIds(bin.commonEntries);
        FixUniqueItemIds(bin.customizeItemUniqueEntries);
    }
    std::string outPath;
    m_lastSaveOk = TkmodIO::SaveDialog(m_data, outPath);
    if (m_lastSaveOk) m_currentFilePath = outPath;
    m_showSaveResult = true;
    m_statusTimer    = 3.0f;
}

void FbsDataView::RenderToolbar()
{
    ImGui::SetCursorPos(ImVec2(10.0f, 8.0f));

    if (ImGui::Button("  New  "))
    {
        m_data            = ModData{};
        m_currentFilePath = {};
        m_modActive       = true;
    }
    ImGui::SameLine(0, 6.0f);

    if (!m_modActive) ImGui::BeginDisabled();
    if (ImGui::Button("  Save  "))
    {
        bool infoEmpty = (m_data.info.author[0]      == '\0' &&
                          m_data.info.description[0]  == '\0' &&
                          m_data.info.version[0]      == '\0');
        if (infoEmpty)
        {
            m_saveConfirmPending = true;
            m_pendingDoSaveAs    = false;
        }
        else
            DoSave();
    }
    ImGui::SameLine(0, 6.0f);

    if (ImGui::Button(" Save As "))
    {
        bool infoEmpty = (m_data.info.author[0]      == '\0' &&
                          m_data.info.description[0]  == '\0' &&
                          m_data.info.version[0]      == '\0');
        if (infoEmpty)
        {
            m_saveConfirmPending = true;
            m_pendingDoSaveAs    = true;
        }
        else
            DoSaveAs();
    }
    if (!m_modActive) ImGui::EndDisabled();
    ImGui::SameLine(0, 6.0f);

    if (ImGui::Button("  Load  "))
    {
        ModData loaded;
        std::string outPath;
        if (TkmodIO::LoadDialog(loaded, outPath))
        {
            m_data            = std::move(loaded);
            m_currentFilePath = outPath;
            m_modActive       = true;
        }
    }

    // Transient status message
    if (m_showSaveResult)
    {
        ImGui::SameLine(0, 16.0f);
        if (m_lastSaveOk)
        {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.45f, 0.85f, 0.45f, 1.00f));
            ImGui::Text("Saved successfully.");
        }
        else
        {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.90f, 0.35f, 0.35f, 1.00f));
            ImGui::Text("Save failed.");
        }
        ImGui::PopStyleColor();
        m_statusTimer -= ImGui::GetIO().DeltaTime;
        if (m_statusTimer <= 0.0f) m_showSaveResult = false;
    }

    // Right-aligned: Manage tkmods + Information Edit (only when a mod is active)
    if (m_modActive)
    {
        const float manageBtnW = 130.0f;
        const float infoBtnW   = 160.0f;
        const float manageX = ImGui::GetWindowWidth() - manageBtnW - infoBtnW - 18.0f;
        if (manageX > ImGui::GetCursorPosX())
        {
            ImGui::SameLine(manageX);
            if (ImGui::Button("tkmod Overview", ImVec2(manageBtnW, 0.f)))
                m_managerView.Open();
        }
        const float infoX = ImGui::GetWindowWidth() - infoBtnW - 10.0f;
        if (infoX > ImGui::GetCursorPosX())
        {
            ImGui::SameLine(infoX);
            if (ImGui::Button("Information Edit", ImVec2(infoBtnW, 0.f)))
                m_infoEditPending = true;
        }
    }
}

// -----------------------------------------------------------------------------
//  Main render entry
// -----------------------------------------------------------------------------

bool FbsDataView::LoadFromPath(const std::string& path)
{
    ModData loaded;
    if (!TkmodIO::LoadFromPath(path, loaded))
        return false;
    m_data            = std::move(loaded);
    m_currentFilePath = path;
    m_modActive       = true;
    return true;
}

// -----------------------------------------------------------------------------

void FbsDataView::Render()
{
    // Reserve toolbar height
    RenderToolbar();
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4.0f);
    ImGui::Separator();
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 2.0f);

    const float totalH = ImGui::GetContentRegionAvail().y;
    const float totalW = ImGui::GetContentRegionAvail().x;
    const float editorW = totalW - LIST_WIDTH - 1.0f;

    if (!m_modActive) ImGui::BeginDisabled();

    // -- Editor area (left) --
    ImGui::BeginChild("##FbsEditor", ImVec2(editorW, totalH), false,
                      ImGuiWindowFlags_NoScrollbar);
    RenderEditorArea();
    ImGui::EndChild();

    ImGui::SameLine(0, 0);

    // 1px vertical divider
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.20f, 0.20f, 0.28f, 1.00f));
    ImGui::BeginChild("##FbsDivider", ImVec2(1.0f, totalH), false);
    ImGui::PopStyleColor();
    ImGui::EndChild();

    ImGui::SameLine(0, 0);

    // -- Contents list (right) --
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.09f, 0.09f, 0.12f, 1.00f));
    ImGui::BeginChild("##FbsList", ImVec2(LIST_WIDTH, totalH), false,
                      ImGuiWindowFlags_NoScrollbar);
    ImGui::PopStyleColor();
    RenderContentsList(LIST_WIDTH);
    ImGui::EndChild();

    if (!m_modActive) ImGui::EndDisabled();

    // Popups at the top-level window context (not inside child windows)
    RenderInfoEditPopup();
    RenderSaveConfirmPopup();

    // Manage tkmods floating window
    m_managerView.Render(*this, [this](const std::string& path) {
        ModData loaded;
        if (TkmodIO::LoadFromPath(path, loaded))
        {
            m_data            = std::move(loaded);
            m_currentFilePath = path;
            m_modActive       = true;
        }
    });
}

// -----------------------------------------------------------------------------
//  Editor area
// -----------------------------------------------------------------------------

void FbsDataView::RenderEditorArea()
{
    if (m_data.selectedIndex < 0 ||
        m_data.selectedIndex >= (int)m_data.contents.size())
    {
        // Nothing selected -- show hint
        const float cw = ImGui::GetContentRegionAvail().x;
        const float ch = ImGui::GetContentRegionAvail().y;
        const char* hint = "Add a bin from the Contents List on the right.";
        ImGui::SetCursorPos(ImVec2((cw - ImGui::CalcTextSize(hint).x) * 0.5f,
                                   ch * 0.45f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.36f, 0.36f, 0.48f, 1.00f));
        ImGui::Text("%s", hint);
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
    case BinType::CharacterList:
        RenderCharacterListEditor(bin);
        break;
    case BinType::CustomizeItemExclusiveList:
        RenderCustomizeItemExclusiveListEditor(bin);
        break;
    case BinType::AreaList:
        RenderAreaListEditor(bin);
        break;
    case BinType::BattleSubtitleInfoList:
        RenderBattleSubtitleInfoEditor(bin);
        break;
    case BinType::FateDramaPlayerStartList:
        RenderFateDramaPlayerStartListEditor(bin);
        break;
    case BinType::JukeboxList:
        RenderJukeboxListEditor(bin);
        break;
    case BinType::SeriesList:
        RenderSeriesListEditor(bin);
        break;
    case BinType::TamMissionList:
        RenderTamMissionListEditor(bin);
        break;
    case BinType::DramaPlayerStartList:
        RenderDramaPlayerStartListEditor(bin);
        break;
    case BinType::StageList:
        RenderStageListEditor(bin);
        break;
    case BinType::BallPropertyList:
        RenderBallPropertyListEditor(bin);
        break;
    case BinType::BodyCylinderDataList:
        RenderBodyCylinderDataListEditor(bin);
        break;
    case BinType::CustomizeItemUniqueList:
        RenderCustomizeItemUniqueListEditor(bin);
        break;
    case BinType::CharacterSelectList:
        RenderCharacterSelectListEditor(bin);
        break;
    case BinType::CustomizeItemProhibitDramaList:
        RenderCustomizeItemProhibitDramaListEditor(bin);
        break;
    case BinType::BattleMotionList:
        RenderBattleMotionListEditor(bin);
        break;
    case BinType::ArcadeCpuList:
        RenderArcadeCpuListEditor(bin);
        break;
    case BinType::BallRecommendList:
        RenderBallRecommendListEditor(bin);
        break;
    case BinType::BallSettingList:
        RenderBallSettingListEditor(bin);
        break;
    case BinType::BattleCommonList:
        RenderBattleCommonListEditor(bin);
        break;
    case BinType::BattleCpuList:
        RenderBattleCpuListEditor(bin);
        break;
    case BinType::RankList:
        RenderRankListEditor(bin);
        break;
    case BinType::AssistInputList:
        RenderAssistInputListEditor(bin);
        break;
    case BinType::CustomizePanelList:
        RenderCustomizePanelListEditor(bin);
        break;
    case BinType::CustomizeItemException:
        RenderCustomizeItemExceptionEditor(bin);
        break;
    default:
        ImGui::TextDisabled("No editor available for this bin type.");
        break;
    }

}

// -----------------------------------------------------------------------------
//  customize_item_common_list TSV export / import helpers
// -----------------------------------------------------------------------------

static std::string OpenTsvSaveDialog(const wchar_t* defaultName)
{
    wchar_t szFile[1024] = {};
    wcscpy_s(szFile, defaultName);
    OPENFILENAMEW ofn    = {};
    ofn.lStructSize  = sizeof(ofn);
    ofn.lpstrFile    = szFile;
    ofn.nMaxFile     = (DWORD)std::size(szFile);
    ofn.lpstrFilter  = L"Tab-Separated Values\0*.tsv\0All Files\0*.*\0";
    ofn.lpstrDefExt  = L"tsv";
    ofn.Flags        = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
    if (!GetSaveFileNameW(&ofn)) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, szFile, -1, nullptr, 0, nullptr, nullptr);
    std::string out(n - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, szFile, -1, &out[0], n, nullptr, nullptr);
    return out;
}

static std::string OpenTsvOpenDialog()
{
    wchar_t szFile[1024] = {};
    OPENFILENAMEW ofn    = {};
    ofn.lStructSize  = sizeof(ofn);
    ofn.lpstrFile    = szFile;
    ofn.nMaxFile     = (DWORD)std::size(szFile);
    ofn.lpstrFilter  = L"Tab-Separated Values\0*.tsv\0All Files\0*.*\0";
    ofn.Flags        = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    if (!GetOpenFileNameW(&ofn)) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, szFile, -1, nullptr, 0, nullptr, nullptr);
    std::string out(n - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, szFile, -1, &out[0], n, nullptr, nullptr);
    return out;
}

static void ExportCommonListTsv(const std::vector<CustomizeItemCommonEntry>& entries,
                                const std::string& path)
{
    FILE* f = nullptr;
    fopen_s(&f, path.c_str(), "wb");
    if (!f) return;

    for (const auto& e : entries)
    {
        char line[2048];
        int n = snprintf(line, sizeof(line),
            "%u\t%d\t%s\t%u\t%u\t%s\t%s\t%s\t%u\t%d\t%s\t%u\t%d\t%s\t%d\t%u\t%s\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%d\t%d\n",
            e.item_id, e.item_no, e.item_code,
            e.hash_0, e.hash_1, e.text_key, e.package_id, e.package_sub_id,
            e.unk_8, e.shop_sort_id,
            e.is_enabled ? "TRUE" : "FALSE",
            e.unk_11, e.price,
            e.unk_13 ? "TRUE" : "FALSE",
            e.category_no, e.hash_2,
            e.unk_16 ? "TRUE" : "FALSE",
            e.unk_17, e.hash_3,
            e.unk_19, e.unk_20, e.unk_21, e.unk_22,
            e.hash_4, e.rarity, e.sort_group);
        fwrite(line, 1, n, f);
    }
    fclose(f);
}

static bool ParseBool(const char* s) { return _stricmp(s, "true") == 0 || strcmp(s, "1") == 0; }

static std::vector<CustomizeItemCommonEntry> ImportCommonListTsv(const std::string& path)
{
    std::vector<CustomizeItemCommonEntry> result;
    FILE* f = nullptr;
    fopen_s(&f, path.c_str(), "rb");
    if (!f) return result;

    char line[2048];
    while (fgets(line, sizeof(line), f))
    {
        // strip \r\n
        int len = (int)strlen(line);
        while (len > 0 && (line[len-1] == '\r' || line[len-1] == '\n')) line[--len] = '\0';
        if (len == 0) continue;

        // split by tab
        char* cols[26] = {};
        int col = 0;
        char* p = line;
        cols[col++] = p;
        for (; *p && col < 26; ++p)
            if (*p == '\t') { *p = '\0'; cols[col++] = p + 1; }
        if (col < 26) continue; // skip malformed rows

        CustomizeItemCommonEntry e;
        e.item_id      = (uint32_t)strtoul(cols[0],  nullptr, 10);
        e.item_no      = (int32_t)strtol (cols[1],  nullptr, 10);
        strncpy_s(e.item_code,      cols[2],  _TRUNCATE);
        e.hash_0       = (uint32_t)strtoul(cols[3],  nullptr, 10);
        e.hash_1       = (uint32_t)strtoul(cols[4],  nullptr, 10);
        strncpy_s(e.text_key,       cols[5],  _TRUNCATE);
        strncpy_s(e.package_id,     cols[6],  _TRUNCATE);
        strncpy_s(e.package_sub_id, cols[7],  _TRUNCATE);
        e.unk_8        = (uint32_t)strtoul(cols[8],  nullptr, 10);
        e.shop_sort_id = (int32_t)strtol (cols[9],  nullptr, 10);
        e.is_enabled   = ParseBool(cols[10]);
        e.unk_11       = (uint32_t)strtoul(cols[11], nullptr, 10);
        e.price        = (int32_t)strtol (cols[12], nullptr, 10);
        e.unk_13       = ParseBool(cols[13]);
        e.category_no  = (int32_t)strtol (cols[14], nullptr, 10);
        e.hash_2       = (uint32_t)strtoul(cols[15], nullptr, 10);
        e.unk_16       = ParseBool(cols[16]);
        e.unk_17       = (uint32_t)strtoul(cols[17], nullptr, 10);
        e.hash_3       = (uint32_t)strtoul(cols[18], nullptr, 10);
        e.unk_19       = (uint32_t)strtoul(cols[19], nullptr, 10);
        e.unk_20       = (uint32_t)strtoul(cols[20], nullptr, 10);
        e.unk_21       = (uint32_t)strtoul(cols[21], nullptr, 10);
        e.unk_22       = (uint32_t)strtoul(cols[22], nullptr, 10);
        e.hash_4       = (uint32_t)strtoul(cols[23], nullptr, 10);
        e.rarity       = (int32_t)strtol (cols[24], nullptr, 10);
        e.sort_group   = (int32_t)strtol (cols[25], nullptr, 10);
        result.push_back(e);
    }
    fclose(f);
    return result;
}

// Lazily-built sorted (hash, "CODE: Name") lists for Char_hash and ItemPos_hash combos.
// Rebuilt automatically whenever FbsDataDict is reloaded (load count changes).
static const std::vector<std::pair<uint32_t, std::string>>& GetCharHashItems()
{
    static std::vector<std::pair<uint32_t, std::string>> s_items;
    static uint32_t s_loadCount = UINT32_MAX;
    uint32_t cur = FbsDataDict::Get().LoadCount();
    if (s_loadCount != cur) {
        s_items.clear();
        s_loadCount = cur;
        for (auto& kv : FbsDataDict::Get().GetCharHashCodeMap()) {
            uint32_t hash = kv.first;
            const std::string& code = kv.second; // uppercase code, e.g. "GRF"
            uint32_t charId = FbsDataDict::Get().CharHashToId(hash);
            const char* name = (charId != UINT32_MAX) ? FbsDataDict::Get().CharName(charId) : nullptr;
            std::string display = name ? (code + ": " + name) : code;
            s_items.push_back(std::make_pair(hash, std::move(display)));
        }
        std::sort(s_items.begin(), s_items.end(),
            [](const std::pair<uint32_t,std::string>& a,
               const std::pair<uint32_t,std::string>& b){ return a.second < b.second; });
    }
    return s_items;
}

static const std::vector<std::pair<uint32_t, std::string>>& GetTypeHashItems()
{
    static std::vector<std::pair<uint32_t, std::string>> s_items;
    static uint32_t s_loadCount = UINT32_MAX;
    uint32_t cur = FbsDataDict::Get().LoadCount();
    if (s_loadCount != cur) {
        s_items.clear();
        s_loadCount = cur;
        for (auto& kv : FbsDataDict::Get().GetTypeHashCodeMap()) {
            uint32_t hash = kv.first;
            const std::string& code = kv.second; // e.g. "hed"
            uint32_t typeId = FbsDataDict::Get().TypeHashToId(hash);
            std::string cleanName;
            if (typeId != UINT32_MAX) {
                const char* fullName = FbsDataDict::Get().TypeName(typeId);
                if (fullName) {
                    cleanName = fullName; // e.g. "Head (hed)"
                    size_t p = cleanName.rfind(" (");
                    if (p != std::string::npos) cleanName.resize(p); // → "Head"
                }
            }
            std::string display = cleanName.empty() ? code : (code + ": " + cleanName);
            s_items.push_back(std::make_pair(hash, std::move(display)));
        }
        std::sort(s_items.begin(), s_items.end(),
            [](const std::pair<uint32_t,std::string>& a,
               const std::pair<uint32_t,std::string>& b){ return a.second < b.second; });
    }
    return s_items;
}

// Assembles item_id from components.
// BB/CC are looked up from hash; falls back to existing id's BB/CC if hash is unknown.
static uint32_t AssembleItemId(uint8_t a, uint32_t charHash, uint32_t typeHash,
                               uint32_t ddd, uint32_t existingId)
{
    uint32_t BB = FbsDataDict::Get().CharHashToId(charHash);
    uint32_t CC = FbsDataDict::Get().TypeHashToId(typeHash);
    if (BB == UINT32_MAX) BB = (existingId / 100000u) % 100u;
    if (CC == UINT32_MAX) CC = (existingId /   1000u) % 100u;
    return (uint32_t)a * 10000000u + BB * 100000u + CC * 1000u + (ddd % 1000u);
}

// Rebuilds item_id / char_item_id for all entries based on their hash fields.
// Only updates entries where the hash-derived BB/CC differ from the stored value.
// Skips entries whose hashes are not found in FbsDataDict (leaves them unchanged).
static void FixCommonItemIds(std::vector<CustomizeItemCommonEntry>& entries)
{
    for (auto& e : entries)
    {
        uint32_t BB = FbsDataDict::Get().CharHashToId(e.hash_0);
        uint32_t CC = FbsDataDict::Get().TypeHashToId(e.hash_1);
        if (BB == UINT32_MAX && CC == UINT32_MAX) continue;
        uint32_t fixed = AssembleItemId(2, e.hash_0, e.hash_1, e.item_id % 1000u, e.item_id);
        e.item_id = fixed;
    }
}

static void FixUniqueItemIds(std::vector<CustomizeItemUniqueEntry>& entries)
{
    for (auto& e : entries)
    {
        uint32_t BB = FbsDataDict::Get().CharHashToId(e.character_hash);
        uint32_t CC = FbsDataDict::Get().TypeHashToId(e.hash_1);
        if (BB == UINT32_MAX && CC == UINT32_MAX) continue;
        uint32_t fixed = AssembleItemId(1, e.character_hash, e.hash_1, e.char_item_id % 1000u, e.char_item_id);
        e.char_item_id = fixed;
    }
}

// Renders a combo cell for a hash field with known code labels.
// Returns true if the value changed.
static bool HashComboCell(const char* id, uint32_t& val,
    const std::unordered_map<uint32_t, std::string>& revMap,
    const std::vector<std::pair<uint32_t, std::string>>& items)
{
    // Look up display string from items first (has full "CODE: Name" format).
    const std::string* displayStr = nullptr;
    for (size_t i = 0; i < items.size(); ++i)
        if (items[i].first == val) { displayStr = &items[i].second; break; }

    char preview[64];
    if (displayStr)
        snprintf(preview, sizeof(preview), "%s", displayStr->c_str());
    else {
        auto it = revMap.find(val);
        if (it != revMap.end())
            snprintf(preview, sizeof(preview), "%s", it->second.c_str());
        else
            snprintf(preview, sizeof(preview), "%u", val);
    }

    bool changed = false;
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::BeginCombo(id, preview, ImGuiComboFlags_HeightLargest))
    {
        for (size_t i = 0; i < items.size(); ++i)
        {
            bool sel = (val == items[i].first);
            if (ImGui::Selectable(items[i].second.c_str(), sel))
            {
                val = items[i].first;
                changed = true;
            }
            if (sel) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    return changed;
}

static void ExportUniqueListTsv(const std::vector<CustomizeItemUniqueEntry>& entries,
                                const std::string& path)
{
    FILE* f = nullptr;
    fopen_s(&f, path.c_str(), "wb");
    if (!f) return;

    for (const auto& e : entries)
    {
        char line[2048];
        int n = snprintf(line, sizeof(line),
            "%u\t%s\t%u\t%u\t%s\t%s\t%s\t%u\t%u\t%s\t%u\t%u\t%u\t%u\t%u\t%s\t%u\t%u\t%u\t%u\t%u\t%u\n",
            e.char_item_id, e.asset_name,
            e.character_hash, e.hash_1,
            e.text_key, e.extra_text_key_1, e.extra_text_key_2,
            e.flag_7,
            e.unk_8,
            e.flag_9   ? "TRUE" : "FALSE",
            e.unk_10, e.price, e.unk_12, e.unk_13, e.hash_2,
            e.flag_15  ? "TRUE" : "FALSE",
            e.unk_16, e.hash_3,
            e.unk_18, e.unk_19, e.unk_20, e.unk_21);
        fwrite(line, 1, n, f);
    }
    fclose(f);
}

static std::vector<CustomizeItemUniqueEntry> ImportUniqueListTsv(const std::string& path)
{
    std::vector<CustomizeItemUniqueEntry> result;
    FILE* f = nullptr;
    fopen_s(&f, path.c_str(), "rb");
    if (!f) return result;

    char line[2048];
    while (fgets(line, sizeof(line), f))
    {
        int len = (int)strlen(line);
        while (len > 0 && (line[len-1] == '\r' || line[len-1] == '\n')) line[--len] = '\0';
        if (len == 0) continue;

        char* cols[22] = {};
        int col = 0;
        char* p = line;
        cols[col++] = p;
        for (; *p && col < 22; ++p)
            if (*p == '\t') { *p = '\0'; cols[col++] = p + 1; }
        if (col < 22) continue;

        CustomizeItemUniqueEntry e;
        e.char_item_id   = (uint32_t)strtoul(cols[ 0], nullptr, 10);
        strncpy_s(e.asset_name,       cols[ 1], _TRUNCATE);
        e.character_hash = (uint32_t)strtoul(cols[ 2], nullptr, 10);
        e.hash_1         = (uint32_t)strtoul(cols[ 3], nullptr, 10);
        strncpy_s(e.text_key,         cols[ 4], _TRUNCATE);
        strncpy_s(e.extra_text_key_1, cols[ 5], _TRUNCATE);
        strncpy_s(e.extra_text_key_2, cols[ 6], _TRUNCATE);
        e.flag_7         = (uint32_t)strtoul(cols[ 7], nullptr, 10);
        e.unk_8          = (uint32_t)strtoul(cols[ 8], nullptr, 10);
        e.flag_9         = ParseBool(cols[ 9]);
        e.unk_10         = (uint32_t)strtoul(cols[10], nullptr, 10);
        e.price          = (uint32_t)strtoul(cols[11], nullptr, 10);
        e.unk_12         = (uint32_t)strtoul(cols[12], nullptr, 10);
        e.unk_13         = (uint32_t)strtoul(cols[13], nullptr, 10);
        e.hash_2         = (uint32_t)strtoul(cols[14], nullptr, 10);
        e.flag_15        = ParseBool(cols[15]);
        e.unk_16         = (uint32_t)strtoul(cols[16], nullptr, 10);
        e.hash_3         = (uint32_t)strtoul(cols[17], nullptr, 10);
        e.unk_18         = (uint32_t)strtoul(cols[18], nullptr, 10);
        e.unk_19         = (uint32_t)strtoul(cols[19], nullptr, 10);
        e.unk_20         = (uint32_t)strtoul(cols[20], nullptr, 10);
        e.unk_21         = (uint32_t)strtoul(cols[21], nullptr, 10);
        result.push_back(e);
    }
    fclose(f);
    return result;
}

// -----------------------------------------------------------------------------
//  customize_item_common_list table editor
// -----------------------------------------------------------------------------

void FbsDataView::RenderCustomizeItemCommonEditor(ContentsBinData& bin)
{
    // -- Header row --
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.65f, 0.82f, 1.00f, 1.00f));
    ImGui::Text("customize_item_common_list.bin");
    ImGui::PopStyleColor();
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.42f, 0.42f, 0.54f, 1.00f));
    ImGui::Text("(%d entries)", (int)bin.commonEntries.size());
    ImGui::PopStyleColor();

    // -- Export / Import / Add Entry buttons (right-aligned) --
    const float addBtnW    = 100.0f;
    const float ioGap      = 4.0f;
    const float exportBtnW = 70.0f;
    const float importBtnW = 70.0f;
    const float totalW     = exportBtnW + ioGap + importBtnW + ioGap + addBtnW;
    ImGui::SameLine(ImGui::GetContentRegionAvail().x - totalW + ImGui::GetCursorPosX());

    if (ImGui::Button("Export", ImVec2(exportBtnW, 0)))
    {
        std::string path = OpenTsvSaveDialog(L"customize_item_common_list.tsv");
        if (!path.empty())
        {
            FixCommonItemIds(bin.commonEntries);
            ExportCommonListTsv(bin.commonEntries, path);
        }
    }
    ImGui::SameLine(0, ioGap);
    if (!m_renderReadOnly && ImGui::Button("Import", ImVec2(importBtnW, 0)))
    {
        std::string path = OpenTsvOpenDialog();
        if (!path.empty())
        {
            auto imported = ImportCommonListTsv(path);
            if (!imported.empty())
            {
                FixCommonItemIds(imported);
                bin.commonEntries = std::move(imported);
            }
        }
    }
    ImGui::SameLine(0, ioGap);
    if (!m_renderReadOnly && ImGui::Button("+ Add Entry", ImVec2(addBtnW, 0)))
        bin.commonEntries.push_back(bin.commonEntries.empty()
            ? DefaultValues::CommonEntry()
            : bin.commonEntries.back());

    ImGui::Separator();

    // -- Table --
    constexpr ImGuiTableFlags tFlags =
        ImGuiTableFlags_ScrollX          |
        ImGuiTableFlags_ScrollY          |
        ImGuiTableFlags_RowBg            |
        ImGuiTableFlags_BordersOuter     |
        ImGuiTableFlags_BordersInnerV    |
        ImGuiTableFlags_Resizable        |
        ImGuiTableFlags_Reorderable      |
        ImGuiTableFlags_Hideable         |
        ImGuiTableFlags_SizingFixedFit;

    // Column count: 1 (row/delete) + 26 fields = 27
    if (!ImGui::BeginTable("##CICLTable", 27, tFlags,
                           ImGui::GetContentRegionAvail()))
        return;

    // Freeze first column (row controls) and header row
    ImGui::TableSetupScrollFreeze(1, 1);

    // All 26 schema fields (id 0..25)
    static const int k_ColIds[] = {
         0,  1,  2,  3,  4,  5,  6,  7,  8,  9,
        10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
        20, 21, 22, 23, 24, 25,
    };
    constexpr int k_ColCount = (int)(sizeof(k_ColIds) / sizeof(k_ColIds[0]));
    ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, ColumnWidths::kRowCtrl);
    for (int ci = 0; ci < k_ColCount; ++ci)
        ImGui::TableSetupColumn(FieldNames::CommonItem[k_ColIds[ci]],
                                ImGuiTableColumnFlags_WidthFixed, ColumnWidths::kCommon[k_ColIds[ci]]);
    ImGui::TableHeadersRow();

    // Build duplicate-id detection map (hash → count)
    std::unordered_map<uint32_t, int> idCounts;
    for (int i = 0; i < (int)bin.commonEntries.size(); ++i)
        idCounts[bin.commonEntries[i].item_id]++;

    int deleteIdx = -1;

    for (int i = 0; i < (int)bin.commonEntries.size(); ++i)
    {
        auto& e = bin.commonEntries[i];
        ImGui::TableNextRow();
        ImGui::PushID(i);

        // -- # column: row number + delete button --
        ImGui::TableSetColumnIndex(0);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.50f, 0.50f, 0.60f, 1.00f));
        ImGui::Text("%d", i + 1);
        ImGui::PopStyleColor();
        ImGui::SameLine(0, 4.0f);
        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.55f, 0.12f, 0.12f, 1.00f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.78f, 0.18f, 0.18f, 1.00f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.65f, 0.15f, 0.15f, 1.00f));
        if (!m_renderReadOnly && ImGui::SmallButton("X")) deleteIdx = i;
        ImGui::PopStyleColor(3);

        // Inline helpers ---- fill the entire column width
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

        ImGui::TableSetColumnIndex(1);
        {
            int ddd = (int)(e.item_id % 1000u);
            bool isDup  = idCounts.count(e.item_id) && idCounts.at(e.item_id) > 1;
            bool isGame = FbsDataDict::Get().IsGameItemId(e.item_id);
            bool warn   = isDup || isGame;
            if (warn) ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.55f, 0.08f, 0.08f, 1.0f));
            ImGui::SetNextItemWidth(-FLT_MIN);
            if (ImGui::InputInt("##ddd", &ddd, 0, 0)) {
                ddd = std::max(0, std::min(999, ddd));
                e.item_id = AssembleItemId(2, e.hash_0, e.hash_1, (uint32_t)ddd, e.item_id);
            }
            if (warn) ImGui::PopStyleColor();
            if (warn && ImGui::IsItemHovered()) {
                if (isDup && isGame)
                    ImGui::SetTooltip("Duplicate item_id: %u\nConflicts with a base game item ID", e.item_id);
                else if (isDup)
                    ImGui::SetTooltip("Duplicate item_id: %u", e.item_id);
                else
                    ImGui::SetTooltip("item_id %u conflicts with a base game item ID", e.item_id);
            }
        }
        ImGui::TableSetColumnIndex(2);  I32Cell("##ino",   e.item_no);
        ImGui::TableSetColumnIndex(3);  StrCell("##icode", e.item_code,      sizeof(e.item_code));
        ImGui::TableSetColumnIndex(4);
        if (HashComboCell("##h0", e.hash_0, FbsDataDict::Get().GetCharHashCodeMap(), GetCharHashItems()))
            e.item_id = AssembleItemId(2, e.hash_0, e.hash_1, e.item_id % 1000u, e.item_id);
        ImGui::TableSetColumnIndex(5);
        if (HashComboCell("##h1", e.hash_1, FbsDataDict::Get().GetTypeHashCodeMap(), GetTypeHashItems()))
            e.item_id = AssembleItemId(2, e.hash_0, e.hash_1, e.item_id % 1000u, e.item_id);
        ImGui::TableSetColumnIndex(6);  StrCell("##tkey",  e.text_key,       sizeof(e.text_key));
        ImGui::TableSetColumnIndex(7);  StrCell("##pkid",  e.package_id,     sizeof(e.package_id));
        ImGui::TableSetColumnIndex(8);  StrCell("##pksu",  e.package_sub_id, sizeof(e.package_sub_id));
        ImGui::TableSetColumnIndex(9);  U32Cell("##u8",    e.unk_8);
        ImGui::TableSetColumnIndex(10); I32Cell("##ssid",  e.shop_sort_id);
        ImGui::TableSetColumnIndex(11); BoolCell("##enb",  e.is_enabled);
        ImGui::TableSetColumnIndex(12); U32Cell("##u11",   e.unk_11);
        ImGui::TableSetColumnIndex(13); I32Cell("##prc",   e.price);
        ImGui::TableSetColumnIndex(14); BoolCell("##u13",  e.unk_13);
        ImGui::TableSetColumnIndex(15); I32Cell("##cno",   e.category_no);
        ImGui::TableSetColumnIndex(16); U32Cell("##h2",    e.hash_2);
        ImGui::TableSetColumnIndex(17); BoolCell("##u16",  e.unk_16);
        ImGui::TableSetColumnIndex(18); U32Cell("##u17",   e.unk_17);
        ImGui::TableSetColumnIndex(19); U32Cell("##h3",    e.hash_3);
        ImGui::TableSetColumnIndex(20); U32Cell("##u19",   e.unk_19);
        ImGui::TableSetColumnIndex(21); U32Cell("##u20",   e.unk_20);
        ImGui::TableSetColumnIndex(22); U32Cell("##u21",   e.unk_21);
        ImGui::TableSetColumnIndex(23); U32Cell("##u22",   e.unk_22);
        ImGui::TableSetColumnIndex(24); U32Cell("##h4",    e.hash_4);
        ImGui::TableSetColumnIndex(25); I32Cell("##rar",   e.rarity);
        ImGui::TableSetColumnIndex(26); I32Cell("##sgrp",  e.sort_group);

        ImGui::PopID();
    }

    if (!m_renderReadOnly && deleteIdx >= 0)
        bin.commonEntries.erase(bin.commonEntries.begin() + deleteIdx);

    ImGui::EndTable();
}

// -----------------------------------------------------------------------------
//  character_list TSV export / import
// -----------------------------------------------------------------------------

static void ExportCharacterListTsv(const std::vector<CharacterEntry>& entries, const std::string& path)
{
    FILE* f = nullptr;
    fopen_s(&f, path.c_str(), "wb");
    if (!f) return;
    for (const auto& e : entries)
    {
        char line[2048];
        int n = snprintf(line, sizeof(line),
            "%s\t%u\t%s\t%s\t%s\t%.9g\t%s\t%u\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n",
            e.character_code, e.name_hash,
            e.is_enabled    ? "TRUE" : "FALSE",
            e.is_selectable ? "TRUE" : "FALSE",
            e.group, e.camera_offset,
            e.is_playable   ? "TRUE" : "FALSE",
            e.sort_order,
            e.full_name_key, e.short_name_jp_key, e.short_name_key,
            e.origin_key, e.fighting_style_key, e.height_key, e.weight_key);
        fwrite(line, 1, n, f);
    }
    fclose(f);
}

static std::vector<CharacterEntry> ImportCharacterListTsv(const std::string& path)
{
    std::vector<CharacterEntry> result;
    FILE* f = nullptr;
    fopen_s(&f, path.c_str(), "rb");
    if (!f) return result;
    char line[2048];
    while (fgets(line, sizeof(line), f))
    {
        int len = (int)strlen(line);
        while (len > 0 && (line[len-1] == '\r' || line[len-1] == '\n')) line[--len] = '\0';
        if (len == 0) continue;
        char* cols[15] = {};
        int col = 0; char* p = line; cols[col++] = p;
        for (; *p && col < 15; ++p) if (*p == '\t') { *p = '\0'; cols[col++] = p + 1; }
        if (col < 15) continue;
        CharacterEntry e;
        strncpy_s(e.character_code,      cols[ 0], _TRUNCATE);
        e.name_hash      = (uint32_t)strtoul(cols[ 1], nullptr, 10);
        e.is_enabled     = ParseBool(cols[ 2]);
        e.is_selectable  = ParseBool(cols[ 3]);
        strncpy_s(e.group,               cols[ 4], _TRUNCATE);
        e.camera_offset  = strtof(cols[ 5], nullptr);
        e.is_playable    = ParseBool(cols[ 6]);
        e.sort_order     = (uint32_t)strtoul(cols[ 7], nullptr, 10);
        strncpy_s(e.full_name_key,       cols[ 8], _TRUNCATE);
        strncpy_s(e.short_name_jp_key,   cols[ 9], _TRUNCATE);
        strncpy_s(e.short_name_key,      cols[10], _TRUNCATE);
        strncpy_s(e.origin_key,          cols[11], _TRUNCATE);
        strncpy_s(e.fighting_style_key,  cols[12], _TRUNCATE);
        strncpy_s(e.height_key,          cols[13], _TRUNCATE);
        strncpy_s(e.weight_key,          cols[14], _TRUNCATE);
        result.push_back(e);
    }
    fclose(f);
    return result;
}

// -----------------------------------------------------------------------------
//  character_list table editor
// -----------------------------------------------------------------------------

void FbsDataView::RenderCharacterListEditor(ContentsBinData& bin)
{
    // -- Header row --
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.65f, 0.82f, 1.00f, 1.00f));
    ImGui::Text("character_list.bin");
    ImGui::PopStyleColor();
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.42f, 0.42f, 0.54f, 1.00f));
    ImGui::Text("(%d entries)", (int)bin.characterEntries.size());
    ImGui::PopStyleColor();

    // -- Export / Import / Add Entry buttons (right-aligned) --
    {
        const float addBtnW    = 100.0f;
        const float ioGap      = 4.0f;
        const float exportBtnW = 70.0f;
        const float importBtnW = 70.0f;
        const float totalW     = exportBtnW + ioGap + importBtnW + ioGap + addBtnW;
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - totalW + ImGui::GetCursorPosX());
        if (ImGui::Button("Export##char", ImVec2(exportBtnW, 0)))
        {
            std::string path = OpenTsvSaveDialog(L"character_list.tsv");
            if (!path.empty()) ExportCharacterListTsv(bin.characterEntries, path);
        }
        ImGui::SameLine(0, ioGap);
        if (!m_renderReadOnly && ImGui::Button("Import##char", ImVec2(importBtnW, 0)))
        {
            std::string path = OpenTsvOpenDialog();
            if (!path.empty())
            {
                auto imported = ImportCharacterListTsv(path);
                if (!imported.empty()) bin.characterEntries = std::move(imported);
            }
        }
        ImGui::SameLine(0, ioGap);
        if (!m_renderReadOnly && ImGui::Button("+ Add Entry", ImVec2(addBtnW, 0)))
            bin.characterEntries.push_back(CharacterEntry{});
    }

    ImGui::Separator();

    // -- Table --
    constexpr ImGuiTableFlags tFlags =
        ImGuiTableFlags_ScrollX          |
        ImGuiTableFlags_ScrollY          |
        ImGuiTableFlags_RowBg            |
        ImGuiTableFlags_BordersOuter     |
        ImGuiTableFlags_BordersInnerV    |
        ImGuiTableFlags_Resizable        |
        ImGuiTableFlags_Reorderable      |
        ImGuiTableFlags_Hideable         |
        ImGuiTableFlags_SizingFixedFit;

    // Column count: 1 (row/delete) + 15 fields = 16
    if (!ImGui::BeginTable("##CharListTable", 16, tFlags,
                           ImGui::GetContentRegionAvail()))
        return;

    ImGui::TableSetupScrollFreeze(1, 1);

    ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, ColumnWidths::kRowCtrl);
    for (int fi = 0; fi < FieldNames::CharacterCount; ++fi)
        ImGui::TableSetupColumn(FieldNames::Character[fi],
                                ImGuiTableColumnFlags_WidthFixed, ColumnWidths::kCharacter[fi]);
    ImGui::TableHeadersRow();

    int deleteIdx = -1;

    for (int i = 0; i < (int)bin.characterEntries.size(); ++i)
    {
        auto& e = bin.characterEntries[i];
        ImGui::TableNextRow();
        ImGui::PushID(i);

        // -- # column: row number + delete button --
        ImGui::TableSetColumnIndex(0);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.50f, 0.50f, 0.60f, 1.00f));
        ImGui::Text("%d", i + 1);
        ImGui::PopStyleColor();
        ImGui::SameLine(0, 4.0f);
        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.55f, 0.12f, 0.12f, 1.00f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.78f, 0.18f, 0.18f, 1.00f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.65f, 0.15f, 0.15f, 1.00f));
        if (!m_renderReadOnly && ImGui::SmallButton("X")) deleteIdx = i;
        ImGui::PopStyleColor(3);

        auto StrCell = [](const char* id, char* buf, size_t sz) {
            ImGui::SetNextItemWidth(-FLT_MIN);
            ImGui::InputText(id, buf, sz);
        };
        auto U32Cell = [](const char* id, uint32_t& v) {
            ImGui::SetNextItemWidth(-FLT_MIN);
            ImGui::InputScalar(id, ImGuiDataType_U32, &v);
        };
        auto FltCell = [](const char* id, float& v) {
            ImGui::SetNextItemWidth(-FLT_MIN);
            ImGui::InputScalar(id, ImGuiDataType_Float, &v);
        };
        auto BoolCell = [](const char* id, bool& v) {
            ImGui::Checkbox(id, &v);
        };

        ImGui::TableSetColumnIndex(1);  StrCell("##cc",   e.character_code,     sizeof(e.character_code));
        ImGui::TableSetColumnIndex(2);  U32Cell("##nh",   e.name_hash);
        ImGui::TableSetColumnIndex(3);  BoolCell("##enb", e.is_enabled);
        ImGui::TableSetColumnIndex(4);  BoolCell("##sel", e.is_selectable);
        ImGui::TableSetColumnIndex(5);  StrCell("##grp",  e.group,              sizeof(e.group));
        ImGui::TableSetColumnIndex(6);  FltCell("##cam",  e.camera_offset);
        ImGui::TableSetColumnIndex(7);  BoolCell("##ply", e.is_playable);
        ImGui::TableSetColumnIndex(8);  U32Cell("##so",   e.sort_order);
        ImGui::TableSetColumnIndex(9);  StrCell("##fnk",  e.full_name_key,      sizeof(e.full_name_key));
        ImGui::TableSetColumnIndex(10); StrCell("##snjk", e.short_name_jp_key,  sizeof(e.short_name_jp_key));
        ImGui::TableSetColumnIndex(11); StrCell("##snk",  e.short_name_key,     sizeof(e.short_name_key));
        ImGui::TableSetColumnIndex(12); StrCell("##org",  e.origin_key,         sizeof(e.origin_key));
        ImGui::TableSetColumnIndex(13); StrCell("##fsk",  e.fighting_style_key, sizeof(e.fighting_style_key));
        ImGui::TableSetColumnIndex(14); StrCell("##htk",  e.height_key,         sizeof(e.height_key));
        ImGui::TableSetColumnIndex(15); StrCell("##wtk",  e.weight_key,         sizeof(e.weight_key));

        ImGui::PopID();
    }

    if (!m_renderReadOnly && deleteIdx >= 0)
        bin.characterEntries.erase(bin.characterEntries.begin() + deleteIdx);

    ImGui::EndTable();
}

// -----------------------------------------------------------------------------
//  customize_item_exclusive_list TSV export / import (per-tab)
// -----------------------------------------------------------------------------

static void ExportExclusiveRuleTsv(const std::vector<CustomizeExclusiveRuleEntry>& entries, const std::string& path)
{
    FILE* f = nullptr;
    fopen_s(&f, path.c_str(), "wb");
    if (!f) return;
    for (const auto& e : entries)
        fprintf(f, "%u\t%u\t%u\t%u\n", e.item_id, e.hash, e.link_type, e.ref_item_id);
    fclose(f);
}

static void ImportExclusiveRuleTsv(const std::string& path, std::vector<CustomizeExclusiveRuleEntry>& entries)
{
    FILE* f = nullptr;
    fopen_s(&f, path.c_str(), "rb");
    if (!f) return;
    entries.clear();
    char line[256];
    while (fgets(line, sizeof(line), f))
    {
        int len = (int)strlen(line);
        while (len > 0 && (line[len-1] == '\r' || line[len-1] == '\n')) line[--len] = '\0';
        if (len == 0 || line[0] == '#') continue;
        char* cols[4] = {}; int col = 0; char* p = line; cols[col++] = p;
        for (; *p && col < 4; ++p) if (*p == '\t') { *p = '\0'; cols[col++] = p + 1; }
        if (col < 4) continue;
        CustomizeExclusiveRuleEntry e;
        e.item_id     = (uint32_t)strtoul(cols[0], nullptr, 10);
        e.hash        = (uint32_t)strtoul(cols[1], nullptr, 10);
        e.link_type   = (uint32_t)strtoul(cols[2], nullptr, 10);
        e.ref_item_id = (uint32_t)strtoul(cols[3], nullptr, 10);
        entries.push_back(e);
    }
    fclose(f);
}

static void ExportExclusivePairTsv(const std::vector<CustomizeExclusivePairEntry>& entries, const std::string& path)
{
    FILE* f = nullptr;
    fopen_s(&f, path.c_str(), "wb");
    if (!f) return;
    for (const auto& e : entries)
        fprintf(f, "%u\t%u\t%u\n", e.item_id_a, e.item_id_b, e.flag);
    fclose(f);
}

static void ImportExclusivePairTsv(const std::string& path, std::vector<CustomizeExclusivePairEntry>& entries)
{
    FILE* f = nullptr;
    fopen_s(&f, path.c_str(), "rb");
    if (!f) return;
    entries.clear();
    char line[256];
    while (fgets(line, sizeof(line), f))
    {
        int len = (int)strlen(line);
        while (len > 0 && (line[len-1] == '\r' || line[len-1] == '\n')) line[--len] = '\0';
        if (len == 0 || line[0] == '#') continue;
        char* cols[3] = {}; int col = 0; char* p = line; cols[col++] = p;
        for (; *p && col < 3; ++p) if (*p == '\t') { *p = '\0'; cols[col++] = p + 1; }
        if (col < 3) continue;
        CustomizeExclusivePairEntry e;
        e.item_id_a = (uint32_t)strtoul(cols[0], nullptr, 10);
        e.item_id_b = (uint32_t)strtoul(cols[1], nullptr, 10);
        e.flag      = (uint32_t)strtoul(cols[2], nullptr, 10);
        entries.push_back(e);
    }
    fclose(f);
}

// -----------------------------------------------------------------------------
//  customize_item_exclusive_list editor (5 sub-tables via tab bar)
// -----------------------------------------------------------------------------

void FbsDataView::RenderCustomizeItemExclusiveListEditor(ContentsBinData& bin)
{
    // -- Header row --
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.65f, 0.82f, 1.00f, 1.00f));
    ImGui::Text("customize_item_exclusive_list.bin");
    ImGui::PopStyleColor();

    ImGui::Separator();

    constexpr ImGuiTableFlags tFlags =
        ImGuiTableFlags_ScrollX          |
        ImGuiTableFlags_ScrollY          |
        ImGuiTableFlags_RowBg            |
        ImGuiTableFlags_BordersOuter     |
        ImGuiTableFlags_BordersInnerV    |
        ImGuiTableFlags_Resizable        |
        ImGuiTableFlags_Reorderable      |
        ImGuiTableFlags_Hideable         |
        ImGuiTableFlags_SizingFixedFit;

    // -- Item list cache: common + unique entries from the current tkmod --
    static std::vector<std::pair<uint32_t, std::string>> s_itemList;
    // -- Local item list cache: deduplicated item_no values from common list --
    static std::vector<std::pair<uint32_t, std::string>> s_localItemList;
    static size_t s_cacheKey = SIZE_MAX;
    {
        size_t curKey = 0;
        for (size_t ci = 0; ci < m_data.contents.size(); ++ci) {
            const ContentsBinData& c = m_data.contents[ci];
            if (c.type == BinType::CustomizeItemCommonList)
                curKey += c.commonEntries.size();
            else if (c.type == BinType::CustomizeItemUniqueList)
                curKey += c.customizeItemUniqueEntries.size();
        }
        if (curKey != s_cacheKey) {
            s_cacheKey = curKey;
            s_itemList.clear();
            s_localItemList.clear();
            for (size_t ci = 0; ci < m_data.contents.size(); ++ci) {
                const ContentsBinData& c = m_data.contents[ci];
                if (c.type == BinType::CustomizeItemCommonList) {
                    for (size_t ei = 0; ei < c.commonEntries.size(); ++ei) {
                        const CustomizeItemCommonEntry& e = c.commonEntries[ei];
                        // item_id list (AssetName display)
                        char label[320];
                        if (e.item_code[0])
                            snprintf(label, sizeof(label), "%s (%u)", e.item_code, e.item_id);
                        else
                            snprintf(label, sizeof(label), "%u", e.item_id);
                        s_itemList.push_back(std::make_pair(e.item_id, std::string(label)));
                        // local item_no list (raw number, deduplicated)
                        uint32_t no = (uint32_t)e.item_no;
                        bool dup = false;
                        for (size_t k = 0; k < s_localItemList.size(); ++k)
                            if (s_localItemList[k].first == no) { dup = true; break; }
                        if (!dup) {
                            char noLabel[32];
                            snprintf(noLabel, sizeof(noLabel), "%d", e.item_no);
                            s_localItemList.push_back(std::make_pair(no, std::string(noLabel)));
                        }
                    }
                } else if (c.type == BinType::CustomizeItemUniqueList) {
                    for (size_t ei = 0; ei < c.customizeItemUniqueEntries.size(); ++ei) {
                        const CustomizeItemUniqueEntry& e = c.customizeItemUniqueEntries[ei];
                        char label[320];
                        if (e.asset_name[0])
                            snprintf(label, sizeof(label), "%s (%u)", e.asset_name, e.char_item_id);
                        else
                            snprintf(label, sizeof(label), "%u", e.char_item_id);
                        s_itemList.push_back(std::make_pair(e.char_item_id, std::string(label)));
                    }
                }
            }
            std::sort(s_itemList.begin(), s_itemList.end(),
                [](const std::pair<uint32_t,std::string>& a,
                   const std::pair<uint32_t,std::string>& b){ return a.first < b.first; });
            std::sort(s_localItemList.begin(), s_localItemList.end(),
                [](const std::pair<uint32_t,std::string>& a,
                   const std::pair<uint32_t,std::string>& b){ return a.first < b.first; });
        }
    }

    // -- Manual input state for "Enter directly..." --
    static uint32_t* s_manualTarget    = nullptr;
    static char      s_manualBuf[32]   = {};
    static bool      s_openManualPopup = false;

    // -- Item ID combo cell (references common/unique list) --
    auto ItemIdCell = [&](const char* id, uint32_t& val) {
        const char* dispName = nullptr;
        for (size_t k = 0; k < s_itemList.size(); ++k)
            if (s_itemList[k].first == val) { dispName = s_itemList[k].second.c_str(); break; }

        char preview[320];
        if (dispName) snprintf(preview, sizeof(preview), "%s", dispName);
        else          snprintf(preview, sizeof(preview), "%u", val);

        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::BeginCombo(id, preview, ImGuiComboFlags_HeightLargest)) {
            for (size_t k = 0; k < s_itemList.size(); ++k) {
                bool sel = (val == s_itemList[k].first);
                if (ImGui::Selectable(s_itemList[k].second.c_str(), sel))
                    val = s_itemList[k].first;
                if (sel) ImGui::SetItemDefaultFocus();
            }
            ImGui::Separator();
            if (ImGui::Selectable("Enter directly...")) {
                snprintf(s_manualBuf, sizeof(s_manualBuf), "%u", val);
                s_manualTarget    = &val;
                s_openManualPopup = true;
            }
            ImGui::EndCombo();
        }
    };

    // Local Item ID combo cell (common list item_no, raw number display)
    auto LocalItemIdCell = [&](const char* id, uint32_t& val) {
        const char* dispName = nullptr;
        for (size_t k = 0; k < s_localItemList.size(); ++k)
            if (s_localItemList[k].first == val) { dispName = s_localItemList[k].second.c_str(); break; }

        char preview[32];
        snprintf(preview, sizeof(preview), "%u", val);
        (void)dispName; // display is the raw number regardless

        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::BeginCombo(id, preview, ImGuiComboFlags_HeightLargest)) {
            for (size_t k = 0; k < s_localItemList.size(); ++k) {
                bool sel = (val == s_localItemList[k].first);
                if (ImGui::Selectable(s_localItemList[k].second.c_str(), sel))
                    val = s_localItemList[k].first;
                if (sel) ImGui::SetItemDefaultFocus();
            }
            ImGui::Separator();
            if (ImGui::Selectable("Enter directly...")) {
                snprintf(s_manualBuf, sizeof(s_manualBuf), "%u", val);
                s_manualTarget    = &val;
                s_openManualPopup = true;
            }
            ImGui::EndCombo();
        }
    };

    // Helper: render a RuleEntry table
    auto RenderRuleTable = [&](const char* tableId, const wchar_t* tsvName, std::vector<CustomizeExclusiveRuleEntry>& entries, const char* const* fieldNames, bool useItemIdDropdown, bool useLocalDropdown) {
        const float addBtnW    = 100.0f;
        const float ioGap      = 4.0f;
        const float exportBtnW = 70.0f;
        const float importBtnW = 70.0f;
        const float totalW     = exportBtnW + ioGap + importBtnW + ioGap + addBtnW;
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.42f, 0.42f, 0.54f, 1.00f));
        ImGui::Text("(%d entries)", (int)entries.size());
        ImGui::PopStyleColor();
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - totalW + ImGui::GetCursorPosX());
        ImGui::PushID(tableId);
        if (ImGui::Button("Export", ImVec2(exportBtnW, 0)))
        {
            std::string path = OpenTsvSaveDialog(tsvName);
            if (!path.empty()) ExportExclusiveRuleTsv(entries, path);
        }
        ImGui::SameLine(0, ioGap);
        if (!m_renderReadOnly && ImGui::Button("Import", ImVec2(importBtnW, 0)))
        {
            std::string path = OpenTsvOpenDialog();
            if (!path.empty()) ImportExclusiveRuleTsv(path, entries);
        }
        ImGui::SameLine(0, ioGap);
        if (!m_renderReadOnly && ImGui::Button("+ Add Entry", ImVec2(addBtnW, 0)))
            entries.push_back(CustomizeExclusiveRuleEntry{});
        ImGui::PopID();

        if (!ImGui::BeginTable(tableId, 5, tFlags, ImGui::GetContentRegionAvail()))
            return;

        ImGui::TableSetupScrollFreeze(1, 1);
        ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, ColumnWidths::kRowCtrl);
        for (int fi = 0; fi < FieldNames::ExclusiveRuleCount; ++fi)
            ImGui::TableSetupColumn(fieldNames[fi],
                                    ImGuiTableColumnFlags_WidthFixed, ColumnWidths::kExclusiveRule[fi]);
        ImGui::TableHeadersRow();

        int deleteIdx = -1;
        for (int i = 0; i < (int)entries.size(); ++i)
        {
            auto& e = entries[i];
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
            if (!m_renderReadOnly && ImGui::SmallButton("X")) deleteIdx = i;
            ImGui::PopStyleColor(3);

            auto U32Cell = [](const char* id, uint32_t& v) {
                ImGui::SetNextItemWidth(-FLT_MIN);
                ImGui::InputScalar(id, ImGuiDataType_U32, &v);
            };
            ImGui::TableSetColumnIndex(1);
            if (useItemIdDropdown)    ItemIdCell("##iid",      e.item_id);
            else if (useLocalDropdown) LocalItemIdCell("##iid", e.item_id);
            else                       U32Cell("##iid",         e.item_id);
            ImGui::TableSetColumnIndex(2); HashComboCell("##hash", e.hash, FbsDataDict::Get().GetTypeHashCodeMap(), GetTypeHashItems());
            ImGui::TableSetColumnIndex(3); U32Cell("##lt",   e.link_type);
            ImGui::TableSetColumnIndex(4);
            if (useItemIdDropdown)    ItemIdCell("##rid",      e.ref_item_id);
            else if (useLocalDropdown) LocalItemIdCell("##rid", e.ref_item_id);
            else                       U32Cell("##rid",         e.ref_item_id);

            ImGui::PopID();
        }
        if (!m_renderReadOnly && deleteIdx >= 0)
            entries.erase(entries.begin() + deleteIdx);

        ImGui::EndTable();
    };

    // Helper: render a PairEntry table
    auto RenderPairTable = [&](const char* tableId, const wchar_t* tsvName, std::vector<CustomizeExclusivePairEntry>& entries, const char* const* fieldNames, bool useItemIdDropdown, bool useLocalDropdown) {
        const float addBtnW    = 100.0f;
        const float ioGap      = 4.0f;
        const float exportBtnW = 70.0f;
        const float importBtnW = 70.0f;
        const float totalW     = exportBtnW + ioGap + importBtnW + ioGap + addBtnW;
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.42f, 0.42f, 0.54f, 1.00f));
        ImGui::Text("(%d entries)", (int)entries.size());
        ImGui::PopStyleColor();
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - totalW + ImGui::GetCursorPosX());
        ImGui::PushID(tableId);
        if (ImGui::Button("Export", ImVec2(exportBtnW, 0)))
        {
            std::string path = OpenTsvSaveDialog(tsvName);
            if (!path.empty()) ExportExclusivePairTsv(entries, path);
        }
        ImGui::SameLine(0, ioGap);
        if (!m_renderReadOnly && ImGui::Button("Import", ImVec2(importBtnW, 0)))
        {
            std::string path = OpenTsvOpenDialog();
            if (!path.empty()) ImportExclusivePairTsv(path, entries);
        }
        ImGui::SameLine(0, ioGap);
        if (!m_renderReadOnly && ImGui::Button("+ Add Entry", ImVec2(addBtnW, 0)))
            entries.push_back(CustomizeExclusivePairEntry{});
        ImGui::PopID();

        if (!ImGui::BeginTable(tableId, 4, tFlags, ImGui::GetContentRegionAvail()))
            return;

        ImGui::TableSetupScrollFreeze(1, 1);
        ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, ColumnWidths::kRowCtrl);
        for (int fi = 0; fi < FieldNames::ExclusivePairCount; ++fi)
            ImGui::TableSetupColumn(fieldNames[fi],
                                    ImGuiTableColumnFlags_WidthFixed, ColumnWidths::kExclusivePair[fi]);
        ImGui::TableHeadersRow();

        int deleteIdx = -1;
        for (int i = 0; i < (int)entries.size(); ++i)
        {
            auto& e = entries[i];
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
            if (!m_renderReadOnly && ImGui::SmallButton("X")) deleteIdx = i;
            ImGui::PopStyleColor(3);

            auto U32Cell = [](const char* id, uint32_t& v) {
                ImGui::SetNextItemWidth(-FLT_MIN);
                ImGui::InputScalar(id, ImGuiDataType_U32, &v);
            };
            ImGui::TableSetColumnIndex(1);
            if (useItemIdDropdown)    ItemIdCell("##ia",       e.item_id_a);
            else if (useLocalDropdown) LocalItemIdCell("##ia",  e.item_id_a);
            else                       U32Cell("##ia",          e.item_id_a);
            ImGui::TableSetColumnIndex(2);
            if (useItemIdDropdown)    ItemIdCell("##ib",       e.item_id_b);
            else if (useLocalDropdown) LocalItemIdCell("##ib",  e.item_id_b);
            else                       U32Cell("##ib",          e.item_id_b);
            ImGui::TableSetColumnIndex(3); U32Cell("##flag", e.flag);

            ImGui::PopID();
        }
        if (!m_renderReadOnly && deleteIdx >= 0)
            entries.erase(entries.begin() + deleteIdx);

        ImGui::EndTable();
    };

    if (ImGui::BeginTabBar("##ExclTabs"))
    {
        if (ImGui::BeginTabItem(FieldNames::ExclusiveArrays[0]))
        {
            RenderRuleTable("##RuleTable",    L"rule_entries.tsv",       bin.exclusiveRuleEntries,      FieldNames::ExclusiveRule,      true,  false);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem(FieldNames::ExclusiveArrays[1]))
        {
            RenderPairTable("##PairTable",    L"pair_entries.tsv",       bin.exclusivePairEntries,      FieldNames::ExclusivePair,      true,  false);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem(FieldNames::ExclusiveArrays[2]))
        {
            RenderRuleTable("##GrpRuleTable", L"group_rule_entries.tsv", bin.exclusiveGroupRuleEntries, FieldNames::ExclusiveGroupRule, false, true);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem(FieldNames::ExclusiveArrays[3]))
        {
            RenderPairTable("##GrpPairTable", L"group_pair_entries.tsv", bin.exclusiveGroupPairEntries, FieldNames::ExclusiveGroupPair, false, true);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem(FieldNames::ExclusiveArrays[4]))
        {
            RenderRuleTable("##SetRuleTable", L"set_rule_entries.tsv",   bin.exclusiveSetRuleEntries,   FieldNames::ExclusiveSetRule,   false, true);
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    // "Enter directly..." manual input popup
    // OpenPopup must be called at the same window level as BeginPopup,
    // not from inside BeginCombo's context.
    if (s_openManualPopup) {
        ImGui::OpenPopup("##excl_manual_id");
        s_openManualPopup = false;
    }
    if (ImGui::BeginPopup("##excl_manual_id")) {
        ImGui::SetNextItemWidth(140.f);
        ImGui::InputText("##mid", s_manualBuf, sizeof(s_manualBuf),
                         ImGuiInputTextFlags_CharsDecimal);
        ImGui::SameLine();
        if (ImGui::SmallButton("OK") && s_manualTarget) {
            *s_manualTarget = (uint32_t)strtoul(s_manualBuf, nullptr, 10);
            s_manualTarget = nullptr;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Cancel")) {
            s_manualTarget = nullptr;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

// -----------------------------------------------------------------------------
//  Contents list panel (right side)
// -----------------------------------------------------------------------------

void FbsDataView::RenderContentsList(float listWidth)
{
    ImGui::SetCursorPos(ImVec2(10.0f, 8.0f));

    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.65f, 0.82f, 1.00f, 1.00f));
    ImGui::Text("Contents");
    ImGui::PopStyleColor();

    ImGui::SameLine(listWidth - 74.0f);

    // "+ Add" button -- opens popup on hover or click
    if (ImGui::Button("+ Add") || ImGui::IsItemHovered())
        ImGui::OpenPopup("##AddPopup");

    RenderAddPopup();

    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4.0f);
    ImGui::Separator();
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4.0f);

    // Bin list
    const float itemH   = 32.0f;
    const float availH  = ImGui::GetContentRegionAvail().y;
    ImGui::BeginChild("##BinList", ImVec2(0.0f, availH), false);

    int removeIdx = -1;

    for (int i = 0; i < (int)m_data.contents.size(); ++i)
    {
        const auto& bin = m_data.contents[i];
        const bool selected = (i == m_data.selectedIndex);

        ImGui::PushID(i);

        if (selected)
        {
            ImGui::PushStyleColor(ImGuiCol_Header,        ImVec4(0.22f, 0.40f, 0.72f, 1.00f));
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.28f, 0.48f, 0.82f, 1.00f));
            ImGui::PushStyleColor(ImGuiCol_HeaderActive,  ImVec4(0.25f, 0.44f, 0.78f, 1.00f));
        }

        if (ImGui::Selectable(bin.name.c_str(), selected,
                              ImGuiSelectableFlags_AllowOverlap, ImVec2(0.0f, itemH)))
        {
            m_data.selectedIndex = i;
        }

        // Right-click context menu: Remove
        if (ImGui::BeginPopupContextItem("##BinCtx"))
        {
            if (ImGui::MenuItem("Remove"))
                removeIdx = i;
            ImGui::EndPopup();
        }

        if (selected)
            ImGui::PopStyleColor(3);

        ImGui::PopID();
    }

    if (removeIdx >= 0)
    {
        m_data.contents.erase(m_data.contents.begin() + removeIdx);
        // Adjust selectedIndex
        if (m_data.selectedIndex >= (int)m_data.contents.size())
            m_data.selectedIndex = (int)m_data.contents.size() - 1;
    }

    ImGui::EndChild();
}

// -----------------------------------------------------------------------------
//  Add popup (fbsdata > bins grouped by category)
// -----------------------------------------------------------------------------

void FbsDataView::RenderAddPopup()
{
    if (!ImGui::BeginPopup("##AddPopup")) return;

    if (ImGui::BeginMenu("fbsdata"))
    {
        const char* currentCategory = nullptr;

        for (int i = 0; i < k_AllBinsCount; ++i)
        {
            const auto& info = k_AllBins[i];

            // Skip bins that are not visible in the current build configuration
            if (!BinVisibility::IsVisible(info.filename))
                continue;

            // Skip bins already added to the mod
            if (m_data.HasBinByName(info.filename)) continue;

            // Category header when group changes
            if (!currentCategory || strcmp(currentCategory, info.category) != 0)
            {
                if (currentCategory != nullptr)
                    ImGui::Separator();
                ImGui::TextDisabled("%s", info.category);
                currentCategory = info.category;
            }

            if (!info.supported)
            {
                ImGui::BeginDisabled();
                ImGui::MenuItem(info.filename);
                ImGui::EndDisabled();
            }
            else
            {
                if (ImGui::MenuItem(info.filename))
                {
                    ContentsBinData bin;
                    bin.type = info.type;
                    bin.name = info.filename;

                    // Pre-populate with one default entry
                    switch (bin.type)
                    {
                    case BinType::CustomizeItemCommonList:
                        bin.commonEntries.push_back(DefaultValues::CommonEntry());
                        break;
                    case BinType::CustomizePanelList:
                        bin.customizePanelEntries.push_back(CustomizePanelEntry{});
                        break;
                    case BinType::CharacterList:
                        bin.characterEntries.push_back(CharacterEntry{});
                        break;
                    case BinType::CustomizeItemExclusiveList:
                        bin.exclusiveRuleEntries.push_back(CustomizeExclusiveRuleEntry{});
                        break;
                    case BinType::AreaList:
                        bin.areaEntries.push_back(AreaEntry{});
                        break;
                    case BinType::BattleSubtitleInfoList:
                        bin.battleSubtitleEntries.push_back(BattleSubtitleInfoEntry{});
                        break;
                    case BinType::FateDramaPlayerStartList:
                        bin.fateDramaPlayerStartEntries.push_back(FateDramaPlayerStartEntry{});
                        break;
                    case BinType::JukeboxList:
                        bin.jukeboxEntries.push_back(JukeboxEntry{});
                        break;
                    case BinType::SeriesList:
                        bin.seriesEntries.push_back(SeriesEntry{});
                        break;
                    case BinType::TamMissionList:
                        bin.tamMissionEntries.push_back(TamMissionEntry{});
                        break;
                    case BinType::DramaPlayerStartList:
                        bin.dramaPlayerStartEntries.push_back(DramaPlayerStartEntry{});
                        break;
                    case BinType::StageList:
                        bin.stageEntries.push_back(StageEntry{});
                        break;
                    case BinType::BallPropertyList:
                        bin.ballPropertyEntries.push_back(BallPropertyEntry{});
                        break;
                    case BinType::BodyCylinderDataList:
                        bin.bodyCylinderDataEntries.push_back(BodyCylinderDataEntry{});
                        break;
                    case BinType::CustomizeItemUniqueList:
                        bin.customizeItemUniqueEntries.push_back(DefaultValues::UniqueEntry());
                        break;
                    case BinType::CharacterSelectList:
                        bin.characterSelectHashEntries.push_back(CharacterSelectHashEntry{});
                        break;
                    case BinType::CustomizeItemProhibitDramaList:
                        bin.prohibitDramaGroup0.push_back(CustomizeItemProhibitDramaEntry{});
                        break;
                    case BinType::BattleMotionList:
                        bin.battleMotionEntries.push_back(BattleMotionEntry{});
                        break;
                    case BinType::ArcadeCpuList:
                        break;
                    case BinType::BallRecommendList:
                        break;
                    case BinType::BallSettingList:
                        break;
                    case BinType::BattleCommonList:
                        break;
                    case BinType::BattleCpuList:
                        break;
                    case BinType::RankList:
                        break;
                    case BinType::AssistInputList:
                        bin.assistInputEntries.push_back(AssistInputEntry{});
                        break;
                    default:
                        break;
                    }

                    m_data.selectedIndex = static_cast<int>(m_data.contents.size());
                    m_data.contents.push_back(std::move(bin));
                    ImGui::CloseCurrentPopup();
                }
            }
        }

        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("exception"))
    {
        struct ExceptionBinInfo { const char* name; BinType type; };
        static const ExceptionBinInfo k_ExceptionBins[] = {
            { "customize_item_exception", BinType::CustomizeItemException },
        };
        for (const auto& info : k_ExceptionBins)
        {
            if (m_data.HasBinByName(info.name)) continue;
            if (ImGui::MenuItem(info.name))
            {
                ContentsBinData bin;
                bin.type = info.type;
                bin.name = info.name;
                bin.exceptionEntries.push_back(CustomizeItemExceptionEntry{});
                m_data.selectedIndex = static_cast<int>(m_data.contents.size());
                m_data.contents.push_back(std::move(bin));
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::EndMenu();
    }

    ImGui::EndPopup();
}

// -----------------------------------------------------------------------------
//  area_list editor
// -----------------------------------------------------------------------------

static void ExportAreaListTsv(const std::vector<AreaEntry>& entries, const std::string& path)
{
    FILE* f = nullptr; fopen_s(&f, path.c_str(), "wb"); if (!f) return;
    for (const auto& e : entries) { char line[512]; int n = snprintf(line, sizeof(line), "%u\t%s\n", e.area_hash, e.area_code); fwrite(line, 1, n, f); }
    fclose(f);
}
static std::vector<AreaEntry> ImportAreaListTsv(const std::string& path)
{
    std::vector<AreaEntry> result; FILE* f = nullptr; fopen_s(&f, path.c_str(), "rb"); if (!f) return result;
    char line[512];
    while (fgets(line, sizeof(line), f))
    {
        int len = (int)strlen(line); while (len > 0 && (line[len-1]=='\r'||line[len-1]=='\n')) line[--len]='\0'; if (!len) continue;
        char* cols[2]={}; int col=0; char* p=line; cols[col++]=p; for(;*p&&col<2;++p) if(*p=='\t'){*p='\0';cols[col++]=p+1;} if(col<2) continue;
        AreaEntry e; e.area_hash=(uint32_t)strtoul(cols[0],nullptr,10); strncpy_s(e.area_code,cols[1],_TRUNCATE); result.push_back(e);
    }
    fclose(f); return result;
}

void FbsDataView::RenderAreaListEditor(ContentsBinData& bin)
{
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.65f, 0.82f, 1.00f, 1.00f));
    ImGui::Text("area_list.bin");
    ImGui::PopStyleColor();
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.42f, 0.42f, 0.54f, 1.00f));
    ImGui::Text("(%d entries)", (int)bin.areaEntries.size());
    ImGui::PopStyleColor();

    {
        const float addBtnW=100.0f, ioGap=4.0f, exportBtnW=70.0f, importBtnW=70.0f;
        const float totalW=exportBtnW+ioGap+importBtnW+ioGap+addBtnW;
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - totalW + ImGui::GetCursorPosX());
        if (ImGui::Button("Export##area", ImVec2(exportBtnW, 0))) { std::string p=OpenTsvSaveDialog(L"area_list.tsv"); if(!p.empty()) ExportAreaListTsv(bin.areaEntries,p); }
        ImGui::SameLine(0, ioGap);
        if (!m_renderReadOnly && ImGui::Button("Import##area", ImVec2(importBtnW, 0))) { std::string p=OpenTsvOpenDialog(); if(!p.empty()){auto imp=ImportAreaListTsv(p);if(!imp.empty())bin.areaEntries=std::move(imp);} }
        ImGui::SameLine(0, ioGap);
        if (!m_renderReadOnly && ImGui::Button("+ Add Entry", ImVec2(addBtnW, 0)))
            bin.areaEntries.push_back(AreaEntry{});
    }

    ImGui::Separator();

    constexpr ImGuiTableFlags tFlags =
        ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg |
        ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersInnerV |
        ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable |
        ImGuiTableFlags_Hideable | ImGuiTableFlags_SizingFixedFit;

    if (!ImGui::BeginTable("##AreaTable", 3, tFlags, ImGui::GetContentRegionAvail()))
        return;

    ImGui::TableSetupScrollFreeze(1, 1);
    ImGui::TableSetupColumn("#",                            ImGuiTableColumnFlags_WidthFixed, ColumnWidths::kRowCtrl);
    ImGui::TableSetupColumn(FieldNames::AreaEntry[0],       ImGuiTableColumnFlags_WidthFixed, ColumnWidths::kArea[0]);
    ImGui::TableSetupColumn(FieldNames::AreaEntry[1],       ImGuiTableColumnFlags_WidthFixed, ColumnWidths::kArea[1]);
    ImGui::TableHeadersRow();

    int deleteIdx = -1;
    ImGuiListClipper clipper;
    clipper.Begin((int)bin.areaEntries.size());
    while (clipper.Step())
    {
        for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i)
        {
            auto& e = bin.areaEntries[i];
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
            if (!m_renderReadOnly && ImGui::SmallButton("X")) deleteIdx = i;
            ImGui::PopStyleColor(3);

            auto U32Cell = [](const char* id, uint32_t& v) {
                ImGui::SetNextItemWidth(-FLT_MIN);
                ImGui::InputScalar(id, ImGuiDataType_U32, &v);
            };
            auto StrCell = [](const char* id, char* buf, size_t sz) {
                ImGui::SetNextItemWidth(-FLT_MIN);
                ImGui::InputText(id, buf, sz);
            };

            ImGui::TableSetColumnIndex(1); U32Cell("##ah", e.area_hash);
            ImGui::TableSetColumnIndex(2); StrCell("##ac", e.area_code, sizeof(e.area_code));

            ImGui::PopID();
        }
    }
    if (!m_renderReadOnly && deleteIdx >= 0)
        bin.areaEntries.erase(bin.areaEntries.begin() + deleteIdx);

    ImGui::EndTable();
}

// -----------------------------------------------------------------------------
//  battle_subtitle_info editor
// -----------------------------------------------------------------------------

static void ExportBattleSubtitleTsv(const std::vector<BattleSubtitleInfoEntry>& entries, const std::string& path)
{
    FILE* f = nullptr; fopen_s(&f, path.c_str(), "wb"); if (!f) return;
    for (const auto& e : entries) fprintf(f, "%u\t%u\n", e.subtitle_hash, e.subtitle_type);
    fclose(f);
}
static std::vector<BattleSubtitleInfoEntry> ImportBattleSubtitleTsv(const std::string& path)
{
    std::vector<BattleSubtitleInfoEntry> result; FILE* f = nullptr; fopen_s(&f, path.c_str(), "rb"); if (!f) return result;
    char line[256];
    while (fgets(line, sizeof(line), f))
    {
        int len=(int)strlen(line); while(len>0&&(line[len-1]=='\r'||line[len-1]=='\n'))line[--len]='\0'; if(!len) continue;
        char* cols[2]={}; int col=0; char* p=line; cols[col++]=p; for(;*p&&col<2;++p) if(*p=='\t'){*p='\0';cols[col++]=p+1;} if(col<2) continue;
        BattleSubtitleInfoEntry e; e.subtitle_hash=(uint32_t)strtoul(cols[0],nullptr,10); e.subtitle_type=(uint32_t)strtoul(cols[1],nullptr,10); result.push_back(e);
    }
    fclose(f); return result;
}

void FbsDataView::RenderBattleSubtitleInfoEditor(ContentsBinData& bin)
{
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.65f, 0.82f, 1.00f, 1.00f));
    ImGui::Text("battle_subtitle_info.bin");
    ImGui::PopStyleColor();
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.42f, 0.42f, 0.54f, 1.00f));
    ImGui::Text("(%d entries)", (int)bin.battleSubtitleEntries.size());
    ImGui::PopStyleColor();

    {
        const float addBtnW=100.0f, ioGap=4.0f, exportBtnW=70.0f, importBtnW=70.0f;
        const float totalW=exportBtnW+ioGap+importBtnW+ioGap+addBtnW;
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - totalW + ImGui::GetCursorPosX());
        if (ImGui::Button("Export##bsi", ImVec2(exportBtnW, 0))) { std::string p=OpenTsvSaveDialog(L"battle_subtitle_info.tsv"); if(!p.empty()) ExportBattleSubtitleTsv(bin.battleSubtitleEntries,p); }
        ImGui::SameLine(0, ioGap);
        if (!m_renderReadOnly && ImGui::Button("Import##bsi", ImVec2(importBtnW, 0))) { std::string p=OpenTsvOpenDialog(); if(!p.empty()){auto imp=ImportBattleSubtitleTsv(p);if(!imp.empty())bin.battleSubtitleEntries=std::move(imp);} }
        ImGui::SameLine(0, ioGap);
        if (!m_renderReadOnly && ImGui::Button("+ Add Entry", ImVec2(addBtnW, 0)))
            bin.battleSubtitleEntries.push_back(BattleSubtitleInfoEntry{});
    }

    ImGui::Separator();

    constexpr ImGuiTableFlags tFlags =
        ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg |
        ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersInnerV |
        ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable |
        ImGuiTableFlags_Hideable | ImGuiTableFlags_SizingFixedFit;

    if (!ImGui::BeginTable("##BSubTable", 3, tFlags, ImGui::GetContentRegionAvail()))
        return;

    ImGui::TableSetupScrollFreeze(1, 1);
    ImGui::TableSetupColumn("#",                                 ImGuiTableColumnFlags_WidthFixed, ColumnWidths::kRowCtrl);
    ImGui::TableSetupColumn(FieldNames::BattleSubtitleInfo[0],   ImGuiTableColumnFlags_WidthFixed, ColumnWidths::kBattleSubtitle[0]);
    ImGui::TableSetupColumn(FieldNames::BattleSubtitleInfo[1],   ImGuiTableColumnFlags_WidthFixed, ColumnWidths::kBattleSubtitle[1]);
    ImGui::TableHeadersRow();

    int deleteIdx = -1;
    ImGuiListClipper clipper;
    clipper.Begin((int)bin.battleSubtitleEntries.size());
    while (clipper.Step())
    {
        for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i)
        {
            auto& e = bin.battleSubtitleEntries[i];
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
            if (!m_renderReadOnly && ImGui::SmallButton("X")) deleteIdx = i;
            ImGui::PopStyleColor(3);

            auto U32Cell = [](const char* id, uint32_t& v) {
                ImGui::SetNextItemWidth(-FLT_MIN);
                ImGui::InputScalar(id, ImGuiDataType_U32, &v);
            };

            ImGui::TableSetColumnIndex(1); U32Cell("##sh", e.subtitle_hash);
            ImGui::TableSetColumnIndex(2); U32Cell("##st", e.subtitle_type);

            ImGui::PopID();
        }
    }
    if (!m_renderReadOnly && deleteIdx >= 0)
        bin.battleSubtitleEntries.erase(bin.battleSubtitleEntries.begin() + deleteIdx);

    ImGui::EndTable();
}

// -----------------------------------------------------------------------------
//  fate_drama_player_start_list editor
// -----------------------------------------------------------------------------

static void ExportFateDramaPlayerStartTsv(const std::vector<FateDramaPlayerStartEntry>& entries, const std::string& path)
{
    FILE* f = nullptr; fopen_s(&f, path.c_str(), "wb"); if (!f) return;
    for (const auto& e : entries) fprintf(f, "%u\t%u\t%u\t%u\t%s\n", e.character1_hash, e.character2_hash, e.value_0, e.hash_2, e.value_4?"TRUE":"FALSE");
    fclose(f);
}
static std::vector<FateDramaPlayerStartEntry> ImportFateDramaPlayerStartTsv(const std::string& path)
{
    std::vector<FateDramaPlayerStartEntry> result; FILE* f=nullptr; fopen_s(&f,path.c_str(),"rb"); if(!f) return result;
    char line[256];
    while (fgets(line, sizeof(line), f))
    {
        int len=(int)strlen(line); while(len>0&&(line[len-1]=='\r'||line[len-1]=='\n'))line[--len]='\0'; if(!len) continue;
        char* cols[5]={}; int col=0; char* p=line; cols[col++]=p; for(;*p&&col<5;++p) if(*p=='\t'){*p='\0';cols[col++]=p+1;} if(col<5) continue;
        FateDramaPlayerStartEntry e;
        e.character1_hash=(uint32_t)strtoul(cols[0],nullptr,10); e.character2_hash=(uint32_t)strtoul(cols[1],nullptr,10);
        e.value_0=(uint32_t)strtoul(cols[2],nullptr,10); e.hash_2=(uint32_t)strtoul(cols[3],nullptr,10); e.value_4=ParseBool(cols[4]);
        result.push_back(e);
    }
    fclose(f); return result;
}

void FbsDataView::RenderFateDramaPlayerStartListEditor(ContentsBinData& bin)
{
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.65f, 0.82f, 1.00f, 1.00f));
    ImGui::Text("fate_drama_player_start_list.bin");
    ImGui::PopStyleColor();
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.42f, 0.42f, 0.54f, 1.00f));
    ImGui::Text("(%d entries)", (int)bin.fateDramaPlayerStartEntries.size());
    ImGui::PopStyleColor();

    {
        const float addBtnW=100.0f, ioGap=4.0f, exportBtnW=70.0f, importBtnW=70.0f;
        const float totalW=exportBtnW+ioGap+importBtnW+ioGap+addBtnW;
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - totalW + ImGui::GetCursorPosX());
        if (ImGui::Button("Export##fdps", ImVec2(exportBtnW, 0))) { std::string p=OpenTsvSaveDialog(L"fate_drama_player_start_list.tsv"); if(!p.empty()) ExportFateDramaPlayerStartTsv(bin.fateDramaPlayerStartEntries,p); }
        ImGui::SameLine(0, ioGap);
        if (!m_renderReadOnly && ImGui::Button("Import##fdps", ImVec2(importBtnW, 0))) { std::string p=OpenTsvOpenDialog(); if(!p.empty()){auto imp=ImportFateDramaPlayerStartTsv(p);if(!imp.empty())bin.fateDramaPlayerStartEntries=std::move(imp);} }
        ImGui::SameLine(0, ioGap);
        if (!m_renderReadOnly && ImGui::Button("+ Add Entry", ImVec2(addBtnW, 0)))
            bin.fateDramaPlayerStartEntries.push_back(FateDramaPlayerStartEntry{});
    }

    ImGui::Separator();

    constexpr ImGuiTableFlags tFlags =
        ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg |
        ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersInnerV |
        ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable |
        ImGuiTableFlags_Hideable | ImGuiTableFlags_SizingFixedFit;

    if (!ImGui::BeginTable("##FDPSTable", 6, tFlags, ImGui::GetContentRegionAvail()))
        return;

    ImGui::TableSetupScrollFreeze(1, 1);
    ImGui::TableSetupColumn("#",                                   ImGuiTableColumnFlags_WidthFixed, ColumnWidths::kRowCtrl);
    ImGui::TableSetupColumn(FieldNames::FateDramaPlayerStart[0],   ImGuiTableColumnFlags_WidthFixed, ColumnWidths::kFateDramaPlayerStart[0]);
    ImGui::TableSetupColumn(FieldNames::FateDramaPlayerStart[1],   ImGuiTableColumnFlags_WidthFixed, ColumnWidths::kFateDramaPlayerStart[1]);
    ImGui::TableSetupColumn(FieldNames::FateDramaPlayerStart[2],   ImGuiTableColumnFlags_WidthFixed, ColumnWidths::kFateDramaPlayerStart[2]);
    ImGui::TableSetupColumn(FieldNames::FateDramaPlayerStart[3],   ImGuiTableColumnFlags_WidthFixed, ColumnWidths::kFateDramaPlayerStart[3]);
    ImGui::TableSetupColumn(FieldNames::FateDramaPlayerStart[4],   ImGuiTableColumnFlags_WidthFixed, ColumnWidths::kFateDramaPlayerStart[4]);
    ImGui::TableHeadersRow();

    int deleteIdx = -1;
    ImGuiListClipper clipper;
    clipper.Begin((int)bin.fateDramaPlayerStartEntries.size());
    while (clipper.Step())
    {
        for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i)
        {
            auto& e = bin.fateDramaPlayerStartEntries[i];
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
            if (!m_renderReadOnly && ImGui::SmallButton("X")) deleteIdx = i;
            ImGui::PopStyleColor(3);

            auto U32Cell = [](const char* id, uint32_t& v) {
                ImGui::SetNextItemWidth(-FLT_MIN);
                ImGui::InputScalar(id, ImGuiDataType_U32, &v);
            };

            ImGui::TableSetColumnIndex(1); U32Cell("##c1h", e.character1_hash);
            ImGui::TableSetColumnIndex(2); U32Cell("##c2h", e.character2_hash);
            ImGui::TableSetColumnIndex(3); U32Cell("##v0",  e.value_0);
            ImGui::TableSetColumnIndex(4); U32Cell("##h2",  e.hash_2);
            ImGui::TableSetColumnIndex(5);
            ImGui::Checkbox("##v4", &e.value_4);

            ImGui::PopID();
        }
    }
    if (!m_renderReadOnly && deleteIdx >= 0)
        bin.fateDramaPlayerStartEntries.erase(bin.fateDramaPlayerStartEntries.begin() + deleteIdx);

    ImGui::EndTable();
}

// -----------------------------------------------------------------------------
//  jukebox_list editor
// -----------------------------------------------------------------------------

static void ExportJukeboxListTsv(const std::vector<JukeboxEntry>& entries, const std::string& path)
{
    FILE* f=nullptr; fopen_s(&f,path.c_str(),"wb"); if(!f) return;
    for (const auto& e : entries)
    {
        char line[2048];
        int n=snprintf(line,sizeof(line),"%u\t%u\t%u\t%s\t%s\t%s\t%s\t%s\t%s\n",
            e.bgm_hash,e.series_hash,e.unk_2,e.cue_name,e.arrangement,e.alt_cue_name_1,e.alt_cue_name_2,e.alt_cue_name_3,e.display_text_key);
        fwrite(line,1,n,f);
    }
    fclose(f);
}
static std::vector<JukeboxEntry> ImportJukeboxListTsv(const std::string& path)
{
    std::vector<JukeboxEntry> result; FILE* f=nullptr; fopen_s(&f,path.c_str(),"rb"); if(!f) return result;
    char line[2048];
    while (fgets(line, sizeof(line), f))
    {
        int len=(int)strlen(line); while(len>0&&(line[len-1]=='\r'||line[len-1]=='\n'))line[--len]='\0'; if(!len) continue;
        char* cols[9]={}; int col=0; char* p=line; cols[col++]=p; for(;*p&&col<9;++p) if(*p=='\t'){*p='\0';cols[col++]=p+1;} if(col<9) continue;
        JukeboxEntry e;
        e.bgm_hash=(uint32_t)strtoul(cols[0],nullptr,10); e.series_hash=(uint32_t)strtoul(cols[1],nullptr,10); e.unk_2=(uint32_t)strtoul(cols[2],nullptr,10);
        strncpy_s(e.cue_name,cols[3],_TRUNCATE); strncpy_s(e.arrangement,cols[4],_TRUNCATE); strncpy_s(e.alt_cue_name_1,cols[5],_TRUNCATE);
        strncpy_s(e.alt_cue_name_2,cols[6],_TRUNCATE); strncpy_s(e.alt_cue_name_3,cols[7],_TRUNCATE); strncpy_s(e.display_text_key,cols[8],_TRUNCATE);
        result.push_back(e);
    }
    fclose(f); return result;
}

void FbsDataView::RenderJukeboxListEditor(ContentsBinData& bin)
{
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.65f, 0.82f, 1.00f, 1.00f));
    ImGui::Text("jukebox_list.bin");
    ImGui::PopStyleColor();
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.42f, 0.42f, 0.54f, 1.00f));
    ImGui::Text("(%d entries)", (int)bin.jukeboxEntries.size());
    ImGui::PopStyleColor();

    {
        const float addBtnW=100.0f, ioGap=4.0f, exportBtnW=70.0f, importBtnW=70.0f;
        const float totalW=exportBtnW+ioGap+importBtnW+ioGap+addBtnW;
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - totalW + ImGui::GetCursorPosX());
        if (ImGui::Button("Export##juke", ImVec2(exportBtnW, 0))) { std::string p=OpenTsvSaveDialog(L"jukebox_list.tsv"); if(!p.empty()) ExportJukeboxListTsv(bin.jukeboxEntries,p); }
        ImGui::SameLine(0, ioGap);
        if (!m_renderReadOnly && ImGui::Button("Import##juke", ImVec2(importBtnW, 0))) { std::string p=OpenTsvOpenDialog(); if(!p.empty()){auto imp=ImportJukeboxListTsv(p);if(!imp.empty())bin.jukeboxEntries=std::move(imp);} }
        ImGui::SameLine(0, ioGap);
        if (!m_renderReadOnly && ImGui::Button("+ Add Entry", ImVec2(addBtnW, 0)))
            bin.jukeboxEntries.push_back(JukeboxEntry{});
    }

    ImGui::Separator();

    constexpr ImGuiTableFlags tFlags =
        ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg |
        ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersInnerV |
        ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable |
        ImGuiTableFlags_Hideable | ImGuiTableFlags_SizingFixedFit;

    // cols: #, + all 9 fields
    if (!ImGui::BeginTable("##JukeTable", 10, tFlags, ImGui::GetContentRegionAvail()))
        return;

    ImGui::TableSetupScrollFreeze(1, 1);
    ImGui::TableSetupColumn("#",                           ImGuiTableColumnFlags_WidthFixed, ColumnWidths::kRowCtrl);
    for (int fi = 0; fi < 9; ++fi)
        ImGui::TableSetupColumn(FieldNames::JukeboxEntry[fi], ImGuiTableColumnFlags_WidthFixed, ColumnWidths::kJukebox[fi]);
    ImGui::TableHeadersRow();

    int deleteIdx = -1;
    ImGuiListClipper clipper;
    clipper.Begin((int)bin.jukeboxEntries.size());
    while (clipper.Step())
    {
        for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i)
        {
            auto& e = bin.jukeboxEntries[i];
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
            if (!m_renderReadOnly && ImGui::SmallButton("X")) deleteIdx = i;
            ImGui::PopStyleColor(3);

            auto U32Cell = [](const char* id, uint32_t& v) {
                ImGui::SetNextItemWidth(-FLT_MIN);
                ImGui::InputScalar(id, ImGuiDataType_U32, &v);
            };
            auto StrCell = [](const char* id, char* buf, size_t sz) {
                ImGui::SetNextItemWidth(-FLT_MIN);
                ImGui::InputText(id, buf, sz);
            };

            ImGui::TableSetColumnIndex(1); U32Cell("##bh",   e.bgm_hash);
            ImGui::TableSetColumnIndex(2); U32Cell("##sh",   e.series_hash);
            ImGui::TableSetColumnIndex(3); U32Cell("##u2",   e.unk_2);
            ImGui::TableSetColumnIndex(4); StrCell("##cn",   e.cue_name,        sizeof(e.cue_name));
            ImGui::TableSetColumnIndex(5); StrCell("##arr",  e.arrangement,     sizeof(e.arrangement));
            ImGui::TableSetColumnIndex(6); StrCell("##ac1",  e.alt_cue_name_1,  sizeof(e.alt_cue_name_1));
            ImGui::TableSetColumnIndex(7); StrCell("##ac2",  e.alt_cue_name_2,  sizeof(e.alt_cue_name_2));
            ImGui::TableSetColumnIndex(8); StrCell("##ac3",  e.alt_cue_name_3,  sizeof(e.alt_cue_name_3));
            ImGui::TableSetColumnIndex(9); StrCell("##dtk",  e.display_text_key,sizeof(e.display_text_key));

            ImGui::PopID();
        }
    }
    if (!m_renderReadOnly && deleteIdx >= 0)
        bin.jukeboxEntries.erase(bin.jukeboxEntries.begin() + deleteIdx);

    ImGui::EndTable();
}

// -----------------------------------------------------------------------------
//  series_list TSV export / import + editor
// -----------------------------------------------------------------------------

static void ExportSeriesListTsv(const std::vector<SeriesEntry>& entries, const std::string& path)
{
    FILE* f=nullptr; fopen_s(&f,path.c_str(),"wb"); if(!f) return;
    for (const auto& e : entries)
    {
        char line[2048]; int n=snprintf(line,sizeof(line),"%u\t%s\t%s\t%s\t%s\n",e.series_hash,e.jacket_text_key,e.jacket_icon_key,e.logo_text_key,e.logo_icon_key);
        fwrite(line,1,n,f);
    }
    fclose(f);
}
static std::vector<SeriesEntry> ImportSeriesListTsv(const std::string& path)
{
    std::vector<SeriesEntry> result; FILE* f=nullptr; fopen_s(&f,path.c_str(),"rb"); if(!f) return result;
    char line[2048];
    while (fgets(line, sizeof(line), f))
    {
        int len=(int)strlen(line); while(len>0&&(line[len-1]=='\r'||line[len-1]=='\n'))line[--len]='\0'; if(!len) continue;
        char* cols[5]={}; int col=0; char* p=line; cols[col++]=p; for(;*p&&col<5;++p) if(*p=='\t'){*p='\0';cols[col++]=p+1;} if(col<5) continue;
        SeriesEntry e; e.series_hash=(uint32_t)strtoul(cols[0],nullptr,10);
        strncpy_s(e.jacket_text_key,cols[1],_TRUNCATE); strncpy_s(e.jacket_icon_key,cols[2],_TRUNCATE);
        strncpy_s(e.logo_text_key,cols[3],_TRUNCATE); strncpy_s(e.logo_icon_key,cols[4],_TRUNCATE);
        result.push_back(e);
    }
    fclose(f); return result;
}

void FbsDataView::RenderSeriesListEditor(ContentsBinData& bin)
{
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.65f, 0.82f, 1.00f, 1.00f));
    ImGui::Text("series_list.bin");
    ImGui::PopStyleColor();
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.42f, 0.42f, 0.54f, 1.00f));
    ImGui::Text("(%d entries)", (int)bin.seriesEntries.size());
    ImGui::PopStyleColor();

    {
        const float addBtnW=100.0f, ioGap=4.0f, exportBtnW=70.0f, importBtnW=70.0f;
        const float totalW=exportBtnW+ioGap+importBtnW+ioGap+addBtnW;
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - totalW + ImGui::GetCursorPosX());
        if (ImGui::Button("Export##series", ImVec2(exportBtnW, 0))) { std::string p=OpenTsvSaveDialog(L"series_list.tsv"); if(!p.empty()) ExportSeriesListTsv(bin.seriesEntries,p); }
        ImGui::SameLine(0, ioGap);
        if (!m_renderReadOnly && ImGui::Button("Import##series", ImVec2(importBtnW, 0))) { std::string p=OpenTsvOpenDialog(); if(!p.empty()){auto imp=ImportSeriesListTsv(p);if(!imp.empty())bin.seriesEntries=std::move(imp);} }
        ImGui::SameLine(0, ioGap);
        if (!m_renderReadOnly && ImGui::Button("+ Add Entry", ImVec2(addBtnW, 0)))
            bin.seriesEntries.push_back(SeriesEntry{});
    }

    ImGui::Separator();

    constexpr ImGuiTableFlags tFlags =
        ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg |
        ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersInnerV |
        ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable |
        ImGuiTableFlags_Hideable | ImGuiTableFlags_SizingFixedFit;

    if (!ImGui::BeginTable("##SeriesTable", 6, tFlags, ImGui::GetContentRegionAvail()))
        return;

    ImGui::TableSetupScrollFreeze(1, 1);
    ImGui::TableSetupColumn("#",                           ImGuiTableColumnFlags_WidthFixed, ColumnWidths::kRowCtrl);
    for (int fi = 0; fi < 5; ++fi)
        ImGui::TableSetupColumn(FieldNames::SeriesEntry[fi], ImGuiTableColumnFlags_WidthFixed, ColumnWidths::kSeries[fi]);
    ImGui::TableHeadersRow();

    int deleteIdx = -1;
    ImGuiListClipper clipper;
    clipper.Begin((int)bin.seriesEntries.size());
    while (clipper.Step())
    {
        for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i)
        {
            auto& e = bin.seriesEntries[i];
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
            if (!m_renderReadOnly && ImGui::SmallButton("X")) deleteIdx = i;
            ImGui::PopStyleColor(3);

            auto U32Cell = [](const char* id, uint32_t& v) {
                ImGui::SetNextItemWidth(-FLT_MIN);
                ImGui::InputScalar(id, ImGuiDataType_U32, &v);
            };
            auto StrCell = [](const char* id, char* buf, size_t sz) {
                ImGui::SetNextItemWidth(-FLT_MIN);
                ImGui::InputText(id, buf, sz);
            };

            ImGui::TableSetColumnIndex(1); U32Cell("##sh",  e.series_hash);
            ImGui::TableSetColumnIndex(2); StrCell("##jtk", e.jacket_text_key, sizeof(e.jacket_text_key));
            ImGui::TableSetColumnIndex(3); StrCell("##jik", e.jacket_icon_key, sizeof(e.jacket_icon_key));
            ImGui::TableSetColumnIndex(4); StrCell("##ltk", e.logo_text_key,   sizeof(e.logo_text_key));
            ImGui::TableSetColumnIndex(5); StrCell("##lik", e.logo_icon_key,   sizeof(e.logo_icon_key));

            ImGui::PopID();
        }
    }
    if (!m_renderReadOnly && deleteIdx >= 0)
        bin.seriesEntries.erase(bin.seriesEntries.begin() + deleteIdx);

    ImGui::EndTable();
}

// -----------------------------------------------------------------------------
//  tam_mission_list TSV export / import + editor
// -----------------------------------------------------------------------------

static void ExportTamMissionListTsv(const std::vector<TamMissionEntry>& entries, const std::string& path)
{
    FILE* f=nullptr; fopen_s(&f,path.c_str(),"wb"); if(!f) return;
    for (const auto& e : entries)
    {
        char line[1024]; int n=snprintf(line,sizeof(line),"%u\t%u\t%u\t%s\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\n",
            e.mission_id,e.value_1,e.value_2,e.location,e.hash_0,e.hash_1,e.hash_2,e.hash_3,e.hash_4,e.value_9,e.value_10,e.value_11);
        fwrite(line,1,n,f);
    }
    fclose(f);
}
static std::vector<TamMissionEntry> ImportTamMissionListTsv(const std::string& path)
{
    std::vector<TamMissionEntry> result; FILE* f=nullptr; fopen_s(&f,path.c_str(),"rb"); if(!f) return result;
    char line[1024];
    while (fgets(line, sizeof(line), f))
    {
        int len=(int)strlen(line); while(len>0&&(line[len-1]=='\r'||line[len-1]=='\n'))line[--len]='\0'; if(!len) continue;
        char* cols[12]={}; int col=0; char* p=line; cols[col++]=p; for(;*p&&col<12;++p) if(*p=='\t'){*p='\0';cols[col++]=p+1;} if(col<12) continue;
        TamMissionEntry e;
        e.mission_id=(uint32_t)strtoul(cols[0],nullptr,10); e.value_1=(uint32_t)strtoul(cols[1],nullptr,10); e.value_2=(uint32_t)strtoul(cols[2],nullptr,10);
        strncpy_s(e.location,cols[3],_TRUNCATE);
        e.hash_0=(uint32_t)strtoul(cols[4],nullptr,10); e.hash_1=(uint32_t)strtoul(cols[5],nullptr,10); e.hash_2=(uint32_t)strtoul(cols[6],nullptr,10);
        e.hash_3=(uint32_t)strtoul(cols[7],nullptr,10); e.hash_4=(uint32_t)strtoul(cols[8],nullptr,10);
        e.value_9=(uint32_t)strtoul(cols[9],nullptr,10); e.value_10=(uint32_t)strtoul(cols[10],nullptr,10); e.value_11=(uint32_t)strtoul(cols[11],nullptr,10);
        result.push_back(e);
    }
    fclose(f); return result;
}

void FbsDataView::RenderTamMissionListEditor(ContentsBinData& bin)
{
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.65f, 0.82f, 1.00f, 1.00f));
    ImGui::Text("tam_mission_list.bin");
    ImGui::PopStyleColor();
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.42f, 0.42f, 0.54f, 1.00f));
    ImGui::Text("(%d entries)", (int)bin.tamMissionEntries.size());
    ImGui::PopStyleColor();

    {
        const float addBtnW=100.0f, ioGap=4.0f, exportBtnW=70.0f, importBtnW=70.0f;
        const float totalW=exportBtnW+ioGap+importBtnW+ioGap+addBtnW;
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - totalW + ImGui::GetCursorPosX());
        if (ImGui::Button("Export##tam", ImVec2(exportBtnW, 0))) { std::string p=OpenTsvSaveDialog(L"tam_mission_list.tsv"); if(!p.empty()) ExportTamMissionListTsv(bin.tamMissionEntries,p); }
        ImGui::SameLine(0, ioGap);
        if (!m_renderReadOnly && ImGui::Button("Import##tam", ImVec2(importBtnW, 0))) { std::string p=OpenTsvOpenDialog(); if(!p.empty()){auto imp=ImportTamMissionListTsv(p);if(!imp.empty())bin.tamMissionEntries=std::move(imp);} }
        ImGui::SameLine(0, ioGap);
        if (!m_renderReadOnly && ImGui::Button("+ Add Entry", ImVec2(addBtnW, 0)))
            bin.tamMissionEntries.push_back(TamMissionEntry{});
    }

    ImGui::Separator();

    constexpr ImGuiTableFlags tFlags =
        ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg |
        ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersInnerV |
        ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable |
        ImGuiTableFlags_Hideable | ImGuiTableFlags_SizingFixedFit;

    // cols: #, mission_id, value_2, location, hash_0..4
    if (!ImGui::BeginTable("##TamMisTable", 9, tFlags, ImGui::GetContentRegionAvail()))
        return;

    ImGui::TableSetupScrollFreeze(1, 1);
    static const int k_TamIds[] = { 0, 2, 3, 4, 5, 6, 7, 8 };
    ImGui::TableSetupColumn("#",                              ImGuiTableColumnFlags_WidthFixed, ColumnWidths::kRowCtrl);
    for (int ci = 0; ci < 8; ++ci)
        ImGui::TableSetupColumn(FieldNames::TamMissionEntry[k_TamIds[ci]], ImGuiTableColumnFlags_WidthFixed, ColumnWidths::kTamMission[ci]);
    ImGui::TableHeadersRow();

    int deleteIdx = -1;
    ImGuiListClipper clipper;
    clipper.Begin((int)bin.tamMissionEntries.size());
    while (clipper.Step())
    {
        for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i)
        {
            auto& e = bin.tamMissionEntries[i];
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
            if (!m_renderReadOnly && ImGui::SmallButton("X")) deleteIdx = i;
            ImGui::PopStyleColor(3);

            auto U32Cell = [](const char* id, uint32_t& v) {
                ImGui::SetNextItemWidth(-FLT_MIN);
                ImGui::InputScalar(id, ImGuiDataType_U32, &v);
            };
            auto StrCell = [](const char* id, char* buf, size_t sz) {
                ImGui::SetNextItemWidth(-FLT_MIN);
                ImGui::InputText(id, buf, sz);
            };

            ImGui::TableSetColumnIndex(1); U32Cell("##mid", e.mission_id);
            ImGui::TableSetColumnIndex(2); U32Cell("##v2",  e.value_2);
            ImGui::TableSetColumnIndex(3); StrCell("##loc", e.location, sizeof(e.location));
            ImGui::TableSetColumnIndex(4); U32Cell("##h0",  e.hash_0);
            ImGui::TableSetColumnIndex(5); U32Cell("##h1",  e.hash_1);
            ImGui::TableSetColumnIndex(6); U32Cell("##h2",  e.hash_2);
            ImGui::TableSetColumnIndex(7); U32Cell("##h3",  e.hash_3);
            ImGui::TableSetColumnIndex(8); U32Cell("##h4",  e.hash_4);

            ImGui::PopID();
        }
    }
    if (!m_renderReadOnly && deleteIdx >= 0)
        bin.tamMissionEntries.erase(bin.tamMissionEntries.begin() + deleteIdx);

    ImGui::EndTable();
}

// -----------------------------------------------------------------------------
//  drama_player_start_list TSV export / import + editor
// -----------------------------------------------------------------------------

static void ExportDramaPlayerStartListTsv(const std::vector<DramaPlayerStartEntry>& entries, const std::string& path)
{
    FILE* f=nullptr; fopen_s(&f,path.c_str(),"wb"); if(!f) return;
    for (const auto& e : entries)
    {
        char line[4096];
        int n=snprintf(line,sizeof(line),
            "%u\t%u\t%u\t%u\t%u\t%.9g\t%.9g\t%.9g\t%u\t%u\t%.9g\t%u\t%.9g\t%.9g\t%.9g\t%.9g\t%u\t%u\t%.9g\t%.9g"
            "\t%u\t%.9g\t%.9g\t%.9g\t%.9g\t%.9g\t%.9g\t%.9g\t%.9g\t%u\t%u"
            "\t%u\t%.9g\t%.9g\t%.9g\t%.9g\t%.9g\t%.9g\t%.9g\t%.9g\t%u\t%u"
            "\t%u\t%.9g\t%.9g\t%.9g\t%.9g\t%.9g\t%.9g\t%.9g\t%.9g\t%u\t%u"
            "\t%u\t%.9g\t%.9g\t%.9g\t%.9g\t%.9g\t%.9g\t%.9g\t%.9g\t%u\t%u"
            "\t%u\t%.9g\t%.9g\t%.9g\t%.9g\t%.9g\t%.9g\n",
            e.character_hash,e.hash_1,e.index,e.scene_hash,e.config_hash,
            e.unk_float_5,e.pos_x,e.pos_y,e.state_hash,e.unk_9,e.scale,e.ref_hash,
            e.unk_float_12,e.unk_float_13,e.unk_float_14,e.unk_float_15,
            e.unk_16,e.unk_17,e.unk_float_18,e.rate,
            e.blk1_marker,e.blk1_scale,e.blk1_field_22,e.blk1_field_23,e.blk1_field_24,e.blk1_field_25,e.blk1_field_26,e.blk1_field_27,e.blk1_field_28,e.blk1_angle,e.blk1_hash_a,e.blk1_hash_b,
            e.blk2_marker,e.blk2_scale,e.blk2_field_34,e.blk2_field_35,e.blk2_field_36,e.blk2_field_37,e.blk2_field_38,e.blk2_field_39,e.blk2_field_40,e.blk2_angle,e.blk2_hash_a,e.blk2_hash_b,
            e.blk3_marker,e.blk3_scale,e.blk3_field_46,e.blk3_field_47,e.blk3_field_48,e.blk3_field_49,e.blk3_field_50,e.blk3_field_51,e.blk3_field_52,e.blk3_angle,e.blk3_hash_a,e.blk3_hash_b,
            e.end_marker,e.unk_float_57,e.extra_range,e.extra_param_a,e.extra_param_b,e.extra_param_c,e.extra_param_d);
        fwrite(line,1,n,f);
    }
    fclose(f);
}
static std::vector<DramaPlayerStartEntry> ImportDramaPlayerStartListTsv(const std::string& path)
{
    std::vector<DramaPlayerStartEntry> result; FILE* f=nullptr; fopen_s(&f,path.c_str(),"rb"); if(!f) return result;
    char line[4096];
    while (fgets(line, sizeof(line), f))
    {
        int len=(int)strlen(line); while(len>0&&(line[len-1]=='\r'||line[len-1]=='\n'))line[--len]='\0'; if(!len) continue;
        char* cols[63]={}; int col=0; char* p=line; cols[col++]=p; for(;*p&&col<63;++p) if(*p=='\t'){*p='\0';cols[col++]=p+1;} if(col<63) continue;
        DramaPlayerStartEntry e;
        e.character_hash=(uint32_t)strtoul(cols[0],nullptr,10); e.hash_1=(uint32_t)strtoul(cols[1],nullptr,10);
        e.index=(uint32_t)strtoul(cols[2],nullptr,10); e.scene_hash=(uint32_t)strtoul(cols[3],nullptr,10); e.config_hash=(uint32_t)strtoul(cols[4],nullptr,10);
        e.unk_float_5=strtof(cols[5],nullptr); e.pos_x=strtof(cols[6],nullptr); e.pos_y=strtof(cols[7],nullptr);
        e.state_hash=(uint32_t)strtoul(cols[8],nullptr,10); e.unk_9=(uint32_t)strtoul(cols[9],nullptr,10);
        e.scale=strtof(cols[10],nullptr); e.ref_hash=(uint32_t)strtoul(cols[11],nullptr,10);
        e.unk_float_12=strtof(cols[12],nullptr); e.unk_float_13=strtof(cols[13],nullptr); e.unk_float_14=strtof(cols[14],nullptr); e.unk_float_15=strtof(cols[15],nullptr);
        e.unk_16=(uint32_t)strtoul(cols[16],nullptr,10); e.unk_17=(uint32_t)strtoul(cols[17],nullptr,10);
        e.unk_float_18=strtof(cols[18],nullptr); e.rate=strtof(cols[19],nullptr);
        e.blk1_marker=(uint32_t)strtoul(cols[20],nullptr,10); e.blk1_scale=strtof(cols[21],nullptr);
        e.blk1_field_22=strtof(cols[22],nullptr); e.blk1_field_23=strtof(cols[23],nullptr); e.blk1_field_24=strtof(cols[24],nullptr);
        e.blk1_field_25=strtof(cols[25],nullptr); e.blk1_field_26=strtof(cols[26],nullptr); e.blk1_field_27=strtof(cols[27],nullptr); e.blk1_field_28=strtof(cols[28],nullptr);
        e.blk1_angle=strtof(cols[29],nullptr); e.blk1_hash_a=(uint32_t)strtoul(cols[30],nullptr,10); e.blk1_hash_b=(uint32_t)strtoul(cols[31],nullptr,10);
        e.blk2_marker=(uint32_t)strtoul(cols[32],nullptr,10); e.blk2_scale=strtof(cols[33],nullptr);
        e.blk2_field_34=strtof(cols[34],nullptr); e.blk2_field_35=strtof(cols[35],nullptr); e.blk2_field_36=strtof(cols[36],nullptr);
        e.blk2_field_37=strtof(cols[37],nullptr); e.blk2_field_38=strtof(cols[38],nullptr); e.blk2_field_39=strtof(cols[39],nullptr); e.blk2_field_40=strtof(cols[40],nullptr);
        e.blk2_angle=strtof(cols[41],nullptr); e.blk2_hash_a=(uint32_t)strtoul(cols[42],nullptr,10); e.blk2_hash_b=(uint32_t)strtoul(cols[43],nullptr,10);
        e.blk3_marker=(uint32_t)strtoul(cols[44],nullptr,10); e.blk3_scale=strtof(cols[45],nullptr);
        e.blk3_field_46=strtof(cols[46],nullptr); e.blk3_field_47=strtof(cols[47],nullptr); e.blk3_field_48=strtof(cols[48],nullptr);
        e.blk3_field_49=strtof(cols[49],nullptr); e.blk3_field_50=strtof(cols[50],nullptr); e.blk3_field_51=strtof(cols[51],nullptr); e.blk3_field_52=strtof(cols[52],nullptr);
        e.blk3_angle=strtof(cols[53],nullptr); e.blk3_hash_a=(uint32_t)strtoul(cols[54],nullptr,10); e.blk3_hash_b=(uint32_t)strtoul(cols[55],nullptr,10);
        e.end_marker=(uint32_t)strtoul(cols[56],nullptr,10); e.unk_float_57=strtof(cols[57],nullptr); e.extra_range=strtof(cols[58],nullptr);
        e.extra_param_a=strtof(cols[59],nullptr); e.extra_param_b=strtof(cols[60],nullptr); e.extra_param_c=strtof(cols[61],nullptr); e.extra_param_d=strtof(cols[62],nullptr);
        result.push_back(e);
    }
    fclose(f); return result;
}

void FbsDataView::RenderDramaPlayerStartListEditor(ContentsBinData& bin)
{
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.65f, 0.82f, 1.00f, 1.00f));
    ImGui::Text("drama_player_start_list.bin");
    ImGui::PopStyleColor();
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.42f, 0.42f, 0.54f, 1.00f));
    ImGui::Text("(%d entries)", (int)bin.dramaPlayerStartEntries.size());
    ImGui::PopStyleColor();

    {
        const float addBtnW=100.0f, ioGap=4.0f, exportBtnW=70.0f, importBtnW=70.0f;
        const float totalW=exportBtnW+ioGap+importBtnW+ioGap+addBtnW;
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - totalW + ImGui::GetCursorPosX());
        if (ImGui::Button("Export##dps", ImVec2(exportBtnW, 0))) { std::string p=OpenTsvSaveDialog(L"drama_player_start_list.tsv"); if(!p.empty()) ExportDramaPlayerStartListTsv(bin.dramaPlayerStartEntries,p); }
        ImGui::SameLine(0, ioGap);
        if (!m_renderReadOnly && ImGui::Button("Import##dps", ImVec2(importBtnW, 0))) { std::string p=OpenTsvOpenDialog(); if(!p.empty()){auto imp=ImportDramaPlayerStartListTsv(p);if(!imp.empty())bin.dramaPlayerStartEntries=std::move(imp);} }
        ImGui::SameLine(0, ioGap);
        if (!m_renderReadOnly && ImGui::Button("+ Add Entry", ImVec2(addBtnW, 0)))
            bin.dramaPlayerStartEntries.push_back(DramaPlayerStartEntry{});
    }

    ImGui::Separator();

    constexpr ImGuiTableFlags tFlags =
        ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg |
        ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersInnerV |
        ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable |
        ImGuiTableFlags_Hideable | ImGuiTableFlags_SizingFixedFit;

    // cols: #, character_hash, index, scene_hash, config_hash, pos_x, pos_y, state_hash, scale
    if (!ImGui::BeginTable("##DPSTable", 10, tFlags, ImGui::GetContentRegionAvail()))
        return;

    ImGui::TableSetupScrollFreeze(1, 1);
    static const int k_DramaIds[] = { 0, 2, 3, 4, 6, 7, 8, 10 };
    ImGui::TableSetupColumn("#",                              ImGuiTableColumnFlags_WidthFixed, ColumnWidths::kRowCtrl);
    for (int ci = 0; ci < 8; ++ci)
        ImGui::TableSetupColumn(FieldNames::DramaPlayerStart[k_DramaIds[ci]], ImGuiTableColumnFlags_WidthFixed, ColumnWidths::kDramaPlayerStart[ci]);
    ImGui::TableHeadersRow();

    int deleteIdx = -1;
    ImGuiListClipper clipper;
    clipper.Begin((int)bin.dramaPlayerStartEntries.size());
    while (clipper.Step())
    {
        for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i)
        {
            auto& e = bin.dramaPlayerStartEntries[i];
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
            if (!m_renderReadOnly && ImGui::SmallButton("X")) deleteIdx = i;
            ImGui::PopStyleColor(3);

            auto U32Cell = [](const char* id, uint32_t& v) {
                ImGui::SetNextItemWidth(-FLT_MIN);
                ImGui::InputScalar(id, ImGuiDataType_U32, &v);
            };
            auto F32Cell = [](const char* id, float& v) {
                ImGui::SetNextItemWidth(-FLT_MIN);
                ImGui::InputScalar(id, ImGuiDataType_Float, &v);
            };

            ImGui::TableSetColumnIndex(1); U32Cell("##ch",  e.character_hash);
            ImGui::TableSetColumnIndex(2); U32Cell("##idx", e.index);
            ImGui::TableSetColumnIndex(3); U32Cell("##sch", e.scene_hash);
            ImGui::TableSetColumnIndex(4); U32Cell("##cfh", e.config_hash);
            ImGui::TableSetColumnIndex(5); F32Cell("##px",  e.pos_x);
            ImGui::TableSetColumnIndex(6); F32Cell("##py",  e.pos_y);
            ImGui::TableSetColumnIndex(7); U32Cell("##sth", e.state_hash);
            ImGui::TableSetColumnIndex(8); F32Cell("##sc",  e.scale);

            ImGui::PopID();
        }
    }
    if (!m_renderReadOnly && deleteIdx >= 0)
        bin.dramaPlayerStartEntries.erase(bin.dramaPlayerStartEntries.begin() + deleteIdx);

    ImGui::EndTable();
}

// -----------------------------------------------------------------------------
//  stage_list editor
// -----------------------------------------------------------------------------

namespace stage_list_helpers {

// Polaris loader assigns custom slots from this pool at runtime; mirror the
// constants in PolarisTkdataModLoader/src/patchers/patcher_stage_newid.h.
constexpr uint32_t kCustomSlotBase = 48;
constexpr uint32_t kCustomSlotMax  = 104;
constexpr size_t   kTableCap       = 64;

// Vanilla stage_hash set (from docs/adding-custom-stage.md §7). Used to warn
// authors when their picked hash collides with a baked switch arm -- collision
// causes the parser to dispatch to the vanilla slot instead of the cave's
// runtime-assigned slot, which usually isn't what the author wants.
constexpr uint32_t kVanillaStageHashes[] = {
    0x78CFEB4Du, 0xDE1FC218u, 0xFDD19224u, 0xA0E2698Cu, 0xB808ABDFu,
    0xB49F515Au, 0xF2215EA2u, 0xD1DBD9A8u, 0x224C2FE4u, 0x78AF4AE6u,
    0x64FD9798u, 0xE09217F5u, 0x583EA215u, 0x73D58236u, 0x100CB6F6u,
    0xE924EC09u, 0xF2DB49F9u, 0x56CCF333u, 0xD0AAEDB0u, 0x4A12EE31u,
    0x3A8582C9u, 0xFF34065Au, 0x5A071E31u, 0xA1E006A5u, 0xEE1507BFu,
    0xA88884F5u, 0x9E981F8Cu, 0x06846A06u, 0xABD68CFFu, 0x647D12BDu,
    0x20FD3C7Cu, 0xEF2B1D63u, 0xD7B648E3u, 0x4FFE2A89u, 0xF35952D4u,
    0x01386500u, 0x7E4EDBF1u, 0x4D77B922u, 0x0407CD58u, 0x6D5FBB9Au,
    0x551D7463u, 0x04FC39CCu, 0x532AB637u, 0x9933F988u, 0x961B9D87u,
    0x5777900Du, 0x29DE2FCEu, 0xDFA5F8CDu,
};

static bool IsVanillaStageHash(uint32_t h)
{
    for (uint32_t v : kVanillaStageHashes) if (v == h) return true;
    return false;
}

static uint32_t Crc32Bytes(const uint8_t* data, size_t len)
{
    static uint32_t s_table[256];
    static bool     s_init = false;
    if (!s_init)
    {
        for (uint32_t i = 0; i < 256; ++i)
        {
            uint32_t c = i;
            for (int j = 0; j < 8; ++j)
                c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
            s_table[i] = c;
        }
        s_init = true;
    }
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < len; ++i)
        crc = s_table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    return crc ^ 0xFFFFFFFFu;
}

// Stable hash from stage_code: CRC32 of the string. If the result lands on a
// vanilla hash (vanishingly unlikely but possible), flip the top bit so the
// loader's switch-default path catches it instead of a vanilla switch arm.
static uint32_t AutoStageHash(const char* stageCode)
{
    if (!stageCode || !stageCode[0]) return 0;
    uint32_t h = Crc32Bytes(reinterpret_cast<const uint8_t*>(stageCode),
                            std::strlen(stageCode));
    if (h == 0) h = 0xDEAD0001u;
    if (IsVanillaStageHash(h)) h ^= 0x80000000u;
    return h;
}

} // namespace stage_list_helpers

static void ExportStageListTsv(const std::vector<StageEntry>& entries, const std::string& path)
{
    FILE* f=nullptr; fopen_s(&f,path.c_str(),"wb"); if(!f) return;
    for (const auto& e : entries)
    {
        char line[4096];
        int n=snprintf(line,sizeof(line),
            "%s\t%u\t%s\t%.9g\t%u\t%u\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%u\t%u\t%u\t%u\t%u\t%s\t%u\t%s\t%s\t%s\t%s\t%u\t%u\t%u\t%u\t%s\n",
            e.stage_code,e.stage_hash,e.is_selectable?"TRUE":"FALSE",e.camera_offset,e.parent_stage_index,e.variant_hash,
            e.has_weather?"TRUE":"FALSE",e.is_active?"TRUE":"FALSE",e.flag_interlocked?"TRUE":"FALSE",e.flag_ocean?"TRUE":"FALSE",
            e.flag_10?"TRUE":"FALSE",e.flag_infinite?"TRUE":"FALSE",e.flag_battle?"TRUE":"FALSE",e.flag_13?"TRUE":"FALSE",
            e.flag_balcony?"TRUE":"FALSE",e.flag_15?"TRUE":"FALSE",e.reserved_16?"TRUE":"FALSE",e.is_online_enabled?"TRUE":"FALSE",
            e.is_ranked_enabled?"TRUE":"FALSE",e.reserved_19?"TRUE":"FALSE",e.reserved_20?"TRUE":"FALSE",
            e.arena_width,e.arena_depth,e.reserved_23,e.arena_param,e.extra_width,
            e.extra_group,e.extra_depth,e.group_id,e.stage_name_key,e.level_name,e.sound_bank,
            e.wall_distance_a,e.wall_distance_b,e.stage_mode,e.reserved_35,e.is_default_variant?"TRUE":"FALSE");
        fwrite(line,1,n,f);
    }
    fclose(f);
}
static std::vector<StageEntry> ImportStageListTsv(const std::string& path)
{
    std::vector<StageEntry> result; FILE* f=nullptr; fopen_s(&f,path.c_str(),"rb"); if(!f) return result;
    char line[4096];
    while (fgets(line, sizeof(line), f))
    {
        int len=(int)strlen(line); while(len>0&&(line[len-1]=='\r'||line[len-1]=='\n'))line[--len]='\0'; if(!len) continue;
        char* cols[37]={}; int col=0; char* p=line; cols[col++]=p; for(;*p&&col<37;++p) if(*p=='\t'){*p='\0';cols[col++]=p+1;} if(col<37) continue;
        StageEntry e;
        strncpy_s(e.stage_code,cols[0],_TRUNCATE); e.stage_hash=(uint32_t)strtoul(cols[1],nullptr,10);
        e.is_selectable=ParseBool(cols[2]); e.camera_offset=strtof(cols[3],nullptr);
        e.parent_stage_index=(uint32_t)strtoul(cols[4],nullptr,10); e.variant_hash=(uint32_t)strtoul(cols[5],nullptr,10);
        e.has_weather=ParseBool(cols[6]); e.is_active=ParseBool(cols[7]); e.flag_interlocked=ParseBool(cols[8]);
        e.flag_ocean=ParseBool(cols[9]); e.flag_10=ParseBool(cols[10]); e.flag_infinite=ParseBool(cols[11]);
        e.flag_battle=ParseBool(cols[12]); e.flag_13=ParseBool(cols[13]); e.flag_balcony=ParseBool(cols[14]);
        e.flag_15=ParseBool(cols[15]); e.reserved_16=ParseBool(cols[16]); e.is_online_enabled=ParseBool(cols[17]);
        e.is_ranked_enabled=ParseBool(cols[18]); e.reserved_19=ParseBool(cols[19]); e.reserved_20=ParseBool(cols[20]);
        e.arena_width=(uint32_t)strtoul(cols[21],nullptr,10); e.arena_depth=(uint32_t)strtoul(cols[22],nullptr,10);
        e.reserved_23=(uint32_t)strtoul(cols[23],nullptr,10); e.arena_param=(uint32_t)strtoul(cols[24],nullptr,10);
        e.extra_width=(uint32_t)strtoul(cols[25],nullptr,10); strncpy_s(e.extra_group,cols[26],_TRUNCATE);
        e.extra_depth=(uint32_t)strtoul(cols[27],nullptr,10); strncpy_s(e.group_id,cols[28],_TRUNCATE);
        strncpy_s(e.stage_name_key,cols[29],_TRUNCATE); strncpy_s(e.level_name,cols[30],_TRUNCATE); strncpy_s(e.sound_bank,cols[31],_TRUNCATE);
        e.wall_distance_a=(uint32_t)strtoul(cols[32],nullptr,10); e.wall_distance_b=(uint32_t)strtoul(cols[33],nullptr,10);
        e.stage_mode=(uint32_t)strtoul(cols[34],nullptr,10); e.reserved_35=(uint32_t)strtoul(cols[35],nullptr,10);
        e.is_default_variant=ParseBool(cols[36]);
        result.push_back(e);
    }
    fclose(f); return result;
}

void FbsDataView::RenderStageListEditor(ContentsBinData& bin)
{
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.65f, 0.82f, 1.00f, 1.00f));
    ImGui::Text("stage_list.bin");
    ImGui::PopStyleColor();
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.42f, 0.42f, 0.54f, 1.00f));
    ImGui::Text("(%d entries)", (int)bin.stageEntries.size());
    ImGui::PopStyleColor();

    {
        const float addBtnW=100.0f, ioGap=4.0f, exportBtnW=70.0f, importBtnW=70.0f;
        const float totalW=exportBtnW+ioGap+importBtnW+ioGap+addBtnW;
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - totalW + ImGui::GetCursorPosX());
        if (ImGui::Button("Export##stage", ImVec2(exportBtnW, 0))) { std::string p=OpenTsvSaveDialog(L"stage_list.tsv"); if(!p.empty()) ExportStageListTsv(bin.stageEntries,p); }
        ImGui::SameLine(0, ioGap);
        if (!m_renderReadOnly && ImGui::Button("Import##stage", ImVec2(importBtnW, 0))) { std::string p=OpenTsvOpenDialog(); if(!p.empty()){auto imp=ImportStageListTsv(p);if(!imp.empty())bin.stageEntries=std::move(imp);} }
        ImGui::SameLine(0, ioGap);
        if (!m_renderReadOnly && ImGui::Button("+ Add Entry", ImVec2(addBtnW, 0)))
            bin.stageEntries.push_back(StageEntry{});
    }

    // Authoring note: the loader's stage-id cave is now table-driven. Authors
    // pick any unique stage_hash and the loader maps it to a free slot in
    // [48..104) at merge time. Up to 64 custom stages may coexist. Hashes that
    // collide with a vanilla switch arm dispatch to the vanilla slot instead.
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.62f, 0.72f, 1.00f));
    ImGui::TextWrapped(
        "stage_hash: pick any unique 32-bit value. Loader auto-assigns slot at runtime "
        "(pool %u..%u, cap %zu). Use the 'Auto' button to derive a stable hash from stage_code.",
        stage_list_helpers::kCustomSlotBase,
        stage_list_helpers::kCustomSlotMax - 1,
        stage_list_helpers::kTableCap);
    ImGui::PopStyleColor();

    ImGui::Separator();

    constexpr ImGuiTableFlags tFlags =
        ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg |
        ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersInnerV |
        ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable |
        ImGuiTableFlags_Hideable | ImGuiTableFlags_SizingFixedFit;

    constexpr int kStageFieldCount = FieldNames::StageEntryCount;

    if (!ImGui::BeginTable("##StageTable", 1 + kStageFieldCount, tFlags, ImGui::GetContentRegionAvail()))
        return;

    ImGui::TableSetupScrollFreeze(1, 1);
    ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, ColumnWidths::kRowCtrl);
    for (int fi = 0; fi < kStageFieldCount; ++fi)
        ImGui::TableSetupColumn(FieldNames::StageEntry[fi], ImGuiTableColumnFlags_WidthFixed,
                                ColumnWidths::kStage[fi]);
    ImGui::TableHeadersRow();

    int deleteIdx = -1;
    int duplicateIdx = -1;
    int pasteIdx = -1;
    static StageEntry s_clipboard;
    static bool       s_clipboardValid = false;

    ImGuiListClipper clipper;
    clipper.Begin((int)bin.stageEntries.size());
    while (clipper.Step())
    {
        for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i)
        {
            auto& e = bin.stageEntries[i];
            ImGui::TableNextRow();
            ImGui::PushID(i);

            ImGui::TableSetColumnIndex(0);
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.50f, 0.50f, 0.60f, 1.00f));
            ImGui::Text("%d", i + 1);
            ImGui::PopStyleColor();
            // Right-click on the row index opens a context menu (copy / paste /
            // duplicate / delete). Anchored to the # cell so right-clicking inside
            // an input field still goes to the input, not this menu.
            if (ImGui::BeginPopupContextItem("##stageRowCtx"))
            {
                if (ImGui::MenuItem("Copy"))
                {
                    s_clipboard = e;
                    s_clipboardValid = true;
                }
                if (ImGui::MenuItem("Paste", nullptr, false, s_clipboardValid))
                    pasteIdx = i;
                if (ImGui::MenuItem("Duplicate (insert below)"))
                    duplicateIdx = i;
                ImGui::Separator();
                if (ImGui::MenuItem("Delete"))
                    deleteIdx = i;
                ImGui::EndPopup();
            }
            ImGui::SameLine(0, 4.0f);
            ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.55f, 0.12f, 0.12f, 1.00f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.78f, 0.18f, 0.18f, 1.00f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.65f, 0.15f, 0.15f, 1.00f));
            if (!m_renderReadOnly && ImGui::SmallButton("X")) deleteIdx = i;
            ImGui::PopStyleColor(3);

            auto U32Cell = [](const char* id, uint32_t& v) {
                ImGui::SetNextItemWidth(-FLT_MIN);
                ImGui::InputScalar(id, ImGuiDataType_U32, &v);
            };
            auto F32Cell = [](const char* id, float& v) {
                ImGui::SetNextItemWidth(-FLT_MIN);
                ImGui::InputScalar(id, ImGuiDataType_Float, &v);
            };
            auto StrCell = [](const char* id, char* buf, size_t sz) {
                ImGui::SetNextItemWidth(-FLT_MIN);
                ImGui::InputText(id, buf, sz);
            };
            auto BoolCell = [](const char* id, bool& v) {
                ImGui::Checkbox(id, &v);
            };

            for (int fid = 0; fid < kStageFieldCount; ++fid)
            {
                ImGui::TableSetColumnIndex(1 + fid);
                char lbl[24];
                snprintf(lbl, sizeof(lbl), "##f%d", fid);
                switch (fid)
                {
                case 0: StrCell(lbl, e.stage_code, sizeof(e.stage_code)); break;
                case 1: {
                    // Input + small "Auto" button on the same row. Tooltip shows
                    // assigned-slot context and warns on vanilla collision.
                    const float btnW = 38.0f;
                    const float spacing = ImGui::GetStyle().ItemInnerSpacing.x;
                    float availW = ImGui::GetContentRegionAvail().x;
                    ImGui::SetNextItemWidth(availW - btnW - spacing);
                    ImGui::InputScalar(lbl, ImGuiDataType_U32, &e.stage_hash);
                    if (ImGui::IsItemHovered())
                    {
                        ImGui::BeginTooltip();
                        if (e.stage_hash == 0)
                            ImGui::TextUnformatted("0 = parser drops this entry. Set a non-zero value.");
                        else if (stage_list_helpers::IsVanillaStageHash(e.stage_hash))
                            ImGui::TextColored(ImVec4(1.0f, 0.55f, 0.45f, 1.0f),
                                "Collides with a vanilla stage hash -- parser will dispatch\n"
                                "to the baked slot, ignoring the loader's runtime table.\n"
                                "Use 'Auto' or pick a different value to route via the cave.");
                        else
                            ImGui::TextUnformatted(
                                "Loader assigns a slot from pool [48..104) at merge time.\n"
                                "Hash must be unique across all installed .tkmods.");
                        ImGui::EndTooltip();
                    }
                    ImGui::SameLine(0, spacing);
                    char btnId[24];
                    snprintf(btnId, sizeof(btnId), "Auto##h%d", i);
                    if (ImGui::Button(btnId, ImVec2(btnW, 0)))
                        e.stage_hash = stage_list_helpers::AutoStageHash(e.stage_code);
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("Generate stage_hash = CRC32(stage_code).\n"
                                          "Stable across rebuilds; collisions across mods are\n"
                                          "the author's responsibility.");
                    break;
                }
                case 2: BoolCell(lbl, e.is_selectable); break;
                case 3: F32Cell(lbl, e.camera_offset); break;
                case 4: U32Cell(lbl, e.parent_stage_index); break;
                case 5: U32Cell(lbl, e.variant_hash); break;
                case 6: BoolCell(lbl, e.has_weather); break;
                case 7: BoolCell(lbl, e.is_active); break;
                case 8: BoolCell(lbl, e.flag_interlocked); break;
                case 9: BoolCell(lbl, e.flag_ocean); break;
                case 10: BoolCell(lbl, e.flag_10); break;
                case 11: BoolCell(lbl, e.flag_infinite); break;
                case 12: BoolCell(lbl, e.flag_battle); break;
                case 13: BoolCell(lbl, e.flag_13); break;
                case 14: BoolCell(lbl, e.flag_balcony); break;
                case 15: BoolCell(lbl, e.flag_15); break;
                case 16: BoolCell(lbl, e.reserved_16); break;
                case 17: BoolCell(lbl, e.is_online_enabled); break;
                case 18: BoolCell(lbl, e.is_ranked_enabled); break;
                case 19: BoolCell(lbl, e.reserved_19); break;
                case 20: BoolCell(lbl, e.reserved_20); break;
                case 21: U32Cell(lbl, e.arena_width); break;
                case 22: U32Cell(lbl, e.arena_depth); break;
                case 23: U32Cell(lbl, e.reserved_23); break;
                case 24: U32Cell(lbl, e.arena_param); break;
                case 25: U32Cell(lbl, e.extra_width); break;
                case 26: StrCell(lbl, e.extra_group, sizeof(e.extra_group)); break;
                case 27: U32Cell(lbl, e.extra_depth); break;
                case 28: StrCell(lbl, e.group_id, sizeof(e.group_id)); break;
                case 29: StrCell(lbl, e.stage_name_key, sizeof(e.stage_name_key)); break;
                case 30: StrCell(lbl, e.level_name, sizeof(e.level_name)); break;
                case 31: StrCell(lbl, e.sound_bank, sizeof(e.sound_bank)); break;
                case 32: U32Cell(lbl, e.wall_distance_a); break;
                case 33: U32Cell(lbl, e.wall_distance_b); break;
                case 34: U32Cell(lbl, e.stage_mode); break;
                case 35: U32Cell(lbl, e.reserved_35); break;
                case 36: BoolCell(lbl, e.is_default_variant); break;
                default: break;
                }
            }

            ImGui::PopID();
        }
    }
    // Apply mutations after the render pass to keep iterator/index validity.
    // Order: paste (in place) -> duplicate (insert) -> delete.
    if (pasteIdx >= 0 && s_clipboardValid &&
        pasteIdx < (int)bin.stageEntries.size())
    {
        bin.stageEntries[pasteIdx] = s_clipboard;
    }
    if (duplicateIdx >= 0 && duplicateIdx < (int)bin.stageEntries.size())
    {
        StageEntry copy = bin.stageEntries[duplicateIdx];
        bin.stageEntries.insert(bin.stageEntries.begin() + duplicateIdx + 1, copy);
    }
    if (!m_renderReadOnly && deleteIdx >= 0)
        bin.stageEntries.erase(bin.stageEntries.begin() + deleteIdx);

    ImGui::EndTable();
}

// -----------------------------------------------------------------------------
//  ball_property_list TSV export / import + editor
// -----------------------------------------------------------------------------

static void ExportBallPropertyListTsv(const std::vector<BallPropertyEntry>& entries, const std::string& path)
{
    FILE* f=nullptr; fopen_s(&f,path.c_str(),"wb"); if(!f) return;
    for (const auto& e : entries)
    {
        char line[2048];
        int n=snprintf(line,sizeof(line),"%u\t%s\t%s\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%.9g\t%.9g\t%.9g\t%.9g\t%.9g\t%.9g\t%.9g\t%.9g\t%.9g\n",
            e.ball_hash,e.ball_code,e.effect_name,e.hash_3,e.hash_4,e.unk_5,e.unk_6,e.hash_7,e.item_no,e.rarity,
            e.value_10,e.value_11,e.value_12,e.value_13,e.value_14,e.value_15,e.value_16,e.value_17,e.value_18);
        fwrite(line,1,n,f);
    }
    fclose(f);
}
static std::vector<BallPropertyEntry> ImportBallPropertyListTsv(const std::string& path)
{
    std::vector<BallPropertyEntry> result; FILE* f=nullptr; fopen_s(&f,path.c_str(),"rb"); if(!f) return result;
    char line[2048];
    while (fgets(line, sizeof(line), f))
    {
        int len=(int)strlen(line); while(len>0&&(line[len-1]=='\r'||line[len-1]=='\n'))line[--len]='\0'; if(!len) continue;
        char* cols[19]={}; int col=0; char* p=line; cols[col++]=p; for(;*p&&col<19;++p) if(*p=='\t'){*p='\0';cols[col++]=p+1;} if(col<19) continue;
        BallPropertyEntry e;
        e.ball_hash=(uint32_t)strtoul(cols[0],nullptr,10); strncpy_s(e.ball_code,cols[1],_TRUNCATE); strncpy_s(e.effect_name,cols[2],_TRUNCATE);
        e.hash_3=(uint32_t)strtoul(cols[3],nullptr,10); e.hash_4=(uint32_t)strtoul(cols[4],nullptr,10);
        e.unk_5=(uint32_t)strtoul(cols[5],nullptr,10); e.unk_6=(uint32_t)strtoul(cols[6],nullptr,10);
        e.hash_7=(uint32_t)strtoul(cols[7],nullptr,10); e.item_no=(uint32_t)strtoul(cols[8],nullptr,10); e.rarity=(uint32_t)strtoul(cols[9],nullptr,10);
        e.value_10=strtof(cols[10],nullptr); e.value_11=strtof(cols[11],nullptr); e.value_12=strtof(cols[12],nullptr);
        e.value_13=strtof(cols[13],nullptr); e.value_14=strtof(cols[14],nullptr); e.value_15=strtof(cols[15],nullptr);
        e.value_16=strtof(cols[16],nullptr); e.value_17=strtof(cols[17],nullptr); e.value_18=strtof(cols[18],nullptr);
        result.push_back(e);
    }
    fclose(f); return result;
}

void FbsDataView::RenderBallPropertyListEditor(ContentsBinData& bin)
{
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.65f, 0.82f, 1.00f, 1.00f));
    ImGui::Text("ball_property_list.bin");
    ImGui::PopStyleColor();
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.42f, 0.42f, 0.54f, 1.00f));
    ImGui::Text("(%d entries)", (int)bin.ballPropertyEntries.size());
    ImGui::PopStyleColor();

    {
        const float addBtnW=100.0f, ioGap=4.0f, exportBtnW=70.0f, importBtnW=70.0f;
        const float totalW=exportBtnW+ioGap+importBtnW+ioGap+addBtnW;
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - totalW + ImGui::GetCursorPosX());
        if (ImGui::Button("Export##ballp", ImVec2(exportBtnW, 0))) { std::string p=OpenTsvSaveDialog(L"ball_property_list.tsv"); if(!p.empty()) ExportBallPropertyListTsv(bin.ballPropertyEntries,p); }
        ImGui::SameLine(0, ioGap);
        if (!m_renderReadOnly && ImGui::Button("Import##ballp", ImVec2(importBtnW, 0))) { std::string p=OpenTsvOpenDialog(); if(!p.empty()){auto imp=ImportBallPropertyListTsv(p);if(!imp.empty())bin.ballPropertyEntries=std::move(imp);} }
        ImGui::SameLine(0, ioGap);
        if (!m_renderReadOnly && ImGui::Button("+ Add Entry", ImVec2(addBtnW, 0)))
            bin.ballPropertyEntries.push_back(BallPropertyEntry{});
    }

    ImGui::Separator();

    constexpr ImGuiTableFlags tFlags =
        ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg |
        ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersInnerV |
        ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable |
        ImGuiTableFlags_Hideable | ImGuiTableFlags_SizingFixedFit;

    // cols: #, ball_hash, ball_code, effect_name, item_no, rarity, value_10..14
    if (!ImGui::BeginTable("##BallPropTable", 10, tFlags, ImGui::GetContentRegionAvail()))
        return;

    ImGui::TableSetupScrollFreeze(1, 1);
    static const int k_BallPropIds[] = { 0, 1, 2, 8, 9, 10, 11, 12, 13 };
    ImGui::TableSetupColumn("#",                                ImGuiTableColumnFlags_WidthFixed, ColumnWidths::kRowCtrl);
    for (int ci = 0; ci < 9; ++ci)
        ImGui::TableSetupColumn(FieldNames::BallPropertyEntry[k_BallPropIds[ci]], ImGuiTableColumnFlags_WidthFixed, ColumnWidths::kBallProperty[ci]);
    ImGui::TableHeadersRow();

    int deleteIdx = -1;
    ImGuiListClipper clipper;
    clipper.Begin((int)bin.ballPropertyEntries.size());
    while (clipper.Step())
    {
        for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i)
        {
            auto& e = bin.ballPropertyEntries[i];
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
            if (!m_renderReadOnly && ImGui::SmallButton("X")) deleteIdx = i;
            ImGui::PopStyleColor(3);

            auto U32Cell = [](const char* id, uint32_t& v) {
                ImGui::SetNextItemWidth(-FLT_MIN);
                ImGui::InputScalar(id, ImGuiDataType_U32, &v);
            };
            auto F32Cell = [](const char* id, float& v) {
                ImGui::SetNextItemWidth(-FLT_MIN);
                ImGui::InputScalar(id, ImGuiDataType_Float, &v);
            };
            auto StrCell = [](const char* id, char* buf, size_t sz) {
                ImGui::SetNextItemWidth(-FLT_MIN);
                ImGui::InputText(id, buf, sz);
            };

            ImGui::TableSetColumnIndex(1); U32Cell("##bh",  e.ball_hash);
            ImGui::TableSetColumnIndex(2); StrCell("##bc",  e.ball_code,   sizeof(e.ball_code));
            ImGui::TableSetColumnIndex(3); StrCell("##en",  e.effect_name, sizeof(e.effect_name));
            ImGui::TableSetColumnIndex(4); U32Cell("##ino", e.item_no);
            ImGui::TableSetColumnIndex(5); U32Cell("##rar", e.rarity);
            ImGui::TableSetColumnIndex(6); F32Cell("##v10", e.value_10);
            ImGui::TableSetColumnIndex(7); F32Cell("##v11", e.value_11);
            ImGui::TableSetColumnIndex(8); F32Cell("##v12", e.value_12);
            ImGui::TableSetColumnIndex(9); F32Cell("##v13", e.value_13);

            ImGui::PopID();
        }
    }
    if (!m_renderReadOnly && deleteIdx >= 0)
        bin.ballPropertyEntries.erase(bin.ballPropertyEntries.begin() + deleteIdx);

    ImGui::EndTable();
}

// -----------------------------------------------------------------------------
//  body_cylinder_data_list TSV export / import + editor
// -----------------------------------------------------------------------------

static void ExportBodyCylinderDataListTsv(const std::vector<BodyCylinderDataEntry>& entries, const std::string& path)
{
    FILE* f=nullptr; fopen_s(&f,path.c_str(),"wb"); if(!f) return;
    for (const auto& e : entries)
    {
        char line[1024];
        int n=snprintf(line,sizeof(line),"%u\t%.9g\t%.9g\t%.9g\t%u\t%u\t%u\t%u\t%.9g\t%.9g\t%.9g\t%u\t%u\t%u\t%u\t%.9g\t%.9g\t%.9g\t%u\n",
            e.character_hash,e.cyl0_radius,e.cyl0_height,e.cyl0_offset_y,e.cyl0_unk_hash,e.unk_5,e.unk_6,e.unk_7,
            e.cyl1_radius,e.cyl1_height,e.cyl1_offset_y,e.cyl1_unk_hash,e.unk_12,e.unk_13,e.unk_14,
            e.cyl2_radius,e.cyl2_height,e.cyl2_offset_y,e.cyl2_unk_hash);
        fwrite(line,1,n,f);
    }
    fclose(f);
}
static std::vector<BodyCylinderDataEntry> ImportBodyCylinderDataListTsv(const std::string& path)
{
    std::vector<BodyCylinderDataEntry> result; FILE* f=nullptr; fopen_s(&f,path.c_str(),"rb"); if(!f) return result;
    char line[1024];
    while (fgets(line, sizeof(line), f))
    {
        int len=(int)strlen(line); while(len>0&&(line[len-1]=='\r'||line[len-1]=='\n'))line[--len]='\0'; if(!len) continue;
        char* cols[19]={}; int col=0; char* p=line; cols[col++]=p; for(;*p&&col<19;++p) if(*p=='\t'){*p='\0';cols[col++]=p+1;} if(col<19) continue;
        BodyCylinderDataEntry e;
        e.character_hash=(uint32_t)strtoul(cols[0],nullptr,10);
        e.cyl0_radius=strtof(cols[1],nullptr); e.cyl0_height=strtof(cols[2],nullptr); e.cyl0_offset_y=strtof(cols[3],nullptr);
        e.cyl0_unk_hash=(uint32_t)strtoul(cols[4],nullptr,10); e.unk_5=(uint32_t)strtoul(cols[5],nullptr,10);
        e.unk_6=(uint32_t)strtoul(cols[6],nullptr,10); e.unk_7=(uint32_t)strtoul(cols[7],nullptr,10);
        e.cyl1_radius=strtof(cols[8],nullptr); e.cyl1_height=strtof(cols[9],nullptr); e.cyl1_offset_y=strtof(cols[10],nullptr);
        e.cyl1_unk_hash=(uint32_t)strtoul(cols[11],nullptr,10); e.unk_12=(uint32_t)strtoul(cols[12],nullptr,10);
        e.unk_13=(uint32_t)strtoul(cols[13],nullptr,10); e.unk_14=(uint32_t)strtoul(cols[14],nullptr,10);
        e.cyl2_radius=strtof(cols[15],nullptr); e.cyl2_height=strtof(cols[16],nullptr); e.cyl2_offset_y=strtof(cols[17],nullptr);
        e.cyl2_unk_hash=(uint32_t)strtoul(cols[18],nullptr,10);
        result.push_back(e);
    }
    fclose(f); return result;
}

void FbsDataView::RenderBodyCylinderDataListEditor(ContentsBinData& bin)
{
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.65f, 0.82f, 1.00f, 1.00f));
    ImGui::Text("body_cylinder_data_list.bin");
    ImGui::PopStyleColor();
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.42f, 0.42f, 0.54f, 1.00f));
    ImGui::Text("(%d entries)", (int)bin.bodyCylinderDataEntries.size());
    ImGui::PopStyleColor();

    {
        const float addBtnW=100.0f, ioGap=4.0f, exportBtnW=70.0f, importBtnW=70.0f;
        const float totalW=exportBtnW+ioGap+importBtnW+ioGap+addBtnW;
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - totalW + ImGui::GetCursorPosX());
        if (ImGui::Button("Export##bcd", ImVec2(exportBtnW, 0))) { std::string p=OpenTsvSaveDialog(L"body_cylinder_data_list.tsv"); if(!p.empty()) ExportBodyCylinderDataListTsv(bin.bodyCylinderDataEntries,p); }
        ImGui::SameLine(0, ioGap);
        if (!m_renderReadOnly && ImGui::Button("Import##bcd", ImVec2(importBtnW, 0))) { std::string p=OpenTsvOpenDialog(); if(!p.empty()){auto imp=ImportBodyCylinderDataListTsv(p);if(!imp.empty())bin.bodyCylinderDataEntries=std::move(imp);} }
        ImGui::SameLine(0, ioGap);
        if (!m_renderReadOnly && ImGui::Button("+ Add Entry", ImVec2(addBtnW, 0)))
            bin.bodyCylinderDataEntries.push_back(BodyCylinderDataEntry{});
    }

    // global_scale scalar above table
    ImGui::SetNextItemWidth(120.0f);
    ImGui::InputScalar("global_scale", ImGuiDataType_Float, &bin.bodyCylinderGlobalScale);

    ImGui::Separator();

    constexpr ImGuiTableFlags tFlags =
        ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg |
        ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersInnerV |
        ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable |
        ImGuiTableFlags_Hideable | ImGuiTableFlags_SizingFixedFit;

    // cols: #, character_hash, cyl0_radius, cyl0_height, cyl0_offset_y,
    //       cyl1_radius, cyl1_height, cyl1_offset_y, cyl2_radius, cyl2_height, cyl2_offset_y
    if (!ImGui::BeginTable("##BodCylTable", 11, tFlags, ImGui::GetContentRegionAvail()))
        return;

    ImGui::TableSetupScrollFreeze(1, 1);
    static const int k_BodCylIds[] = { 0, 1, 2, 3, 8, 9, 10, 15, 16, 17 };
    ImGui::TableSetupColumn("#",                                    ImGuiTableColumnFlags_WidthFixed, ColumnWidths::kRowCtrl);
    for (int ci = 0; ci < 10; ++ci)
        ImGui::TableSetupColumn(FieldNames::BodyCylinderDataEntry[k_BodCylIds[ci]], ImGuiTableColumnFlags_WidthFixed, ColumnWidths::kBodyCylinder[ci]);
    ImGui::TableHeadersRow();

    int deleteIdx = -1;
    ImGuiListClipper clipper;
    clipper.Begin((int)bin.bodyCylinderDataEntries.size());
    while (clipper.Step())
    {
        for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i)
        {
            auto& e = bin.bodyCylinderDataEntries[i];
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
            if (!m_renderReadOnly && ImGui::SmallButton("X")) deleteIdx = i;
            ImGui::PopStyleColor(3);

            auto U32Cell = [](const char* id, uint32_t& v) {
                ImGui::SetNextItemWidth(-FLT_MIN);
                ImGui::InputScalar(id, ImGuiDataType_U32, &v);
            };
            auto F32Cell = [](const char* id, float& v) {
                ImGui::SetNextItemWidth(-FLT_MIN);
                ImGui::InputScalar(id, ImGuiDataType_Float, &v);
            };

            ImGui::TableSetColumnIndex(1);  U32Cell("##ch",  e.character_hash);
            ImGui::TableSetColumnIndex(2);  F32Cell("##c0r", e.cyl0_radius);
            ImGui::TableSetColumnIndex(3);  F32Cell("##c0h", e.cyl0_height);
            ImGui::TableSetColumnIndex(4);  F32Cell("##c0o", e.cyl0_offset_y);
            ImGui::TableSetColumnIndex(5);  F32Cell("##c1r", e.cyl1_radius);
            ImGui::TableSetColumnIndex(6);  F32Cell("##c1h", e.cyl1_height);
            ImGui::TableSetColumnIndex(7);  F32Cell("##c1o", e.cyl1_offset_y);
            ImGui::TableSetColumnIndex(8);  F32Cell("##c2r", e.cyl2_radius);
            ImGui::TableSetColumnIndex(9);  F32Cell("##c2h", e.cyl2_height);
            ImGui::TableSetColumnIndex(10); F32Cell("##c2o", e.cyl2_offset_y);

            ImGui::PopID();
        }
    }
    if (!m_renderReadOnly && deleteIdx >= 0)
        bin.bodyCylinderDataEntries.erase(bin.bodyCylinderDataEntries.begin() + deleteIdx);

    ImGui::EndTable();
}

// -----------------------------------------------------------------------------
//  customize_item_unique_list editor
// -----------------------------------------------------------------------------

void FbsDataView::RenderCustomizeItemUniqueListEditor(ContentsBinData& bin)
{
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.65f, 0.82f, 1.00f, 1.00f));
    ImGui::Text("customize_item_unique_list.bin");
    ImGui::PopStyleColor();
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.42f, 0.42f, 0.54f, 1.00f));
    ImGui::Text("(%d entries)", (int)bin.customizeItemUniqueEntries.size());
    ImGui::PopStyleColor();

    const float addBtnW    = 100.0f;
    const float ioGap      = 4.0f;
    const float exportBtnW = 70.0f;
    const float importBtnW = 70.0f;
    const float totalW     = exportBtnW + ioGap + importBtnW + ioGap + addBtnW;
    ImGui::SameLine(ImGui::GetContentRegionAvail().x - totalW + ImGui::GetCursorPosX());

    if (ImGui::Button("Export##u", ImVec2(exportBtnW, 0)))
    {
        std::string path = OpenTsvSaveDialog(L"customize_item_unique_list.tsv");
        if (!path.empty())
        {
            FixUniqueItemIds(bin.customizeItemUniqueEntries);
            ExportUniqueListTsv(bin.customizeItemUniqueEntries, path);
        }
    }
    ImGui::SameLine(0, ioGap);
    if (!m_renderReadOnly && ImGui::Button("Import##u", ImVec2(importBtnW, 0)))
    {
        std::string path = OpenTsvOpenDialog();
        if (!path.empty())
        {
            auto imported = ImportUniqueListTsv(path);
            if (!imported.empty())
            {
                FixUniqueItemIds(imported);
                bin.customizeItemUniqueEntries = std::move(imported);
            }
        }
    }
    ImGui::SameLine(0, ioGap);
    if (!m_renderReadOnly && ImGui::Button("+ Add Entry##u", ImVec2(addBtnW, 0)))
        bin.customizeItemUniqueEntries.push_back(bin.customizeItemUniqueEntries.empty()
            ? DefaultValues::UniqueEntry()
            : bin.customizeItemUniqueEntries.back());

    ImGui::Separator();

    constexpr ImGuiTableFlags tFlags =
        ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg |
        ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersInnerV |
        ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable |
        ImGuiTableFlags_Hideable | ImGuiTableFlags_SizingFixedFit;

    // cols: # + all 22 fields
    if (!ImGui::BeginTable("##CIUTable", 23, tFlags, ImGui::GetContentRegionAvail()))
        return;

    ImGui::TableSetupScrollFreeze(1, 1);
    ImGui::TableSetupColumn("#",                                 ImGuiTableColumnFlags_WidthFixed, ColumnWidths::kRowCtrl);
    for (int fi = 0; fi < 22; ++fi)
        ImGui::TableSetupColumn(FieldNames::CustomizeItemUnique[fi], ImGuiTableColumnFlags_WidthFixed, ColumnWidths::kUnique[fi]);
    ImGui::TableHeadersRow();

    // Build duplicate-id detection map
    std::unordered_map<uint32_t, int> idCounts;
    for (int i = 0; i < (int)bin.customizeItemUniqueEntries.size(); ++i)
        idCounts[bin.customizeItemUniqueEntries[i].char_item_id]++;

    int deleteIdx = -1;
    ImGuiListClipper clipper;
    clipper.Begin((int)bin.customizeItemUniqueEntries.size());
    while (clipper.Step())
    {
        for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i)
        {
            auto& e = bin.customizeItemUniqueEntries[i];
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
            if (!m_renderReadOnly && ImGui::SmallButton("X")) deleteIdx = i;
            ImGui::PopStyleColor(3);

            auto U32Cell = [](const char* id, uint32_t& v) {
                ImGui::SetNextItemWidth(-FLT_MIN);
                ImGui::InputScalar(id, ImGuiDataType_U32, &v);
            };
            auto StrCell = [](const char* id, char* buf, size_t sz) {
                ImGui::SetNextItemWidth(-FLT_MIN);
                ImGui::InputText(id, buf, sz);
            };
            auto BoolCell = [](const char* id, bool& v) {
                ImGui::SetNextItemWidth(-FLT_MIN);
                ImGui::Checkbox(id, &v);
            };

            ImGui::TableSetColumnIndex( 1);
            {
                int ddd = (int)(e.char_item_id % 1000u);
                bool isDup  = idCounts.count(e.char_item_id) && idCounts.at(e.char_item_id) > 1;
                bool isGame = FbsDataDict::Get().IsGameItemId(e.char_item_id);
                bool warn   = isDup || isGame;
                if (warn) ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.55f, 0.08f, 0.08f, 1.0f));
                ImGui::SetNextItemWidth(-FLT_MIN);
                if (ImGui::InputInt("##ddd", &ddd, 0, 0)) {
                    ddd = std::max(0, std::min(999, ddd));
                    e.char_item_id = AssembleItemId(1, e.character_hash, e.hash_1, (uint32_t)ddd, e.char_item_id);
                }
                if (warn) ImGui::PopStyleColor();
                if (warn && ImGui::IsItemHovered()) {
                    if (isDup && isGame)
                        ImGui::SetTooltip("Duplicate item_id: %u\nConflicts with a base game item ID", e.char_item_id);
                    else if (isDup)
                        ImGui::SetTooltip("Duplicate item_id: %u", e.char_item_id);
                    else
                        ImGui::SetTooltip("item_id %u conflicts with a base game item ID", e.char_item_id);
                }
            }
            ImGui::TableSetColumnIndex( 2); StrCell ("##an",   e.asset_name,        sizeof(e.asset_name));
            ImGui::TableSetColumnIndex( 3);
            if (HashComboCell("##ch", e.character_hash, FbsDataDict::Get().GetCharHashCodeMap(), GetCharHashItems()))
                e.char_item_id = AssembleItemId(1, e.character_hash, e.hash_1, e.char_item_id % 1000u, e.char_item_id);
            ImGui::TableSetColumnIndex( 4);
            if (HashComboCell("##h1", e.hash_1, FbsDataDict::Get().GetTypeHashCodeMap(), GetTypeHashItems()))
                e.char_item_id = AssembleItemId(1, e.character_hash, e.hash_1, e.char_item_id % 1000u, e.char_item_id);
            ImGui::TableSetColumnIndex( 5); StrCell ("##tk",   e.text_key,          sizeof(e.text_key));
            ImGui::TableSetColumnIndex( 6); StrCell ("##ek1",  e.extra_text_key_1,  sizeof(e.extra_text_key_1));
            ImGui::TableSetColumnIndex( 7); StrCell ("##ek2",  e.extra_text_key_2,  sizeof(e.extra_text_key_2));
            ImGui::TableSetColumnIndex( 8); U32Cell ("##f7",   e.flag_7);
            ImGui::TableSetColumnIndex( 9); U32Cell ("##u8",   e.unk_8);
            ImGui::TableSetColumnIndex(10); BoolCell("##f9",   e.flag_9);
            ImGui::TableSetColumnIndex(11); U32Cell ("##u10",  e.unk_10);
            ImGui::TableSetColumnIndex(12); U32Cell ("##prc",  e.price);
            ImGui::TableSetColumnIndex(13); U32Cell ("##u12",  e.unk_12);
            ImGui::TableSetColumnIndex(14); U32Cell ("##u13",  e.unk_13);
            ImGui::TableSetColumnIndex(15); U32Cell ("##h2",   e.hash_2);
            ImGui::TableSetColumnIndex(16); BoolCell("##f15",  e.flag_15);
            ImGui::TableSetColumnIndex(17); U32Cell ("##u16",  e.unk_16);
            ImGui::TableSetColumnIndex(18); U32Cell ("##h3",   e.hash_3);
            ImGui::TableSetColumnIndex(19); U32Cell ("##u18",  e.unk_18);
            ImGui::TableSetColumnIndex(20); U32Cell ("##u19",  e.unk_19);
            ImGui::TableSetColumnIndex(21); U32Cell ("##u20",  e.unk_20);
            ImGui::TableSetColumnIndex(22); U32Cell ("##u21",  e.unk_21);

            ImGui::PopID();
        }
    }
    if (!m_renderReadOnly && deleteIdx >= 0)
        bin.customizeItemUniqueEntries.erase(bin.customizeItemUniqueEntries.begin() + deleteIdx);

    ImGui::EndTable();
}

// -----------------------------------------------------------------------------
//  character_select_list TSV export / import + editor
// -----------------------------------------------------------------------------

static void ExportCharacterSelectListTsv(const ContentsBinData& bin, const std::string& path)
{
    FILE* f=nullptr; fopen_s(&f,path.c_str(),"wb"); if(!f) return;
    fprintf(f,"#hash_entries\n");
    for (const auto& e : bin.characterSelectHashEntries) fprintf(f,"%u\n",e.character_hash);
    fprintf(f,"#param_entries\n");
    for (const auto& e : bin.characterSelectParamEntries) fprintf(f,"%u\t%u\n",e.game_version,e.value_1);
    fclose(f);
}
static void ImportCharacterSelectListTsv(const std::string& path, ContentsBinData& bin)
{
    FILE* f=nullptr; fopen_s(&f,path.c_str(),"rb"); if(!f) return;
    bin.characterSelectHashEntries.clear(); bin.characterSelectParamEntries.clear();
    enum { NONE=0, HASH, PARAM } cur=NONE;
    char line[512];
    while (fgets(line, sizeof(line), f))
    {
        int len=(int)strlen(line); while(len>0&&(line[len-1]=='\r'||line[len-1]=='\n'))line[--len]='\0'; if(!len) continue;
        if (line[0]=='#') { if(strcmp(line,"#hash_entries")==0)cur=HASH; else if(strcmp(line,"#param_entries")==0)cur=PARAM; continue; }
        if (cur==HASH) { CharacterSelectHashEntry e; e.character_hash=(uint32_t)strtoul(line,nullptr,10); bin.characterSelectHashEntries.push_back(e); }
        else if (cur==PARAM)
        {
            char* cols[2]={}; int col=0; char* p=line; cols[col++]=p; for(;*p&&col<2;++p) if(*p=='\t'){*p='\0';cols[col++]=p+1;} if(col<2) continue;
            CharacterSelectParamEntry e; e.game_version=(uint32_t)strtoul(cols[0],nullptr,10); e.value_1=(uint32_t)strtoul(cols[1],nullptr,10);
            bin.characterSelectParamEntries.push_back(e);
        }
    }
    fclose(f);
}

void FbsDataView::RenderCharacterSelectListEditor(ContentsBinData& bin)
{
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.65f, 0.82f, 1.00f, 1.00f));
    ImGui::Text("character_select_list.bin");
    ImGui::PopStyleColor();

    // -- Export / Import buttons (right-aligned, no Add Entry at top level) --
    {
        const float ioGap=4.0f, exportBtnW=70.0f, importBtnW=70.0f;
        const float totalW=exportBtnW+ioGap+importBtnW;
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - totalW + ImGui::GetCursorPosX());
        if (ImGui::Button("Export##charsel", ImVec2(exportBtnW, 0))) { std::string p=OpenTsvSaveDialog(L"character_select_list.tsv"); if(!p.empty()) ExportCharacterSelectListTsv(bin,p); }
        ImGui::SameLine(0, ioGap);
        if (!m_renderReadOnly && ImGui::Button("Import##charsel", ImVec2(importBtnW, 0))) { std::string p=OpenTsvOpenDialog(); if(!p.empty()) ImportCharacterSelectListTsv(p,bin); }
    }

    ImGui::Separator();

    constexpr ImGuiTableFlags tFlags =
        ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg |
        ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersInnerV |
        ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable |
        ImGuiTableFlags_Hideable | ImGuiTableFlags_SizingFixedFit;

    if (!ImGui::BeginTabBar("##CharSelTabs"))
        return;

    if (ImGui::BeginTabItem("character_entries"))
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.42f, 0.42f, 0.54f, 1.00f));
        ImGui::Text("(%d entries)", (int)bin.characterSelectHashEntries.size());
        ImGui::PopStyleColor();
        const float addBtnW = 100.0f;
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - addBtnW + ImGui::GetCursorPosX());
        if (!m_renderReadOnly && ImGui::Button("+ Add Entry", ImVec2(addBtnW, 0)))
            bin.characterSelectHashEntries.push_back(CharacterSelectHashEntry{});

        if (ImGui::BeginTable("##CSHashTable", 2, tFlags, ImGui::GetContentRegionAvail()))
        {
            ImGui::TableSetupScrollFreeze(1, 1);
            ImGui::TableSetupColumn("#",                                 ImGuiTableColumnFlags_WidthFixed, ColumnWidths::kRowCtrl);
            ImGui::TableSetupColumn(FieldNames::CharacterSelectHash[0],  ImGuiTableColumnFlags_WidthFixed, ColumnWidths::kCharSelectHash[0]);
            ImGui::TableHeadersRow();

            int deleteIdx = -1;
            ImGuiListClipper clipper;
            clipper.Begin((int)bin.characterSelectHashEntries.size());
            while (clipper.Step())
            {
                for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i)
                {
                    auto& e = bin.characterSelectHashEntries[i];
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
                    if (!m_renderReadOnly && ImGui::SmallButton("X")) deleteIdx = i;
                    ImGui::PopStyleColor(3);

                    ImGui::TableSetColumnIndex(1);
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    ImGui::InputScalar("##ch", ImGuiDataType_U32, &e.character_hash);

                    ImGui::PopID();
                }
            }
            if (!m_renderReadOnly && deleteIdx >= 0)
                bin.characterSelectHashEntries.erase(bin.characterSelectHashEntries.begin() + deleteIdx);

            ImGui::EndTable();
        }
        ImGui::EndTabItem();
    }

    if (ImGui::BeginTabItem("param_entries"))
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.42f, 0.42f, 0.54f, 1.00f));
        ImGui::Text("(%d entries)", (int)bin.characterSelectParamEntries.size());
        ImGui::PopStyleColor();
        const float addBtnW = 100.0f;
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - addBtnW + ImGui::GetCursorPosX());
        if (!m_renderReadOnly && ImGui::Button("+ Add Entry##prm", ImVec2(addBtnW, 0)))
            bin.characterSelectParamEntries.push_back(CharacterSelectParamEntry{});

        if (ImGui::BeginTable("##CSParamTable", 3, tFlags, ImGui::GetContentRegionAvail()))
        {
            ImGui::TableSetupScrollFreeze(1, 1);
            ImGui::TableSetupColumn("#",                                  ImGuiTableColumnFlags_WidthFixed, ColumnWidths::kRowCtrl);
            ImGui::TableSetupColumn(FieldNames::CharacterSelectParam[0],  ImGuiTableColumnFlags_WidthFixed, ColumnWidths::kCharSelectParam[0]);
            ImGui::TableSetupColumn(FieldNames::CharacterSelectParam[1],  ImGuiTableColumnFlags_WidthFixed, ColumnWidths::kCharSelectParam[1]);
            ImGui::TableHeadersRow();

            int deleteIdx = -1;
            ImGuiListClipper clipper;
            clipper.Begin((int)bin.characterSelectParamEntries.size());
            while (clipper.Step())
            {
                for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i)
                {
                    auto& e = bin.characterSelectParamEntries[i];
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
                    if (!m_renderReadOnly && ImGui::SmallButton("X")) deleteIdx = i;
                    ImGui::PopStyleColor(3);

                    ImGui::TableSetColumnIndex(1);
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    ImGui::InputScalar("##gv", ImGuiDataType_U32, &e.game_version);
                    ImGui::TableSetColumnIndex(2);
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    ImGui::InputScalar("##v1", ImGuiDataType_U32, &e.value_1);

                    ImGui::PopID();
                }
            }
            if (!m_renderReadOnly && deleteIdx >= 0)
                bin.characterSelectParamEntries.erase(bin.characterSelectParamEntries.begin() + deleteIdx);

            ImGui::EndTable();
        }
        ImGui::EndTabItem();
    }

    ImGui::EndTabBar();
}

// -----------------------------------------------------------------------------
//  customize_item_prohibit_drama_list TSV export / import + editor
// -----------------------------------------------------------------------------

static void ExportProhibitDramaListTsv(const ContentsBinData& bin, const std::string& path)
{
    FILE* f=nullptr; fopen_s(&f,path.c_str(),"wb"); if(!f) return;
    fprintf(f,"#group_0\n");
    for (const auto& e : bin.prohibitDramaGroup0) fprintf(f,"%d\t%d\n",e.value_0,e.value_1);
    fprintf(f,"#group_1\n");
    for (const auto& e : bin.prohibitDramaGroup1) fprintf(f,"%d\t%d\n",e.value_0,e.value_1);
    fprintf(f,"#category_values\n");
    for (const auto& v : bin.prohibitDramaCategoryValues) fprintf(f,"%u\n",v);
    fclose(f);
}
static void ImportProhibitDramaListTsv(const std::string& path, ContentsBinData& bin)
{
    FILE* f=nullptr; fopen_s(&f,path.c_str(),"rb"); if(!f) return;
    bin.prohibitDramaGroup0.clear(); bin.prohibitDramaGroup1.clear(); bin.prohibitDramaCategoryValues.clear();
    enum { NONE=0, G0, G1, CAT } cur=NONE;
    char line[512];
    while (fgets(line, sizeof(line), f))
    {
        int len=(int)strlen(line); while(len>0&&(line[len-1]=='\r'||line[len-1]=='\n'))line[--len]='\0'; if(!len) continue;
        if (line[0]=='#') { if(strcmp(line,"#group_0")==0)cur=G0; else if(strcmp(line,"#group_1")==0)cur=G1; else if(strcmp(line,"#category_values")==0)cur=CAT; continue; }
        if (cur==G0||cur==G1)
        {
            char* cols[2]={}; int col=0; char* p=line; cols[col++]=p; for(;*p&&col<2;++p) if(*p=='\t'){*p='\0';cols[col++]=p+1;} if(col<2) continue;
            CustomizeItemProhibitDramaEntry e; e.value_0=(int32_t)strtol(cols[0],nullptr,10); e.value_1=(int32_t)strtol(cols[1],nullptr,10);
            if(cur==G0) bin.prohibitDramaGroup0.push_back(e); else bin.prohibitDramaGroup1.push_back(e);
        }
        else if (cur==CAT) bin.prohibitDramaCategoryValues.push_back((uint32_t)strtoul(line,nullptr,10));
    }
    fclose(f);
}

void FbsDataView::RenderCustomizeItemProhibitDramaListEditor(ContentsBinData& bin)
{
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.65f, 0.82f, 1.00f, 1.00f));
    ImGui::Text("customize_item_prohibit_drama_list.bin");
    ImGui::PopStyleColor();

    // -- Export / Import buttons (right-aligned) --
    {
        const float ioGap=4.0f, exportBtnW=70.0f, importBtnW=70.0f;
        const float totalW=exportBtnW+ioGap+importBtnW;
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - totalW + ImGui::GetCursorPosX());
        if (ImGui::Button("Export##cpd", ImVec2(exportBtnW, 0))) { std::string p=OpenTsvSaveDialog(L"customize_item_prohibit_drama_list.tsv"); if(!p.empty()) ExportProhibitDramaListTsv(bin,p); }
        ImGui::SameLine(0, ioGap);
        if (!m_renderReadOnly && ImGui::Button("Import##cpd", ImVec2(importBtnW, 0))) { std::string p=OpenTsvOpenDialog(); if(!p.empty()) ImportProhibitDramaListTsv(p,bin); }
    }

    ImGui::Separator();

    constexpr ImGuiTableFlags tFlags =
        ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg |
        ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersInnerV |
        ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable |
        ImGuiTableFlags_Hideable | ImGuiTableFlags_SizingFixedFit;

    auto RenderProhibitTable = [&](const char* tableId, const char* addId, std::vector<CustomizeItemProhibitDramaEntry>& entries) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.42f, 0.42f, 0.54f, 1.00f));
        ImGui::Text("(%d entries)", (int)entries.size());
        ImGui::PopStyleColor();
        const float addBtnW = 100.0f;
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - addBtnW + ImGui::GetCursorPosX());
        ImGui::PushID(addId);
        if (!m_renderReadOnly && ImGui::Button("+ Add Entry", ImVec2(addBtnW, 0)))
            entries.push_back(CustomizeItemProhibitDramaEntry{});
        ImGui::PopID();

        if (!ImGui::BeginTable(tableId, 3, tFlags, ImGui::GetContentRegionAvail()))
            return;

        ImGui::TableSetupScrollFreeze(1, 1);
        ImGui::TableSetupColumn("#",                                     ImGuiTableColumnFlags_WidthFixed, ColumnWidths::kRowCtrl);
        ImGui::TableSetupColumn(FieldNames::CustomizeItemProhibitDrama[0], ImGuiTableColumnFlags_WidthFixed, ColumnWidths::kProhibitDrama[0]);
        ImGui::TableSetupColumn(FieldNames::CustomizeItemProhibitDrama[1], ImGuiTableColumnFlags_WidthFixed, ColumnWidths::kProhibitDrama[1]);
        ImGui::TableHeadersRow();

        int deleteIdx = -1;
        ImGuiListClipper clipper;
        clipper.Begin((int)entries.size());
        while (clipper.Step())
        {
            for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i)
            {
                auto& e = entries[i];
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
                if (!m_renderReadOnly && ImGui::SmallButton("X")) deleteIdx = i;
                ImGui::PopStyleColor(3);

                ImGui::TableSetColumnIndex(1);
                ImGui::SetNextItemWidth(-FLT_MIN);
                ImGui::InputScalar("##v0", ImGuiDataType_S32, &e.value_0);
                ImGui::TableSetColumnIndex(2);
                ImGui::SetNextItemWidth(-FLT_MIN);
                ImGui::InputScalar("##v1", ImGuiDataType_S32, &e.value_1);

                ImGui::PopID();
            }
        }
        if (!m_renderReadOnly && deleteIdx >= 0)
            entries.erase(entries.begin() + deleteIdx);

        ImGui::EndTable();
    };

    if (!ImGui::BeginTabBar("##ProhibitDramaTabs"))
        return;

    if (ImGui::BeginTabItem("entry_group_0"))
    {
        RenderProhibitTable("##PDG0Table", "##PDG0Add", bin.prohibitDramaGroup0);
        ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("entry_group_1"))
    {
        RenderProhibitTable("##PDG1Table", "##PDG1Add", bin.prohibitDramaGroup1);
        ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("category_values"))
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.42f, 0.42f, 0.54f, 1.00f));
        ImGui::Text("(%d values)", (int)bin.prohibitDramaCategoryValues.size());
        ImGui::PopStyleColor();
        ImGui::SameLine();
        if (ImGui::SmallButton("+ Add##catv"))
            bin.prohibitDramaCategoryValues.push_back(0);

        const float avH = ImGui::GetContentRegionAvail().y;
        ImGui::BeginChild("##PDCatScroll", ImVec2(0.0f, avH), false);
        int deleteIdx = -1;
        for (int i = 0; i < (int)bin.prohibitDramaCategoryValues.size(); ++i)
        {
            ImGui::PushID(i);
            ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.55f, 0.12f, 0.12f, 1.00f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.78f, 0.18f, 0.18f, 1.00f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.65f, 0.15f, 0.15f, 1.00f));
            if (!m_renderReadOnly && ImGui::SmallButton("X")) deleteIdx = i;
            ImGui::PopStyleColor(3);
            ImGui::SameLine();
            ImGui::SetNextItemWidth(120.0f);
            ImGui::InputScalar("##cv", ImGuiDataType_U32, &bin.prohibitDramaCategoryValues[i]);
            ImGui::PopID();
        }
        if (!m_renderReadOnly && deleteIdx >= 0)
            bin.prohibitDramaCategoryValues.erase(bin.prohibitDramaCategoryValues.begin() + deleteIdx);
        ImGui::EndChild();
        ImGui::EndTabItem();
    }

    ImGui::EndTabBar();
}

// -----------------------------------------------------------------------------
//  battle_motion_list TSV export / import + editor
// -----------------------------------------------------------------------------

static void WriteBattleMotionEntries(FILE* f, const std::vector<BattleMotionEntry>& entries)
{
    for (const auto& e : entries) fprintf(f,"%u\t%u\t%u\n",(uint32_t)e.motion_id,e.value_1,e.value_2);
}

static void ExportBattleMotionListTsv(const ContentsBinData& bin, const std::string& path)
{
    FILE* f=nullptr; fopen_s(&f,path.c_str(),"wb"); if(!f) return;
    fprintf(f,"#list_params\n");
    fprintf(f,"%.9g\t%.9g\t%.9g\t%.9g\t%.9g\t%.9g\t%.9g\t%.9g\t%.9g\t%.9g\t%u\n",
        bin.battleMotionValue1,bin.battleMotionValue2,bin.battleMotionValue3,bin.battleMotionValue4,
        bin.battleMotionValue5,bin.battleMotionValue6,bin.battleMotionValue7,bin.battleMotionValue8,
        bin.battleMotionValue9,bin.battleMotionValue10,bin.battleMotionValue11);
    fprintf(f,"#entries\n");
    WriteBattleMotionEntries(f, bin.battleMotionEntries);
    fprintf(f,"#alt_entries\n");
    WriteBattleMotionEntries(f, bin.battleMotionEntriesAlt);
    fclose(f);
}
static void ImportBattleMotionListTsv(const std::string& path, ContentsBinData& bin)
{
    FILE* f=nullptr; fopen_s(&f,path.c_str(),"rb"); if(!f) return;
    bin.battleMotionEntries.clear(); bin.battleMotionEntriesAlt.clear();
    enum { NONE=0, PARAMS, ENTRIES, ALT } cur=NONE;
    char line[512];
    while (fgets(line, sizeof(line), f))
    {
        int len=(int)strlen(line); while(len>0&&(line[len-1]=='\r'||line[len-1]=='\n'))line[--len]='\0'; if(!len) continue;
        if (line[0]=='#')
        {
            if(strcmp(line,"#list_params")==0)cur=PARAMS;
            else if(strcmp(line,"#entries")==0)cur=ENTRIES;
            else if(strcmp(line,"#alt_entries")==0)cur=ALT;
            continue;
        }
        if (cur==PARAMS)
        {
            char* cols[11]={}; int col=0; char* p=line; cols[col++]=p; for(;*p&&col<11;++p) if(*p=='\t'){*p='\0';cols[col++]=p+1;} if(col<11) continue;
            bin.battleMotionValue1=strtof(cols[0],nullptr); bin.battleMotionValue2=strtof(cols[1],nullptr);
            bin.battleMotionValue3=strtof(cols[2],nullptr); bin.battleMotionValue4=strtof(cols[3],nullptr);
            bin.battleMotionValue5=strtof(cols[4],nullptr); bin.battleMotionValue6=strtof(cols[5],nullptr);
            bin.battleMotionValue7=strtof(cols[6],nullptr); bin.battleMotionValue8=strtof(cols[7],nullptr);
            bin.battleMotionValue9=strtof(cols[8],nullptr); bin.battleMotionValue10=strtof(cols[9],nullptr);
            bin.battleMotionValue11=(uint32_t)strtoul(cols[10],nullptr,10);
        }
        else if (cur==ENTRIES||cur==ALT)
        {
            char* cols[3]={}; int col=0; char* p=line; cols[col++]=p; for(;*p&&col<3;++p) if(*p=='\t'){*p='\0';cols[col++]=p+1;} if(col<3) continue;
            BattleMotionEntry e; e.motion_id=(uint8_t)strtoul(cols[0],nullptr,10); e.value_1=(uint32_t)strtoul(cols[1],nullptr,10); e.value_2=(uint32_t)strtoul(cols[2],nullptr,10);
            if(cur==ENTRIES) bin.battleMotionEntries.push_back(e); else bin.battleMotionEntriesAlt.push_back(e);
        }
    }
    fclose(f);
}

void FbsDataView::RenderBattleMotionListEditor(ContentsBinData& bin)
{
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.65f, 0.82f, 1.00f, 1.00f));
    ImGui::Text("battle_motion_list.bin");
    ImGui::PopStyleColor();

    // -- Export / Import buttons (right-aligned) --
    {
        const float ioGap=4.0f, exportBtnW=70.0f, importBtnW=70.0f;
        const float totalW=exportBtnW+ioGap+importBtnW;
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - totalW + ImGui::GetCursorPosX());
        if (ImGui::Button("Export##bmotion", ImVec2(exportBtnW, 0))) { std::string p=OpenTsvSaveDialog(L"battle_motion_list.tsv"); if(!p.empty()) ExportBattleMotionListTsv(bin,p); }
        ImGui::SameLine(0, ioGap);
        if (!m_renderReadOnly && ImGui::Button("Import##bmotion", ImVec2(importBtnW, 0))) { std::string p=OpenTsvOpenDialog(); if(!p.empty()) ImportBattleMotionListTsv(p,bin); }
    }

    ImGui::Separator();

    // Scalar header section -- 2-column grid
    {
        float* floatVals[10] = {
            &bin.battleMotionValue1,  &bin.battleMotionValue2,
            &bin.battleMotionValue3,  &bin.battleMotionValue4,
            &bin.battleMotionValue5,  &bin.battleMotionValue6,
            &bin.battleMotionValue7,  &bin.battleMotionValue8,
            &bin.battleMotionValue9,  &bin.battleMotionValue10,
        };
        const char* floatNames[10] = {
            "battleMotionValue1",  "battleMotionValue2",
            "battleMotionValue3",  "battleMotionValue4",
            "battleMotionValue5",  "battleMotionValue6",
            "battleMotionValue7",  "battleMotionValue8",
            "battleMotionValue9",  "battleMotionValue10",
        };
        for (int fi = 0; fi < 10; ++fi)
        {
            if (fi % 2 != 0) ImGui::SameLine(300.0f);
            ImGui::SetNextItemWidth(120.0f);
            ImGui::InputScalar(floatNames[fi], ImGuiDataType_Float, floatVals[fi]);
        }
        ImGui::SetNextItemWidth(120.0f);
        ImGui::InputScalar("battleMotionValue11", ImGuiDataType_U32, &bin.battleMotionValue11);
    }

    ImGui::Separator();

    constexpr ImGuiTableFlags tFlags =
        ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg |
        ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersInnerV |
        ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable |
        ImGuiTableFlags_Hideable | ImGuiTableFlags_SizingFixedFit;

    auto RenderMotionTable = [&](const char* tableId, const char* addId, std::vector<BattleMotionEntry>& entries) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.42f, 0.42f, 0.54f, 1.00f));
        ImGui::Text("(%d entries)", (int)entries.size());
        ImGui::PopStyleColor();
        const float addBtnW = 100.0f;
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - addBtnW + ImGui::GetCursorPosX());
        ImGui::PushID(addId);
        if (!m_renderReadOnly && ImGui::Button("+ Add Entry", ImVec2(addBtnW, 0)))
            entries.push_back(BattleMotionEntry{});
        ImGui::PopID();

        if (!ImGui::BeginTable(tableId, 4, tFlags, ImGui::GetContentRegionAvail()))
            return;

        ImGui::TableSetupScrollFreeze(1, 1);
        ImGui::TableSetupColumn("#",                              ImGuiTableColumnFlags_WidthFixed, ColumnWidths::kRowCtrl);
        for (int fi = 0; fi < 3; ++fi)
            ImGui::TableSetupColumn(FieldNames::BattleMotionEntry[fi], ImGuiTableColumnFlags_WidthFixed, ColumnWidths::kBattleMotion[fi]);
        ImGui::TableHeadersRow();

        int deleteIdx = -1;
        ImGuiListClipper clipper;
        clipper.Begin((int)entries.size());
        while (clipper.Step())
        {
            for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i)
            {
                auto& e = entries[i];
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
                if (!m_renderReadOnly && ImGui::SmallButton("X")) deleteIdx = i;
                ImGui::PopStyleColor(3);

                ImGui::TableSetColumnIndex(1);
                ImGui::SetNextItemWidth(-FLT_MIN);
                ImGui::InputScalar("##mid", ImGuiDataType_U8, &e.motion_id);
                ImGui::TableSetColumnIndex(2);
                ImGui::SetNextItemWidth(-FLT_MIN);
                ImGui::InputScalar("##v1", ImGuiDataType_U32, &e.value_1);
                ImGui::TableSetColumnIndex(3);
                ImGui::SetNextItemWidth(-FLT_MIN);
                ImGui::InputScalar("##v2", ImGuiDataType_U32, &e.value_2);

                ImGui::PopID();
            }
        }
        if (!m_renderReadOnly && deleteIdx >= 0)
            entries.erase(entries.begin() + deleteIdx);

        ImGui::EndTable();
    };

    if (!ImGui::BeginTabBar("##MotionTabs"))
        return;

    if (ImGui::BeginTabItem("entries"))
    {
        RenderMotionTable("##MotTable", "##MotAdd", bin.battleMotionEntries);
        ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("entries_alt"))
    {
        RenderMotionTable("##MotAltTable", "##MotAltAdd", bin.battleMotionEntriesAlt);
        ImGui::EndTabItem();
    }

    ImGui::EndTabBar();
}

// -----------------------------------------------------------------------------
//  arcade_cpu_list TSV export / import + editor
// -----------------------------------------------------------------------------

static void ExportArcadeCpuListTsv(const ContentsBinData& bin, const std::string& path)
{
    FILE* f=nullptr; fopen_s(&f,path.c_str(),"wb"); if(!f) return;
    fprintf(f,"#settings\n%u\t%u\t%u\n",bin.arcadeCpuSettings.unk_0,bin.arcadeCpuSettings.unk_1,bin.arcadeCpuSettings.unk_2);
    fprintf(f,"#character_entries\n");
    for (const auto& e : bin.arcadeCpuCharacterEntries)
        fprintf(f,"%u\t%u\t%.9g\t%u\t%.9g\t%u\t%.9g\n",e.character_hash,e.ai_level,e.float_1,e.uint_2,e.float_2,e.uint_3,e.float_3);
    fprintf(f,"#hash_group_a\n");
    for (const auto& e : bin.arcadeCpuHashGroupA) fprintf(f,"%u\n",e.value_hash);
    fprintf(f,"#hash_group_b\n");
    for (const auto& e : bin.arcadeCpuHashGroupB) fprintf(f,"%u\n",e.value_hash);
    fprintf(f,"#rule_entries\n");
    for (const auto& e : bin.arcadeCpuRuleEntries)
        fprintf(f,"%u\t%u\t%u\t%u\n",(uint32_t)e.flag_0,(uint32_t)e.flag_1,e.value_2,e.value_3);
    fclose(f);
}
static void ImportArcadeCpuListTsv(const std::string& path, ContentsBinData& bin)
{
    FILE* f=nullptr; fopen_s(&f,path.c_str(),"rb"); if(!f) return;
    bin.arcadeCpuCharacterEntries.clear(); bin.arcadeCpuHashGroupA.clear(); bin.arcadeCpuHashGroupB.clear(); bin.arcadeCpuRuleEntries.clear();
    enum { NONE=0, SETTINGS, CHAR, HA, HB, RULE } cur=NONE;
    char line[512];
    while (fgets(line, sizeof(line), f))
    {
        int len=(int)strlen(line); while(len>0&&(line[len-1]=='\r'||line[len-1]=='\n'))line[--len]='\0'; if(!len) continue;
        if (line[0]=='#')
        {
            if(strcmp(line,"#settings")==0)cur=SETTINGS;
            else if(strcmp(line,"#character_entries")==0)cur=CHAR;
            else if(strcmp(line,"#hash_group_a")==0)cur=HA;
            else if(strcmp(line,"#hash_group_b")==0)cur=HB;
            else if(strcmp(line,"#rule_entries")==0)cur=RULE;
            continue;
        }
        if (cur==SETTINGS)
        {
            char* cols[3]={}; int col=0; char* p=line; cols[col++]=p; for(;*p&&col<3;++p) if(*p=='\t'){*p='\0';cols[col++]=p+1;} if(col<3) continue;
            bin.arcadeCpuSettings.unk_0=(uint32_t)strtoul(cols[0],nullptr,10); bin.arcadeCpuSettings.unk_1=(uint32_t)strtoul(cols[1],nullptr,10); bin.arcadeCpuSettings.unk_2=(uint32_t)strtoul(cols[2],nullptr,10);
        }
        else if (cur==CHAR)
        {
            char* cols[7]={}; int col=0; char* p=line; cols[col++]=p; for(;*p&&col<7;++p) if(*p=='\t'){*p='\0';cols[col++]=p+1;} if(col<7) continue;
            ArcadeCpuCharacterEntry e;
            e.character_hash=(uint32_t)strtoul(cols[0],nullptr,10); e.ai_level=(uint32_t)strtoul(cols[1],nullptr,10);
            e.float_1=strtof(cols[2],nullptr); e.uint_2=(uint32_t)strtoul(cols[3],nullptr,10);
            e.float_2=strtof(cols[4],nullptr); e.uint_3=(uint32_t)strtoul(cols[5],nullptr,10); e.float_3=strtof(cols[6],nullptr);
            bin.arcadeCpuCharacterEntries.push_back(e);
        }
        else if (cur==HA||cur==HB)
        {
            ArcadeCpuHashEntry e; e.value_hash=(uint32_t)strtoul(line,nullptr,10);
            if(cur==HA) bin.arcadeCpuHashGroupA.push_back(e); else bin.arcadeCpuHashGroupB.push_back(e);
        }
        else if (cur==RULE)
        {
            char* cols[4]={}; int col=0; char* p=line; cols[col++]=p; for(;*p&&col<4;++p) if(*p=='\t'){*p='\0';cols[col++]=p+1;} if(col<4) continue;
            ArcadeCpuRuleEntry e;
            e.flag_0=(uint8_t)strtoul(cols[0],nullptr,10); e.flag_1=(uint8_t)strtoul(cols[1],nullptr,10);
            e.value_2=(uint32_t)strtoul(cols[2],nullptr,10); e.value_3=(uint32_t)strtoul(cols[3],nullptr,10);
            bin.arcadeCpuRuleEntries.push_back(e);
        }
    }
    fclose(f);
}

void FbsDataView::RenderArcadeCpuListEditor(ContentsBinData& bin)
{
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.65f, 0.82f, 1.00f, 1.00f));
    ImGui::Text("arcade_cpu_list.bin");
    ImGui::PopStyleColor();

    // -- Export / Import buttons (right-aligned) --
    {
        const float ioGap=4.0f, exportBtnW=70.0f, importBtnW=70.0f;
        const float totalW=exportBtnW+ioGap+importBtnW;
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - totalW + ImGui::GetCursorPosX());
        if (ImGui::Button("Export##acpu", ImVec2(exportBtnW, 0))) { std::string p=OpenTsvSaveDialog(L"arcade_cpu_list.tsv"); if(!p.empty()) ExportArcadeCpuListTsv(bin,p); }
        ImGui::SameLine(0, ioGap);
        if (!m_renderReadOnly && ImGui::Button("Import##acpu", ImVec2(importBtnW, 0))) { std::string p=OpenTsvOpenDialog(); if(!p.empty()) ImportArcadeCpuListTsv(p,bin); }
    }

    ImGui::Separator();

    constexpr ImGuiTableFlags tFlags =
        ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg |
        ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersInnerV |
        ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable |
        ImGuiTableFlags_Hideable | ImGuiTableFlags_SizingFixedFit;

    if (!ImGui::BeginTabBar("##ArcadeCpuTabs"))
        return;

    if (ImGui::BeginTabItem("settings"))
    {
        ImGui::SetNextItemWidth(120.0f);
        ImGui::InputScalar(FieldNames::ArcadeCpuSettings[0], ImGuiDataType_U32, &bin.arcadeCpuSettings.unk_0);
        ImGui::SetNextItemWidth(120.0f);
        ImGui::InputScalar(FieldNames::ArcadeCpuSettings[1], ImGuiDataType_U32, &bin.arcadeCpuSettings.unk_1);
        ImGui::SetNextItemWidth(120.0f);
        ImGui::InputScalar(FieldNames::ArcadeCpuSettings[2], ImGuiDataType_U32, &bin.arcadeCpuSettings.unk_2);
        ImGui::EndTabItem();
    }

    if (ImGui::BeginTabItem("character_entries"))
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.42f, 0.42f, 0.54f, 1.00f));
        ImGui::Text("(%d entries)", (int)bin.arcadeCpuCharacterEntries.size());
        ImGui::PopStyleColor();
        const float addBtnW = 100.0f;
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - addBtnW + ImGui::GetCursorPosX());
        if (!m_renderReadOnly && ImGui::Button("+ Add Entry", ImVec2(addBtnW, 0)))
            bin.arcadeCpuCharacterEntries.push_back(ArcadeCpuCharacterEntry{});

        if (ImGui::BeginTable("##ACCharTable", 8, tFlags, ImGui::GetContentRegionAvail()))
        {
            ImGui::TableSetupScrollFreeze(1, 1);
            ImGui::TableSetupColumn("#",                               ImGuiTableColumnFlags_WidthFixed, ColumnWidths::kRowCtrl);
            for (int fi = 0; fi < 7; ++fi)
                ImGui::TableSetupColumn(FieldNames::ArcadeCpuCharacter[fi], ImGuiTableColumnFlags_WidthFixed, ColumnWidths::kArcadeCpuCharacter[fi]);
            ImGui::TableHeadersRow();

            int deleteIdx = -1;
            ImGuiListClipper clipper;
            clipper.Begin((int)bin.arcadeCpuCharacterEntries.size());
            while (clipper.Step())
            {
                for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i)
                {
                    auto& e = bin.arcadeCpuCharacterEntries[i];
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
                    if (!m_renderReadOnly && ImGui::SmallButton("X")) deleteIdx = i;
                    ImGui::PopStyleColor(3);

                    auto U32Cell = [](const char* id, uint32_t& v) {
                        ImGui::SetNextItemWidth(-FLT_MIN);
                        ImGui::InputScalar(id, ImGuiDataType_U32, &v);
                    };
                    auto F32Cell = [](const char* id, float& v) {
                        ImGui::SetNextItemWidth(-FLT_MIN);
                        ImGui::InputScalar(id, ImGuiDataType_Float, &v);
                    };

                    ImGui::TableSetColumnIndex(1); U32Cell("##ch", e.character_hash);
                    ImGui::TableSetColumnIndex(2); U32Cell("##al", e.ai_level);
                    ImGui::TableSetColumnIndex(3); F32Cell("##f1", e.float_1);
                    ImGui::TableSetColumnIndex(4); U32Cell("##u2", e.uint_2);
                    ImGui::TableSetColumnIndex(5); F32Cell("##f2", e.float_2);
                    ImGui::TableSetColumnIndex(6); U32Cell("##u3", e.uint_3);
                    ImGui::TableSetColumnIndex(7); F32Cell("##f3", e.float_3);

                    ImGui::PopID();
                }
            }
            if (!m_renderReadOnly && deleteIdx >= 0)
                bin.arcadeCpuCharacterEntries.erase(bin.arcadeCpuCharacterEntries.begin() + deleteIdx);

            ImGui::EndTable();
        }
        ImGui::EndTabItem();
    }

    auto RenderACHashGroupTab = [&](const char* tabName, const char* tableId, const char* addId, std::vector<ArcadeCpuHashEntry>& entries) {
        if (ImGui::BeginTabItem(tabName))
        {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.42f, 0.42f, 0.54f, 1.00f));
            ImGui::Text("(%d entries)", (int)entries.size());
            ImGui::PopStyleColor();
            const float addBtnW = 100.0f;
            ImGui::SameLine(ImGui::GetContentRegionAvail().x - addBtnW + ImGui::GetCursorPosX());
            ImGui::PushID(addId);
            if (!m_renderReadOnly && ImGui::Button("+ Add Entry", ImVec2(addBtnW, 0)))
                entries.push_back(ArcadeCpuHashEntry{});
            ImGui::PopID();

            if (ImGui::BeginTable(tableId, 2, tFlags, ImGui::GetContentRegionAvail()))
            {
                ImGui::TableSetupScrollFreeze(1, 1);
                ImGui::TableSetupColumn("#",                           ImGuiTableColumnFlags_WidthFixed, ColumnWidths::kRowCtrl);
                ImGui::TableSetupColumn(FieldNames::ArcadeCpuHash[0], ImGuiTableColumnFlags_WidthFixed, ColumnWidths::kArcadeCpuHash[0]);
                ImGui::TableHeadersRow();

                int deleteIdx = -1;
                ImGuiListClipper clipper;
                clipper.Begin((int)entries.size());
                while (clipper.Step())
                {
                    for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i)
                    {
                        auto& e = entries[i];
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
                        if (!m_renderReadOnly && ImGui::SmallButton("X")) deleteIdx = i;
                        ImGui::PopStyleColor(3);

                        ImGui::TableSetColumnIndex(1);
                        ImGui::SetNextItemWidth(-FLT_MIN);
                        ImGui::InputScalar("##vh", ImGuiDataType_U32, &e.value_hash);

                        ImGui::PopID();
                    }
                }
                if (!m_renderReadOnly && deleteIdx >= 0)
                    entries.erase(entries.begin() + deleteIdx);

                ImGui::EndTable();
            }
            ImGui::EndTabItem();
        }
    };

    RenderACHashGroupTab("hash_group_a", "##ACHashATable", "##ACHashAAdd", bin.arcadeCpuHashGroupA);
    RenderACHashGroupTab("hash_group_b", "##ACHashBTable", "##ACHashBAdd", bin.arcadeCpuHashGroupB);

    if (ImGui::BeginTabItem("rule_entries"))
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.42f, 0.42f, 0.54f, 1.00f));
        ImGui::Text("(%d entries)", (int)bin.arcadeCpuRuleEntries.size());
        ImGui::PopStyleColor();
        const float addBtnW = 100.0f;
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - addBtnW + ImGui::GetCursorPosX());
        if (!m_renderReadOnly && ImGui::Button("+ Add Entry##rule", ImVec2(addBtnW, 0)))
            bin.arcadeCpuRuleEntries.push_back(ArcadeCpuRuleEntry{});

        if (ImGui::BeginTable("##ACRuleTable", 5, tFlags, ImGui::GetContentRegionAvail()))
        {
            ImGui::TableSetupScrollFreeze(1, 1);
            ImGui::TableSetupColumn("#",                           ImGuiTableColumnFlags_WidthFixed, ColumnWidths::kRowCtrl);
            for (int fi = 0; fi < 4; ++fi)
                ImGui::TableSetupColumn(FieldNames::ArcadeCpuRule[fi], ImGuiTableColumnFlags_WidthFixed, ColumnWidths::kArcadeCpuRule[fi]);
            ImGui::TableHeadersRow();

            int deleteIdx = -1;
            ImGuiListClipper clipper;
            clipper.Begin((int)bin.arcadeCpuRuleEntries.size());
            while (clipper.Step())
            {
                for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i)
                {
                    auto& e = bin.arcadeCpuRuleEntries[i];
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
                    if (!m_renderReadOnly && ImGui::SmallButton("X")) deleteIdx = i;
                    ImGui::PopStyleColor(3);

                    ImGui::TableSetColumnIndex(1);
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    ImGui::InputScalar("##f0", ImGuiDataType_U8, &e.flag_0);
                    ImGui::TableSetColumnIndex(2);
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    ImGui::InputScalar("##f1", ImGuiDataType_U8, &e.flag_1);
                    ImGui::TableSetColumnIndex(3);
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    ImGui::InputScalar("##v2", ImGuiDataType_U32, &e.value_2);
                    ImGui::TableSetColumnIndex(4);
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    ImGui::InputScalar("##v3", ImGuiDataType_U32, &e.value_3);

                    ImGui::PopID();
                }
            }
            if (!m_renderReadOnly && deleteIdx >= 0)
                bin.arcadeCpuRuleEntries.erase(bin.arcadeCpuRuleEntries.begin() + deleteIdx);

            ImGui::EndTable();
        }
        ImGui::EndTabItem();
    }

    ImGui::EndTabBar();
}

// -----------------------------------------------------------------------------
//  ball_recommend_list editor
// -----------------------------------------------------------------------------

static void ExportBallRecommendListTsv(const ContentsBinData& bin, const std::string& path)
{
    FILE* f = nullptr;
    if (fopen_s(&f, path.c_str(), "wb") != 0 || !f) return;

    auto writeGroup = [&](const char* header, const std::vector<BallRecommendEntry>& entries) {
        fprintf(f, "%s\n", header);
        fprintf(f, "character_hash\tmove_name_key\tcommand_text_key\tunk_3\tunk_4\n");
        for (const auto& e : entries)
            fprintf(f, "%u\t%s\t%s\t%u\t%u\n", e.character_hash, e.move_name_key, e.command_text_key, e.unk_3, e.unk_4);
    };

    writeGroup("#group_0", bin.ballRecommendGroup0);
    writeGroup("#group_1", bin.ballRecommendGroup1);
    writeGroup("#group_2", bin.ballRecommendGroup2);

    fprintf(f, "#unk_values\n");
    for (int i = 0; i < (int)bin.ballRecommendUnkValues.size(); ++i)
    {
        if (i > 0) fputc('\t', f);
        fprintf(f, "%u", bin.ballRecommendUnkValues[i]);
    }
    fputc('\n', f);

    fclose(f);
}

static void ImportBallRecommendListTsv(const std::string& path, ContentsBinData& bin)
{
    FILE* f = nullptr;
    if (fopen_s(&f, path.c_str(), "rb") != 0 || !f) return;

    std::vector<BallRecommendEntry> g0, g1, g2;
    std::vector<uint32_t> unkVals;

    enum class Sec { None, G0, G1, G2, Unk } sec = Sec::None;
    bool headerSkipped = false;

    char line[1024];
    while (fgets(line, sizeof(line), f))
    {
        int len = (int)strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r')) line[--len] = '\0';
        if (len == 0) continue;

        if (line[0] == '#')
        {
            headerSkipped = false;
            if      (strcmp(line, "#group_0")   == 0) { sec = Sec::G0;  }
            else if (strcmp(line, "#group_1")   == 0) { sec = Sec::G1;  }
            else if (strcmp(line, "#group_2")   == 0) { sec = Sec::G2;  }
            else if (strcmp(line, "#unk_values") == 0) { sec = Sec::Unk; }
            else                                        { sec = Sec::None; }
            continue;
        }

        if (sec == Sec::G0 || sec == Sec::G1 || sec == Sec::G2)
        {
            if (!headerSkipped) { headerSkipped = true; continue; }
            char* cols[5] = {}; int col = 0; char* p = line;
            cols[col++] = p;
            for (; *p && col < 5; ++p) if (*p == '\t') { *p = '\0'; cols[col++] = p+1; }
            if (col < 5) continue;
            BallRecommendEntry e;
            e.character_hash = (uint32_t)strtoul(cols[0], nullptr, 10);
            strncpy_s(e.move_name_key,    sizeof(e.move_name_key),    cols[1], _TRUNCATE);
            strncpy_s(e.command_text_key, sizeof(e.command_text_key), cols[2], _TRUNCATE);
            e.unk_3 = (uint32_t)strtoul(cols[3], nullptr, 10);
            e.unk_4 = (uint32_t)strtoul(cols[4], nullptr, 10);
            if      (sec == Sec::G0) g0.push_back(e);
            else if (sec == Sec::G1) g1.push_back(e);
            else                     g2.push_back(e);
        }
        else if (sec == Sec::Unk)
        {
            // single row, tab-separated uint32 values
            char* p = line;
            while (*p)
            {
                char* start = p;
                while (*p && *p != '\t') ++p;
                char saved = *p; *p = '\0';
                unkVals.push_back((uint32_t)strtoul(start, nullptr, 10));
                if (saved) ++p; else break;
            }
        }
    }
    fclose(f);

    bin.ballRecommendGroup0    = std::move(g0);
    bin.ballRecommendGroup1    = std::move(g1);
    bin.ballRecommendGroup2    = std::move(g2);
    bin.ballRecommendUnkValues = std::move(unkVals);
}

void FbsDataView::RenderBallRecommendListEditor(ContentsBinData& bin)
{
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.65f, 0.82f, 1.00f, 1.00f));
    ImGui::Text("ball_recommend_list.bin");
    ImGui::PopStyleColor();

    // -- Export / Import buttons (right-aligned) --
    {
        const float ioGap=4.0f, exportBtnW=70.0f, importBtnW=70.0f;
        const float totalW=exportBtnW+ioGap+importBtnW;
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - totalW + ImGui::GetCursorPosX());
        if (ImGui::Button("Export##brecom", ImVec2(exportBtnW, 0))) { std::string p=OpenTsvSaveDialog(L"ball_recommend_list.tsv"); if(!p.empty()) ExportBallRecommendListTsv(bin,p); }
        ImGui::SameLine(0, ioGap);
        if (!m_renderReadOnly && ImGui::Button("Import##brecom", ImVec2(importBtnW, 0))) { std::string p=OpenTsvOpenDialog(); if(!p.empty()) ImportBallRecommendListTsv(p,bin); }
    }

    ImGui::Separator();

    constexpr ImGuiTableFlags tFlags =
        ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg |
        ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersInnerV |
        ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable |
        ImGuiTableFlags_Hideable | ImGuiTableFlags_SizingFixedFit;

    auto RenderRecommendGroupTab = [&](const char* tabName, const char* tableId, const char* addId, std::vector<BallRecommendEntry>& entries) {
        if (ImGui::BeginTabItem(tabName))
        {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.42f, 0.42f, 0.54f, 1.00f));
            ImGui::Text("(%d entries)", (int)entries.size());
            ImGui::PopStyleColor();
            const float addBtnW = 100.0f;
            ImGui::SameLine(ImGui::GetContentRegionAvail().x - addBtnW + ImGui::GetCursorPosX());
            ImGui::PushID(addId);
            if (!m_renderReadOnly && ImGui::Button("+ Add Entry", ImVec2(addBtnW, 0)))
                entries.push_back(BallRecommendEntry{});
            ImGui::PopID();

            if (ImGui::BeginTable(tableId, 6, tFlags, ImGui::GetContentRegionAvail()))
            {
                ImGui::TableSetupScrollFreeze(1, 1);
                ImGui::TableSetupColumn("#",                               ImGuiTableColumnFlags_WidthFixed, ColumnWidths::kRowCtrl);
                for (int fi = 0; fi < 5; ++fi)
                    ImGui::TableSetupColumn(FieldNames::BallRecommendEntry[fi], ImGuiTableColumnFlags_WidthFixed, ColumnWidths::kBallRecommend[fi]);
                ImGui::TableHeadersRow();

                int deleteIdx = -1;
                ImGuiListClipper clipper;
                clipper.Begin((int)entries.size());
                while (clipper.Step())
                {
                    for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i)
                    {
                        auto& e = entries[i];
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
                        if (!m_renderReadOnly && ImGui::SmallButton("X")) deleteIdx = i;
                        ImGui::PopStyleColor(3);

                        ImGui::TableSetColumnIndex(1);
                        ImGui::SetNextItemWidth(-FLT_MIN);
                        ImGui::InputScalar("##ch", ImGuiDataType_U32, &e.character_hash);
                        ImGui::TableSetColumnIndex(2);
                        ImGui::SetNextItemWidth(-FLT_MIN);
                        ImGui::InputText("##mn", e.move_name_key, sizeof(e.move_name_key));
                        ImGui::TableSetColumnIndex(3);
                        ImGui::SetNextItemWidth(-FLT_MIN);
                        ImGui::InputText("##ct", e.command_text_key, sizeof(e.command_text_key));
                        ImGui::TableSetColumnIndex(4);
                        ImGui::SetNextItemWidth(-FLT_MIN);
                        ImGui::InputScalar("##u3", ImGuiDataType_U32, &e.unk_3);
                        ImGui::TableSetColumnIndex(5);
                        ImGui::SetNextItemWidth(-FLT_MIN);
                        ImGui::InputScalar("##u4", ImGuiDataType_U32, &e.unk_4);

                        ImGui::PopID();
                    }
                }
                if (!m_renderReadOnly && deleteIdx >= 0)
                    entries.erase(entries.begin() + deleteIdx);

                ImGui::EndTable();
            }
            ImGui::EndTabItem();
        }
    };

    if (!ImGui::BeginTabBar("##BallRecTabs"))
        return;

    RenderRecommendGroupTab("recommend_group_0", "##BRG0Table", "##BRG0Add", bin.ballRecommendGroup0);
    RenderRecommendGroupTab("recommend_group_1", "##BRG1Table", "##BRG1Add", bin.ballRecommendGroup1);
    RenderRecommendGroupTab("recommend_group_2", "##BRG2Table", "##BRG2Add", bin.ballRecommendGroup2);

    if (ImGui::BeginTabItem("unk_values"))
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.42f, 0.42f, 0.54f, 1.00f));
        ImGui::Text("(%d values)", (int)bin.ballRecommendUnkValues.size());
        ImGui::PopStyleColor();
        ImGui::SameLine();
        if (ImGui::SmallButton("+ Add##brunk"))
            bin.ballRecommendUnkValues.push_back(0);

        const float avH = ImGui::GetContentRegionAvail().y;
        ImGui::BeginChild("##BRUnkScroll", ImVec2(0.0f, avH), false);
        int deleteIdx = -1;
        for (int i = 0; i < (int)bin.ballRecommendUnkValues.size(); ++i)
        {
            ImGui::PushID(i);
            ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.55f, 0.12f, 0.12f, 1.00f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.78f, 0.18f, 0.18f, 1.00f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.65f, 0.15f, 0.15f, 1.00f));
            if (!m_renderReadOnly && ImGui::SmallButton("X")) deleteIdx = i;
            ImGui::PopStyleColor(3);
            ImGui::SameLine();
            ImGui::SetNextItemWidth(120.0f);
            ImGui::InputScalar("##uv", ImGuiDataType_U32, &bin.ballRecommendUnkValues[i]);
            ImGui::PopID();
        }
        if (!m_renderReadOnly && deleteIdx >= 0)
            bin.ballRecommendUnkValues.erase(bin.ballRecommendUnkValues.begin() + deleteIdx);
        ImGui::EndChild();
        ImGui::EndTabItem();
    }

    ImGui::EndTabBar();
}

// -----------------------------------------------------------------------------
//  ball_setting_list editor (property grid)
// -----------------------------------------------------------------------------

static void ExportBallSettingListTsv(const ContentsBinData& bin, const std::string& path)
{
    FILE* f = nullptr;
    if (fopen_s(&f, path.c_str(), "wb") != 0 || !f) return;

    static const bool k_IsUint[72] = {
        false,false,false,false,false,false,false,false,false,
        true,true,
        false,false,false,false,
        true,
        false,
        true,
        false,false,false,false,false,false,
        true,true,true,true,true,true,
        false,
        true,
        false,
        true,true,true,true,true,true,true,true,
        false,false,false,
        true,true,
        false,
        true,true,true,true,true,true,
        false,false,
        true,
        false,false,
        true,true,true,
        false,false,
        true,true,true,true,true,true,true,true,true
    };

    // header row
    fprintf(f, "value_0");
    for (int i = 1; i < 72; ++i) fprintf(f, "\tvalue_%d", i);
    fputc('\n', f);

    // data row - single row with all 72 values
    const auto& d = bin.ballSettingData;
    const float*    fv[72] = {};
    const uint32_t* uv[72] = {};
    fv[0]=&d.value_0; fv[1]=&d.value_1; fv[2]=&d.value_2; fv[3]=&d.value_3;
    fv[4]=&d.value_4; fv[5]=&d.value_5; fv[6]=&d.value_6; fv[7]=&d.value_7;
    fv[8]=&d.value_8;
    uv[9]=&d.value_9; uv[10]=&d.value_10;
    fv[11]=&d.value_11; fv[12]=&d.value_12; fv[13]=&d.value_13; fv[14]=&d.value_14;
    uv[15]=&d.value_15; fv[16]=&d.value_16; uv[17]=&d.value_17;
    fv[18]=&d.value_18; fv[19]=&d.value_19; fv[20]=&d.value_20; fv[21]=&d.value_21;
    fv[22]=&d.value_22; fv[23]=&d.value_23;
    uv[24]=&d.value_24; uv[25]=&d.value_25; uv[26]=&d.value_26; uv[27]=&d.value_27;
    uv[28]=&d.value_28; uv[29]=&d.value_29;
    fv[30]=&d.value_30; uv[31]=&d.value_31; fv[32]=&d.value_32;
    uv[33]=&d.value_33; uv[34]=&d.value_34; uv[35]=&d.value_35; uv[36]=&d.value_36;
    uv[37]=&d.value_37; uv[38]=&d.value_38; uv[39]=&d.value_39; uv[40]=&d.value_40;
    fv[41]=&d.value_41; fv[42]=&d.value_42; fv[43]=&d.value_43;
    uv[44]=&d.value_44; uv[45]=&d.value_45;
    fv[46]=&d.value_46;
    uv[47]=&d.value_47; uv[48]=&d.value_48; uv[49]=&d.value_49;
    uv[50]=&d.value_50; uv[51]=&d.value_51; uv[52]=&d.value_52;
    fv[53]=&d.value_53; fv[54]=&d.value_54; uv[55]=&d.value_55;
    fv[56]=&d.value_56; fv[57]=&d.value_57;
    uv[58]=&d.value_58; uv[59]=&d.value_59; uv[60]=&d.value_60;
    fv[61]=&d.value_61; fv[62]=&d.value_62;
    uv[63]=&d.value_63; uv[64]=&d.value_64; uv[65]=&d.value_65; uv[66]=&d.value_66;
    uv[67]=&d.value_67; uv[68]=&d.value_68; uv[69]=&d.value_69; uv[70]=&d.value_70;
    uv[71]=&d.value_71;

    for (int i = 0; i < 72; ++i)
    {
        if (i > 0) fputc('\t', f);
        if (k_IsUint[i]) fprintf(f, "%u", *uv[i]);
        else             fprintf(f, "%.9g", *fv[i]);
    }
    fputc('\n', f);
    fclose(f);
}

static void ImportBallSettingListTsv(const std::string& path, ContentsBinData& bin)
{
    FILE* f = nullptr;
    if (fopen_s(&f, path.c_str(), "rb") != 0 || !f) return;

    static const bool k_IsUint[72] = {
        false,false,false,false,false,false,false,false,false,
        true,true,
        false,false,false,false,
        true,
        false,
        true,
        false,false,false,false,false,false,
        true,true,true,true,true,true,
        false,
        true,
        false,
        true,true,true,true,true,true,true,true,
        false,false,false,
        true,true,
        false,
        true,true,true,true,true,true,
        false,false,
        true,
        false,false,
        true,true,true,
        false,false,
        true,true,true,true,true,true,true,true,true
    };

    auto& d = bin.ballSettingData;
    float*    fv[72] = {};
    uint32_t* uv[72] = {};
    fv[0]=&d.value_0; fv[1]=&d.value_1; fv[2]=&d.value_2; fv[3]=&d.value_3;
    fv[4]=&d.value_4; fv[5]=&d.value_5; fv[6]=&d.value_6; fv[7]=&d.value_7;
    fv[8]=&d.value_8;
    uv[9]=&d.value_9; uv[10]=&d.value_10;
    fv[11]=&d.value_11; fv[12]=&d.value_12; fv[13]=&d.value_13; fv[14]=&d.value_14;
    uv[15]=&d.value_15; fv[16]=&d.value_16; uv[17]=&d.value_17;
    fv[18]=&d.value_18; fv[19]=&d.value_19; fv[20]=&d.value_20; fv[21]=&d.value_21;
    fv[22]=&d.value_22; fv[23]=&d.value_23;
    uv[24]=&d.value_24; uv[25]=&d.value_25; uv[26]=&d.value_26; uv[27]=&d.value_27;
    uv[28]=&d.value_28; uv[29]=&d.value_29;
    fv[30]=&d.value_30; uv[31]=&d.value_31; fv[32]=&d.value_32;
    uv[33]=&d.value_33; uv[34]=&d.value_34; uv[35]=&d.value_35; uv[36]=&d.value_36;
    uv[37]=&d.value_37; uv[38]=&d.value_38; uv[39]=&d.value_39; uv[40]=&d.value_40;
    fv[41]=&d.value_41; fv[42]=&d.value_42; fv[43]=&d.value_43;
    uv[44]=&d.value_44; uv[45]=&d.value_45;
    fv[46]=&d.value_46;
    uv[47]=&d.value_47; uv[48]=&d.value_48; uv[49]=&d.value_49;
    uv[50]=&d.value_50; uv[51]=&d.value_51; uv[52]=&d.value_52;
    fv[53]=&d.value_53; fv[54]=&d.value_54; uv[55]=&d.value_55;
    fv[56]=&d.value_56; fv[57]=&d.value_57;
    uv[58]=&d.value_58; uv[59]=&d.value_59; uv[60]=&d.value_60;
    fv[61]=&d.value_61; fv[62]=&d.value_62;
    uv[63]=&d.value_63; uv[64]=&d.value_64; uv[65]=&d.value_65; uv[66]=&d.value_66;
    uv[67]=&d.value_67; uv[68]=&d.value_68; uv[69]=&d.value_69; uv[70]=&d.value_70;
    uv[71]=&d.value_71;

    char line[2048];
    bool headerSkipped = false;
    while (fgets(line, sizeof(line), f))
    {
        int len = (int)strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r')) line[--len] = '\0';
        if (len == 0 || line[0] == '#') continue;
        if (!headerSkipped) { headerSkipped = true; continue; }

        char* cols[72] = {}; int col = 0; char* p = line;
        cols[col++] = p;
        for (; *p && col < 72; ++p) if (*p == '\t') { *p = '\0'; cols[col++] = p+1; }
        for (int i = 0; i < col && i < 72; ++i)
        {
            if (!cols[i]) continue;
            if (k_IsUint[i]) { if (uv[i]) *uv[i] = (uint32_t)strtoul(cols[i], nullptr, 10); }
            else              { if (fv[i]) *fv[i] = strtof(cols[i], nullptr); }
        }
        break; // single data row
    }
    fclose(f);
}

void FbsDataView::RenderBallSettingListEditor(ContentsBinData& bin)
{
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.65f, 0.82f, 1.00f, 1.00f));
    ImGui::Text("ball_setting_list.bin");
    ImGui::PopStyleColor();

    // -- Export / Import buttons (right-aligned) --
    {
        const float ioGap=4.0f, exportBtnW=70.0f, importBtnW=70.0f;
        const float totalW=exportBtnW+ioGap+importBtnW;
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - totalW + ImGui::GetCursorPosX());
        if (ImGui::Button("Export##bset", ImVec2(exportBtnW, 0))) { std::string p=OpenTsvSaveDialog(L"ball_setting_list.tsv"); if(!p.empty()) ExportBallSettingListTsv(bin,p); }
        ImGui::SameLine(0, ioGap);
        if (!m_renderReadOnly && ImGui::Button("Import##bset", ImVec2(importBtnW, 0))) { std::string p=OpenTsvOpenDialog(); if(!p.empty()) ImportBallSettingListTsv(p,bin); }
    }

    ImGui::Separator();

    constexpr ImGuiTableFlags tFlags =
        ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg |
        ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersInnerV |
        ImGuiTableFlags_SizingFixedFit;

    if (!ImGui::BeginTable("##BallSettingGrid", 2, tFlags, ImGui::GetContentRegionAvail()))
        return;

    ImGui::TableSetupColumn("Field", ImGuiTableColumnFlags_WidthFixed, 160.0f);
    ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableHeadersRow();

    auto& d = bin.ballSettingData;

    static const bool k_BSIsUint[72] = {
        false,false,false,false,false,false,false,false,false,
        true,true,
        false,false,false,false,
        true,
        false,
        true,
        false,false,false,false,false,false,
        true,true,true,true,true,true,
        false,
        true,
        false,
        true,true,true,true,true,true,true,true,
        false,false,false,
        true,true,
        false,
        true,true,true,true,true,true,
        false,false,
        true,
        false,false,
        true,true,true,
        false,false,
        true,true,true,true,true,true,true,true,true
    };

    float* floatPtrs[72] = {};
    uint32_t* u32Ptrs[72] = {};

    floatPtrs[0]  = &d.value_0;  floatPtrs[1]  = &d.value_1;  floatPtrs[2]  = &d.value_2;
    floatPtrs[3]  = &d.value_3;  floatPtrs[4]  = &d.value_4;  floatPtrs[5]  = &d.value_5;
    floatPtrs[6]  = &d.value_6;  floatPtrs[7]  = &d.value_7;  floatPtrs[8]  = &d.value_8;
    u32Ptrs[9]    = &d.value_9;  u32Ptrs[10]   = &d.value_10;
    floatPtrs[11] = &d.value_11; floatPtrs[12] = &d.value_12; floatPtrs[13] = &d.value_13;
    floatPtrs[14] = &d.value_14; u32Ptrs[15]   = &d.value_15; floatPtrs[16] = &d.value_16;
    u32Ptrs[17]   = &d.value_17; floatPtrs[18] = &d.value_18; floatPtrs[19] = &d.value_19;
    floatPtrs[20] = &d.value_20; floatPtrs[21] = &d.value_21; floatPtrs[22] = &d.value_22;
    floatPtrs[23] = &d.value_23; u32Ptrs[24]   = &d.value_24; u32Ptrs[25]   = &d.value_25;
    u32Ptrs[26]   = &d.value_26; u32Ptrs[27]   = &d.value_27; u32Ptrs[28]   = &d.value_28;
    u32Ptrs[29]   = &d.value_29; floatPtrs[30] = &d.value_30; u32Ptrs[31]   = &d.value_31;
    floatPtrs[32] = &d.value_32; u32Ptrs[33]   = &d.value_33; u32Ptrs[34]   = &d.value_34;
    u32Ptrs[35]   = &d.value_35; u32Ptrs[36]   = &d.value_36; u32Ptrs[37]   = &d.value_37;
    u32Ptrs[38]   = &d.value_38; u32Ptrs[39]   = &d.value_39; u32Ptrs[40]   = &d.value_40;
    floatPtrs[41] = &d.value_41; floatPtrs[42] = &d.value_42; floatPtrs[43] = &d.value_43;
    u32Ptrs[44]   = &d.value_44; u32Ptrs[45]   = &d.value_45; floatPtrs[46] = &d.value_46;
    u32Ptrs[47]   = &d.value_47; u32Ptrs[48]   = &d.value_48; u32Ptrs[49]   = &d.value_49;
    u32Ptrs[50]   = &d.value_50; u32Ptrs[51]   = &d.value_51; u32Ptrs[52]   = &d.value_52;
    floatPtrs[53] = &d.value_53; floatPtrs[54] = &d.value_54; u32Ptrs[55]   = &d.value_55;
    floatPtrs[56] = &d.value_56; floatPtrs[57] = &d.value_57; u32Ptrs[58]   = &d.value_58;
    u32Ptrs[59]   = &d.value_59; u32Ptrs[60]   = &d.value_60; floatPtrs[61] = &d.value_61;
    floatPtrs[62] = &d.value_62; u32Ptrs[63]   = &d.value_63; u32Ptrs[64]   = &d.value_64;
    u32Ptrs[65]   = &d.value_65; u32Ptrs[66]   = &d.value_66; u32Ptrs[67]   = &d.value_67;
    u32Ptrs[68]   = &d.value_68; u32Ptrs[69]   = &d.value_69; u32Ptrs[70]   = &d.value_70;
    u32Ptrs[71]   = &d.value_71;

    for (int id = 0; id < 72; ++id)
    {
        ImGui::TableNextRow();
        ImGui::PushID(id);
        ImGui::TableSetColumnIndex(0);
        ImGui::TextUnformatted(FieldNames::BallSettingData[id]);
        ImGui::TableSetColumnIndex(1);
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (k_BSIsUint[id])
            ImGui::InputScalar("##v", ImGuiDataType_U32, u32Ptrs[id]);
        else
            ImGui::InputScalar("##v", ImGuiDataType_Float, floatPtrs[id]);
        ImGui::PopID();
    }

    ImGui::EndTable();
}

// -----------------------------------------------------------------------------
//  battle_common_list editor
// -----------------------------------------------------------------------------

static void ExportBattleCommonListTsv(const ContentsBinData& bin, const std::string& path)
{
    FILE* f = nullptr;
    if (fopen_s(&f, path.c_str(), "wb") != 0 || !f) return;

    // list_params: 10 scalar values (single row)
    fprintf(f, "#list_params\n");
    fprintf(f, "battleCommonValue3\tbattleCommonValue4\tbattleCommonValue6\tbattleCommonValue7\tbattleCommonValue8\tbattleCommonValue9\tbattleCommonValue10\tbattleCommonValue11\tbattleCommonValue12\tbattleCommonValue13\n");
    fprintf(f, "%u\t%u\t%.9g\t%.9g\t%.9g\t%u\t%.9g\t%u\t%u\t%u\n",
        bin.battleCommonValue3, bin.battleCommonValue4,
        bin.battleCommonValue6, bin.battleCommonValue7, bin.battleCommonValue8,
        bin.battleCommonValue9, bin.battleCommonValue10,
        bin.battleCommonValue11, bin.battleCommonValue12, bin.battleCommonValue13);

    // single_value_entries
    fprintf(f, "#single_value_entries\n");
    fprintf(f, "value\n");
    for (const auto& e : bin.battleCommonSingleValueEntries)
        fprintf(f, "%u\n", e.value);

    // character_scale_entries
    fprintf(f, "#character_scale_entries\n");
    fprintf(f, "hash_0\tvalue_1\tvalue_2\tvalue_3\tvalue_4\tvalue_5\tvalue_6\tvalue_7\n");
    for (const auto& e : bin.battleCommonCharacterScaleEntries)
        fprintf(f, "%u\t%.9g\t%.9g\t%.9g\t%.9g\t%.9g\t%.9g\t%.9g\n",
            e.hash_0, e.value_1, e.value_2, e.value_3, e.value_4, e.value_5, e.value_6, e.value_7);

    // pair_entries
    fprintf(f, "#pair_entries\n");
    fprintf(f, "value_0\tvalue_1\tvalue_2\n");
    for (const auto& e : bin.battleCommonPairEntries)
        fprintf(f, "%u\t%u\t%u\n", e.value_0, e.value_1, e.value_2);

    // misc_entries
    fprintf(f, "#misc_entries\n");
    fprintf(f, "value_0\tvalue_1\tvalue_2\n");
    for (const auto& e : bin.battleCommonMiscEntries)
        fprintf(f, "%.9g\t%.9g\t%.9g\n", e.value_0, e.value_1, e.value_2);

    fclose(f);
}

static void ImportBattleCommonListTsv(const std::string& path, ContentsBinData& bin)
{
    FILE* f = nullptr;
    if (fopen_s(&f, path.c_str(), "rb") != 0 || !f) return;

    std::vector<BattleCommonSingleValueEntry>    sv;
    std::vector<BattleCommonCharacterScaleEntry> cs;
    std::vector<BattleCommonPairEntry>           pe;
    std::vector<BattleCommonMiscEntry>           me;

    enum class Sec { None, Params, SingleValue, CharScale, Pair, Misc } sec = Sec::None;
    bool headerSkipped = false;

    char line[512];
    while (fgets(line, sizeof(line), f))
    {
        int len = (int)strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r')) line[--len] = '\0';
        if (len == 0) continue;

        if (line[0] == '#')
        {
            headerSkipped = false;
            if      (strcmp(line, "#list_params")              == 0) sec = Sec::Params;
            else if (strcmp(line, "#single_value_entries")     == 0) sec = Sec::SingleValue;
            else if (strcmp(line, "#character_scale_entries")  == 0) sec = Sec::CharScale;
            else if (strcmp(line, "#pair_entries")             == 0) sec = Sec::Pair;
            else if (strcmp(line, "#misc_entries")             == 0) sec = Sec::Misc;
            else                                                      sec = Sec::None;
            continue;
        }
        if (!headerSkipped) { headerSkipped = true; continue; }

        if (sec == Sec::Params)
        {
            char* cols[10] = {}; int col = 0; char* p = line;
            cols[col++] = p;
            for (; *p && col < 10; ++p) if (*p == '\t') { *p = '\0'; cols[col++] = p+1; }
            if (col < 10) continue;
            bin.battleCommonValue3  = (uint32_t)strtoul(cols[0], nullptr, 10);
            bin.battleCommonValue4  = (uint32_t)strtoul(cols[1], nullptr, 10);
            bin.battleCommonValue6  = strtof(cols[2], nullptr);
            bin.battleCommonValue7  = strtof(cols[3], nullptr);
            bin.battleCommonValue8  = strtof(cols[4], nullptr);
            bin.battleCommonValue9  = (uint32_t)strtoul(cols[5], nullptr, 10);
            bin.battleCommonValue10 = strtof(cols[6], nullptr);
            bin.battleCommonValue11 = (uint32_t)strtoul(cols[7], nullptr, 10);
            bin.battleCommonValue12 = (uint32_t)strtoul(cols[8], nullptr, 10);
            bin.battleCommonValue13 = (uint32_t)strtoul(cols[9], nullptr, 10);
        }
        else if (sec == Sec::SingleValue)
        {
            BattleCommonSingleValueEntry e;
            e.value = (uint32_t)strtoul(line, nullptr, 10);
            sv.push_back(e);
        }
        else if (sec == Sec::CharScale)
        {
            char* cols[8] = {}; int col = 0; char* p = line;
            cols[col++] = p;
            for (; *p && col < 8; ++p) if (*p == '\t') { *p = '\0'; cols[col++] = p+1; }
            if (col < 8) continue;
            BattleCommonCharacterScaleEntry e;
            e.hash_0  = (uint32_t)strtoul(cols[0], nullptr, 10);
            e.value_1 = strtof(cols[1], nullptr); e.value_2 = strtof(cols[2], nullptr);
            e.value_3 = strtof(cols[3], nullptr); e.value_4 = strtof(cols[4], nullptr);
            e.value_5 = strtof(cols[5], nullptr); e.value_6 = strtof(cols[6], nullptr);
            e.value_7 = strtof(cols[7], nullptr);
            cs.push_back(e);
        }
        else if (sec == Sec::Pair)
        {
            char* cols[3] = {}; int col = 0; char* p = line;
            cols[col++] = p;
            for (; *p && col < 3; ++p) if (*p == '\t') { *p = '\0'; cols[col++] = p+1; }
            if (col < 3) continue;
            BattleCommonPairEntry e;
            e.value_0 = (uint32_t)strtoul(cols[0], nullptr, 10);
            e.value_1 = (uint32_t)strtoul(cols[1], nullptr, 10);
            e.value_2 = (uint32_t)strtoul(cols[2], nullptr, 10);
            pe.push_back(e);
        }
        else if (sec == Sec::Misc)
        {
            char* cols[3] = {}; int col = 0; char* p = line;
            cols[col++] = p;
            for (; *p && col < 3; ++p) if (*p == '\t') { *p = '\0'; cols[col++] = p+1; }
            if (col < 3) continue;
            BattleCommonMiscEntry e;
            e.value_0 = strtof(cols[0], nullptr);
            e.value_1 = strtof(cols[1], nullptr);
            e.value_2 = strtof(cols[2], nullptr);
            me.push_back(e);
        }
    }
    fclose(f);

    bin.battleCommonSingleValueEntries    = std::move(sv);
    bin.battleCommonCharacterScaleEntries = std::move(cs);
    bin.battleCommonPairEntries           = std::move(pe);
    bin.battleCommonMiscEntries           = std::move(me);
}

void FbsDataView::RenderBattleCommonListEditor(ContentsBinData& bin)
{
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.65f, 0.82f, 1.00f, 1.00f));
    ImGui::Text("battle_common_list.bin");
    ImGui::PopStyleColor();

    // -- Export / Import buttons (right-aligned) --
    {
        const float ioGap=4.0f, exportBtnW=70.0f, importBtnW=70.0f;
        const float totalW=exportBtnW+ioGap+importBtnW;
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - totalW + ImGui::GetCursorPosX());
        if (ImGui::Button("Export##bcommon", ImVec2(exportBtnW, 0))) { std::string p=OpenTsvSaveDialog(L"battle_common_list.tsv"); if(!p.empty()) ExportBattleCommonListTsv(bin,p); }
        ImGui::SameLine(0, ioGap);
        if (!m_renderReadOnly && ImGui::Button("Import##bcommon", ImVec2(importBtnW, 0))) { std::string p=OpenTsvOpenDialog(); if(!p.empty()) ImportBattleCommonListTsv(p,bin); }
    }

    ImGui::Separator();

    constexpr ImGuiTableFlags tFlags =
        ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg |
        ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersInnerV |
        ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable |
        ImGuiTableFlags_Hideable | ImGuiTableFlags_SizingFixedFit;

    if (!ImGui::BeginTabBar("##BCLTabs"))
        return;

    if (ImGui::BeginTabItem("settings"))
    {
        ImGui::SetNextItemWidth(120.0f); ImGui::InputScalar("battleCommonValue3",  ImGuiDataType_U32,  &bin.battleCommonValue3);
        ImGui::SetNextItemWidth(120.0f); ImGui::InputScalar("battleCommonValue4",  ImGuiDataType_U32,  &bin.battleCommonValue4);
        ImGui::SetNextItemWidth(120.0f); ImGui::InputScalar("battleCommonValue6",  ImGuiDataType_Float,&bin.battleCommonValue6);
        ImGui::SetNextItemWidth(120.0f); ImGui::InputScalar("battleCommonValue7",  ImGuiDataType_Float,&bin.battleCommonValue7);
        ImGui::SetNextItemWidth(120.0f); ImGui::InputScalar("battleCommonValue8",  ImGuiDataType_Float,&bin.battleCommonValue8);
        ImGui::SetNextItemWidth(120.0f); ImGui::InputScalar("battleCommonValue9",  ImGuiDataType_U32,  &bin.battleCommonValue9);
        ImGui::SetNextItemWidth(120.0f); ImGui::InputScalar("battleCommonValue10", ImGuiDataType_Float,&bin.battleCommonValue10);
        ImGui::SetNextItemWidth(120.0f); ImGui::InputScalar("battleCommonValue11", ImGuiDataType_U32,  &bin.battleCommonValue11);
        ImGui::SetNextItemWidth(120.0f); ImGui::InputScalar("battleCommonValue12", ImGuiDataType_U32,  &bin.battleCommonValue12);
        ImGui::SetNextItemWidth(120.0f); ImGui::InputScalar("battleCommonValue13", ImGuiDataType_U32,  &bin.battleCommonValue13);
        ImGui::EndTabItem();
    }

    if (ImGui::BeginTabItem("single_value_entries"))
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.42f, 0.42f, 0.54f, 1.00f));
        ImGui::Text("(%d entries)", (int)bin.battleCommonSingleValueEntries.size());
        ImGui::PopStyleColor();
        const float addBtnW = 100.0f;
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - addBtnW + ImGui::GetCursorPosX());
        if (!m_renderReadOnly && ImGui::Button("+ Add Entry", ImVec2(addBtnW, 0)))
            bin.battleCommonSingleValueEntries.push_back(BattleCommonSingleValueEntry{});

        if (ImGui::BeginTable("##BCSVTable", 2, tFlags, ImGui::GetContentRegionAvail()))
        {
            ImGui::TableSetupScrollFreeze(1, 1);
            ImGui::TableSetupColumn("#",                                   ImGuiTableColumnFlags_WidthFixed, ColumnWidths::kRowCtrl);
            ImGui::TableSetupColumn(FieldNames::BattleCommonSingleValue[0],ImGuiTableColumnFlags_WidthFixed, ColumnWidths::kBattleCommonGeneric);
            ImGui::TableHeadersRow();

            int deleteIdx = -1;
            ImGuiListClipper clipper;
            clipper.Begin((int)bin.battleCommonSingleValueEntries.size());
            while (clipper.Step())
            {
                for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i)
                {
                    auto& e = bin.battleCommonSingleValueEntries[i];
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
                    if (!m_renderReadOnly && ImGui::SmallButton("X")) deleteIdx = i;
                    ImGui::PopStyleColor(3);

                    ImGui::TableSetColumnIndex(1);
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    ImGui::InputScalar("##v", ImGuiDataType_U32, &e.value);

                    ImGui::PopID();
                }
            }
            if (!m_renderReadOnly && deleteIdx >= 0)
                bin.battleCommonSingleValueEntries.erase(bin.battleCommonSingleValueEntries.begin() + deleteIdx);

            ImGui::EndTable();
        }
        ImGui::EndTabItem();
    }

    if (ImGui::BeginTabItem("character_scale_entries"))
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.42f, 0.42f, 0.54f, 1.00f));
        ImGui::Text("(%d entries)", (int)bin.battleCommonCharacterScaleEntries.size());
        ImGui::PopStyleColor();
        const float addBtnW = 100.0f;
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - addBtnW + ImGui::GetCursorPosX());
        if (!m_renderReadOnly && ImGui::Button("+ Add Entry##cs", ImVec2(addBtnW, 0)))
            bin.battleCommonCharacterScaleEntries.push_back(BattleCommonCharacterScaleEntry{});

        if (ImGui::BeginTable("##BCCSTable", 9, tFlags, ImGui::GetContentRegionAvail()))
        {
            ImGui::TableSetupScrollFreeze(1, 1);
            ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, ColumnWidths::kRowCtrl);
            for (int fi = 0; fi < 8; ++fi)
                ImGui::TableSetupColumn(FieldNames::BattleCommonCharacterScale[fi], ImGuiTableColumnFlags_WidthFixed,
                    fi == 0 ? ColumnWidths::kBattleCommonCharacterScale0 : ColumnWidths::kBattleCommonCharacterScaleRest);
            ImGui::TableHeadersRow();

            int deleteIdx = -1;
            ImGuiListClipper clipper;
            clipper.Begin((int)bin.battleCommonCharacterScaleEntries.size());
            while (clipper.Step())
            {
                for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i)
                {
                    auto& e = bin.battleCommonCharacterScaleEntries[i];
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
                    if (!m_renderReadOnly && ImGui::SmallButton("X")) deleteIdx = i;
                    ImGui::PopStyleColor(3);

                    ImGui::TableSetColumnIndex(1); ImGui::SetNextItemWidth(-FLT_MIN); ImGui::InputScalar("##h0", ImGuiDataType_U32,   &e.hash_0);
                    ImGui::TableSetColumnIndex(2); ImGui::SetNextItemWidth(-FLT_MIN); ImGui::InputScalar("##v1", ImGuiDataType_Float,  &e.value_1);
                    ImGui::TableSetColumnIndex(3); ImGui::SetNextItemWidth(-FLT_MIN); ImGui::InputScalar("##v2", ImGuiDataType_Float,  &e.value_2);
                    ImGui::TableSetColumnIndex(4); ImGui::SetNextItemWidth(-FLT_MIN); ImGui::InputScalar("##v3", ImGuiDataType_Float,  &e.value_3);
                    ImGui::TableSetColumnIndex(5); ImGui::SetNextItemWidth(-FLT_MIN); ImGui::InputScalar("##v4", ImGuiDataType_Float,  &e.value_4);
                    ImGui::TableSetColumnIndex(6); ImGui::SetNextItemWidth(-FLT_MIN); ImGui::InputScalar("##v5", ImGuiDataType_Float,  &e.value_5);
                    ImGui::TableSetColumnIndex(7); ImGui::SetNextItemWidth(-FLT_MIN); ImGui::InputScalar("##v6", ImGuiDataType_Float,  &e.value_6);
                    ImGui::TableSetColumnIndex(8); ImGui::SetNextItemWidth(-FLT_MIN); ImGui::InputScalar("##v7", ImGuiDataType_Float,  &e.value_7);

                    ImGui::PopID();
                }
            }
            if (!m_renderReadOnly && deleteIdx >= 0)
                bin.battleCommonCharacterScaleEntries.erase(bin.battleCommonCharacterScaleEntries.begin() + deleteIdx);

            ImGui::EndTable();
        }
        ImGui::EndTabItem();
    }

    if (ImGui::BeginTabItem("pair_entries"))
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.42f, 0.42f, 0.54f, 1.00f));
        ImGui::Text("(%d entries)", (int)bin.battleCommonPairEntries.size());
        ImGui::PopStyleColor();
        const float addBtnW = 100.0f;
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - addBtnW + ImGui::GetCursorPosX());
        if (!m_renderReadOnly && ImGui::Button("+ Add Entry##pe", ImVec2(addBtnW, 0)))
            bin.battleCommonPairEntries.push_back(BattleCommonPairEntry{});

        if (ImGui::BeginTable("##BCPairTable", 4, tFlags, ImGui::GetContentRegionAvail()))
        {
            ImGui::TableSetupScrollFreeze(1, 1);
            ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, ColumnWidths::kRowCtrl);
            for (int fi = 0; fi < 3; ++fi)
                ImGui::TableSetupColumn(FieldNames::BattleCommonPair[fi], ImGuiTableColumnFlags_WidthFixed, ColumnWidths::kBattleCommonGeneric);
            ImGui::TableHeadersRow();

            int deleteIdx = -1;
            ImGuiListClipper clipper;
            clipper.Begin((int)bin.battleCommonPairEntries.size());
            while (clipper.Step())
            {
                for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i)
                {
                    auto& e = bin.battleCommonPairEntries[i];
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
                    if (!m_renderReadOnly && ImGui::SmallButton("X")) deleteIdx = i;
                    ImGui::PopStyleColor(3);

                    ImGui::TableSetColumnIndex(1); ImGui::SetNextItemWidth(-FLT_MIN); ImGui::InputScalar("##v0", ImGuiDataType_U32, &e.value_0);
                    ImGui::TableSetColumnIndex(2); ImGui::SetNextItemWidth(-FLT_MIN); ImGui::InputScalar("##v1", ImGuiDataType_U32, &e.value_1);
                    ImGui::TableSetColumnIndex(3); ImGui::SetNextItemWidth(-FLT_MIN); ImGui::InputScalar("##v2", ImGuiDataType_U32, &e.value_2);

                    ImGui::PopID();
                }
            }
            if (!m_renderReadOnly && deleteIdx >= 0)
                bin.battleCommonPairEntries.erase(bin.battleCommonPairEntries.begin() + deleteIdx);

            ImGui::EndTable();
        }
        ImGui::EndTabItem();
    }

    if (ImGui::BeginTabItem("misc_entries"))
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.42f, 0.42f, 0.54f, 1.00f));
        ImGui::Text("(%d entries)", (int)bin.battleCommonMiscEntries.size());
        ImGui::PopStyleColor();
        const float addBtnW = 100.0f;
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - addBtnW + ImGui::GetCursorPosX());
        if (!m_renderReadOnly && ImGui::Button("+ Add Entry##me", ImVec2(addBtnW, 0)))
            bin.battleCommonMiscEntries.push_back(BattleCommonMiscEntry{});

        if (ImGui::BeginTable("##BCMiscTable", 4, tFlags, ImGui::GetContentRegionAvail()))
        {
            ImGui::TableSetupScrollFreeze(1, 1);
            ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, ColumnWidths::kRowCtrl);
            for (int fi = 0; fi < 3; ++fi)
                ImGui::TableSetupColumn(FieldNames::BattleCommonMisc[fi], ImGuiTableColumnFlags_WidthFixed, ColumnWidths::kBattleCommonGeneric);
            ImGui::TableHeadersRow();

            int deleteIdx = -1;
            ImGuiListClipper clipper;
            clipper.Begin((int)bin.battleCommonMiscEntries.size());
            while (clipper.Step())
            {
                for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i)
                {
                    auto& e = bin.battleCommonMiscEntries[i];
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
                    if (!m_renderReadOnly && ImGui::SmallButton("X")) deleteIdx = i;
                    ImGui::PopStyleColor(3);

                    ImGui::TableSetColumnIndex(1); ImGui::SetNextItemWidth(-FLT_MIN); ImGui::InputScalar("##v0", ImGuiDataType_Float, &e.value_0);
                    ImGui::TableSetColumnIndex(2); ImGui::SetNextItemWidth(-FLT_MIN); ImGui::InputScalar("##v1", ImGuiDataType_Float, &e.value_1);
                    ImGui::TableSetColumnIndex(3); ImGui::SetNextItemWidth(-FLT_MIN); ImGui::InputScalar("##v2", ImGuiDataType_Float, &e.value_2);

                    ImGui::PopID();
                }
            }
            if (!m_renderReadOnly && deleteIdx >= 0)
                bin.battleCommonMiscEntries.erase(bin.battleCommonMiscEntries.begin() + deleteIdx);

            ImGui::EndTable();
        }
        ImGui::EndTabItem();
    }

    ImGui::EndTabBar();
}

// -----------------------------------------------------------------------------
//  battle_cpu_list editor
// -----------------------------------------------------------------------------

static void WriteBattleCpuRankEntry(FILE* f, const BattleCpuRankEntry& e)
{
    for (int vi = 0; vi < 47; ++vi)
    {
        if (vi > 0) fputc('\t', f);
        fprintf(f, "%u", e.values[vi]);
    }
    fprintf(f, "\t%s\n", e.rank_label);
}

static void ExportBattleCpuListTsv(const ContentsBinData& bin, const std::string& path)
{
    FILE* f = nullptr;
    if (fopen_s(&f, path.c_str(), "wb") != 0 || !f) return;

    // rank_entries
    fprintf(f, "#rank_entries\n");
    fprintf(f, "values[0]");
    for (int vi = 1; vi < 47; ++vi) fprintf(f, "\tvalues[%d]", vi);
    fprintf(f, "\trank_label\n");
    for (const auto& e : bin.battleCpuRankEntries)
        WriteBattleCpuRankEntry(f, e);

    // step_entries
    fprintf(f, "#step_entries\n");
    fprintf(f, "value_0\tvalue_1\tvalue_2\tvalue_3\n");
    for (const auto& e : bin.battleCpuStepEntries)
        fprintf(f, "%u\t%u\t%u\t%u\n", e.value_0, e.value_1, e.value_2, e.value_3);

    // param_values
    fprintf(f, "#param_values\n");
    for (int i = 0; i < (int)bin.battleCpuParamValues.size(); ++i)
    {
        if (i > 0) fputc('\t', f);
        fprintf(f, "%d", bin.battleCpuParamValues[i]);
    }
    fputc('\n', f);

    // rank_ex_entry
    fprintf(f, "#rank_ex_entry\n");
    fprintf(f, "values[0]");
    for (int vi = 1; vi < 47; ++vi) fprintf(f, "\tvalues[%d]", vi);
    fprintf(f, "\trank_label\n");
    WriteBattleCpuRankEntry(f, bin.battleCpuRankExEntry);

    fclose(f);
}

static bool ParseBattleCpuRankEntry(char* line, BattleCpuRankEntry& out)
{
    char* cols[48] = {}; int col = 0; char* p = line;
    cols[col++] = p;
    for (; *p && col < 48; ++p) if (*p == '\t') { *p = '\0'; cols[col++] = p+1; }
    if (col < 48) return false;
    for (int vi = 0; vi < 47; ++vi)
        out.values[vi] = (uint32_t)strtoul(cols[vi], nullptr, 10);
    strncpy_s(out.rank_label, sizeof(out.rank_label), cols[47], _TRUNCATE);
    return true;
}

static void ImportBattleCpuListTsv(const std::string& path, ContentsBinData& bin)
{
    FILE* f = nullptr;
    if (fopen_s(&f, path.c_str(), "rb") != 0 || !f) return;

    std::vector<BattleCpuRankEntry> rankEntries;
    std::vector<BattleCpuStepEntry> stepEntries;
    std::vector<int32_t>            paramValues;
    BattleCpuRankEntry              rankExEntry{};

    enum class Sec { None, Rank, Step, Param, RankEx } sec = Sec::None;
    bool headerSkipped = false;

    char line[2048];
    while (fgets(line, sizeof(line), f))
    {
        int len = (int)strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r')) line[--len] = '\0';
        if (len == 0) continue;

        if (line[0] == '#')
        {
            headerSkipped = false;
            if      (strcmp(line, "#rank_entries") == 0) sec = Sec::Rank;
            else if (strcmp(line, "#step_entries") == 0) sec = Sec::Step;
            else if (strcmp(line, "#param_values") == 0) sec = Sec::Param;
            else if (strcmp(line, "#rank_ex_entry") == 0) sec = Sec::RankEx;
            else                                           sec = Sec::None;
            continue;
        }
        if (!headerSkipped) { headerSkipped = true; continue; }

        if (sec == Sec::Rank)
        {
            BattleCpuRankEntry e{};
            if (ParseBattleCpuRankEntry(line, e)) rankEntries.push_back(e);
        }
        else if (sec == Sec::Step)
        {
            char* cols[4] = {}; int col = 0; char* p = line;
            cols[col++] = p;
            for (; *p && col < 4; ++p) if (*p == '\t') { *p = '\0'; cols[col++] = p+1; }
            if (col < 4) continue;
            BattleCpuStepEntry e;
            e.value_0 = (uint32_t)strtoul(cols[0], nullptr, 10);
            e.value_1 = (uint32_t)strtoul(cols[1], nullptr, 10);
            e.value_2 = (uint32_t)strtoul(cols[2], nullptr, 10);
            e.value_3 = (uint32_t)strtoul(cols[3], nullptr, 10);
            stepEntries.push_back(e);
        }
        else if (sec == Sec::Param)
        {
            char* p = line;
            while (*p)
            {
                char* start = p;
                while (*p && *p != '\t') ++p;
                char saved = *p; *p = '\0';
                paramValues.push_back((int32_t)strtol(start, nullptr, 10));
                if (saved) ++p; else break;
            }
        }
        else if (sec == Sec::RankEx)
        {
            ParseBattleCpuRankEntry(line, rankExEntry);
        }
    }
    fclose(f);

    bin.battleCpuRankEntries = std::move(rankEntries);
    bin.battleCpuStepEntries = std::move(stepEntries);
    bin.battleCpuParamValues = std::move(paramValues);
    bin.battleCpuRankExEntry = rankExEntry;
}

void FbsDataView::RenderBattleCpuListEditor(ContentsBinData& bin)
{
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.65f, 0.82f, 1.00f, 1.00f));
    ImGui::Text("battle_cpu_list.bin");
    ImGui::PopStyleColor();

    // -- Export / Import buttons (right-aligned) --
    {
        const float ioGap=4.0f, exportBtnW=70.0f, importBtnW=70.0f;
        const float totalW=exportBtnW+ioGap+importBtnW;
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - totalW + ImGui::GetCursorPosX());
        if (ImGui::Button("Export##bcpu", ImVec2(exportBtnW, 0))) { std::string p=OpenTsvSaveDialog(L"battle_cpu_list.tsv"); if(!p.empty()) ExportBattleCpuListTsv(bin,p); }
        ImGui::SameLine(0, ioGap);
        if (!m_renderReadOnly && ImGui::Button("Import##bcpu", ImVec2(importBtnW, 0))) { std::string p=OpenTsvOpenDialog(); if(!p.empty()) ImportBattleCpuListTsv(p,bin); }
    }

    // rank_ex_entry header section
    {
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.42f, 0.42f, 0.54f, 1.00f));
        ImGui::Text("| rank_ex_entry:");
        ImGui::PopStyleColor();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(120.0f);
        ImGui::InputText("##rex_label", bin.battleCpuRankExEntry.rank_label, sizeof(bin.battleCpuRankExEntry.rank_label));
    }

    ImGui::Separator();

    constexpr ImGuiTableFlags tFlags =
        ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg |
        ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersInnerV |
        ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable |
        ImGuiTableFlags_Hideable | ImGuiTableFlags_SizingFixedFit;

    if (!ImGui::BeginTabBar("##BCpuTabs"))
        return;

    if (ImGui::BeginTabItem("rank_entries"))
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.42f, 0.42f, 0.54f, 1.00f));
        ImGui::Text("(%d entries)", (int)bin.battleCpuRankEntries.size());
        ImGui::PopStyleColor();
        const float addBtnW = 100.0f;
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - addBtnW + ImGui::GetCursorPosX());
        if (!m_renderReadOnly && ImGui::Button("+ Add Entry", ImVec2(addBtnW, 0)))
            bin.battleCpuRankEntries.push_back(BattleCpuRankEntry{});

        // 49 cols: # + values[0..46] + rank_label
        if (ImGui::BeginTable("##BCRankTable", 49, tFlags, ImGui::GetContentRegionAvail()))
        {
            ImGui::TableSetupScrollFreeze(1, 1);
            ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, ColumnWidths::kRowCtrl);
            for (int vi = 0; vi < 47; ++vi)
                ImGui::TableSetupColumn(FieldNames::BattleCpuRank[vi], ImGuiTableColumnFlags_WidthFixed, ColumnWidths::kBattleCpuRankGeneric);
            ImGui::TableSetupColumn(FieldNames::BattleCpuRank[47], ImGuiTableColumnFlags_WidthFixed, ColumnWidths::kBattleCpuRank47);
            ImGui::TableHeadersRow();

            int deleteIdx = -1;
            ImGuiListClipper clipper;
            clipper.Begin((int)bin.battleCpuRankEntries.size());
            while (clipper.Step())
            {
                for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i)
                {
                    auto& e = bin.battleCpuRankEntries[i];
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
                    if (!m_renderReadOnly && ImGui::SmallButton("X")) deleteIdx = i;
                    ImGui::PopStyleColor(3);

                    for (int vi = 0; vi < 47; ++vi)
                    {
                        ImGui::TableSetColumnIndex(vi + 1);
                        ImGui::SetNextItemWidth(-FLT_MIN);
                        char cellId[16];
                        snprintf(cellId, sizeof(cellId), "##rv%d", vi);
                        ImGui::InputScalar(cellId, ImGuiDataType_U32, &e.values[vi]);
                    }
                    ImGui::TableSetColumnIndex(48);
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    ImGui::InputText("##rl", e.rank_label, sizeof(e.rank_label));

                    ImGui::PopID();
                }
            }
            if (!m_renderReadOnly && deleteIdx >= 0)
                bin.battleCpuRankEntries.erase(bin.battleCpuRankEntries.begin() + deleteIdx);

            ImGui::EndTable();
        }
        ImGui::EndTabItem();
    }

    if (ImGui::BeginTabItem("step_entries"))
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.42f, 0.42f, 0.54f, 1.00f));
        ImGui::Text("(%d entries)", (int)bin.battleCpuStepEntries.size());
        ImGui::PopStyleColor();
        const float addBtnW = 100.0f;
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - addBtnW + ImGui::GetCursorPosX());
        if (!m_renderReadOnly && ImGui::Button("+ Add Entry##step", ImVec2(addBtnW, 0)))
            bin.battleCpuStepEntries.push_back(BattleCpuStepEntry{});

        if (ImGui::BeginTable("##BCStepTable", 5, tFlags, ImGui::GetContentRegionAvail()))
        {
            ImGui::TableSetupScrollFreeze(1, 1);
            ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, ColumnWidths::kRowCtrl);
            for (int fi = 0; fi < 4; ++fi)
                ImGui::TableSetupColumn(FieldNames::BattleCpuStep[fi], ImGuiTableColumnFlags_WidthFixed, ColumnWidths::kBattleCpuStepGeneric);
            ImGui::TableHeadersRow();

            int deleteIdx = -1;
            ImGuiListClipper clipper;
            clipper.Begin((int)bin.battleCpuStepEntries.size());
            while (clipper.Step())
            {
                for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i)
                {
                    auto& e = bin.battleCpuStepEntries[i];
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
                    if (!m_renderReadOnly && ImGui::SmallButton("X")) deleteIdx = i;
                    ImGui::PopStyleColor(3);

                    ImGui::TableSetColumnIndex(1); ImGui::SetNextItemWidth(-FLT_MIN); ImGui::InputScalar("##v0", ImGuiDataType_U32, &e.value_0);
                    ImGui::TableSetColumnIndex(2); ImGui::SetNextItemWidth(-FLT_MIN); ImGui::InputScalar("##v1", ImGuiDataType_U32, &e.value_1);
                    ImGui::TableSetColumnIndex(3); ImGui::SetNextItemWidth(-FLT_MIN); ImGui::InputScalar("##v2", ImGuiDataType_U32, &e.value_2);
                    ImGui::TableSetColumnIndex(4); ImGui::SetNextItemWidth(-FLT_MIN); ImGui::InputScalar("##v3", ImGuiDataType_U32, &e.value_3);

                    ImGui::PopID();
                }
            }
            if (!m_renderReadOnly && deleteIdx >= 0)
                bin.battleCpuStepEntries.erase(bin.battleCpuStepEntries.begin() + deleteIdx);

            ImGui::EndTable();
        }
        ImGui::EndTabItem();
    }

    if (ImGui::BeginTabItem("param_values"))
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.42f, 0.42f, 0.54f, 1.00f));
        ImGui::Text("(%d values)", (int)bin.battleCpuParamValues.size());
        ImGui::PopStyleColor();
        ImGui::SameLine();
        if (ImGui::SmallButton("+ Add##bcpv"))
            bin.battleCpuParamValues.push_back(0);

        const float avH = ImGui::GetContentRegionAvail().y;
        ImGui::BeginChild("##BCParamScroll", ImVec2(0.0f, avH), false);
        int deleteIdx = -1;
        for (int i = 0; i < (int)bin.battleCpuParamValues.size(); ++i)
        {
            ImGui::PushID(i);
            ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.55f, 0.12f, 0.12f, 1.00f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.78f, 0.18f, 0.18f, 1.00f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.65f, 0.15f, 0.15f, 1.00f));
            if (!m_renderReadOnly && ImGui::SmallButton("X")) deleteIdx = i;
            ImGui::PopStyleColor(3);
            ImGui::SameLine();
            ImGui::SetNextItemWidth(120.0f);
            ImGui::InputScalar("##pv", ImGuiDataType_S32, &bin.battleCpuParamValues[i]);
            ImGui::PopID();
        }
        if (!m_renderReadOnly && deleteIdx >= 0)
            bin.battleCpuParamValues.erase(bin.battleCpuParamValues.begin() + deleteIdx);
        ImGui::EndChild();
        ImGui::EndTabItem();
    }

    ImGui::EndTabBar();
}

// -----------------------------------------------------------------------------
//  rank_list editor
// -----------------------------------------------------------------------------

static void ExportRankListTsv(const ContentsBinData& bin, const std::string& path)
{
    FILE* f = nullptr;
    if (fopen_s(&f, path.c_str(), "wb") != 0 || !f) return;

    fprintf(f, "group_id\thash\ttext_key\tname\trank\n");
    for (const auto& grp : bin.rankGroups)
        for (const auto& e : grp.entries)
            fprintf(f, "%u\t%u\t%s\t%s\t%u\n", grp.group_id, e.hash, e.text_key, e.name, e.rank);

    fclose(f);
}

static void ImportRankListTsv(const std::string& path, ContentsBinData& bin)
{
    FILE* f = nullptr;
    if (fopen_s(&f, path.c_str(), "rb") != 0 || !f) return;

    std::unordered_map<uint32_t, std::vector<RankItem>> groupMap;
    std::vector<uint32_t> groupOrder;

    bool headerSkipped = false;
    char line[1024];
    while (fgets(line, sizeof(line), f))
    {
        int len = (int)strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r')) line[--len] = '\0';
        if (len == 0 || line[0] == '#') continue;
        if (!headerSkipped) { headerSkipped = true; continue; }

        char* cols[5] = {}; int col = 0; char* p = line;
        cols[col++] = p;
        for (; *p && col < 5; ++p) if (*p == '\t') { *p = '\0'; cols[col++] = p+1; }
        if (col < 5) continue;

        uint32_t gid = (uint32_t)strtoul(cols[0], nullptr, 10);
        if (groupMap.find(gid) == groupMap.end())
            groupOrder.push_back(gid);

        RankItem e;
        e.hash = (uint32_t)strtoul(cols[1], nullptr, 10);
        strncpy_s(e.text_key, sizeof(e.text_key), cols[2], _TRUNCATE);
        strncpy_s(e.name,     sizeof(e.name),     cols[3], _TRUNCATE);
        e.rank = (uint32_t)strtoul(cols[4], nullptr, 10);
        groupMap[gid].push_back(e);
    }
    fclose(f);

    std::vector<RankGroup> groups;
    groups.reserve(groupOrder.size());
    for (uint32_t gid : groupOrder)
    {
        RankGroup grp;
        grp.group_id = gid;
        grp.entries  = std::move(groupMap[gid]);
        groups.push_back(std::move(grp));
    }
    bin.rankGroups = std::move(groups);
}

void FbsDataView::RenderRankListEditor(ContentsBinData& bin)
{
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.65f, 0.82f, 1.00f, 1.00f));
    ImGui::Text("rank_list.bin");
    ImGui::PopStyleColor();

    // -- Export / Import buttons (right-aligned) --
    {
        const float ioGap=4.0f, exportBtnW=70.0f, importBtnW=70.0f;
        const float totalW=exportBtnW+ioGap+importBtnW;
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - totalW + ImGui::GetCursorPosX());
        if (ImGui::Button("Export##rank", ImVec2(exportBtnW, 0))) { std::string p=OpenTsvSaveDialog(L"rank_list.tsv"); if(!p.empty()) ExportRankListTsv(bin,p); }
        ImGui::SameLine(0, ioGap);
        if (!m_renderReadOnly && ImGui::Button("Import##rank", ImVec2(importBtnW, 0))) { std::string p=OpenTsvOpenDialog(); if(!p.empty()) ImportRankListTsv(p,bin); }
    }

    ImGui::Separator();

    constexpr ImGuiTableFlags tFlags =
        ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg |
        ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersInnerV |
        ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable |
        ImGuiTableFlags_Hideable | ImGuiTableFlags_SizingFixedFit;

    const float totalH = ImGui::GetContentRegionAvail().y;
    const float totalW = ImGui::GetContentRegionAvail().x;
    const float leftW  = 180.0f;

    // Left panel -- group list
    ImGui::BeginChild("##RankGroupList", ImVec2(leftW, totalH), true);

    if (ImGui::SmallButton("+ Add Group"))
        bin.rankGroups.push_back(RankGroup{});

    static int s_selectedRankGroup = -1;

    for (int gi = 0; gi < (int)bin.rankGroups.size(); ++gi)
    {
        auto& grp = bin.rankGroups[gi];
        ImGui::PushID(gi);

        char label[64];
        snprintf(label, sizeof(label), "Group %u", grp.group_id);
        bool selected = (gi == s_selectedRankGroup);
        if (selected)
        {
            ImGui::PushStyleColor(ImGuiCol_Header,        ImVec4(0.22f, 0.40f, 0.72f, 1.00f));
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.28f, 0.48f, 0.82f, 1.00f));
            ImGui::PushStyleColor(ImGuiCol_HeaderActive,  ImVec4(0.25f, 0.44f, 0.78f, 1.00f));
        }
        if (ImGui::Selectable(label, selected))
            s_selectedRankGroup = gi;
        if (selected)
            ImGui::PopStyleColor(3);

        // Right-click: remove group
        if (ImGui::BeginPopupContextItem("##RankGrpCtx"))
        {
            if (ImGui::MenuItem("Remove Group"))
            {
                bin.rankGroups.erase(bin.rankGroups.begin() + gi);
                if (s_selectedRankGroup >= (int)bin.rankGroups.size())
                    s_selectedRankGroup = (int)bin.rankGroups.size() - 1;
                ImGui::EndPopup();
                ImGui::PopID();
                break;
            }
            ImGui::EndPopup();
        }

        ImGui::PopID();
    }

    ImGui::EndChild();
    ImGui::SameLine();

    // Right panel -- items for selected group
    ImGui::BeginChild("##RankItemPanel", ImVec2(totalW - leftW - 4.0f, totalH), false);

    if (s_selectedRankGroup >= 0 && s_selectedRankGroup < (int)bin.rankGroups.size())
    {
        auto& grp = bin.rankGroups[s_selectedRankGroup];

        ImGui::SetNextItemWidth(120.0f);
        ImGui::InputScalar("group_id", ImGuiDataType_U32, &grp.group_id);

        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.42f, 0.42f, 0.54f, 1.00f));
        ImGui::Text("(%d items)", (int)grp.entries.size());
        ImGui::PopStyleColor();
        const float addBtnW = 100.0f;
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - addBtnW + ImGui::GetCursorPosX());
        if (ImGui::Button("+ Add Item", ImVec2(addBtnW, 0)))
            grp.entries.push_back(RankItem{});

        if (ImGui::BeginTable("##RankItemTable", 5, tFlags, ImGui::GetContentRegionAvail()))
        {
            ImGui::TableSetupScrollFreeze(1, 1);
            ImGui::TableSetupColumn("#",                     ImGuiTableColumnFlags_WidthFixed, ColumnWidths::kRowCtrl);
            for (int fi = 0; fi < 4; ++fi)
                ImGui::TableSetupColumn(FieldNames::RankItem[fi], ImGuiTableColumnFlags_WidthFixed, ColumnWidths::kRankItem[fi]);
            ImGui::TableHeadersRow();

            int deleteIdx = -1;
            ImGuiListClipper clipper;
            clipper.Begin((int)grp.entries.size());
            while (clipper.Step())
            {
                for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i)
                {
                    auto& e = grp.entries[i];
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
                    if (!m_renderReadOnly && ImGui::SmallButton("X")) deleteIdx = i;
                    ImGui::PopStyleColor(3);

                    ImGui::TableSetColumnIndex(1); ImGui::SetNextItemWidth(-FLT_MIN); ImGui::InputScalar("##h",  ImGuiDataType_U32, &e.hash);
                    ImGui::TableSetColumnIndex(2); ImGui::SetNextItemWidth(-FLT_MIN); ImGui::InputText("##tk",   e.text_key, sizeof(e.text_key));
                    ImGui::TableSetColumnIndex(3); ImGui::SetNextItemWidth(-FLT_MIN); ImGui::InputText("##nm",   e.name,     sizeof(e.name));
                    ImGui::TableSetColumnIndex(4); ImGui::SetNextItemWidth(-FLT_MIN); ImGui::InputScalar("##rk", ImGuiDataType_U32, &e.rank);

                    ImGui::PopID();
                }
            }
            if (!m_renderReadOnly && deleteIdx >= 0)
                grp.entries.erase(grp.entries.begin() + deleteIdx);

            ImGui::EndTable();
        }
    }
    else
    {
        ImGui::TextDisabled("Select a group from the left panel.");
    }

    ImGui::EndChild();
}

// -----------------------------------------------------------------------------
//  assist_input_list editor
// -----------------------------------------------------------------------------

static void ExportAssistInputListTsv(const ContentsBinData& bin, const std::string& path)
{
    FILE* f = nullptr;
    if (fopen_s(&f, path.c_str(), "wb") != 0 || !f) return;

    fprintf(f, "character_hash");
    for (int vi = 0; vi < 58; ++vi) fprintf(f, "\tvalues[%d]", vi);
    fputc('\n', f);

    for (const auto& e : bin.assistInputEntries)
    {
        fprintf(f, "%u", e.character_hash);
        for (int vi = 0; vi < 58; ++vi) fprintf(f, "\t%d", e.values[vi]);
        fputc('\n', f);
    }
    fclose(f);
}

static std::vector<AssistInputEntry> ImportAssistInputListTsv(const std::string& path)
{
    std::vector<AssistInputEntry> result;
    FILE* f = nullptr;
    if (fopen_s(&f, path.c_str(), "rb") != 0 || !f) return result;

    bool headerSkipped = false;
    char line[2048];
    while (fgets(line, sizeof(line), f))
    {
        int len = (int)strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r')) line[--len] = '\0';
        if (len == 0 || line[0] == '#') continue;
        if (!headerSkipped) { headerSkipped = true; continue; }

        char* cols[59] = {}; int col = 0; char* p = line;
        cols[col++] = p;
        for (; *p && col < 59; ++p) if (*p == '\t') { *p = '\0'; cols[col++] = p+1; }
        if (col < 59) continue;

        AssistInputEntry e;
        e.character_hash = (uint32_t)strtoul(cols[0], nullptr, 10);
        for (int vi = 0; vi < 58; ++vi)
            e.values[vi] = (int32_t)strtol(cols[vi+1], nullptr, 10);
        result.push_back(e);
    }
    fclose(f);
    return result;
}

void FbsDataView::RenderAssistInputListEditor(ContentsBinData& bin)
{
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.65f, 0.82f, 1.00f, 1.00f));
    ImGui::Text("assist_input_list.bin");
    ImGui::PopStyleColor();
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.42f, 0.42f, 0.54f, 1.00f));
    ImGui::Text("(%d entries)", (int)bin.assistInputEntries.size());
    ImGui::PopStyleColor();

    {
        const float addBtnW=100.0f, ioGap=4.0f, exportBtnW=70.0f, importBtnW=70.0f;
        const float totalW=exportBtnW+ioGap+importBtnW+ioGap+addBtnW;
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - totalW + ImGui::GetCursorPosX());
        if (ImGui::Button("Export##asst", ImVec2(exportBtnW, 0))) { std::string p=OpenTsvSaveDialog(L"assist_input_list.tsv"); if(!p.empty()) ExportAssistInputListTsv(bin,p); }
        ImGui::SameLine(0, ioGap);
        if (!m_renderReadOnly && ImGui::Button("Import##asst", ImVec2(importBtnW, 0))) { std::string p=OpenTsvOpenDialog(); if(!p.empty()){auto imp=ImportAssistInputListTsv(p);if(!imp.empty())bin.assistInputEntries=std::move(imp);} }
        ImGui::SameLine(0, ioGap);
        if (!m_renderReadOnly && ImGui::Button("+ Add Entry", ImVec2(addBtnW, 0)))
            bin.assistInputEntries.push_back(AssistInputEntry{});
    }

    ImGui::Separator();

    constexpr ImGuiTableFlags tFlags =
        ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg |
        ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersInnerV |
        ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable |
        ImGuiTableFlags_Hideable | ImGuiTableFlags_SizingFixedFit;

    // 60 cols: # + character_hash + values[0..57]
    if (!ImGui::BeginTable("##AssistTable", 60, tFlags, ImGui::GetContentRegionAvail()))
        return;

    ImGui::TableSetupScrollFreeze(1, 1);
    ImGui::TableSetupColumn("#",                               ImGuiTableColumnFlags_WidthFixed, ColumnWidths::kRowCtrl);
    ImGui::TableSetupColumn(FieldNames::AssistInputEntry[0],   ImGuiTableColumnFlags_WidthFixed, ColumnWidths::kAssistInputEntry0);
    for (int vi = 1; vi < 59; ++vi)
        ImGui::TableSetupColumn(FieldNames::AssistInputEntry[vi], ImGuiTableColumnFlags_WidthFixed, ColumnWidths::kAssistInputValue);
    ImGui::TableHeadersRow();

    int deleteIdx = -1;
    ImGuiListClipper clipper;
    clipper.Begin((int)bin.assistInputEntries.size());
    while (clipper.Step())
    {
        for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i)
        {
            auto& e = bin.assistInputEntries[i];
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
            if (!m_renderReadOnly && ImGui::SmallButton("X")) deleteIdx = i;
            ImGui::PopStyleColor(3);

            ImGui::TableSetColumnIndex(1);
            ImGui::SetNextItemWidth(-FLT_MIN);
            ImGui::InputScalar("##ch", ImGuiDataType_U32, &e.character_hash);

            for (int vi = 0; vi < 58; ++vi)
            {
                ImGui::TableSetColumnIndex(vi + 2);
                ImGui::SetNextItemWidth(-FLT_MIN);
                char cellId[16];
                snprintf(cellId, sizeof(cellId), "##ai%d", vi);
                ImGui::InputScalar(cellId, ImGuiDataType_S32, &e.values[vi]);
            }

            ImGui::PopID();
        }
    }
    if (!m_renderReadOnly && deleteIdx >= 0)
        bin.assistInputEntries.erase(bin.assistInputEntries.begin() + deleteIdx);

    ImGui::EndTable();
}

// -----------------------------------------------------------------------------
//  Item ID popup editor  (AXXYYZZZ decomposed into XX/YY/ZZZ fields)
// -----------------------------------------------------------------------------


// -----------------------------------------------------------------------------
//  Mod info edit popup
// -----------------------------------------------------------------------------

void FbsDataView::RenderInfoEditPopup()
{
    if (m_infoEditPending) {
        ImGui::OpenPopup("##info_edit");
        m_infoEditPending = false;
    }
    if (!ImGui::BeginPopup("##info_edit")) return;

    ModInfo& info    = m_data.info;
    const bool isNew = m_data.isNew;

    ImGui::Text("Mod Information");
    ImGui::Separator();

    // Author
    ImGui::Text("Author");
    ImGui::SameLine(100.0f);
    ImGui::SetNextItemWidth(260.0f);
    if (!isNew) ImGui::BeginDisabled();
    ImGui::InputText("##info_author", info.author, sizeof(info.author));
    if (!isNew) ImGui::EndDisabled();

    // Description
    ImGui::Text("Description");
    ImGui::SameLine(100.0f);
    ImGui::SetNextItemWidth(260.0f);
    if (!isNew) ImGui::BeginDisabled();
    ImGui::InputText("##info_desc", info.description, sizeof(info.description));
    if (!isNew) ImGui::EndDisabled();

    // Version (always editable)
    ImGui::Text("Version");
    ImGui::SameLine(100.0f);
    ImGui::SetNextItemWidth(260.0f);
    ImGui::InputText("##info_ver", info.version, sizeof(info.version));

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Text("Custom GTB Tables");

    int removeGtb = -1;
    for (int i = 0; i < (int)m_data.customGtbNames.size(); ++i)
    {
        ImGui::PushID(i);
        char buf[256] = {};
        strncpy_s(buf, m_data.customGtbNames[i].c_str(), _TRUNCATE);

        const bool valid = IsValidGtbManifestName(m_data.customGtbNames[i]);
        if (!valid)
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.90f, 0.35f, 0.35f, 1.00f));

        ImGui::SetNextItemWidth(260.0f);
        if (ImGui::InputText("##gtb_name", buf, sizeof(buf)))
            m_data.customGtbNames[i] = buf;

        if (!valid)
            ImGui::PopStyleColor();

        ImGui::SameLine(0, 6.0f);
        if (ImGui::SmallButton("X"))
            removeGtb = i;

        ImGui::PopID();
    }

    if (removeGtb >= 0)
        m_data.customGtbNames.erase(m_data.customGtbNames.begin() + removeGtb);

    if (ImGui::Button("+ Add GTB Table"))
        m_data.customGtbNames.push_back("GTB_");

    ImGui::Spacing();
    if (ImGui::Button("Close"))
        ImGui::CloseCurrentPopup();

    ImGui::EndPopup();
}

// -----------------------------------------------------------------------------
//  customize_panel_list editor
// -----------------------------------------------------------------------------

static void ExportCustomizePanelListTsv(const ContentsBinData& bin, const std::string& path)
{
    FILE* f = nullptr;
    if (fopen_s(&f, path.c_str(), "wb") != 0 || !f) return;

    fprintf(f, "panel_hash\tpanel_id\tprice\tcategory\tsort_id\ttext_key\ttexture_1\ttexture_2\ttexture_3\tflag_9\thash_10\n");
    for (const auto& e : bin.customizePanelEntries)
        fprintf(f, "%u\t%u\t%u\t%u\t%u\t%s\t%s\t%s\t%s\t%s\t%u\n",
            e.panel_hash, e.panel_id, e.price, e.category, e.sort_id,
            e.text_key, e.texture_1, e.texture_2, e.texture_3,
            e.flag_9 ? "TRUE" : "FALSE", e.hash_10);
    fclose(f);
}

static std::vector<CustomizePanelEntry> ImportCustomizePanelListTsv(const std::string& path)
{
    std::vector<CustomizePanelEntry> result;
    FILE* f = nullptr;
    if (fopen_s(&f, path.c_str(), "rb") != 0 || !f) return result;

    bool headerSkipped = false;
    char line[1536];
    while (fgets(line, sizeof(line), f))
    {
        int len = (int)strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r')) line[--len] = '\0';
        if (len == 0 || line[0] == '#') continue;
        if (!headerSkipped) { headerSkipped = true; continue; }

        char* cols[11] = {}; int col = 0; char* p = line;
        cols[col++] = p;
        for (; *p && col < 11; ++p) if (*p == '\t') { *p = '\0'; cols[col++] = p+1; }
        if (col < 11) continue;

        CustomizePanelEntry e;
        e.panel_hash = (uint32_t)strtoul(cols[0], nullptr, 10);
        e.panel_id   = (uint32_t)strtoul(cols[1], nullptr, 10);
        e.price      = (uint32_t)strtoul(cols[2], nullptr, 10);
        e.category   = (uint32_t)strtoul(cols[3], nullptr, 10);
        e.sort_id    = (uint32_t)strtoul(cols[4], nullptr, 10);
        strncpy_s(e.text_key,  sizeof(e.text_key),  cols[5], _TRUNCATE);
        strncpy_s(e.texture_1, sizeof(e.texture_1), cols[6], _TRUNCATE);
        strncpy_s(e.texture_2, sizeof(e.texture_2), cols[7], _TRUNCATE);
        strncpy_s(e.texture_3, sizeof(e.texture_3), cols[8], _TRUNCATE);
        e.flag_9  = ParseBool(cols[9]);
        e.hash_10 = (uint32_t)strtoul(cols[10], nullptr, 10);
        result.push_back(e);
    }
    fclose(f);
    return result;
}

void FbsDataView::RenderCustomizePanelListEditor(ContentsBinData& bin)
{
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.65f, 0.82f, 1.00f, 1.00f));
    ImGui::Text("customize_panel_list.bin");
    ImGui::PopStyleColor();
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.42f, 0.42f, 0.54f, 1.00f));
    ImGui::Text("(%d entries)", (int)bin.customizePanelEntries.size());
    ImGui::PopStyleColor();

    {
        const float addBtnW=100.0f, ioGap=4.0f, exportBtnW=70.0f, importBtnW=70.0f;
        const float totalW=exportBtnW+ioGap+importBtnW+ioGap+addBtnW;
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - totalW + ImGui::GetCursorPosX());
        if (ImGui::Button("Export##cpanel", ImVec2(exportBtnW, 0))) { std::string p=OpenTsvSaveDialog(L"customize_panel_list.tsv"); if(!p.empty()) ExportCustomizePanelListTsv(bin,p); }
        ImGui::SameLine(0, ioGap);
        if (!m_renderReadOnly && ImGui::Button("Import##cpanel", ImVec2(importBtnW, 0))) { std::string p=OpenTsvOpenDialog(); if(!p.empty()){auto imp=ImportCustomizePanelListTsv(p);if(!imp.empty())bin.customizePanelEntries=std::move(imp);} }
        ImGui::SameLine(0, ioGap);
        if (!m_renderReadOnly && ImGui::Button("+ Add Entry", ImVec2(addBtnW, 0)))
            bin.customizePanelEntries.push_back(bin.customizePanelEntries.empty()
                ? CustomizePanelEntry{}
                : bin.customizePanelEntries.back());
    }

    ImGui::Separator();

    constexpr ImGuiTableFlags tFlags =
        ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg |
        ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersInnerV |
        ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable |
        ImGuiTableFlags_Hideable | ImGuiTableFlags_SizingFixedFit;

    // cols: # + 11 fields
    if (!ImGui::BeginTable("##CPLTable", 12, tFlags, ImGui::GetContentRegionAvail()))
        return;

    ImGui::TableSetupScrollFreeze(1, 1);
    ImGui::TableSetupColumn("#",                                      ImGuiTableColumnFlags_WidthFixed, ColumnWidths::kRowCtrl);
    for (int ci = 0; ci < FieldNames::CustomizePanelEntryCount; ++ci)
        ImGui::TableSetupColumn(FieldNames::CustomizePanelEntry[ci],  ImGuiTableColumnFlags_WidthFixed,
            (ci >= 5 && ci <= 8) ? ColumnWidths::kCustomizePanelString : ColumnWidths::kCustomizePanelDefault);
    ImGui::TableHeadersRow();

    int deleteIdx = -1;

    for (int i = 0; i < (int)bin.customizePanelEntries.size(); ++i)
    {
        auto& e = bin.customizePanelEntries[i];
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
        if (!m_renderReadOnly && ImGui::SmallButton("X")) deleteIdx = i;
        ImGui::PopStyleColor(3);

        auto U32Cell = [](const char* id, uint32_t& v) {
            ImGui::SetNextItemWidth(-FLT_MIN);
            ImGui::InputScalar(id, ImGuiDataType_U32, &v);
        };
        auto StrCell = [](const char* id, char* buf, size_t sz) {
            ImGui::SetNextItemWidth(-FLT_MIN);
            ImGui::InputText(id, buf, sz);
        };

        ImGui::TableSetColumnIndex(1);  U32Cell("##ph",   e.panel_hash);
        ImGui::TableSetColumnIndex(2);  U32Cell("##pid",  e.panel_id);
        ImGui::TableSetColumnIndex(3);  U32Cell("##prc",  e.price);
        ImGui::TableSetColumnIndex(4);  U32Cell("##cat",  e.category);
        ImGui::TableSetColumnIndex(5);  U32Cell("##sid",  e.sort_id);
        ImGui::TableSetColumnIndex(6);  StrCell("##tkey", e.text_key,  sizeof(e.text_key));
        ImGui::TableSetColumnIndex(7);  StrCell("##tx1",  e.texture_1, sizeof(e.texture_1));
        ImGui::TableSetColumnIndex(8);  StrCell("##tx2",  e.texture_2, sizeof(e.texture_2));
        ImGui::TableSetColumnIndex(9);  StrCell("##tx3",  e.texture_3, sizeof(e.texture_3));
        ImGui::TableSetColumnIndex(10); ImGui::Checkbox("##f9", &e.flag_9);
        ImGui::TableSetColumnIndex(11); U32Cell("##h10",  e.hash_10);

        ImGui::PopID();
    }

    if (!m_renderReadOnly && deleteIdx >= 0)
        bin.customizePanelEntries.erase(bin.customizePanelEntries.begin() + deleteIdx);

    ImGui::EndTable();
}

// -----------------------------------------------------------------------------
//  customize_item_exception editor
// -----------------------------------------------------------------------------

struct ExcTypeInfo { int32_t value; const char* name; int32_t reqCharId; int32_t reqPosId; };
static const ExcTypeInfo k_ExcTypes[] = {
    {  0, "NoException",              -1, -1 },
    {  1, "Kuni Mask",                41,  4 },
    {  2, "french_bread",             -1,  8 },
    {  3, "honey_spoon",              -1,  8 },
    {  4, "nailbat",                  -1,  8 },
    {  5, "shotgun",                  -1,  8 },
    {  6, "naginata",                 -1,  8 },
    {  7, "prowrestling_pipechair",   -1,  8 },
    {  8, "ukulele",                  -1,  8 },
    {  9, "fireknife",                -1,  8 },
    { 10, "morning_star",             -1,  8 },
    { 11, "endbiker_gun",             -1,  8 },
    { 12, "ordinary_scythe",          -1,  8 },
    { 13, "shakujo",                  -1,  8 },
    { 14, "traffic_wand",             -1,  8 },
    { 15, "1000t_hammer",             -1,  8 },
    { 16, "golf_club",                -1,  8 },
};

void FbsDataView::RenderCustomizeItemExceptionEditor(ContentsBinData& bin)
{
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.65f, 0.82f, 1.00f, 1.00f));
    ImGui::Text("customize_item_exception");
    ImGui::PopStyleColor();
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.42f, 0.42f, 0.54f, 1.00f));
    ImGui::Text("(%d entries)", (int)bin.exceptionEntries.size());
    ImGui::PopStyleColor();

    const float addBtnW = 100.0f;
    ImGui::SameLine(ImGui::GetContentRegionAvail().x - addBtnW + ImGui::GetCursorPosX());
    if (!m_renderReadOnly && ImGui::Button("+ Add Entry", ImVec2(addBtnW, 0)))
        bin.exceptionEntries.push_back(CustomizeItemExceptionEntry{});

    ImGui::Separator();

    // Build item_id list (asset name label) from all common + unique bins in this mod
    struct IdLabel { uint32_t id; std::string label; };
    std::vector<IdLabel> available;
    for (const auto& other : m_data.contents)
    {
        for (const auto& e : other.commonEntries)
        {
            char buf[320];
            if (e.item_code[0])
                snprintf(buf, sizeof(buf), "%s (%u)", e.item_code, e.item_id);
            else
                snprintf(buf, sizeof(buf), "%u", e.item_id);
            available.push_back({ e.item_id, buf });
        }
        for (const auto& e : other.customizeItemUniqueEntries)
        {
            char buf[320];
            if (e.asset_name[0])
                snprintf(buf, sizeof(buf), "%s (%u)", e.asset_name, e.char_item_id);
            else
                snprintf(buf, sizeof(buf), "%u", e.char_item_id);
            available.push_back({ e.char_item_id, buf });
        }
    }

    // Collect item_ids already referenced by other exception entries (for dup filtering)
    std::vector<uint32_t> usedIds;
    usedIds.reserve(bin.exceptionEntries.size());
    for (const auto& ex : bin.exceptionEntries)
        if (ex.item_id != 0)
            usedIds.push_back(ex.item_id);

    constexpr ImGuiTableFlags tFlags =
        ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersInnerH |
        ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY |
        ImGuiTableFlags_SizingStretchProp;

    if (!ImGui::BeginTable("##exc_tbl", 3, tFlags)) return;

    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableSetupColumn("Item ID",        ImGuiTableColumnFlags_WidthStretch, 3.0f);
    ImGui::TableSetupColumn("Exception Type", ImGuiTableColumnFlags_WidthFixed,  120.0f);
    ImGui::TableSetupColumn("##del",          ImGuiTableColumnFlags_WidthFixed,   28.0f);
    ImGui::TableHeadersRow();

    int removeIdx = -1;
    for (int i = 0; i < (int)bin.exceptionEntries.size(); ++i)
    {
        auto& e = bin.exceptionEntries[i];
        ImGui::TableNextRow();
        ImGui::PushID(i);

        // -- Item ID dropdown -------------------------------------------------
        ImGui::TableSetColumnIndex(0);
        ImGui::SetNextItemWidth(-1.0f);

        // Find current label
        const char* previewLabel = "-- select --";
        for (const auto& il : available)
            if (il.id == e.item_id) { previewLabel = il.label.c_str(); break; }

        if (ImGui::BeginCombo("##iid", previewLabel))
        {
            for (const auto& il : available)
            {
                // Skip ids already used by other rows
                bool usedByOther = false;
                if (il.id != e.item_id)
                {
                    for (uint32_t uid : usedIds)
                        if (uid == il.id) { usedByOther = true; break; }
                }
                if (usedByOther) continue;

                const bool sel = (il.id == e.item_id);
                if (ImGui::Selectable(il.label.c_str(), sel))
                {
                    e.item_id = il.id;
                    // Reset exception_type if it is no longer valid for the new item_id
                    uint32_t newBB = (e.item_id / 100000u) % 100u;
                    uint32_t newCC = (e.item_id /   1000u) % 100u;
                    bool stillOk = false;
                    for (const auto& t : k_ExcTypes)
                        if (t.value == e.exception_type) {
                            if ((t.reqCharId == -1 || (int32_t)newBB == t.reqCharId) &&
                                (t.reqPosId  == -1 || (int32_t)newCC == t.reqPosId))
                                stillOk = true;
                            break;
                        }
                    if (!stillOk) e.exception_type = 0;
                }
                if (sel) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        // -- Exception type dropdown ------------------------------------------
        {
            const uint32_t BB = (e.item_id / 100000u) % 100u;
            const uint32_t CC = (e.item_id /   1000u) % 100u;

            auto isAvailable = [&](const ExcTypeInfo& t) -> bool {
                if (e.item_id == 0) return t.value == 0;
                if (t.reqCharId != -1 && (int32_t)BB != t.reqCharId) return false;
                if (t.reqPosId  != -1 && (int32_t)CC != t.reqPosId)  return false;
                return true;
            };

            const char* typeName = "Unknown";
            for (const auto& t : k_ExcTypes)
                if (t.value == e.exception_type) { typeName = t.name; break; }

            ImGui::TableSetColumnIndex(1);
            ImGui::SetNextItemWidth(-1.0f);
            if (e.item_id == 0) ImGui::BeginDisabled();
            if (ImGui::BeginCombo("##etype", typeName))
            {
                for (const auto& t : k_ExcTypes)
                {
                    if (!isAvailable(t)) continue;
                    const bool sel = (t.value == e.exception_type);
                    if (ImGui::Selectable(t.name, sel))
                        e.exception_type = t.value;
                    if (sel) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
            if (e.item_id == 0) ImGui::EndDisabled();
        }

        // -- Delete button ----------------------------------------------------
        ImGui::TableSetColumnIndex(2);
        if (!m_renderReadOnly && ImGui::SmallButton("x"))
            removeIdx = i;

        ImGui::PopID();
    }

    if (!m_renderReadOnly && removeIdx >= 0)
        bin.exceptionEntries.erase(bin.exceptionEntries.begin() + removeIdx);

    ImGui::EndTable();
}

// -----------------------------------------------------------------------------
//  Merged overview table (used by TkmodManagerView)
// -----------------------------------------------------------------------------

// Concatenates one bin-member vector across every overview source, then runs the
// single-bin TSV exporter once on the merged result. Lets the merged overview's
// Export button write every tkmod's rows, not just the first source's.
template <class Entry>
static void ExportMergedOverviewTsv(
    const std::vector<FbsDataView::BinViewSource>& sources,
    const std::vector<Entry> ContentsBinData::* member,
    void (*exportFn)(const std::vector<Entry>&, const std::string&),
    const std::string& path)
{
    std::vector<Entry> merged;
    for (const auto& s : sources)
    {
        if (!s.bin) continue;
        const std::vector<Entry>& v = s.bin->*member;
        merged.insert(merged.end(), v.begin(), v.end());
    }
    exportFn(merged, path);
}

void FbsDataView::RenderBinMergedOverview(
    const std::vector<BinViewSource>& sources,
    const std::function<void(const std::string&)>& openCb)
{
    if (sources.empty()) return;

    constexpr ImGuiTableFlags kOvTF =
        ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg |
        ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersInnerV |
        ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable |
        ImGuiTableFlags_Hideable | ImGuiTableFlags_SizingFixedFit;
    constexpr float kTkmodW = 190.0f;

    auto fmtHash = [](uint32_t h, const std::vector<std::pair<uint32_t,std::string>>& items) -> std::string {
        for (auto& kv : items) if (kv.first == h) return kv.second;
        char buf[16]; snprintf(buf, sizeof(buf), "%u", h); return buf;
    };
    auto& charItems = GetCharHashItems();
    auto& typeItems = GetTypeHashItems();

    // Helper: render the tkmod cell (column 0) for source index si.
    // Call inside PushID(rowIndex)/PopID() so the button ID is unique per row.
    auto RenderTkmodCell = [&](int si) {
        ImGui::TableSetColumnIndex(0);
        if (ImGui::SmallButton("Open##ov"))
            openCb(sources[si].path);
        ImGui::SameLine(0, 4.0f);
        ImGui::TextUnformatted(sources[si].filename);
    };

    // Helper: section header with optional Export button.
    // exportFilename: suggested filename for the save dialog; exportFn: called with the
    // chosen save path and is expected to merge/export rows across all sources itself.
    auto RenderSectionHeader = [&](const char* binLabel, int totalEntries,
        const wchar_t* exportFilename = nullptr,
        std::function<void(const std::string&)> exportFn = nullptr)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.65f, 0.82f, 1.00f, 1.00f));
        ImGui::TextUnformatted(binLabel);
        ImGui::PopStyleColor();
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.42f, 0.42f, 0.54f, 1.00f));
        ImGui::Text("(%d entries across %d tkmods)", totalEntries, (int)sources.size());
        ImGui::PopStyleColor();
        if (exportFilename && exportFn)
        {
            const float exportBtnW = 70.0f;
            ImGui::SameLine(ImGui::GetContentRegionAvail().x - exportBtnW + ImGui::GetCursorPosX());
            ImGui::PushID(binLabel);
            if (ImGui::Button("Export##ov", ImVec2(exportBtnW, 0)))
            {
                std::string p = OpenTsvSaveDialog(exportFilename);
                if (!p.empty())
                    exportFn(p);
            }
            ImGui::PopID();
        }
        ImGui::Separator();
    };

    BinType type = sources[0].bin->type;

    switch (type)
    {
    // -------------------------------------------------------------------------
    case BinType::CustomizeItemCommonList:
    {
        struct Item { int si; const CustomizeItemCommonEntry* e; };
        std::vector<Item> items;
        for (int si = 0; si < (int)sources.size(); ++si)
            for (auto& e : sources[si].bin->commonEntries)
                items.push_back({si, &e});
        std::sort(items.begin(), items.end(), [](const Item& a, const Item& b){ return a.e->item_id < b.e->item_id; });

        RenderSectionHeader("customize_item_common_list.bin", (int)items.size(), L"customize_item_common_list.tsv",
            [&](const std::string& p){ ExportMergedOverviewTsv(sources, &ContentsBinData::commonEntries, &ExportCommonListTsv, p); });

        if (ImGui::BeginTable("##ov_common", 1 + 26, kOvTF, ImVec2(0.0f, 0.0f)))
        {
            ImGui::TableSetupScrollFreeze(1, 1);
            ImGui::TableSetupColumn("tkmod", ImGuiTableColumnFlags_WidthFixed, kTkmodW);
            for (int c = 0; c < 26; ++c)
                ImGui::TableSetupColumn(FieldNames::CommonItem[c], ImGuiTableColumnFlags_WidthFixed, ColumnWidths::kCommon[c]);
            ImGui::TableHeadersRow();

            ImGuiListClipper clipper;
            clipper.Begin((int)items.size());
            while (clipper.Step())
            for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i)
            {
                const int si = items[i].si; const auto* e = items[i].e;
                ImGui::TableNextRow();
                ImGui::PushID(i);
                RenderTkmodCell(si);
                ImGui::TableSetColumnIndex(1);  ImGui::Text("%u", e->item_id);
                ImGui::TableSetColumnIndex(2);  ImGui::Text("%d", e->item_no);
                ImGui::TableSetColumnIndex(3);  ImGui::TextUnformatted(e->item_code);
                ImGui::TableSetColumnIndex(4);  ImGui::TextUnformatted(fmtHash(e->hash_0, charItems).c_str());
                ImGui::TableSetColumnIndex(5);  ImGui::TextUnformatted(fmtHash(e->hash_1, typeItems).c_str());
                ImGui::TableSetColumnIndex(6);  ImGui::TextUnformatted(e->text_key);
                ImGui::TableSetColumnIndex(7);  ImGui::TextUnformatted(e->package_id);
                ImGui::TableSetColumnIndex(8);  ImGui::TextUnformatted(e->package_sub_id);
                ImGui::TableSetColumnIndex(9);  ImGui::Text("%u", e->unk_8);
                ImGui::TableSetColumnIndex(10); ImGui::Text("%d", e->shop_sort_id);
                ImGui::TableSetColumnIndex(11); ImGui::TextUnformatted(e->is_enabled ? "true" : "false");
                ImGui::TableSetColumnIndex(12); ImGui::Text("%u", e->unk_11);
                ImGui::TableSetColumnIndex(13); ImGui::Text("%d", e->price);
                ImGui::TableSetColumnIndex(14); ImGui::TextUnformatted(e->unk_13 ? "true" : "false");
                ImGui::TableSetColumnIndex(15); ImGui::Text("%d", e->category_no);
                ImGui::TableSetColumnIndex(16); ImGui::Text("%u", e->hash_2);
                ImGui::TableSetColumnIndex(17); ImGui::TextUnformatted(e->unk_16 ? "true" : "false");
                ImGui::TableSetColumnIndex(18); ImGui::Text("%u", e->unk_17);
                ImGui::TableSetColumnIndex(19); ImGui::Text("%u", e->hash_3);
                ImGui::TableSetColumnIndex(20); ImGui::Text("%u", e->unk_19);
                ImGui::TableSetColumnIndex(21); ImGui::Text("%u", e->unk_20);
                ImGui::TableSetColumnIndex(22); ImGui::Text("%u", e->unk_21);
                ImGui::TableSetColumnIndex(23); ImGui::Text("%u", e->unk_22);
                ImGui::TableSetColumnIndex(24); ImGui::Text("%u", e->hash_4);
                ImGui::TableSetColumnIndex(25); ImGui::Text("%d", e->rarity);
                ImGui::TableSetColumnIndex(26); ImGui::Text("%d", e->sort_group);
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
        break;
    }
    // -------------------------------------------------------------------------
    case BinType::CustomizeItemUniqueList:
    {
        struct Item { int si; const CustomizeItemUniqueEntry* e; };
        std::vector<Item> items;
        for (int si = 0; si < (int)sources.size(); ++si)
            for (auto& e : sources[si].bin->customizeItemUniqueEntries)
                items.push_back({si, &e});
        std::sort(items.begin(), items.end(), [](const Item& a, const Item& b){ return a.e->char_item_id < b.e->char_item_id; });

        RenderSectionHeader("customize_item_unique_list.bin", (int)items.size(), L"customize_item_unique_list.tsv",
            [&](const std::string& p){ ExportMergedOverviewTsv(sources, &ContentsBinData::customizeItemUniqueEntries, &ExportUniqueListTsv, p); });

        if (ImGui::BeginTable("##ov_unique", 1 + 22, kOvTF, ImVec2(0.0f, 0.0f)))
        {
            ImGui::TableSetupScrollFreeze(1, 1);
            ImGui::TableSetupColumn("tkmod", ImGuiTableColumnFlags_WidthFixed, kTkmodW);
            for (int c = 0; c < 22; ++c)
                ImGui::TableSetupColumn(FieldNames::CustomizeItemUnique[c], ImGuiTableColumnFlags_WidthFixed, ColumnWidths::kUnique[c]);
            ImGui::TableHeadersRow();

            ImGuiListClipper clipper;
            clipper.Begin((int)items.size());
            while (clipper.Step())
            for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i)
            {
                const int si = items[i].si; const auto* e = items[i].e;
                ImGui::TableNextRow();
                ImGui::PushID(i);
                RenderTkmodCell(si);
                ImGui::TableSetColumnIndex(1);  ImGui::Text("%u", e->char_item_id);
                ImGui::TableSetColumnIndex(2);  ImGui::TextUnformatted(e->asset_name);
                ImGui::TableSetColumnIndex(3);  ImGui::TextUnformatted(fmtHash(e->character_hash, charItems).c_str());
                ImGui::TableSetColumnIndex(4);  ImGui::TextUnformatted(fmtHash(e->hash_1, typeItems).c_str());
                ImGui::TableSetColumnIndex(5);  ImGui::TextUnformatted(e->text_key);
                ImGui::TableSetColumnIndex(6);  ImGui::TextUnformatted(e->extra_text_key_1);
                ImGui::TableSetColumnIndex(7);  ImGui::TextUnformatted(e->extra_text_key_2);
                ImGui::TableSetColumnIndex(8);  ImGui::Text("%u", e->flag_7);
                ImGui::TableSetColumnIndex(9);  ImGui::Text("%u", e->unk_8);
                ImGui::TableSetColumnIndex(10); ImGui::TextUnformatted(e->flag_9 ? "true" : "false");
                ImGui::TableSetColumnIndex(11); ImGui::Text("%u", e->unk_10);
                ImGui::TableSetColumnIndex(12); ImGui::Text("%u", e->price);
                ImGui::TableSetColumnIndex(13); ImGui::Text("%u", e->unk_12);
                ImGui::TableSetColumnIndex(14); ImGui::Text("%u", e->unk_13);
                ImGui::TableSetColumnIndex(15); ImGui::Text("%u", e->hash_2);
                ImGui::TableSetColumnIndex(16); ImGui::TextUnformatted(e->flag_15 ? "true" : "false");
                ImGui::TableSetColumnIndex(17); ImGui::Text("%u", e->unk_16);
                ImGui::TableSetColumnIndex(18); ImGui::Text("%u", e->hash_3);
                ImGui::TableSetColumnIndex(19); ImGui::Text("%u", e->unk_18);
                ImGui::TableSetColumnIndex(20); ImGui::Text("%u", e->unk_19);
                ImGui::TableSetColumnIndex(21); ImGui::Text("%u", e->unk_20);
                ImGui::TableSetColumnIndex(22); ImGui::Text("%u", e->unk_21);
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
        break;
    }
    // -------------------------------------------------------------------------
    case BinType::CharacterList:
    {
        struct Item { int si; const CharacterEntry* e; };
        std::vector<Item> items;
        for (int si = 0; si < (int)sources.size(); ++si)
            for (auto& e : sources[si].bin->characterEntries)
                items.push_back({si, &e});

        RenderSectionHeader("character_list.bin", (int)items.size(), L"character_list.tsv",
            [&](const std::string& p){ ExportMergedOverviewTsv(sources, &ContentsBinData::characterEntries, &ExportCharacterListTsv, p); });

        if (ImGui::BeginTable("##ov_char", 1 + 15, kOvTF, ImVec2(0.0f, 0.0f)))
        {
            ImGui::TableSetupScrollFreeze(1, 1);
            ImGui::TableSetupColumn("tkmod", ImGuiTableColumnFlags_WidthFixed, kTkmodW);
            for (int c = 0; c < 15; ++c)
                ImGui::TableSetupColumn(FieldNames::Character[c], ImGuiTableColumnFlags_WidthFixed, ColumnWidths::kCharacter[c]);
            ImGui::TableHeadersRow();

            for (int i = 0; i < (int)items.size(); ++i)
            {
                const int si = items[i].si; const auto* e = items[i].e;
                ImGui::TableNextRow();
                ImGui::PushID(i);
                RenderTkmodCell(si);
                ImGui::TableSetColumnIndex(1);  ImGui::TextUnformatted(e->character_code);
                ImGui::TableSetColumnIndex(2);  ImGui::Text("%u", e->name_hash);
                ImGui::TableSetColumnIndex(3);  ImGui::TextUnformatted(e->is_enabled ? "true" : "false");
                ImGui::TableSetColumnIndex(4);  ImGui::TextUnformatted(e->is_selectable ? "true" : "false");
                ImGui::TableSetColumnIndex(5);  ImGui::TextUnformatted(e->group);
                ImGui::TableSetColumnIndex(6);  ImGui::Text("%.4g", e->camera_offset);
                ImGui::TableSetColumnIndex(7);  ImGui::TextUnformatted(e->is_playable ? "true" : "false");
                ImGui::TableSetColumnIndex(8);  ImGui::Text("%u", e->sort_order);
                ImGui::TableSetColumnIndex(9);  ImGui::TextUnformatted(e->full_name_key);
                ImGui::TableSetColumnIndex(10); ImGui::TextUnformatted(e->short_name_jp_key);
                ImGui::TableSetColumnIndex(11); ImGui::TextUnformatted(e->short_name_key);
                ImGui::TableSetColumnIndex(12); ImGui::TextUnformatted(e->origin_key);
                ImGui::TableSetColumnIndex(13); ImGui::TextUnformatted(e->fighting_style_key);
                ImGui::TableSetColumnIndex(14); ImGui::TextUnformatted(e->height_key);
                ImGui::TableSetColumnIndex(15); ImGui::TextUnformatted(e->weight_key);
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
        break;
    }
    // -------------------------------------------------------------------------
    case BinType::AreaList:
    {
        struct Item { int si; const AreaEntry* e; };
        std::vector<Item> items;
        for (int si = 0; si < (int)sources.size(); ++si)
            for (auto& e : sources[si].bin->areaEntries)
                items.push_back({si, &e});

        RenderSectionHeader("area_list.bin", (int)items.size(), L"area_list.tsv",
            [&](const std::string& p){ ExportMergedOverviewTsv(sources, &ContentsBinData::areaEntries, &ExportAreaListTsv, p); });

        if (ImGui::BeginTable("##ov_area", 1 + 2, kOvTF, ImVec2(0.0f, 0.0f)))
        {
            ImGui::TableSetupScrollFreeze(1, 1);
            ImGui::TableSetupColumn("tkmod", ImGuiTableColumnFlags_WidthFixed, kTkmodW);
            for (int c = 0; c < 2; ++c)
                ImGui::TableSetupColumn(FieldNames::AreaEntry[c], ImGuiTableColumnFlags_WidthFixed, ColumnWidths::kArea[c]);
            ImGui::TableHeadersRow();

            for (int i = 0; i < (int)items.size(); ++i)
            {
                const int si = items[i].si; const auto* e = items[i].e;
                ImGui::TableNextRow();
                ImGui::PushID(i);
                RenderTkmodCell(si);
                ImGui::TableSetColumnIndex(1); ImGui::Text("%u", e->area_hash);
                ImGui::TableSetColumnIndex(2); ImGui::TextUnformatted(e->area_code);
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
        break;
    }
    // -------------------------------------------------------------------------
    case BinType::BattleSubtitleInfoList:
    {
        struct Item { int si; const BattleSubtitleInfoEntry* e; };
        std::vector<Item> items;
        for (int si = 0; si < (int)sources.size(); ++si)
            for (auto& e : sources[si].bin->battleSubtitleEntries)
                items.push_back({si, &e});

        RenderSectionHeader("battle_subtitle_info_list.bin", (int)items.size(), L"battle_subtitle_info.tsv",
            [&](const std::string& p){ ExportMergedOverviewTsv(sources, &ContentsBinData::battleSubtitleEntries, &ExportBattleSubtitleTsv, p); });

        if (ImGui::BeginTable("##ov_bsub", 1 + 2, kOvTF, ImVec2(0.0f, 0.0f)))
        {
            ImGui::TableSetupScrollFreeze(1, 1);
            ImGui::TableSetupColumn("tkmod", ImGuiTableColumnFlags_WidthFixed, kTkmodW);
            for (int c = 0; c < 2; ++c)
                ImGui::TableSetupColumn(FieldNames::BattleSubtitleInfo[c], ImGuiTableColumnFlags_WidthFixed, ColumnWidths::kBattleSubtitle[c]);
            ImGui::TableHeadersRow();

            for (int i = 0; i < (int)items.size(); ++i)
            {
                const int si = items[i].si; const auto* e = items[i].e;
                ImGui::TableNextRow();
                ImGui::PushID(i);
                RenderTkmodCell(si);
                ImGui::TableSetColumnIndex(1); ImGui::Text("%u", e->subtitle_hash);
                ImGui::TableSetColumnIndex(2); ImGui::Text("%u", e->subtitle_type);
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
        break;
    }
    // -------------------------------------------------------------------------
    case BinType::FateDramaPlayerStartList:
    {
        struct Item { int si; const FateDramaPlayerStartEntry* e; };
        std::vector<Item> items;
        for (int si = 0; si < (int)sources.size(); ++si)
            for (auto& e : sources[si].bin->fateDramaPlayerStartEntries)
                items.push_back({si, &e});

        RenderSectionHeader("fate_drama_player_start_list.bin", (int)items.size(), L"fate_drama_player_start_list.tsv",
            [&](const std::string& p){ ExportMergedOverviewTsv(sources, &ContentsBinData::fateDramaPlayerStartEntries, &ExportFateDramaPlayerStartTsv, p); });

        if (ImGui::BeginTable("##ov_fdps", 1 + 5, kOvTF, ImVec2(0.0f, 0.0f)))
        {
            ImGui::TableSetupScrollFreeze(1, 1);
            ImGui::TableSetupColumn("tkmod", ImGuiTableColumnFlags_WidthFixed, kTkmodW);
            for (int c = 0; c < 5; ++c)
                ImGui::TableSetupColumn(FieldNames::FateDramaPlayerStart[c], ImGuiTableColumnFlags_WidthFixed, ColumnWidths::kFateDramaPlayerStart[c]);
            ImGui::TableHeadersRow();

            for (int i = 0; i < (int)items.size(); ++i)
            {
                const int si = items[i].si; const auto* e = items[i].e;
                ImGui::TableNextRow();
                ImGui::PushID(i);
                RenderTkmodCell(si);
                ImGui::TableSetColumnIndex(1); ImGui::Text("%u", e->character1_hash);
                ImGui::TableSetColumnIndex(2); ImGui::Text("%u", e->character2_hash);
                ImGui::TableSetColumnIndex(3); ImGui::Text("%u", e->value_0);
                ImGui::TableSetColumnIndex(4); ImGui::Text("%u", e->hash_2);
                ImGui::TableSetColumnIndex(5); ImGui::TextUnformatted(e->value_4 ? "true" : "false");
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
        break;
    }
    // -------------------------------------------------------------------------
    case BinType::JukeboxList:
    {
        struct Item { int si; const JukeboxEntry* e; };
        std::vector<Item> items;
        for (int si = 0; si < (int)sources.size(); ++si)
            for (auto& e : sources[si].bin->jukeboxEntries)
                items.push_back({si, &e});

        RenderSectionHeader("jukebox_list.bin", (int)items.size(), L"jukebox_list.tsv",
            [&](const std::string& p){ ExportMergedOverviewTsv(sources, &ContentsBinData::jukeboxEntries, &ExportJukeboxListTsv, p); });

        if (ImGui::BeginTable("##ov_juke", 1 + 9, kOvTF, ImVec2(0.0f, 0.0f)))
        {
            ImGui::TableSetupScrollFreeze(1, 1);
            ImGui::TableSetupColumn("tkmod", ImGuiTableColumnFlags_WidthFixed, kTkmodW);
            for (int c = 0; c < 9; ++c)
                ImGui::TableSetupColumn(FieldNames::JukeboxEntry[c], ImGuiTableColumnFlags_WidthFixed, ColumnWidths::kJukebox[c]);
            ImGui::TableHeadersRow();

            for (int i = 0; i < (int)items.size(); ++i)
            {
                const int si = items[i].si; const auto* e = items[i].e;
                ImGui::TableNextRow();
                ImGui::PushID(i);
                RenderTkmodCell(si);
                ImGui::TableSetColumnIndex(1); ImGui::Text("%u", e->bgm_hash);
                ImGui::TableSetColumnIndex(2); ImGui::Text("%u", e->series_hash);
                ImGui::TableSetColumnIndex(3); ImGui::Text("%u", e->unk_2);
                ImGui::TableSetColumnIndex(4); ImGui::TextUnformatted(e->cue_name);
                ImGui::TableSetColumnIndex(5); ImGui::TextUnformatted(e->arrangement);
                ImGui::TableSetColumnIndex(6); ImGui::TextUnformatted(e->alt_cue_name_1);
                ImGui::TableSetColumnIndex(7); ImGui::TextUnformatted(e->alt_cue_name_2);
                ImGui::TableSetColumnIndex(8); ImGui::TextUnformatted(e->alt_cue_name_3);
                ImGui::TableSetColumnIndex(9); ImGui::TextUnformatted(e->display_text_key);
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
        break;
    }
    // -------------------------------------------------------------------------
    case BinType::SeriesList:
    {
        struct Item { int si; const SeriesEntry* e; };
        std::vector<Item> items;
        for (int si = 0; si < (int)sources.size(); ++si)
            for (auto& e : sources[si].bin->seriesEntries)
                items.push_back({si, &e});

        RenderSectionHeader("series_list.bin", (int)items.size(), L"series_list.tsv",
            [&](const std::string& p){ ExportMergedOverviewTsv(sources, &ContentsBinData::seriesEntries, &ExportSeriesListTsv, p); });

        if (ImGui::BeginTable("##ov_series", 1 + 5, kOvTF, ImVec2(0.0f, 0.0f)))
        {
            ImGui::TableSetupScrollFreeze(1, 1);
            ImGui::TableSetupColumn("tkmod", ImGuiTableColumnFlags_WidthFixed, kTkmodW);
            for (int c = 0; c < 5; ++c)
                ImGui::TableSetupColumn(FieldNames::SeriesEntry[c], ImGuiTableColumnFlags_WidthFixed, ColumnWidths::kSeries[c]);
            ImGui::TableHeadersRow();

            for (int i = 0; i < (int)items.size(); ++i)
            {
                const int si = items[i].si; const auto* e = items[i].e;
                ImGui::TableNextRow();
                ImGui::PushID(i);
                RenderTkmodCell(si);
                ImGui::TableSetColumnIndex(1); ImGui::Text("%u", e->series_hash);
                ImGui::TableSetColumnIndex(2); ImGui::TextUnformatted(e->jacket_text_key);
                ImGui::TableSetColumnIndex(3); ImGui::TextUnformatted(e->jacket_icon_key);
                ImGui::TableSetColumnIndex(4); ImGui::TextUnformatted(e->logo_text_key);
                ImGui::TableSetColumnIndex(5); ImGui::TextUnformatted(e->logo_icon_key);
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
        break;
    }
    // -------------------------------------------------------------------------
    case BinType::TamMissionList:
    {
        // Show 8 fields: ids 0,2,3,4,5,6,7,8
        struct Item { int si; const TamMissionEntry* e; };
        std::vector<Item> items;
        for (int si = 0; si < (int)sources.size(); ++si)
            for (auto& e : sources[si].bin->tamMissionEntries)
                items.push_back({si, &e});

        RenderSectionHeader("tam_mission_list.bin", (int)items.size(), L"tam_mission_list.tsv",
            [&](const std::string& p){ ExportMergedOverviewTsv(sources, &ContentsBinData::tamMissionEntries, &ExportTamMissionListTsv, p); });

        // Column headers: TamMissionEntry[0], [2..8]
        const char* headers[8] = {
            FieldNames::TamMissionEntry[0], FieldNames::TamMissionEntry[2],
            FieldNames::TamMissionEntry[3], FieldNames::TamMissionEntry[4],
            FieldNames::TamMissionEntry[5], FieldNames::TamMissionEntry[6],
            FieldNames::TamMissionEntry[7], FieldNames::TamMissionEntry[8]
        };

        if (ImGui::BeginTable("##ov_tam", 1 + 8, kOvTF, ImVec2(0.0f, 0.0f)))
        {
            ImGui::TableSetupScrollFreeze(1, 1);
            ImGui::TableSetupColumn("tkmod", ImGuiTableColumnFlags_WidthFixed, kTkmodW);
            for (int c = 0; c < 8; ++c)
                ImGui::TableSetupColumn(headers[c], ImGuiTableColumnFlags_WidthFixed, ColumnWidths::kTamMission[c]);
            ImGui::TableHeadersRow();

            for (int i = 0; i < (int)items.size(); ++i)
            {
                const int si = items[i].si; const auto* e = items[i].e;
                ImGui::TableNextRow();
                ImGui::PushID(i);
                RenderTkmodCell(si);
                ImGui::TableSetColumnIndex(1); ImGui::Text("%u", e->mission_id);
                ImGui::TableSetColumnIndex(2); ImGui::Text("%u", e->value_2);
                ImGui::TableSetColumnIndex(3); ImGui::TextUnformatted(e->location);
                ImGui::TableSetColumnIndex(4); ImGui::Text("%u", e->hash_0);
                ImGui::TableSetColumnIndex(5); ImGui::Text("%u", e->hash_1);
                ImGui::TableSetColumnIndex(6); ImGui::Text("%u", e->hash_2);
                ImGui::TableSetColumnIndex(7); ImGui::Text("%u", e->hash_3);
                ImGui::TableSetColumnIndex(8); ImGui::Text("%u", e->hash_4);
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
        break;
    }
    // -------------------------------------------------------------------------
    case BinType::DramaPlayerStartList:
    {
        // Show 8 fields: ids 0,2,3,4,6,7,8,10
        struct Item { int si; const DramaPlayerStartEntry* e; };
        std::vector<Item> items;
        for (int si = 0; si < (int)sources.size(); ++si)
            for (auto& e : sources[si].bin->dramaPlayerStartEntries)
                items.push_back({si, &e});

        RenderSectionHeader("drama_player_start_list.bin", (int)items.size(), L"drama_player_start_list.tsv",
            [&](const std::string& p){ ExportMergedOverviewTsv(sources, &ContentsBinData::dramaPlayerStartEntries, &ExportDramaPlayerStartListTsv, p); });

        const char* headers[8] = {
            FieldNames::DramaPlayerStart[0], FieldNames::DramaPlayerStart[2],
            FieldNames::DramaPlayerStart[3], FieldNames::DramaPlayerStart[4],
            FieldNames::DramaPlayerStart[6], FieldNames::DramaPlayerStart[7],
            FieldNames::DramaPlayerStart[8], FieldNames::DramaPlayerStart[10]
        };

        if (ImGui::BeginTable("##ov_drama", 1 + 8, kOvTF, ImVec2(0.0f, 0.0f)))
        {
            ImGui::TableSetupScrollFreeze(1, 1);
            ImGui::TableSetupColumn("tkmod", ImGuiTableColumnFlags_WidthFixed, kTkmodW);
            for (int c = 0; c < 8; ++c)
                ImGui::TableSetupColumn(headers[c], ImGuiTableColumnFlags_WidthFixed, ColumnWidths::kDramaPlayerStart[c]);
            ImGui::TableHeadersRow();

            for (int i = 0; i < (int)items.size(); ++i)
            {
                const int si = items[i].si; const auto* e = items[i].e;
                ImGui::TableNextRow();
                ImGui::PushID(i);
                RenderTkmodCell(si);
                ImGui::TableSetColumnIndex(1); ImGui::Text("%u", e->character_hash);
                ImGui::TableSetColumnIndex(2); ImGui::Text("%u", e->index);
                ImGui::TableSetColumnIndex(3); ImGui::Text("%u", e->scene_hash);
                ImGui::TableSetColumnIndex(4); ImGui::Text("%u", e->config_hash);
                ImGui::TableSetColumnIndex(5); ImGui::Text("%.4g", e->pos_x);
                ImGui::TableSetColumnIndex(6); ImGui::Text("%.4g", e->pos_y);
                ImGui::TableSetColumnIndex(7); ImGui::Text("%u", e->state_hash);
                ImGui::TableSetColumnIndex(8); ImGui::Text("%.4g", e->scale);
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
        break;
    }
    // -------------------------------------------------------------------------
    case BinType::StageList:
    {
        struct Item { int si; const StageEntry* e; };
        std::vector<Item> items;
        for (int si = 0; si < (int)sources.size(); ++si)
            for (auto& e : sources[si].bin->stageEntries)
                items.push_back({si, &e});

        RenderSectionHeader("stage_list.bin", (int)items.size(), L"stage_list.tsv",
            [&](const std::string& p){ ExportMergedOverviewTsv(sources, &ContentsBinData::stageEntries, &ExportStageListTsv, p); });

        if (ImGui::BeginTable("##ov_stage", 1 + 37, kOvTF, ImVec2(0.0f, 0.0f)))
        {
            ImGui::TableSetupScrollFreeze(1, 1);
            ImGui::TableSetupColumn("tkmod", ImGuiTableColumnFlags_WidthFixed, kTkmodW);
            for (int c = 0; c < 37; ++c)
                ImGui::TableSetupColumn(FieldNames::StageEntry[c], ImGuiTableColumnFlags_WidthFixed, ColumnWidths::kStage[c]);
            ImGui::TableHeadersRow();

            ImGuiListClipper clipper;
            clipper.Begin((int)items.size());
            while (clipper.Step())
            for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i)
            {
                const int si = items[i].si; const auto* e = items[i].e;
                ImGui::TableNextRow();
                ImGui::PushID(i);
                RenderTkmodCell(si);
                ImGui::TableSetColumnIndex(1);  ImGui::TextUnformatted(e->stage_code);
                ImGui::TableSetColumnIndex(2);  ImGui::Text("%u", e->stage_hash);
                ImGui::TableSetColumnIndex(3);  ImGui::TextUnformatted(e->is_selectable ? "true" : "false");
                ImGui::TableSetColumnIndex(4);  ImGui::Text("%.4g", e->camera_offset);
                ImGui::TableSetColumnIndex(5);  ImGui::Text("%u", e->parent_stage_index);
                ImGui::TableSetColumnIndex(6);  ImGui::Text("%u", e->variant_hash);
                ImGui::TableSetColumnIndex(7);  ImGui::TextUnformatted(e->has_weather ? "true" : "false");
                ImGui::TableSetColumnIndex(8);  ImGui::TextUnformatted(e->is_active ? "true" : "false");
                ImGui::TableSetColumnIndex(9);  ImGui::TextUnformatted(e->flag_interlocked ? "true" : "false");
                ImGui::TableSetColumnIndex(10); ImGui::TextUnformatted(e->flag_ocean ? "true" : "false");
                ImGui::TableSetColumnIndex(11); ImGui::TextUnformatted(e->flag_10 ? "true" : "false");
                ImGui::TableSetColumnIndex(12); ImGui::TextUnformatted(e->flag_infinite ? "true" : "false");
                ImGui::TableSetColumnIndex(13); ImGui::TextUnformatted(e->flag_battle ? "true" : "false");
                ImGui::TableSetColumnIndex(14); ImGui::TextUnformatted(e->flag_13 ? "true" : "false");
                ImGui::TableSetColumnIndex(15); ImGui::TextUnformatted(e->flag_balcony ? "true" : "false");
                ImGui::TableSetColumnIndex(16); ImGui::TextUnformatted(e->flag_15 ? "true" : "false");
                ImGui::TableSetColumnIndex(17); ImGui::TextUnformatted(e->reserved_16 ? "true" : "false");
                ImGui::TableSetColumnIndex(18); ImGui::TextUnformatted(e->is_online_enabled ? "true" : "false");
                ImGui::TableSetColumnIndex(19); ImGui::TextUnformatted(e->is_ranked_enabled ? "true" : "false");
                ImGui::TableSetColumnIndex(20); ImGui::TextUnformatted(e->reserved_19 ? "true" : "false");
                ImGui::TableSetColumnIndex(21); ImGui::TextUnformatted(e->reserved_20 ? "true" : "false");
                ImGui::TableSetColumnIndex(22); ImGui::Text("%u", e->arena_width);
                ImGui::TableSetColumnIndex(23); ImGui::Text("%u", e->arena_depth);
                ImGui::TableSetColumnIndex(24); ImGui::Text("%u", e->reserved_23);
                ImGui::TableSetColumnIndex(25); ImGui::Text("%u", e->arena_param);
                ImGui::TableSetColumnIndex(26); ImGui::Text("%u", e->extra_width);
                ImGui::TableSetColumnIndex(27); ImGui::TextUnformatted(e->extra_group);
                ImGui::TableSetColumnIndex(28); ImGui::Text("%u", e->extra_depth);
                ImGui::TableSetColumnIndex(29); ImGui::TextUnformatted(e->group_id);
                ImGui::TableSetColumnIndex(30); ImGui::TextUnformatted(e->stage_name_key);
                ImGui::TableSetColumnIndex(31); ImGui::TextUnformatted(e->level_name);
                ImGui::TableSetColumnIndex(32); ImGui::TextUnformatted(e->sound_bank);
                ImGui::TableSetColumnIndex(33); ImGui::Text("%u", e->wall_distance_a);
                ImGui::TableSetColumnIndex(34); ImGui::Text("%u", e->wall_distance_b);
                ImGui::TableSetColumnIndex(35); ImGui::Text("%u", e->stage_mode);
                ImGui::TableSetColumnIndex(36); ImGui::Text("%u", e->reserved_35);
                ImGui::TableSetColumnIndex(37); ImGui::TextUnformatted(e->is_default_variant ? "true" : "false");
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
        break;
    }
    // -------------------------------------------------------------------------
    case BinType::BallPropertyList:
    {
        // Show 9 fields: ids 0,1,2,8,9,10,11,12,13
        struct Item { int si; const BallPropertyEntry* e; };
        std::vector<Item> items;
        for (int si = 0; si < (int)sources.size(); ++si)
            for (auto& e : sources[si].bin->ballPropertyEntries)
                items.push_back({si, &e});

        RenderSectionHeader("ball_property_list.bin", (int)items.size(), L"ball_property_list.tsv",
            [&](const std::string& p){ ExportMergedOverviewTsv(sources, &ContentsBinData::ballPropertyEntries, &ExportBallPropertyListTsv, p); });

        const char* headers[9] = {
            FieldNames::BallPropertyEntry[0], FieldNames::BallPropertyEntry[1],
            FieldNames::BallPropertyEntry[2], FieldNames::BallPropertyEntry[8],
            FieldNames::BallPropertyEntry[9], FieldNames::BallPropertyEntry[10],
            FieldNames::BallPropertyEntry[11], FieldNames::BallPropertyEntry[12],
            FieldNames::BallPropertyEntry[13]
        };

        if (ImGui::BeginTable("##ov_ballprop", 1 + 9, kOvTF, ImVec2(0.0f, 0.0f)))
        {
            ImGui::TableSetupScrollFreeze(1, 1);
            ImGui::TableSetupColumn("tkmod", ImGuiTableColumnFlags_WidthFixed, kTkmodW);
            for (int c = 0; c < 9; ++c)
                ImGui::TableSetupColumn(headers[c], ImGuiTableColumnFlags_WidthFixed, ColumnWidths::kBallProperty[c]);
            ImGui::TableHeadersRow();

            for (int i = 0; i < (int)items.size(); ++i)
            {
                const int si = items[i].si; const auto* e = items[i].e;
                ImGui::TableNextRow();
                ImGui::PushID(i);
                RenderTkmodCell(si);
                ImGui::TableSetColumnIndex(1); ImGui::Text("%u", e->ball_hash);
                ImGui::TableSetColumnIndex(2); ImGui::TextUnformatted(e->ball_code);
                ImGui::TableSetColumnIndex(3); ImGui::TextUnformatted(e->effect_name);
                ImGui::TableSetColumnIndex(4); ImGui::Text("%u", e->item_no);
                ImGui::TableSetColumnIndex(5); ImGui::Text("%u", e->rarity);
                ImGui::TableSetColumnIndex(6); ImGui::Text("%.4g", e->value_10);
                ImGui::TableSetColumnIndex(7); ImGui::Text("%.4g", e->value_11);
                ImGui::TableSetColumnIndex(8); ImGui::Text("%.4g", e->value_12);
                ImGui::TableSetColumnIndex(9); ImGui::Text("%.4g", e->value_13);
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
        break;
    }
    // -------------------------------------------------------------------------
    case BinType::BodyCylinderDataList:
    {
        // Show 10 fields: ids 0,1,2,3,8,9,10,15,16,17
        struct Item { int si; const BodyCylinderDataEntry* e; };
        std::vector<Item> items;
        for (int si = 0; si < (int)sources.size(); ++si)
            for (auto& e : sources[si].bin->bodyCylinderDataEntries)
                items.push_back({si, &e});

        RenderSectionHeader("body_cylinder_data_list.bin", (int)items.size());

        const char* headers[10] = {
            FieldNames::BodyCylinderDataEntry[0],  FieldNames::BodyCylinderDataEntry[1],
            FieldNames::BodyCylinderDataEntry[2],  FieldNames::BodyCylinderDataEntry[3],
            FieldNames::BodyCylinderDataEntry[8],  FieldNames::BodyCylinderDataEntry[9],
            FieldNames::BodyCylinderDataEntry[10], FieldNames::BodyCylinderDataEntry[15],
            FieldNames::BodyCylinderDataEntry[16], FieldNames::BodyCylinderDataEntry[17]
        };

        if (ImGui::BeginTable("##ov_bodycyl", 1 + 10, kOvTF, ImVec2(0.0f, 0.0f)))
        {
            ImGui::TableSetupScrollFreeze(1, 1);
            ImGui::TableSetupColumn("tkmod", ImGuiTableColumnFlags_WidthFixed, kTkmodW);
            for (int c = 0; c < 10; ++c)
                ImGui::TableSetupColumn(headers[c], ImGuiTableColumnFlags_WidthFixed, ColumnWidths::kBodyCylinder[c]);
            ImGui::TableHeadersRow();

            for (int i = 0; i < (int)items.size(); ++i)
            {
                const int si = items[i].si; const auto* e = items[i].e;
                ImGui::TableNextRow();
                ImGui::PushID(i);
                RenderTkmodCell(si);
                ImGui::TableSetColumnIndex(1);  ImGui::Text("%u", e->character_hash);
                ImGui::TableSetColumnIndex(2);  ImGui::Text("%.4g", e->cyl0_radius);
                ImGui::TableSetColumnIndex(3);  ImGui::Text("%.4g", e->cyl0_height);
                ImGui::TableSetColumnIndex(4);  ImGui::Text("%.4g", e->cyl0_offset_y);
                ImGui::TableSetColumnIndex(5);  ImGui::Text("%.4g", e->cyl1_radius);
                ImGui::TableSetColumnIndex(6);  ImGui::Text("%.4g", e->cyl1_height);
                ImGui::TableSetColumnIndex(7);  ImGui::Text("%.4g", e->cyl1_offset_y);
                ImGui::TableSetColumnIndex(8);  ImGui::Text("%.4g", e->cyl2_radius);
                ImGui::TableSetColumnIndex(9);  ImGui::Text("%.4g", e->cyl2_height);
                ImGui::TableSetColumnIndex(10); ImGui::Text("%.4g", e->cyl2_offset_y);
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
        break;
    }
    // -------------------------------------------------------------------------
    case BinType::CharacterSelectList:
    {
        // Sub-table 1: character_entries
        {
            struct Item { int si; const CharacterSelectHashEntry* e; };
            std::vector<Item> items;
            for (int si = 0; si < (int)sources.size(); ++si)
                for (auto& e : sources[si].bin->characterSelectHashEntries)
                    items.push_back({si, &e});

            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.65f, 0.82f, 1.00f, 1.00f));
            ImGui::TextUnformatted("character_select_list.bin");
            ImGui::PopStyleColor();
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.42f, 0.42f, 0.54f, 1.00f));
            ImGui::Text("(%d tkmods)", (int)sources.size());
            ImGui::PopStyleColor();
            ImGui::Separator();

            ImGui::TextDisabled("character_entries");
            if (ImGui::BeginTable("##ov_cshash", 1 + 1, kOvTF, ImVec2(0.0f, 0.0f)))
            {
                ImGui::TableSetupScrollFreeze(1, 1);
                ImGui::TableSetupColumn("tkmod", ImGuiTableColumnFlags_WidthFixed, kTkmodW);
                ImGui::TableSetupColumn(FieldNames::CharacterSelectHash[0], ImGuiTableColumnFlags_WidthFixed, ColumnWidths::kCharSelectHash[0]);
                ImGui::TableHeadersRow();

                for (int i = 0; i < (int)items.size(); ++i)
                {
                    const int si = items[i].si; const auto* e = items[i].e;
                    ImGui::TableNextRow();
                    ImGui::PushID(i);
                    RenderTkmodCell(si);
                    ImGui::TableSetColumnIndex(1); ImGui::Text("%u", e->character_hash);
                    ImGui::PopID();
                }
                ImGui::EndTable();
            }
        }

        ImGui::Spacing(); ImGui::Spacing();

        // Sub-table 2: param_entries
        {
            struct Item { int si; const CharacterSelectParamEntry* e; };
            std::vector<Item> items;
            for (int si = 0; si < (int)sources.size(); ++si)
                for (auto& e : sources[si].bin->characterSelectParamEntries)
                    items.push_back({si, &e});

            ImGui::TextDisabled("param_entries");
            if (ImGui::BeginTable("##ov_csparam", 1 + 2, kOvTF, ImVec2(0.0f, 0.0f)))
            {
                ImGui::TableSetupScrollFreeze(1, 1);
                ImGui::TableSetupColumn("tkmod", ImGuiTableColumnFlags_WidthFixed, kTkmodW);
                for (int c = 0; c < 2; ++c)
                    ImGui::TableSetupColumn(FieldNames::CharacterSelectParam[c], ImGuiTableColumnFlags_WidthFixed, ColumnWidths::kCharSelectParam[c]);
                ImGui::TableHeadersRow();

                for (int i = 0; i < (int)items.size(); ++i)
                {
                    const int si = items[i].si; const auto* e = items[i].e;
                    ImGui::TableNextRow();
                    ImGui::PushID(i);
                    RenderTkmodCell(si);
                    ImGui::TableSetColumnIndex(1); ImGui::Text("%u", e->game_version);
                    ImGui::TableSetColumnIndex(2); ImGui::Text("%u", e->value_1);
                    ImGui::PopID();
                }
                ImGui::EndTable();
            }
        }
        break;
    }
    // -------------------------------------------------------------------------
    case BinType::CustomizeItemProhibitDramaList:
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.65f, 0.82f, 1.00f, 1.00f));
        ImGui::TextUnformatted("customize_item_prohibit_drama_list.bin");
        ImGui::PopStyleColor();
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.42f, 0.42f, 0.54f, 1.00f));
        ImGui::Text("(%d tkmods)", (int)sources.size());
        ImGui::PopStyleColor();
        ImGui::Separator();

        // Group 0
        {
            struct Item { int si; const CustomizeItemProhibitDramaEntry* e; };
            std::vector<Item> items;
            for (int si = 0; si < (int)sources.size(); ++si)
                for (auto& e : sources[si].bin->prohibitDramaGroup0)
                    items.push_back({si, &e});

            ImGui::TextDisabled("group_0 entries");
            if (ImGui::BeginTable("##ov_prohibit0", 1 + 2, kOvTF, ImVec2(0.0f, 0.0f)))
            {
                ImGui::TableSetupScrollFreeze(1, 1);
                ImGui::TableSetupColumn("tkmod", ImGuiTableColumnFlags_WidthFixed, kTkmodW);
                for (int c = 0; c < 2; ++c)
                    ImGui::TableSetupColumn(FieldNames::CustomizeItemProhibitDrama[c], ImGuiTableColumnFlags_WidthFixed, ColumnWidths::kProhibitDrama[c]);
                ImGui::TableHeadersRow();

                for (int i = 0; i < (int)items.size(); ++i)
                {
                    const int si = items[i].si; const auto* e = items[i].e;
                    ImGui::TableNextRow();
                    ImGui::PushID(i);
                    RenderTkmodCell(si);
                    ImGui::TableSetColumnIndex(1); ImGui::Text("%d", e->value_0);
                    ImGui::TableSetColumnIndex(2); ImGui::Text("%d", e->value_1);
                    ImGui::PopID();
                }
                ImGui::EndTable();
            }
        }

        ImGui::Spacing(); ImGui::Spacing();

        // Group 1
        {
            struct Item { int si; const CustomizeItemProhibitDramaEntry* e; };
            std::vector<Item> items;
            for (int si = 0; si < (int)sources.size(); ++si)
                for (auto& e : sources[si].bin->prohibitDramaGroup1)
                    items.push_back({si, &e});

            ImGui::TextDisabled("group_1 entries");
            if (ImGui::BeginTable("##ov_prohibit1", 1 + 2, kOvTF, ImVec2(0.0f, 0.0f)))
            {
                ImGui::TableSetupScrollFreeze(1, 1);
                ImGui::TableSetupColumn("tkmod", ImGuiTableColumnFlags_WidthFixed, kTkmodW);
                for (int c = 0; c < 2; ++c)
                    ImGui::TableSetupColumn(FieldNames::CustomizeItemProhibitDrama[c], ImGuiTableColumnFlags_WidthFixed, ColumnWidths::kProhibitDrama[c]);
                ImGui::TableHeadersRow();

                for (int i = 0; i < (int)items.size(); ++i)
                {
                    const int si = items[i].si; const auto* e = items[i].e;
                    ImGui::TableNextRow();
                    ImGui::PushID(i);
                    RenderTkmodCell(si);
                    ImGui::TableSetColumnIndex(1); ImGui::Text("%d", e->value_0);
                    ImGui::TableSetColumnIndex(2); ImGui::Text("%d", e->value_1);
                    ImGui::PopID();
                }
                ImGui::EndTable();
            }
        }
        break;
    }
    // -------------------------------------------------------------------------
    case BinType::BattleMotionList:
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.65f, 0.82f, 1.00f, 1.00f));
        ImGui::TextUnformatted("battle_motion_list.bin");
        ImGui::PopStyleColor();
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.42f, 0.42f, 0.54f, 1.00f));
        ImGui::Text("(%d tkmods)", (int)sources.size());
        ImGui::PopStyleColor();
        ImGui::Separator();

        // Main entries
        {
            struct Item { int si; const BattleMotionEntry* e; };
            std::vector<Item> items;
            for (int si = 0; si < (int)sources.size(); ++si)
                for (auto& e : sources[si].bin->battleMotionEntries)
                    items.push_back({si, &e});

            ImGui::TextDisabled("battle_motion entries");
            if (ImGui::BeginTable("##ov_bmotion", 1 + 3, kOvTF, ImVec2(0.0f, 0.0f)))
            {
                ImGui::TableSetupScrollFreeze(1, 1);
                ImGui::TableSetupColumn("tkmod", ImGuiTableColumnFlags_WidthFixed, kTkmodW);
                for (int c = 0; c < 3; ++c)
                    ImGui::TableSetupColumn(FieldNames::BattleMotionEntry[c], ImGuiTableColumnFlags_WidthFixed, ColumnWidths::kBattleMotion[c]);
                ImGui::TableHeadersRow();

                for (int i = 0; i < (int)items.size(); ++i)
                {
                    const int si = items[i].si; const auto* e = items[i].e;
                    ImGui::TableNextRow();
                    ImGui::PushID(i);
                    RenderTkmodCell(si);
                    ImGui::TableSetColumnIndex(1); ImGui::Text("%u", (uint32_t)e->motion_id);
                    ImGui::TableSetColumnIndex(2); ImGui::Text("%u", e->value_1);
                    ImGui::TableSetColumnIndex(3); ImGui::Text("%u", e->value_2);
                    ImGui::PopID();
                }
                ImGui::EndTable();
            }
        }

        ImGui::Spacing(); ImGui::Spacing();

        // Alt entries
        {
            struct Item { int si; const BattleMotionEntry* e; };
            std::vector<Item> items;
            for (int si = 0; si < (int)sources.size(); ++si)
                for (auto& e : sources[si].bin->battleMotionEntriesAlt)
                    items.push_back({si, &e});

            ImGui::TextDisabled("battle_motion_alt entries");
            if (ImGui::BeginTable("##ov_bmotionalt", 1 + 3, kOvTF, ImVec2(0.0f, 0.0f)))
            {
                ImGui::TableSetupScrollFreeze(1, 1);
                ImGui::TableSetupColumn("tkmod", ImGuiTableColumnFlags_WidthFixed, kTkmodW);
                for (int c = 0; c < 3; ++c)
                    ImGui::TableSetupColumn(FieldNames::BattleMotionEntry[c], ImGuiTableColumnFlags_WidthFixed, ColumnWidths::kBattleMotion[c]);
                ImGui::TableHeadersRow();

                for (int i = 0; i < (int)items.size(); ++i)
                {
                    const int si = items[i].si; const auto* e = items[i].e;
                    ImGui::TableNextRow();
                    ImGui::PushID(i);
                    RenderTkmodCell(si);
                    ImGui::TableSetColumnIndex(1); ImGui::Text("%u", (uint32_t)e->motion_id);
                    ImGui::TableSetColumnIndex(2); ImGui::Text("%u", e->value_1);
                    ImGui::TableSetColumnIndex(3); ImGui::Text("%u", e->value_2);
                    ImGui::PopID();
                }
                ImGui::EndTable();
            }
        }
        break;
    }
    // -------------------------------------------------------------------------
    case BinType::ArcadeCpuList:
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.65f, 0.82f, 1.00f, 1.00f));
        ImGui::TextUnformatted("arcade_cpu_list.bin");
        ImGui::PopStyleColor();
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.42f, 0.42f, 0.54f, 1.00f));
        ImGui::Text("(%d tkmods)", (int)sources.size());
        ImGui::PopStyleColor();
        ImGui::Separator();

        // Character entries
        {
            struct Item { int si; const ArcadeCpuCharacterEntry* e; };
            std::vector<Item> items;
            for (int si = 0; si < (int)sources.size(); ++si)
                for (auto& e : sources[si].bin->arcadeCpuCharacterEntries)
                    items.push_back({si, &e});

            ImGui::TextDisabled("character entries");
            if (ImGui::BeginTable("##ov_acpuchar", 1 + 7, kOvTF, ImVec2(0.0f, 0.0f)))
            {
                ImGui::TableSetupScrollFreeze(1, 1);
                ImGui::TableSetupColumn("tkmod", ImGuiTableColumnFlags_WidthFixed, kTkmodW);
                for (int c = 0; c < 7; ++c)
                    ImGui::TableSetupColumn(FieldNames::ArcadeCpuCharacter[c], ImGuiTableColumnFlags_WidthFixed, ColumnWidths::kArcadeCpuCharacter[c]);
                ImGui::TableHeadersRow();

                for (int i = 0; i < (int)items.size(); ++i)
                {
                    const int si = items[i].si; const auto* e = items[i].e;
                    ImGui::TableNextRow();
                    ImGui::PushID(i);
                    RenderTkmodCell(si);
                    ImGui::TableSetColumnIndex(1); ImGui::Text("%u", e->character_hash);
                    ImGui::TableSetColumnIndex(2); ImGui::Text("%u", e->ai_level);
                    ImGui::TableSetColumnIndex(3); ImGui::Text("%.4g", e->float_1);
                    ImGui::TableSetColumnIndex(4); ImGui::Text("%u", e->uint_2);
                    ImGui::TableSetColumnIndex(5); ImGui::Text("%.4g", e->float_2);
                    ImGui::TableSetColumnIndex(6); ImGui::Text("%u", e->uint_3);
                    ImGui::TableSetColumnIndex(7); ImGui::Text("%.4g", e->float_3);
                    ImGui::PopID();
                }
                ImGui::EndTable();
            }
        }

        ImGui::Spacing(); ImGui::Spacing();

        // Hash group A
        {
            struct Item { int si; const ArcadeCpuHashEntry* e; };
            std::vector<Item> items;
            for (int si = 0; si < (int)sources.size(); ++si)
                for (auto& e : sources[si].bin->arcadeCpuHashGroupA)
                    items.push_back({si, &e});

            ImGui::TextDisabled("hash_group_a");
            if (ImGui::BeginTable("##ov_acpuha", 1 + 1, kOvTF, ImVec2(0.0f, 0.0f)))
            {
                ImGui::TableSetupScrollFreeze(1, 1);
                ImGui::TableSetupColumn("tkmod", ImGuiTableColumnFlags_WidthFixed, kTkmodW);
                ImGui::TableSetupColumn(FieldNames::ArcadeCpuHash[0], ImGuiTableColumnFlags_WidthFixed, ColumnWidths::kArcadeCpuHash[0]);
                ImGui::TableHeadersRow();

                for (int i = 0; i < (int)items.size(); ++i)
                {
                    const int si = items[i].si; const auto* e = items[i].e;
                    ImGui::TableNextRow();
                    ImGui::PushID(i);
                    RenderTkmodCell(si);
                    ImGui::TableSetColumnIndex(1); ImGui::Text("%u", e->value_hash);
                    ImGui::PopID();
                }
                ImGui::EndTable();
            }
        }

        ImGui::Spacing(); ImGui::Spacing();

        // Hash group B
        {
            struct Item { int si; const ArcadeCpuHashEntry* e; };
            std::vector<Item> items;
            for (int si = 0; si < (int)sources.size(); ++si)
                for (auto& e : sources[si].bin->arcadeCpuHashGroupB)
                    items.push_back({si, &e});

            ImGui::TextDisabled("hash_group_b");
            if (ImGui::BeginTable("##ov_acpuhb", 1 + 1, kOvTF, ImVec2(0.0f, 0.0f)))
            {
                ImGui::TableSetupScrollFreeze(1, 1);
                ImGui::TableSetupColumn("tkmod", ImGuiTableColumnFlags_WidthFixed, kTkmodW);
                ImGui::TableSetupColumn(FieldNames::ArcadeCpuHash[0], ImGuiTableColumnFlags_WidthFixed, ColumnWidths::kArcadeCpuHash[0]);
                ImGui::TableHeadersRow();

                for (int i = 0; i < (int)items.size(); ++i)
                {
                    const int si = items[i].si; const auto* e = items[i].e;
                    ImGui::TableNextRow();
                    ImGui::PushID(i);
                    RenderTkmodCell(si);
                    ImGui::TableSetColumnIndex(1); ImGui::Text("%u", e->value_hash);
                    ImGui::PopID();
                }
                ImGui::EndTable();
            }
        }

        ImGui::Spacing(); ImGui::Spacing();

        // Rule entries
        {
            struct Item { int si; const ArcadeCpuRuleEntry* e; };
            std::vector<Item> items;
            for (int si = 0; si < (int)sources.size(); ++si)
                for (auto& e : sources[si].bin->arcadeCpuRuleEntries)
                    items.push_back({si, &e});

            ImGui::TextDisabled("rule entries");
            if (ImGui::BeginTable("##ov_acpurule", 1 + 4, kOvTF, ImVec2(0.0f, 0.0f)))
            {
                ImGui::TableSetupScrollFreeze(1, 1);
                ImGui::TableSetupColumn("tkmod", ImGuiTableColumnFlags_WidthFixed, kTkmodW);
                for (int c = 0; c < 4; ++c)
                    ImGui::TableSetupColumn(FieldNames::ArcadeCpuRule[c], ImGuiTableColumnFlags_WidthFixed, ColumnWidths::kArcadeCpuRule[c]);
                ImGui::TableHeadersRow();

                for (int i = 0; i < (int)items.size(); ++i)
                {
                    const int si = items[i].si; const auto* e = items[i].e;
                    ImGui::TableNextRow();
                    ImGui::PushID(i);
                    RenderTkmodCell(si);
                    ImGui::TableSetColumnIndex(1); ImGui::Text("%u", (uint32_t)e->flag_0);
                    ImGui::TableSetColumnIndex(2); ImGui::Text("%u", (uint32_t)e->flag_1);
                    ImGui::TableSetColumnIndex(3); ImGui::Text("%u", e->value_2);
                    ImGui::TableSetColumnIndex(4); ImGui::Text("%u", e->value_3);
                    ImGui::PopID();
                }
                ImGui::EndTable();
            }
        }
        break;
    }
    // -------------------------------------------------------------------------
    case BinType::BallRecommendList:
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.65f, 0.82f, 1.00f, 1.00f));
        ImGui::TextUnformatted("ball_recommend_list.bin");
        ImGui::PopStyleColor();
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.42f, 0.42f, 0.54f, 1.00f));
        ImGui::Text("(%d tkmods)", (int)sources.size());
        ImGui::PopStyleColor();
        ImGui::Separator();

        auto RenderBallRecommendGroup = [&](const char* label, const char* tableId,
            std::vector<std::pair<int, const BallRecommendEntry*>>& items)
        {
            ImGui::TextDisabled("%s", label);
            if (ImGui::BeginTable(tableId, 1 + 5, kOvTF, ImVec2(0.0f, 0.0f)))
            {
                ImGui::TableSetupScrollFreeze(1, 1);
                ImGui::TableSetupColumn("tkmod", ImGuiTableColumnFlags_WidthFixed, kTkmodW);
                for (int c = 0; c < 5; ++c)
                    ImGui::TableSetupColumn(FieldNames::BallRecommendEntry[c], ImGuiTableColumnFlags_WidthFixed, ColumnWidths::kBallRecommend[c]);
                ImGui::TableHeadersRow();

                for (int i = 0; i < (int)items.size(); ++i)
                {
                    const int si = items[i].first; const auto* e = items[i].second;
                    ImGui::TableNextRow();
                    ImGui::PushID(i);
                    RenderTkmodCell(si);
                    ImGui::TableSetColumnIndex(1); ImGui::Text("%u", e->character_hash);
                    ImGui::TableSetColumnIndex(2); ImGui::TextUnformatted(e->move_name_key);
                    ImGui::TableSetColumnIndex(3); ImGui::TextUnformatted(e->command_text_key);
                    ImGui::TableSetColumnIndex(4); ImGui::Text("%u", e->unk_3);
                    ImGui::TableSetColumnIndex(5); ImGui::Text("%u", e->unk_4);
                    ImGui::PopID();
                }
                ImGui::EndTable();
            }
        };

        {
            std::vector<std::pair<int, const BallRecommendEntry*>> items;
            for (int si = 0; si < (int)sources.size(); ++si)
                for (auto& e : sources[si].bin->ballRecommendGroup0)
                    items.push_back({si, &e});
            RenderBallRecommendGroup("group_0", "##ov_brec0", items);
        }

        ImGui::Spacing(); ImGui::Spacing();

        {
            std::vector<std::pair<int, const BallRecommendEntry*>> items;
            for (int si = 0; si < (int)sources.size(); ++si)
                for (auto& e : sources[si].bin->ballRecommendGroup1)
                    items.push_back({si, &e});
            RenderBallRecommendGroup("group_1", "##ov_brec1", items);
        }

        ImGui::Spacing(); ImGui::Spacing();

        {
            std::vector<std::pair<int, const BallRecommendEntry*>> items;
            for (int si = 0; si < (int)sources.size(); ++si)
                for (auto& e : sources[si].bin->ballRecommendGroup2)
                    items.push_back({si, &e});
            RenderBallRecommendGroup("group_2", "##ov_brec2", items);
        }
        break;
    }
    // -------------------------------------------------------------------------
    case BinType::BallSettingList:
    {
        RenderSectionHeader("ball_setting_list.bin", (int)sources.size());
        ImGui::TextDisabled("BallSettingList contains a single data table per file. Open in the editor to view and edit.");
        break;
    }
    // -------------------------------------------------------------------------
    case BinType::BattleCommonList:
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.65f, 0.82f, 1.00f, 1.00f));
        ImGui::TextUnformatted("battle_common_list.bin");
        ImGui::PopStyleColor();
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.42f, 0.42f, 0.54f, 1.00f));
        ImGui::Text("(%d tkmods)", (int)sources.size());
        ImGui::PopStyleColor();
        ImGui::Separator();

        // single_value_entries
        {
            struct Item { int si; const BattleCommonSingleValueEntry* e; };
            std::vector<Item> items;
            for (int si = 0; si < (int)sources.size(); ++si)
                for (auto& e : sources[si].bin->battleCommonSingleValueEntries)
                    items.push_back({si, &e});

            ImGui::TextDisabled("single_value_entries");
            if (ImGui::BeginTable("##ov_bcsv", 1 + 1, kOvTF, ImVec2(0.0f, 0.0f)))
            {
                ImGui::TableSetupScrollFreeze(1, 1);
                ImGui::TableSetupColumn("tkmod", ImGuiTableColumnFlags_WidthFixed, kTkmodW);
                ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_WidthFixed, ColumnWidths::kBattleCommonGeneric);
                ImGui::TableHeadersRow();

                for (int i = 0; i < (int)items.size(); ++i)
                {
                    const int si = items[i].si; const auto* e = items[i].e;
                    ImGui::TableNextRow();
                    ImGui::PushID(i);
                    RenderTkmodCell(si);
                    ImGui::TableSetColumnIndex(1); ImGui::Text("%u", e->value);
                    ImGui::PopID();
                }
                ImGui::EndTable();
            }
        }

        ImGui::Spacing(); ImGui::Spacing();

        // character_scale_entries
        {
            struct Item { int si; const BattleCommonCharacterScaleEntry* e; };
            std::vector<Item> items;
            for (int si = 0; si < (int)sources.size(); ++si)
                for (auto& e : sources[si].bin->battleCommonCharacterScaleEntries)
                    items.push_back({si, &e});

            ImGui::TextDisabled("character_scale_entries");
            if (ImGui::BeginTable("##ov_bccs", 1 + 8, kOvTF, ImVec2(0.0f, 0.0f)))
            {
                ImGui::TableSetupScrollFreeze(1, 1);
                ImGui::TableSetupColumn("tkmod", ImGuiTableColumnFlags_WidthFixed, kTkmodW);
                ImGui::TableSetupColumn("hash_0", ImGuiTableColumnFlags_WidthFixed, ColumnWidths::kBattleCommonCharacterScale0);
                for (int c = 1; c < 8; ++c)
                {
                    char colName[16]; snprintf(colName, sizeof(colName), "value_%d", c);
                    ImGui::TableSetupColumn(colName, ImGuiTableColumnFlags_WidthFixed, ColumnWidths::kBattleCommonCharacterScaleRest);
                }
                ImGui::TableHeadersRow();

                for (int i = 0; i < (int)items.size(); ++i)
                {
                    const int si = items[i].si; const auto* e = items[i].e;
                    ImGui::TableNextRow();
                    ImGui::PushID(i);
                    RenderTkmodCell(si);
                    ImGui::TableSetColumnIndex(1); ImGui::Text("%u", e->hash_0);
                    ImGui::TableSetColumnIndex(2); ImGui::Text("%.4g", e->value_1);
                    ImGui::TableSetColumnIndex(3); ImGui::Text("%.4g", e->value_2);
                    ImGui::TableSetColumnIndex(4); ImGui::Text("%.4g", e->value_3);
                    ImGui::TableSetColumnIndex(5); ImGui::Text("%.4g", e->value_4);
                    ImGui::TableSetColumnIndex(6); ImGui::Text("%.4g", e->value_5);
                    ImGui::TableSetColumnIndex(7); ImGui::Text("%.4g", e->value_6);
                    ImGui::TableSetColumnIndex(8); ImGui::Text("%.4g", e->value_7);
                    ImGui::PopID();
                }
                ImGui::EndTable();
            }
        }

        ImGui::Spacing(); ImGui::Spacing();

        // pair_entries
        {
            struct Item { int si; const BattleCommonPairEntry* e; };
            std::vector<Item> items;
            for (int si = 0; si < (int)sources.size(); ++si)
                for (auto& e : sources[si].bin->battleCommonPairEntries)
                    items.push_back({si, &e});

            ImGui::TextDisabled("pair_entries");
            if (ImGui::BeginTable("##ov_bcpair", 1 + 3, kOvTF, ImVec2(0.0f, 0.0f)))
            {
                ImGui::TableSetupScrollFreeze(1, 1);
                ImGui::TableSetupColumn("tkmod", ImGuiTableColumnFlags_WidthFixed, kTkmodW);
                ImGui::TableSetupColumn("value_0", ImGuiTableColumnFlags_WidthFixed, ColumnWidths::kBattleCommonGeneric);
                ImGui::TableSetupColumn("value_1", ImGuiTableColumnFlags_WidthFixed, ColumnWidths::kBattleCommonGeneric);
                ImGui::TableSetupColumn("value_2", ImGuiTableColumnFlags_WidthFixed, ColumnWidths::kBattleCommonGeneric);
                ImGui::TableHeadersRow();

                for (int i = 0; i < (int)items.size(); ++i)
                {
                    const int si = items[i].si; const auto* e = items[i].e;
                    ImGui::TableNextRow();
                    ImGui::PushID(i);
                    RenderTkmodCell(si);
                    ImGui::TableSetColumnIndex(1); ImGui::Text("%u", e->value_0);
                    ImGui::TableSetColumnIndex(2); ImGui::Text("%u", e->value_1);
                    ImGui::TableSetColumnIndex(3); ImGui::Text("%u", e->value_2);
                    ImGui::PopID();
                }
                ImGui::EndTable();
            }
        }

        ImGui::Spacing(); ImGui::Spacing();

        // misc_entries
        {
            struct Item { int si; const BattleCommonMiscEntry* e; };
            std::vector<Item> items;
            for (int si = 0; si < (int)sources.size(); ++si)
                for (auto& e : sources[si].bin->battleCommonMiscEntries)
                    items.push_back({si, &e});

            ImGui::TextDisabled("misc_entries");
            if (ImGui::BeginTable("##ov_bcmisc", 1 + 3, kOvTF, ImVec2(0.0f, 0.0f)))
            {
                ImGui::TableSetupScrollFreeze(1, 1);
                ImGui::TableSetupColumn("tkmod", ImGuiTableColumnFlags_WidthFixed, kTkmodW);
                ImGui::TableSetupColumn("value_0", ImGuiTableColumnFlags_WidthFixed, ColumnWidths::kBattleCommonGeneric);
                ImGui::TableSetupColumn("value_1", ImGuiTableColumnFlags_WidthFixed, ColumnWidths::kBattleCommonGeneric);
                ImGui::TableSetupColumn("value_2", ImGuiTableColumnFlags_WidthFixed, ColumnWidths::kBattleCommonGeneric);
                ImGui::TableHeadersRow();

                for (int i = 0; i < (int)items.size(); ++i)
                {
                    const int si = items[i].si; const auto* e = items[i].e;
                    ImGui::TableNextRow();
                    ImGui::PushID(i);
                    RenderTkmodCell(si);
                    ImGui::TableSetColumnIndex(1); ImGui::Text("%.4g", e->value_0);
                    ImGui::TableSetColumnIndex(2); ImGui::Text("%.4g", e->value_1);
                    ImGui::TableSetColumnIndex(3); ImGui::Text("%.4g", e->value_2);
                    ImGui::PopID();
                }
                ImGui::EndTable();
            }
        }
        break;
    }
    // -------------------------------------------------------------------------
    case BinType::BattleCpuList:
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.65f, 0.82f, 1.00f, 1.00f));
        ImGui::TextUnformatted("battle_cpu_list.bin");
        ImGui::PopStyleColor();
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.42f, 0.42f, 0.54f, 1.00f));
        ImGui::Text("(%d tkmods)", (int)sources.size());
        ImGui::PopStyleColor();
        ImGui::Separator();

        // rank_entries
        {
            struct Item { int si; const BattleCpuRankEntry* e; };
            std::vector<Item> items;
            for (int si = 0; si < (int)sources.size(); ++si)
                for (auto& e : sources[si].bin->battleCpuRankEntries)
                    items.push_back({si, &e});

            ImGui::TextDisabled("rank_entries");
            if (ImGui::BeginTable("##ov_bcpurank", 1 + 48, kOvTF, ImVec2(0.0f, 0.0f)))
            {
                ImGui::TableSetupScrollFreeze(1, 1);
                ImGui::TableSetupColumn("tkmod", ImGuiTableColumnFlags_WidthFixed, kTkmodW);
                for (int c = 0; c < 47; ++c)
                    ImGui::TableSetupColumn(FieldNames::BattleCpuRank[c], ImGuiTableColumnFlags_WidthFixed, ColumnWidths::kBattleCpuRankGeneric);
                ImGui::TableSetupColumn(FieldNames::BattleCpuRank[47], ImGuiTableColumnFlags_WidthFixed, ColumnWidths::kBattleCpuRank47);
                ImGui::TableHeadersRow();

                for (int i = 0; i < (int)items.size(); ++i)
                {
                    const int si = items[i].si; const auto* e = items[i].e;
                    ImGui::TableNextRow();
                    ImGui::PushID(i);
                    RenderTkmodCell(si);
                    for (int c = 0; c < 47; ++c)
                    {
                        ImGui::TableSetColumnIndex(1 + c);
                        ImGui::Text("%u", e->values[c]);
                    }
                    ImGui::TableSetColumnIndex(48); ImGui::TextUnformatted(e->rank_label);
                    ImGui::PopID();
                }
                ImGui::EndTable();
            }
        }

        ImGui::Spacing(); ImGui::Spacing();

        // step_entries
        {
            struct Item { int si; const BattleCpuStepEntry* e; };
            std::vector<Item> items;
            for (int si = 0; si < (int)sources.size(); ++si)
                for (auto& e : sources[si].bin->battleCpuStepEntries)
                    items.push_back({si, &e});

            ImGui::TextDisabled("step_entries");
            if (ImGui::BeginTable("##ov_bcpustep", 1 + 4, kOvTF, ImVec2(0.0f, 0.0f)))
            {
                ImGui::TableSetupScrollFreeze(1, 1);
                ImGui::TableSetupColumn("tkmod", ImGuiTableColumnFlags_WidthFixed, kTkmodW);
                for (int c = 0; c < 4; ++c)
                    ImGui::TableSetupColumn(FieldNames::BattleCpuStep[c], ImGuiTableColumnFlags_WidthFixed, ColumnWidths::kBattleCpuStepGeneric);
                ImGui::TableHeadersRow();

                for (int i = 0; i < (int)items.size(); ++i)
                {
                    const int si = items[i].si; const auto* e = items[i].e;
                    ImGui::TableNextRow();
                    ImGui::PushID(i);
                    RenderTkmodCell(si);
                    ImGui::TableSetColumnIndex(1); ImGui::Text("%u", e->value_0);
                    ImGui::TableSetColumnIndex(2); ImGui::Text("%u", e->value_1);
                    ImGui::TableSetColumnIndex(3); ImGui::Text("%u", e->value_2);
                    ImGui::TableSetColumnIndex(4); ImGui::Text("%u", e->value_3);
                    ImGui::PopID();
                }
                ImGui::EndTable();
            }
        }
        break;
    }
    // -------------------------------------------------------------------------
    case BinType::RankList:
    {
        // Collect all rank items from all sources and all groups
        struct Item { int si; uint32_t group_id; const RankItem* e; };
        std::vector<Item> items;
        for (int si = 0; si < (int)sources.size(); ++si)
            for (auto& g : sources[si].bin->rankGroups)
                for (auto& e : g.entries)
                    items.push_back({si, g.group_id, &e});

        RenderSectionHeader("rank_list.bin", (int)items.size());

        if (ImGui::BeginTable("##ov_rank", 1 + 1 + 4, kOvTF, ImVec2(0.0f, 0.0f)))
        {
            ImGui::TableSetupScrollFreeze(1, 1);
            ImGui::TableSetupColumn("tkmod", ImGuiTableColumnFlags_WidthFixed, kTkmodW);
            ImGui::TableSetupColumn("group_id", ImGuiTableColumnFlags_WidthFixed, 80.0f);
            ImGui::TableSetupColumn(FieldNames::RankItem[0], ImGuiTableColumnFlags_WidthFixed, ColumnWidths::kRankItem[0]);
            ImGui::TableSetupColumn(FieldNames::RankItem[1], ImGuiTableColumnFlags_WidthFixed, ColumnWidths::kRankItem[1]);
            ImGui::TableSetupColumn(FieldNames::RankItem[2], ImGuiTableColumnFlags_WidthFixed, ColumnWidths::kRankItem[2]);
            ImGui::TableSetupColumn(FieldNames::RankItem[3], ImGuiTableColumnFlags_WidthFixed, ColumnWidths::kRankItem[3]);
            ImGui::TableHeadersRow();

            for (int i = 0; i < (int)items.size(); ++i)
            {
                const int si = items[i].si; const uint32_t gid = items[i].group_id; const auto* e = items[i].e;
                ImGui::TableNextRow();
                ImGui::PushID(i);
                RenderTkmodCell(si);
                ImGui::TableSetColumnIndex(1); ImGui::Text("%u", gid);
                ImGui::TableSetColumnIndex(2); ImGui::Text("%u", e->hash);
                ImGui::TableSetColumnIndex(3); ImGui::TextUnformatted(e->text_key);
                ImGui::TableSetColumnIndex(4); ImGui::TextUnformatted(e->name);
                ImGui::TableSetColumnIndex(5); ImGui::Text("%u", e->rank);
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
        break;
    }
    // -------------------------------------------------------------------------
    case BinType::AssistInputList:
    {
        struct Item { int si; const AssistInputEntry* e; };
        std::vector<Item> items;
        for (int si = 0; si < (int)sources.size(); ++si)
            for (auto& e : sources[si].bin->assistInputEntries)
                items.push_back({si, &e});

        RenderSectionHeader("assist_input_list.bin", (int)items.size());

        if (ImGui::BeginTable("##ov_assist", 1 + 59, kOvTF, ImVec2(0.0f, 0.0f)))
        {
            ImGui::TableSetupScrollFreeze(1, 1);
            ImGui::TableSetupColumn("tkmod", ImGuiTableColumnFlags_WidthFixed, kTkmodW);
            ImGui::TableSetupColumn(FieldNames::AssistInputEntry[0], ImGuiTableColumnFlags_WidthFixed, ColumnWidths::kAssistInputEntry0);
            for (int c = 1; c < 59; ++c)
                ImGui::TableSetupColumn(FieldNames::AssistInputEntry[c], ImGuiTableColumnFlags_WidthFixed, ColumnWidths::kAssistInputValue);
            ImGui::TableHeadersRow();

            ImGuiListClipper clipper;
            clipper.Begin((int)items.size());
            while (clipper.Step())
            for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i)
            {
                const int si = items[i].si; const auto* e = items[i].e;
                ImGui::TableNextRow();
                ImGui::PushID(i);
                RenderTkmodCell(si);
                ImGui::TableSetColumnIndex(1); ImGui::Text("%u", e->character_hash);
                for (int c = 0; c < 58; ++c)
                {
                    ImGui::TableSetColumnIndex(2 + c);
                    ImGui::Text("%d", e->values[c]);
                }
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
        break;
    }
    // -------------------------------------------------------------------------
    case BinType::CustomizePanelList:
    {
        struct Item { int si; const CustomizePanelEntry* e; };
        std::vector<Item> items;
        for (int si = 0; si < (int)sources.size(); ++si)
            for (auto& e : sources[si].bin->customizePanelEntries)
                items.push_back({si, &e});

        RenderSectionHeader("customize_panel_list.bin", (int)items.size());

        // Widths: ids 5,6,7,8 = kCustomizePanelString, others = kCustomizePanelDefault
        const float panelWidths[11] = {
            ColumnWidths::kCustomizePanelDefault, ColumnWidths::kCustomizePanelDefault,
            ColumnWidths::kCustomizePanelDefault, ColumnWidths::kCustomizePanelDefault,
            ColumnWidths::kCustomizePanelDefault, ColumnWidths::kCustomizePanelString,
            ColumnWidths::kCustomizePanelString,  ColumnWidths::kCustomizePanelString,
            ColumnWidths::kCustomizePanelString,  ColumnWidths::kCustomizePanelDefault,
            ColumnWidths::kCustomizePanelDefault,
        };

        if (ImGui::BeginTable("##ov_panel", 1 + 11, kOvTF, ImVec2(0.0f, 0.0f)))
        {
            ImGui::TableSetupScrollFreeze(1, 1);
            ImGui::TableSetupColumn("tkmod", ImGuiTableColumnFlags_WidthFixed, kTkmodW);
            for (int c = 0; c < 11; ++c)
                ImGui::TableSetupColumn(FieldNames::CustomizePanelEntry[c], ImGuiTableColumnFlags_WidthFixed, panelWidths[c]);
            ImGui::TableHeadersRow();

            for (int i = 0; i < (int)items.size(); ++i)
            {
                const int si = items[i].si; const auto* e = items[i].e;
                ImGui::TableNextRow();
                ImGui::PushID(i);
                RenderTkmodCell(si);
                ImGui::TableSetColumnIndex(1);  ImGui::Text("%u", e->panel_hash);
                ImGui::TableSetColumnIndex(2);  ImGui::Text("%u", e->panel_id);
                ImGui::TableSetColumnIndex(3);  ImGui::Text("%u", e->price);
                ImGui::TableSetColumnIndex(4);  ImGui::Text("%u", e->category);
                ImGui::TableSetColumnIndex(5);  ImGui::Text("%u", e->sort_id);
                ImGui::TableSetColumnIndex(6);  ImGui::TextUnformatted(e->text_key);
                ImGui::TableSetColumnIndex(7);  ImGui::TextUnformatted(e->texture_1);
                ImGui::TableSetColumnIndex(8);  ImGui::TextUnformatted(e->texture_2);
                ImGui::TableSetColumnIndex(9);  ImGui::TextUnformatted(e->texture_3);
                ImGui::TableSetColumnIndex(10); ImGui::TextUnformatted(e->flag_9 ? "true" : "false");
                ImGui::TableSetColumnIndex(11); ImGui::Text("%u", e->hash_10);
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
        break;
    }
    // -------------------------------------------------------------------------
    case BinType::CustomizeItemException:
    {
        struct Item { int si; const CustomizeItemExceptionEntry* e; };
        std::vector<Item> items;
        for (int si = 0; si < (int)sources.size(); ++si)
            for (auto& e : sources[si].bin->exceptionEntries)
                items.push_back({si, &e});

        RenderSectionHeader("customize_item_exception", (int)items.size());

        // Build item_id → label map from all sources' common+unique entries
        std::unordered_map<uint32_t, std::string> idLabelMap;
        for (int si = 0; si < (int)sources.size(); ++si)
        {
            if (!sources[si].modData) continue;
            for (const auto& bin2 : sources[si].modData->contents)
            {
                for (const auto& ce : bin2.commonEntries)
                {
                    if (idLabelMap.count(ce.item_id)) continue;
                    char buf[320];
                    if (ce.item_code[0])
                        snprintf(buf, sizeof(buf), "%s (%u)", ce.item_code, ce.item_id);
                    else
                        snprintf(buf, sizeof(buf), "%u", ce.item_id);
                    idLabelMap[ce.item_id] = buf;
                }
                for (const auto& ue : bin2.customizeItemUniqueEntries)
                {
                    if (idLabelMap.count(ue.char_item_id)) continue;
                    char buf[320];
                    if (ue.asset_name[0])
                        snprintf(buf, sizeof(buf), "%s (%u)", ue.asset_name, ue.char_item_id);
                    else
                        snprintf(buf, sizeof(buf), "%u", ue.char_item_id);
                    idLabelMap[ue.char_item_id] = buf;
                }
            }
        }

        constexpr ImGuiTableFlags kExTF =
            ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersInnerH |
            ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY |
            ImGuiTableFlags_SizingStretchProp;

        if (ImGui::BeginTable("##ov_except", 3, kExTF, ImGui::GetContentRegionAvail()))
        {
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableSetupColumn("tkmod",          ImGuiTableColumnFlags_WidthFixed,   kTkmodW);
            ImGui::TableSetupColumn("Item ID",        ImGuiTableColumnFlags_WidthStretch, 3.0f);
            ImGui::TableSetupColumn("Exception Type", ImGuiTableColumnFlags_WidthFixed,   120.0f);
            ImGui::TableHeadersRow();

            for (int i = 0; i < (int)items.size(); ++i)
            {
                const int si = items[i].si; const auto* e = items[i].e;
                ImGui::TableNextRow();
                ImGui::PushID(i);
                RenderTkmodCell(si);

                // Item ID — show "AssetName (id)" label if known
                ImGui::TableSetColumnIndex(1);
                {
                    auto it = idLabelMap.find(e->item_id);
                    if (it != idLabelMap.end())
                        ImGui::TextUnformatted(it->second.c_str());
                    else
                        ImGui::Text("%u", e->item_id);
                }

                // Exception Type — show name from k_ExcTypes
                ImGui::TableSetColumnIndex(2);
                {
                    const char* typeName = "Unknown";
                    for (const auto& t : k_ExcTypes)
                        if (t.value == e->exception_type) { typeName = t.name; break; }
                    ImGui::TextUnformatted(typeName);
                }

                ImGui::PopID();
            }
            ImGui::EndTable();
        }
        break;
    }
    // -------------------------------------------------------------------------
    case BinType::CustomizeItemExclusiveList:
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.65f, 0.82f, 1.00f, 1.00f));
        ImGui::TextUnformatted("customize_item_exclusive_list.bin");
        ImGui::PopStyleColor();
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.42f, 0.42f, 0.54f, 1.00f));
        ImGui::Text("(%d tkmods)", (int)sources.size());
        ImGui::PopStyleColor();
        ImGui::Separator();

        // Helper for rule-type sub-tables (4 columns)
        auto RenderExclusiveRuleTable = [&](const char* label, const char* tableId,
            const wchar_t* tsvName,
            std::vector<std::pair<int, const CustomizeExclusiveRuleEntry*>>& items,
            const char* const* hdrs)
        {
            ImGui::TextDisabled("%s", label);
            ImGui::SameLine();
            ImGui::PushID(tableId);
            if (ImGui::SmallButton("Export"))
            {
                std::string path = OpenTsvSaveDialog(tsvName);
                if (!path.empty())
                {
                    FILE* fout = nullptr;
                    fopen_s(&fout, path.c_str(), "wb");
                    if (fout)
                    {
                        for (const auto& it : items)
                            fprintf(fout, "%s\t%u\t%u\t%u\t%u\n",
                                sources[it.first].filename,
                                it.second->item_id, it.second->hash,
                                it.second->link_type, it.second->ref_item_id);
                        fclose(fout);
                    }
                }
            }
            ImGui::PopID();
            if (ImGui::BeginTable(tableId, 1 + 4, kOvTF, ImVec2(0.0f, 0.0f)))
            {
                ImGui::TableSetupScrollFreeze(1, 1);
                ImGui::TableSetupColumn("tkmod", ImGuiTableColumnFlags_WidthFixed, kTkmodW);
                for (int c = 0; c < 4; ++c)
                    ImGui::TableSetupColumn(hdrs[c], ImGuiTableColumnFlags_WidthFixed, ColumnWidths::kExclusiveRule[c]);
                ImGui::TableHeadersRow();

                for (int i = 0; i < (int)items.size(); ++i)
                {
                    const int si = items[i].first; const auto* e = items[i].second;
                    ImGui::TableNextRow();
                    ImGui::PushID(i);
                    RenderTkmodCell(si);
                    ImGui::TableSetColumnIndex(1); ImGui::Text("%u", e->item_id);
                    ImGui::TableSetColumnIndex(2); ImGui::Text("%u", e->hash);
                    ImGui::TableSetColumnIndex(3); ImGui::Text("%u", e->link_type);
                    ImGui::TableSetColumnIndex(4); ImGui::Text("%u", e->ref_item_id);
                    ImGui::PopID();
                }
                ImGui::EndTable();
            }
        };

        auto RenderExclusivePairTable = [&](const char* label, const char* tableId,
            const wchar_t* tsvName,
            std::vector<std::pair<int, const CustomizeExclusivePairEntry*>>& items,
            const char* const* hdrs)
        {
            ImGui::TextDisabled("%s", label);
            ImGui::SameLine();
            ImGui::PushID(tableId);
            if (ImGui::SmallButton("Export"))
            {
                std::string path = OpenTsvSaveDialog(tsvName);
                if (!path.empty())
                {
                    FILE* fout = nullptr;
                    fopen_s(&fout, path.c_str(), "wb");
                    if (fout)
                    {
                        for (const auto& it : items)
                            fprintf(fout, "%s\t%u\t%u\t%u\n",
                                sources[it.first].filename,
                                it.second->item_id_a, it.second->item_id_b,
                                it.second->flag);
                        fclose(fout);
                    }
                }
            }
            ImGui::PopID();
            if (ImGui::BeginTable(tableId, 1 + 3, kOvTF, ImVec2(0.0f, 0.0f)))
            {
                ImGui::TableSetupScrollFreeze(1, 1);
                ImGui::TableSetupColumn("tkmod", ImGuiTableColumnFlags_WidthFixed, kTkmodW);
                for (int c = 0; c < 3; ++c)
                    ImGui::TableSetupColumn(hdrs[c], ImGuiTableColumnFlags_WidthFixed, ColumnWidths::kExclusivePair[c]);
                ImGui::TableHeadersRow();

                for (int i = 0; i < (int)items.size(); ++i)
                {
                    const int si = items[i].first; const auto* e = items[i].second;
                    ImGui::TableNextRow();
                    ImGui::PushID(i);
                    RenderTkmodCell(si);
                    ImGui::TableSetColumnIndex(1); ImGui::Text("%u", e->item_id_a);
                    ImGui::TableSetColumnIndex(2); ImGui::Text("%u", e->item_id_b);
                    ImGui::TableSetColumnIndex(3); ImGui::Text("%u", e->flag);
                    ImGui::PopID();
                }
                ImGui::EndTable();
            }
        };

        // rule_entries
        {
            std::vector<std::pair<int, const CustomizeExclusiveRuleEntry*>> items;
            for (int si = 0; si < (int)sources.size(); ++si)
                for (auto& e : sources[si].bin->exclusiveRuleEntries)
                    items.push_back({si, &e});
            RenderExclusiveRuleTable("rule_entries", "##ov_exrule", L"rule_entries.tsv", items, FieldNames::ExclusiveRule);
        }

        ImGui::Spacing(); ImGui::Spacing();

        // pair_entries
        {
            std::vector<std::pair<int, const CustomizeExclusivePairEntry*>> items;
            for (int si = 0; si < (int)sources.size(); ++si)
                for (auto& e : sources[si].bin->exclusivePairEntries)
                    items.push_back({si, &e});
            RenderExclusivePairTable("pair_entries", "##ov_expair", L"pair_entries.tsv", items, FieldNames::ExclusivePair);
        }

        ImGui::Spacing(); ImGui::Spacing();

        // group_rule_entries
        {
            std::vector<std::pair<int, const CustomizeExclusiveRuleEntry*>> items;
            for (int si = 0; si < (int)sources.size(); ++si)
                for (auto& e : sources[si].bin->exclusiveGroupRuleEntries)
                    items.push_back({si, &e});
            RenderExclusiveRuleTable("group_rule_entries", "##ov_exgrule", L"group_rule_entries.tsv", items, FieldNames::ExclusiveGroupRule);
        }

        ImGui::Spacing(); ImGui::Spacing();

        // group_pair_entries
        {
            std::vector<std::pair<int, const CustomizeExclusivePairEntry*>> items;
            for (int si = 0; si < (int)sources.size(); ++si)
                for (auto& e : sources[si].bin->exclusiveGroupPairEntries)
                    items.push_back({si, &e});
            RenderExclusivePairTable("group_pair_entries", "##ov_exgpair", L"group_pair_entries.tsv", items, FieldNames::ExclusiveGroupPair);
        }

        ImGui::Spacing(); ImGui::Spacing();

        // set_rule_entries
        {
            std::vector<std::pair<int, const CustomizeExclusiveRuleEntry*>> items;
            for (int si = 0; si < (int)sources.size(); ++si)
                for (auto& e : sources[si].bin->exclusiveSetRuleEntries)
                    items.push_back({si, &e});
            RenderExclusiveRuleTable("set_rule_entries", "##ov_exsetrule", L"set_rule_entries.tsv", items, FieldNames::ExclusiveSetRule);
        }
        break;
    }
    // -------------------------------------------------------------------------
    default:
        ImGui::TextDisabled("No merged overview available for this bin type.");
        break;
    }
}

// -----------------------------------------------------------------------------
//  Read-only bin preview (used by TkmodManagerView)
// -----------------------------------------------------------------------------

void FbsDataView::RenderBinReadOnly(ContentsBinData& bin)
{
    const bool savedRO = m_renderReadOnly;
    m_renderReadOnly = true;
    ImGui::SetCursorPos(ImVec2(10.0f, 6.0f));
    switch (bin.type)
    {
    case BinType::CustomizeItemCommonList:
        RenderCustomizeItemCommonEditor(bin);
        break;
    case BinType::CharacterList:
        RenderCharacterListEditor(bin);
        break;
    case BinType::CustomizeItemExclusiveList:
        RenderCustomizeItemExclusiveListEditor(bin);
        break;
    case BinType::AreaList:
        RenderAreaListEditor(bin);
        break;
    case BinType::BattleSubtitleInfoList:
        RenderBattleSubtitleInfoEditor(bin);
        break;
    case BinType::FateDramaPlayerStartList:
        RenderFateDramaPlayerStartListEditor(bin);
        break;
    case BinType::JukeboxList:
        RenderJukeboxListEditor(bin);
        break;
    case BinType::SeriesList:
        RenderSeriesListEditor(bin);
        break;
    case BinType::TamMissionList:
        RenderTamMissionListEditor(bin);
        break;
    case BinType::DramaPlayerStartList:
        RenderDramaPlayerStartListEditor(bin);
        break;
    case BinType::StageList:
        RenderStageListEditor(bin);
        break;
    case BinType::BallPropertyList:
        RenderBallPropertyListEditor(bin);
        break;
    case BinType::BodyCylinderDataList:
        RenderBodyCylinderDataListEditor(bin);
        break;
    case BinType::CustomizeItemUniqueList:
        RenderCustomizeItemUniqueListEditor(bin);
        break;
    case BinType::CharacterSelectList:
        RenderCharacterSelectListEditor(bin);
        break;
    case BinType::CustomizeItemProhibitDramaList:
        RenderCustomizeItemProhibitDramaListEditor(bin);
        break;
    case BinType::BattleMotionList:
        RenderBattleMotionListEditor(bin);
        break;
    case BinType::ArcadeCpuList:
        RenderArcadeCpuListEditor(bin);
        break;
    case BinType::BallRecommendList:
        RenderBallRecommendListEditor(bin);
        break;
    case BinType::BallSettingList:
        RenderBallSettingListEditor(bin);
        break;
    case BinType::BattleCommonList:
        RenderBattleCommonListEditor(bin);
        break;
    case BinType::BattleCpuList:
        RenderBattleCpuListEditor(bin);
        break;
    case BinType::RankList:
        RenderRankListEditor(bin);
        break;
    case BinType::AssistInputList:
        RenderAssistInputListEditor(bin);
        break;
    case BinType::CustomizePanelList:
        RenderCustomizePanelListEditor(bin);
        break;
    case BinType::CustomizeItemException:
        RenderCustomizeItemExceptionEditor(bin);
        break;
    default:
        ImGui::TextDisabled("No editor available for this bin type.");
        break;
    }
    m_renderReadOnly = savedRO;
}

// -----------------------------------------------------------------------------
//  Save confirmation popup (shown when mod info is empty)
// -----------------------------------------------------------------------------

void FbsDataView::RenderSaveConfirmPopup()
{
    if (m_saveConfirmPending) {
        ImGui::OpenPopup("##save_confirm");
        m_saveConfirmPending = false;
    }
    if (!ImGui::BeginPopupModal("##save_confirm", nullptr,
                                ImGuiWindowFlags_AlwaysAutoResize)) return;

    ImGui::TextUnformatted("Mod info is not filled in. Save anyway?");
    ImGui::Spacing();

    if (ImGui::Button("Confirm", ImVec2(100.0f, 0.f))) {
        ImGui::CloseCurrentPopup();
        if (m_pendingDoSaveAs)
            DoSaveAs();
        else
            DoSave();
    }
    ImGui::SameLine(0, 8.0f);
    if (ImGui::Button("Cancel", ImVec2(100.0f, 0.f)))
        ImGui::CloseCurrentPopup();

    ImGui::EndPopup();
}
