#pragma once
#include "moveset/data/AnimNameDB.h"
#include "moveset/data/MotbinData.h"
#include <string>
#include <vector>

// -------------------------------------------------------------
//  RebuildAnmbin  --  patch-in-place strategy
//
//  Reads the existing <folderPath>/moveset.anmbin into memory,
//  applies two patches, then writes the buffer back:
//
//  1. moveList[0] (fullbody): remaps hash values for moves whose
//     anim_key maps (via AnimNameDB) to a different pool entry.
//
//  2. characterFlags (pool entry +0x18~+0x24): sets all four u32
//     flag fields to 0xFFFFFFFF in every pool entry across all
//     6 categories, removing the per-character animation restriction.
//
//  3. New animation embedding: scans anim/<cat>/ on disk for files
//     anim_N.<ext> where N >= existing pool count.  Each found file's
//     pool entry array is relocated to the end of the buffer with the
//     new entries appended; PANM blobs are appended after.  The header
//     poolCount and poolListOffset fields are updated accordingly.
//
//  File size grows only when new animations are embedded.
//
//  Returns true on success (including "nothing changed").
//  errorMsg is set and false returned on I/O or structural errors.
// -------------------------------------------------------------
bool RebuildAnmbin(const std::string&             folderPath,
                   const AnimNameDB&               animNameDB,
                   const std::vector<ParsedMove>&  moves,
                   std::string&                    errorMsg);
