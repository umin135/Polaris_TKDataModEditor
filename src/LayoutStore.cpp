// LayoutStore.cpp -- INI-based persistence of window sizes and section widths
#include "LayoutStore.h"
#include <windows.h>
#include <cstdio>
#include <cstdlib>

LayoutStore& LayoutStore::Get()
{
    static LayoutStore s_instance;
    return s_instance;
}

// res/layout.ini next to the exe (mirrors Config's res-folder convention).
std::string LayoutStore::FilePath()
{
    wchar_t exePath[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    wchar_t* last = wcsrchr(exePath, L'\\');
    if (last) *(last + 1) = L'\0';

    int len = WideCharToMultiByte(CP_UTF8, 0, exePath, -1, nullptr, 0, nullptr, nullptr);
    std::string dir(len > 1 ? len - 1 : 0, '\0');
    WideCharToMultiByte(CP_UTF8, 0, exePath, -1, &dir[0], len, nullptr, nullptr);

    std::string resDir = dir + "res";
    CreateDirectoryA(resDir.c_str(), nullptr);
    return resDir + "\\layout.ini";
}

void LayoutStore::Load()
{
    m_vals.clear();

    std::string path = FilePath();
    int wlen = MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, nullptr, 0);
    std::wstring wPath(wlen > 1 ? wlen - 1 : 0, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, &wPath[0], wlen);

    FILE* f = nullptr;
    _wfopen_s(&f, wPath.c_str(), L"r");
    if (!f) return;

    char line[512];
    while (fgets(line, sizeof(line), f))
    {
        std::string s = line;
        while (!s.empty() && (s.back() == '\n' || s.back() == '\r' || s.back() == ' ' || s.back() == '\t'))
            s.pop_back();
        if (s.empty() || s[0] == ';' || s[0] == '#') continue;

        size_t eq = s.find('=');
        if (eq == std::string::npos) continue;
        std::string key = s.substr(0, eq);
        std::string val = s.substr(eq + 1);
        if (key.empty() || val.empty()) continue;
        m_vals[key] = (float)atof(val.c_str());
    }
    fclose(f);
}

void LayoutStore::Save() const
{
    std::string path = FilePath();
    int wlen = MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, nullptr, 0);
    std::wstring wPath(wlen > 1 ? wlen - 1 : 0, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, &wPath[0], wlen);

    FILE* f = nullptr;
    _wfopen_s(&f, wPath.c_str(), L"w");
    if (!f) return;

    fprintf(f, "; PolarisTK layout -- sub-window sizes and section widths (auto-generated)\n");
    for (const auto& kv : m_vals)
        fprintf(f, "%s=%.1f\n", kv.first.c_str(), kv.second);
    fclose(f);
}
