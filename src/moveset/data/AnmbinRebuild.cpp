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
// Move count changes:
//   When the motbin has more moves than the original anmbin moveList (e.g. a
//   duplicate was appended), the moveList is relocated to end of file and
//   extended with zero-filled slots so that AnmbinRebuild can patch the new
//   entries.  The game uses anmbin_body_idx (= move index) to look up
//   moveList[anmbin_body_idx], so every move index must have an entry.
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

    // --- Parse moveList header fields (updated if list is extended) ------
    uint32_t moveListCount = rdU32(0x1C);   // per-move entries cat 0
    uint64_t moveListOff0  = rdU64(0x68);   // moveList offset cat 0

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

    // --- Extend moveList when motbin has more moves than original anmbin --------
    // The game resolves each move's animation as: moveList[anmbin_body_idx].
    // anmbin_body_idx == move index for every move.  New moves at indices
    // >= moveListCount need new entries; relocate the list and zero-fill them.
    {
        const uint32_t newMoveCount = static_cast<uint32_t>(moves.size());
        if (newMoveCount > moveListCount)
        {
            const size_t origOff  = static_cast<size_t>(moveListOff0);
            const size_t origSize = moveListCount * 4;

            std::vector<uint8_t> tmp;
            if (origSize > 0 && origOff + origSize <= bytes.size())
                tmp.assign(bytes.data() + origOff, bytes.data() + origOff + origSize);
            else
                tmp.resize(origSize, 0);

            tmp.resize(tmp.size() + (newMoveCount - moveListCount) * 4, 0);

            const uint64_t newOff = static_cast<uint64_t>(bytes.size());
            bytes.insert(bytes.end(), tmp.begin(), tmp.end());

            patchU32(0x1C, newMoveCount);
            patchU64(0x68, newOff);
            moveListCount = newMoveCount;
            moveListOff0  = newOff;

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
    // moveListCount was extended above; all move indices are now covered.
    const uint32_t patchCount = static_cast<uint32_t>(moves.size());

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

// =============================================================================
//  Shared CRC32 helper (same polynomial as above)
// =============================================================================

static uint32_t ComputeCRC32(const uint8_t* data, size_t len)
{
    static uint32_t table[256];
    static bool     init = false;
    if (!init)
    {
        for (uint32_t i = 0; i < 256; ++i)
        {
            uint32_t c = i;
            for (int j = 0; j < 8; ++j)
                c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
            table[i] = c;
        }
        init = true;
    }
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < len; ++i)
        crc = table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    return crc ^ 0xFFFFFFFFu;
}

// =============================================================================
//  Shared anmbin read / write helpers
// =============================================================================

static bool ReadAnmbinBytes(const std::string& path, std::vector<uint8_t>& out, std::string& err)
{
    FILE* f = nullptr;
    if (fopen_s(&f, path.c_str(), "rb") != 0 || !f)
    { err = "Cannot open moveset.anmbin: " + path; return false; }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz < 0x98) { fclose(f); err = "moveset.anmbin too small"; return false; }
    out.resize(static_cast<size_t>(sz));
    fread(out.data(), 1, out.size(), f);
    fclose(f);
    return true;
}

static bool WriteAnmbinBytes(const std::string& path, const std::vector<uint8_t>& data, std::string& err)
{
    FILE* f = nullptr;
    if (fopen_s(&f, path.c_str(), "wb") != 0 || !f)
    { err = "Cannot write moveset.anmbin"; return false; }
    fwrite(data.data(), 1, data.size(), f);
    fclose(f);
    return true;
}

