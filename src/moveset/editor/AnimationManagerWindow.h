#pragma once
#include "moveset/data/AnmbinData.h"
#include "moveset/data/AnimNameDB.h"
#include <string>
#include <vector>
#include <unordered_map>

// AnimationManagerWindow -- sub-window for viewing anmbin animation lists
class AnimationManagerWindow {
public:
    AnimationManagerWindow(const std::string& folderPath, const std::string& movesetName, int uid);

    // Returns false when the window has been closed.
    bool Render();

    bool IsOpen() const { return m_open; }

    // Navigate to the pool entry whose animKey32 matches moveList[cat][moveIdx].
    void NavigateTo(int cat, int moveIdx);

    // Provide per-move anim_key values from motbin (moves[i].anim_key).
    void SetMotbinAnimKeys(const std::vector<uint32_t>& animKeys);

    // Provide the AnimNameDB (used only for lookup, not for file tracking).
    void SetAnimNameDB(const AnimNameDB* db) { m_animNameDB = db; }

    bool IsLoaded() const { return m_loaded && m_anmbin.loaded; }

    std::string AnimKeyToName(uint32_t motbinAnimKey, int cat = 0);
    bool        NameToAnimKey(const std::string& name, uint32_t& outMotbinKey, int cat = 0);
    int         AnimKeyToPoolIdx(uint32_t motbinAnimKey, int cat = 0);
    void        NavigateToPool(int cat, int poolIdx);

    // Navigate to the pool entry for a given motbin anim_key.
    // Works for both original animations (via moveList map) and newly-added
    // animations not yet in moveList (direct animKey scan fallback).
    void        NavigateByMotbinKey(int cat, uint32_t motbinAnimKey);

    // Reload anmbin from disk (called by MovesetEditorWindow after auto-rebuild).
    void ForceReload();

    // Display a rebuild error message in the AnimMgr UI (called by MovesetEditorWindow on failure).
    void SetRebuildError(const std::string& msg) { m_rebuildError = msg; }

    // New pool entries discovered by the last Refresh, not yet registered in AnimNameDB.
    // Cleared after each call.
    struct NewAnimEntry {
        int         cat;
        int         poolIdx;
        uint32_t    hash;      // CRC32 of file content == initial motbin_anim_key
        std::string name;      // filename stem (e.g. "testanim")
        std::string filename;  // original filename (e.g. "testanim.bin") for display
    };
    std::vector<NewAnimEntry> TakePendingNewEntries();

private:
    void TryLoad();
    void DoRefresh();
    void RenderTabContent(int cat);

    std::string m_folderPath;
    std::string m_windowId;

    AnmbinData  m_anmbin;
    bool        m_loaded           = false;
    bool        m_open             = true;
    int         m_selRow[6]        = {};
    int         m_pendingTab       = -1;
    bool        m_scrollPending[6] = {};

    std::vector<uint32_t>                  m_motbinAnimKeys;
    std::unordered_map<uint32_t, uint32_t> m_hashToAnimKey;
    std::unordered_map<uint32_t, int>      m_animKeyToPoolIdx[6];
    bool                                   m_mapBuilt = false;

    std::vector<NewAnimEntry>              m_pendingNew;
    bool                                   m_showComEntries = true;

    const AnimNameDB*                      m_animNameDB = nullptr;

    // Stem labels for pool entries added via DoRefresh (in-memory only, cleared on ForceReload).
    std::unordered_map<int, std::string>   m_poolFilenames[6]; // poolIdx → stem

    // Diagnostic info from the last DoRefresh call
    struct RefreshResult {
        int  totalFound = 0;
        bool ran        = false;
        std::string statusLines;
    };
    RefreshResult m_lastRefresh;
    std::string   m_rebuildError; // set by MovesetEditorWindow if RebuildAnmbin fails

    void BuildAnimKeyMap();
};
