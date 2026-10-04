// CinematicSeqDB.cpp -- loads res/cinematics/data.json (see header).
#include "moveset/data/CinematicSeqDB.h"
#include <cstdio>

CinematicSeqDB& CinematicSeqDB::Get()
{
    static CinematicSeqDB s;
    return s;
}

void CinematicSeqDB::Load(const std::string& dataJsonPath)
{
    FILE* f = nullptr;
    if (fopen_s(&f, dataJsonPath.c_str(), "rb") != 0 || !f) return;
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    std::string js(sz > 0 ? (size_t)sz : 0, '\0');
    if (sz > 0) fread(&js[0], 1, (size_t)sz, f);
    fclose(f);

    size_t arr = js.find("\"paths\"");
    if (arr == std::string::npos) return;
    arr = js.find('[', arr);
    if (arr == std::string::npos) return;

    // Every string inside the "paths" array: "<base>/<season>/<tail...>" -> key "<base>/<tail...>".
    std::unordered_map<std::string, std::string> map;
    size_t i = arr + 1;
    while (i < js.size() && js[i] != ']') {
        if (js[i] != '"') { ++i; continue; }
        size_t q = js.find('"', i + 1);
        if (q == std::string::npos) break;
        std::string p = js.substr(i + 1, q - i - 1);
        i = q + 1;
        size_t s1 = p.find('/');
        size_t s2 = (s1 == std::string::npos) ? s1 : p.find('/', s1 + 1);
        if (s2 == std::string::npos) continue;
        map[p.substr(0, s1) + p.substr(s2)] = p.substr(s1 + 1, s2 - s1 - 1);
    }
    if (map.empty()) return;

    m_seasonByKey.swap(map);
    m_loaded = true;
}

std::string CinematicSeqDB::FindSeason(const std::string& base, const std::string& tail) const
{
    auto it = m_seasonByKey.find(base + "/" + tail);
    return it != m_seasonByKey.end() ? it->second : std::string();
}
