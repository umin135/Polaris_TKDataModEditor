#pragma once
#include "moveset/data/AnmbinData.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>

// -------------------------------------------------------------
//  AnimNameDB  --  persistent name <-> motbin_anim_key mapping
//
//  Stored in <movesetFolder>/.tkedit/anim_names.json :
//    {"anim_0":2963988539,"anim_1":1353056581,...}
//
//  "anim_N" = Fullbody pool index N in moveset.anmbin
//  value    = motbin anim_key (decrypted uint32)
//
//  Build flow (once, on first edit):
//    anmbin.moveList[0][i] (hash)  +  motbin.moves[i].anim_key
//    → hash→key map
//    → pool[0][j] hash → key  →  "anim_j" → key
// -------------------------------------------------------------
class AnimNameDB {
public:
    // Load from existing .tkedit/anim_names.json.
    // Returns true if the file exists and is non-empty.
    bool Load(const std::string& folderPath);

    // Build from anmbin + motbin anim_key array, then save.
    // charaCode (e.g. "grf") is used as a prefix: "anim_grf_N".
    // Returns true if the database was built and saved successfully.
    bool BuildAndSave(const std::string& folderPath,
                      const AnmbinData&             anmbin,
                      const std::vector<uint32_t>&  motbinAnimKeys,
                      const std::string&            charaCode = {});

    bool IsLoaded() const { return m_loaded; }

    // motbin anim_key → "anim_N".  Returns "" if not found.
    std::string AnimKeyToName(uint32_t animKey) const;

    // "anim_N" → motbin anim_key.  Returns false if not found.
    bool NameToAnimKey(const std::string& name, uint32_t& outKey) const;

    // Add a single name→key mapping and persist to disk immediately.
    // No-op if the name is already mapped to the same key. Clears any stale
    // reverse mapping (old name of this key / old key of this name) so the
    // bidirectional maps stay consistent. Returns false if the save fails.
    bool AddEntry(const std::string& folderPath,
                  const std::string& name, uint32_t key);

    // Remove the mapping for a motbin anim_key (both directions) and persist.
    // Returns true if nothing to remove or the save succeeds.
    bool RemoveKey(const std::string& folderPath, uint32_t key);

    // Rename oldName -> newName for the same key and persist.
    // Fails if oldName is unknown, or newName is already used by a different key.
    bool Rename(const std::string& folderPath,
                const std::string& oldName, const std::string& newName);

    // True if the name is already mapped.
    bool HasName(const std::string& name) const { return m_nameToKey.count(name) > 0; }

    // Returns `base` if unused, otherwise base_1, base_2, ... until unused.
    std::string MakeUniqueName(const std::string& base) const;

    // Remove name<->key mappings whose key is NOT in validKeys -- orphaned entries
    // left in the JSON after their animation was removed from the anmbin. Persists
    // only if anything was removed. Returns the number of entries removed.
    int PruneToValidKeys(const std::string& folderPath,
                         const std::unordered_set<uint32_t>& validKeys);

private:
    bool Save(const std::string& jsonPath);
    static std::string JsonPath(const std::string& folderPath);

    bool m_loaded = false;
    std::unordered_map<uint32_t, std::string> m_keyToName; // motbin_key → "anim_N"
    std::unordered_map<std::string, uint32_t> m_nameToKey; // "anim_N"  → motbin_key
};