// Helper: patch moveList[cat] for a given bytes[] buffer, using animNameDB + moves.
// cat == 0 is the only category currently supported for moveList patching.
static void PatchMoveList(std::vector<uint8_t>& bytes,
                          const AnimNameDB& animNameDB,
                          const std::vector<ParsedMove>& moves,
                          int patchCat)
{
    if (patchCat != 0) return; // only Fullbody moveList is currently patched

    auto rdU32 = [&](size_t o) { uint32_t v; memcpy(&v, bytes.data()+o, 4); return v; };
    auto rdU64 = [&](size_t o) { uint64_t v; memcpy(&v, bytes.data()+o, 8); return v; };
    auto wrU32 = [&](size_t o, uint32_t v) { if(o+4<=bytes.size()) memcpy(bytes.data()+o,&v,4); };

    uint32_t moveListCount = rdU32(0x1C);
    uint64_t moveListOff   = rdU64(0x68);
    if (moveListOff == 0 || moveListCount == 0) return;

    // Build pool hash array for cat 0 from updated bytes[]
    uint32_t poolCount0   = rdU32(0x04);
    uint64_t poolListOff0 = rdU64(0x38);
    std::vector<uint32_t> poolHash0;
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

    const uint32_t patchCount = (uint32_t)moves.size();
    for (uint32_t i = 0; i < patchCount && i < moveListCount; ++i)
    {
        std::string name = animNameDB.AnimKeyToName(moves[i].anim_key);
        if (name.empty()) continue;

        uint32_t newHash  = 0;
        bool     resolved = false;

        // CRC scan first
        uint32_t target = moves[i].anim_key;
        for (uint32_t j = 0; j < (uint32_t)poolHash0.size(); ++j)
        {
            if (poolHash0[j] == target) { newHash = poolHash0[j]; resolved = true; break; }
        }
        // N-based fallback
        if (!resolved)
        {
            int N = -1;
            if (name.rfind("anim_", 0) == 0 || name.rfind("com_", 0) == 0)
            {
                size_t u = name.rfind('_');
                if (u != std::string::npos) N = atoi(name.c_str() + u + 1);
            }
            if (N >= 0 && N < (int)poolHash0.size()) { newHash = poolHash0[N]; resolved = true; }
        }
        if (!resolved) continue;

        size_t   off = static_cast<size_t>(moveListOff) + i * 4;
        uint32_t old; memcpy(&old, bytes.data()+off, 4);
        if (old != newHash) wrU32(off, newHash);
    }
}

// =============================================================================
//  AddAnimToAnmbin
// =============================================================================

