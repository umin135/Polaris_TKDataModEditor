#pragma once
#include <string>
#include <vector>

struct MovesetEntry {
    std::string path;      // full path to the moveset folder
    std::string name;      // folder name (used as MovesetName)
    std::string origChar;  // OriginalCharacter from moveset.ini, or "None"
    std::string version;   // Version from moveset.ini, or "None"
};

class MovesetView {
public:
    // Renders the moveset list table inside the current ImGui window.
    void Render();

    // Forces a rescan of the root directory on the next Render() call.
    void ForceRefresh() { m_scannedRoot.clear(); }

    // Returns the pending open request (path + name) and clears it.
    // App calls this after Render() to open an editor window if needed.
    bool TakePendingOpen(std::string& outPath, std::string& outName)
    {
        if (m_pendingOpenPath.empty()) return false;
        outPath = std::move(m_pendingOpenPath);
        outName = std::move(m_pendingOpenName);
        return true;
    }

private:
    std::vector<MovesetEntry> m_entries;
    std::string               m_scannedRoot;
    std::string               m_pendingOpenPath;
    std::string               m_pendingOpenName;

    void RefreshIfNeeded(const std::string& rootDir);
    void Scan(const std::string& rootDir);
    static bool ParseIni(const std::wstring& iniPath, std::string& origChar, std::string& version);
};
