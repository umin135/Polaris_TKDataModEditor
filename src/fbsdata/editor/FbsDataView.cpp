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

void FbsDataView::DoSave()
{
    for (auto& bin : m_data.contents)
    {
        FixCommonItemIds(bin.commonEntries);
        FixUniqueItemIds(bin.customizeItemUniqueEntries);
    }
    m_lastSaveOk     = TkmodIO::SaveDialog(m_data);
    m_showSaveResult = true;
    m_statusTimer    = 3.0f;
}

void FbsDataView::RenderToolbar()
{
    ImGui::SetCursorPos(ImVec2(10.0f, 8.0f));

    if (ImGui::Button("  New  "))
    {
        m_data      = ModData{};
        m_modActive = true;
    }
    ImGui::SameLine(0, 6.0f);

    if (!m_modActive) ImGui::BeginDisabled();
    if (ImGui::Button("  Save  "))
    {
        bool infoEmpty = (m_data.info.author[0]      == '\0' &&
                          m_data.info.description[0]  == '\0' &&
                          m_data.info.version[0]      == '\0');
        if (infoEmpty)
            m_saveConfirmPending = true;
        else
            DoSave();
    }
    if (!m_modActive) ImGui::EndDisabled();
    ImGui::SameLine(0, 6.0f);

    if (ImGui::Button("  Load  "))
    {
        ModData loaded;
        if (TkmodIO::LoadDialog(loaded))
        {
            m_data      = std::move(loaded);
            m_modActive = true;
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

    // Right-aligned: Information Edit button (only when a mod is active)
    if (m_modActive)
    {
        const float infoBtnW = 160.0f;
        const float rightX   = ImGui::GetWindowWidth() - infoBtnW - 10.0f;
        if (rightX > ImGui::GetCursorPosX())
        {
            ImGui::SameLine(rightX);
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
    m_data      = std::move(loaded);
    m_modActive = true;
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
    if (ImGui::Button("Import", ImVec2(importBtnW, 0)))
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
    if (ImGui::Button("+ Add Entry", ImVec2(addBtnW, 0)))
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
        if (ImGui::SmallButton("X")) deleteIdx = i;
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

    if (deleteIdx >= 0)
        bin.commonEntries.erase(bin.commonEntries.begin() + deleteIdx);

    ImGui::EndTable();
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

    // -- Add Entry button (right-aligned) --
    const float addBtnW = 100.0f;
    ImGui::SameLine(ImGui::GetContentRegionAvail().x - addBtnW + ImGui::GetCursorPosX());
    if (ImGui::Button("+ Add Entry", ImVec2(addBtnW, 0)))
        bin.characterEntries.push_back(CharacterEntry{});

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
        if (ImGui::SmallButton("X")) deleteIdx = i;
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

    if (deleteIdx >= 0)
        bin.characterEntries.erase(bin.characterEntries.begin() + deleteIdx);

    ImGui::EndTable();
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

    // Helper: render a RuleEntry table (item_id, hash, link_type, ref_item_id)
    auto RenderRuleTable = [&](const char* tableId, std::vector<CustomizeExclusiveRuleEntry>& entries) {
        const float addBtnW = 100.0f;
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.42f, 0.42f, 0.54f, 1.00f));
        ImGui::Text("(%d entries)", (int)entries.size());
        ImGui::PopStyleColor();
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - addBtnW + ImGui::GetCursorPosX());
        ImGui::PushID(tableId);
        if (ImGui::Button("+ Add Entry", ImVec2(addBtnW, 0)))
            entries.push_back(CustomizeExclusiveRuleEntry{});
        ImGui::PopID();

        if (!ImGui::BeginTable(tableId, 5, tFlags, ImGui::GetContentRegionAvail()))
            return;

        ImGui::TableSetupScrollFreeze(1, 1);
        ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, ColumnWidths::kRowCtrl);
        for (int fi = 0; fi < FieldNames::ExclusiveRuleCount; ++fi)
            ImGui::TableSetupColumn(FieldNames::ExclusiveRule[fi],
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
            if (ImGui::SmallButton("X")) deleteIdx = i;
            ImGui::PopStyleColor(3);

            auto U32Cell = [](const char* id, uint32_t& v) {
                ImGui::SetNextItemWidth(-FLT_MIN);
                ImGui::InputScalar(id, ImGuiDataType_U32, &v);
            };
            ImGui::TableSetColumnIndex(1); U32Cell("##iid",  e.item_id);
            ImGui::TableSetColumnIndex(2); U32Cell("##hash", e.hash);
            ImGui::TableSetColumnIndex(3); U32Cell("##lt",   e.link_type);
            ImGui::TableSetColumnIndex(4); U32Cell("##rid",  e.ref_item_id);

            ImGui::PopID();
        }
        if (deleteIdx >= 0)
            entries.erase(entries.begin() + deleteIdx);

        ImGui::EndTable();
    };

    // Helper: render a PairEntry table (item_id_a, item_id_b, flag)
    auto RenderPairTable = [&](const char* tableId, std::vector<CustomizeExclusivePairEntry>& entries) {
        const float addBtnW = 100.0f;
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.42f, 0.42f, 0.54f, 1.00f));
        ImGui::Text("(%d entries)", (int)entries.size());
        ImGui::PopStyleColor();
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - addBtnW + ImGui::GetCursorPosX());
        ImGui::PushID(tableId);
        if (ImGui::Button("+ Add Entry", ImVec2(addBtnW, 0)))
            entries.push_back(CustomizeExclusivePairEntry{});
        ImGui::PopID();

        if (!ImGui::BeginTable(tableId, 4, tFlags, ImGui::GetContentRegionAvail()))
            return;

        ImGui::TableSetupScrollFreeze(1, 1);
        ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, ColumnWidths::kRowCtrl);
        for (int fi = 0; fi < FieldNames::ExclusivePairCount; ++fi)
            ImGui::TableSetupColumn(FieldNames::ExclusivePair[fi],
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
            if (ImGui::SmallButton("X")) deleteIdx = i;
            ImGui::PopStyleColor(3);

            auto U32Cell = [](const char* id, uint32_t& v) {
                ImGui::SetNextItemWidth(-FLT_MIN);
                ImGui::InputScalar(id, ImGuiDataType_U32, &v);
            };
            ImGui::TableSetColumnIndex(1); U32Cell("##ia",   e.item_id_a);
            ImGui::TableSetColumnIndex(2); U32Cell("##ib",   e.item_id_b);
            ImGui::TableSetColumnIndex(3); U32Cell("##flag", e.flag);

            ImGui::PopID();
        }
        if (deleteIdx >= 0)
            entries.erase(entries.begin() + deleteIdx);

        ImGui::EndTable();
    };

    if (ImGui::BeginTabBar("##ExclTabs"))
    {
        if (ImGui::BeginTabItem(FieldNames::ExclusiveArrays[0]))
        {
            RenderRuleTable("##RuleTable", bin.exclusiveRuleEntries);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem(FieldNames::ExclusiveArrays[1]))
        {
            RenderPairTable("##PairTable", bin.exclusivePairEntries);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem(FieldNames::ExclusiveArrays[2]))
        {
            RenderRuleTable("##GrpRuleTable", bin.exclusiveGroupRuleEntries);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem(FieldNames::ExclusiveArrays[3]))
        {
            RenderPairTable("##GrpPairTable", bin.exclusiveGroupPairEntries);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem(FieldNames::ExclusiveArrays[4]))
        {
            RenderRuleTable("##SetRuleTable", bin.exclusiveSetRuleEntries);
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
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

    ImGui::EndPopup();
}

// -----------------------------------------------------------------------------
//  area_list editor
// -----------------------------------------------------------------------------

void FbsDataView::RenderAreaListEditor(ContentsBinData& bin)
{
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.65f, 0.82f, 1.00f, 1.00f));
    ImGui::Text("area_list.bin");
    ImGui::PopStyleColor();
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.42f, 0.42f, 0.54f, 1.00f));
    ImGui::Text("(%d entries)", (int)bin.areaEntries.size());
    ImGui::PopStyleColor();

    const float addBtnW = 100.0f;
    ImGui::SameLine(ImGui::GetContentRegionAvail().x - addBtnW + ImGui::GetCursorPosX());
    if (ImGui::Button("+ Add Entry", ImVec2(addBtnW, 0)))
        bin.areaEntries.push_back(AreaEntry{});

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
            if (ImGui::SmallButton("X")) deleteIdx = i;
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
    if (deleteIdx >= 0)
        bin.areaEntries.erase(bin.areaEntries.begin() + deleteIdx);

    ImGui::EndTable();
}

