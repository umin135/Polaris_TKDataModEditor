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

// -------------------------------------------------------------
//  AddAnimToAnmbin  --  embed a single PANM blob from memory
//
//  Reads moveset.anmbin, checks whether the blob's CRC32 is
//  already in pool[cat], then appends the new pool entry and
//  PANM blob, patches characterFlags, extends + patches the
//  Fullbody moveList (same as RebuildAnmbin), and writes back.
//
//  outCRC32 is set to the computed CRC32 of panmBytes.
//  Returns true on success (including "already present" no-op).
// -------------------------------------------------------------
bool AddAnimToAnmbin(const std::string&             folderPath,
                     const AnimNameDB&               animNameDB,
                     const std::vector<ParsedMove>&  moves,
                     int                             cat,
                     const std::vector<uint8_t>&     panmBytes,
                     uint32_t&                       outCRC32,
                     std::string&                    errorMsg);

// -------------------------------------------------------------
//  RemoveAnimFromAnmbin  --  remove one pool entry from a category
//
//  Reads moveset.anmbin, rebuilds pool[cat] without the entry at
//  poolIdx (appended at end-of-file, patch-in-place strategy),
//  updates the header, zeros any moveList[cat] hashes that matched
//  the removed entry, and writes back.
//
//  outRemovedHash receives the animKey (low32) of the removed entry.
//  The PANM blob becomes orphaned bytes (not compacted).
//  Returns true on success.
// -------------------------------------------------------------
bool RemoveAnimFromAnmbin(const std::string& folderPath,
                          int                cat,
                          int                poolIdx,
                          uint32_t&          outRemovedHash,
                          std::string&       errorMsg);

// -------------------------------------------------------------
//  AssignHandKeyInAnmbin  --  set moveList[1][keyIdx] = crc32
//
//  Directly patches one u32 in the Hand (cat=1) moveList so that
//  hand key index <keyIdx> references the animation with <crc32>.
//  Returns true on success; false if keyIdx is out of range.
// -------------------------------------------------------------
bool AssignHandKeyInAnmbin(const std::string& folderPath,
                           int                keyIdx,
                           uint32_t           crc32,
                           std::string&       errorMsg);
