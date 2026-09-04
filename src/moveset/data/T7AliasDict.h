#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// T7 → T8 alias tables from data/MovesetDatas/t7_aliases.json
class T7AliasDict {
public:
    static T7AliasDict& Get();

    void Load(const std::string& jsonPath);
    void LoadFromResources();
    // Disk candidates (res/ / data/ MovesetDatas) then embedded RCDATA.
    void EnsureLoaded();
    bool IsLoaded() const { return m_loaded; }

    // Requirement / extraprop ID: returns true if mapped; outAlias set.
    // Missing entry → false (caller zeros id for extraprops / leaves req unmapped carefully).
    bool MapRequirement(uint32_t t7Id, uint32_t& outAlias) const;

    // Character ID: mapped T8 id, or kPlaceholderCharId (999) if unmapped.
    uint32_t MapCharacterId(uint32_t t7Id) const;
    bool IsCharacterIdReq(uint32_t t7ReqId) const;
    bool IsSoundProp(uint32_t propId) const;

    // Cancels
    uint64_t MapCancelCommand(uint64_t command) const;

    // Hitbox byte remap (identity if unknown)
    uint8_t MapHitbox(uint8_t t7Byte) const;

    // Sound param aliases: key = T7 value bits; fills up to 5 params
    bool MapSoundParams(uint32_t t7Value,
                        uint32_t& v0, uint32_t& v1, uint32_t& v2,
                        uint32_t& v3, uint32_t& v4) const;

private:
    T7AliasDict() = default;
    void Parse(const std::string& json);

    std::unordered_map<uint32_t, uint32_t> m_reqAlias;       // t7 → t8
    std::unordered_map<uint32_t, uint32_t> m_charIds;        // t7 → t8
    std::unordered_set<uint32_t>           m_charIdReqs;
    std::unordered_set<uint32_t>           m_soundProps;
    std::unordered_map<uint64_t, uint64_t>  m_cancelCmds;     // 0x800b → 0x8012
    uint64_t m_inputSeqStart = 0x800D;
    uint64_t m_inputSeqDelta = 7;
    std::unordered_map<uint32_t, uint32_t> m_hitbox;
    struct SoundParams { uint32_t v[5]; };
    std::unordered_map<uint32_t, SoundParams> m_soundParams;
    bool m_loaded = false;
};
