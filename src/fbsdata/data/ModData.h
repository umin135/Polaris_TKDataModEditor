#pragma once
#include <vector>
#include <string>
#include <cstdint>
#include <cstring>

// -- Supported bin types ------------------------------------------------------
enum class BinType
{
    None,
    CustomizeItemCommonList,
    CharacterList,
    CustomizeItemExclusiveList,
    // additional types will be added as editors are implemented
    AreaList,
    BattleSubtitleInfoList,
    FateDramaPlayerStartList,
    JukeboxList,
    SeriesList,
    TamMissionList,
    DramaPlayerStartList,
    StageList,
    BallPropertyList,
    BodyCylinderDataList,
    CustomizeItemUniqueList,
    CharacterSelectList,
    CustomizeItemProhibitDramaList,
    BattleMotionList,
    ArcadeCpuList,
    BallRecommendList,
    BallSettingList,
    BattleCommonList,
    BattleCpuList,
    RankList,
    AssistInputList,
    CustomizePanelList,
};

// -- customize_item_common_list entry -----------------------------------------
struct CustomizeItemCommonEntry
{
    uint32_t item_id        = 0;
    int32_t  item_no        = 0;
    char     item_code[256] = {};
    uint32_t hash_0         = 0;
    uint32_t hash_1         = 0;
    char     text_key[256]  = {};
    char     package_id[128]     = {};
    char     package_sub_id[128] = {};
    uint32_t unk_8          = 0;    // schema id: 8
    int32_t  shop_sort_id   = 0;
    bool     is_enabled     = false;
    uint32_t unk_11         = 0;    // schema id: 11
    int32_t  price          = 0;
    bool     unk_13         = false; // schema id: 13
    int32_t  category_no    = 0;
    uint32_t hash_2         = 0;
    bool     unk_16         = false;
    uint32_t unk_17         = 0;    // schema id: 17
    uint32_t hash_3         = 0;
    uint32_t unk_19         = 0;    // schema id: 19
    uint32_t unk_20         = 0;    // schema id: 20
    uint32_t unk_21         = 0;    // schema id: 21
    uint32_t unk_22         = 0;    // schema id: 22
    uint32_t hash_4         = 0;
    int32_t  rarity         = 0;
    int32_t  sort_group     = 0;
};

// -- character_list entry ------------------------------------------------------
struct CharacterEntry
{
    char     character_code[64]     = {};
    uint32_t name_hash              = 0;
    bool     is_enabled             = true;
    bool     is_selectable          = true;
    char     group[64]              = {};
    float    camera_offset          = 0.0f;
    bool     is_playable            = true;
    uint32_t sort_order             = 0;
    char     full_name_key[128]     = {};
    char     short_name_jp_key[128] = {};
    char     short_name_key[128]    = {};
    char     origin_key[128]        = {};
    char     fighting_style_key[128]= {};
    char     height_key[128]        = {};
    char     weight_key[128]        = {};
};

// -- customize_item_exclusive_list entries ------------------------------------
struct CustomizeExclusiveRuleEntry
{
    uint32_t item_id    = 0;
    uint32_t hash       = 0;
    uint32_t link_type  = 0;
    uint32_t ref_item_id = 0;
};

struct CustomizeExclusivePairEntry
{
    uint32_t item_id_a = 0;
    uint32_t item_id_b = 0;
    uint32_t flag      = 0;
};

// -- area_list entry -----------------------------------------------------------
struct AreaEntry
{
    uint32_t area_hash = 0;
    char     area_code[256] = {};
};

// -- battle_subtitle_info entry ------------------------------------------------
struct BattleSubtitleInfoEntry
{
    uint32_t subtitle_hash = 0;
    uint32_t subtitle_type = 0;
};

// -- fate_drama_player_start_list entry ---------------------------------------
struct FateDramaPlayerStartEntry
{
    uint32_t character1_hash = 0;
    uint32_t character2_hash = 0;
    uint32_t value_0         = 0;
    uint32_t hash_2          = 0;
    bool     value_4         = false;
};

