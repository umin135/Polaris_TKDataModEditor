#pragma once
#include <vector>
#include <string>
#include <cstdint>
#include <cstring>

// ── Supported bin types ──────────────────────────────────────────────────────
enum class BinType
{
    None,
    CustomizeItemCommonList,
    CharacterList,
    CustomizeItemExclusiveList,
    // additional types will be added as editors are implemented
};

// ── customize_item_common_list entry ─────────────────────────────────────────
struct CustomizeItemCommonEntry
{
    uint32_t item_id;
    int32_t  item_no;
    char     item_code[256];
    uint32_t hash_0;
    uint32_t hash_1;
    char     text_key[256];
    char     package_id[128];
    char     package_sub_id[128];
    uint32_t unk_8;           // schema id: 8
    int32_t  shop_sort_id;
    bool     is_enabled;
    uint32_t unk_11;          // schema id: 11
    int32_t  price;
    bool     unk_13;          // schema id: 13
    int32_t  category_no;
    uint32_t hash_2;
    bool     unk_16;
    uint32_t unk_17;          // schema id: 17
    uint32_t hash_3;
    uint32_t unk_19;          // schema id: 19
    uint32_t unk_20;          // schema id: 20
    uint32_t unk_21;          // schema id: 21
    uint32_t unk_22;          // schema id: 22
    uint32_t hash_4;
    int32_t  rarity;
    int32_t  sort_group;

    // Default-constructs with the reference entry from the example JSON
    CustomizeItemCommonEntry()
        : item_id(20000001), item_no(1)
        , hash_0(2802412287u), hash_1(2325958612u)
        , unk_8(0)
        , shop_sort_id(1000000), is_enabled(true)
        , unk_11(0), price(30000), unk_13(false)
        , category_no(102)
        , hash_2(3229833922u), unk_16(true)
        , unk_17(0)
        , hash_3(2609503483u)
        , unk_19(0), unk_20(0), unk_21(0), unk_22(0)
        , hash_4(1611006924u)
        , rarity(2), sort_group(100)
    {
        strcpy_s(item_code,      "IP_grf_hed_hachimaki");
        strcpy_s(text_key,       "TEXT_UI_000_cmn_hed_hachimaki");
        strcpy_s(package_id,     "PAU_CUS_001");
        strcpy_s(package_sub_id, "PAU_CUS_001");
    }
};

// ── character_list entry ──────────────────────────────────────────────────────
struct CharacterEntry
{
    char     character_code[64];
    uint32_t name_hash;
    bool     is_enabled;
    bool     is_selectable;
    char     group[64];
    float    camera_offset;
    bool     is_playable;
    uint32_t sort_order;
    char     full_name_key[128];
    char     short_name_jp_key[128];
    char     short_name_key[128];
    char     origin_key[128];
    char     fighting_style_key[128];
    char     height_key[128];
    char     weight_key[128];

    CharacterEntry()
        : name_hash(0), is_enabled(true), is_selectable(true)
        , camera_offset(0.0f), is_playable(true), sort_order(0)
    {
        character_code[0]     = '\0';
        group[0]              = '\0';
        full_name_key[0]      = '\0';
        short_name_jp_key[0]  = '\0';
        short_name_key[0]     = '\0';
        origin_key[0]         = '\0';
        fighting_style_key[0] = '\0';
        height_key[0]         = '\0';
        weight_key[0]         = '\0';
    }
};

// ── customize_item_exclusive_list entries ────────────────────────────────────
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

// ── A single bin file added to the mod ───────────────────────────────────────
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
};

// ── Top-level mod data container ─────────────────────────────────────────────
struct ModData
{
    std::vector<ContentsBinData> contents;
    int selectedIndex = -1;

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
