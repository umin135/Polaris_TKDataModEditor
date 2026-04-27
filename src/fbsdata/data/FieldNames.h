#pragma once

// -----------------------------------------------------------------------------
//  FieldNames -- editor display name mappings for each schema field
//
//  Each array is indexed by schema field id (id_0, id_1, ...).
//  Editing these names changes every column header, TSV header, and any
//  other label that references them -- JSON keys (id_0, id_1, ...) are
//  unaffected and remain stable regardless of changes here.
//
//  Comment format per entry:
//    "display name",  // id: N | type | (original schema name if different)
// -----------------------------------------------------------------------------

namespace FieldNames
{
    // -- customize_item_common_list.bin ----------------------------------------
    //    Entry type : CustomizeItemCommonEntry   (26 fields, id 0..25)
    constexpr const char* CommonItem[26] = {
        "Item ID",         // id:  0 | uint32
        "Local Item ID",   // id:  1 | uint32
        "AssetName",       // id:  2 | string
        "Character ID",    // id:  3 | uint32  
        "ItemPosition ID", // id:  4 | uint32
        "Name Key",        // id:  5 | string
        "Extra Text Key 1",// id:  6 | string
        "Extra Text Key 2",// id:  7 | string
        "isDefaultKey",    // id:  8 | uint32  (unknown)
        "shop_sort_id",    // id:  9 | uint32
        "Visiblity",       // id: 10 | bool
        "Rarity",          // id: 11 | uint32  (unknown)
        "Price",           // id: 12 | uint32
        "isColorable",     // id: 13 | bool    (unknown)
        "category_no",     // id: 14 | uint32
        "hash_2",          // id: 15 | uint32
        "unk_16",          // id: 16 | bool    (unknown)
        "unk_17",          // id: 17 | uint32  (unknown)
        "hash_3",          // id: 18 | uint32
        "unk_19",          // id: 19 | uint32  (unknown)
        "unk_20",          // id: 20 | uint32  (unknown)
        "unk_21",          // id: 21 | uint32  (unknown)
        "unk_22",          // id: 22 | uint32  (unknown)
        "hash_4",          // id: 23 | uint32
        "unk_24",          // id: 24 | uint32  (unknown)
        "Game Version",      // id: 25 | uint32
    };
    constexpr int CommonItemCount = 26;

    // -- customize_item_unique_list.bin -- unique entry -------------------------
    //    Entry type : CustomizeItemUniqueEntry   (22 fields, id 0..21)
    constexpr const char* CustomizeItemUnique[22] = {
        "Item ID",         // id:  0 | uint32
        "AssetName",       // id:  1 | string
        "Character ID",       // id:  2 | uint32  (character hash)
        "ItemPosition ID",    // id:  3 | uint32
        "Name Key",        // id:  4 | string
        "Extra Text Key 1",// id:  5 | string
        "Extra Text Key 2",// id:  6 | string
        "IsDefault",       // id:  7 | uint32
        "unk_8",           // id:  8 | uint32
        "Visiblity",       // id:  9 | bool
        "Rarity",          // id: 10 | uint32
        "Price",           // id: 11 | uint32
        "unk_12",          // id: 12 | uint32
        "unk_13",          // id: 13 | uint32
        "hash_2",          // id: 14 | uint32
        "isColorable",     // id: 15 | bool
        "unk_16",          // id: 16 | uint32
        "hash_3",          // id: 17 | uint32
        "unk_18",          // id: 18 | uint32
        "unk_19",          // id: 19 | uint32
        "unk_20",          // id: 20 | uint32
        "Game Version",    // id: 21 | uint32
    };
    constexpr int CustomizeItemUniqueCount = 22;

    // -- character_list.bin ----------------------------------------------------
    //    Entry type : CharacterEntry   (15 fields, id 0..14)
    constexpr const char* Character[15] = {
        "character_code",      // id:  0 | string
        "name_hash",           // id:  1 | uint32
        "is_enabled",          // id:  2 | bool
        "is_selectable",       // id:  3 | bool
        "group",               // id:  4 | string
        "camera_offset",       // id:  5 | float
        "is_playable",         // id:  6 | bool
        "sort_order",          // id:  7 | uint32
        "full_name_key",       // id:  8 | string
        "short_name_jp_key",   // id:  9 | string
        "short_name_key",      // id: 10 | string
        "origin_key",          // id: 11 | string
        "fighting_style_key",  // id: 12 | string
        "height_key",          // id: 13 | string
        "weight_key",          // id: 14 | string
    };
    constexpr int CharacterCount = 15;