bool AddAnimToAnmbin(const std::string&             folderPath,
                     const AnimNameDB&               animNameDB,
                     const std::vector<ParsedMove>&  moves,
                     int                             cat,
                     const std::vector<uint8_t>&     panmBytes,
                     uint32_t&                       outCRC32,
                     std::string&                    errorMsg)
{
    if (cat < 0 || cat >= 6) { errorMsg = "Invalid category index"; return false; }
    if (panmBytes.empty())   { errorMsg = "Empty PANM data";        return false; }

    // --- Compute CRC32 ---
    outCRC32 = ComputeCRC32(panmBytes.data(), panmBytes.size());

    // --- Build path and read ---
    std::string base = folderPath;
    if (!base.empty() && base.back() != '\\' && base.back() != '/') base += '\\';
    const std::string anmbinPath = base + "moveset.anmbin";

    std::vector<uint8_t> bytes;
    if (!ReadAnmbinBytes(anmbinPath, bytes, errorMsg)) return false;

    auto rdU32 = [&](size_t o) -> uint32_t { uint32_t v; memcpy(&v,bytes.data()+o,4); return v; };
    auto rdU64 = [&](size_t o) -> uint64_t { uint64_t v; memcpy(&v,bytes.data()+o,8); return v; };
    auto wrU32 = [&](size_t o, uint32_t v){ if(o+4<=bytes.size()) memcpy(bytes.data()+o,&v,4); };
    auto wrU64 = [&](size_t o, uint64_t v){ if(o+8<=bytes.size()) memcpy(bytes.data()+o,&v,8); };

    bool anyChanged = false;

    // --- characterFlags patch (all cats) ---
    static const size_t kFlagOff[4] = { 0x18, 0x1C, 0x20, 0x24 };
    for (int c = 0; c < 6; ++c)
    {
        uint32_t cnt  = rdU32(0x04 + c * 4);
        uint64_t plOff = rdU64(0x38 + c * 8);
        if (cnt == 0 || plOff == 0) continue;
        if (plOff + (uint64_t)cnt * 0x38 > (uint64_t)bytes.size()) continue;
        for (uint32_t j = 0; j < cnt; ++j)
        {
            size_t base2 = static_cast<size_t>(plOff) + j * 0x38;
            for (size_t fo : kFlagOff)
            {
                uint32_t cur; memcpy(&cur, bytes.data()+base2+fo, 4);
                if (cur != 0xFFFFFFFFu) { wrU32(base2+fo, 0xFFFFFFFFu); anyChanged = true; }
            }
        }
    }

    // --- Embed new PANM blob ---
    {
        uint32_t origCount  = rdU32(0x04 + cat * 4);
        uint64_t origPlOff  = rdU64(0x38 + cat * 8);
        size_t   origSize   = (size_t)origCount * 0x38;

        // Relocate original pool list to end of file
        std::vector<uint8_t> origPool;
        if (origSize > 0 && origPlOff != 0 &&
            static_cast<size_t>(origPlOff) + origSize <= bytes.size())
            origPool.assign(bytes.data() + origPlOff, bytes.data() + origPlOff + origSize);

        uint64_t newPlOff = static_cast<uint64_t>(bytes.size());
        bytes.insert(bytes.end(), origPool.begin(), origPool.end());

        // Append new pool entry (animDataPtr filled in below)
        uint8_t entry[0x38] = {};
        memcpy(entry + 0x00, &outCRC32, 4);          // animKey low32 = CRC32
        memset(entry + 0x18, 0xFF, 4);               // characterFlags1
        memset(entry + 0x1C, 0xFF, 4);               // characterFlags2
        memset(entry + 0x20, 0xFF, 4);               // characterFlags3
        memset(entry + 0x24, 0xFF, 4);               // characterFlags4
        size_t entryOff = bytes.size();
        bytes.insert(bytes.end(), entry, entry + 0x38);

        // Append PANM blob, patch animDataPtr
        uint64_t panmOff = static_cast<uint64_t>(bytes.size());
        memcpy(bytes.data() + entryOff + 0x08, &panmOff, 8);
        bytes.insert(bytes.end(), panmBytes.begin(), panmBytes.end());

        // Update header
        uint32_t newCount = origCount + 1;
        wrU32(0x04 + cat * 4, newCount);
        wrU64(0x38 + cat * 8, newPlOff);

        anyChanged = true;
    }

    // --- Extend moveList[0] if motbin has more moves ---
    if (cat == 0)
    {
        uint32_t moveListCount = rdU32(0x1C);
        uint64_t moveListOff0  = rdU64(0x68);
        const uint32_t newMoveCount = static_cast<uint32_t>(moves.size());
        if (moveListOff0 != 0 && newMoveCount > moveListCount)
        {
            size_t origOff  = static_cast<size_t>(moveListOff0);
            size_t origSize = moveListCount * 4;
            std::vector<uint8_t> tmp;
            if (origSize > 0 && origOff + origSize <= bytes.size())
                tmp.assign(bytes.data() + origOff, bytes.data() + origOff + origSize);
            else
                tmp.resize(origSize, 0);
            tmp.resize(tmp.size() + (newMoveCount - moveListCount) * 4, 0);
            uint64_t newOff = static_cast<uint64_t>(bytes.size());
            bytes.insert(bytes.end(), tmp.begin(), tmp.end());
            wrU32(0x1C, newMoveCount);
            wrU64(0x68, newOff);
        }
    }

    // --- Patch moveList[0] ---
    PatchMoveList(bytes, animNameDB, moves, cat);

    if (!anyChanged) return true;

    return WriteAnmbinBytes(anmbinPath, bytes, errorMsg);
}