// -----------------------------------------------------------------------------
//  battle_subtitle_info editor
// -----------------------------------------------------------------------------

void FbsDataView::RenderBattleSubtitleInfoEditor(ContentsBinData& bin)
{
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.65f, 0.82f, 1.00f, 1.00f));
    ImGui::Text("battle_subtitle_info.bin");
    ImGui::PopStyleColor();
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.42f, 0.42f, 0.54f, 1.00f));
    ImGui::Text("(%d entries)", (int)bin.battleSubtitleEntries.size());
    ImGui::PopStyleColor();

    const float addBtnW = 100.0f;
    ImGui::SameLine(ImGui::GetContentRegionAvail().x - addBtnW + ImGui::GetCursorPosX());
    if (ImGui::Button("+ Add Entry", ImVec2(addBtnW, 0)))
        bin.battleSubtitleEntries.push_back(BattleSubtitleInfoEntry{});

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
            if (ImGui::SmallButton("X")) deleteIdx = i;
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
    if (deleteIdx >= 0)
        bin.battleSubtitleEntries.erase(bin.battleSubtitleEntries.begin() + deleteIdx);

    ImGui::EndTable();
}

// -----------------------------------------------------------------------------
//  fate_drama_player_start_list editor
// -----------------------------------------------------------------------------

