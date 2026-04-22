#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
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

    // character hash: uint32 id -> hash value (hash_0 / character_hash field)
    // Returns UINT32_MAX if not found.
    uint32_t CharHash(uint32_t id) const;

    // customize_item_type dict: uint32 id -> name
    const std::unordered_map<uint32_t, std::string>& GetTypeMap() const { return m_types; }
    const char* TypeName(uint32_t id) const;

    // customize_item_type short code, e.g. "hed" for Head
    const char* TypeCode(uint32_t id) const;

    // hash_1 for a given type id: KamuiHash(code).  Returns UINT32_MAX if not found.
    uint32_t TypeHash(uint32_t id) const;

    // Sorted views for dropdown rendering (sorted by id ascending).
    std::vector<std::pair<uint32_t, std::string>> SortedChars() const;
    std::vector<std::pair<uint32_t, std::string>> SortedTypes() const;

    // Returns true if id exists in the base game's item list.
    bool IsGameItemId(uint32_t id) const { return m_gameIds.count(id) > 0; }

private:
    FbsDataDict() = default;
    void ParseJson(const char* buf, size_t sz);

    std::unordered_map<uint32_t, std::string>  m_chars;
    std::unordered_map<uint32_t, std::string>  m_codes;
    std::unordered_map<uint32_t, uint32_t>     m_hashes;
    std::unordered_map<uint32_t, std::string>  m_types;
    std::unordered_map<uint32_t, std::string>  m_typeCodes;
    std::unordered_map<uint32_t, uint32_t>     m_typeHashes;
    std::unordered_set<uint32_t>               m_gameIds;
    bool m_loaded = false;
};
