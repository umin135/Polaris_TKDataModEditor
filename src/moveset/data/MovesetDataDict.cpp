// MovesetDataDict.cpp
// Loads data/MovesetDatas/data.json and exposes per-category lookups.
#include "MovesetDataDict.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

// ---------------------------------------------------------------------------
//  Singleton
// ---------------------------------------------------------------------------

MovesetDataDict& MovesetDataDict::Get()
{
    static MovesetDataDict s_instance;
    return s_instance;
}

// ---------------------------------------------------------------------------
//  Minimal JSON helpers
// ---------------------------------------------------------------------------

// Returns the content of the first JSON string value for the given key
// within json[from..to), or "" if not found.
// Handles only simple string values (no escaped-quote support needed here).
static std::string JsString(const std::string& json,
                             const char* key,
                             size_t from, size_t to)
{
    std::string searchKey = std::string("\"") + key + "\"";
    size_t k = json.find(searchKey, from);
    if (k == std::string::npos || k >= to) return {};
    k += searchKey.size();
    size_t colon = json.find(':', k);
    if (colon == std::string::npos || colon >= to) return {};
    size_t q1 = json.find('"', colon + 1);
    if (q1 == std::string::npos || q1 >= to) return {};
    // Walk forward handling \" escapes
    size_t q2 = q1 + 1;
    while (q2 < to) {
        if (json[q2] == '\\') { q2 += 2; continue; }
        if (json[q2] == '"')  break;
        ++q2;
    }
    if (q2 >= to) return {};
    // Unescape basic sequences (\n \t \\ \")
    std::string raw = json.substr(q1 + 1, q2 - q1 - 1);
    std::string out;
    out.reserve(raw.size());
    for (size_t i = 0; i < raw.size(); ++i) {
        if (raw[i] == '\\' && i + 1 < raw.size()) {
            char c = raw[++i];
            if      (c == 'n')  out += '\n';
            else if (c == 't')  out += '\t';
            else if (c == '"')  out += '"';
            else if (c == '\\') out += '\\';
            else { out += '\\'; out += c; }
        } else {
            out += raw[i];
        }
    }
    return out;
}

// Finds the index of the closing brace that matches the opening brace at 'open'.
// Returns std::string::npos if not found.
static size_t MatchingBrace(const std::string& json, size_t open)
{
    int depth = 0;
    for (size_t i = open; i < json.size(); ++i) {
        if      (json[i] == '{') ++depth;
        else if (json[i] == '}') { if (--depth == 0) return i; }
    }
    return std::string::npos;
}

// ---------------------------------------------------------------------------
//  Load
// ---------------------------------------------------------------------------

void MovesetDataDict::Load(const std::string& path)
{
    FILE* f = nullptr;
    fopen_s(&f, path.c_str(), "rb");
    if (!f) return;

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    rewind(f);
    if (sz <= 0) { fclose(f); return; }

    std::string json(static_cast<size_t>(sz), '\0');
    fread(&json[0], 1, static_cast<size_t>(sz), f);
    fclose(f);

    // ── requirements section ─────────────────────────────────────────────
    m_req.clear();

    size_t secPos = json.find("\"requirements\"");
    if (secPos != std::string::npos)
    {
        size_t secOpen = json.find('{', secPos + 14);
        if (secOpen != std::string::npos)
        {
            size_t secClose = MatchingBrace(json, secOpen);
            if (secClose == std::string::npos) secClose = json.size();

            size_t pos = secOpen + 1;
            while (pos < secClose)
            {
                // Find next quoted key (the req value as a decimal string)
                size_t q1 = json.find('"', pos);
                if (q1 == std::string::npos || q1 >= secClose) break;
                size_t q2 = json.find('"', q1 + 1);
                if (q2 == std::string::npos || q2 >= secClose) break;

                std::string keyStr = json.substr(q1 + 1, q2 - q1 - 1);

                // Find the value object opening brace
                size_t entOpen = json.find('{', q2 + 1);
                if (entOpen == std::string::npos || entOpen >= secClose) break;
                size_t entClose = MatchingBrace(json, entOpen);
                if (entClose == std::string::npos || entClose > secClose) break;

                uint32_t reqVal = static_cast<uint32_t>(std::atoi(keyStr.c_str()));
                ReqEntry entry;
                entry.param     = JsString(json, "param",     entOpen, entClose);
                entry.condition = JsString(json, "Condition", entOpen, entClose);
                entry.tooltip   = JsString(json, "ToolTip",   entOpen, entClose);
                m_req[reqVal]   = std::move(entry);

                pos = entClose + 1;
            }
        }
    }

    m_loaded = true;
}

// ---------------------------------------------------------------------------
//  Lookup
// ---------------------------------------------------------------------------

const MovesetDataDict::ReqEntry* MovesetDataDict::GetReq(uint32_t req) const
{
    auto it = m_req.find(req);
    return (it != m_req.end()) ? &it->second : nullptr;
}
