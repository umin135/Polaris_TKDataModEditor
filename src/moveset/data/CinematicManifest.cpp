// CinematicManifest.cpp -- read/write <moveset>/polaris/cinematic.json (self-produced format).
#include "moveset/data/CinematicManifest.h"
#include <functional>
#include <cstdio>
#include <cstdlib>
#include <windows.h>

// ---- tiny scanner helpers ----------------------------------------------------

static std::string ReadWhole(const std::string& path)
{
    FILE* f = nullptr;
    if (fopen_s(&f, path.c_str(), "rb") != 0 || !f) return {};
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    std::string s(sz > 0 ? (size_t)sz : 0, '\0');
    if (sz > 0) fread(&s[0], 1, (size_t)sz, f);
    fclose(f);
    return s;
}

static size_t MatchPair(const std::string& s, size_t open, char oc, char cc)
{
    int depth = 0; bool inStr = false;
    for (size_t i = open; i < s.size(); ++i) {
        char c = s[i];
        if (inStr) { if (c == '\\') ++i; else if (c == '"') inStr = false; continue; }
        if (c == '"') inStr = true;
        else if (c == oc) ++depth;
        else if (c == cc) { if (--depth == 0) return i; }
    }
    return std::string::npos;
}

// Iterate immediate members of the object/array whose brackets span [open, close].
static void ForEachMember(const std::string& js, size_t open, size_t close,
                          const std::function<void(const std::string&, size_t, size_t)>& cb)
{
    size_t i = open + 1;
    while (i < close) {
        while (i < close && js[i] != '"') ++i;              // find key (objects) — arrays have none
        if (i >= close) break;
        size_t k1 = i + 1;
        size_t k2 = js.find('"', k1);
        if (k2 == std::string::npos || k2 >= close) break;
        std::string key = js.substr(k1, k2 - k1);
        size_t colon = js.find(':', k2 + 1);
        if (colon == std::string::npos || colon >= close) break;
        size_t v = colon + 1;
        while (v < close && (js[v] == ' ' || js[v] == '\t' || js[v] == '\n' || js[v] == '\r')) ++v;
        size_t vEnd;
        if (v >= close) break;
        if (js[v] == '{')      vEnd = MatchPair(js, v, '{', '}');
        else if (js[v] == '[') vEnd = MatchPair(js, v, '[', ']');
        else if (js[v] == '"') { size_t q = v + 1; while (q < close && js[q] != '"') { if (js[q] == '\\') ++q; ++q; } vEnd = q; }
        else { vEnd = v; while (vEnd < close && js[vEnd] != ',' && js[vEnd] != '}') ++vEnd; --vEnd; }
        if (vEnd == std::string::npos || vEnd > close) break;
        cb(key, v, vEnd);
        i = vEnd + 1;
    }
}

static bool GetStr(const std::string& js, const char* key, size_t from, size_t to, std::string& out)
{
    std::string pat = std::string("\"") + key + "\"";
    size_t k = js.find(pat, from);
    if (k == std::string::npos || k >= to) return false;
    size_t colon = js.find(':', k + pat.size());
    if (colon == std::string::npos || colon >= to) return false;
    size_t q1 = js.find('"', colon + 1);
    if (q1 == std::string::npos || q1 >= to) return false;
    std::string r; size_t q2 = q1 + 1;
    while (q2 < to && js[q2] != '"') { if (js[q2] == '\\') ++q2; if (q2 < to) r += js[q2]; ++q2; }
    out = r; return true;
}

static bool GetInt(const std::string& js, const char* key, size_t from, size_t to, int& out)
{
    std::string pat = std::string("\"") + key + "\"";
    size_t k = js.find(pat, from);
    if (k == std::string::npos || k >= to) return false;
    size_t colon = js.find(':', k + pat.size());
    if (colon == std::string::npos || colon >= to) return false;
    out = atoi(js.c_str() + colon + 1); return true;
}

static bool GetBool(const std::string& js, const char* key, size_t from, size_t to, bool& out)
{
    std::string pat = std::string("\"") + key + "\"";
    size_t k = js.find(pat, from);
    if (k == std::string::npos || k >= to) return false;
    size_t colon = js.find(':', k + pat.size());
    if (colon == std::string::npos || colon >= to) return false;
    size_t t = js.find("true", colon + 1), fs = js.find("false", colon + 1);
    out = (t != std::string::npos && (fs == std::string::npos || t < fs) && t < to);
    return true;
}

// Finds a top-level `"key": {` / `"key": [` and returns the bracket span [open, close].
static bool FindContainer(const std::string& js, const char* key, char oc, char cc, size_t& open, size_t& close)
{
    std::string pat = std::string("\"") + key + "\"";
    size_t k = js.find(pat);
    if (k == std::string::npos) return false;
    open = js.find(oc, k + pat.size());
    if (open == std::string::npos) return false;
    close = MatchPair(js, open, oc, cc);
    return close != std::string::npos;
}

// ---- load --------------------------------------------------------------------

