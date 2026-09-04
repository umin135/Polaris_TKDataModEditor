// T7AliasDict.cpp — hand-rolled JSON loader for t7_aliases.json
#include "T7AliasDict.h"
#include "extract/T7Constants.h"
#include "resource.h"
#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>

T7AliasDict& T7AliasDict::Get()
{
    static T7AliasDict s;
    return s;
}

static uint32_t ParseU32Key(const std::string& s)
{
    if (s.size() > 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X'))
        return static_cast<uint32_t>(strtoul(s.c_str() + 2, nullptr, 16));
    return static_cast<uint32_t>(strtoul(s.c_str(), nullptr, 10));
}

static uint64_t ParseU64Key(const std::string& s)
{
    if (s.size() > 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X'))
        return static_cast<uint64_t>(strtoull(s.c_str() + 2, nullptr, 16));
    return static_cast<uint64_t>(strtoull(s.c_str(), nullptr, 10));
}

static size_t MatchingBrace(const std::string& json, size_t open)
{
    int depth = 0;
    for (size_t i = open; i < json.size(); ++i) {
        if (json[i] == '{') ++depth;
        else if (json[i] == '}') { if (--depth == 0) return i; }
    }
    return std::string::npos;
}

static size_t MatchingBracket(const std::string& json, size_t open)
{
    int depth = 0;
    for (size_t i = open; i < json.size(); ++i) {
        if (json[i] == '[') ++depth;
        else if (json[i] == ']') { if (--depth == 0) return i; }
    }
    return std::string::npos;
}

static std::string JsString(const std::string& json, const char* key,
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
    size_t q2 = q1 + 1;
    while (q2 < to) {
        if (json[q2] == '\\') { q2 += 2; continue; }
        if (json[q2] == '"') break;
        ++q2;
    }
    if (q2 >= to) return {};
    return json.substr(q1 + 1, q2 - q1 - 1);
}

static bool JsU32Field(const std::string& json, const char* key,
                       size_t from, size_t to, uint32_t& out)
{
    std::string searchKey = std::string("\"") + key + "\"";
    size_t k = json.find(searchKey, from);
    if (k == std::string::npos || k >= to) return false;
    k += searchKey.size();
    size_t colon = json.find(':', k);
    if (colon == std::string::npos || colon >= to) return false;
    size_t p = colon + 1;
    while (p < to && (json[p] == ' ' || json[p] == '\t' || json[p] == '\n' || json[p] == '\r'))
        ++p;
    if (p >= to) return false;
    if (json[p] == '"') {
        size_t q2 = json.find('"', p + 1);
        if (q2 == std::string::npos || q2 >= to) return false;
        out = ParseU32Key(json.substr(p + 1, q2 - p - 1));
        return true;
    }
    out = static_cast<uint32_t>(strtoul(json.c_str() + p, nullptr, 0));
    return true;
}

void T7AliasDict::Parse(const std::string& json)
{
    m_reqAlias.clear();
    m_charIds.clear();
    m_charIdReqs.clear();
    m_soundProps.clear();
    m_cancelCmds.clear();
    m_hitbox.clear();
    m_soundParams.clear();
    m_inputSeqStart = 0x800D;
    m_inputSeqDelta = 7;

    // character_ids: {"8": 35, ...}
    {
        size_t pos = json.find("\"character_ids\"");
        if (pos != std::string::npos) {
            size_t open = json.find('{', pos);
            size_t close = MatchingBrace(json, open);
            if (open != std::string::npos && close != std::string::npos) {
                size_t p = open + 1;
                while (p < close) {
                    size_t q1 = json.find('"', p);
                    if (q1 == std::string::npos || q1 >= close) break;
                    size_t q2 = json.find('"', q1 + 1);
                    if (q2 == std::string::npos || q2 >= close) break;
                    uint32_t k = ParseU32Key(json.substr(q1 + 1, q2 - q1 - 1));
                    size_t colon = json.find(':', q2 + 1);
                    if (colon == std::string::npos || colon >= close) break;
                    uint32_t v = static_cast<uint32_t>(strtoul(json.c_str() + colon + 1, nullptr, 0));
                    m_charIds[k] = v;
                    p = colon + 1;
                    while (p < close && json[p] != ',' && json[p] != '}') ++p;
                    if (p < close && json[p] == ',') ++p;
                }
            }
        }
    }

    // t7_character_id_reqs: [217, ...]
    {
        size_t pos = json.find("\"t7_character_id_reqs\"");
        if (pos != std::string::npos) {
            size_t open = json.find('[', pos);
            size_t close = MatchingBracket(json, open);
            if (open != std::string::npos && close != std::string::npos) {
                size_t p = open + 1;
                while (p < close) {
                    while (p < close && (json[p] == ' ' || json[p] == ',' || json[p] == '\n' || json[p] == '\r'))
                        ++p;
                    if (p >= close) break;
                    char* end = nullptr;
                    unsigned long v = strtoul(json.c_str() + p, &end, 0);
                    if (end == json.c_str() + p) break;
                    m_charIdReqs.insert(static_cast<uint32_t>(v));
                    p = static_cast<size_t>(end - json.c_str());
                }
            }
        }
    }

    // t7_sound_props
    {
        size_t pos = json.find("\"t7_sound_props\"");
        if (pos != std::string::npos) {
            size_t open = json.find('[', pos);
            size_t close = MatchingBracket(json, open);
            if (open != std::string::npos && close != std::string::npos) {
                size_t p = open + 1;
                while (p < close) {
                    while (p < close && (json[p] == ' ' || json[p] == ',' || json[p] == '\n' || json[p] == '\r'))
                        ++p;
                    if (p >= close) break;
                    char* end = nullptr;
                    unsigned long v = strtoul(json.c_str() + p, &end, 0);
                    if (end == json.c_str() + p) break;
                    m_soundProps.insert(static_cast<uint32_t>(v));
                    p = static_cast<size_t>(end - json.c_str());
                }
            }
        }
    }

    // cancels
    {
        size_t pos = json.find("\"cancels\"");
        if (pos != std::string::npos) {
            size_t open = json.find('{', pos);
            size_t close = MatchingBrace(json, open);
            if (open != std::string::npos && close != std::string::npos) {
                // commands sub-object
                size_t cpos = json.find("\"commands\"", open);
                if (cpos != std::string::npos && cpos < close) {
                    size_t copen = json.find('{', cpos);
                    size_t cclose = MatchingBrace(json, copen);
                    if (copen != std::string::npos && cclose != std::string::npos && cclose <= close) {
                        size_t p = copen + 1;
                        while (p < cclose) {
                            size_t q1 = json.find('"', p);
                            if (q1 == std::string::npos || q1 >= cclose) break;
                            size_t q2 = json.find('"', q1 + 1);
                            if (q2 == std::string::npos || q2 >= cclose) break;
                            uint64_t k = ParseU64Key(json.substr(q1 + 1, q2 - q1 - 1));
                            size_t colon = json.find(':', q2 + 1);
                            if (colon == std::string::npos || colon >= cclose) break;
                            uint64_t v = static_cast<uint64_t>(strtoull(json.c_str() + colon + 1, nullptr, 0));
                            m_cancelCmds[k] = v;
                            p = colon + 1;
                            while (p < cclose && json[p] != ',' && json[p] != '}') ++p;
                            if (p < cclose && json[p] == ',') ++p;
                        }
                    }
                }
                uint32_t tmp = 0;
                if (JsU32Field(json, "input_sequence_start", open, close, tmp))
                    m_inputSeqStart = tmp;
                if (JsU32Field(json, "input_sequence_delta", open, close, tmp))
                    m_inputSeqDelta = tmp;
            }
        }
    }

    // requirement_aliases: {"0": {"alias":"0",...}, "0x8001": {"alias":"0x8001"}}
    {
        size_t pos = json.find("\"requirement_aliases\"");
        if (pos != std::string::npos) {
            size_t open = json.find('{', pos);
            size_t close = MatchingBrace(json, open);
            if (open != std::string::npos && close != std::string::npos) {
                size_t p = open + 1;
                while (p < close) {
                    size_t q1 = json.find('"', p);
                    if (q1 == std::string::npos || q1 >= close) break;
                    size_t q2 = json.find('"', q1 + 1);
                    if (q2 == std::string::npos || q2 >= close) break;
                    uint32_t key = ParseU32Key(json.substr(q1 + 1, q2 - q1 - 1));
                    size_t entOpen = json.find('{', q2 + 1);
                    if (entOpen == std::string::npos || entOpen >= close) break;
                    size_t entClose = MatchingBrace(json, entOpen);
                    if (entClose == std::string::npos || entClose > close) break;
                    std::string aliasStr = JsString(json, "alias", entOpen, entClose);
                    if (!aliasStr.empty())
                        m_reqAlias[key] = ParseU32Key(aliasStr);
                    p = entClose + 1;
                }
            }
        }
    }

    // sound_param_aliases
    {
        size_t pos = json.find("\"sound_param_aliases\"");
        if (pos != std::string::npos) {
            size_t open = json.find('{', pos);
            size_t close = MatchingBrace(json, open);
            if (open != std::string::npos && close != std::string::npos) {
                size_t p = open + 1;
                while (p < close) {
                    size_t q1 = json.find('"', p);
                    if (q1 == std::string::npos || q1 >= close) break;
                    size_t q2 = json.find('"', q1 + 1);
                    if (q2 == std::string::npos || q2 >= close) break;
                    uint32_t key = ParseU32Key(json.substr(q1 + 1, q2 - q1 - 1));
                    size_t entOpen = json.find('{', q2 + 1);
                    if (entOpen == std::string::npos || entOpen >= close) break;
                    size_t entClose = MatchingBrace(json, entOpen);
                    if (entClose == std::string::npos || entClose > close) break;
                    SoundParams sp = {};
                    JsU32Field(json, "value",  entOpen, entClose, sp.v[0]);
                    JsU32Field(json, "value2", entOpen, entClose, sp.v[1]);
                    JsU32Field(json, "value3", entOpen, entClose, sp.v[2]);
                    JsU32Field(json, "value4", entOpen, entClose, sp.v[3]);
                    sp.v[4] = 0;
                    m_soundParams[key] = sp;
                    p = entClose + 1;
                }
            }
        }
    }

    // hitbox_aliases
    {
        size_t pos = json.find("\"hitbox_aliases\"");
        if (pos != std::string::npos) {
            size_t open = json.find('{', pos);
            size_t close = MatchingBrace(json, open);
            if (open != std::string::npos && close != std::string::npos) {
                size_t p = open + 1;
                while (p < close) {
                    size_t q1 = json.find('"', p);
                    if (q1 == std::string::npos || q1 >= close) break;
                    size_t q2 = json.find('"', q1 + 1);
                    if (q2 == std::string::npos || q2 >= close) break;
                    uint32_t k = ParseU32Key(json.substr(q1 + 1, q2 - q1 - 1));
                    size_t colon = json.find(':', q2 + 1);
                    if (colon == std::string::npos || colon >= close) break;
                    uint32_t v = static_cast<uint32_t>(strtoul(json.c_str() + colon + 1, nullptr, 0));
                    m_hitbox[k] = v;
                    p = colon + 1;
                    while (p < close && json[p] != ',' && json[p] != '}') ++p;
                    if (p < close && json[p] == ',') ++p;
                }
            }
        }
    }

    m_loaded = true;
}

void T7AliasDict::Load(const std::string& path)
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
    Parse(json);
}

void T7AliasDict::LoadFromResources()
{
    HRSRC hrs = FindResourceW(nullptr, MAKEINTRESOURCEW(IDR_DATA_T7ALIASES), RT_RCDATA);
    if (!hrs) return;
    HGLOBAL hg = LoadResource(nullptr, hrs);
    if (!hg) return;
    const char* data = static_cast<const char*>(LockResource(hg));
    DWORD sz = SizeofResource(nullptr, hrs);
    if (!data || sz == 0) return;
    Parse(std::string(data, sz));
}

void T7AliasDict::EnsureLoaded()
{
    if (m_loaded) return;
    static const char* kCandidates[] = {
        "res\\MovesetDatas\\t7_aliases.json",
        "data\\MovesetDatas\\t7_aliases.json",
    };
    for (const char* p : kCandidates) {
        Load(p);
        if (m_loaded) return;
    }
    LoadFromResources();
}

bool T7AliasDict::MapRequirement(uint32_t t7Id, uint32_t& outAlias) const
{
    auto it = m_reqAlias.find(t7Id);
    if (it == m_reqAlias.end()) return false;
    outAlias = it->second;
    return true;
}

uint32_t T7AliasDict::MapCharacterId(uint32_t t7Id) const
{
    auto it = m_charIds.find(t7Id);
    if (it == m_charIds.end()) return T7::kPlaceholderCharId;
    return it->second;
}

bool T7AliasDict::IsCharacterIdReq(uint32_t t7ReqId) const
{
    return m_charIdReqs.count(t7ReqId) > 0;
}

bool T7AliasDict::IsSoundProp(uint32_t propId) const
{
    return m_soundProps.count(propId) > 0;
}

uint64_t T7AliasDict::MapCancelCommand(uint64_t command) const
{
    auto it = m_cancelCmds.find(command);
    if (it != m_cancelCmds.end())
        return it->second;
    if (command >= m_inputSeqStart)
        return command + m_inputSeqDelta;
    return command;
}

uint8_t T7AliasDict::MapHitbox(uint8_t t7Byte) const
{
    auto it = m_hitbox.find(t7Byte);
    if (it == m_hitbox.end()) return t7Byte;
    return static_cast<uint8_t>(it->second & 0xFF);
}

bool T7AliasDict::MapSoundParams(uint32_t t7Value,
                                 uint32_t& v0, uint32_t& v1, uint32_t& v2,
                                 uint32_t& v3, uint32_t& v4) const
{
    auto it = m_soundParams.find(t7Value);
    if (it != m_soundParams.end()) {
        v0 = it->second.v[0];
        v1 = it->second.v[1];
        v2 = it->second.v[2];
        v3 = it->second.v[3];
        v4 = it->second.v[4];
        return true;
    }
    // Voiceclip-style: 0xAABBCCCC → val1=AA, val2=0, val3=CCCC
    v0 = (t7Value >> 24) & 0xFF;
    v1 = 0;
    v2 = t7Value & 0xFFFF;
    v3 = 0;
    v4 = 0;
    return false;
}
