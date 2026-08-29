#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>

// -------------------------------------------------------------
//  ExternalItemIdIndex
//  Built ONCE at startup from the tkmod-manager directory. Indexes every
//  item_id / char_item_id used by the .tkmod files found there, mapping each
//  id -> the tkmod filename(s) that use it. The fbsdata editors consult this
//  to warn when an edited id collides with an id already used by another tkmod.
//
//  Scan is startup-only (cheap at render: just map lookups). Not rescanned while
//  running -- reopen the app to pick up new tkmods.
// -------------------------------------------------------------
class ExternalItemIdIndex
{
public:
    static ExternalItemIdIndex& Get();

    // Recursively scan `dir` for *.tkmod and index all item ids. No-op if dir is empty.
    void BuildFromDir(const std::string& dir);

    // Returns the filename of a tkmod (other than `selfFile`) that uses `id`,
    // or nullptr if no OTHER tkmod uses it. `selfFile` is the basename of the
    // currently-edited tkmod (pass "" when the mod is unsaved / not from the folder).
    const char* FindOwner(uint32_t id, const std::string& selfFile) const;

    bool IsBuilt() const { return m_built; }

private:
    ExternalItemIdIndex() = default;
    // id -> distinct tkmod filenames using it (built once, never mutated after)
    std::unordered_map<uint32_t, std::vector<std::string>> m_idToFiles;
    bool m_built = false;
};