// -- jukebox_list entry --------------------------------------------------------
struct JukeboxEntry
{
    uint32_t bgm_hash    = 0;
    uint32_t series_hash = 0;
    uint32_t unk_2       = 0;
    char     cue_name[256]          = {};
    char     arrangement[256]       = {};
    char     alt_cue_name_1[256]    = {};
    char     alt_cue_name_2[256]    = {};
    char     alt_cue_name_3[256]    = {};
    char     display_text_key[256]  = {};
};

// -- series_list entry ---------------------------------------------------------
struct SeriesEntry
{
    uint32_t series_hash    = 0;
    char     jacket_text_key[256] = {};
    char     jacket_icon_key[256] = {};
    char     logo_text_key[256]   = {};
    char     logo_icon_key[256]   = {};
};

// -- tam_mission_list entry ----------------------------------------------------
struct TamMissionEntry
{
    uint32_t mission_id = 0;
    uint32_t value_1    = 0;
    uint32_t value_2    = 0;
    char     location[256] = {};
    uint32_t hash_0     = 0;
    uint32_t hash_1     = 0;
    uint32_t hash_2     = 0;
    uint32_t hash_3     = 0;
    uint32_t hash_4     = 0;
    uint32_t value_9    = 0;
    uint32_t value_10   = 0;
    uint32_t value_11   = 0;
};

// -- drama_player_start_list entry ---------------------------------------------
struct DramaPlayerStartEntry
{
    uint32_t character_hash  = 0;
    uint32_t hash_1          = 0;
    uint32_t index           = 0;
    uint32_t scene_hash      = 0;
    uint32_t config_hash     = 0;
    float    unk_float_5     = 0.0f;
    float    pos_x           = 0.0f;
    float    pos_y           = 0.0f;
    uint32_t state_hash      = 0;
    uint32_t unk_9           = 0;
    float    scale           = 0.0f;
    uint32_t ref_hash        = 0;
    float    unk_float_12    = 0.0f;
    float    unk_float_13    = 0.0f;
    float    unk_float_14    = 0.0f;
    float    unk_float_15    = 0.0f;
    uint32_t unk_16          = 0;
    uint32_t unk_17          = 0;
    float    unk_float_18    = 0.0f;
    float    rate            = 0.0f;
    uint32_t blk1_marker     = 0;
    float    blk1_scale      = 0.0f;
    float    blk1_field_22   = 0.0f;
    float    blk1_field_23   = 0.0f;
    float    blk1_field_24   = 0.0f;
    float    blk1_field_25   = 0.0f;
    float    blk1_field_26   = 0.0f;
    float    blk1_field_27   = 0.0f;
    float    blk1_field_28   = 0.0f;
    float    blk1_angle      = 0.0f;
    uint32_t blk1_hash_a     = 0;
    uint32_t blk1_hash_b     = 0;
    uint32_t blk2_marker     = 0;
    float    blk2_scale      = 0.0f;
    float    blk2_field_34   = 0.0f;
    float    blk2_field_35   = 0.0f;
    float    blk2_field_36   = 0.0f;
    float    blk2_field_37   = 0.0f;
    float    blk2_field_38   = 0.0f;
    float    blk2_field_39   = 0.0f;
    float    blk2_field_40   = 0.0f;
    float    blk2_angle      = 0.0f;
    uint32_t blk2_hash_a     = 0;
    uint32_t blk2_hash_b     = 0;
    uint32_t blk3_marker     = 0;
    float    blk3_scale      = 0.0f;
    float    blk3_field_46   = 0.0f;
    float    blk3_field_47   = 0.0f;
    float    blk3_field_48   = 0.0f;
    float    blk3_field_49   = 0.0f;
    float    blk3_field_50   = 0.0f;
    float    blk3_field_51   = 0.0f;
    float    blk3_field_52   = 0.0f;
    float    blk3_angle      = 0.0f;
    uint32_t blk3_hash_a     = 0;
    uint32_t blk3_hash_b     = 0;
    uint32_t end_marker      = 0;
    float    unk_float_57    = 0.0f;
    float    extra_range     = 0.0f;
    float    extra_param_a   = 0.0f;
    float    extra_param_b   = 0.0f;
    float    extra_param_c   = 0.0f;
    float    extra_param_d   = 0.0f;
};