void FbsDataView::RenderFateDramaPlayerStartListEditor(ContentsBinData& bin)
{
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.65f, 0.82f, 1.00f, 1.00f));
    ImGui::Text("fate_drama_player_start_list.bin");
    ImGui::PopStyleColor();
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.42f, 0.42f, 0.54f, 1.00f));
    ImGui::Text("(%d entries)", (int)bin.fateDramaPlayerStartEntries.size());
    ImGui::PopStyleColor();

    const float addBtnW = 100.0f;
    ImGui::SameLine(ImGui::GetContentRegionAvail().x - addBtnW + ImGui::GetCursorPosX());
    if (ImGui::Button("+ Add Entry", ImVec2(addBtnW, 0)))
        bin.fateDramaPlayerStartEntries.push_back(FateDramaPlayerStartEntry{});

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
            if (ImGui::SmallButton("X")) deleteIdx = i;
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
    if (deleteIdx >= 0)
        bin.fateDramaPlayerStartEntries.erase(bin.fateDramaPlayerStartEntries.begin() + deleteIdx);

    ImGui::EndTable();
}

// -----------------------------------------------------------------------------
//  jukebox_list editor
// -----------------------------------------------------------------------------

void FbsDataView::RenderJukeboxListEditor(ContentsBinData& bin)
{
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.65f, 0.82f, 1.00f, 1.00f));
    ImGui::Text("jukebox_list.bin");
    ImGui::PopStyleColor();
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.42f, 0.42f, 0.54f, 1.00f));
    ImGui::Text("(%d entries)", (int)bin.jukeboxEntries.size());
    ImGui::PopStyleColor();

    const float addBtnW = 100.0f;
    ImGui::SameLine(ImGui::GetContentRegionAvail().x - addBtnW + ImGui::GetCursorPosX());
    if (ImGui::Button("+ Add Entry", ImVec2(addBtnW, 0)))
        bin.jukeboxEntries.push_back(JukeboxEntry{});

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
            if (ImGui::SmallButton("X")) deleteIdx = i;
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
    if (deleteIdx >= 0)
        bin.jukeboxEntries.erase(bin.jukeboxEntries.begin() + deleteIdx);

    ImGui::EndTable();
}

// -----------------------------------------------------------------------------
//  series_list editor
// -----------------------------------------------------------------------------

void FbsDataView::RenderSeriesListEditor(ContentsBinData& bin)
{
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.65f, 0.82f, 1.00f, 1.00f));
    ImGui::Text("series_list.bin");
    ImGui::PopStyleColor();
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.42f, 0.42f, 0.54f, 1.00f));
    ImGui::Text("(%d entries)", (int)bin.seriesEntries.size());
    ImGui::PopStyleColor();

    const float addBtnW = 100.0f;
    ImGui::SameLine(ImGui::GetContentRegionAvail().x - addBtnW + ImGui::GetCursorPosX());
    if (ImGui::Button("+ Add Entry", ImVec2(addBtnW, 0)))
        bin.seriesEntries.push_back(SeriesEntry{});

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
            if (ImGui::SmallButton("X")) deleteIdx = i;
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
    if (deleteIdx >= 0)
        bin.seriesEntries.erase(bin.seriesEntries.begin() + deleteIdx);

    ImGui::EndTable();
}

// -----------------------------------------------------------------------------
//  tam_mission_list editor
// -----------------------------------------------------------------------------

