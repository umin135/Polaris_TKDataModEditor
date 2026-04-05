// AnmbinRebuild.cpp
//
// Strategy: PATCH-IN-PLACE + APPEND (not rebuild from scratch).
//
// The anmbin contains PANM animation blobs at file offsets referenced by
// pool entry animDataPtr fields.  Rebuilding the file from scratch would
// require embedding all PANM blobs and recomputing every offset — any error
// causes a Fatal Error in the game.
//
// Instead, we read the original moveset.anmbin into memory, apply patches,
// then write the buffer back:
//
//   1. characterFlags (pool entry +0x18~+0x24): set all four 32-bit flag
//      fields to 0xFFFFFFFF across every pool entry in all 6 categories.
//      Each bit represents a character that is allowed to use the animation;
//      setting all bits removes the per-character restriction so any moveset
//      can reference animations from another character's anmbin.
//
//   2. New animation embedding: for each .<ext> file on disk where its
//      CRC32 is not yet in the existing pool, we relocate that category's
//      pool entry array to the end of the buffer (appending the original
//      entries + new blank/real entries), then append new PANM blobs.  The
//      header poolCount and poolListOffset fields are updated accordingly.
//      Existing animDataPtr values in copied original entries remain valid
//      because the PANM blobs they point to are not moved.
//
//   3. moveList[0] patch: AFTER embedding (so poolHash0 includes new entries),
//      remap hash values for moves whose animation reference changed
//      (via AnimNameDB anim_N / com_N → pool index → new hash).
//
// Limitations:
//   - Move count changes (motbin adds/removes moves) are not handled here.
//     The moveList is patched for min(moves.size(), existingMoveCount) entries.
// -------------------------------------------------------------

#define NOMINMAX
#include "AnmbinRebuild.h"
#include "moveset/data/AnmbinData.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <algorithm>
#include <unordered_map>
#include <windows.h>

// -------------------------------------------------------------
//  RebuildAnmbin
// -------------------------------------------------------------