// -- stage_list entry ----------------------------------------------------------
struct StageEntry
{
    char     stage_code[128]    = {};
    uint32_t stage_hash         = 0;
    bool     is_selectable      = false;
    float    camera_offset      = 0.0f;
    uint32_t parent_stage_index = 0;
    uint32_t variant_hash       = 0;
    bool     has_weather        = false;
    bool     is_active          = false;
    bool     flag_interlocked   = false;
    bool     flag_ocean         = false;
    bool     flag_10            = false;
    bool     flag_infinite      = false;
    bool     flag_battle        = false;
    bool     flag_13            = false;
    bool     flag_balcony       = false;
    bool     flag_15            = false;
    bool     reserved_16        = false;
    bool     is_online_enabled  = false;
    bool     is_ranked_enabled  = false;
    bool     reserved_19        = false;
    bool     reserved_20        = false;
    uint32_t arena_width        = 0;
    uint32_t arena_depth        = 0;
    uint32_t reserved_23        = 0;
    uint32_t arena_param        = 0;
    uint32_t extra_width        = 0;
    char     extra_group[256]   = {};
    uint32_t extra_depth        = 0;
    char     group_id[256]      = {};
    char     stage_name_key[256] = {};
    char     level_name[256]    = {};
    char     sound_bank[256]    = {};
    uint32_t wall_distance_a    = 0;
    uint32_t wall_distance_b    = 0;
    uint32_t stage_mode         = 0;
    uint32_t reserved_35        = 0;
    bool     is_default_variant = false;
};

// -- ball_property_list entry --------------------------------------------------
struct BallPropertyEntry
{
    uint32_t ball_hash   = 0;
    char     ball_code[256]   = {};
    char     effect_name[256] = {};
    uint32_t hash_3      = 0;
    uint32_t hash_4      = 0;
    uint32_t unk_5       = 0;
    uint32_t unk_6       = 0;
    uint32_t hash_7      = 0;
    uint32_t item_no     = 0;
    uint32_t rarity      = 0;
    float    value_10    = 0.0f;
    float    value_11    = 0.0f;
    float    value_12    = 0.0f;
    float    value_13    = 0.0f;
    float    value_14    = 0.0f;
    float    value_15    = 0.0f;
    float    value_16    = 0.0f;
    float    value_17    = 0.0f;
    float    value_18    = 0.0f;
};

// -- body_cylinder_data_list entry ---------------------------------------------
struct BodyCylinderDataEntry
{
    uint32_t character_hash  = 0;
    float    cyl0_radius     = 0.0f;
    float    cyl0_height     = 0.0f;
    float    cyl0_offset_y   = 0.0f;
    uint32_t cyl0_unk_hash   = 0;
    uint32_t unk_5           = 0;
    uint32_t unk_6           = 0;
    uint32_t unk_7           = 0;
    float    cyl1_radius     = 0.0f;
    float    cyl1_height     = 0.0f;
    float    cyl1_offset_y   = 0.0f;
    uint32_t cyl1_unk_hash   = 0;
    uint32_t unk_12          = 0;
    uint32_t unk_13          = 0;
    uint32_t unk_14          = 0;
    float    cyl2_radius     = 0.0f;
    float    cyl2_height     = 0.0f;
    float    cyl2_offset_y   = 0.0f;
    uint32_t cyl2_unk_hash   = 0;
};

// -- customize_item_unique_list entries ----------------------------------------
struct CustomizeItemUniqueEntry
{
    uint32_t char_item_id     = 0;
    char     asset_name[256]       = {};
    uint32_t character_hash   = 0;
    uint32_t hash_1           = 0;
    char     text_key[256]         = {};
    char     extra_text_key_1[256] = {};
    char     extra_text_key_2[256] = {};
    bool     flag_7           = false;
    uint32_t unk_8            = 0;
    bool     flag_9           = false;
    uint32_t unk_10           = 0;
    uint32_t price            = 0;
    uint32_t unk_12           = 0;
    uint32_t unk_13           = 0;
    uint32_t hash_2           = 0;
    bool     flag_15          = false;
    uint32_t unk_16           = 0;
    uint32_t hash_3           = 0;
    uint32_t unk_18           = 0;
    uint32_t unk_19           = 0;
    uint32_t unk_20           = 0;
    uint32_t unk_21           = 0;
};

