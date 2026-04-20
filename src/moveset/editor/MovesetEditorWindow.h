#pragma once
#include "moveset/data/MotbinData.h"
#include "moveset/data/AnimNameDB.h"
#include "moveset/data/AnmbinRebuild.h"
#include "moveset/editor/AnimationManagerWindow.h"
#include <string>
#include <unordered_map>
#include <functional>
#include <memory>
#include <future>

// Forward declarations
struct ID3D11Device;
struct ID3D11DeviceContext;

class MovesetEditorWindow {
public:
    // Used by inline req/extradata pop-ups (kept for backward compat)
    struct ReqViewState {
        bool   open  = false;
        std::vector<ParsedRequirement> reqs;
    };
    struct ExtradataViewState {
        bool     open  = false;
        uint32_t idx   = 0xFFFFFFFF;
        uint32_t value = 0xFFFFFFFF;
    };

    // 2-level selection state (outer list index, inner item index)
    struct TwoLevelSel {
        int  outer       = 0;
        int  inner       = 0;
        bool scrollOuter = false; // request scroll to outer item on next render
    };

    struct CancelsWinState {
        bool         open                  = false;
        bool         pendingFocus          = false;
        TwoLevelSel  cancelSel;
        TwoLevelSel  groupCancelSel;
        int          extradataSel          = 0;
        bool         extraScrollPending    = false;
        int          pendingTab            = -1; // 0=Cancel List, 1=Group Cancel List
    };

    struct ReactionListWinState {
        bool open            = false;
        int  selectedIdx     = 0;
        bool scrollPending   = false;
    };

    struct PushbackWinState {
        bool open                 = false;
        int  pushbackSel          = 0;
        int  extraSel             = 0;
        bool extraScrollPending   = false;
        bool pbScrollPending      = false;
    };

    struct InputSeqWinState {
        bool        open         = false;
        TwoLevelSel sel;
    };

    struct ProjectileWinState {
        bool open          = false;
        int  selectedIdx   = 0;
        bool scrollPending = false;
    };

    struct ThrowsWinState {
        bool        open               = false;
        int         throwSel           = 0;
        bool        throwScrollPending = false;
        TwoLevelSel extraSel;
    };

    struct RefFinderState {
        bool  open     = false;
        char  inputBuf[64] = {};
        int   targetMove   = -1;
        bool  searched     = false;

        struct Hit {
            enum class Type { Cancel, GroupCancel };
            Type     type;
            uint32_t blockIdx;    // C: absolute index in the block
            uint32_t groupOuter;  // group number (for navigation)
            uint32_t groupFirst;  // A: first block index of the group (for display)
            std::vector<int> owningMoves; // move indices whose cancel list contains this hit
        };
        std::vector<Hit> results;
    };

    struct PropertiesWinState {
        bool        open         = false;
        bool        pendingFocus = false;
        TwoLevelSel epSel;             // Extra Properties
        TwoLevelSel spSel;             // Start Properties
        TwoLevelSel npSel;             // End Properties
        int         pendingTab   = -1; // -1=none, 0=Extra, 1=Start, 2=End
    };

    MovesetEditorWindow(const std::string& folderPath,
                        const std::string& movesetName,
                        int uid);

    bool Render();

    // Provide D3D11 device/context for the 3D preview in AnimationManagerWindow.
    void SetD3DContext(ID3D11Device* dev, ID3D11DeviceContext* ctx);

    // Called by RenderCancelSection / RenderPropSection (free statics) — must stay public
    void RenderCancelInnerDetail(
        ParsedCancel& c, int localIdx, uint32_t blockIdx,
        const std::vector<std::pair<uint32_t,uint32_t>>& gcGroups);

    // Remove-confirmation modal  (accessed by free static render helpers)
    struct RemoveConfirmState {
        bool pending = false;   // one-frame trigger: begin showing
        bool showing = false;   // persistent: dialog remains until dismissed
        char message[300] = {};
        std::function<void()> onConfirm;
        uint32_t callerViewportId = 0; // viewport ID of the sub-window that triggered this dialog
    };
    RemoveConfirmState m_removeConfirm;
    bool     m_endInsertBlocked        = false; // one-frame trigger
    bool     m_showInsertBlocked       = false; // persistent show flag
    uint32_t m_insertBlockedViewportId = 0;     // viewport of the sub-window that set the flag

private:
    void RenderMoveList();
    void RenderMoveProperties(int idx);
    void RenderSection_Overview(ParsedMove& m, bool& dirty);
    void RenderSection_Unknown(ParsedMove& m, bool& dirty);

private:
    void RenderReqViewWindow();
    void RenderExtradataViewWindow();

