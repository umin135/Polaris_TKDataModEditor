// FbsData editor view implementation
#include "editors/FbsDataView.h"
#include "data/FieldNames.h"
#include "io/TkmodIO.h"
#include "imgui/imgui.h"
#include <cstring>
#include <cctype>

// ─────────────────────────────────────────────────────────────────────────────
//  All fbsdata bin files with support status
// ─────────────────────────────────────────────────────────────────────────────

struct BinInfo
{
    const char* filename;
    BinType     type;
    bool        supported;   // true = editor implemented, false = greyed out
    const char* category;    // submenu group label
};

static const BinInfo k_AllBins[] =
{
    // Character category (supported)
    { "customize_item_common_list.bin",          BinType::CustomizeItemCommonList,   true,  "Character"     },
    { "customize_item_exclusive_list.bin",       BinType::CustomizeItemExclusiveList,true,  "Character"     },
    { "character_list.bin",                      BinType::CharacterList,             true,  "Character"     },

    // Not supported
    { "arcade_cpu_list.bin",                     BinType::None,                    false, "Not supported" },
    { "area_list.bin",                           BinType::None,                    false, "Not supported" },
    { "assist_input_list.bin",                   BinType::None,                    false, "Not supported" },
    { "ball_property_list.bin",                  BinType::None,                    false, "Not supported" },
    { "ball_recommend_list.bin",                 BinType::None,                    false, "Not supported" },
    { "ball_setting_list.bin",                   BinType::None,                    false, "Not supported" },
    { "battle_common_list.bin",                  BinType::None,                    false, "Not supported" },
    { "battle_cpu_list.bin",                     BinType::None,                    false, "Not supported" },
    { "battle_motion_list.bin",                  BinType::None,                    false, "Not supported" },
    { "battle_subtitle_info.bin",                BinType::None,                    false, "Not supported" },
    { "body_cylinder_data_list.bin",             BinType::None,                    false, "Not supported" },
    { "button_help_list.bin",                    BinType::None,                    false, "Not supported" },
    { "button_image_list.bin",                   BinType::None,                    false, "Not supported" },
    { "character_episode_list.bin",              BinType::None,                    false, "Not supported" },
    { "character_panel_list.bin",                BinType::None,                    false, "Not supported" },
    { "character_select_list.bin",               BinType::None,                    false, "Not supported" },
    { "chat_window_list.bin",                    BinType::None,                    false, "Not supported" },
    { "common_dialog_details_list.bin",          BinType::None,                    false, "Not supported" },
    { "cosmos_country_code_list.bin",            BinType::None,                    false, "Not supported" },
    { "cosmos_language_code_game_list.bin",      BinType::None,                    false, "Not supported" },
    { "cosmos_language_code_list.bin",           BinType::None,                    false, "Not supported" },
    { "customize_gauge_list.bin",                BinType::None,                    false, "Not supported" },
    { "customize_item_acc_parameter_list.bin",   BinType::None,                    false, "Not supported" },
    { "customize_item_color_palette_list.bin",   BinType::None,                    false, "Not supported" },
    { "customize_item_color_slot_list.bin",      BinType::None,                    false, "Not supported" },
    { "customize_item_prohibit_drama_list.bin",  BinType::None,                    false, "Not supported" },
    { "customize_item_shop_camera_list.bin",     BinType::None,                    false, "Not supported" },
    { "customize_item_unique_list.bin",          BinType::None,                    false, "Not supported" },
    { "customize_model_viewer_list.bin",         BinType::None,                    false, "Not supported" },
    { "customize_panel_list.bin",                BinType::None,                    false, "Not supported" },
    { "customize_set_list.bin",                  BinType::None,                    false, "Not supported" },
    { "customize_shogo_bg_list.bin",             BinType::None,                    false, "Not supported" },
    { "customize_shogo_list.bin",                BinType::None,                    false, "Not supported" },
    { "customize_stage_light_list.bin",          BinType::None,                    false, "Not supported" },
    { "customize_unique_exclusion_list.bin",     BinType::None,                    false, "Not supported" },
    { "drama_player_start_list.bin",             BinType::None,                    false, "Not supported" },
    { "drama_voice_change_list.bin",             BinType::None,                    false, "Not supported" },
    { "fate_drama_player_start_list.bin",        BinType::None,                    false, "Not supported" },
    { "gallery_illust_list.bin",                 BinType::None,                    false, "Not supported" },
    { "gallery_movie_list.bin",                  BinType::None,                    false, "Not supported" },
    { "gallery_title_list.bin",                  BinType::None,                    false, "Not supported" },
    { "game_camera_data_list.bin",               BinType::None,                    false, "Not supported" },
    { "ghost_vs_ghost_battle_property_list.bin", BinType::None,                    false, "Not supported" },
    { "ghost_vs_ghost_property_list.bin",        BinType::None,                    false, "Not supported" },
    { "help_dialog_list.bin",                    BinType::None,                    false, "Not supported" },
    { "jukebox_list.bin",                        BinType::None,                    false, "Not supported" },
    { "lobby_menu_help_list.bin",                BinType::None,                    false, "Not supported" },
    { "movie_vibration_list.bin",                BinType::None,                    false, "Not supported" },
    { "option_settings_list.bin",                BinType::None,                    false, "Not supported" },
    { "parameter_camera_data_list.bin",          BinType::None,                    false, "Not supported" },
    { "per_fighter_basic_info_list.bin",         BinType::None,                    false, "Not supported" },
    { "per_fighter_battle_info_list.bin",        BinType::None,                    false, "Not supported" },
    { "per_fighter_motion_info_list.bin",        BinType::None,                    false, "Not supported" },
    { "per_fighter_voice_info_list.bin",         BinType::None,                    false, "Not supported" },
    { "photo_mode_list.bin",                     BinType::None,                    false, "Not supported" },
    { "player_profile_stage_light_list.bin",     BinType::None,                    false, "Not supported" },
    { "playing_stats_table.bin",                 BinType::None,                    false, "Not supported" },
    { "practice_position_reset_list.bin",        BinType::None,                    false, "Not supported" },
    { "quake_camera_data_list.bin",              BinType::None,                    false, "Not supported" },
    { "rank_list.bin",                           BinType::None,                    false, "Not supported" },
    { "region_list.bin",                         BinType::None,                    false, "Not supported" },
    { "replace_text_list.bin",                   BinType::None,                    false, "Not supported" },
    { "rom_ghost_info_list.bin",                 BinType::None,                    false, "Not supported" },
    { "rt_drama_player_start_list.bin",          BinType::None,                    false, "Not supported" },
    { "scene_bgm_list.bin",                      BinType::None,                    false, "Not supported" },
    { "scene_setting_list.bin",                  BinType::None,                    false, "Not supported" },
    { "series_list.bin",                         BinType::None,                    false, "Not supported" },
    { "software_keyboard_list.bin",              BinType::None,                    false, "Not supported" },
    { "sound_parameter_list.bin",                BinType::None,                    false, "Not supported" },
    { "staffroll_list.bin",                      BinType::None,                    false, "Not supported" },
    { "stage_list.bin",                          BinType::None,                    false, "Not supported" },
    { "store_customize_item_exclusive_list.bin", BinType::None,                    false, "Not supported" },
    { "store_stage_light_list.bin",              BinType::None,                    false, "Not supported" },
    { "story_battle_voice_list.bin",             BinType::None,                    false, "Not supported" },
    { "story_iw_battle_event_list.bin",          BinType::None,                    false, "Not supported" },
    { "story_settings_info_list.bin",            BinType::None,                    false, "Not supported" },
    { "subtitle_list.bin",                       BinType::None,                    false, "Not supported" },
    { "tam_battle_navi_list.bin",                BinType::None,                    false, "Not supported" },
    { "tam_help_dialog_list.bin",                BinType::None,                    false, "Not supported" },
    { "tam_message_list.bin",                    BinType::None,                    false, "Not supported" },
    { "tam_mission_list.bin",                    BinType::None,                    false, "Not supported" },
    { "tam_npc_list.bin",                        BinType::None,                    false, "Not supported" },
    { "tam_tips_command_list.bin",               BinType::None,                    false, "Not supported" },
    { "tips_list.bin",                           BinType::None,                    false, "Not supported" },
    { "unlock_list.bin",                         BinType::None,                    false, "Not supported" },
    { "vibration_pattern_list.bin",              BinType::None,                    false, "Not supported" },
    { "yellow_book_battle_voice_list.bin",       BinType::None,                    false, "Not supported" },
    { "yellow_book_settings_list.bin",           BinType::None,                    false, "Not supported" },
};
static constexpr int k_AllBinsCount = (int)(sizeof(k_AllBins) / sizeof(k_AllBins[0]));