struct CustomizeItemUniqueBodyEntry
{
    char     asset_name[256] = {};
    uint32_t char_item_id    = 0;
};

// -- character_select_list entries ---------------------------------------------
struct CharacterSelectHashEntry
{
    uint32_t character_hash = 0;
};

struct CharacterSelectParamEntry
{
    uint32_t game_version = 0;
    uint32_t value_1      = 0;
};

// -- customize_item_prohibit_drama_list entry ----------------------------------
struct CustomizeItemProhibitDramaEntry
{
    int32_t value_0 = 0;
    int32_t value_1 = 0;
};

// -- battle_motion_list entry --------------------------------------------------
struct BattleMotionEntry
{
    uint8_t  motion_id = 0;
    uint32_t value_1   = 0;
    uint32_t value_2   = 0;
};

// -- arcade_cpu_list entries ---------------------------------------------------
struct ArcadeCpuSettings
{
    uint32_t unk_0 = 0;
    uint32_t unk_1 = 0;
    uint32_t unk_2 = 0;
};

struct ArcadeCpuCharacterEntry
{
    uint32_t character_hash = 0;
    uint32_t ai_level       = 0;
    float    float_1        = 0.0f;
    uint32_t uint_2         = 0;
    float    float_2        = 0.0f;
    uint32_t uint_3         = 0;
    float    float_3        = 0.0f;
};

struct ArcadeCpuHashEntry
{
    uint32_t value_hash = 0;
};

struct ArcadeCpuRuleEntry
{
    uint8_t  flag_0  = 0;
    uint8_t  flag_1  = 0;
    uint32_t value_2 = 0;
    uint32_t value_3 = 0;
};

// -- ball_recommend_list entry -------------------------------------------------
struct BallRecommendEntry
{
    uint32_t character_hash    = 0;
    char     move_name_key[256]    = {};
    char     command_text_key[256] = {};
    uint32_t unk_3             = 0;
    uint32_t unk_4             = 0;
};

