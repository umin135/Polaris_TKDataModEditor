#pragma once
#include "imgui/imgui.h"
#include "fbsdata/editor/FbsDataView.h"
#include "moveset/editor/MovesetView.h"
#include "moveset/editor/MovesetEditorWindow.h"
#include "extract/ExtractorView.h"
#include <string>
#include <vector>
#include <future>

#ifdef _DEBUG
#include "devmode/FbsDevView.h"
#include "devmode/MotbinDiffView.h"
#endif

// Forward declarations to avoid pulling d3d11.h into all translation units
struct ID3D11Device;
struct ID3D11DeviceContext;
struct ID3D11ShaderResourceView;

// Main application class
// Layout: left sidebar (fixed width) | right content area (switches by active view)
class App
{
public:
    App(ID3D11Device* device, ID3D11DeviceContext* ctx);
    ~App();

    // Called every frame to render all UI
    void Render();

    // Open a .tkmod file directly and switch to the FbsData view.
    // Called at startup when the app is launched via double-click.
    void OpenFile(const std::string& path);

private:
    // Which view is shown in the right content area
    enum class ContentView {
        Home, FbsData, Moveset,
#ifdef _DEBUG
        FbsDevMode,
        MotbinDiff,
#endif
    };

    // Apply custom ImGui color/style settings
    void ApplyStyle();

    // Main layout: sidebar + content
    void RenderMainLayout();
    void RenderSidebar(float sidebarWidth);

    // Right-side content views
    void RenderHomeView();
    void RenderFbsDataView();
    void RenderMovesetView();
#ifdef _DEBUG
    void RenderFbsDevView();
    void RenderMotbinDiffView();
#endif

    // Floating settings window
    void RenderSettingsWindow();
    void ApplyAndSaveSettings();

    // Loading overlay (dim background + centered spinner popup)
    void RenderLoadingOverlay(const char* msg);

    ContentView  m_currentView = ContentView::Home;
    FbsDataView  m_fbsDataView;
    MovesetView  m_movesetView;
    ExtractorView m_extractorView{ "" };
    std::vector<MovesetEditorWindow> m_editorWindows;
    int          m_nextEditorUid = 0;
#ifdef _DEBUG
    FbsDevView      m_fbsDevView;
    MotbinDiffView  m_motbinDiffView;
#endif

    // D3D11 device / context (non-owning, for preview renderers)
    ID3D11Device*        m_d3dDev = nullptr;
    ID3D11DeviceContext* m_d3dCtx = nullptr;

    // Home screen logo texture (loaded from res/Home_Logo.png)
    ID3D11ShaderResourceView* m_logoTex  = nullptr;
    ImVec2                    m_logoSize = { 0.0f, 0.0f };

    // Async editor loading
    std::future<MovesetEditorWindow> m_pendingEditor;
    bool        m_loadingActive  = false;
    std::string m_loadingMessage;

    // Settings window state
    bool m_showSettings      = false;
    bool m_settingsInitialized = false;
    int  m_settingsCat       = 1;     // 0 = fbsdata, 1 = moveset
    char m_settingsMovesetRoot[1024]  = {};
};
