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
    uint32_t LoadCount() const { return m_loadCount; }

    // character_id dict: uint32 id -> name
    const std::unordered_map<uint32_t, std::string>& GetCharMap() const { return m_chars; }
    const char* CharName(uint32_t id) const;

    // character_code dict: uint32 id -> short code (e.g. "grl" for KAZUYA)
    const char* CharaCode(uint32_t id) const;

    // Computes hash_0 / character_hash for a given character id via KamuiHash(code.toUpperCase()).
    // Returns UINT32_MAX if the character code is unknown.
    uint32_t CharHash(uint32_t id) const;

    // customize_item_type dict: uint32 id -> name
    const std::unordered_map<uint32_t, std::string>& GetTypeMap() const { return m_types; }
    const char* TypeName(uint32_t id) const;

    // customize_item_type short code, e.g. "hed" for Head
    const char* TypeCode(uint32_t id) const;

    // hash_1 for a given type id: KamuiHash(code).  Returns UINT32_MAX if not found.
    uint32_t TypeHash(uint32_t id) const;

    // Reverse hash lookups: hash -> short code string.
    // CharHashToCode: hash -> uppercase code, e.g. 2802412287 -> "GRF"
    // TypeHashToCode: hash -> lowercase code, e.g.  952745790 -> "bdf"
    const char* CharHashToCode(uint32_t hash) const;
    const char* TypeHashToCode(uint32_t hash) const;
    const std::unordered_map<uint32_t, std::string>& GetCharHashCodeMap() const { return m_charHashToCode; }
    const std::unordered_map<uint32_t, std::string>& GetTypeHashCodeMap() const { return m_typeHashToCode; }

    // Reverse id lookups: hash -> numeric id (for item_id assembly).
    // Returns UINT32_MAX if hash is not found.
    uint32_t CharHashToId(uint32_t hash) const;
    uint32_t TypeHashToId(uint32_t hash) const;

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
    std::unordered_map<uint32_t, std::string>  m_types;
    std::unordered_map<uint32_t, std::string>  m_typeCodes;
    std::unordered_map<uint32_t, uint32_t>     m_typeHashes;
    std::unordered_map<uint32_t, std::string>  m_charHashToCode;  // hash -> uppercase code
    std::unordered_map<uint32_t, std::string>  m_typeHashToCode;  // hash -> type code
    std::unordered_map<uint32_t, uint32_t>     m_charHashToId;    // hash -> character_id
    std::unordered_map<uint32_t, uint32_t>     m_typeHashToId;    // hash -> type_id
    uint32_t m_loadCount = 0;
    std::unordered_set<uint32_t>               m_gameIds;
    bool m_loaded = false;
};
