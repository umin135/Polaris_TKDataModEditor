#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>
#include "GameStatic.h"

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

    // All command entries (for building dropdown lists).
    const std::unordered_map<uint64_t, std::string>& CmdMap() const { return m_cmd; }

    // Move/anim/VFS name: Kamui hash uint32 -> string (res/kamui-hashes/data.json).
    // Call AddNames() to merge additional JSON without clearing.
    void LoadNames(const std::string& jsonPath);
    void AddNames(const std::string& jsonPath);
    const char* GetMoveName(uint32_t key) const;

    // Reverse name lookup: string -> hash (uint32).
    // Populated from m_names during LoadNames / AddNames.
    uint32_t GetHashByName(const std::string& name) const;

    // True for legacy sized placeholders (e.g. "nk0E8134F4__", "ak0A27D720___")
    // that may still appear in old extracted string blocks.
    static bool IsSizedKeyPlaceholder(const char* s);

    bool IsLoaded() const { return m_loaded; }

    // Load from embedded RCDATA resources (fallback when disk files are absent).
    // Requires resource.h defines IDR_DATA_* constants.
    void LoadFromResources();

    static const char* ParseCommand(uint64_t command);

private:
    LabelDB() = default;

    static void ParseFile(const std::string& path,
                          std::unordered_map<uint64_t, std::string>& out);
    static void ParseBuffer(const char* buf, size_t sz,
                            std::unordered_map<uint64_t, std::string>& out);

    std::unordered_map<uint64_t, std::string> m_req;
    std::unordered_map<uint64_t, std::string> m_prop;
    std::unordered_map<uint64_t, std::string> m_cmd;
    std::unordered_map<uint32_t, std::string> m_names;
    std::unordered_map<std::string, uint32_t> m_hashByName;
    bool m_loaded = false;
};