// -- ball_setting_list data (single table, no entries vector) ------------------
struct BallSettingData
{
    float    value_0  = 0.0f;  // id: 0  | float
    float    value_1  = 0.0f;  // id: 1  | float
    float    value_2  = 0.0f;  // id: 2  | float
    float    value_3  = 0.0f;  // id: 3  | float
    float    value_4  = 0.0f;  // id: 4  | float
    float    value_5  = 0.0f;  // id: 5  | float
    float    value_6  = 0.0f;  // id: 6  | float
    float    value_7  = 0.0f;  // id: 7  | float
    float    value_8  = 0.0f;  // id: 8  | float
    uint32_t value_9  = 0;     // id: 9  | uint32
    uint32_t value_10 = 0;     // id: 10 | uint32
    float    value_11 = 0.0f;  // id: 11 | float
    float    value_12 = 0.0f;  // id: 12 | float
    float    value_13 = 0.0f;  // id: 13 | float
    float    value_14 = 0.0f;  // id: 14 | float
    uint32_t value_15 = 0;     // id: 15 | uint32
    float    value_16 = 0.0f;  // id: 16 | float
    uint32_t value_17 = 0;     // id: 17 | uint32
    float    value_18 = 0.0f;  // id: 18 | float
    float    value_19 = 0.0f;  // id: 19 | float
    float    value_20 = 0.0f;  // id: 20 | float
    float    value_21 = 0.0f;  // id: 21 | float
    float    value_22 = 0.0f;  // id: 22 | float
    float    value_23 = 0.0f;  // id: 23 | float
    uint32_t value_24 = 0;     // id: 24 | uint32
    uint32_t value_25 = 0;     // id: 25 | uint32
    uint32_t value_26 = 0;     // id: 26 | uint32
    uint32_t value_27 = 0;     // id: 27 | uint32
    uint32_t value_28 = 0;     // id: 28 | uint32
    uint32_t value_29 = 0;     // id: 29 | uint32
    float    value_30 = 0.0f;  // id: 30 | float
    uint32_t value_31 = 0;     // id: 31 | uint32
    float    value_32 = 0.0f;  // id: 32 | float
    uint32_t value_33 = 0;     // id: 33 | uint32
    uint32_t value_34 = 0;     // id: 34 | uint32
    uint32_t value_35 = 0;     // id: 35 | uint32
    uint32_t value_36 = 0;     // id: 36 | uint32
    uint32_t value_37 = 0;     // id: 37 | uint32
    uint32_t value_38 = 0;     // id: 38 | uint32
    uint32_t value_39 = 0;     // id: 39 | uint32
    uint32_t value_40 = 0;     // id: 40 | uint32
    float    value_41 = 0.0f;  // id: 41 | float
    float    value_42 = 0.0f;  // id: 42 | float
    float    value_43 = 0.0f;  // id: 43 | float
    uint32_t value_44 = 0;     // id: 44 | uint32
    uint32_t value_45 = 0;     // id: 45 | uint32
    float    value_46 = 0.0f;  // id: 46 | float
    uint32_t value_47 = 0;     // id: 47 | uint32
    uint32_t value_48 = 0;     // id: 48 | uint32
    uint32_t value_49 = 0;     // id: 49 | uint32
    uint32_t value_50 = 0;     // id: 50 | uint32
    uint32_t value_51 = 0;     // id: 51 | uint32
    uint32_t value_52 = 0;     // id: 52 | uint32
    float    value_53 = 0.0f;  // id: 53 | float
    float    value_54 = 0.0f;  // id: 54 | float
    uint32_t value_55 = 0;     // id: 55 | uint32
    float    value_56 = 0.0f;  // id: 56 | float
    float    value_57 = 0.0f;  // id: 57 | float
    uint32_t value_58 = 0;     // id: 58 | uint32
    uint32_t value_59 = 0;     // id: 59 | uint32
    uint32_t value_60 = 0;     // id: 60 | uint32
    float    value_61 = 0.0f;  // id: 61 | float
    float    value_62 = 0.0f;  // id: 62 | float
    uint32_t value_63 = 0;     // id: 63 | uint32
    uint32_t value_64 = 0;     // id: 64 | uint32
    uint32_t value_65 = 0;     // id: 65 | uint32
    uint32_t value_66 = 0;     // id: 66 | uint32
    uint32_t value_67 = 0;     // id: 67 | uint32
    uint32_t value_68 = 0;     // id: 68 | uint32
    uint32_t value_69 = 0;     // id: 69 | uint32
    uint32_t value_70 = 0;     // id: 70 | uint32
    uint32_t value_71 = 0;     // id: 71 | uint32
};

// -- battle_common_list entries ------------------------------------------------
struct BattleCommonSingleValueEntry
{
    uint32_t value = 0;
};

struct BattleCommonCharacterScaleEntry
{
    uint32_t hash_0  = 0;
    float    value_1 = 0.0f;
    float    value_2 = 0.0f;
    float    value_3 = 0.0f;
    float    value_4 = 0.0f;
    float    value_5 = 0.0f;
    float    value_6 = 0.0f;
    float    value_7 = 0.0f;
};

struct BattleCommonPairEntry
{
    uint32_t value_0 = 0;
    uint32_t value_1 = 0;
    uint32_t value_2 = 0;
};

struct BattleCommonMiscEntry
{
    float value_0 = 0.0f;
    float value_1 = 0.0f;
    float value_2 = 0.0f;
};

// -- battle_cpu_list entries ---------------------------------------------------
struct BattleCpuRankEntry
{
    uint32_t values[47]    = {};
    char     rank_label[128] = {};
};

struct BattleCpuStepEntry
{
    uint32_t value_0 = 0;
    uint32_t value_1 = 0;
    uint32_t value_2 = 0;
    uint32_t value_3 = 0;
};

