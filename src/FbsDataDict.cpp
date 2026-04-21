// FbsDataDict.cpp
// Loads data/fbsdatas/data.json — character_id and customize_item_type dicts.
#include "FbsDataDict.h"
#include "resource.h"
#define NOMINMAX
#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>

// ---------------------------------------------------------------------------
//  Singleton
// ---------------------------------------------------------------------------

FbsDataDict& FbsDataDict::Get()
{
    static FbsDataDict s_instance;
    return s_instance;
}

// ---------------------------------------------------------------------------
//  Lookup
// ---------------------------------------------------------------------------

const char* FbsDataDict::CharName(uint32_t id) const
{
    auto it = m_chars.find(id);
    return it != m_chars.end() ? it->second.c_str() : nullptr;
}

const char* FbsDataDict::TypeName(uint32_t id) const
{
    auto it = m_types.find(id);
    return it != m_types.end() ? it->second.c_str() : nullptr;
}

const char* FbsDataDict::CharaCode(uint32_t id) const
{
    auto it = m_codes.find(id);
    return it != m_codes.end() ? it->second.c_str() : nullptr;
}

std::vector<std::pair<uint32_t, std::string>> FbsDataDict::SortedChars() const
{
    std::vector<std::pair<uint32_t, std::string>> v(m_chars.begin(), m_chars.end());
    std::sort(v.begin(), v.end(), [](const auto& a, const auto& b){ return a.first < b.first; });
    return v;
}

std::vector<std::pair<uint32_t, std::string>> FbsDataDict::SortedTypes() const
{
    std::vector<std::pair<uint32_t, std::string>> v(m_types.begin(), m_types.end());
    std::sort(v.begin(), v.end(), [](const auto& a, const auto& b){ return a.first < b.first; });
    return v;
}

// ---------------------------------------------------------------------------
//  JSON parser  (section-aware, line-by-line)
//
//  Recognises two top-level sections: "character_id" and "customize_item_type".
//  Within each section, parses "numeric_key": "value" pairs.
// ---------------------------------------------------------------------------

void FbsDataDict::ParseJson(const char* buf, size_t sz)
{
    enum class Section { None, Chars, Codes, Types };
    Section sec = Section::None;

    const char* end = buf + sz;
    while (buf < end)
    {
        // Read one line
        const char* lineEnd = buf;
        while (lineEnd < end && *lineEnd != '\n') ++lineEnd;
        std::string line(buf, lineEnd);
        buf = (lineEnd < end) ? lineEnd + 1 : end;

        // Trim trailing CR
        while (!line.empty() && (line.back() == '\r' || line.back() == '\n'))
            line.pop_back();

        // Detect section headers
        if (line.find("\"character_id\"") != std::string::npos)
        { sec = Section::Chars; continue; }
        if (line.find("\"character_code\"") != std::string::npos)
        { sec = Section::Codes; continue; }
        if (line.find("\"customize_item_type\"") != std::string::npos)
        { sec = Section::Types; continue; }

        // Closing brace resets section
        if (line.find('}') != std::string::npos)
        { sec = Section::None; continue; }

        if (sec == Section::None) continue;

        // Parse "key": "value" on this line
        size_t q1 = line.find('"');
        if (q1 == std::string::npos) continue;
        size_t q2 = line.find('"', q1 + 1);
        if (q2 == std::string::npos) continue;
        size_t q3 = line.find('"', q2 + 1);
        if (q3 == std::string::npos) continue;
        size_t q4 = line.find('"', q3 + 1);
        if (q4 == std::string::npos) continue;

        std::string keyStr = line.substr(q1 + 1, q2 - q1 - 1);
        std::string valStr = line.substr(q3 + 1, q4 - q3 - 1);
        if (keyStr.empty() || valStr.empty()) continue;

        // Key must be a non-negative integer
        bool allDigits = true;
        for (char c : keyStr) { if (c < '0' || c > '9') { allDigits = false; break; } }
        if (!allDigits) continue;

        try {
            uint32_t key = static_cast<uint32_t>(std::stoull(keyStr));
            if      (sec == Section::Chars) m_chars[key] = valStr;
            else if (sec == Section::Codes) m_codes[key] = valStr;
            else                            m_types[key] = valStr;
        } catch (...) {}
    }
}

// ---------------------------------------------------------------------------
//  Load from file
// ---------------------------------------------------------------------------

void FbsDataDict::Load(const std::string& jsonPath)
{
    FILE* f = nullptr;
    fopen_s(&f, jsonPath.c_str(), "rb");
    if (!f) return;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0) { fclose(f); return; }
    std::string buf(static_cast<size_t>(sz), '\0');
    fread(&buf[0], 1, buf.size(), f);
    fclose(f);

    m_chars.clear();
    m_codes.clear();
    m_types.clear();
    ParseJson(buf.data(), buf.size());
    m_loaded = !m_chars.empty();
}

// ---------------------------------------------------------------------------
//  Load from embedded RCDATA resource (IDR_DATA_FBSDICT)
// ---------------------------------------------------------------------------

void FbsDataDict::LoadFromResources()
{
    HRSRC   hRes  = FindResource(nullptr, MAKEINTRESOURCE(IDR_DATA_FBSDICT), RT_RCDATA);
    if (!hRes) return;
    HGLOBAL hGlob = LoadResource(nullptr, hRes);
    if (!hGlob) return;
    const char* data = static_cast<const char*>(LockResource(hGlob));
    DWORD       sz   = SizeofResource(nullptr, hRes);
    if (!data || sz == 0) return;

    m_chars.clear();
    m_codes.clear();
    m_types.clear();
    ParseJson(data, static_cast<size_t>(sz));
    m_loaded = !m_chars.empty();
}