// =============================================================================
//  RemoveAnimFromAnmbin
// =============================================================================

bool RemoveAnimFromAnmbin(const std::string& folderPath,
                          int                cat,
                          int                poolIdx,
                          uint32_t&          outRemovedHash,
                          std::string&       errorMsg)
{
    if (cat < 0 || cat >= 6) { errorMsg = "Invalid category index"; return false; }

    std::string base = folderPath;
    if (!base.empty() && base.back() != '\\' && base.back() != '/') base += '\\';
    const std::string anmbinPath = base + "moveset.anmbin";

    std::vector<uint8_t> bytes;
    if (!ReadAnmbinBytes(anmbinPath, bytes, errorMsg)) return false;

    auto rdU32 = [&](size_t o) -> uint32_t { uint32_t v; memcpy(&v,bytes.data()+o,4); return v; };
    auto rdU64 = [&](size_t o) -> uint64_t { uint64_t v; memcpy(&v,bytes.data()+o,8); return v; };
    auto wrU32 = [&](size_t o, uint32_t v){ if(o+4<=bytes.size()) memcpy(bytes.data()+o,&v,4); };
    auto wrU64 = [&](size_t o, uint64_t v){ if(o+8<=bytes.size()) memcpy(bytes.data()+o,&v,8); };

    uint32_t origCount = rdU32(0x04 + cat * 4);
    uint64_t origPlOff = rdU64(0x38 + cat * 8);

    if ((uint32_t)poolIdx >= origCount)
    { errorMsg = "poolIdx out of range"; return false; }

    // --- Read removed hash ---
    size_t removedEntOff = static_cast<size_t>(origPlOff) + (size_t)poolIdx * 0x38;
    if (removedEntOff + 0x38 > bytes.size())
    { errorMsg = "Pool entry out of bounds"; return false; }
    uint64_t removedAnimKey; memcpy(&removedAnimKey, bytes.data() + removedEntOff, 8);
    outRemovedHash = static_cast<uint32_t>(removedAnimKey & 0xFFFFFFFFu);

    // --- Build new pool list (all entries except poolIdx) ---
    std::vector<uint8_t> newPool;
    newPool.reserve((origCount - 1) * 0x38);
    for (uint32_t j = 0; j < origCount; ++j)
    {
        if ((int)j == poolIdx) continue;
        size_t eoff = static_cast<size_t>(origPlOff) + j * 0x38;
        if (eoff + 0x38 > bytes.size()) break;
        newPool.insert(newPool.end(), bytes.data()+eoff, bytes.data()+eoff+0x38);
    }

    // --- Append new pool list at end of file ---
    uint64_t newPlOff = static_cast<uint64_t>(bytes.size());
    bytes.insert(bytes.end(), newPool.begin(), newPool.end());

    // --- Update header ---
    uint32_t newCount = origCount - 1;
    wrU32(0x04 + cat * 4, newCount);
    wrU64(0x38 + cat * 8, newPlOff);

    // --- Zero moveList[cat] entries that referenced the removed hash ---
    {
        uint32_t mlCount = rdU32(0x1C + cat * 4);
        uint64_t mlOff   = rdU64(0x68 + cat * 8);
        if (mlOff != 0 && mlCount != 0 &&
            static_cast<size_t>(mlOff) + mlCount * 4 <= bytes.size())
        {
            for (uint32_t i = 0; i < mlCount; ++i)
            {
                size_t   off = static_cast<size_t>(mlOff) + i * 4;
                uint32_t h;  memcpy(&h, bytes.data()+off, 4);
                if (h == outRemovedHash) wrU32(off, 0u);
            }
        }
    }

    return WriteAnmbinBytes(anmbinPath, bytes, errorMsg);
}
