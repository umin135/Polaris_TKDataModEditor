#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>
#include <utility>

// -------------------------------------------------------------
//  FbsDataDict
//  Loads data/fbsdatas/data.json and provides dictionaries for:
//    - character_id  : uint32 id -> character name  (XX field of item_id)
//    - customize_item_type : uint32 id -> type name  (YY field of item_id)
//
//  Loaded before LabelDB so it can serve as the primary source
//  for character ID -> name lookups.
//
//  Usage:
//    FbsDataDict::Get().Load("path/to/fbsdatas/data.json");
//    const char* name = FbsDataDict::Get().CharName(8); // "KAZUYA"
// -------------------------------------------------------------
class FbsDataDict
{
public:
    static FbsDataDict& Get();

    void Load(const std::string& jsonPath);
    void LoadFromResources(); // IDR_DATA_FBSDICT fallback

    bool IsLoaded() const { return m_loaded; }

    // character_id dict: uint32 id -> name
    const std::unordered_map<uint32_t, std::string>& GetCharMap() const { return m_chars; }
    const char* CharName(uint32_t id) const;

    // character_code dict: uint32 id -> short code (e.g. "grl" for KAZUYA)
    const char* CharaCode(uint32_t id) const;

    // customize_item_type dict: uint32 id -> name
    const std::unordered_map<uint32_t, std::string>& GetTypeMap() const { return m_types; }
    const char* TypeName(uint32_t id) const;

    // Sorted views for dropdown rendering (sorted by id ascending).
    std::vector<std::pair<uint32_t, std::string>> SortedChars() const;
    std::vector<std::pair<uint32_t, std::string>> SortedTypes() const;

private:
    FbsDataDict() = default;
    void ParseJson(const char* buf, size_t sz);

    std::unordered_map<uint32_t, std::string> m_chars;
    std::unordered_map<uint32_t, std::string> m_codes;
    std::unordered_map<uint32_t, std::string> m_types;
    bool m_loaded = false;
};