static void LoadSection(const std::string& js, const char* name, const char* group,
                        std::vector<CineManifestEntry>& out)
{
    size_t o, c;
    if (!FindContainer(js, name, '[', ']', o, c)) return;
    // array of objects
    size_t i = o + 1;
    while (i < c) {
        size_t eo = js.find('{', i);
        if (eo == std::string::npos || eo >= c) break;
        size_t ec = MatchPair(js, eo, '{', '}');
        if (ec == std::string::npos || ec > c) break;
        CineManifestEntry e; e.group = group;
        GetStr(js, "id",  eo, ec, e.id);
        GetStr(js, "sub", eo, ec, e.sub);
        GetStr(js, "cam", eo, ec, e.cam);
        GetStr(js, "src", eo, ec, e.src);
        GetInt(js, "nn",  eo, ec, e.num);
        GetInt(js, "no",  eo, ec, e.num);
        e.hasExists = GetBool(js, "exists", eo, ec, e.exists);
        GetBool(js, "added", eo, ec, e.added);
        out.push_back(std::move(e));
        i = ec + 1;
    }
}

CineManifest LoadCineManifest(const std::string& folderPath)
{
    CineManifest man;
    std::string js = ReadWhole(folderPath + "\\polaris\\cinematic.json");
    if (js.empty()) return man;

    GetStr(js, "code", 0, js.size(), man.code);
    GetStr(js, "folder_source", 0, js.size(), man.folderSource);

    LoadSection(js, "rage",  "rage",  man.entries);
    LoadSection(js, "intro", "intro", man.entries);
    LoadSection(js, "outro", "outro", man.entries);
    LoadSection(js, "throw", "throw", man.entries);

    // overrides map -> apply to entries by id
    { size_t o, c; if (FindContainer(js, "overrides", '{', '}', o, c))
        ForEachMember(js, o, c, [&](const std::string& id, size_t v, size_t vEnd) {
            if (js[v] != '"') return;
            std::string val = js.substr(v + 1, vEnd - v - 1);
            for (auto& e : man.entries) if (e.id == id) e.overridePath = val;
        }); }


    // excluded object -> keep verbatim inner text
    { size_t o, c; if (FindContainer(js, "excluded", '{', '}', o, c)) {
        std::string inner = js.substr(o + 1, c - o - 1);
        size_t a = inner.find_first_not_of(" \t\r\n");
        size_t b = inner.find_last_not_of(" \t\r\n");
        if (a != std::string::npos) man.excludedRaw = inner.substr(a, b - a + 1);
    } }

    man.loaded = true;
    return man;
}

// ---- save --------------------------------------------------------------------

static void WriteEntry(FILE* f, const CineManifestEntry& e, bool first)
{
    fprintf(f, first ? "\n    { \"id\": \"%s\"" : ",\n    { \"id\": \"%s\"", e.id.c_str());
    if (e.group == "rage")            fprintf(f, ", \"sub\": \"%s\", \"cam\": \"%s\"", e.sub.c_str(), e.cam.c_str());
    else if (e.group == "throw")      fprintf(f, ", \"nn\": %d, \"cam\": \"%s\"", e.num, e.cam.c_str());
    else                              fprintf(f, ", \"no\": %d", e.num); // intro / outro
    fprintf(f, ", \"src\": \"%s\"", e.src.c_str());
    if (e.hasExists)  fprintf(f, ", \"exists\": %s", e.exists ? "true" : "false");
    if (e.added)      fprintf(f, ", \"added\": true");
    fprintf(f, " }");
}

static void WriteSection(FILE* f, const CineManifest& m, const char* name, const char* group)
{
    fprintf(f, "  \"%s\": [", name);
    bool first = true;
    for (const auto& e : m.entries) {
        if (e.group != group) continue;
        WriteEntry(f, e, first); first = false;
    }
    fprintf(f, first ? "],\n" : "\n  ],\n");
}

bool SaveCineManifest(const std::string& folderPath, const CineManifest& man)
{
    std::string dir = folderPath + "\\polaris";
    CreateDirectoryA(dir.c_str(), nullptr);
    FILE* f = nullptr;
    if (fopen_s(&f, (dir + "\\cinematic.json").c_str(), "w") != 0 || !f) return false;

    fprintf(f, "{\n");
    fprintf(f, "  \"code\": \"%s\",\n", man.code.c_str());
    fprintf(f, "  \"folder_source\": \"%s\",\n", man.folderSource.c_str());
    fprintf(f, "  \"note\": \"Camera cutscene manifest. Each entry's 'src' is the source character's path. Editing a sequence's Source in the editor stores the new value in 'overrides' (id -> path); empty override = use src. When this moveset is injected onto another slot, the Loader rebuilds that slot's request path from the section (rage/intro/outro/throw) + descriptors and redirects it to override||src. 'added_throws' = throw numbers added in the editor.\",\n");

    // overrides map (from entries with a non-empty overridePath)
    fprintf(f, "  \"overrides\": {");
    { bool first = true;
      for (const auto& e : man.entries) {
          if (e.overridePath.empty()) continue;
          fprintf(f, first ? "\n    \"%s\": \"%s\"" : ",\n    \"%s\": \"%s\"", e.id.c_str(), e.overridePath.c_str());
          first = false;
      }
      fprintf(f, first ? "},\n" : "\n  },\n"); }

    WriteSection(f, man, "rage",  "rage");
    WriteSection(f, man, "intro", "intro");
    WriteSection(f, man, "outro", "outro");
    WriteSection(f, man, "throw", "throw");

    fprintf(f, "  \"excluded\": {");
    if (!man.excludedRaw.empty()) fprintf(f, "\n    %s\n  }\n", man.excludedRaw.c_str());
    else                          fprintf(f, "}\n");
    fprintf(f, "}\n");
    fclose(f);
    return true;
}