void FbsDataView::RenderTamMissionListEditor(ContentsBinData& bin)
{
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.65f, 0.82f, 1.00f, 1.00f));
    ImGui::Text("tam_mission_list.bin");
    ImGui::PopStyleColor();
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.42f, 0.42f, 0.54f, 1.00f));
    ImGui::Text("(%d entries)", (int)bin.tamMissionEntries.size());
    ImGui::PopStyleColor();

    const float addBtnW = 100.0f;
    ImGui::SameLine(ImGui::GetContentRegionAvail().x - addBtnW + ImGui::GetCursorPosX());
    if (ImGui::Button("+ Add Entry", ImVec2(addBtnW, 0)))
        bin.tamMissionEntries.push_back(TamMissionEntry{});

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
            if (ImGui::SmallButton("X")) deleteIdx = i;
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
    if (deleteIdx >= 0)
        bin.tamMissionEntries.erase(bin.tamMissionEntries.begin() + deleteIdx);

    ImGui::EndTable();
}

// -----------------------------------------------------------------------------
//  drama_player_start_list editor
// -----------------------------------------------------------------------------

void FbsDataView::RenderDramaPlayerStartListEditor(ContentsBinData& bin)
{
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.65f, 0.82f, 1.00f, 1.00f));
    ImGui::Text("drama_player_start_list.bin");
    ImGui::PopStyleColor();
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.42f, 0.42f, 0.54f, 1.00f));
    ImGui::Text("(%d entries)", (int)bin.dramaPlayerStartEntries.size());
    ImGui::PopStyleColor();

    const float addBtnW = 100.0f;
    ImGui::SameLine(ImGui::GetContentRegionAvail().x - addBtnW + ImGui::GetCursorPosX());
    if (ImGui::Button("+ Add Entry", ImVec2(addBtnW, 0)))
        bin.dramaPlayerStartEntries.push_back(DramaPlayerStartEntry{});

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
            if (ImGui::SmallButton("X")) deleteIdx = i;
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
    if (deleteIdx >= 0)
        bin.dramaPlayerStartEntries.erase(bin.dramaPlayerStartEntries.begin() + deleteIdx);

    ImGui::EndTable();
}

// -----------------------------------------------------------------------------
//  stage_list editor
// -----------------------------------------------------------------------------

void FbsDataView::RenderStageListEditor(ContentsBinData& bin)
{
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.65f, 0.82f, 1.00f, 1.00f));
    ImGui::Text("stage_list.bin");
    ImGui::PopStyleColor();
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.42f, 0.42f, 0.54f, 1.00f));
    ImGui::Text("(%d entries)", (int)bin.stageEntries.size());
    ImGui::PopStyleColor();

    const float addBtnW = 100.0f;
    ImGui::SameLine(ImGui::GetContentRegionAvail().x - addBtnW + ImGui::GetCursorPosX());
    if (ImGui::Button("+ Add Entry", ImVec2(addBtnW, 0)))
        bin.stageEntries.push_back(StageEntry{});

    ImGui::Separator();

    constexpr ImGuiTableFlags tFlags =
        ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg |
        ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersInnerV |
        ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable |
        ImGuiTableFlags_Hideable | ImGuiTableFlags_SizingFixedFit;

    // cols: #, stage_code, stage_hash, is_selectable, camera_offset, parent_stage_index,
    //       is_online_enabled, is_ranked_enabled, arena_width, arena_depth,
    //       group_id, stage_name_key, stage_mode, is_default_variant
    if (!ImGui::BeginTable("##StageTable", 14, tFlags, ImGui::GetContentRegionAvail()))
        return;

    ImGui::TableSetupScrollFreeze(1, 1);
    static const int k_StageIds[] = { 0, 1, 2, 3, 4, 17, 18, 21, 22, 28, 29, 34, 36 };
    ImGui::TableSetupColumn("#",                          ImGuiTableColumnFlags_WidthFixed, ColumnWidths::kRowCtrl);
    for (int ci = 0; ci < 13; ++ci)
        ImGui::TableSetupColumn(FieldNames::StageEntry[k_StageIds[ci]], ImGuiTableColumnFlags_WidthFixed, ColumnWidths::kStage[ci]);
    ImGui::TableHeadersRow();

    int deleteIdx = -1;
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

            ImGui::TableSetColumnIndex(1);  StrCell("##sc",  e.stage_code,         sizeof(e.stage_code));
            ImGui::TableSetColumnIndex(2);  U32Cell("##sh",  e.stage_hash);
            ImGui::TableSetColumnIndex(3);  BoolCell("##isel", e.is_selectable);
            ImGui::TableSetColumnIndex(4);  F32Cell("##cam", e.camera_offset);
            ImGui::TableSetColumnIndex(5);  U32Cell("##psi", e.parent_stage_index);
            ImGui::TableSetColumnIndex(6);  BoolCell("##ion", e.is_online_enabled);
            ImGui::TableSetColumnIndex(7);  BoolCell("##irk", e.is_ranked_enabled);
            ImGui::TableSetColumnIndex(8);  U32Cell("##aw",  e.arena_width);
            ImGui::TableSetColumnIndex(9);  U32Cell("##ad",  e.arena_depth);
            ImGui::TableSetColumnIndex(10); StrCell("##gid", e.group_id,            sizeof(e.group_id));
            ImGui::TableSetColumnIndex(11); StrCell("##snk", e.stage_name_key,      sizeof(e.stage_name_key));
            ImGui::TableSetColumnIndex(12); U32Cell("##sm",  e.stage_mode);
            ImGui::TableSetColumnIndex(13); BoolCell("##idv", e.is_default_variant);

            ImGui::PopID();
        }
    }
    if (deleteIdx >= 0)
        bin.stageEntries.erase(bin.stageEntries.begin() + deleteIdx);

    ImGui::EndTable();
}