    // -- customize_item_exclusive_list.bin -- rule entry ------------------------
    //    Entry type : CustomizeExclusiveRuleEntry   (4 fields, id 0..3)
    //    Used by: rule_entries (id_0), group_rule_entries (id_2), set_rule_entries (id_4)
    constexpr const char* ExclusiveRule[4] = {
        "item_id",      // id: 0 | uint32
        "hash",         // id: 1 | uint32
        "link_type",    // id: 2 | uint32
        "ref_item_id",  // id: 3 | uint32
    };
    constexpr int ExclusiveRuleCount = 4;

    // -- customize_item_exclusive_list.bin -- pair entry ------------------------
    //    Entry type : CustomizeExclusivePairEntry   (3 fields, id 0..2)
    //    Used by: pair_entries (id_1), group_pair_entries (id_3)
    constexpr const char* ExclusivePair[3] = {
        "item_id_a",  // id: 0 | uint32
        "item_id_b",  // id: 1 | uint32
        "flag",       // id: 2 | uint32
    };
    constexpr int ExclusivePairCount = 3;

    // -- customize_item_exclusive_list.bin -- sub-vector names -----------------
    //    These label the 5 array tabs shown in the exclusive list editor.
    //    (Parent table field ids: id_0..id_4)
    constexpr const char* ExclusiveArrays[5] = {
        "rule_entries",        // id: 0 | [ExclusiveRule]
        "pair_entries",        // id: 1 | [ExclusivePair]
        "group_rule_entries",  // id: 2 | [ExclusiveRule]
        "group_pair_entries",  // id: 3 | [ExclusivePair]
        "set_rule_entries",    // id: 4 | [ExclusiveRule]
    };

    // -- area_list.bin ---------------------------------------------------------
    //    Entry type : AreaEntry   (2 fields, id 0..1)
    constexpr const char* AreaEntry[2] = {
        "area_hash",  // id: 0 | uint32
        "area_code",  // id: 1 | string
    };
    constexpr int AreaEntryCount = 2;

    // -- battle_subtitle_info.bin ----------------------------------------------
    //    Entry type : BattleSubtitleInfoEntry   (2 fields, id 0..1)
    constexpr const char* BattleSubtitleInfo[2] = {
        "subtitle_hash",  // id: 0 | uint32
        "subtitle_type",  // id: 1 | uint32
    };
    constexpr int BattleSubtitleInfoCount = 2;

    // -- fate_drama_player_start_list.bin --------------------------------------
    //    Entry type : FateDramaPlayerStartEntry   (5 fields, id 0..4)
    constexpr const char* FateDramaPlayerStart[5] = {
        "character1_hash",  // id: 0 | uint32
        "character2_hash",  // id: 1 | uint32
        "value_0",          // id: 2 | uint32
        "hash_2",           // id: 3 | uint32
        "value_4",          // id: 4 | bool
    };
    constexpr int FateDramaPlayerStartCount = 5;

    // -- jukebox_list.bin ------------------------------------------------------
    //    Entry type : JukeboxEntry   (9 fields, id 0..8)
    constexpr const char* JukeboxEntry[9] = {
        "bgm_hash",         // id: 0 | uint32
        "series_hash",      // id: 1 | uint32
        "unk_2",            // id: 2 | uint32
        "cue_name",         // id: 3 | string
        "arrangement",      // id: 4 | string
        "alt_cue_name_1",   // id: 5 | string
        "alt_cue_name_2",   // id: 6 | string
        "alt_cue_name_3",   // id: 7 | string
        "display_text_key", // id: 8 | string
    };
    constexpr int JukeboxEntryCount = 9;

    // -- series_list.bin -------------------------------------------------------
    //    Entry type : SeriesEntry   (5 fields, id 0..4)
    constexpr const char* SeriesEntry[5] = {
        "series_hash",     // id: 0 | uint32
        "jacket_text_key", // id: 1 | string
        "jacket_icon_key", // id: 2 | string
        "logo_text_key",   // id: 3 | string
        "logo_icon_key",   // id: 4 | string
    };
    constexpr int SeriesEntryCount = 5;

