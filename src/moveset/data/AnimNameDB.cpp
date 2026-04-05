// AnimNameDB.cpp
#include "AnimNameDB.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#define NOMINMAX
#include <windows.h>

// -------------------------------------------------------------
//  Helpers
// -------------------------------------------------------------

static std::string WithSlash(const std::string& p)
{
    std::string r = p;
    if (!r.empty() && r.back() != '\\' && r.back() != '/') r += '\\';
    return r;
}

std::string AnimNameDB::JsonPath(const std::string& folderPath)
{
    return WithSlash(folderPath) + ".tkedit\\anim_names.json";
}

// -------------------------------------------------------------
//  Load
// -------------------------------------------------------------

bool AnimNameDB::Load(const std::string& folderPath)
{
    m_loaded = false;
    m_keyToName.clear();
    m_nameToKey.clear();

    FILE* f = nullptr;
    if (fopen_s(&f, JsonPath(folderPath).c_str(), "r") != 0 || !f)
        return false;

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 2) { fclose(f); return false; }

    std::string buf(static_cast<size_t>(sz), '\0');
    fread(&buf[0], 1, static_cast<size_t>(sz), f);
    fclose(f);

    // Minimal parser for {"anim_N":uint32,...}
    size_t pos = 0;
    auto skipWS = [&]() {
        while (pos < buf.size() &&
               (buf[pos]==' '||buf[pos]=='\t'||buf[pos]=='\n'||buf[pos]=='\r'))
            ++pos;
    };

    skipWS();
    if (pos >= buf.size() || buf[pos] != '{') return false;
    ++pos;

    while (pos < buf.size())
    {
        skipWS();
        if (pos >= buf.size()) break;
        char c = buf[pos];
        if (c == '}') break;
        if (c == ',') { ++pos; continue; }
        if (c != '"') break;

        // key
        ++pos;
        size_t kStart = pos;
        while (pos < buf.size() && buf[pos] != '"') ++pos;
        std::string key = buf.substr(kStart, pos - kStart);
        if (pos < buf.size()) ++pos; // closing "

        skipWS();
        if (pos >= buf.size() || buf[pos] != ':') break;
        ++pos;
        skipWS();

        // value (unsigned integer)
        size_t vStart = pos;
        while (pos < buf.size() && buf[pos] >= '0' && buf[pos] <= '9') ++pos;
        if (pos == vStart) break;

        uint32_t val = static_cast<uint32_t>(
            strtoul(buf.substr(vStart, pos - vStart).c_str(), nullptr, 10));

        m_keyToName[val] = key;
        m_nameToKey[key] = val;
    }

    m_loaded = !m_nameToKey.empty();
    return m_loaded;
}

// -------------------------------------------------------------
//  Save  (private)
// -------------------------------------------------------------

bool AnimNameDB::Save(const std::string& jsonPath)
{
    FILE* f = nullptr;
    if (fopen_s(&f, jsonPath.c_str(), "w") != 0 || !f)
        return false;

    // Collect all entries; sort anim_N/com_N by pool index, then arbitrary names at end.
    struct Entry { std::string name; uint32_t key; int idx; };
    std::vector<Entry> entries;   // anim_N / com_N (sorted by pool index)
    std::vector<Entry> customs;   // arbitrary user names (e.g. "testanim")
    entries.reserve(m_nameToKey.size());
    for (const auto& kv : m_nameToKey)
    {
        int idx = -1;
        if (kv.first.rfind("anim_", 0) == 0 || kv.first.rfind("com_", 0) == 0)
        {
            size_t u = kv.first.rfind('_');
            if (u != std::string::npos) idx = std::atoi(kv.first.c_str() + u + 1);
        }
        if (idx >= 0)
            entries.push_back({ kv.first, kv.second, idx });
        else
            customs.push_back({ kv.first, kv.second, 0 });
    }
    // Sort: com entries first (ascending pool index), then anim entries.
    std::sort(entries.begin(), entries.end(),
        [](const Entry& a, const Entry& b) {
            bool aCom = (a.name.rfind("com_", 0) == 0);
            bool bCom = (b.name.rfind("com_", 0) == 0);
            if (aCom != bCom) return aCom > bCom; // com_* before anim_*
            return a.idx < b.idx;                  // numeric sort within group
        });
    // Append arbitrary-named entries (user files like "testanim") after sorted entries.
    entries.insert(entries.end(), customs.begin(), customs.end());

    fprintf(f, "{");
    for (size_t i = 0; i < entries.size(); ++i)
    {
        if (i > 0) fprintf(f, ",");
        fprintf(f, "\"%s\":%u", entries[i].name.c_str(), entries[i].key);
    }
    fprintf(f, "}");
    fclose(f);
    return true;
}

