// FbsData editor view implementation
#include "editors/FbsDataView.h"
#include "data/FieldNames.h"
#include "io/TkmodIO.h"
#include "imgui/imgui.h"
#include <cstring>
#include <cctype>

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
    { "customize_panel_list.bin",                BinType::None,                            false, "Not Supported" },
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

void FbsDataView::RenderToolbar()
{
    ImGui::SetCursorPos(ImVec2(10.0f, 8.0f));

    if (ImGui::Button("  Save  "))
    {
        m_lastSaveOk     = TkmodIO::SaveDialog(m_data);
        m_showSaveResult = true;
        m_statusTimer    = 3.0f;
    }
    ImGui::SameLine(0, 6.0f);

    if (ImGui::Button("  Load  "))
    {
        ModData loaded;
        if (TkmodIO::LoadDialog(loaded))
        {
            m_data = std::move(loaded);
        }
    }

    // Transient status message after save
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
}

// -----------------------------------------------------------------------------
//  Main render entry
// -----------------------------------------------------------------------------

bool FbsDataView::LoadFromPath(const std::string& path)
{
    ModData loaded;
    if (!TkmodIO::LoadFromPath(path, loaded))
        return false;
    m_data = std::move(loaded);
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
    default:
        ImGui::TextDisabled("No editor available for this bin type.");
        break;
    }
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

    // -- Add Entry button (right-aligned) --
    const float addBtnW = 100.0f;
    ImGui::SameLine(ImGui::GetContentRegionAvail().x - addBtnW + ImGui::GetCursorPosX());
    if (ImGui::Button("+ Add Entry", ImVec2(addBtnW, 0)))
        bin.commonEntries.push_back(CustomizeItemCommonEntry{});

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

    // Column count: 1 (row/delete) + 18 fields = 19
    if (!ImGui::BeginTable("##CICLTable", 19, tFlags,
                           ImGui::GetContentRegionAvail()))
        return;

    // Freeze first column (row controls) and header row
    ImGui::TableSetupScrollFreeze(1, 1);

    // Columns shown in the regular editor (subset of 26 -- unknown fields omitted)
    // Schema id -> display width
    static const struct { int id; float w; } k_Cols[] = {
        {  0, 95.0f }, {  1, 62.0f }, {  2, 195.0f },
        {  3, 95.0f }, {  4, 95.0f }, {  5, 215.0f },
        {  6, 115.0f }, {  7, 115.0f },
        {  9, 95.0f }, { 10, 78.0f }, { 12, 82.0f },
        { 14, 95.0f }, { 15, 95.0f }, { 16, 60.0f },
        { 18, 95.0f }, { 23, 95.0f }, { 24, 62.0f }, { 25, 82.0f },
    };
    constexpr int k_ColCount = (int)(sizeof(k_Cols) / sizeof(k_Cols[0]));
    ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, 52.0f);
    for (int ci = 0; ci < k_ColCount; ++ci)
        ImGui::TableSetupColumn(FieldNames::CommonItem[k_Cols[ci].id],
                                ImGuiTableColumnFlags_WidthFixed, k_Cols[ci].w);
    ImGui::TableHeadersRow();

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

        ImGui::TableSetColumnIndex(1);  U32Cell("##iid",   e.item_id);
        ImGui::TableSetColumnIndex(2);  I32Cell("##ino",   e.item_no);
        ImGui::TableSetColumnIndex(3);  StrCell("##icode", e.item_code,      sizeof(e.item_code));
        ImGui::TableSetColumnIndex(4);  U32Cell("##h0",    e.hash_0);
        ImGui::TableSetColumnIndex(5);  U32Cell("##h1",    e.hash_1);
        ImGui::TableSetColumnIndex(6);  StrCell("##tkey",  e.text_key,       sizeof(e.text_key));
        ImGui::TableSetColumnIndex(7);  StrCell("##pkid",  e.package_id,     sizeof(e.package_id));
        ImGui::TableSetColumnIndex(8);  StrCell("##pksu",  e.package_sub_id, sizeof(e.package_sub_id));
        ImGui::TableSetColumnIndex(9);  I32Cell("##ssid",  e.shop_sort_id);
        ImGui::TableSetColumnIndex(10); BoolCell("##enb",  e.is_enabled);
        ImGui::TableSetColumnIndex(11); I32Cell("##prc",   e.price);
        ImGui::TableSetColumnIndex(12); I32Cell("##cno",   e.category_no);
        ImGui::TableSetColumnIndex(13); U32Cell("##h2",    e.hash_2);
        ImGui::TableSetColumnIndex(14); BoolCell("##u16",  e.unk_16);
        ImGui::TableSetColumnIndex(15); U32Cell("##h3",    e.hash_3);
        ImGui::TableSetColumnIndex(16); U32Cell("##h4",    e.hash_4);
        ImGui::TableSetColumnIndex(17); I32Cell("##rar",   e.rarity);
        ImGui::TableSetColumnIndex(18); I32Cell("##sgrp",  e.sort_group);

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

    static const float k_CharWidths[15] = {
        130.0f,  // id  0
         95.0f,  // id  1
         78.0f,  // id  2
         88.0f,  // id  3
        100.0f,  // id  4
        105.0f,  // id  5
         78.0f,  // id  6
         82.0f,  // id  7
        170.0f,  // id  8
        150.0f,  // id  9
        150.0f,  // id 10
        150.0f,  // id 11
        155.0f,  // id 12
        130.0f,  // id 13
        130.0f,  // id 14
    };
    ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, 52.0f);
    for (int fi = 0; fi < FieldNames::CharacterCount; ++fi)
        ImGui::TableSetupColumn(FieldNames::Character[fi],
                                ImGuiTableColumnFlags_WidthFixed, k_CharWidths[fi]);
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
        static const float k_RuleW[4] = { 95.0f, 95.0f, 82.0f, 95.0f };
        ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, 52.0f);
        for (int fi = 0; fi < FieldNames::ExclusiveRuleCount; ++fi)
            ImGui::TableSetupColumn(FieldNames::ExclusiveRule[fi],
                                    ImGuiTableColumnFlags_WidthFixed, k_RuleW[fi]);
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
        static const float k_PairW[3] = { 95.0f, 95.0f, 82.0f };
        ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, 52.0f);
        for (int fi = 0; fi < FieldNames::ExclusivePairCount; ++fi)
            ImGui::TableSetupColumn(FieldNames::ExclusivePair[fi],
                                    ImGuiTableColumnFlags_WidthFixed, k_PairW[fi]);
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
                        bin.commonEntries.push_back(CustomizeItemCommonEntry{});
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
                        bin.customizeItemUniqueEntries.push_back(CustomizeItemUniqueEntry{});
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
    ImGui::TableSetupColumn("#",                            ImGuiTableColumnFlags_WidthFixed, 52.0f);
    ImGui::TableSetupColumn(FieldNames::AreaEntry[0],       ImGuiTableColumnFlags_WidthFixed, 95.0f);
    ImGui::TableSetupColumn(FieldNames::AreaEntry[1],       ImGuiTableColumnFlags_WidthFixed, 200.0f);
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
    ImGui::TableSetupColumn("#",                                 ImGuiTableColumnFlags_WidthFixed, 52.0f);
    ImGui::TableSetupColumn(FieldNames::BattleSubtitleInfo[0],   ImGuiTableColumnFlags_WidthFixed, 95.0f);
    ImGui::TableSetupColumn(FieldNames::BattleSubtitleInfo[1],   ImGuiTableColumnFlags_WidthFixed, 95.0f);
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
    ImGui::TableSetupColumn("#",                                   ImGuiTableColumnFlags_WidthFixed, 52.0f);
    ImGui::TableSetupColumn(FieldNames::FateDramaPlayerStart[0],   ImGuiTableColumnFlags_WidthFixed, 95.0f);
    ImGui::TableSetupColumn(FieldNames::FateDramaPlayerStart[1],   ImGuiTableColumnFlags_WidthFixed, 95.0f);
    ImGui::TableSetupColumn(FieldNames::FateDramaPlayerStart[2],   ImGuiTableColumnFlags_WidthFixed, 80.0f);
    ImGui::TableSetupColumn(FieldNames::FateDramaPlayerStart[3],   ImGuiTableColumnFlags_WidthFixed, 95.0f);
    ImGui::TableSetupColumn(FieldNames::FateDramaPlayerStart[4],   ImGuiTableColumnFlags_WidthFixed, 78.0f);
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

    // cols: #, bgm_hash, series_hash, cue_name, arrangement, display_text_key
    if (!ImGui::BeginTable("##JukeTable", 6, tFlags, ImGui::GetContentRegionAvail()))
        return;

    ImGui::TableSetupScrollFreeze(1, 1);
    ImGui::TableSetupColumn("#",                           ImGuiTableColumnFlags_WidthFixed, 52.0f);
    ImGui::TableSetupColumn(FieldNames::JukeboxEntry[0],   ImGuiTableColumnFlags_WidthFixed, 95.0f);
    ImGui::TableSetupColumn(FieldNames::JukeboxEntry[1],   ImGuiTableColumnFlags_WidthFixed, 95.0f);
    ImGui::TableSetupColumn(FieldNames::JukeboxEntry[3],   ImGuiTableColumnFlags_WidthFixed, 200.0f);
    ImGui::TableSetupColumn(FieldNames::JukeboxEntry[4],   ImGuiTableColumnFlags_WidthFixed, 200.0f);
    ImGui::TableSetupColumn(FieldNames::JukeboxEntry[8],   ImGuiTableColumnFlags_WidthFixed, 200.0f);
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

            ImGui::TableSetColumnIndex(1); U32Cell("##bh",  e.bgm_hash);
            ImGui::TableSetColumnIndex(2); U32Cell("##sh",  e.series_hash);
            ImGui::TableSetColumnIndex(3); StrCell("##cn",  e.cue_name,        sizeof(e.cue_name));
            ImGui::TableSetColumnIndex(4); StrCell("##arr", e.arrangement,     sizeof(e.arrangement));
            ImGui::TableSetColumnIndex(5); StrCell("##dtk", e.display_text_key,sizeof(e.display_text_key));

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
    ImGui::TableSetupColumn("#",                           ImGuiTableColumnFlags_WidthFixed, 52.0f);
    ImGui::TableSetupColumn(FieldNames::SeriesEntry[0],    ImGuiTableColumnFlags_WidthFixed, 95.0f);
    ImGui::TableSetupColumn(FieldNames::SeriesEntry[1],    ImGuiTableColumnFlags_WidthFixed, 180.0f);
    ImGui::TableSetupColumn(FieldNames::SeriesEntry[2],    ImGuiTableColumnFlags_WidthFixed, 180.0f);
    ImGui::TableSetupColumn(FieldNames::SeriesEntry[3],    ImGuiTableColumnFlags_WidthFixed, 180.0f);
    ImGui::TableSetupColumn(FieldNames::SeriesEntry[4],    ImGuiTableColumnFlags_WidthFixed, 180.0f);
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
    ImGui::TableSetupColumn("#",                              ImGuiTableColumnFlags_WidthFixed, 52.0f);
    ImGui::TableSetupColumn(FieldNames::TamMissionEntry[0],   ImGuiTableColumnFlags_WidthFixed, 95.0f);
    ImGui::TableSetupColumn(FieldNames::TamMissionEntry[2],   ImGuiTableColumnFlags_WidthFixed, 80.0f);
    ImGui::TableSetupColumn(FieldNames::TamMissionEntry[3],   ImGuiTableColumnFlags_WidthFixed, 200.0f);
    ImGui::TableSetupColumn(FieldNames::TamMissionEntry[4],   ImGuiTableColumnFlags_WidthFixed, 95.0f);
    ImGui::TableSetupColumn(FieldNames::TamMissionEntry[5],   ImGuiTableColumnFlags_WidthFixed, 95.0f);
    ImGui::TableSetupColumn(FieldNames::TamMissionEntry[6],   ImGuiTableColumnFlags_WidthFixed, 95.0f);
    ImGui::TableSetupColumn(FieldNames::TamMissionEntry[7],   ImGuiTableColumnFlags_WidthFixed, 95.0f);
    ImGui::TableSetupColumn(FieldNames::TamMissionEntry[8],   ImGuiTableColumnFlags_WidthFixed, 95.0f);
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
    ImGui::TableSetupColumn("#",                              ImGuiTableColumnFlags_WidthFixed, 52.0f);
    ImGui::TableSetupColumn(FieldNames::DramaPlayerStart[0],  ImGuiTableColumnFlags_WidthFixed, 95.0f);
    ImGui::TableSetupColumn(FieldNames::DramaPlayerStart[2],  ImGuiTableColumnFlags_WidthFixed, 80.0f);
    ImGui::TableSetupColumn(FieldNames::DramaPlayerStart[3],  ImGuiTableColumnFlags_WidthFixed, 95.0f);
    ImGui::TableSetupColumn(FieldNames::DramaPlayerStart[4],  ImGuiTableColumnFlags_WidthFixed, 95.0f);
    ImGui::TableSetupColumn(FieldNames::DramaPlayerStart[6],  ImGuiTableColumnFlags_WidthFixed, 80.0f);
    ImGui::TableSetupColumn(FieldNames::DramaPlayerStart[7],  ImGuiTableColumnFlags_WidthFixed, 80.0f);
    ImGui::TableSetupColumn(FieldNames::DramaPlayerStart[8],  ImGuiTableColumnFlags_WidthFixed, 95.0f);
    ImGui::TableSetupColumn(FieldNames::DramaPlayerStart[10], ImGuiTableColumnFlags_WidthFixed, 80.0f);
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
    ImGui::TableSetupColumn("#",                          ImGuiTableColumnFlags_WidthFixed, 52.0f);
    ImGui::TableSetupColumn(FieldNames::StageEntry[0],    ImGuiTableColumnFlags_WidthFixed, 140.0f);
    ImGui::TableSetupColumn(FieldNames::StageEntry[1],    ImGuiTableColumnFlags_WidthFixed, 95.0f);
    ImGui::TableSetupColumn(FieldNames::StageEntry[2],    ImGuiTableColumnFlags_WidthFixed, 82.0f);
    ImGui::TableSetupColumn(FieldNames::StageEntry[3],    ImGuiTableColumnFlags_WidthFixed, 95.0f);
    ImGui::TableSetupColumn(FieldNames::StageEntry[4],    ImGuiTableColumnFlags_WidthFixed, 80.0f);
    ImGui::TableSetupColumn(FieldNames::StageEntry[17],   ImGuiTableColumnFlags_WidthFixed, 82.0f);
    ImGui::TableSetupColumn(FieldNames::StageEntry[18],   ImGuiTableColumnFlags_WidthFixed, 82.0f);
    ImGui::TableSetupColumn(FieldNames::StageEntry[21],   ImGuiTableColumnFlags_WidthFixed, 95.0f);
    ImGui::TableSetupColumn(FieldNames::StageEntry[22],   ImGuiTableColumnFlags_WidthFixed, 95.0f);
    ImGui::TableSetupColumn(FieldNames::StageEntry[28],   ImGuiTableColumnFlags_WidthFixed, 130.0f);
    ImGui::TableSetupColumn(FieldNames::StageEntry[29],   ImGuiTableColumnFlags_WidthFixed, 200.0f);
    ImGui::TableSetupColumn(FieldNames::StageEntry[34],   ImGuiTableColumnFlags_WidthFixed, 95.0f);
    ImGui::TableSetupColumn(FieldNames::StageEntry[36],   ImGuiTableColumnFlags_WidthFixed, 82.0f);
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
    ImGui::TableSetupColumn("#",                                ImGuiTableColumnFlags_WidthFixed, 52.0f);
    ImGui::TableSetupColumn(FieldNames::BallPropertyEntry[0],   ImGuiTableColumnFlags_WidthFixed, 95.0f);
    ImGui::TableSetupColumn(FieldNames::BallPropertyEntry[1],   ImGuiTableColumnFlags_WidthFixed, 140.0f);
    ImGui::TableSetupColumn(FieldNames::BallPropertyEntry[2],   ImGuiTableColumnFlags_WidthFixed, 140.0f);
    ImGui::TableSetupColumn(FieldNames::BallPropertyEntry[8],   ImGuiTableColumnFlags_WidthFixed, 80.0f);
    ImGui::TableSetupColumn(FieldNames::BallPropertyEntry[9],   ImGuiTableColumnFlags_WidthFixed, 80.0f);
    ImGui::TableSetupColumn(FieldNames::BallPropertyEntry[10],  ImGuiTableColumnFlags_WidthFixed, 80.0f);
    ImGui::TableSetupColumn(FieldNames::BallPropertyEntry[11],  ImGuiTableColumnFlags_WidthFixed, 80.0f);
    ImGui::TableSetupColumn(FieldNames::BallPropertyEntry[12],  ImGuiTableColumnFlags_WidthFixed, 80.0f);
    ImGui::TableSetupColumn(FieldNames::BallPropertyEntry[13],  ImGuiTableColumnFlags_WidthFixed, 80.0f);
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
    ImGui::TableSetupColumn("#",                                    ImGuiTableColumnFlags_WidthFixed, 52.0f);
    ImGui::TableSetupColumn(FieldNames::BodyCylinderDataEntry[0],   ImGuiTableColumnFlags_WidthFixed, 95.0f);
    ImGui::TableSetupColumn(FieldNames::BodyCylinderDataEntry[1],   ImGuiTableColumnFlags_WidthFixed, 80.0f);
    ImGui::TableSetupColumn(FieldNames::BodyCylinderDataEntry[2],   ImGuiTableColumnFlags_WidthFixed, 80.0f);
    ImGui::TableSetupColumn(FieldNames::BodyCylinderDataEntry[3],   ImGuiTableColumnFlags_WidthFixed, 80.0f);
    ImGui::TableSetupColumn(FieldNames::BodyCylinderDataEntry[8],   ImGuiTableColumnFlags_WidthFixed, 80.0f);
    ImGui::TableSetupColumn(FieldNames::BodyCylinderDataEntry[9],   ImGuiTableColumnFlags_WidthFixed, 80.0f);
    ImGui::TableSetupColumn(FieldNames::BodyCylinderDataEntry[10],  ImGuiTableColumnFlags_WidthFixed, 80.0f);
    ImGui::TableSetupColumn(FieldNames::BodyCylinderDataEntry[15],  ImGuiTableColumnFlags_WidthFixed, 80.0f);
    ImGui::TableSetupColumn(FieldNames::BodyCylinderDataEntry[16],  ImGuiTableColumnFlags_WidthFixed, 80.0f);
    ImGui::TableSetupColumn(FieldNames::BodyCylinderDataEntry[17],  ImGuiTableColumnFlags_WidthFixed, 80.0f);
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
    ImGui::Separator();

    constexpr ImGuiTableFlags tFlags =
        ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg |
        ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersInnerV |
        ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable |
        ImGuiTableFlags_Hideable | ImGuiTableFlags_SizingFixedFit;

    if (!ImGui::BeginTabBar("##UniqueItemTabs"))
        return;

    if (ImGui::BeginTabItem("entries"))
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.42f, 0.42f, 0.54f, 1.00f));
        ImGui::Text("(%d entries)", (int)bin.customizeItemUniqueEntries.size());
        ImGui::PopStyleColor();
        const float addBtnW = 100.0f;
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - addBtnW + ImGui::GetCursorPosX());
        if (ImGui::Button("+ Add Entry", ImVec2(addBtnW, 0)))
            bin.customizeItemUniqueEntries.push_back(CustomizeItemUniqueEntry{});

        // cols: #, char_item_id, asset_name, character_hash, text_key, price, hash_2, hash_3
        if (ImGui::BeginTable("##CIUTable", 9, tFlags, ImGui::GetContentRegionAvail()))
        {
            ImGui::TableSetupScrollFreeze(1, 1);
            ImGui::TableSetupColumn("#",                                  ImGuiTableColumnFlags_WidthFixed, 52.0f);
            ImGui::TableSetupColumn(FieldNames::CustomizeItemUnique[0],   ImGuiTableColumnFlags_WidthFixed, 95.0f);
            ImGui::TableSetupColumn(FieldNames::CustomizeItemUnique[1],   ImGuiTableColumnFlags_WidthFixed, 200.0f);
            ImGui::TableSetupColumn(FieldNames::CustomizeItemUnique[2],   ImGuiTableColumnFlags_WidthFixed, 95.0f);
            ImGui::TableSetupColumn(FieldNames::CustomizeItemUnique[4],   ImGuiTableColumnFlags_WidthFixed, 200.0f);
            ImGui::TableSetupColumn(FieldNames::CustomizeItemUnique[11],  ImGuiTableColumnFlags_WidthFixed, 80.0f);
            ImGui::TableSetupColumn(FieldNames::CustomizeItemUnique[14],  ImGuiTableColumnFlags_WidthFixed, 95.0f);
            ImGui::TableSetupColumn(FieldNames::CustomizeItemUnique[17],  ImGuiTableColumnFlags_WidthFixed, 95.0f);
            ImGui::TableHeadersRow();

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

                    ImGui::TableSetColumnIndex(1); U32Cell("##cid", e.char_item_id);
                    ImGui::TableSetColumnIndex(2); StrCell("##an",  e.asset_name,      sizeof(e.asset_name));
                    ImGui::TableSetColumnIndex(3); U32Cell("##ch",  e.character_hash);
                    ImGui::TableSetColumnIndex(4); StrCell("##tk",  e.text_key,        sizeof(e.text_key));
                    ImGui::TableSetColumnIndex(5); U32Cell("##prc", e.price);
                    ImGui::TableSetColumnIndex(6); U32Cell("##h2",  e.hash_2);
                    ImGui::TableSetColumnIndex(7); U32Cell("##h3",  e.hash_3);

                    ImGui::PopID();
                }
            }
            if (deleteIdx >= 0)
                bin.customizeItemUniqueEntries.erase(bin.customizeItemUniqueEntries.begin() + deleteIdx);

            ImGui::EndTable();
        }
        ImGui::EndTabItem();
    }

    if (ImGui::BeginTabItem("body_entries"))
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.42f, 0.42f, 0.54f, 1.00f));
        ImGui::Text("(%d entries)", (int)bin.customizeItemUniqueBodyEntries.size());
        ImGui::PopStyleColor();
        const float addBtnW = 100.0f;
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - addBtnW + ImGui::GetCursorPosX());
        if (ImGui::Button("+ Add Entry##body", ImVec2(addBtnW, 0)))
            bin.customizeItemUniqueBodyEntries.push_back(CustomizeItemUniqueBodyEntry{});

        if (ImGui::BeginTable("##CIUBodyTable", 3, tFlags, ImGui::GetContentRegionAvail()))
        {
            ImGui::TableSetupScrollFreeze(1, 1);
            ImGui::TableSetupColumn("#",                                   ImGuiTableColumnFlags_WidthFixed, 52.0f);
            ImGui::TableSetupColumn(FieldNames::CustomizeItemUniqueBody[0], ImGuiTableColumnFlags_WidthFixed, 200.0f);
            ImGui::TableSetupColumn(FieldNames::CustomizeItemUniqueBody[1], ImGuiTableColumnFlags_WidthFixed, 95.0f);
            ImGui::TableHeadersRow();

            int deleteIdx = -1;
            ImGuiListClipper clipper;
            clipper.Begin((int)bin.customizeItemUniqueBodyEntries.size());
            while (clipper.Step())
            {
                for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i)
                {
                    auto& e = bin.customizeItemUniqueBodyEntries[i];
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
                    ImGui::InputText("##an", e.asset_name, sizeof(e.asset_name));
                    ImGui::TableSetColumnIndex(2);
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    ImGui::InputScalar("##cid", ImGuiDataType_U32, &e.char_item_id);

                    ImGui::PopID();
                }
            }
            if (deleteIdx >= 0)
                bin.customizeItemUniqueBodyEntries.erase(bin.customizeItemUniqueBodyEntries.begin() + deleteIdx);

            ImGui::EndTable();
        }
        ImGui::EndTabItem();
    }

    ImGui::EndTabBar();
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
            ImGui::TableSetupColumn("#",                                 ImGuiTableColumnFlags_WidthFixed, 52.0f);
            ImGui::TableSetupColumn(FieldNames::CharacterSelectHash[0],  ImGuiTableColumnFlags_WidthFixed, 95.0f);
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
            ImGui::TableSetupColumn("#",                                  ImGuiTableColumnFlags_WidthFixed, 52.0f);
            ImGui::TableSetupColumn(FieldNames::CharacterSelectParam[0],  ImGuiTableColumnFlags_WidthFixed, 100.0f);
            ImGui::TableSetupColumn(FieldNames::CharacterSelectParam[1],  ImGuiTableColumnFlags_WidthFixed, 80.0f);
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
        ImGui::TableSetupColumn("#",                                     ImGuiTableColumnFlags_WidthFixed, 52.0f);
        ImGui::TableSetupColumn(FieldNames::CustomizeItemProhibitDrama[0], ImGuiTableColumnFlags_WidthFixed, 80.0f);
        ImGui::TableSetupColumn(FieldNames::CustomizeItemProhibitDrama[1], ImGuiTableColumnFlags_WidthFixed, 80.0f);
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
        ImGui::TableSetupColumn("#",                              ImGuiTableColumnFlags_WidthFixed, 52.0f);
        ImGui::TableSetupColumn(FieldNames::BattleMotionEntry[0], ImGuiTableColumnFlags_WidthFixed, 80.0f);
        ImGui::TableSetupColumn(FieldNames::BattleMotionEntry[1], ImGuiTableColumnFlags_WidthFixed, 80.0f);
        ImGui::TableSetupColumn(FieldNames::BattleMotionEntry[2], ImGuiTableColumnFlags_WidthFixed, 80.0f);
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
            ImGui::TableSetupColumn("#",                               ImGuiTableColumnFlags_WidthFixed, 52.0f);
            ImGui::TableSetupColumn(FieldNames::ArcadeCpuCharacter[0], ImGuiTableColumnFlags_WidthFixed, 95.0f);
            ImGui::TableSetupColumn(FieldNames::ArcadeCpuCharacter[1], ImGuiTableColumnFlags_WidthFixed, 80.0f);
            ImGui::TableSetupColumn(FieldNames::ArcadeCpuCharacter[2], ImGuiTableColumnFlags_WidthFixed, 80.0f);
            ImGui::TableSetupColumn(FieldNames::ArcadeCpuCharacter[3], ImGuiTableColumnFlags_WidthFixed, 80.0f);
            ImGui::TableSetupColumn(FieldNames::ArcadeCpuCharacter[4], ImGuiTableColumnFlags_WidthFixed, 80.0f);
            ImGui::TableSetupColumn(FieldNames::ArcadeCpuCharacter[5], ImGuiTableColumnFlags_WidthFixed, 80.0f);
            ImGui::TableSetupColumn(FieldNames::ArcadeCpuCharacter[6], ImGuiTableColumnFlags_WidthFixed, 80.0f);
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
                ImGui::TableSetupColumn("#",                           ImGuiTableColumnFlags_WidthFixed, 52.0f);
                ImGui::TableSetupColumn(FieldNames::ArcadeCpuHash[0], ImGuiTableColumnFlags_WidthFixed, 95.0f);
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
            ImGui::TableSetupColumn("#",                           ImGuiTableColumnFlags_WidthFixed, 52.0f);
            ImGui::TableSetupColumn(FieldNames::ArcadeCpuRule[0], ImGuiTableColumnFlags_WidthFixed, 60.0f);
            ImGui::TableSetupColumn(FieldNames::ArcadeCpuRule[1], ImGuiTableColumnFlags_WidthFixed, 60.0f);
            ImGui::TableSetupColumn(FieldNames::ArcadeCpuRule[2], ImGuiTableColumnFlags_WidthFixed, 80.0f);
            ImGui::TableSetupColumn(FieldNames::ArcadeCpuRule[3], ImGuiTableColumnFlags_WidthFixed, 80.0f);
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
                ImGui::TableSetupColumn("#",                               ImGuiTableColumnFlags_WidthFixed, 52.0f);
                ImGui::TableSetupColumn(FieldNames::BallRecommendEntry[0], ImGuiTableColumnFlags_WidthFixed, 95.0f);
                ImGui::TableSetupColumn(FieldNames::BallRecommendEntry[1], ImGuiTableColumnFlags_WidthFixed, 200.0f);
                ImGui::TableSetupColumn(FieldNames::BallRecommendEntry[2], ImGuiTableColumnFlags_WidthFixed, 200.0f);
                ImGui::TableSetupColumn(FieldNames::BallRecommendEntry[3], ImGuiTableColumnFlags_WidthFixed, 80.0f);
                ImGui::TableSetupColumn(FieldNames::BallRecommendEntry[4], ImGuiTableColumnFlags_WidthFixed, 80.0f);
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
            ImGui::TableSetupColumn("#",                                   ImGuiTableColumnFlags_WidthFixed, 52.0f);
            ImGui::TableSetupColumn(FieldNames::BattleCommonSingleValue[0],ImGuiTableColumnFlags_WidthFixed, 80.0f);
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
            ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, 52.0f);
            for (int fi = 0; fi < 8; ++fi)
                ImGui::TableSetupColumn(FieldNames::BattleCommonCharacterScale[fi], ImGuiTableColumnFlags_WidthFixed, fi == 0 ? 95.0f : 75.0f);
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
            ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, 52.0f);
            for (int fi = 0; fi < 3; ++fi)
                ImGui::TableSetupColumn(FieldNames::BattleCommonPair[fi], ImGuiTableColumnFlags_WidthFixed, 80.0f);
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
            ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, 52.0f);
            for (int fi = 0; fi < 3; ++fi)
                ImGui::TableSetupColumn(FieldNames::BattleCommonMisc[fi], ImGuiTableColumnFlags_WidthFixed, 80.0f);
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
            ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, 52.0f);
            for (int vi = 0; vi < 47; ++vi)
                ImGui::TableSetupColumn(FieldNames::BattleCpuRank[vi], ImGuiTableColumnFlags_WidthFixed, 80.0f);
            ImGui::TableSetupColumn(FieldNames::BattleCpuRank[47], ImGuiTableColumnFlags_WidthFixed, 130.0f);
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
            ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, 52.0f);
            for (int fi = 0; fi < 4; ++fi)
                ImGui::TableSetupColumn(FieldNames::BattleCpuStep[fi], ImGuiTableColumnFlags_WidthFixed, 80.0f);
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
            ImGui::TableSetupColumn("#",                     ImGuiTableColumnFlags_WidthFixed, 52.0f);
            ImGui::TableSetupColumn(FieldNames::RankItem[0], ImGuiTableColumnFlags_WidthFixed, 95.0f);
            ImGui::TableSetupColumn(FieldNames::RankItem[1], ImGuiTableColumnFlags_WidthFixed, 180.0f);
            ImGui::TableSetupColumn(FieldNames::RankItem[2], ImGuiTableColumnFlags_WidthFixed, 130.0f);
            ImGui::TableSetupColumn(FieldNames::RankItem[3], ImGuiTableColumnFlags_WidthFixed, 80.0f);
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
    ImGui::TableSetupColumn("#",                               ImGuiTableColumnFlags_WidthFixed, 52.0f);
    ImGui::TableSetupColumn(FieldNames::AssistInputEntry[0],   ImGuiTableColumnFlags_WidthFixed, 95.0f);
    for (int vi = 1; vi < 59; ++vi)
        ImGui::TableSetupColumn(FieldNames::AssistInputEntry[vi], ImGuiTableColumnFlags_WidthFixed, 60.0f);
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