    // -- tam_mission_list.bin --------------------------------------------------
    //    Entry type : TamMissionEntry   (12 fields, id 0..11)
    constexpr const char* TamMissionEntry[12] = {
        "mission_id",  // id:  0 | uint32
        "value_1",     // id:  1 | uint32
        "value_2",     // id:  2 | uint32
        "location",    // id:  3 | string
        "hash_0",      // id:  4 | uint32
        "hash_1",      // id:  5 | uint32
        "hash_2",      // id:  6 | uint32
        "hash_3",      // id:  7 | uint32
        "hash_4",      // id:  8 | uint32
        "value_9",     // id:  9 | uint32
        "value_10",    // id: 10 | uint32
        "value_11",    // id: 11 | uint32
    };
    constexpr int TamMissionEntryCount = 12;

    // -- drama_player_start_list.bin -------------------------------------------
    //    Entry type : DramaPlayerStartEntry   (63 fields, id 0..62)
    constexpr const char* DramaPlayerStart[63] = {
        "character_hash",  // id:  0 | uint32
        "hash_1",          // id:  1 | uint32
        "index",           // id:  2 | uint32
        "scene_hash",      // id:  3 | uint32
        "config_hash",     // id:  4 | uint32
        "unk_float_5",     // id:  5 | float
        "pos_x",           // id:  6 | float
        "pos_y",           // id:  7 | float
        "state_hash",      // id:  8 | uint32
        "unk_9",           // id:  9 | uint32
        "scale",           // id: 10 | float
        "ref_hash",        // id: 11 | uint32
        "unk_float_12",    // id: 12 | float
        "unk_float_13",    // id: 13 | float
        "unk_float_14",    // id: 14 | float
        "unk_float_15",    // id: 15 | float
        "unk_16",          // id: 16 | uint32
        "unk_17",          // id: 17 | uint32
        "unk_float_18",    // id: 18 | float
        "rate",            // id: 19 | float
        "blk1_marker",     // id: 20 | uint32
        "blk1_scale",      // id: 21 | float
        "blk1_field_22",   // id: 22 | float
        "blk1_field_23",   // id: 23 | float
        "blk1_field_24",   // id: 24 | float
        "blk1_field_25",   // id: 25 | float
        "blk1_field_26",   // id: 26 | float
        "blk1_field_27",   // id: 27 | float
        "blk1_field_28",   // id: 28 | float
        "blk1_angle",      // id: 29 | float
        "blk1_hash_a",     // id: 30 | uint32
        "blk1_hash_b",     // id: 31 | uint32
        "blk2_marker",     // id: 32 | uint32
        "blk2_scale",      // id: 33 | float
        "blk2_field_34",   // id: 34 | float
        "blk2_field_35",   // id: 35 | float
        "blk2_field_36",   // id: 36 | float
        "blk2_field_37",   // id: 37 | float
        "blk2_field_38",   // id: 38 | float
        "blk2_field_39",   // id: 39 | float
        "blk2_field_40",   // id: 40 | float
        "blk2_angle",      // id: 41 | float
        "blk2_hash_a",     // id: 42 | uint32
        "blk2_hash_b",     // id: 43 | uint32
        "blk3_marker",     // id: 44 | uint32
        "blk3_scale",      // id: 45 | float
        "blk3_field_46",   // id: 46 | float
        "blk3_field_47",   // id: 47 | float
        "blk3_field_48",   // id: 48 | float
        "blk3_field_49",   // id: 49 | float
        "blk3_field_50",   // id: 50 | float
        "blk3_field_51",   // id: 51 | float
        "blk3_field_52",   // id: 52 | float
        "blk3_angle",      // id: 53 | float
        "blk3_hash_a",     // id: 54 | uint32
        "blk3_hash_b",     // id: 55 | uint32
        "end_marker",      // id: 56 | uint32
        "unk_float_57",    // id: 57 | float
        "extra_range",     // id: 58 | float
        "extra_param_a",   // id: 59 | float
        "extra_param_b",   // id: 60 | float
        "extra_param_c",   // id: 61 | float
        "extra_param_d",   // id: 62 | float
    };
    constexpr int DramaPlayerStartCount = 63;