// -----------------------------------------------------------------------------
//  ball_property_list editor
// -----------------------------------------------------------------------------

void FbsDataView::RenderBallPropertyListEditor(ContentsBinData& bin)
{
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.65f, 0.82f, 1.00f, 1.00f));
    ImGui::Text("ball_property_list.bin");
    ImGui::PopStyleColor();
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.42f, 0.42f, 0.54f, 1.00f));
    ImGui::Text("(%d entries)", (int)bin.ballPropertyEntries.size());
    ImGui::PopStyleColor();

    const float addBtnW = 100.0f;
    ImGui::SameLine(ImGui::GetContentRegionAvail().x - addBtnW + ImGui::GetCursorPosX());
    if (ImGui::Button("+ Add Entry", ImVec2(addBtnW, 0)))
        bin.ballPropertyEntries.push_back(BallPropertyEntry{});

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
            if (ImGui::SmallButton("X")) deleteIdx = i;
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
    if (deleteIdx >= 0)
        bin.ballPropertyEntries.erase(bin.ballPropertyEntries.begin() + deleteIdx);

    ImGui::EndTable();
}

// -----------------------------------------------------------------------------
//  body_cylinder_data_list editor
// -----------------------------------------------------------------------------

void FbsDataView::RenderBodyCylinderDataListEditor(ContentsBinData& bin)
{
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.65f, 0.82f, 1.00f, 1.00f));
    ImGui::Text("body_cylinder_data_list.bin");
    ImGui::PopStyleColor();
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.42f, 0.42f, 0.54f, 1.00f));
    ImGui::Text("(%d entries)", (int)bin.bodyCylinderDataEntries.size());
    ImGui::PopStyleColor();

    const float addBtnW = 100.0f;
    ImGui::SameLine(ImGui::GetContentRegionAvail().x - addBtnW + ImGui::GetCursorPosX());
    if (ImGui::Button("+ Add Entry", ImVec2(addBtnW, 0)))
        bin.bodyCylinderDataEntries.push_back(BodyCylinderDataEntry{});

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
            if (ImGui::SmallButton("X")) deleteIdx = i;
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
    if (deleteIdx >= 0)
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
    if (ImGui::Button("Import##u", ImVec2(importBtnW, 0)))
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
    if (ImGui::Button("+ Add Entry##u", ImVec2(addBtnW, 0)))
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
            if (ImGui::SmallButton("X")) deleteIdx = i;
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
    if (deleteIdx >= 0)
        bin.customizeItemUniqueEntries.erase(bin.customizeItemUniqueEntries.begin() + deleteIdx);

    ImGui::EndTable();
}

// -----------------------------------------------------------------------------
//  character_select_list editor
// -----------------------------------------------------------------------------

void FbsDataView::RenderCharacterSelectListEditor(ContentsBinData& bin)
{
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.65f, 0.82f, 1.00f, 1.00f));
    ImGui::Text("character_select_list.bin");
    ImGui::PopStyleColor();
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
        if (ImGui::Button("+ Add Entry", ImVec2(addBtnW, 0)))
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
                    if (ImGui::SmallButton("X")) deleteIdx = i;
                    ImGui::PopStyleColor(3);

                    ImGui::TableSetColumnIndex(1);
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    ImGui::InputScalar("##ch", ImGuiDataType_U32, &e.character_hash);

                    ImGui::PopID();
                }
            }
            if (deleteIdx >= 0)
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
        if (ImGui::Button("+ Add Entry##prm", ImVec2(addBtnW, 0)))
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
                    if (ImGui::SmallButton("X")) deleteIdx = i;
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
            if (deleteIdx >= 0)
                bin.characterSelectParamEntries.erase(bin.characterSelectParamEntries.begin() + deleteIdx);

            ImGui::EndTable();
        }
        ImGui::EndTabItem();
    }

    ImGui::EndTabBar();
}

