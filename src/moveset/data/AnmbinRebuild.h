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
//  outAlreadyPresent (optional): set to true when the CRC32 already exists in
//  pool[cat] and embedding was skipped (byte-identical animation).
//  outMatchPoolIdx (optional): when already present, the pool[cat] index of the
//  byte-identical entry the blob collided with (-1 otherwise).
//  Returns true on success (including "already present" no-op).
// -------------------------------------------------------------
bool AddAnimToAnmbin(const std::string&             folderPath,
                     const AnimNameDB&               animNameDB,
                     const std::vector<ParsedMove>&  moves,
                     int                             cat,
                     const std::vector<uint8_t>&     panmBytes,
                     uint32_t&                       outCRC32,
                     std::string&                    errorMsg,
                     bool*                           outAlreadyPresent = nullptr,
                     int*                            outMatchPoolIdx   = nullptr);

// -------------------------------------------------------------
//  RemoveAnimFromAnmbin  --  remove one pool entry from a category
//
//  Reads moveset.anmbin, rebuilds pool[cat] without the entry at
//  poolIdx (appended at end-of-file, patch-in-place strategy),
//  updates the header, and writes back. Works for all 6 categories.
//
//  moveList[cat] slots that referenced the removed hash:
//    Fullbody -- repointed to a fallback animation (move 0's anim if it
//                isn't the removed one, else the first remaining entry).
//    others   -- cleared to 0 (the next Add reuses the first 0 slot);
//                if the category became empty, trailing 0 slots are trimmed.
//  If another pool[cat] entry still carries the same hash (legacy
//  duplicate), slots are left untouched since they remain valid.
//
//  outRemovedHash receives the animKey (low32) of the removed entry.
//  outFallbackHash / outStillPresent / outRepointed (optional) report
//  what happened to the referencing slots (fallback is 0 when cleared;
//  outRepointed counts repointed or cleared slots).
//  The PANM blob becomes orphaned bytes (not compacted).
//  Returns true on success.
// -------------------------------------------------------------
bool RemoveAnimFromAnmbin(const std::string& folderPath,
                          int                cat,
                          int                poolIdx,
                          uint32_t&          outRemovedHash,
                          std::string&       errorMsg,
                          uint32_t*          outFallbackHash = nullptr,
                          bool*              outStillPresent = nullptr,
                          int*               outRepointed    = nullptr);

// -------------------------------------------------------------
//  AssignAnimKeyInAnmbin  --  set moveList[cat][keyIdx] = crc32
//
//  moveList[cat] is the category's key table (slot -> pool hash):
//  Hand keys, Facial keys, etc. Patches one u32 so that key index
//  <keyIdx> references the animation with <crc32>. When keyIdx is past
//  the end (or the table is empty), the table is relocated to the end
//  of the file and zero-extended up to keyIdx. Not for Fullbody (cat 0),
//  whose table is indexed by move and patched from the motbin instead.
// -------------------------------------------------------------
bool AssignAnimKeyInAnmbin(const std::string& folderPath,
                           int                cat,
                           int                keyIdx,
                           uint32_t           crc32,
                           std::string&       errorMsg);

// Hand (cat 1) shorthand for AssignAnimKeyInAnmbin.
bool AssignHandKeyInAnmbin(const std::string& folderPath,
                           int                keyIdx,
                           uint32_t           crc32,
                           std::string&       errorMsg);