    // -- stage_list.bin --------------------------------------------------------
    //    Entry type : StageEntry   (37 fields, id 0..36)
    constexpr const char* StageEntry[37] = {
        "stage_code",          // id:  0 | string
        "stage_hash",          // id:  1 | uint32
        "is_selectable",       // id:  2 | bool
        "camera_offset",       // id:  3 | float
        "parent_stage_index",  // id:  4 | uint32
        "variant_hash",        // id:  5 | uint32
        "has_weather",         // id:  6 | bool
        "is_active",           // id:  7 | bool
        "flag_interlocked",    // id:  8 | bool
        "flag_ocean",          // id:  9 | bool
        "flag_10",             // id: 10 | bool
        "flag_infinite",       // id: 11 | bool
        "flag_battle",         // id: 12 | bool
        "flag_13",             // id: 13 | bool
        "flag_balcony",        // id: 14 | bool
        "flag_15",             // id: 15 | bool
        "reserved_16",         // id: 16 | bool
        "is_online_enabled",   // id: 17 | bool
        "is_ranked_enabled",   // id: 18 | bool
        "reserved_19",         // id: 19 | bool
        "reserved_20",         // id: 20 | bool
        "arena_width",         // id: 21 | uint32
        "arena_depth",         // id: 22 | uint32
        "reserved_23",         // id: 23 | uint32
        "arena_param",         // id: 24 | uint32
        "extra_width",         // id: 25 | uint32
        "extra_group",         // id: 26 | string
        "extra_depth",         // id: 27 | uint32
        "group_id",            // id: 28 | string
        "stage_name_key",      // id: 29 | string
        "level_name",          // id: 30 | string
        "sound_bank",          // id: 31 | string
        "wall_distance_a",     // id: 32 | uint32
        "wall_distance_b",     // id: 33 | uint32
        "stage_mode",          // id: 34 | uint32
        "reserved_35",         // id: 35 | uint32
        "is_default_variant",  // id: 36 | bool
    };
    constexpr int StageEntryCount = 37;

    // -- ball_property_list.bin ------------------------------------------------
    //    Entry type : BallPropertyEntry   (19 fields, id 0..18)
    constexpr const char* BallPropertyEntry[19] = {
        "ball_hash",   // id:  0 | uint32
        "ball_code",   // id:  1 | string
        "effect_name", // id:  2 | string
        "hash_3",      // id:  3 | uint32
        "hash_4",      // id:  4 | uint32
        "unk_5",       // id:  5 | uint32
        "unk_6",       // id:  6 | uint32
        "hash_7",      // id:  7 | uint32
        "item_no",     // id:  8 | uint32
        "rarity",      // id:  9 | uint32
        "value_10",    // id: 10 | float
        "value_11",    // id: 11 | float
        "value_12",    // id: 12 | float
        "value_13",    // id: 13 | float
        "value_14",    // id: 14 | float
        "value_15",    // id: 15 | float
        "value_16",    // id: 16 | float
        "value_17",    // id: 17 | float
        "value_18",    // id: 18 | float
    };
    constexpr int BallPropertyEntryCount = 19;

    // -- body_cylinder_data_list.bin -------------------------------------------
    //    Entry type : BodyCylinderDataEntry   (19 fields, id 0..18)
    constexpr const char* BodyCylinderDataEntry[19] = {
        "character_hash",  // id:  0 | uint32
        "cyl0_radius",     // id:  1 | float
        "cyl0_height",     // id:  2 | float
        "cyl0_offset_y",   // id:  3 | float
        "cyl0_unk_hash",   // id:  4 | uint32
        "unk_5",           // id:  5 | uint32
        "unk_6",           // id:  6 | uint32
        "unk_7",           // id:  7 | uint32
        "cyl1_radius",     // id:  8 | float
        "cyl1_height",     // id:  9 | float
        "cyl1_offset_y",   // id: 10 | float
        "cyl1_unk_hash",   // id: 11 | uint32
        "unk_12",          // id: 12 | uint32
        "unk_13",          // id: 13 | uint32
        "unk_14",          // id: 14 | uint32
        "cyl2_radius",     // id: 15 | float
        "cyl2_height",     // id: 16 | float
        "cyl2_offset_y",   // id: 17 | float
        "cyl2_unk_hash",   // id: 18 | uint32
    };
    constexpr int BodyCylinderDataEntryCount = 19;

