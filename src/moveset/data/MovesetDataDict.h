#pragma once
#include <string>
#include <unordered_map>
#include <cstdint>

// Loads MovesetDatas/data.json and provides per-category dictionary lookups.
// Currently supports: requirements.
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

private:
    MovesetDataDict() = default;

    std::unordered_map<uint32_t, ReqEntry>   m_req;
    std::unordered_map<uint32_t, std::string> m_cancelExtras;
    std::unordered_map<uint32_t, PropEntry>  m_props;
    bool m_loaded = false;
};
