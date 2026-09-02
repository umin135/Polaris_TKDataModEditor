#pragma once
// -------------------------------------------------------------
//  CinematicPaths -- reconstructs the level-sequence (camera cutscene) uasset paths a
//  character uses, from the moveset's cinematic properties:
//    0x838E "Trigger cinematic camera" -> rage (5-7) / throw (8+)   [game/ builder]
//    0x8313 "Set Drama Type" + 0x8314 "Set Drama No." -> intro/outro [demo/ builder]
//  Paths are built exactly like the game's decompiled builders (see
//  _references/CinematicSequence_Paths_RE.md). Output: <folder>/polaris/cinematic.json
//  — consumed later by the editor UI and by an external ModLoader to decide which uasset to redirect.
// -------------------------------------------------------------
#include <string>
#include <functional>

struct MotbinData;

// Resolves the season-folder bucket ("polaris" / "polaris01" / …) for a cinematic sequence, given
// its (side, index) in the game's per-character cinematic data. Provided by the extractor (reads
// live game memory). Return "" if unresolvable → caller falls back to "polaris".
//   side: 0 = rage, 1 = outro(win), 2 = intro(sta), 3 = throw   index: per-category sub-index
using SeasonFolderResolver = std::function<std::string(int side, int index)>;

// Scans the moveset's cinematic properties and writes the cinematic path JSON.
// `code` is the character code (e.g. "grl"); `folderPath` is the moveset folder.
// `resolver` (optional) supplies each sequence's real season folder from live game memory.
// `exportRoot` (optional) is a ripped cinematics dump root (…\Content\cinematics); when non-empty
// each emitted path's existence is cross-checked against it (*_exists fields).
void WriteCinematicSequencesJson(const MotbinData& data,
                                 const std::string& code,
                                 const std::string& folderPath,
                                 const SeasonFolderResolver& resolver,
                                 const std::string& exportRoot);