    // -- customize_item_unique_list.bin -- body entry ---------------------------
    //    Entry type : CustomizeItemUniqueBodyEntry   (2 fields, id 0..1)
    constexpr const char* CustomizeItemUniqueBody[2] = {
        "asset_name",   // id: 0 | string
        "char_item_id", // id: 1 | uint32
    };
    constexpr int CustomizeItemUniqueBodyCount = 2;

    // -- character_select_list.bin -- hash entry --------------------------------
    //    Entry type : CharacterSelectHashEntry   (1 field, id 0)
    constexpr const char* CharacterSelectHash[1] = {
        "character_hash",  // id: 0 | uint32
    };
    constexpr int CharacterSelectHashCount = 1;

    // -- character_select_list.bin -- param entry -------------------------------
    //    Entry type : CharacterSelectParamEntry   (2 fields, id 0..1)
    constexpr const char* CharacterSelectParam[2] = {
        "game_version",  // id: 0 | uint32
        "value_1",       // id: 1 | uint32
    };
    constexpr int CharacterSelectParamCount = 2;

    // -- customize_item_prohibit_drama_list.bin -- entry ------------------------
    //    Entry type : CustomizeItemProhibitDramaEntry   (2 fields, id 0..1)
    constexpr const char* CustomizeItemProhibitDrama[2] = {
        "value_0",  // id: 0 | int32
        "value_1",  // id: 1 | int32
    };
    constexpr int CustomizeItemProhibitDramaCount = 2;

    // -- battle_motion_list.bin -- entry ----------------------------------------
    //    Entry type : BattleMotionEntry   (3 fields, id 0..2)
    constexpr const char* BattleMotionEntry[3] = {
        "motion_id",  // id: 0 | ubyte
        "value_1",    // id: 1 | uint32
        "value_2",    // id: 2 | uint32
    };
    constexpr int BattleMotionEntryCount = 3;

    // -- arcade_cpu_list.bin -- settings ---------------------------------------
    //    Entry type : ArcadeCpuSettings   (3 fields, id 0..2)
    constexpr const char* ArcadeCpuSettings[3] = {
        "unk_0",  // id: 0 | uint32
        "unk_1",  // id: 1 | uint32
        "unk_2",  // id: 2 | uint32
    };
    constexpr int ArcadeCpuSettingsCount = 3;

    // -- arcade_cpu_list.bin -- character entry ---------------------------------
    //    Entry type : ArcadeCpuCharacterEntry   (7 fields, id 0..6)
    constexpr const char* ArcadeCpuCharacter[7] = {
        "character_hash",  // id: 0 | uint32
        "ai_level",        // id: 1 | uint32
        "float_1",         // id: 2 | float
        "uint_2",          // id: 3 | uint32
        "float_2",         // id: 4 | float
        "uint_3",          // id: 5 | uint32
        "float_3",         // id: 6 | float
    };
    constexpr int ArcadeCpuCharacterCount = 7;

    // -- arcade_cpu_list.bin -- hash entry -------------------------------------
    //    Entry type : ArcadeCpuHashEntry   (1 field, id 0)
    constexpr const char* ArcadeCpuHash[1] = {
        "value_hash",  // id: 0 | uint32
    };
    constexpr int ArcadeCpuHashCount = 1;

    // -- arcade_cpu_list.bin -- rule entry -------------------------------------
    //    Entry type : ArcadeCpuRuleEntry   (4 fields, id 0..3)
    constexpr const char* ArcadeCpuRule[4] = {
        "flag_0",   // id: 0 | uint8
        "flag_1",   // id: 1 | uint8
        "value_2",  // id: 2 | uint32
        "value_3",  // id: 3 | uint32
    };
    constexpr int ArcadeCpuRuleCount = 4;

