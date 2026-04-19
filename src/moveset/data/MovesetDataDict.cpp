// MovesetDataDict.cpp
// Loads data/MovesetDatas/data.json and exposes per-category lookups.
#include "MovesetDataDict.h"
#include "moveset/labels/LabelDB.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <iomanip>
#include <utility>

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

    // ── cancel_extras section ────────────────────────────────────────────
    // Format: {"VALUE": "description", ...}  (flat string values, not objects)
    m_cancelExtras.clear();

    size_t cePos = json.find("\"cancel_extras\"");
    if (cePos != std::string::npos)
    {
        size_t ceOpen = json.find('{', cePos + 15);
        if (ceOpen != std::string::npos)
        {
            size_t ceClose = MatchingBrace(json, ceOpen);
            if (ceClose == std::string::npos) ceClose = json.size();

            size_t pos = ceOpen + 1;
            while (pos < ceClose)
            {
                // Key
                size_t q1 = json.find('"', pos);
                if (q1 == std::string::npos || q1 >= ceClose) break;
                size_t q2 = json.find('"', q1 + 1);
                if (q2 == std::string::npos || q2 >= ceClose) break;
                std::string keyStr = json.substr(q1 + 1, q2 - q1 - 1);

                // Colon + string value
                size_t colon = json.find(':', q2 + 1);
                if (colon == std::string::npos || colon >= ceClose) break;
                size_t vq1 = json.find('"', colon + 1);
                if (vq1 == std::string::npos || vq1 >= ceClose) break;

                // Walk to closing quote (escape-aware)
                size_t vq2 = vq1 + 1;
                while (vq2 < ceClose) {
                    if (json[vq2] == '\\') { vq2 += 2; continue; }
                    if (json[vq2] == '"')  break;
                    ++vq2;
                }
                if (vq2 >= ceClose) break;

                // Unescape value
                std::string raw = json.substr(vq1 + 1, vq2 - vq1 - 1);
                std::string desc;
                desc.reserve(raw.size());
                for (size_t i = 0; i < raw.size(); ++i) {
                    if (raw[i] == '\\' && i + 1 < raw.size()) {
                        char c = raw[++i];
                        if      (c == 'n')  desc += '\n';
                        else if (c == 't')  desc += '\t';
                        else if (c == '"')  desc += '"';
                        else if (c == '\\') desc += '\\';
                        else { desc += '\\'; desc += c; }
                    } else {
                        desc += raw[i];
                    }
                }

                uint32_t ceVal = static_cast<uint32_t>(std::atoi(keyStr.c_str()));
                m_cancelExtras[ceVal] = std::move(desc);

                pos = vq2 + 1;
            }
        }
    }

    // ── properties section ──────────────────────────────────────────────
    m_props.clear();

    size_t prPos = json.find("\"properties\"");
    if (prPos != std::string::npos)
    {
        size_t prOpen = json.find('{', prPos + 12);
        if (prOpen != std::string::npos)
        {
            size_t prClose = MatchingBrace(json, prOpen);
            if (prClose == std::string::npos) prClose = json.size();

            size_t pos = prOpen + 1;
            while (pos < prClose)
            {
                size_t q1 = json.find('"', pos);
                if (q1 == std::string::npos || q1 >= prClose) break;
                size_t q2 = json.find('"', q1 + 1);
                if (q2 == std::string::npos || q2 >= prClose) break;

                std::string keyStr = json.substr(q1 + 1, q2 - q1 - 1);

                size_t entOpen = json.find('{', q2 + 1);
                if (entOpen == std::string::npos || entOpen >= prClose) break;
                size_t entClose = MatchingBrace(json, entOpen);
                if (entClose == std::string::npos || entClose > prClose) break;

                uint32_t propVal = static_cast<uint32_t>(std::atoi(keyStr.c_str()));
                PropEntry entry;
                entry.param    = JsString(json, "param",    entOpen, entClose);
                entry.function = JsString(json, "Function", entOpen, entClose);
                entry.tooltip  = JsString(json, "ToolTip",  entOpen, entClose);
                m_props[propVal] = std::move(entry);

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
    // I don't know if this is the right way to do this, but for the time being, seems appropriate
    if (req > 0x8000) {
        if (const PropEntry* prop = GetPropEntry(req)) {
            return reinterpret_cast<const ReqEntry*>(prop);
        }
        return nullptr;
    }

    auto it = m_req.find(req);
    return (it != m_req.end()) ? &it->second : nullptr;
}

const char* MovesetDataDict::GetCancelExtra(uint32_t val) const
{
    auto it = m_cancelExtras.find(val);
    return (it != m_cancelExtras.end()) ? it->second.c_str() : nullptr;
}

const MovesetDataDict::PropEntry* MovesetDataDict::GetPropEntry(uint32_t val) const
{
    auto it = m_props.find(val);
    return (it != m_props.end()) ? &it->second : nullptr;
}

//////////////////////////////////

// Copies into thread_local storage so GetParam* can return const char* without dangling temporaries.
static const char* ParamLabelCStr(std::string s)
{
    thread_local std::string s_buf;
    s_buf = std::move(s);
    return s_buf.c_str();
}

const char* MovesetDataDict::GetDialogueTypeLabel(uint32_t type) const
{
    switch (type)
    {
        case 0: return "Intro";
        case 1: return "Outro";
        case 2: return "Fate";
        default: return "";
    }
}

const char* MovesetDataDict::GetDramaTypeLabel(uint32_t value) const
{
    switch (value)
    {
        case 1: return "Intro";
        case 2: return "Outro";
        case 5: return "Fate";
        case 6: return "Fate (End)";
        case 7: return "Story Pre-Fight";
        case 8: return "Story Post-Fight";
        case 9: return "Story Mid-Fight";
        default: return "";
    }
}

const char *MovesetDataDict::GetDramaLabel(uint16_t type, uint16_t id) const
{
    std::string typeLabel = GetDialogueTypeLabel(type);
    std::string idLabel = "";
    switch (type)
    {
    case 0:
    case 1:
    {
        return ParamLabelCStr(typeLabel + " " + std::to_string(id));
    }
    case 2:
    {
        switch (id) {
            case 0: idLabel = "Part 0 : Player"; break;
            case 1: idLabel = "Part 0 : Opponent"; break;
            case 10: idLabel = "Part 1 : Player"; break;
            case 11: idLabel = "Part 1 : Opponent"; break;
            case 20: idLabel = "Part 2 : Player"; break;
            case 21: idLabel = "Part 2 : Opponent"; break;
            default: idLabel = std::to_string(id) + " : Unknown"; break;
        };
        return ParamLabelCStr(typeLabel + " " + idLabel);
    }
    default:
        return "";
    }
}

std::string divBy1000(int param0)
{
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2) << (param0 / 1000.0);
    return oss.str();
}

const char* MovesetDataDict::GetParamLabel(uint32_t reqOrPropId, uint32_t pIndex, uint32_t param) const
{
    switch (reqOrPropId)
    {
    // Yes/No Options
    case 159:
    case 1028:
    case 1029:
    {
        if (pIndex > 0) return "";
        return param ? "Yes" : "No";
    }
    case 35:
    case 36:
    case 758:
    {
        if (pIndex > 0) return "";
        return ParamLabelCStr(divBy1000(static_cast<int>(param)));
    }
    // Character ID Checks
    case 220:
    case 221:
    case 222:
    case 223:
    case 224:
    case 225:
    case 226:
    case 227:
    {
        if (pIndex > 0) return "";
        return LabelDB::Get().CharaName(param);
    }
    // Character ID Checks (multiple)
    case 499:
    case 500:
    case 501:
    case 502:
    case 503:
    case 504:
    case 505:
    case 506:
    {
        return LabelDB::Get().CharaName(param);
    }
    // Short Flag
    case 288:
    case 326:
    case 365:
    case 0x8128:
    case 0x8129:
    case 0x812A:
    case 0x812B:
    case 0x812C:
    case 0x812D:
    {
        if (pIndex > 0) return "";
        // 0xAAAABBBB. 0xAAAA = flag, 0xBBBB = value. Interpret the flag and value as needed to produce a descriptive label.
        int flag = (param >> 16) & 0xFFFF;
        int value = param & 0xFFFF;
        std::ostringstream oss;
        oss << "Flag[" << flag << "]" << " = " << value;
        return ParamLabelCStr(oss.str());
    }
    // Story Battle Number
    case 668:
    {
        if (pIndex > 0) return "";
        // 0xAB: A chapter, B = battle number.
        int chapter = (param >> 4) & 0xFF;
        int battle = param & 0xF;
        std::ostringstream oss;
        oss << "Chapter " << chapter << ", Battle " << battle;
        return ParamLabelCStr(oss.str());
    }
    case 0x8313:
    {
        if (pIndex > 0) return "";
        return GetDramaTypeLabel(param);
    }
    case 0x8695:
    {
        if (pIndex > 0) return "";
        else if (param == 0) return "Player 1";
        else if (param == 1) return "Player 2";
        else if (param == 2) return "Player 1 & Player 2";
        return "";
    }
    case 0x877B:
    case 0x87F8:
    {
        if (pIndex > 0) return "";
        int dramaType = (param >> 16) & 0xFFFF;
        int dramaId = param & 0xFFFF;
        std::ostringstream oss;
        oss << GetDramaTypeLabel(param) << ": " << dramaId;
        return ParamLabelCStr(oss.str());
    }
    case 0x87EF:
    case 0x87F0:
    case 0x87F1:
    case 0x87F2:
    case 0x87F3:
    {
        std::ostringstream oss;
        oss << "";
        if (pIndex == 0) oss << "Folder ID: " << param;
        else if (pIndex == 2) oss << "Clip ID: " << param;
        return ParamLabelCStr(oss.str());
    }
    default:
        return "";
    }
}