// -------------------------------------------------------------
//  BuildAndSave
// -------------------------------------------------------------

bool AnimNameDB::BuildAndSave(const std::string&            folderPath,
                               const AnmbinData&             anmbin,
                               const std::vector<uint32_t>&  motbinAnimKeys,
                               const std::string&            charaCode)
{
    m_loaded = false;
    m_keyToName.clear();
    m_nameToKey.clear();

    if (!anmbin.loaded || motbinAnimKeys.empty()) return false;

    // Step 1: anmbin hash → motbin_key
    //   moveList[0][i] = hash for move i
    //   motbinAnimKeys[i] = anim_key for move i
    std::unordered_map<uint32_t, uint32_t> hashToKey;
    const auto& ml = anmbin.moveList[0];
    const size_t n = ml.size() < motbinAnimKeys.size() ? ml.size() : motbinAnimKeys.size();
    for (size_t i = 0; i < n; ++i)
        hashToKey.emplace(ml[i], motbinAnimKeys[i]);

    // Step 2: pool[0][j] → name → motbin_key
    //   j = pool index (used directly as the name number)
    //   animDataPtr == 0  →  "com_j"  (animation lives in com.anmbin)
    //   animDataPtr != 0  →  "anim_j" (animation embedded in this anmbin)
    //
    // Using pool index directly ensures the name matches the filename convention
    // used by extraction tools (e.g., pool entry 500 → file "anim_500.bin").
    const auto& pool = anmbin.pool[0];
    for (int j = 0; j < (int)pool.size(); ++j)
    {
        if (pool[j].animDataPtr == 0) continue; // com.anmbin ref — referenced by raw key, not named

        uint32_t hash32 = static_cast<uint32_t>(pool[j].animKey & 0xFFFFFFFF);
        auto it = hashToKey.find(hash32);
        if (it == hashToKey.end()) continue;

        char name[64];
        if (!charaCode.empty())
            snprintf(name, sizeof(name), "anim_%s_%d", charaCode.c_str(), j);
        else
            snprintf(name, sizeof(name), "anim_%d", j); // j = pool index
        m_keyToName[it->second] = name;
        m_nameToKey[name]       = it->second;
    }

    if (m_nameToKey.empty()) return false;

    // Ensure .tkedit folder exists
    std::string tkeditDir = WithSlash(folderPath) + ".tkedit";
    CreateDirectoryA(tkeditDir.c_str(), nullptr); // ok if already exists

    if (!Save(JsonPath(folderPath))) return false;

    m_loaded = true;
    return true;
}

// -------------------------------------------------------------
//  AddEntry
// -------------------------------------------------------------

bool AnimNameDB::AddEntry(const std::string& folderPath,
                          const std::string& name, uint32_t key)
{
    // Idempotent: skip if already correct
    auto it = m_nameToKey.find(name);
    if (it != m_nameToKey.end() && it->second == key) return true;

    m_nameToKey[name] = key;
    m_keyToName[key]  = name;
    m_loaded = true;

    std::string tkeditDir = WithSlash(folderPath) + ".tkedit";
    CreateDirectoryA(tkeditDir.c_str(), nullptr);
    return Save(JsonPath(folderPath));
}

// -------------------------------------------------------------
//  Lookup API
// -------------------------------------------------------------

std::string AnimNameDB::AnimKeyToName(uint32_t animKey) const
{
    auto it = m_keyToName.find(animKey);
    if (it == m_keyToName.end()) return "";
    // Ignore legacy com_ entries from old JSON files
    if (it->second.rfind("com_", 0) == 0) return "";
    return it->second;
}

bool AnimNameDB::NameToAnimKey(const std::string& name, uint32_t& outKey) const
{
    // com_ entries are no longer named — caller should pass raw hex for com refs
    if (name.rfind("com_", 0) == 0) return false;
    auto it = m_nameToKey.find(name);
    if (it == m_nameToKey.end()) return false;
    outKey = it->second;
    return true;
}
