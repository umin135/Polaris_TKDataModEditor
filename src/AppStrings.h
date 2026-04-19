#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  AppStrings.h
//  All user-visible text and layout constants for the main menu / Home view.
//  Edit here to update sidebar labels, Home page content, credits, links, etc.
// ─────────────────────────────────────────────────────────────────────────────

namespace AppStr
{
    // ── Tool version (shown bottom-left of sidebar) ───────────────────────────
    // APPSTR_VERSION is a macro so adjacent string literals can compose it at
    // compile time (e.g. "Title - " APPSTR_VERSION).  Update only this one line.
#define APPSTR_VERSION   "v0.1.2b"
#define APPSTR_VERSION_W L"v0.1.2b"
    static constexpr const char* Version       = APPSTR_VERSION;

    // ── Sidebar header text (top of the sidebar) ─────────────────────────────
    static constexpr const char* SidebarLogoText = "TEKKEN8";
    static constexpr const char* SidebarSubText  = "TKData Mod Editor";

    // ── Sidebar layout ────────────────────────────────────────────────────────
    static constexpr float       SidebarWidth  = 200.0f;

    // ── Sidebar navigation buttons ────────────────────────────────────────────
    static constexpr const char* BtnHome       = "HOME";
    static constexpr const char* BtnFbsData    = "FBSDATA";
    static constexpr const char* BtnMoveset    = "MOVESET";
    static constexpr const char* BtnSettings   = "Settings";

    // ── Sidebar dev-mode labels (Debug build only) ────────────────────────────
    static constexpr const char* DevModeLabel  = "-- DEV MODE --";
    static constexpr const char* BtnFbsDev     = "FBSDATA (dev)";
    static constexpr const char* BtnMotbinDiff = "MOTBIN DIFF";

    // ── Home view — title / subtitle (shown when logo texture is unavailable) ─
    static constexpr const char* AppTitle      = "TEKKEN8 TKData Mod Editor";
    static constexpr const char* AppSubtitle   = "TEKKEN8 TKData Mod Editor";
    // Combined title with version suffix — use this for display rendering.
    static constexpr const char* AppTitleFull  = "TEKKEN8 TKData Mod Editor - " APPSTR_VERSION;

    // ── Home view — Supported Modules table ──────────────────────────────────
    static constexpr const char* SectionModules = "Supported Modules";

    struct ModuleEntry { const char* name; const char* desc; bool ready; };
    static constexpr ModuleEntry Modules[] = {
        { "fbsdata", "Add modded items to fbsdata (.tkmod)",                                    true  },
        { "moveset", "Moveset Extraction/Editor (.motbin / .anmbin / .stllstb / .mvl)",         true  },
        { "ghost",   "Ghost data (low priority)",                                               false },
    };

    // ── Home view — Credits ───────────────────────────────────────────────────
    static constexpr const char* SectionCredits = "Credits";

    struct CreditEntry { const char* name; const char* role; };
    static constexpr CreditEntry Credits[] = {
        { "UMIN",   "Editor development"                    },
        { "Ali",    "Game Reversing"                        },
        { "dawc17", "Game reversing / Mod Loader development" },
    };

    // ── Home view — Links ─────────────────────────────────────────────────────
    static constexpr const char* SectionLinks = "Links";

    struct LinkEntry { const char* label; const char* url; };
    static constexpr LinkEntry Links[] = {
        { "TekkenMods",      "https://tekkenmods.com/"                               },
        { "ModdingZaibatsu", "https://discord.gg/nCAeJE4z5U"                         },
        { "Github",          "https://github.com/umin135/Polaris_TKDataModEditor"    },
    };

} // namespace AppStr
