// Config.cpp — simple INI-based config persistence
#include "Config.h"
#include <windows.h>
#include <cstdio>
#include <string>

Config& Config::Get()
{
    static Config s_instance;
    return s_instance;
}

// Returns the path to "config.ini" in the same directory as the exe.
std::string Config::GetConfigPath()
{
    wchar_t exePath[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);

    // Strip the filename, keep the directory
    wchar_t* last = wcsrchr(exePath, L'\\');
    if (last) *(last + 1) = L'\0';

    // Convert to UTF-8
    int len = WideCharToMultiByte(CP_UTF8, 0, exePath, -1, nullptr, 0, nullptr, nullptr);
    std::string dir(len > 1 ? len - 1 : 0, '\0');
    WideCharToMultiByte(CP_UTF8, 0, exePath, -1, &dir[0], len, nullptr, nullptr);

    return dir + "config.ini";
}

// ─────────────────────────────────────────────────────────────
//  Load — parse a minimal key=value INI file
// ─────────────────────────────────────────────────────────────

void Config::Load()
{
    std::string cfgPath = GetConfigPath();

    // Open via wide path so Unicode directory names work
    int wlen = MultiByteToWideChar(CP_UTF8, 0, cfgPath.c_str(), -1, nullptr, 0);
    std::wstring wPath(wlen > 1 ? wlen - 1 : 0, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, cfgPath.c_str(), -1, &wPath[0], wlen);

    FILE* f = nullptr;
    _wfopen_s(&f, wPath.c_str(), L"r");
    if (!f) return;

    char line[1024];
    std::string section;
    while (fgets(line, sizeof(line), f))
    {
        std::string s = line;

        // Strip trailing CR/LF
        while (!s.empty() && (s.back() == '\n' || s.back() == '\r')) s.pop_back();

        // Strip leading whitespace
        size_t start = s.find_first_not_of(" \t");
        if (start == std::string::npos) continue;
        s = s.substr(start);

        if (s.empty() || s[0] == ';' || s[0] == '#') continue;

        if (s[0] == '[')
        {
            size_t end = s.find(']');
            section = (end != std::string::npos) ? s.substr(1, end - 1) : "";
            continue;
        }

        size_t eq = s.find('=');
        if (eq == std::string::npos) continue;

        std::string key = s.substr(0, eq);
        std::string val = s.substr(eq + 1);

        // Strip trailing whitespace
        while (!val.empty() && (val.back() == ' ' || val.back() == '\t')) val.pop_back();

        if (section == "Moveset" && key == "RootDir")
            data.movesetRootDir = val;
    }

    fclose(f);
}

// ─────────────────────────────────────────────────────────────
//  Save — write current config to config.ini
// ─────────────────────────────────────────────────────────────

void Config::Save() const
{
    std::string cfgPath = GetConfigPath();

    int wlen = MultiByteToWideChar(CP_UTF8, 0, cfgPath.c_str(), -1, nullptr, 0);
    std::wstring wPath(wlen > 1 ? wlen - 1 : 0, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, cfgPath.c_str(), -1, &wPath[0], wlen);

    FILE* f = nullptr;
    _wfopen_s(&f, wPath.c_str(), L"w");
    if (!f) return;

    fprintf(f, "[Moveset]\n");
    fprintf(f, "RootDir=%s\n", data.movesetRootDir.c_str());

    fclose(f);
}
