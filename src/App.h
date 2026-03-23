#pragma once
#include "imgui/imgui.h"
#include "editors/FbsDataView.h"
#include <string>

#ifdef _DEBUG
#include "devmode/FbsDevView.h"
#endif

// Forward declarations to avoid pulling d3d11.h into all translation units
struct ID3D11Device;
struct ID3D11ShaderResourceView;

// Main application class
// Layout: left sidebar (fixed width) | right content area (switches by active view)
class App
{
public:
    explicit App(ID3D11Device* device);
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
#endif

    ContentView  m_currentView = ContentView::Home;
    FbsDataView  m_fbsDataView;
#ifdef _DEBUG
    FbsDevView   m_fbsDevView;
#endif

    // Home screen logo texture (loaded from res/Home_Logo.png)
    ID3D11ShaderResourceView* m_logoTex  = nullptr;
    ImVec2                    m_logoSize = { 0.0f, 0.0f };
};
