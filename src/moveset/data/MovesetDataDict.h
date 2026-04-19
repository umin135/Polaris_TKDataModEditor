#pragma once
#include <string>
#include <unordered_map>
#include <cstdint>

// Loads MovesetDatas/data.json and provides per-category dictionary lookups.
// Currently supports: requirements.
// TODO: Refactor this in a way where ReqEntry and PropEntry are easily interchangable.
// This is needed because a requirements array can also store an extraprop — any req that's > 0x8000 isn't a req, it's an extraprop.
// So there should be a single "entry" struct that can be used for both reqs and extraprops, and the dictionary should just be a map of uint32_t → entry.
// Or, there should be a single calling function which then internally checks if the value is a req or an extraprop and then looks it up in the correct dictionary,
// so that the caller doesn't have to care about this distinction at all.
class MovesetDataDict
{
public:
    struct ReqEntry {
        std::string param;
        std::string condition;
        std::string tooltip;
    };

    struct PropEntry {
        std::string param;
        std::string function;
        std::string tooltip;
    };

    static MovesetDataDict& Get();

    // Load (or reload) from data.json. Safe to call multiple times.
    void Load(const std::string& dataJsonPath);

    // Returns nullptr if req value has no entry in the dictionary.
    const ReqEntry* GetReq(uint32_t req) const;

    // cancel_extras: simple value → description string.
    // Returns nullptr if value has no entry in the dictionary.
    const char* GetCancelExtra(uint32_t val) const;

    // properties: value → PropEntry (function name, param desc, tooltip).
    // Returns nullptr if value has no entry in the dictionary.
    const PropEntry* GetPropEntry(uint32_t val) const;

    bool IsLoaded() const { return m_loaded; }

    const char* GetParamLabel(uint32_t reqOrPropId, uint32_t index, uint32_t param) const;

private:
    MovesetDataDict() = default;

    std::unordered_map<uint32_t, ReqEntry>   m_req;
    std::unordered_map<uint32_t, std::string> m_cancelExtras;
    std::unordered_map<uint32_t, PropEntry>  m_props;
    bool m_loaded = false;
};
