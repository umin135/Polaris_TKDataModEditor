#pragma once
#include "moveset/data/AnmbinData.h"
#include "moveset/data/AnimNameDB.h"
#include "moveset/data/MotbinData.h"
#include <string>
#include <vector>
#include <functional>
#include <unordered_map>

// AnimationManagerWindow -- sub-window for viewing and managing anmbin animation lists
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

    // Provide the AnimNameDB (read-only; used for key lookups and moveList patching).
    void SetAnimNameDB(const AnimNameDB* db) { m_animNameDB = db; }

    // Provide a pointer to the live moves list (for AddAnimToAnmbin moveList patching).
    void SetMoves(const std::vector<ParsedMove>* moves) { m_moves = moves; }

    // Called when an animation was successfully added:
    //   MovesetEditorWindow registers name → crc32 in AnimNameDB.
    void SetOnAnimAdded(std::function<void(int cat, const std::string& name, uint32_t crc32)> cb)
    { m_onAnimAdded = std::move(cb); }

    // Called when an animation was successfully removed:
    //   MovesetEditorWindow clears anim_key references and marks motbin dirty.
    void SetOnAnimRemoved(std::function<void(uint32_t removedHash)> cb)
    { m_onAnimRemoved = std::move(cb); }

    // Provide the character code (e.g. "grf") for fallback animation naming.
    void SetCharaCode(const std::string& code) { m_charaCode = code; }

    bool IsLoaded() const { return m_loaded && m_anmbin.loaded; }

    std::string AnimKeyToName(uint32_t motbinAnimKey, int cat = 0);
    bool        NameToAnimKey(const std::string& name, uint32_t& outMotbinKey, int cat = 0);
    int         AnimKeyToPoolIdx(uint32_t motbinAnimKey, int cat = 0);
    void        NavigateToPool(int cat, int poolIdx);

    // Navigate to the pool entry for a given motbin anim_key.
    void NavigateByMotbinKey(int cat, uint32_t motbinAnimKey);

    // Reload anmbin from disk (called after rebuild).
    void ForceReload();

    // Display a rebuild error message in the AnimMgr UI.
    void SetRebuildError(const std::string& msg) { m_rebuildError = msg; }

private:
    void TryLoad();
    void BuildAnimKeyMap();
    void RenderTabContent(int cat);
    void RenderPreviewPanel();

    // Action handlers
    void DoAdd(int cat);
    void DoExtract(int cat, int poolIdx);
    void DoRemove(int cat, int poolIdx);
    void DoExtractAll();

    // Compute PANM size for a pool entry (sort-by-ptr method).
    size_t ComputePanmSize(int cat, int poolIdx);

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

    const AnimNameDB*              m_animNameDB = nullptr;
    const std::vector<ParsedMove>* m_moves      = nullptr;
    std::string                    m_charaCode;

    std::function<void(int, const std::string&, uint32_t)> m_onAnimAdded;
    std::function<void(uint32_t)>                          m_onAnimRemoved;

    // Status message shown in toolbar (Add/Remove/Extract results)
    std::string m_statusMsg;
    bool        m_statusOk = true;

    // Rebuild error (set by MovesetEditorWindow if RebuildAnmbin fails)
    std::string m_rebuildError;

    // Remove confirmation state
    struct RemoveConfirm {
        bool        showing  = false;
        int         cat      = -1;
        int         poolIdx  = -1;
        std::string animName;
    };
    RemoveConfirm m_removeConfirm;

};