// -----------------------------------------------------------------------------
//  customize_item_prohibit_drama_list editor
// -----------------------------------------------------------------------------

void FbsDataView::RenderCustomizeItemProhibitDramaListEditor(ContentsBinData& bin)
{
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.65f, 0.82f, 1.00f, 1.00f));
    ImGui::Text("customize_item_prohibit_drama_list.bin");
    ImGui::PopStyleColor();
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
        if (ImGui::Button("+ Add Entry", ImVec2(addBtnW, 0)))
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
                if (ImGui::SmallButton("X")) deleteIdx = i;
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
        if (deleteIdx >= 0)
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
            if (ImGui::SmallButton("X")) deleteIdx = i;
            ImGui::PopStyleColor(3);
            ImGui::SameLine();
            ImGui::SetNextItemWidth(120.0f);
            ImGui::InputScalar("##cv", ImGuiDataType_U32, &bin.prohibitDramaCategoryValues[i]);
            ImGui::PopID();
        }
        if (deleteIdx >= 0)
            bin.prohibitDramaCategoryValues.erase(bin.prohibitDramaCategoryValues.begin() + deleteIdx);
        ImGui::EndChild();
        ImGui::EndTabItem();
    }

    ImGui::EndTabBar();
}

// -----------------------------------------------------------------------------
//  battle_motion_list editor
// -----------------------------------------------------------------------------

void FbsDataView::RenderBattleMotionListEditor(ContentsBinData& bin)
{
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.65f, 0.82f, 1.00f, 1.00f));
    ImGui::Text("battle_motion_list.bin");
    ImGui::PopStyleColor();
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
        if (ImGui::Button("+ Add Entry", ImVec2(addBtnW, 0)))
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
                if (ImGui::SmallButton("X")) deleteIdx = i;
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
        if (deleteIdx >= 0)
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
//  arcade_cpu_list editor
// -----------------------------------------------------------------------------

void FbsDataView::RenderArcadeCpuListEditor(ContentsBinData& bin)
{
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.65f, 0.82f, 1.00f, 1.00f));
    ImGui::Text("arcade_cpu_list.bin");
    ImGui::PopStyleColor();
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
        if (ImGui::Button("+ Add Entry", ImVec2(addBtnW, 0)))
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
                    if (ImGui::SmallButton("X")) deleteIdx = i;
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
            if (deleteIdx >= 0)
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
            if (ImGui::Button("+ Add Entry", ImVec2(addBtnW, 0)))
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
                        if (ImGui::SmallButton("X")) deleteIdx = i;
                        ImGui::PopStyleColor(3);

                        ImGui::TableSetColumnIndex(1);
                        ImGui::SetNextItemWidth(-FLT_MIN);
                        ImGui::InputScalar("##vh", ImGuiDataType_U32, &e.value_hash);

                        ImGui::PopID();
                    }
                }
                if (deleteIdx >= 0)
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
        if (ImGui::Button("+ Add Entry##rule", ImVec2(addBtnW, 0)))
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
                    if (ImGui::SmallButton("X")) deleteIdx = i;
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
            if (deleteIdx >= 0)
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

void FbsDataView::RenderBallRecommendListEditor(ContentsBinData& bin)
{
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.65f, 0.82f, 1.00f, 1.00f));
    ImGui::Text("ball_recommend_list.bin");
    ImGui::PopStyleColor();
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
            if (ImGui::Button("+ Add Entry", ImVec2(addBtnW, 0)))
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
                        if (ImGui::SmallButton("X")) deleteIdx = i;
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
                if (deleteIdx >= 0)
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
            if (ImGui::SmallButton("X")) deleteIdx = i;
            ImGui::PopStyleColor(3);
            ImGui::SameLine();
            ImGui::SetNextItemWidth(120.0f);
            ImGui::InputScalar("##uv", ImGuiDataType_U32, &bin.ballRecommendUnkValues[i]);
            ImGui::PopID();
        }
        if (deleteIdx >= 0)
            bin.ballRecommendUnkValues.erase(bin.ballRecommendUnkValues.begin() + deleteIdx);
        ImGui::EndChild();
        ImGui::EndTabItem();
    }

    ImGui::EndTabBar();
}

