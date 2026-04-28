#pragma once

// -----------------------------------------------------------------------------
//  ColumnWidths -- ImGui table column widths for each FbsData editor.
//
//  All widths are in pixels (WidthFixed).
//  The row-control "#" column always uses kRowCtrl (52.0f).
//  Edit the arrays below to adjust column widths without touching FbsDataView.cpp.
// -----------------------------------------------------------------------------

namespace ColumnWidths
{
    constexpr float kRowCtrl = 52.0f;

    // -- customize_item_common_list (26 data fields, indexed by field id 0..25) --
    // k_Common[i] = width for CommonItem[i]
    constexpr float kCommon[26] = {
          50.0f,  // 0  Item ID
          100.0f,  // 1  Local Item ID
          150.0f,  // 2  AssetName
          130.0f,  // 3  Character ID
          130.0f, // 4  ItemPosition ID
          200.0f,  // 5  Name Key
          100.0f,  // 6  Extra Name Key 1
          100.0f,  // 7  Extra Name Key 2
          65.0f,  // 8  isDefaultKey
          60.0f,  // 9  shop_sort_id
          60.0f,  // 10 Visiblity
          60.0f,  // 11 Rarity
          60.0f,  // 12 Price
          60.0f,  // 13 IsColorable
          60.0f,  // 14 category_no
          60.0f,  // 15 hash_2
          60.0f,  // 16 unk_16
          60.0f,  // 17 unk_17
          60.0f,  // 18 hash_3
          60.0f,  // 19 unk_19
          60.0f,  // 20 unk_20
          60.0f,  // 21 unk_21
          60.0f,  // 22 unk_22
          60.0f,  // 23 hash_4
          60.0f,  // 24 unk_24
          70.0f,  // 25 Game Version
    };
    // -- customize_item_unique_list (22 fields, indexed 0..21) --
    constexpr float kUnique[22] = {
          50.0f,  // 0  char_item_id (DDD)
          150.0f,  // 1  asset_name
          130.0f,  // 2  Char_hash
          130.0f,  // 3  ItemPos_hash
          200.0f,  // 4  string_4
          100.0f,  // 5  string_5
          100.0f,  // 6  string_6
          60.0f,  // 7  u32_7
          60.0f,  // 8  u32_8
          60.0f,  // 9  u32_9
          60.0f,  // 10 u32_10
          60.0f,  // 11 u32_11
          60.0f,  // 12 u32_12
          60.0f,  // 13 u32_13
          60.0f,  // 14 u32_14
          60.0f,  // 15 u32_15
          60.0f,  // 16 u32_16
          60.0f,  // 17 u32_17
          60.0f,  // 18 u32_18
          60.0f,  // 19 u32_19
          60.0f,  // 20 u32_20
          70.0f,  // 21 u32_21
    };

    // -- character_list (15 fields, sequential) --
    constexpr float kCharacter[15] = {
        130.0f,  // 0
         95.0f,  // 1
         78.0f,  // 2
         88.0f,  // 3
        100.0f,  // 4
        105.0f,  // 5
         78.0f,  // 6
         82.0f,  // 7
        170.0f,  // 8
        150.0f,  // 9
        150.0f,  // 10
        150.0f,  // 11
        155.0f,  // 12
        130.0f,  // 13
        130.0f,  // 14
    };

    // -- area_list (2 fields) --
    constexpr float kArea[2] = { 95.0f, 200.0f };

    // -- battle_subtitle_info (2 fields) --
    constexpr float kBattleSubtitle[2] = { 95.0f, 95.0f };

    // -- fate_drama_player_start_list (5 fields) --
    constexpr float kFateDramaPlayerStart[5] = { 95.0f, 95.0f, 80.0f, 95.0f, 78.0f };

    // -- jukebox_list (9 fields, id 0..8) --
    constexpr float kJukebox[9] = {
         95.0f,  // 0
         95.0f,  // 1
         95.0f,  // 2
        200.0f,  // 3
        200.0f,  // 4
        200.0f,  // 5
        200.0f,  // 6
        200.0f,  // 7
        200.0f,  // 8
    };

    // -- series_list (5 fields) --
    constexpr float kSeries[5] = { 95.0f, 180.0f, 180.0f, 180.0f, 180.0f };

    // -- tam_mission_list (displayed 8 fields: ids 0,2,3,4,5,6,7,8) --
    constexpr float kTamMission[8] = { 95.0f, 80.0f, 200.0f, 95.0f, 95.0f, 95.0f, 95.0f, 95.0f };