// -- rank_list entries ---------------------------------------------------------
struct RankItem
{
    uint32_t hash     = 0;
    char     text_key[256] = {};
    char     name[256]     = {};
    uint32_t rank     = 0;
};

struct RankGroup
{
    uint32_t             group_id = 0;
    std::vector<RankItem> entries;
};

// -- customize_panel_list entry -----------------------------------------------
struct CustomizePanelEntry
{
    uint32_t panel_hash  = 0;   // id: 0
    uint32_t panel_id    = 0;   // id: 1
    uint32_t price       = 0;   // id: 2
    uint32_t category    = 0;   // id: 3
    uint32_t sort_id     = 0;   // id: 4
    char     text_key[256]  = {};  // id: 5
    char     texture_1[256] = {};  // id: 6
    char     texture_2[256] = {};  // id: 7
    char     texture_3[256] = {};  // id: 8
    bool     flag_9      = false;  // id: 9
    uint32_t hash_10     = 0;   // id: 10
};

// -- assist_input_list entry ---------------------------------------------------
struct AssistInputEntry
{
    uint32_t character_hash = 0;
    int32_t  values[58]    = {};
};

// -- A single bin file added to the mod ---------------------------------------
struct ContentsBinData
{
    BinType     type = BinType::None;
    std::string name;  // e.g. "customize_item_common_list.bin"

    // Per-type payload (only the field matching 'type' is used)
    std::vector<CustomizeItemCommonEntry>  commonEntries;
    std::vector<CharacterEntry>            characterEntries;

    // customize_item_exclusive_list sub-vectors
    std::vector<CustomizeExclusiveRuleEntry> exclusiveRuleEntries;
    std::vector<CustomizeExclusivePairEntry> exclusivePairEntries;
    std::vector<CustomizeExclusiveRuleEntry> exclusiveGroupRuleEntries;
    std::vector<CustomizeExclusivePairEntry> exclusiveGroupPairEntries;
    std::vector<CustomizeExclusiveRuleEntry> exclusiveSetRuleEntries;

    // area_list
    std::vector<AreaEntry>                   areaEntries;

    // battle_subtitle_info
    std::vector<BattleSubtitleInfoEntry>     battleSubtitleEntries;

    // fate_drama_player_start_list
    std::vector<FateDramaPlayerStartEntry>   fateDramaPlayerStartEntries;

    // jukebox_list
    std::vector<JukeboxEntry>                jukeboxEntries;
    uint32_t                                 jukeboxListUnkValue1 = 0;  // list-level id:1

    // series_list
    std::vector<SeriesEntry>                 seriesEntries;

    // tam_mission_list
    std::vector<TamMissionEntry>             tamMissionEntries;

    // drama_player_start_list
    std::vector<DramaPlayerStartEntry>       dramaPlayerStartEntries;

    // stage_list
    std::vector<StageEntry>                  stageEntries;

    // ball_property_list
    std::vector<BallPropertyEntry>           ballPropertyEntries;
    uint32_t                                 ballPropertyListUnkValue1 = 0;  // list-level id:1

    // body_cylinder_data_list
    std::vector<BodyCylinderDataEntry>       bodyCylinderDataEntries;
    float                                    bodyCylinderGlobalScale = 0.0f;  // list-level id:0

    // customize_item_unique_list
    std::vector<CustomizeItemUniqueEntry>    customizeItemUniqueEntries;
    std::vector<CustomizeItemUniqueBodyEntry> customizeItemUniqueBodyEntries;

    // character_select_list
    std::vector<CharacterSelectHashEntry>    characterSelectHashEntries;
    std::vector<CharacterSelectParamEntry>   characterSelectParamEntries;

    // customize_item_prohibit_drama_list
    std::vector<CustomizeItemProhibitDramaEntry> prohibitDramaGroup0;
    std::vector<CustomizeItemProhibitDramaEntry> prohibitDramaGroup1;
    std::vector<uint32_t>                    prohibitDramaCategoryValues;

