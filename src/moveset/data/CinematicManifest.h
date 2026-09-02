#pragma once
// -------------------------------------------------------------
//  CinematicManifest -- in-memory model + (de)serializer for <moveset>/polaris/cinematic.json.
//  One shared serializer is used by both the extractor (builds entries from live game data) and the
//  editor GUI (edits Source/overrides + adds throws), so the on-disk format has a single source.
//  Layout: section arrays (rage / intro / outro / throw), a per-id "overrides" map (id -> custom
//  path; empty = use src), and "added_throws" (throw numbers the user added). No external JSON lib.
// -------------------------------------------------------------
#include <string>
#include <vector>

// One redirectable sequence path (one cam of a rage/throw entry, or one intro/outro).
struct CineManifestEntry {
    std::string id;            // stable key, e.g. "rage_pre_cam1p" / "throw_00_cam2p" / "intro_0"
    std::string group;         // "rage" | "throw" | "intro" | "outro"
    std::string sub;           // rage: "pre"/"finish"/"finishko"   (unused for others)
    std::string cam;           // "cam1p"/"cam2p"  (empty for intro/outro)
    int         num = -1;      // throw NN or drama no (-1 for rage)
    std::string src;           // source uasset package path (game-resolved)
    std::string overridePath;  // user redirect target ("" = use src)
    bool        hasExists = false;
    bool        exists    = false;
    bool        added     = false; // throw entry the user added (not referenced by the moveset)
};

struct CineManifest {
    bool        loaded = false;
    std::string code;
    std::string folderSource;                  // "game-runtime" | "assumed-polaris"
    std::vector<CineManifestEntry> entries;    // grouped: rage, intro, outro, throw
    std::string excludedRaw;                   // verbatim inner text of the "excluded" object ("" if none)
};

// Loads <folderPath>/polaris/cinematic.json. `loaded` is false if the file is missing/unreadable.
CineManifest LoadCineManifest(const std::string& folderPath);

// Writes the whole cinematic.json from `man` (shared by extractor + GUI). Returns false on I/O error.
bool SaveCineManifest(const std::string& folderPath, const CineManifest& man);