    void RenderMenuBar();
    void LoadEditorDatas();
    void SaveEditorDatas();
    void SaveToFile();
    void RequestClose();
    void RenderCloseConfirmModal();
    void RenderSavePopups();
    void RenderSubWin_Requirements();
    void RenderSubWin_Cancels();
    void RenderSubWin_HitConditions();
    void RenderSubWin_ReactionLists();
    void RenderSubWin_Pushbacks();
    void RenderSubWin_Voiceclips();
    void RenderSubWin_Properties();
    void RenderSubWin_Throws();
    void RenderSubWin_Projectiles();
    void RenderSubWin_InputSequences();
    void RenderSubWin_ParryableMoves();
    void RenderSubWin_Dialogues();
    void RenderSubWin_ReferenceFinder();
    void RenderRemoveConfirmModal();

    // Returns a per-instance ImGui window/popup ID: "Label##tag_<uid>"
    // Prevents ID collisions when multiple MovesetEditorWindows are open.
    std::string WinId(const char* id) const
    {
        return std::string(id) + "_" + std::to_string(m_uid);
    }

    MotbinData  m_data;
    std::unordered_map<int, std::string> m_customNames;
    std::string m_windowTitle;
    bool        m_open              = true;
    bool        m_firstFrame        = true;  // used to set initial position outside main viewport
    bool        m_pendingFocus      = false; // bring editor OS window to front on second frame
    float       m_pendingMoveX      = -1.f; // if >= 0, move window to this pos next frame via ImGui
    float       m_pendingMoveY      = -1.f;
    uint32_t    m_viewportId        = 0;     // ImGuiID of the viewport this window is in (updated each frame)
    int         m_selectedIdx       = -1;
    bool        m_moveListScrollPending = false; // request scroll-to-selected on next render
    bool        m_dirty            = false; // unsaved changes exist
    bool        m_pendingClose     = false; // close requested while dirty

    enum class SaveState { Idle, Saving, Done };
    SaveState          m_saveState        = SaveState::Idle;
    std::future<void>  m_saveFuture;                         // async save task
    bool               m_donePoppedFirst  = false;           // skip first-frame click on Done popup
    char        m_searchBuf[128] = {};

    ReqViewState       m_reqView;
    ExtradataViewState m_extradataView;

    // Subwindow states (open = only one instance allowed)
    bool                 m_reqWinOpen       = false;
    TwoLevelSel          m_reqWinSel;
    CancelsWinState      m_cancelsWin;
    bool                 m_hitCondWinOpen   = false;
    bool                 m_hitCondWinFocus  = false;
    TwoLevelSel          m_hitCondWinSel;
    ReactionListWinState m_reacWin;
    PushbackWinState     m_pushbackWin;
    bool                 m_voiceclipWinOpen   = false;
    bool                 m_voiceclipWinFocus  = false;
    bool                 m_voiceclipWinScroll = false;
    int                  m_voiceclipWinSel    = 0;
    PropertiesWinState   m_propertiesWin;
    InputSeqWinState     m_inputSeqWin;
    ProjectileWinState   m_projectileWin;
    ThrowsWinState       m_throwsWin;
    bool                 m_parryWinOpen   = false;
    TwoLevelSel          m_parryWinSel;
    bool                 m_dialogueWinOpen = false;
    int                  m_dialogueSel     = 0;
    RefFinderState       m_refFinder;

    // Animation name DB (anim_N <-> motbin anim_key, loaded from .tkedit/anim_names.json)
    AnimNameDB           m_animNameDB;
    void                 TryInitAnimNameDB();

    // Animation Manager
    std::string          m_movesetName;
    int                  m_uid            = 0;
    std::unique_ptr<AnimationManagerWindow> m_animMgr;

    // D3D11 context for the 3D preview (non-owning)
    ID3D11Device*        m_d3dDev = nullptr;
    ID3D11DeviceContext* m_d3dCtx = nullptr;

    // anim_key InputText state
    char                 m_animKeyBuf[32]    = {};
    int                  m_animKeyBufIdx     = -1;  // m_selectedIdx when buf was last built

};
