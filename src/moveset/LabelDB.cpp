// LabelDB.cpp
#include "LabelDB.h"
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

    // characterList.txt: same id,name format
    std::unordered_map<uint64_t, std::string> charasTmp;
    ParseFile(base + "characterList.txt", charasTmp);
    m_charas.clear();
    for (auto& kv : charasTmp)
        m_charas[static_cast<uint32_t>(kv.first)] = std::move(kv.second);

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

const char* LabelDB::CharaName(uint32_t id) const
{
    auto it = m_charas.find(id);
    return it != m_charas.end() ? it->second.c_str() : nullptr;
}
