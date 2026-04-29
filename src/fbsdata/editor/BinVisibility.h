#pragma once
#include <cstring>

// -----------------------------------------------------------------------------
//  BinVisibility.h
//  Controls which bin menu items appear in the "Contents > Add" menu.
//
//  In DEBUG builds: ALL bin items are shown regardless of these settings
//                   (same behavior as before).
//  In Release builds: only items with showRelease = true are visible.
//                     Items with showRelease = false are completely hidden
//                     (not shown at all, not even greyed out).
//
//  The supported/not-supported classification (which controls whether a bin
//  can be edited or is greyed out) is defined in FbsDataView.cpp and is
//  independent of this visibility table.
//
//  To change Release visibility of a bin, set its showRelease value below.
// -----------------------------------------------------------------------------

namespace BinVisibility {

    struct Entry {
        const char* filename;
        bool showRelease;  // true = visible in Release builds; false = hidden
    };

    // -----------------------------------------------------------------------
    //  Visibility table.
    //  Supported bins default to showRelease=true  (implemented, usable).
    //  Not-Supported bins default to showRelease=false (hidden in Release).
    // -----------------------------------------------------------------------
    static constexpr Entry k_Entries[] = {
        // -- Supported (implemented editors) --------------------------------
        { "arcade_cpu_list.bin",                       false  },
        { "area_list.bin",                             false  },
        { "assist_input_list.bin",                     false  },
        { "ball_property_list.bin",                    false  },
        { "ball_recommend_list.bin",                   false  },
        { "ball_setting_list.bin",                     false  },
        { "battle_common_list.bin",                    false  },
        { "battle_cpu_list.bin",                       false  },
        { "battle_motion_list.bin",                    false  },
        { "battle_subtitle_info.bin",                  false  },
        { "body_cylinder_data_list.bin",               false  },
        { "character_list.bin",                        false  },
        { "character_select_list.bin",                 false  },
        { "customize_item_common_list.bin",            true  },
        { "customize_item_exclusive_list.bin",         false  },
        { "customize_item_prohibit_drama_list.bin",    false  },
        { "customize_item_unique_list.bin",            true  },
        { "drama_player_start_list.bin",               false  },
        { "fate_drama_player_start_list.bin",          false  },
        { "jukebox_list.bin",                          false  },
        { "rank_list.bin",                             false  },
        { "series_list.bin",                           false  },
        { "stage_list.bin",                            true   },
        { "tam_mission_list.bin",                      false  },
        { "customize_panel_list.bin",                  false   },

        // -- Not Supported (no editor implemented) ---------------------------
        { "button_help_list.bin",                      false },
        { "button_image_list.bin",                     false },
        { "character_episode_list.bin",                false },
        { "character_panel_list.bin",                  false },
        { "chat_window_list.bin",                      false },
        { "common_dialog_details_list.bin",            false },
        { "cosmos_country_code_list.bin",              false },
        { "cosmos_language_code_game_list.bin",        false },
        { "cosmos_language_code_list.bin",             false },
        { "customize_gauge_list.bin",                  false },
        { "customize_item_acc_parameter_list.bin",     false },
        { "customize_item_color_palette_list.bin",     false },
        { "customize_item_color_slot_list.bin",        false },
        { "customize_item_shop_camera_list.bin",       false },
        { "customize_model_viewer_list.bin",           false },
        { "customize_set_list.bin",                    false },
        { "customize_shogo_bg_list.bin",               false },
        { "customize_shogo_list.bin",                  false },
        { "customize_stage_light_list.bin",            false },
        { "customize_unique_exclusion_list.bin",       false },
        { "drama_voice_change_list.bin",               false },
        { "gallery_illust_list.bin",                   false },
        { "gallery_movie_list.bin",                    false },
        { "gallery_title_list.bin",                    false },
        { "game_camera_data_list.bin",                 false },
        { "ghost_vs_ghost_battle_property_list.bin",   false },
        { "ghost_vs_ghost_property_list.bin",          false },
        { "help_dialog_list.bin",                      false },
        { "lobby_menu_help_list.bin",                  false },
        { "movie_vibration_list.bin",                  false },
        { "option_settings_list.bin",                  false },
        { "parameter_camera_data_list.bin",            false },
        { "per_fighter_basic_info_list.bin",           false },
        { "per_fighter_battle_info_list.bin",          false },
        { "per_fighter_motion_info_list.bin",          false },
        { "per_fighter_voice_info_list.bin",           false },
        { "photo_mode_list.bin",                       false },
        { "player_profile_stage_light_list.bin",       false },
        { "playing_stats_table.bin",                   false },
        { "practice_position_reset_list.bin",          false },
        { "quake_camera_data_list.bin",                false },
        { "region_list.bin",                           false },
        { "replace_text_list.bin",                     false },
        { "rom_ghost_info_list.bin",                   false },
        { "rt_drama_player_start_list.bin",            false },
        { "scene_bgm_list.bin",                        false },
        { "scene_setting_list.bin",                    false },
        { "software_keyboard_list.bin",                false },
        { "sound_parameter_list.bin",                  false },
        { "staffroll_list.bin",                        false },
        { "store_customize_item_exclusive_list.bin",   false },
        { "store_stage_light_list.bin",                false },
        { "story_battle_voice_list.bin",               false },
        { "story_iw_battle_event_list.bin",            false },
        { "story_settings_info_list.bin",              false },
        { "subtitle_list.bin",                         false },
        { "tam_battle_navi_list.bin",                  false },
        { "tam_help_dialog_list.bin",                  false },
        { "tam_message_list.bin",                      false },
        { "tam_npc_list.bin",                          false },
        { "tam_tips_command_list.bin",                 false },
        { "tips_list.bin",                             false },
        { "unlock_list.bin",                           false },
        { "vibration_pattern_list.bin",                false },
        { "yellow_book_battle_voice_list.bin",         false },
        { "yellow_book_settings_list.bin",             false },
    };

    static constexpr int k_EntryCount = static_cast<int>(sizeof(k_Entries) / sizeof(k_Entries[0]));

    // Returns true if this bin should be shown in the current build configuration.
    // In DEBUG: always returns true (all bins visible).
    // In Release: returns the showRelease flag for the bin; defaults to true if not found.
    inline bool IsVisible(const char* filename) {
#ifdef _DEBUG
        (void)filename;
        return true;
#else
        for (int i = 0; i < k_EntryCount; ++i)
            if (std::strcmp(k_Entries[i].filename, filename) == 0)
                return k_Entries[i].showRelease;
        return true;  // unknown bins default to visible
#endif
    }

} // namespace BinVisibility