    // -- ball_recommend_list.bin -- entry ---------------------------------------
    //    Entry type : BallRecommendEntry   (5 fields, id 0..4)
    constexpr const char* BallRecommendEntry[5] = {
        "character_hash",    // id: 0 | uint32
        "move_name_key",     // id: 1 | string
        "command_text_key",  // id: 2 | string
        "unk_3",             // id: 3 | uint32
        "unk_4",             // id: 4 | uint32
    };
    constexpr int BallRecommendEntryCount = 5;

    // -- ball_setting_list.bin -- data (72 scalar fields, id 0..71) ------------
    constexpr const char* BallSettingData[72] = {
        "value_0",   // id:  0 | float
        "value_1",   // id:  1 | float
        "value_2",   // id:  2 | float
        "value_3",   // id:  3 | float
        "value_4",   // id:  4 | float
        "value_5",   // id:  5 | float
        "value_6",   // id:  6 | float
        "value_7",   // id:  7 | float
        "value_8",   // id:  8 | float
        "value_9",   // id:  9 | uint32
        "value_10",  // id: 10 | uint32
        "value_11",  // id: 11 | float
        "value_12",  // id: 12 | float
        "value_13",  // id: 13 | float
        "value_14",  // id: 14 | float
        "value_15",  // id: 15 | uint32
        "value_16",  // id: 16 | float
        "value_17",  // id: 17 | uint32
        "value_18",  // id: 18 | float
        "value_19",  // id: 19 | float
        "value_20",  // id: 20 | float
        "value_21",  // id: 21 | float
        "value_22",  // id: 22 | float
        "value_23",  // id: 23 | float
        "value_24",  // id: 24 | uint32
        "value_25",  // id: 25 | uint32
        "value_26",  // id: 26 | uint32
        "value_27",  // id: 27 | uint32
        "value_28",  // id: 28 | uint32
        "value_29",  // id: 29 | uint32
        "value_30",  // id: 30 | float
        "value_31",  // id: 31 | uint32
        "value_32",  // id: 32 | float
        "value_33",  // id: 33 | uint32
        "value_34",  // id: 34 | uint32
        "value_35",  // id: 35 | uint32
        "value_36",  // id: 36 | uint32
        "value_37",  // id: 37 | uint32
        "value_38",  // id: 38 | uint32
        "value_39",  // id: 39 | uint32
        "value_40",  // id: 40 | uint32
        "value_41",  // id: 41 | float
        "value_42",  // id: 42 | float
        "value_43",  // id: 43 | float
        "value_44",  // id: 44 | uint32
        "value_45",  // id: 45 | uint32
        "value_46",  // id: 46 | float
        "value_47",  // id: 47 | uint32
        "value_48",  // id: 48 | uint32
        "value_49",  // id: 49 | uint32
        "value_50",  // id: 50 | uint32
        "value_51",  // id: 51 | uint32
        "value_52",  // id: 52 | uint32
        "value_53",  // id: 53 | float
        "value_54",  // id: 54 | float
        "value_55",  // id: 55 | uint32
        "value_56",  // id: 56 | float
        "value_57",  // id: 57 | float
        "value_58",  // id: 58 | uint32
        "value_59",  // id: 59 | uint32
        "value_60",  // id: 60 | uint32
        "value_61",  // id: 61 | float
        "value_62",  // id: 62 | float
        "value_63",  // id: 63 | uint32
        "value_64",  // id: 64 | uint32
        "value_65",  // id: 65 | uint32
        "value_66",  // id: 66 | uint32
        "value_67",  // id: 67 | uint32
        "value_68",  // id: 68 | uint32
        "value_69",  // id: 69 | uint32
        "value_70",  // id: 70 | uint32
        "value_71",  // id: 71 | uint32
    };
    constexpr int BallSettingDataCount = 72;

    // -- battle_common_list.bin -- single value entry ---------------------------
    //    Entry type : BattleCommonSingleValueEntry   (1 field, id 0)
    constexpr const char* BattleCommonSingleValue[1] = {
        "value",  // id: 0 | uint32
    };
    constexpr int BattleCommonSingleValueCount = 1;

