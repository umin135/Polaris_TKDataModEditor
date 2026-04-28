// LabelDB.cpp
#include "moveset/labels/LabelDB.h"
#include "resource.h"
#include <windows.h>
#include <fstream>
#include <sstream>
#include <cstdlib>

// -------------------------------------------------------------
//  Singleton
// -------------------------------------------------------------

LabelDB& LabelDB::Get()
{
    static LabelDB s_instance;
    return s_instance;
}

// -------------------------------------------------------------
//  ParseFile  ("id,label" per line, id = decimal or 0x hex)
// -------------------------------------------------------------

void LabelDB::ParseFile(const std::string& path,
                        std::unordered_map<uint64_t, std::string>& out)
{
    std::ifstream f(path);
    if (!f.is_open()) return;

    std::string line;
    while (std::getline(f, line))
    {
        if (line.empty() || line[0] == '#') continue;

        // Find first comma
        size_t comma = line.find(',');
        if (comma == std::string::npos) continue;

        std::string idStr  = line.substr(0, comma);
        std::string label  = line.substr(comma + 1);

        // Trim whitespace from id
        while (!idStr.empty() && (idStr.front() == ' ' || idStr.front() == '\t'))
            idStr.erase(idStr.begin());
        while (!idStr.empty() && (idStr.back() == ' ' || idStr.back() == '\r' || idStr.back() == '\n'))
            idStr.pop_back();
        // Trim whitespace from label
        while (!label.empty() && (label.back() == '\r' || label.back() == '\n'))
            label.pop_back();

        if (idStr.empty()) continue;

        uint64_t id = 0;
        try
        {
            if (idStr.size() > 2 &&
                idStr[0] == '0' && (idStr[1] == 'x' || idStr[1] == 'X'))
                id = std::stoull(idStr, nullptr, 16);
            else
                id = std::stoull(idStr, nullptr, 10);
        }
        catch (...) { continue; }

        out[id] = std::move(label);
    }
}

// -------------------------------------------------------------
//  Load
// -------------------------------------------------------------

void LabelDB::Load(const std::string& dir)
{
    std::string base = dir;
    if (!base.empty() && base.back() != '\\' && base.back() != '/')
        base += '\\';

    m_req.clear();
    m_prop.clear();
    m_cmd.clear();

    ParseFile(base + "editorRequirements.txt", m_req);
    ParseFile(base + "editorProperties.txt",   m_prop);
    ParseFile(base + "editorCommands.txt",     m_cmd);

    m_loaded = (!m_req.empty() || !m_prop.empty() || !m_cmd.empty());
}

// -------------------------------------------------------------
//  Lookup helpers
// -------------------------------------------------------------

const char* LabelDB::Req(uint32_t id) const
{
    auto it = m_req.find(static_cast<uint64_t>(id));
    return it != m_req.end() ? it->second.c_str() : nullptr;
}

const char* LabelDB::Prop(uint32_t id) const
{
    auto it = m_prop.find(static_cast<uint64_t>(id));
    return it != m_prop.end() ? it->second.c_str() : nullptr;
}

const char* LabelDB::Cmd(uint64_t cmd) const
{
    auto it = m_cmd.find(cmd);
    return it != m_cmd.end() ? it->second.c_str() : nullptr;
}

// -------------------------------------------------------------
//  LoadNames / GetMoveName  (name_keys.json)
//
//  Format: flat JSON object  { "decimal_key": "move_name", ... }
//  No full JSON parser needed -- just scan for "key": "value" pairs.
// -------------------------------------------------------------

// -------------------------------------------------------------
//  ParseNameJson  -- shared parser for flat {decimal_key: name} JSON
// -------------------------------------------------------------

static void ParseNameJson(const std::string& jsonPath,
                          std::unordered_map<uint32_t, std::string>& out,
                          bool clearFirst)
{
    if (clearFirst) out.clear();
    std::ifstream f(jsonPath);
    if (!f.is_open()) return;

    std::string line;
    while (std::getline(f, line))
    {
        // Find: "NNNNN": "name"
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

        try
        {
            uint32_t key = static_cast<uint32_t>(std::stoull(keyStr));
            out[key] = std::move(valStr);
        }
        catch (...) {}
    }
}

void LabelDB::LoadNames(const std::string& jsonPath)
{
    ParseNameJson(jsonPath, m_names, /*clearFirst=*/true);
}

void LabelDB::AddNames(const std::string& jsonPath)
{
    ParseNameJson(jsonPath, m_names, /*clearFirst=*/false);
}

const char* LabelDB::GetMoveName(uint32_t key) const
{
    auto it = m_names.find(key);
    return it != m_names.end() ? it->second.c_str() : nullptr;
}

// -------------------------------------------------------------
//  LoadAnimNames / GetAnimName  (anim_keys.json)
//
//  Same flat JSON format as name_keys.json.
//  Values are real anim name strings (for the 4 keys that overlap
//  with name_keys.json) or sized placeholders for all others.
// -------------------------------------------------------------

void LabelDB::LoadAnimNames(const std::string& jsonPath)
{
    ParseNameJson(jsonPath, m_animNames, /*clearFirst=*/true);
}

