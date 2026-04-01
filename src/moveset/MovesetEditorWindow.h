#pragma once
#include "MotbinData.h"
#include <string>
#include <unordered_map>

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

    // Called by RenderCancelSection (free static) -- must be public for that access
    void RenderCancelInnerDetail(
        ParsedCancel& c, int localIdx, uint32_t blockIdx,
        const std::vector<std::pair<uint32_t,uint32_t>>& gcGroups);

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

    MotbinData  m_data;
    std::unordered_map<int, std::string> m_customNames;
    std::string m_windowTitle;
    bool        m_open              = true;
    int         m_selectedIdx       = -1;
    bool        m_moveListScrollPending = false; // request scroll-to-selected on next render
    bool        m_dirty            = false; // unsaved changes exist
    bool        m_pendingClose     = false; // close requested while dirty

    enum class SaveState { Idle, Saving, Done };
    SaveState   m_saveState        = SaveState::Idle;
    bool        m_openSavingPopup  = false;
    bool        m_openDonePopup    = false;
    bool        m_doSaveThisFrame  = false; // two-frame delay: show popup first, save next
    bool        m_donePoppedFirst  = false; // skip first-frame click on Done popup
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
};