static const float LIST_WIDTH = 290.0f;  // Contents List panel width

// ─────────────────────────────────────────────────────────────────────────────
//  Top toolbar
// ─────────────────────────────────────────────────────────────────────────────

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

// ─────────────────────────────────────────────────────────────────────────────
//  Main render entry
// ─────────────────────────────────────────────────────────────────────────────

bool FbsDataView::LoadFromPath(const std::string& path)
{
    ModData loaded;
    if (!TkmodIO::LoadFromPath(path, loaded))
        return false;
    m_data = std::move(loaded);
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────

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

    // ── Editor area (left) ──
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

    // ── Contents list (right) ──
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.09f, 0.09f, 0.12f, 1.00f));
    ImGui::BeginChild("##FbsList", ImVec2(LIST_WIDTH, totalH), false,
                      ImGuiWindowFlags_NoScrollbar);
    ImGui::PopStyleColor();
    RenderContentsList(LIST_WIDTH);
    ImGui::EndChild();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Editor area
// ─────────────────────────────────────────────────────────────────────────────

void FbsDataView::RenderEditorArea()
{
    if (m_data.selectedIndex < 0 ||
        m_data.selectedIndex >= (int)m_data.contents.size())
    {
        // Nothing selected — show hint
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
    default:
        ImGui::TextDisabled("No editor available for this bin type.");
        break;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  customize_item_common_list table editor
// ─────────────────────────────────────────────────────────────────────────────

void FbsDataView::RenderCustomizeItemCommonEditor(ContentsBinData& bin)
{
    // ── Header row ──
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.65f, 0.82f, 1.00f, 1.00f));
    ImGui::Text("customize_item_common_list.bin");
    ImGui::PopStyleColor();
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.42f, 0.42f, 0.54f, 1.00f));
    ImGui::Text("(%d entries)", (int)bin.commonEntries.size());
    ImGui::PopStyleColor();

    // ── Add Entry button (right-aligned) ──
    const float addBtnW = 100.0f;
    ImGui::SameLine(ImGui::GetContentRegionAvail().x - addBtnW + ImGui::GetCursorPosX());
    if (ImGui::Button("+ Add Entry", ImVec2(addBtnW, 0)))
        bin.commonEntries.push_back(CustomizeItemCommonEntry{});

    ImGui::Separator();

    // ── Table ──
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

    // Columns shown in the regular editor (subset of 26 — unknown fields omitted)
    // Schema id → display width
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

        // ── # column: row number + delete button ──
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

        // Inline helpers —— fill the entire column width
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

