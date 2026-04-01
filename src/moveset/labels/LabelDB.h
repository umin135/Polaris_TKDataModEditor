#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>

// -------------------------------------------------------------
//  LabelDB
//  Loads InterfaceData txt files (editorRequirements, editorProperties,
//  editorCommands) and provides ID -> human-readable label lookups.
//
//  File format: one entry per line, "id,label"
//  IDs can be decimal or hex (0x...).
//
//  Usage:
//    LabelDB::Get().Load("path/to/interfacedata/");
//    const char* lbl = LabelDB::Get().Req(43);  // "On Hit"
// -------------------------------------------------------------
class LabelDB
{
public:
    static LabelDB& Get();

    // Load all three files from the given directory.
    // Safe to call multiple times; subsequent calls reload.
    void Load(const std::string& dir);

    // Requirement ID -> label, e.g. 43 -> "On Hit"
    // Returns nullptr if ID not found.
    const char* Req(uint32_t id) const;

    // ExtraMoveProperty ID -> label, e.g. 0x800B -> "Ground Ripple"
    const char* Prop(uint32_t id) const;

    // Cancel command bitmask -> label, e.g. 0x40 -> "f"
    // Returns nullptr if ID not found.
    const char* Cmd(uint64_t cmd) const;

    // Move name: name_key uint32 -> move name string, e.g. 391423 -> "Hw_66RK"
    // Loaded from name_keys.json.  Call AddNames() to merge additional JSON files
    // (e.g. supplement_name_keys.json) without clearing the existing map.
    void LoadNames(const std::string& jsonPath);
    void AddNames(const std::string& jsonPath);
    const char* GetMoveName(uint32_t key) const;

    // Anim name: anim_key uint32 -> animation name string (or placeholder)
    // Loaded from anim_keys.json.
    void LoadAnimNames(const std::string& jsonPath);
    const char* GetAnimName(uint32_t key) const;

    // Character ID -> name string, e.g. 6 -> "JIN"
    // Character ID -> code string, e.g. 6 -> "ant"
    // Loaded from characterList.txt (id,name,code format).
    // Returns nullptr if ID not found.
    const char* CharaName(uint32_t id) const;
    const char* CharaCode(uint32_t id) const;

    bool IsLoaded() const { return m_loaded; }

private:
    LabelDB() = default;

    static void ParseFile(const std::string& path,
                          std::unordered_map<uint64_t, std::string>& out);

    std::unordered_map<uint64_t, std::string> m_req;
    std::unordered_map<uint64_t, std::string> m_prop;
    std::unordered_map<uint64_t, std::string> m_cmd;
    std::unordered_map<uint32_t, std::string> m_names;
    std::unordered_map<uint32_t, std::string> m_animNames;
    std::unordered_map<uint32_t, std::string> m_charas;
    std::unordered_map<uint32_t, std::string> m_charasCodes;
    bool m_loaded = false;
};