bool RebuildAnmbin(const std::string&             folderPath,
                   const AnimNameDB&               animNameDB,
                   const std::vector<ParsedMove>&  moves,
                   std::string&                    errorMsg)
{
    // --- Build path -----------------------------------------------
    std::string base = folderPath;
    if (!base.empty() && base.back() != '\\' && base.back() != '/') base += '\\';
    const std::string anmbinPath = base + "moveset.anmbin";

    // --- Read entire file into memory -----------------------------
    FILE* f = nullptr;
    if (fopen_s(&f, anmbinPath.c_str(), "rb") != 0 || !f)
    {
        errorMsg = "Cannot open moveset.anmbin for patching";
        return false;
    }

    fseek(f, 0, SEEK_END);
    long fileSize = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (fileSize < 0x98)
    {
        fclose(f);
        errorMsg = "moveset.anmbin too small";
        return false;
    }

    std::vector<uint8_t> bytes(static_cast<size_t>(fileSize));
    fread(bytes.data(), 1, bytes.size(), f);
    fclose(f);

    // --- Helpers --------------------------------------------------
    auto rdU32 = [&](size_t off) -> uint32_t {
        if (off + 4 > bytes.size()) return 0;
        uint32_t v; memcpy(&v, bytes.data() + off, 4); return v;
    };
    auto rdU64 = [&](size_t off) -> uint64_t {
        if (off + 8 > bytes.size()) return 0;
        uint64_t v; memcpy(&v, bytes.data() + off, 8); return v;
    };
    auto patchU32 = [&](size_t off, uint32_t v) {
        if (off + 4 <= bytes.size()) memcpy(bytes.data() + off, &v, 4);
    };
    auto patchU64 = [&](size_t off, uint64_t v) {
        if (off + 8 <= bytes.size()) memcpy(bytes.data() + off, &v, 8);
    };

    // --- Parse stable header fields (unchanged by embedding) ------
    const uint32_t moveListCount = rdU32(0x1C);   // per-move entries cat 0
    const uint64_t moveListOff0  = rdU64(0x68);   // moveList offset cat 0

    if (moveListOff0 == 0 || moveListCount == 0)
    {
        errorMsg = "moveset.anmbin: no moveList for cat 0";
        return false;
    }
    if (moveListOff0 + (uint64_t)moveListCount * 4 > (uint64_t)bytes.size())
    {
        errorMsg = "moveset.anmbin: moveList out of bounds";
        return false;
    }

    bool anyChanged = false;

    // --- Patch characterFlags to 0xFFFFFFFF for all pool entries (all cats) ---
    // Pool entry layout (stride 0x38):
    //   +0x00: animKey u64
    //   +0x08: animDataPtr u64
    //   +0x10: reserved u32 ×2
    //   +0x18: characterFlags1 u32  ← each bit = one character allowed to use this anim
    //   +0x1C: characterFlags2 u32
    //   +0x20: characterFlags3 u32
    //   +0x24: characterFlags4 u32
    //   +0x28: reserved u32 ×4
    // Setting all bits allows any moveset to reference animations from this anmbin,
    // bypassing the per-character validation check in the game engine.
    for (int cat = 0; cat < 6; ++cat)
    {
        uint32_t poolCount   = rdU32(0x04 + cat * 4);
        uint64_t poolListOff = rdU64(0x38 + cat * 8);
        if (poolCount == 0 || poolListOff == 0) continue;
        if (poolListOff + (uint64_t)poolCount * 0x38 > (uint64_t)bytes.size()) continue;

        static const size_t kFlagOffsets[4] = { 0x18, 0x1C, 0x20, 0x24 };
        for (uint32_t j = 0; j < poolCount; ++j)
        {
            size_t entOff = static_cast<size_t>(poolListOff) + j * 0x38;
            for (size_t fo : kFlagOffsets)
            {
                uint32_t cur; memcpy(&cur, bytes.data() + entOff + fo, 4);
                if (cur != 0xFFFFFFFFu)
                {
                    patchU32(entOff + fo, 0xFFFFFFFFu);
                    anyChanged = true;
                }
            }
        }
    }

    // --- Embed new animation files --------------------------------
    // Scan anim/<cat_folder>/ for files with the correct extension whose
    // CRC32 is not already present in the pool.
    // For each category that has new files:
    //   1. Copy the original pool entry array to the end of bytes[] (relocation).
    //   2. Append blank entries for any gap indices, then real entries for found files.
    //   3. Append each new PANM blob and set its animDataPtr in the new pool entry.
    //   4. Update header: poolCount[cat] and poolListOffset[cat].
    // Original pool entry bytes at the old offset become orphaned (harmless).
    // Existing animDataPtr values in copied entries are still valid because the
    // PANM blobs they reference have not moved.
    {
        // Build CRC-32 table (same polynomial as DoRefresh in AnimationManagerWindow).
        static uint32_t crcTable[256];
        static bool     crcInit = false;
        if (!crcInit)
        {
            for (uint32_t i = 0; i < 256; ++i)
            {
                uint32_t c = i;
                for (int j = 0; j < 8; ++j)
                    c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
                crcTable[i] = c;
            }
            crcInit = true;
        }

        struct NewPanm {
            int      cat;
            int      poolIdx;
            uint32_t hash32;
            std::vector<uint8_t> data;
        };
        std::vector<NewPanm> newPanms;

        std::string animRoot = base + "anim";
        for (int cat = 0; cat < 6; ++cat)
        {
            uint32_t    existCount = rdU32(0x04 + cat * 4);
            std::string catDir     = animRoot + "\\" + AnmbinCategoryFolder(cat);
            std::string ext        = AnmbinCategoryExt(cat);
            // Accept any filename with the correct extension.
            std::string pattern    = catDir + "\\*" + ext;

            WIN32_FIND_DATAA fd = {};
            HANDLE h = FindFirstFileA(pattern.c_str(), &fd);
            if (h == INVALID_HANDLE_VALUE) continue;

            // Build hash set of existing pool entries to detect duplicates.
            std::unordered_map<uint32_t, int> knownHashToIdx;
            {
                uint64_t plOff = rdU64(0x38 + cat * 8);
                if (plOff != 0)
                {
                    for (uint32_t j = 0; j < existCount; ++j)
                    {
                        size_t   eoff = static_cast<size_t>(plOff) + j * 0x38;
                        uint64_t ak   = rdU64(eoff);
                        knownHashToIdx[static_cast<uint32_t>(ak & 0xFFFFFFFFu)] = (int)j;
                    }
                }
            }

            // Collect all files with the correct extension whose CRC32 is not yet in pool.
            struct Candidate { std::string filename; };
            std::vector<Candidate> candidates;

            do {
                std::string fname = fd.cFileName;
                std::string fpath = catDir + "\\" + fname;

                // Quick CRC check to skip files already embedded.
                FILE* pck = nullptr;
                if (fopen_s(&pck, fpath.c_str(), "rb") != 0 || !pck) continue;
                uint32_t crcCheck = 0xFFFFFFFFu;
                {
                    uint8_t buf[4096]; size_t n;
                    while ((n = fread(buf, 1, sizeof(buf), pck)) > 0)
                        for (size_t bi = 0; bi < n; ++bi)
                            crcCheck = crcTable[(crcCheck ^ buf[bi]) & 0xFF] ^ (crcCheck >> 8);
                }
                fclose(pck);
                crcCheck ^= 0xFFFFFFFFu;

                if (knownHashToIdx.count(crcCheck)) continue; // already in pool
                candidates.push_back({ fname });
            } while (FindNextFileA(h, &fd));

            FindClose(h);

            if (candidates.empty()) continue;

            // Sort alphabetically for deterministic pool-index assignment.
            std::sort(candidates.begin(), candidates.end(),
                [](const Candidate& a, const Candidate& b) {
                    return a.filename < b.filename;
                });

            int nextAutoIdx = (int)existCount;

            for (const auto& c : candidates)
            {
                int idx = nextAutoIdx++;

                std::string fullPath = catDir + "\\" + c.filename;

                FILE* pf = nullptr;
                if (fopen_s(&pf, fullPath.c_str(), "rb") != 0 || !pf) continue;
                fseek(pf, 0, SEEK_END);
                long sz = ftell(pf);
                fseek(pf, 0, SEEK_SET);
                if (sz < 4) { fclose(pf); continue; }

                std::vector<uint8_t> panmData(static_cast<size_t>(sz));
                fread(panmData.data(), 1, panmData.size(), pf);
                fclose(pf);

                uint32_t crc = 0xFFFFFFFFu;
                for (uint8_t b : panmData)
                    crc = crcTable[(crc ^ b) & 0xFF] ^ (crc >> 8);
                crc ^= 0xFFFFFFFFu;

                newPanms.push_back({ cat, idx, crc, std::move(panmData) });
            }
        }

        if (!newPanms.empty())
        {
            // Sort by cat then poolIdx for deterministic processing.
            std::sort(newPanms.begin(), newPanms.end(),
                [](const NewPanm& a, const NewPanm& b) {
                    return a.cat != b.cat ? a.cat < b.cat : a.poolIdx < b.poolIdx;
                });

            // Snapshot original header values before we start modifying bytes[].
            // (rdU32/rdU64 read from bytes[], which we'll append to but not modify
            //  at the header region until the final header-update step.)
            uint32_t origPoolCounts[6];
            uint64_t origPoolListOffsets[6];
            for (int c = 0; c < 6; ++c)
            {
                origPoolCounts[c]      = rdU32(0x04 + c * 4);
                origPoolListOffsets[c] = rdU64(0x38 + c * 8);
            }

            // Track where each new pool entry's animDataPtr field sits in bytes[]
            // so we can patch it after appending the PANM blob.
            struct PendingPatch {
                size_t   animDataPtrOff; // offset into bytes[] of the 8-byte animDataPtr field
                uint32_t panmIdx;        // index into newPanms
            };
            std::vector<PendingPatch> patches;

            // Process cat by cat.  For each cat that has new files, relocate its
            // pool entry array and append new entries (with blank gap-fillers).
            int prevCat = -1;
            for (uint32_t ni = 0; ni < (uint32_t)newPanms.size(); ++ni)
            {
                const NewPanm& np = newPanms[ni];

                if (np.cat != prevCat)
                {
                    prevCat = np.cat;

                    // Relocate original pool array for this cat to end of bytes[].
                    // Copy to a temp buffer first to avoid aliasing when bytes[] reallocates.
                    size_t origOff  = static_cast<size_t>(origPoolListOffsets[np.cat]);
                    size_t origSize = origPoolCounts[np.cat] * 0x38;

                    std::vector<uint8_t> tmp;
                    if (origSize > 0 && origOff + origSize <= bytes.size())
                        tmp.assign(bytes.data() + origOff, bytes.data() + origOff + origSize);

                    // New pool array starts here.
                    origPoolListOffsets[np.cat] = static_cast<uint64_t>(bytes.size());
                    bytes.insert(bytes.end(), tmp.begin(), tmp.end());
                }

                // Determine the first new index for this cat that we haven't yet emitted.
                // We may need to emit blank entries for gaps before np.poolIdx.
                uint32_t nextIdx = origPoolCounts[np.cat]; // already appended this many

                // Emit blank pool entries for any gap between nextIdx and np.poolIdx.
                while (nextIdx < (uint32_t)np.poolIdx)
                {
                    uint8_t blank[0x38] = {};
                    // characterFlags in blank entries: 0xFFFFFFFF.
                    memset(blank + 0x18, 0xFF, 4);
                    memset(blank + 0x1C, 0xFF, 4);
                    memset(blank + 0x20, 0xFF, 4);
                    memset(blank + 0x24, 0xFF, 4);
                    bytes.insert(bytes.end(), blank, blank + 0x38);
                    ++nextIdx;
                }

                // Emit pool entry for this new animation (animDataPtr = 0 placeholder).
                uint8_t entry[0x38] = {};
                memcpy(entry + 0x00, &np.hash32, 4); // animKey lower 32 bits
                // animDataPtr at +0x08 stays 0 (filled in below).
                memset(entry + 0x18, 0xFF, 4); // characterFlags1
                memset(entry + 0x1C, 0xFF, 4); // characterFlags2
                memset(entry + 0x20, 0xFF, 4); // characterFlags3
                memset(entry + 0x24, 0xFF, 4); // characterFlags4

                size_t entryStart = bytes.size();
                bytes.insert(bytes.end(), entry, entry + 0x38);

                patches.push_back({ entryStart + 8, ni });
                origPoolCounts[np.cat] = (uint32_t)np.poolIdx + 1;
            }

            // Append PANM blobs and patch animDataPtr in the corresponding pool entries.
            for (const PendingPatch& pp : patches)
            {
                uint64_t panmOff = static_cast<uint64_t>(bytes.size());
                // Patch animDataPtr (8 bytes at pp.animDataPtrOff).
                memcpy(bytes.data() + pp.animDataPtrOff, &panmOff, 8);
                const auto& pd = newPanms[pp.panmIdx].data;
                bytes.insert(bytes.end(), pd.begin(), pd.end());
            }

            // Update header: poolCounts and poolListOffsets for affected cats.
            for (int c = 0; c < 6; ++c)
            {
                patchU32(0x04 + c * 4, origPoolCounts[c]);
                patchU64(0x38 + c * 8, origPoolListOffsets[c]);
            }

            anyChanged = true;
        }
    }

    // --- Build pool index → hash map (cat 0) from UPDATED bytes[] -
    // This must happen AFTER embedding so that newly added pool entries
    // (indices >= original pool count) are included in poolHash0.
    // pool entry at poolListOff0 + j * 0x38:
    //   +0x00: animKey u64  (lower 32 bits = the hash stored in moveList)
    std::vector<uint32_t> poolHash0;
    {
        uint32_t poolCount0   = rdU32(0x04);       // may have grown if cat 0 had new entries
        uint64_t poolListOff0 = rdU64(0x38);       // may have been relocated
        if (poolListOff0 != 0 && poolCount0 != 0 &&
            poolListOff0 + (uint64_t)poolCount0 * 0x38 <= (uint64_t)bytes.size())
        {
            poolHash0.resize(poolCount0);
            for (uint32_t j = 0; j < poolCount0; ++j)
            {
                size_t   entOff  = static_cast<size_t>(poolListOff0) + j * 0x38;
                uint64_t animKey = rdU64(entOff);
                poolHash0[j]     = static_cast<uint32_t>(animKey & 0xFFFFFFFFu);
            }
        }
    }

    // --- Patch moveList[0] ----------------------------------------
    // For each move i: look up name from AnimNameDB, resolve to pool hash, patch.
    //
    // Resolution order (two strategies):
    //
    //   1. CRC scan (tried first):
    //      For user-added animations (CRC32-keyed), moves[i].anim_key == CRC32 ==
    //      poolHash0[actual_insert_index].  Scan poolHash0 for this value.
    //      This correctly handles cross-character animations where the numeric suffix
    //      in the name (e.g. "anim_grf_500" → 500) is the SOURCE character's pool
    //      index, not the TARGET character's insert index.
    //
    //   2. N-based lookup (fallback when CRC scan finds nothing):
    //      For original game animations, moves[i].anim_key is an encrypted game key
    //      (not a CRC32) that never matches any poolHash0 entry.  In that case the
    //      name suffix N is the correct pool index — use poolHash0[N] directly.
    //
    //   3. Arbitrary stem without anim_/com_ prefix → CRC scan only (same as 1).
    uint32_t patchCount = static_cast<uint32_t>(moves.size());
    if (patchCount > moveListCount) patchCount = moveListCount;

    for (uint32_t i = 0; i < patchCount; ++i)
    {
        std::string name = animNameDB.AnimKeyToName(moves[i].anim_key);
        if (name.empty()) continue;

        uint32_t newHash  = 0;
        bool     resolved = false;

        // Strategy 1: CRC scan — covers user-added and cross-character animations.
        {
            uint32_t target = moves[i].anim_key;
            for (uint32_t j = 0; j < (uint32_t)poolHash0.size(); ++j)
            {
                if (poolHash0[j] == target)
                {
                    newHash  = poolHash0[j];
                    resolved = true;
                    break;
                }
            }
        }

        // Strategy 2: N-based fallback — original game animations whose anim_key
        // is an encrypted value that never appears in poolHash0.
        if (!resolved)
        {
            int N = -1;
            if (name.rfind("anim_", 0) == 0 || name.rfind("com_", 0) == 0)
            {
                size_t u = name.rfind('_');
                if (u != std::string::npos) N = atoi(name.c_str() + u + 1);
            }
            if (N >= 0 && N < (int)poolHash0.size())
            {
                newHash  = poolHash0[N];
                resolved = true;
            }
        }

        if (!resolved) continue;

        size_t   off = static_cast<size_t>(moveListOff0) + i * 4;
        uint32_t oldHash; memcpy(&oldHash, bytes.data() + off, 4);
        if (oldHash == newHash) continue;

        patchU32(off, newHash);
        anyChanged = true;
    }

    if (!anyChanged)
        return true; // nothing to write

    // --- Write back -----------------------------------------------
    if (fopen_s(&f, anmbinPath.c_str(), "wb") != 0 || !f)
    {
        errorMsg = "Cannot write patched moveset.anmbin";
        return false;
    }
    fwrite(bytes.data(), 1, bytes.size(), f);
    fclose(f);
    return true;
}