    // -- battle_common_list.bin -- character scale entry ------------------------
    //    Entry type : BattleCommonCharacterScaleEntry   (8 fields, id 0..7)
    constexpr const char* BattleCommonCharacterScale[8] = {
        "hash_0",   // id: 0 | uint32
        "value_1",  // id: 1 | float
        "value_2",  // id: 2 | float
        "value_3",  // id: 3 | float
        "value_4",  // id: 4 | float
        "value_5",  // id: 5 | float
        "value_6",  // id: 6 | float
        "value_7",  // id: 7 | float
    };
    constexpr int BattleCommonCharacterScaleCount = 8;

    // -- battle_common_list.bin -- pair entry -----------------------------------
    //    Entry type : BattleCommonPairEntry   (3 fields, id 0..2)
    constexpr const char* BattleCommonPair[3] = {
        "value_0",  // id: 0 | uint32
        "value_1",  // id: 1 | uint32
        "value_2",  // id: 2 | uint32
    };
    constexpr int BattleCommonPairCount = 3;

    // -- battle_common_list.bin -- misc entry -----------------------------------
    //    Entry type : BattleCommonMiscEntry   (3 fields, id 0..2)
    constexpr const char* BattleCommonMisc[3] = {
        "value_0",  // id: 0 | float
        "value_1",  // id: 1 | float
        "value_2",  // id: 2 | float
    };
    constexpr int BattleCommonMiscCount = 3;

    // -- battle_cpu_list.bin -- rank entry -------------------------------------
    //    Entry type : BattleCpuRankEntry   (48 fields, id 0..47)
    //    id 0..46: value_N (uint32), id 47: rank_label (string)
    constexpr const char* BattleCpuRank[48] = {
        "value_0",    // id:  0 | uint32
        "value_1",    // id:  1 | uint32
        "value_2",    // id:  2 | uint32
        "value_3",    // id:  3 | uint32
        "value_4",    // id:  4 | uint32
        "value_5",    // id:  5 | uint32
        "value_6",    // id:  6 | uint32
        "value_7",    // id:  7 | uint32
        "value_8",    // id:  8 | uint32
        "value_9",    // id:  9 | uint32
        "value_10",   // id: 10 | uint32
        "value_11",   // id: 11 | uint32
        "value_12",   // id: 12 | uint32
        "value_13",   // id: 13 | uint32
        "value_14",   // id: 14 | uint32
        "value_15",   // id: 15 | uint32
        "value_16",   // id: 16 | uint32
        "value_17",   // id: 17 | uint32
        "value_18",   // id: 18 | uint32
        "value_19",   // id: 19 | uint32
        "value_20",   // id: 20 | uint32
        "value_21",   // id: 21 | uint32
        "value_22",   // id: 22 | uint32
        "value_23",   // id: 23 | uint32
        "value_24",   // id: 24 | uint32
        "value_25",   // id: 25 | uint32
        "value_26",   // id: 26 | uint32
        "value_27",   // id: 27 | uint32
        "value_28",   // id: 28 | uint32
        "value_29",   // id: 29 | uint32
        "value_30",   // id: 30 | uint32
        "value_31",   // id: 31 | uint32
        "value_32",   // id: 32 | uint32
        "value_33",   // id: 33 | uint32
        "value_34",   // id: 34 | uint32
        "value_35",   // id: 35 | uint32
        "value_36",   // id: 36 | uint32
        "value_37",   // id: 37 | uint32
        "value_38",   // id: 38 | uint32
        "value_39",   // id: 39 | uint32
        "value_40",   // id: 40 | uint32
        "value_41",   // id: 41 | uint32
        "value_42",   // id: 42 | uint32
        "value_43",   // id: 43 | uint32
        "value_44",   // id: 44 | uint32
        "value_45",   // id: 45 | uint32
        "value_46",   // id: 46 | uint32
        "rank_label", // id: 47 | string
    };
    constexpr int BattleCpuRankCount = 48;

    // -- battle_cpu_list.bin -- step entry -------------------------------------
    //    Entry type : BattleCpuStepEntry   (4 fields, id 0..3)
    constexpr const char* BattleCpuStep[4] = {
        "value_0",  // id: 0 | uint32
        "value_1",  // id: 1 | uint32
        "value_2",  // id: 2 | uint32
        "value_3",  // id: 3 | uint32
    };
    constexpr int BattleCpuStepCount = 4;

