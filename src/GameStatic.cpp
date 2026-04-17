// GameStatic.cpp
// Loads game-version-specific magic values from GameStatic.ini.
// When Tekken 8 updates and these values change, edit the ini file --
// no recompile needed.
#include "GameStatic.h"
#include <windows.h>
#include <cstdio>
#include <cstring>
#include <string>

GameStatic& GameStatic::Get()
{
    static GameStatic s_instance;
    return s_instance;
}

std::string GameStatic::GetIniPath()
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
    return resDir + "\\GameStatic.ini";
}

// -------------------------------------------------------------
//  Write default GameStatic.ini with comments
// -------------------------------------------------------------

static void WriteDefault(const std::string& path)
{
    int wlen = MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, nullptr, 0);
    std::wstring wPath(wlen > 1 ? wlen - 1 : 0, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, &wPath[0], wlen);

    FILE* f = nullptr;
    _wfopen_s(&f, wPath.c_str(), L"w");
    if (!f) return;

    fprintf(f,
        "; GameStatic.ini -- Tekken 8 version-specific magic values\n"
        "; Edit this file (without recompiling) when a game update changes these values.\n"
        "; Values can be written in decimal or hex (prefix with 0x).\n"
        "\n"
        "[Cancel]\n"
        "; cancel.command == GroupCancelStart  ->  group cancel list entry (move_id = group cancel index)\n"
        "GroupCancelStart=0x8012\n"
        "; cancel.command == GroupCancelEnd    ->  group cancel list terminator\n"
        "GroupCancelEnd=0x8013\n"
        "; cancel.command >= InputSeqStart     ->  input_sequence[command - InputSeqStart]\n"
        "InputSeqStart=0x8014\n"
        "\n"
        "[Requirement]\n"
        "; requirement.req == this value       ->  end of requirement list\n"
        "ListEnd=1100\n"
        "\n"
        "[ExtraProp]\n"
        "; extraprop.id values that trigger special behaviour\n"
        "Dialogue1=0x877b\n"
        "Dialogue2=0x87f8\n"
        "Projectile=0x827b\n"
        "Throw=0x868f\n"
    );

    fclose(f);
}

// -------------------------------------------------------------
//  Parse a single value: decimal or 0x-prefixed hex
// -------------------------------------------------------------

static uint32_t ParseVal(const std::string& s)
{
    const char* p = s.c_str();
    if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X'))
        return static_cast<uint32_t>(strtoul(p + 2, nullptr, 16));
    return static_cast<uint32_t>(strtoul(p, nullptr, 10));
}

// -------------------------------------------------------------
//  Load
// -------------------------------------------------------------

void GameStatic::Load()
{
    std::string iniPath = GetIniPath();

    int wlen = MultiByteToWideChar(CP_UTF8, 0, iniPath.c_str(), -1, nullptr, 0);
    std::wstring wPath(wlen > 1 ? wlen - 1 : 0, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, iniPath.c_str(), -1, &wPath[0], wlen);

    FILE* f = nullptr;
    _wfopen_s(&f, wPath.c_str(), L"r");
    if (!f)
    {
        // First run: write defaults, then use in-memory defaults (already set).
        WriteDefault(iniPath);
        return;
    }

    char line[256];
    std::string section;
    while (fgets(line, sizeof(line), f))
    {
        std::string s = line;

        while (!s.empty() && (s.back() == '\n' || s.back() == '\r')) s.pop_back();

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
        while (!val.empty() && (val.back() == ' ' || val.back() == '\t')) val.pop_back();

        if (section == "Cancel")
        {
            if (key == "GroupCancelStart") data.groupCancelStart  = ParseVal(val);
            if (key == "GroupCancelEnd")   data.groupCancelEnd    = ParseVal(val);
            if (key == "InputSeqStart")    data.inputSeqStart     = ParseVal(val);
        }
        else if (section == "Requirement")
        {
            if (key == "ListEnd")          data.reqListEnd        = ParseVal(val);
        }
        else if (section == "ExtraProp")
        {
            if (key == "Dialogue1")        data.extrapropDialogue1  = ParseVal(val);
            if (key == "Dialogue2")        data.extrapropDialogue2  = ParseVal(val);
            if (key == "Projectile")       data.extrapropProjectile = ParseVal(val);
            if (key == "Throw")            data.extrapropThrow      = ParseVal(val);
        }
    }

    fclose(f);
}