// -----------------------------------------------------------------------------
//  ball_setting_list editor (property grid)
// -----------------------------------------------------------------------------

void FbsDataView::RenderBallSettingListEditor(ContentsBinData& bin)
{
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.65f, 0.82f, 1.00f, 1.00f));
    ImGui::Text("ball_setting_list.bin");
    ImGui::PopStyleColor();
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

void FbsDataView::RenderBattleCommonListEditor(ContentsBinData& bin)
{
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.65f, 0.82f, 1.00f, 1.00f));
    ImGui::Text("battle_common_list.bin");
    ImGui::PopStyleColor();
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
        if (ImGui::Button("+ Add Entry", ImVec2(addBtnW, 0)))
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
                    if (ImGui::SmallButton("X")) deleteIdx = i;
                    ImGui::PopStyleColor(3);

                    ImGui::TableSetColumnIndex(1);
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    ImGui::InputScalar("##v", ImGuiDataType_U32, &e.value);

                    ImGui::PopID();
                }
            }
            if (deleteIdx >= 0)
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
        if (ImGui::Button("+ Add Entry##cs", ImVec2(addBtnW, 0)))
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
                    if (ImGui::SmallButton("X")) deleteIdx = i;
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
            if (deleteIdx >= 0)
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
        if (ImGui::Button("+ Add Entry##pe", ImVec2(addBtnW, 0)))
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
                    if (ImGui::SmallButton("X")) deleteIdx = i;
                    ImGui::PopStyleColor(3);

                    ImGui::TableSetColumnIndex(1); ImGui::SetNextItemWidth(-FLT_MIN); ImGui::InputScalar("##v0", ImGuiDataType_U32, &e.value_0);
                    ImGui::TableSetColumnIndex(2); ImGui::SetNextItemWidth(-FLT_MIN); ImGui::InputScalar("##v1", ImGuiDataType_U32, &e.value_1);
                    ImGui::TableSetColumnIndex(3); ImGui::SetNextItemWidth(-FLT_MIN); ImGui::InputScalar("##v2", ImGuiDataType_U32, &e.value_2);

                    ImGui::PopID();
                }
            }
            if (deleteIdx >= 0)
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
        if (ImGui::Button("+ Add Entry##me", ImVec2(addBtnW, 0)))
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
                    if (ImGui::SmallButton("X")) deleteIdx = i;
                    ImGui::PopStyleColor(3);

                    ImGui::TableSetColumnIndex(1); ImGui::SetNextItemWidth(-FLT_MIN); ImGui::InputScalar("##v0", ImGuiDataType_Float, &e.value_0);
                    ImGui::TableSetColumnIndex(2); ImGui::SetNextItemWidth(-FLT_MIN); ImGui::InputScalar("##v1", ImGuiDataType_Float, &e.value_1);
                    ImGui::TableSetColumnIndex(3); ImGui::SetNextItemWidth(-FLT_MIN); ImGui::InputScalar("##v2", ImGuiDataType_Float, &e.value_2);

                    ImGui::PopID();
                }
            }
            if (deleteIdx >= 0)
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

void FbsDataView::RenderBattleCpuListEditor(ContentsBinData& bin)
{
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.65f, 0.82f, 1.00f, 1.00f));
    ImGui::Text("battle_cpu_list.bin");
    ImGui::PopStyleColor();

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
        if (ImGui::Button("+ Add Entry", ImVec2(addBtnW, 0)))
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
                    if (ImGui::SmallButton("X")) deleteIdx = i;
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
            if (deleteIdx >= 0)
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
        if (ImGui::Button("+ Add Entry##step", ImVec2(addBtnW, 0)))
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
                    if (ImGui::SmallButton("X")) deleteIdx = i;
                    ImGui::PopStyleColor(3);

                    ImGui::TableSetColumnIndex(1); ImGui::SetNextItemWidth(-FLT_MIN); ImGui::InputScalar("##v0", ImGuiDataType_U32, &e.value_0);
                    ImGui::TableSetColumnIndex(2); ImGui::SetNextItemWidth(-FLT_MIN); ImGui::InputScalar("##v1", ImGuiDataType_U32, &e.value_1);
                    ImGui::TableSetColumnIndex(3); ImGui::SetNextItemWidth(-FLT_MIN); ImGui::InputScalar("##v2", ImGuiDataType_U32, &e.value_2);
                    ImGui::TableSetColumnIndex(4); ImGui::SetNextItemWidth(-FLT_MIN); ImGui::InputScalar("##v3", ImGuiDataType_U32, &e.value_3);

                    ImGui::PopID();
                }
            }
            if (deleteIdx >= 0)
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
            if (ImGui::SmallButton("X")) deleteIdx = i;
            ImGui::PopStyleColor(3);
            ImGui::SameLine();
            ImGui::SetNextItemWidth(120.0f);
            ImGui::InputScalar("##pv", ImGuiDataType_S32, &bin.battleCpuParamValues[i]);
            ImGui::PopID();
        }
        if (deleteIdx >= 0)
            bin.battleCpuParamValues.erase(bin.battleCpuParamValues.begin() + deleteIdx);
        ImGui::EndChild();
        ImGui::EndTabItem();
    }

    ImGui::EndTabBar();
}

