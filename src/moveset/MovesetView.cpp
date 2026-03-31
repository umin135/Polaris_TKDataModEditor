// MovesetView.cpp
// Scans a root directory for moveset folders and renders a list table.
#include "MovesetView.h"
#include "Config.h"
#include "imgui/imgui.h"
#include <windows.h>
#include <algorithm>
#include <cstdio>
#include <string>

// ─────────────────────────────────────────────────────────────
//  String helpers
// ─────────────────────────────────────────────────────────────

static std::wstring Utf8ToWide(const std::string& s)
{
    if (s.empty()) return {};
    int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    std::wstring w(len > 0 ? len - 1 : 0, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &w[0], len);
    return w;
}

static std::string WideToUtf8(const wchar_t* w)
{
    if (!w || !*w) return {};
    int len = WideCharToMultiByte(CP_UTF8, 0, w, -1, nullptr, 0, nullptr, nullptr);
    std::string s(len > 0 ? len - 1 : 0, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w, -1, &s[0], len, nullptr, nullptr);
    return s;
}

// ─────────────────────────────────────────────────────────────
//  INI parser for moveset.ini
// ─────────────────────────────────────────────────────────────

bool MovesetView::ParseIni(const std::wstring& iniPath,
                            std::string& origChar,
                            std::string& version)
{
    FILE* f = nullptr;
    _wfopen_s(&f, iniPath.c_str(), L"r");
    if (!f) return false;

    char line[512];
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

        // Strip inline // comment (often preceded by a tab in this format)
        size_t cmt = val.find("//");
        if (cmt != std::string::npos) val = val.substr(0, cmt);

        // Trim trailing whitespace/tabs
        while (!val.empty() && (val.back() == ' ' || val.back() == '\t')) val.pop_back();

        if (section == "Info")
        {
            if (key == "OriginalCharacter") origChar = val;
            else if (key == "Version")      version  = val;
        }
    }

    fclose(f);
    return true;
}

// ─────────────────────────────────────────────────────────────
//  Scan — enumerate subdirectories and detect moveset folders
// ─────────────────────────────────────────────────────────────

static const wchar_t* k_MovesetFiles[] = {
    L"moveset.anmbin",
    L"moveset.motbin",
    L"moveset.stllstb",
    L"moveset.mvl",
};

void MovesetView::Scan(const std::string& rootDir)
{
    m_entries.clear();
    m_scannedRoot = rootDir;

    if (rootDir.empty()) return;

    std::wstring wRoot = Utf8ToWide(rootDir);
    if (!wRoot.empty() && wRoot.back() != L'\\' && wRoot.back() != L'/')
        wRoot += L'\\';

    WIN32_FIND_DATAW fd = {};
    HANDLE hFind = FindFirstFileW((wRoot + L"*").c_str(), &fd);
    if (hFind == INVALID_HANDLE_VALUE) return;

    do
    {
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) continue;
        if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0) continue;

        std::wstring subDir = wRoot + fd.cFileName + L"\\";

        // Check if at least one recognized moveset file exists in this folder
        bool isMoveset = false;
        for (const wchar_t* fname : k_MovesetFiles)
        {
            DWORD attr = GetFileAttributesW((subDir + fname).c_str());
            if (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY))
            {
                isMoveset = true;
                break;
            }
        }
        if (!isMoveset) continue;

        MovesetEntry entry;
        entry.path     = WideToUtf8((wRoot + fd.cFileName).c_str());
        entry.name     = WideToUtf8(fd.cFileName);
        entry.origChar = "None";
        entry.version  = "None";

        ParseIni(subDir + L"moveset.ini", entry.origChar, entry.version);

        m_entries.push_back(std::move(entry));
    }
    while (FindNextFileW(hFind, &fd));

    FindClose(hFind);

    std::sort(m_entries.begin(), m_entries.end(),
              [](const MovesetEntry& a, const MovesetEntry& b) { return a.name < b.name; });
}

void MovesetView::RefreshIfNeeded(const std::string& rootDir)
{
    if (rootDir != m_scannedRoot)
        Scan(rootDir);
}

// ─────────────────────────────────────────────────────────────
//  Render
// ─────────────────────────────────────────────────────────────

void MovesetView::Render()
{
    const std::string& rootDir = Config::Get().data.movesetRootDir;
    RefreshIfNeeded(rootDir);

    if (rootDir.empty())
    {
        const float cw = ImGui::GetContentRegionAvail().x;
        const float ch = ImGui::GetContentRegionAvail().y;
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + ch * 0.38f);

        auto CenterText = [&](const char* text)
        {
            ImGui::SetCursorPosX((cw - ImGui::CalcTextSize(text).x) * 0.5f);
            ImGui::TextDisabled("%s", text);
        };
        CenterText("Moveset root directory is not configured.");
        CenterText("Open Settings and set the Moveset > Root Directory.");
        return;
    }

    // Header bar
    ImGui::Text("Root: %s", rootDir.c_str());
    ImGui::SameLine();
    if (ImGui::SmallButton("Refresh"))
        ForceRefresh();
    ImGui::Separator();
    ImGui::Spacing();

    if (m_entries.empty())
    {
        ImGui::TextDisabled("No moveset folders found in the configured root directory.");
        return;
    }

    // ── Table ──────────────────────────────────────────────────
    constexpr ImGuiTableFlags kTableFlags =
        ImGuiTableFlags_BordersOuter      |
        ImGuiTableFlags_BordersInnerH     |
        ImGuiTableFlags_RowBg             |
        ImGuiTableFlags_ScrollY           |
        ImGuiTableFlags_SizingStretchProp;

    if (!ImGui::BeginTable("##movesets", 4, kTableFlags)) return;

    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableSetupColumn("MovesetName",       ImGuiTableColumnFlags_WidthStretch, 2.0f);
    ImGui::TableSetupColumn("OriginalCharacter", ImGuiTableColumnFlags_WidthFixed,  130.0f);
    ImGui::TableSetupColumn("Version",           ImGuiTableColumnFlags_WidthFixed,  110.0f);
    ImGui::TableSetupColumn("##actions",         ImGuiTableColumnFlags_WidthFixed,   60.0f);
    ImGui::TableHeadersRow();

    for (int i = 0; i < static_cast<int>(m_entries.size()); ++i)
    {
        const MovesetEntry& e = m_entries[i];
        ImGui::TableNextRow();

        ImGui::TableSetColumnIndex(0);
        ImGui::TextUnformatted(e.name.c_str());
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
            ImGui::SetTooltip("%s", e.path.c_str());

        ImGui::TableSetColumnIndex(1);
        if (e.origChar == "None")
            ImGui::TextDisabled("None");
        else
            ImGui::TextUnformatted(e.origChar.c_str());

        ImGui::TableSetColumnIndex(2);
        if (e.version == "None")
            ImGui::TextDisabled("None");
        else
            ImGui::TextUnformatted(e.version.c_str());

        ImGui::TableSetColumnIndex(3);
        ImGui::PushID(i);
        if (ImGui::SmallButton("Edit"))
        {
            m_pendingOpenPath = e.path;
            m_pendingOpenName = e.name;
        }
        ImGui::PopID();
    }

    ImGui::EndTable();
}