    // -- drama_player_start_list (displayed 9 fields: ids 0,2,3,4,6,7,8,10) --
    constexpr float kDramaPlayerStart[9] = { 95.0f, 80.0f, 95.0f, 95.0f, 80.0f, 80.0f, 95.0f, 80.0f };

    // -- stage_list (displayed 13 fields: ids 0,1,2,3,4,17,18,21,22,28,29,34,36) --
    constexpr float kStage[13] = {
        140.0f,  // 0
         95.0f,  // 1
         82.0f,  // 2
         95.0f,  // 3
         80.0f,  // 4
         82.0f,  // 17
         82.0f,  // 18
         95.0f,  // 21
         95.0f,  // 22
        130.0f,  // 28
        200.0f,  // 29
         95.0f,  // 34
         82.0f,  // 36
    };

    // -- ball_property_list (displayed 9 fields: ids 0,1,2,8,9,10,11,12,13) --
    constexpr float kBallProperty[9] = {
         95.0f,  // 0
        140.0f,  // 1
        140.0f,  // 2
         80.0f,  // 8
         80.0f,  // 9
         80.0f,  // 10
         80.0f,  // 11
         80.0f,  // 12
         80.0f,  // 13
    };

    // -- body_cylinder_data_list (displayed 10 fields: ids 0,1,2,3,8,9,10,15,16,17) --
    constexpr float kBodyCylinder[10] = {
         95.0f,  // 0
         80.0f,  // 1
         80.0f,  // 2
         80.0f,  // 3
         80.0f,  // 8
         80.0f,  // 9
         80.0f,  // 10
         80.0f,  // 15
         80.0f,  // 16
         80.0f,  // 17
    };

    // -- character_select_list: hash sub-table (1 field) --
    constexpr float kCharSelectHash[1] = { 95.0f };

    // -- character_select_list: param sub-table (2 fields) --
    constexpr float kCharSelectParam[2] = { 100.0f, 80.0f };

    // -- customize_item_prohibit_drama_list (2 fields) --
    constexpr float kProhibitDrama[2] = { 80.0f, 80.0f };

    // -- battle_motion_list (3 fields) --
    constexpr float kBattleMotion[3] = { 80.0f, 80.0f, 80.0f };

    // -- arcade_cpu_list: character sub-table (7 fields) --
    constexpr float kArcadeCpuCharacter[7] = { 95.0f, 80.0f, 80.0f, 80.0f, 80.0f, 80.0f, 80.0f };

    // -- arcade_cpu_list: hash sub-table (1 field) --
    constexpr float kArcadeCpuHash[1] = { 95.0f };

    // -- arcade_cpu_list: rule sub-table (4 fields) --
    constexpr float kArcadeCpuRule[4] = { 60.0f, 60.0f, 80.0f, 80.0f };

    // -- ball_recommend_list (5 fields) --
    constexpr float kBallRecommend[5] = { 95.0f, 200.0f, 200.0f, 80.0f, 80.0f };

    // -- battle_common_list: CharacterScale (field 0 = 95, rest = 75) --
    constexpr float kBattleCommonCharacterScale0 = 95.0f;
    constexpr float kBattleCommonCharacterScaleRest = 75.0f;

    // -- battle_common_list: SingleValue, Pair, Misc rows (all 80) --
    constexpr float kBattleCommonGeneric = 80.0f;

    // -- battle_cpu_list: Rank row (all 80, except BattleCpuRank[47] = 130) --
    constexpr float kBattleCpuRankGeneric = 80.0f;
    constexpr float kBattleCpuRank47 = 130.0f;

    // -- battle_cpu_list: Step row (all 80) --
    constexpr float kBattleCpuStepGeneric = 80.0f;

    // -- rank_list (4 fields) --
    constexpr float kRankItem[4] = { 95.0f, 180.0f, 130.0f, 80.0f };

    // -- assist_input_list: entry[0] = 95, value columns = 60 --
    constexpr float kAssistInputEntry0 = 95.0f;
    constexpr float kAssistInputValue = 60.0f;

    // -- customize_panel_list (11 fields: string fields 5-8 = 200, others = 95) --
    constexpr float kCustomizePanelString = 200.0f;
    constexpr float kCustomizePanelDefault = 95.0f;

    // -- customize_item_exclusive_list: rule sub-table (4 fields) --
    constexpr float kExclusiveRule[4] = { 95.0f, 95.0f, 82.0f, 95.0f };

    // -- customize_item_exclusive_list: pair sub-table (3 fields) --
    constexpr float kExclusivePair[3] = { 95.0f, 95.0f, 82.0f };
} // namespace ColumnWidths
