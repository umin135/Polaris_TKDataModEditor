#pragma once
#include "ModData.h"
#include <cstring>

// -----------------------------------------------------------------------------
//  DefaultValues -- sample/reference default values for new entries.
//
//  Used when "+ Add Entry" is clicked and the list is empty.
//  All factory functions return a fully populated entry with sensible defaults.
//  Edit the bodies below to change what a blank new entry looks like.
// -----------------------------------------------------------------------------

namespace DefaultValues
{
    // -- customize_item_common_list -------------------------------------------
    inline CustomizeItemCommonEntry CommonEntry()
    {
        CustomizeItemCommonEntry e;
        e.item_id      = 20006101;
        e.item_no      = 1;
        strcpy_s(e.item_code,      "IP_grf_bdf_customNew");
        e.hash_0       = 2802412287u;   // KamuiHash("GRF")
        e.hash_1       = 952745790u;    // KamuiHash("bdf") -- Entire body
        strcpy_s(e.text_key,       "TEXT_000_UI_STORYMENU_COMMON_004");
        strcpy_s(e.package_id,     "PAU_CUS_001");
        strcpy_s(e.package_sub_id, "PAU_CUS_001");
        e.unk_8        = 0;
        e.shop_sort_id = 1000000;
        e.is_enabled   = true;
        e.unk_11       = 0;
        e.price        = 30000;
        e.unk_13       = false;
        e.category_no  = 102;
        e.hash_2       = 3229833922u;
        e.unk_16       = false;
        e.unk_17       = 0;
        e.hash_3       = 2609503483u;
        e.unk_19       = 0;
        e.unk_20       = 0;
        e.unk_21       = 0;
        e.unk_22       = 0;
        e.hash_4       = 1611006924u;
        e.rarity       = 2;
        e.sort_group   = 100;
        return e;
    }

    // -- customize_item_unique_list ------------------------------------------
    inline CustomizeItemUniqueEntry UniqueEntry()
    {
        CustomizeItemUniqueEntry e;
        e.char_item_id   = 10006101;                                        // A=1(unique), XX=00, YY=00, ZZZ=001
        strcpy_s(e.asset_name,       "IP_grf_bdf_customNew");
        e.character_hash = 2802412287u;
        e.hash_1         = 952745790u;
        strcpy_s(e.text_key,         "TEXT_000_UI_STORYMENU_COMMON_004");
        strcpy_s(e.extra_text_key_1, "PAU_CUS_001");
        strcpy_s(e.extra_text_key_2, "PAU_CUS_001");
        e.flag_7         = false;
        e.unk_8          = 0;
        e.flag_9         = false;
        e.unk_10         = 0;
        e.price          = 0;
        e.unk_12         = 0;
        e.unk_13         = 0;
        e.hash_2         = 0;
        e.flag_15        = false;
        e.unk_16         = 0;
        e.hash_3         = 0;
        e.unk_18         = 0;
        e.unk_19         = 0;
        e.unk_20         = 0;
        e.unk_21         = 0;
        return e;
    }

} // namespace DefaultValues
