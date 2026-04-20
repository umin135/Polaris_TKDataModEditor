#pragma once
#include "moveset/data/AnmbinData.h"
#include "moveset/data/AnimNameDB.h"
#include "moveset/data/MotbinData.h"
#include "moveset/preview/PanmParser.h"
#include <string>
#include <vector>
#include <functional>
#include <unordered_map>
#include <memory>

// Forward declarations — keep d3d11.h out of the header chain.
struct ID3D11Device;
struct ID3D11DeviceContext;
class  PreviewRenderer;

// AnimationManagerWindow -- sub-window for viewing and managing anmbin animation lists
class AnimationManagerWindow {
public:
    AnimationManagerWindow(const std::string& folderPath, const std::string& movesetName, int uid);
    ~AnimationManagerWindow();  // defined in .cpp (PreviewRenderer complete type required)

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

    // Provide D3D11 device/context for the 3D preview renderer.
    // Must be called before the first Render() for the preview to be active.
    void SetD3DContext(ID3D11Device* dev, ID3D11DeviceContext* ctx);

    bool IsLoaded() const { return m_loaded && m_anmbin.loaded; }

    std::string AnimKeyToName(uint32_t motbinAnimKey, int cat = 0);
    bool        NameToAnimKey(const std::string& name, uint32_t& outMotbinKey, int cat = 0);
    int         AnimKeyToPoolIdx(uint32_t motbinAnimKey, int cat = 0);
    void        NavigateToPool(int cat, int poolIdx);

    // Navigate to the pool entry for a given motbin anim_key.
    void NavigateByMotbinKey(int cat, uint32_t motbinAnimKey);

    // Returns totalFrames read from the PANM header for the given anim_key, or -1 if not found.
    int32_t GetTotalFramesForKey(uint32_t animKey);

    // Reload anmbin from disk (called after rebuild).
    void ForceReload();

    // Display a rebuild error message in the AnimMgr UI.
    void SetRebuildError(const std::string& msg) { m_rebuildError = msg; }

private:
    void TryLoad();
    void BuildAnimKeyMap();
    void RenderTabContent(int cat);
    void RenderPreviewPanel(int cat);
    void LoadSelectedAnim(int cat, int poolIdx);

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

    // 3D preview renderer (created lazily via SetD3DContext)
    std::unique_ptr<PreviewRenderer> m_preview;
    ID3D11Device*        m_d3dDev = nullptr;
    ID3D11DeviceContext* m_d3dCtx = nullptr;

    // Animation playback state
    ParsedAnim  m_currentAnim;          // currently loaded PANM data
    bool        m_animLoaded    = false;
    bool        m_playing       = false;
    float       m_playTime      = 0.f;  // accumulated time within current frame (seconds)
    int         m_currentFrame  = 0;
    int         m_previewCat    = -1;   // cat of the last loaded animation
    int         m_previewPoolIdx= -1;   // poolIdx of the last loaded animation
    bool        m_showSkeleton  = false; // x-ray skeleton overlay toggle
    float       m_floorHeightInput = 115.f; // UI input value for floor/camera Y offset

    // Per-category search filter buffers
    char        m_searchBuf[6][128] = {};
};