// ─────────────────────────────────────────────────────────────────────────────
//  character_list table editor
// ─────────────────────────────────────────────────────────────────────────────

void FbsDataView::RenderCharacterListEditor(ContentsBinData& bin)
{
    // ── Header row ──
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.65f, 0.82f, 1.00f, 1.00f));
    ImGui::Text("character_list.bin");
    ImGui::PopStyleColor();
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.42f, 0.42f, 0.54f, 1.00f));
    ImGui::Text("(%d entries)", (int)bin.characterEntries.size());
    ImGui::PopStyleColor();

    // ── Add Entry button (right-aligned) ──
    const float addBtnW = 100.0f;
    ImGui::SameLine(ImGui::GetContentRegionAvail().x - addBtnW + ImGui::GetCursorPosX());
    if (ImGui::Button("+ Add Entry", ImVec2(addBtnW, 0)))
        bin.characterEntries.push_back(CharacterEntry{});

    ImGui::Separator();

    // ── Table ──
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

        // ── # column: row number + delete button ──
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

// ─────────────────────────────────────────────────────────────────────────────
//  customize_item_exclusive_list editor (5 sub-tables via tab bar)
// ─────────────────────────────────────────────────────────────────────────────

void FbsDataView::RenderCustomizeItemExclusiveListEditor(ContentsBinData& bin)
{
    // ── Header row ──
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

// ─────────────────────────────────────────────────────────────────────────────
//  Contents list panel (right side)
// ─────────────────────────────────────────────────────────────────────────────

void FbsDataView::RenderContentsList(float listWidth)
{
    ImGui::SetCursorPos(ImVec2(10.0f, 8.0f));

    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.65f, 0.82f, 1.00f, 1.00f));
    ImGui::Text("Contents");
    ImGui::PopStyleColor();

    ImGui::SameLine(listWidth - 74.0f);

    // "+ Add" button — opens popup on hover or click
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

// ─────────────────────────────────────────────────────────────────────────────
//  Add popup (fbsdata > bins grouped by category)
// ─────────────────────────────────────────────────────────────────────────────

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
                    if (bin.type == BinType::CustomizeItemCommonList)
                        bin.commonEntries.push_back(CustomizeItemCommonEntry{});
                    else if (bin.type == BinType::CharacterList)
                        bin.characterEntries.push_back(CharacterEntry{});
                    else if (bin.type == BinType::CustomizeItemExclusiveList)
                        bin.exclusiveRuleEntries.push_back(CustomizeExclusiveRuleEntry{});

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