const char* LabelDB::GetAnimName(uint32_t key) const
{
    auto it = m_animNames.find(key);
    return it != m_animNames.end() ? it->second.c_str() : nullptr;
}

// -------------------------------------------------------------
//  ParseBuffer  (memory-based counterpart to ParseFile)
//  "id,label" per line, same rules as ParseFile.
// -------------------------------------------------------------

void LabelDB::ParseBuffer(const char* buf, size_t sz,
                          std::unordered_map<uint64_t, std::string>& out)
{
    const char* end = buf + sz;
    while (buf < end)
    {
        // Find end of line
        const char* lineEnd = buf;
        while (lineEnd < end && *lineEnd != '\n') ++lineEnd;

        std::string line(buf, lineEnd);
        buf = (lineEnd < end) ? lineEnd + 1 : end;

        // Strip trailing \r
        while (!line.empty() && (line.back() == '\r' || line.back() == '\n'))
            line.pop_back();

        if (line.empty() || line[0] == '#') continue;

        size_t comma = line.find(',');
        if (comma == std::string::npos) continue;

        std::string idStr = line.substr(0, comma);
        std::string label = line.substr(comma + 1);

        // Trim idStr
        while (!idStr.empty() && (idStr.front() == ' ' || idStr.front() == '\t'))
            idStr.erase(idStr.begin());
        while (!idStr.empty() && (idStr.back() == ' ' || idStr.back() == '\r'))
            idStr.pop_back();

        if (idStr.empty()) continue;

        uint64_t id = 0;
        try
        {
            if (idStr.size() > 2 && idStr[0] == '0' && (idStr[1] == 'x' || idStr[1] == 'X'))
                id = std::stoull(idStr, nullptr, 16);
            else
                id = std::stoull(idStr, nullptr, 10);
        }
        catch (...) { continue; }

        out[id] = std::move(label);
    }
}

// -------------------------------------------------------------
//  ParseNameJsonBuffer  (memory-based counterpart to ParseNameJson)
// -------------------------------------------------------------

void LabelDB::ParseNameJsonBuffer(const char* buf, size_t sz,
                                  std::unordered_map<uint32_t, std::string>& out,
                                  bool clearFirst)
{
    if (clearFirst) out.clear();
    const char* end = buf + sz;
    while (buf < end)
    {
        const char* lineEnd = buf;
        while (lineEnd < end && *lineEnd != '\n') ++lineEnd;

        std::string line(buf, lineEnd);
        buf = (lineEnd < end) ? lineEnd + 1 : end;

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

        try
        {
            uint32_t key = static_cast<uint32_t>(std::stoull(keyStr));
            out[key] = std::move(valStr);
        }
        catch (...) {}
    }
}

// -------------------------------------------------------------
//  Helper: load a Win32 RCDATA resource into a (data, size) pair.
//  Returns {nullptr, 0} if not found.
// -------------------------------------------------------------

static std::pair<const char*, size_t> GetResourceData(int id)
{
    HRSRC   hRes  = FindResource(nullptr, MAKEINTRESOURCE(id), RT_RCDATA);
    if (!hRes) return { nullptr, 0 };
    HGLOBAL hGlob = LoadResource(nullptr, hRes);
    if (!hGlob) return { nullptr, 0 };
    void*  data = LockResource(hGlob);
    DWORD  sz   = SizeofResource(nullptr, hRes);
    return { static_cast<const char*>(data), static_cast<size_t>(sz) };
}

// -------------------------------------------------------------
//  LoadFromResources
//  Loads all interface-data files from embedded RCDATA resources.
//  Used as fallback when disk files are not present.
// -------------------------------------------------------------

void LabelDB::LoadFromResources()
{
    m_req.clear(); m_prop.clear(); m_cmd.clear();

    std::pair<const char*, size_t> req  = GetResourceData(IDR_DATA_REQTXT);
    std::pair<const char*, size_t> prop = GetResourceData(IDR_DATA_PROPTXT);
    std::pair<const char*, size_t> cmd  = GetResourceData(IDR_DATA_CMDTXT);

    if (req.first  && req.second)  ParseBuffer(req.first,  req.second,  m_req);
    if (prop.first && prop.second) ParseBuffer(prop.first, prop.second, m_prop);
    if (cmd.first  && cmd.second)  ParseBuffer(cmd.first,  cmd.second,  m_cmd);

    m_loaded = (!m_req.empty() || !m_prop.empty() || !m_cmd.empty());

    std::pair<const char*, size_t> names = GetResourceData(IDR_DATA_NAMEKEYS);
    std::pair<const char*, size_t> supp  = GetResourceData(IDR_DATA_SUPPKEYS);
    std::pair<const char*, size_t> anim  = GetResourceData(IDR_DATA_ANIMKEYS);

    if (names.first && names.second) ParseNameJsonBuffer(names.first, names.second, m_names,     /*clearFirst=*/true);
    if (supp.first  && supp.second)  ParseNameJsonBuffer(supp.first,  supp.second,  m_names,     /*clearFirst=*/false);
    if (anim.first  && anim.second)  ParseNameJsonBuffer(anim.first,  anim.second,  m_animNames, /*clearFirst=*/true);
}