    // -- rank_list.bin -- rank item ---------------------------------------------
    //    Entry type : RankItem   (4 fields, id 0..3)
    constexpr const char* RankItem[4] = {
        "hash",     // id: 0 | uint32
        "text_key", // id: 1 | string
        "name",     // id: 2 | string
        "rank",     // id: 3 | uint32
    };
    constexpr int RankItemCount = 4;

    // -- rank_list.bin -- rank group --------------------------------------------
    //    Entry type : RankGroup   (2 fields, id 0..1)
    constexpr const char* RankGroup[2] = {
        "group_id",  // id: 0 | uint32
        "entries",   // id: 1 | [RankItem]
    };
    constexpr int RankGroupCount = 2;

    // -- assist_input_list.bin -- entry -----------------------------------------
    //    Entry type : AssistInputEntry   (59 fields, id 0..58)
    //    id 0: character_hash (uint32), id 1..58: value_N (int32)
    constexpr const char* AssistInputEntry[59] = {
        "character_hash",  // id:  0 | uint32
        "value_1",         // id:  1 | int32
        "value_2",         // id:  2 | int32
        "value_3",         // id:  3 | int32
        "value_4",         // id:  4 | int32
        "value_5",         // id:  5 | int32
        "value_6",         // id:  6 | int32
        "value_7",         // id:  7 | int32
        "value_8",         // id:  8 | int32
        "value_9",         // id:  9 | int32
        "value_10",        // id: 10 | int32
        "value_11",        // id: 11 | int32
        "value_12",        // id: 12 | int32
        "value_13",        // id: 13 | int32
        "value_14",        // id: 14 | int32
        "value_15",        // id: 15 | int32
        "value_16",        // id: 16 | int32
        "value_17",        // id: 17 | int32
        "value_18",        // id: 18 | int32
        "value_19",        // id: 19 | int32
        "value_20",        // id: 20 | int32
        "value_21",        // id: 21 | int32
        "value_22",        // id: 22 | int32
        "value_23",        // id: 23 | int32
        "value_24",        // id: 24 | int32
        "value_25",        // id: 25 | int32
        "value_26",        // id: 26 | int32
        "value_27",        // id: 27 | int32
        "value_28",        // id: 28 | int32
        "value_29",        // id: 29 | int32
        "value_30",        // id: 30 | int32
        "value_31",        // id: 31 | int32
        "value_32",        // id: 32 | int32
        "value_33",        // id: 33 | int32
        "value_34",        // id: 34 | int32
        "value_35",        // id: 35 | int32
        "value_36",        // id: 36 | int32
        "value_37",        // id: 37 | int32
        "value_38",        // id: 38 | int32
        "value_39",        // id: 39 | int32
        "value_40",        // id: 40 | int32
        "value_41",        // id: 41 | int32
        "value_42",        // id: 42 | int32
        "value_43",        // id: 43 | int32
        "value_44",        // id: 44 | int32
        "value_45",        // id: 45 | int32
        "value_46",        // id: 46 | int32
        "value_47",        // id: 47 | int32
        "value_48",        // id: 48 | int32
        "value_49",        // id: 49 | int32
        "value_50",        // id: 50 | int32
        "value_51",        // id: 51 | int32
        "value_52",        // id: 52 | int32
        "value_53",        // id: 53 | int32
        "value_54",        // id: 54 | int32
        "value_55",        // id: 55 | int32
        "value_56",        // id: 56 | int32
        "value_57",        // id: 57 | int32
        "value_58",        // id: 58 | int32
    };
    constexpr int AssistInputEntryCount = 59;

    // -- customize_panel_list.bin ---------------------------------------------
    //    Entry type : CustomizePanelEntry   (11 fields, id 0..10)
    constexpr const char* CustomizePanelEntry[11] = {
        "panel_hash",  // id:  0 | uint32
        "panel_id",    // id:  1 | uint32
        "price",       // id:  2 | uint32
        "category",    // id:  3 | uint32
        "sort_id",     // id:  4 | uint32
        "text_key",    // id:  5 | string
        "texture_1",   // id:  6 | string
        "texture_2",   // id:  7 | string
        "texture_3",   // id:  8 | string
        "flag_9",      // id:  9 | bool
        "hash_10",     // id: 10 | uint32
    };
    constexpr int CustomizePanelEntryCount = 11;
}