    // battle_motion_list
    std::vector<BattleMotionEntry>           battleMotionEntries;
    float                                    battleMotionValue1  = 0.0f;  // list-level id:1
    float                                    battleMotionValue2  = 0.0f;  // list-level id:2
    float                                    battleMotionValue3  = 0.0f;  // list-level id:3
    float                                    battleMotionValue4  = 0.0f;  // list-level id:4
    float                                    battleMotionValue5  = 0.0f;  // list-level id:5
    float                                    battleMotionValue6  = 0.0f;  // list-level id:6
    float                                    battleMotionValue7  = 0.0f;  // list-level id:7
    float                                    battleMotionValue8  = 0.0f;  // list-level id:8
    float                                    battleMotionValue9  = 0.0f;  // list-level id:9
    float                                    battleMotionValue10 = 0.0f;  // list-level id:10
    uint32_t                                 battleMotionValue11 = 0;     // list-level id:11
    std::vector<BattleMotionEntry>           battleMotionEntriesAlt;      // list-level id:12

    // arcade_cpu_list
    ArcadeCpuSettings                        arcadeCpuSettings;
    std::vector<ArcadeCpuCharacterEntry>     arcadeCpuCharacterEntries;
    std::vector<ArcadeCpuHashEntry>          arcadeCpuHashGroupA;
    std::vector<ArcadeCpuHashEntry>          arcadeCpuHashGroupB;
    std::vector<ArcadeCpuRuleEntry>          arcadeCpuRuleEntries;

    // ball_recommend_list
    std::vector<BallRecommendEntry>          ballRecommendGroup0;
    std::vector<BallRecommendEntry>          ballRecommendGroup1;
    std::vector<BallRecommendEntry>          ballRecommendGroup2;
    std::vector<uint32_t>                    ballRecommendUnkValues;

    // ball_setting_list (single data table)
    BallSettingData                          ballSettingData;

    // battle_common_list
    std::vector<BattleCommonSingleValueEntry>     battleCommonSingleValueEntries;
    std::vector<BattleCommonCharacterScaleEntry>  battleCommonCharacterScaleEntries;
    std::vector<BattleCommonPairEntry>            battleCommonPairEntries;
    uint32_t                                 battleCommonValue3 = 0;
    uint32_t                                 battleCommonValue4 = 0;
    std::vector<BattleCommonMiscEntry>            battleCommonMiscEntries;
    float                                    battleCommonValue6 = 0.0f;
    float                                    battleCommonValue7 = 0.0f;
    float                                    battleCommonValue8 = 0.0f;
    uint32_t                                 battleCommonValue9 = 0;
    float                                    battleCommonValue10 = 0.0f;
    uint32_t                                 battleCommonValue11 = 0;
    uint32_t                                 battleCommonValue12 = 0;
    uint32_t                                 battleCommonValue13 = 0;

    // battle_cpu_list
    std::vector<BattleCpuRankEntry>          battleCpuRankEntries;
    std::vector<BattleCpuStepEntry>          battleCpuStepEntries;
    std::vector<int32_t>                     battleCpuParamValues;
    BattleCpuRankEntry                       battleCpuRankExEntry;

    // rank_list
    std::vector<RankGroup>                   rankGroups;

    // assist_input_list
    std::vector<AssistInputEntry>            assistInputEntries;

    // customize_panel_list
    std::vector<CustomizePanelEntry>         customizePanelEntries;
};

// -- Mod metadata (written to mod_info.json) ----------------------------------
struct ModInfo
{
    char author[256]      = {};
    char description[512] = {};
    char version[64]      = {};
};

// -- Top-level mod data container ---------------------------------------------
struct ModData
{
    std::vector<ContentsBinData> contents;
    int     selectedIndex = -1;
    ModInfo info;
    bool    isNew = true;  // true = created via New, false = loaded from .tkmod

    bool HasBin(BinType type) const
    {
        for (const auto& c : contents)
            if (c.type == type) return true;
        return false;
    }

    bool HasBinByName(const std::string& name) const
    {
        for (const auto& c : contents)
            if (c.name == name) return true;
        return false;
    }
};