// -----------------------------------------------------------------------------
//  rank_list editor
// -----------------------------------------------------------------------------

void FbsDataView::RenderRankListEditor(ContentsBinData& bin)
{
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.65f, 0.82f, 1.00f, 1.00f));
    ImGui::Text("rank_list.bin");
    ImGui::PopStyleColor();
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
                    if (ImGui::SmallButton("X")) deleteIdx = i;
                    ImGui::PopStyleColor(3);

                    ImGui::TableSetColumnIndex(1); ImGui::SetNextItemWidth(-FLT_MIN); ImGui::InputScalar("##h",  ImGuiDataType_U32, &e.hash);
                    ImGui::TableSetColumnIndex(2); ImGui::SetNextItemWidth(-FLT_MIN); ImGui::InputText("##tk",   e.text_key, sizeof(e.text_key));
                    ImGui::TableSetColumnIndex(3); ImGui::SetNextItemWidth(-FLT_MIN); ImGui::InputText("##nm",   e.name,     sizeof(e.name));
                    ImGui::TableSetColumnIndex(4); ImGui::SetNextItemWidth(-FLT_MIN); ImGui::InputScalar("##rk", ImGuiDataType_U32, &e.rank);

                    ImGui::PopID();
                }
            }
            if (deleteIdx >= 0)
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

void FbsDataView::RenderAssistInputListEditor(ContentsBinData& bin)
{
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.65f, 0.82f, 1.00f, 1.00f));
    ImGui::Text("assist_input_list.bin");
    ImGui::PopStyleColor();
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.42f, 0.42f, 0.54f, 1.00f));
    ImGui::Text("(%d entries)", (int)bin.assistInputEntries.size());
    ImGui::PopStyleColor();

    const float addBtnW = 100.0f;
    ImGui::SameLine(ImGui::GetContentRegionAvail().x - addBtnW + ImGui::GetCursorPosX());
    if (ImGui::Button("+ Add Entry", ImVec2(addBtnW, 0)))
        bin.assistInputEntries.push_back(AssistInputEntry{});

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
            if (ImGui::SmallButton("X")) deleteIdx = i;
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
    if (deleteIdx >= 0)
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
    if (ImGui::Button("Close"))
        ImGui::CloseCurrentPopup();

    ImGui::EndPopup();
}

// -----------------------------------------------------------------------------
//  customize_panel_list editor
// -----------------------------------------------------------------------------

void FbsDataView::RenderCustomizePanelListEditor(ContentsBinData& bin)
{
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.65f, 0.82f, 1.00f, 1.00f));
    ImGui::Text("customize_panel_list.bin");
    ImGui::PopStyleColor();
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.42f, 0.42f, 0.54f, 1.00f));
    ImGui::Text("(%d entries)", (int)bin.customizePanelEntries.size());
    ImGui::PopStyleColor();

    const float addBtnW = 100.0f;
    ImGui::SameLine(ImGui::GetContentRegionAvail().x - addBtnW + ImGui::GetCursorPosX());
    if (ImGui::Button("+ Add Entry", ImVec2(addBtnW, 0)))
        bin.customizePanelEntries.push_back(bin.customizePanelEntries.empty()
            ? CustomizePanelEntry{}
            : bin.customizePanelEntries.back());

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
        if (ImGui::SmallButton("X")) deleteIdx = i;
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

    if (deleteIdx >= 0)
        bin.customizePanelEntries.erase(bin.customizePanelEntries.begin() + deleteIdx);

    ImGui::EndTable();
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
        DoSave();
    }
    ImGui::SameLine(0, 8.0f);
    if (ImGui::Button("Cancel", ImVec2(100.0f, 0.f)))
        ImGui::CloseCurrentPopup();

    ImGui::EndPopup();
}
