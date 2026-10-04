#pragma once
// -------------------------------------------------------------
//  CinematicPaths -- reconstructs the level-sequence (camera cutscene) uasset paths a
//  character uses, from the moveset's cinematic properties:
//    0x838E "Trigger cinematic camera" -> rage (5-7) / throw (8+)   [game/ builder]
//    0x8313 "Set Drama Type" + 0x8314 "Set Drama No." -> intro/outro [demo/ builder]
//  Paths are built exactly like the game's decompiled builders (see
//  _references/CinematicSequence_Paths_RE.md). Output: <folder>/polaris/cinematic.json
//  — consumed later by the editor UI and by an external ModLoader to decide which uasset to redirect.
//
//  Season folder / existence resolution (ResolveCineSeq), in order:
//    1. live-game season table: slot out of range or value < 0 -> no sequence (always applied)
//    2. CinematicSeqDB (res/cinematics/data.json): authoritative folder; not listed -> no asset
//    3. fallback (DB unavailable): season from the live table
//    4. neither available -> unresolved (nothing is guessed)
// -------------------------------------------------------------
#include "moveset/data/CinematicManifest.h"
#include <string>

struct MotbinData;

// side ids (match the game's per-character cinematic data): rage=0, outro=1, intro=2, throw=3.
enum { CINE_SIDE_RAGE = 0, CINE_SIDE_OUTRO = 1, CINE_SIDE_INTRO = 2, CINE_SIDE_THROW = 3 };

enum class CineSeqState { Ok, EmptyInGame, NoAsset, Unresolved };

struct CineSeqResolve {
    CineSeqState state    = CineSeqState::Unresolved;
    std::string  folder;            // season folder when state == Ok
    bool         verified = false;  // folder + existence confirmed by CinematicSeqDB
};

// path tails below the season folder (filenames match the game / asset dump exactly)
std::string CineRageTail (const std::string& code, const std::string& sub, const char* cam);
std::string CineThrowTail(const std::string& code, int nn, const char* cam);
std::string CineDemoTail (const std::string& code, const char* tok, int no); // tok: "sta" / "win"

// Resolves one sequence. base: "game" / "demo"; (side, index) address the live season table.
CineSeqResolve ResolveCineSeq(const CineSeasonTable& table, const char* base,
                              int side, int index, const std::string& tail);

// Scans the moveset's cinematic properties and writes the cinematic path JSON.
// `code` is the character code (e.g. "grl"); `folderPath` is the moveset folder.
// `seasons` = live-game season table (valid=false if the game data couldn't be read).
// `exportRoot` (optional) = ripped cinematics dump root; used only for the fallback path
// (no sequence DB) to cross-check existence (*_exists fields).
// Returns a short status suffix for the extractor's message (warns when unresolved).
std::string WriteCinematicSequencesJson(const MotbinData& data,
                                        const std::string& code,
                                        const std::string& folderPath,
                                        const CineSeasonTable& seasons,
                                        const std::string& exportRoot);
