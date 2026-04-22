// FbsDataDict.cpp
// Loads data/fbsdatas/data.json — character_id and customize_item_type dicts.
#include "FbsDataDict.h"
#include "moveset/data/KamuiHash.h"
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

const char* FbsDataDict::TypeCode(uint32_t id) const
{
    auto it = m_typeCodes.find(id);
    return it != m_typeCodes.end() ? it->second.c_str() : nullptr;
}

uint32_t FbsDataDict::TypeHash(uint32_t id) const
{
    auto it = m_typeHashes.find(id);
    return it != m_typeHashes.end() ? it->second : UINT32_MAX;
}

const char* FbsDataDict::CharaCode(uint32_t id) const
{
    auto it = m_codes.find(id);
    return it != m_codes.end() ? it->second.c_str() : nullptr;
}

uint32_t FbsDataDict::CharHash(uint32_t id) const
{
    auto it = m_hashes.find(id);
    return it != m_hashes.end() ? it->second : UINT32_MAX;
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
    enum class Section { None, Characters, Types, GameIds };
    Section sec = Section::None;

    // Extract unquoted uint32 after "key": N  (returns UINT32_MAX on failure)
    auto findUint = [](const std::string& s, const char* key) -> uint32_t {
        std::string pat = std::string("\"") + key + "\"";
        size_t p = s.find(pat);
        if (p == std::string::npos) return UINT32_MAX;
        size_t c = s.find(':', p + pat.size());
        if (c == std::string::npos) return UINT32_MAX;
        ++c;
        while (c < s.size() && (s[c] == ' ' || s[c] == '\t')) ++c;
        if (c >= s.size() || !(s[c] >= '0' && s[c] <= '9')) return UINT32_MAX;
        return static_cast<uint32_t>(std::atoi(s.c_str() + c));
    };

    // Extract quoted string after "key": "value"
    auto findStr = [](const std::string& s, const char* key) -> std::string {
        std::string pat = std::string("\"") + key + "\"";
        size_t p = s.find(pat);
        if (p == std::string::npos) return {};
        size_t c = s.find(':', p + pat.size());
        if (c == std::string::npos) return {};
        size_t q1 = s.find('"', c + 1);
        if (q1 == std::string::npos) return {};
        size_t q2 = s.find('"', q1 + 1);
        if (q2 == std::string::npos) return {};
        return s.substr(q1 + 1, q2 - q1 - 1);
    };

    const char* end = buf + sz;
    while (buf < end)
    {
        const char* lineEnd = buf;
        while (lineEnd < end && *lineEnd != '\n') ++lineEnd;
        std::string line(buf, lineEnd);
        buf = (lineEnd < end) ? lineEnd + 1 : end;

        while (!line.empty() && (line.back() == '\r' || line.back() == '\n'))
            line.pop_back();

        // Section headers
        if (line.find("\"characters\"") != std::string::npos)
        { sec = Section::Characters; continue; }
        if (line.find("\"customize_item_type\"") != std::string::npos)
        { sec = Section::Types; continue; }
        if (line.find("\"game_item_ids\"") != std::string::npos)
        { sec = Section::GameIds; continue; }

        // End of array sections
        if ((sec == Section::Characters || sec == Section::GameIds) &&
            line.find(']') != std::string::npos)
        { sec = Section::None; continue; }

        // End of types object
        if (sec == Section::Types && line.find('}') != std::string::npos)
        { sec = Section::None; continue; }

        if (sec == Section::None) continue;

        if (sec == Section::GameIds)
        {
            // Supports: plain integers and "start-end" ranges (quoted or unquoted)
            const char* p = line.c_str();
            const char* e = p + line.size();
            while (p < e) {
                while (p < e && (*p < '0' || *p > '9')) ++p;
                if (p >= e) break;
                uint32_t start = 0;
                while (p < e && *p >= '0' && *p <= '9') { start = start * 10 + (uint32_t)(*p++ - '0'); }
                if (p < e && *p == '-') {
                    ++p;
                    uint32_t end = 0;
                    while (p < e && *p >= '0' && *p <= '9') { end = end * 10 + (uint32_t)(*p++ - '0'); }
                    for (uint32_t v = start; v <= end; ++v) m_gameIds.insert(v);
                } else {
                    if (start > 0) m_gameIds.insert(start);
                }
            }
        }
        else if (sec == Section::Characters)
        {
            // Parse: { "id": N, "name": "...", "code": "...", "hash": N }
            uint32_t id = findUint(line, "id");
            if (id == UINT32_MAX) continue;
            std::string name = findStr(line, "name");
            std::string code = findStr(line, "code");
            uint32_t    hash = findUint(line, "hash");
            if (!name.empty()) m_chars[id]  = std::move(name);
            if (!code.empty()) m_codes[id]  = std::move(code);
            if (hash != UINT32_MAX) m_hashes[id] = hash;
        }
        else // Types: "N": "value"
        {
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

            bool allDigits = true;
            for (char c : keyStr) { if (c < '0' || c > '9') { allDigits = false; break; } }
            if (!allDigits) continue;

            try {
                uint32_t key = static_cast<uint32_t>(std::stoull(keyStr));
                // Extract short code from "Name (code)" — text inside last parentheses
                std::string code;
                size_t p1 = valStr.rfind('(');
                size_t p2 = valStr.rfind(')');
                if (p1 != std::string::npos && p2 != std::string::npos && p2 > p1 + 1)
                    code = valStr.substr(p1 + 1, p2 - p1 - 1);
                m_types[key] = std::move(valStr);
                if (!code.empty()) {
                    m_typeHashes[key] = static_cast<uint32_t>(
                        KamuiHash::Compute(code.c_str()));
                    m_typeCodes[key] = std::move(code);
                }
            } catch (...) {}
        }
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
    m_hashes.clear();
    m_types.clear();
    m_typeCodes.clear();
    m_typeHashes.clear();
    m_gameIds.clear();
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
    m_hashes.clear();
    m_types.clear();
    m_typeCodes.clear();
    m_typeHashes.clear();
    m_gameIds.clear();
    ParseJson(data, static_cast<size_t>(sz));
    m_loaded = !m_chars.empty();
}
