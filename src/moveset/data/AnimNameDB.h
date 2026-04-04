#pragma once
#include "moveset/data/AnmbinData.h"
#include <string>
#include <vector>
#include <unordered_map>

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
    // Returns true if the database was built and saved successfully.
    bool BuildAndSave(const std::string& folderPath,
                      const AnmbinData&             anmbin,
                      const std::vector<uint32_t>&  motbinAnimKeys);

    bool IsLoaded() const { return m_loaded; }

    // motbin anim_key → "anim_N".  Returns "" if not found.
    std::string AnimKeyToName(uint32_t animKey) const;

    // "anim_N" → motbin anim_key.  Returns false if not found.
    bool NameToAnimKey(const std::string& name, uint32_t& outKey) const;

    // Add a single name→key mapping and persist to disk immediately.
    // No-op if the name is already mapped to the same key.
    // Returns false if the save fails.
    bool AddEntry(const std::string& folderPath,
                  const std::string& name, uint32_t key);

private:
    bool Save(const std::string& jsonPath);
    static std::string JsonPath(const std::string& folderPath);

    bool m_loaded = false;
    std::unordered_map<uint32_t, std::string> m_keyToName; // motbin_key → "anim_N"
    std::unordered_map<std::string, uint32_t> m_nameToKey; // "anim_N"  → motbin_key
};
