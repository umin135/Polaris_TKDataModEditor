#pragma once

// ─────────────────────────────────────────────────────────────────────────────
//  FieldNames — editor display name mappings for each schema field
//
//  Each array is indexed by schema field id (id_0, id_1, ...).
//  Editing these names changes every column header, TSV header, and any
//  other label that references them — JSON keys (id_0, id_1, ...) are
//  unaffected and remain stable regardless of changes here.
//
//  Comment format per entry:
//    "display name",  // id: N | type | (original schema name if different)
// ─────────────────────────────────────────────────────────────────────────────

namespace FieldNames
{
    // ── customize_item_common_list.bin ────────────────────────────────────────
    //    Entry type : CustomizeItemCommonEntry   (26 fields, id 0..25)
    constexpr const char* CommonItem[26] = {
        "item_id",         // id:  0 | uint32
        "item_no",         // id:  1 | uint32
        "item_code",       // id:  2 | string
        "hash_0",          // id:  3 | uint32
        "hash_1",          // id:  4 | uint32
        "text_key",        // id:  5 | string
        "package_id",      // id:  6 | string
        "package_sub_id",  // id:  7 | string
        "unk_8",           // id:  8 | uint32  (unknown)
        "shop_sort_id",    // id:  9 | uint32
        "is_enabled",      // id: 10 | bool
        "unk_11",          // id: 11 | uint32  (unknown)
        "price",           // id: 12 | uint32
        "unk_13",          // id: 13 | bool    (unknown)
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
        "unk_24",          // id: 24 | uint32
        "sort_group",      // id: 25 | uint32
    };
    constexpr int CommonItemCount = 26;

    // ── character_list.bin ────────────────────────────────────────────────────
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

    // ── customize_item_exclusive_list.bin — rule entry ────────────────────────
    //    Entry type : CustomizeExclusiveRuleEntry   (4 fields, id 0..3)
    //    Used by: rule_entries (id_0), group_rule_entries (id_2), set_rule_entries (id_4)
    constexpr const char* ExclusiveRule[4] = {
        "item_id",      // id: 0 | uint32
        "hash",         // id: 1 | uint32
        "link_type",    // id: 2 | uint32
        "ref_item_id",  // id: 3 | uint32
    };
    constexpr int ExclusiveRuleCount = 4;

    // ── customize_item_exclusive_list.bin — pair entry ────────────────────────
    //    Entry type : CustomizeExclusivePairEntry   (3 fields, id 0..2)
    //    Used by: pair_entries (id_1), group_pair_entries (id_3)
    constexpr const char* ExclusivePair[3] = {
        "item_id_a",  // id: 0 | uint32
        "item_id_b",  // id: 1 | uint32
        "flag",       // id: 2 | uint32
    };
    constexpr int ExclusivePairCount = 3;

    // ── customize_item_exclusive_list.bin — sub-vector names ─────────────────
    //    These label the 5 array tabs shown in the exclusive list editor.
    //    (Parent table field ids: id_0..id_4)
    constexpr const char* ExclusiveArrays[5] = {
        "rule_entries",        // id: 0 | [ExclusiveRule]
        "pair_entries",        // id: 1 | [ExclusivePair]
        "group_rule_entries",  // id: 2 | [ExclusiveRule]
        "group_pair_entries",  // id: 3 | [ExclusivePair]
        "set_rule_entries",    // id: 4 | [ExclusiveRule]
    };
}
