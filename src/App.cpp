// Main application rendering logic
#include "App.h"
#include "Config.h"
#include "moveset/LabelDB.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"  // DockBuilder API
#include <functional>

#include <d3d11.h>
#include <wincodec.h>
#include <windows.h>
#include <shobjidl.h>   // IFileDialog for folder picker
#include <vector>
#pragma comment(lib, "windowscodecs.lib")

#include "resource.h"

static const char* APP_VERSION   = "v0.1.0";
static const float SIDEBAR_WIDTH = 200.0f;

// -------------------------------------------------------------
//  PNG texture loader from embedded Win32 resource (RCDATA)
//  Uses WIC memory stream -- no external file needed.
// -------------------------------------------------------------

static ID3D11ShaderResourceView* LoadTexturePNGFromResource(
    ID3D11Device* device, int resourceId, int* outW, int* outH)
{
    *outW = *outH = 0;

    // Locate and lock the embedded PNG bytes
    HRSRC   hRes  = FindResource(nullptr, MAKEINTRESOURCE(resourceId), RT_RCDATA);
    if (!hRes) return nullptr;
    HGLOBAL hGlob = LoadResource(nullptr, hRes);
    if (!hGlob) return nullptr;
    void*  pData = LockResource(hGlob);
    DWORD  nSize = SizeofResource(nullptr, hRes);
    if (!pData || !nSize) return nullptr;

    IWICImagingFactory* wic = nullptr;
    if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                IID_PPV_ARGS(&wic))))
        return nullptr;

    // Feed PNG bytes into a WIC memory stream
    IWICStream* wicStream = nullptr;
    wic->CreateStream(&wicStream);
    wicStream->InitializeFromMemory(reinterpret_cast<BYTE*>(pData), nSize);

    IWICBitmapDecoder* decoder = nullptr;
    if (FAILED(wic->CreateDecoderFromStream(wicStream, nullptr,
                                             WICDecodeMetadataCacheOnDemand, &decoder)))
    {
        wicStream->Release();
        wic->Release();
        return nullptr;
    }

    IWICBitmapFrameDecode* frame = nullptr;
    decoder->GetFrame(0, &frame);

    IWICFormatConverter* conv = nullptr;
    wic->CreateFormatConverter(&conv);
    conv->Initialize(frame, GUID_WICPixelFormat32bppRGBA,
                     WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom);

    UINT w = 0, h = 0;
    conv->GetSize(&w, &h);

    std::vector<BYTE> pixels(w * h * 4);
    conv->CopyPixels(nullptr, w * 4, (UINT)pixels.size(), pixels.data());

    // Upload to DX11 texture
    D3D11_TEXTURE2D_DESC td = {};
    td.Width            = w;
    td.Height           = h;
    td.MipLevels        = 1;
    td.ArraySize        = 1;
    td.Format           = DXGI_FORMAT_R8G8B8A8_UNORM;
    td.SampleDesc.Count = 1;
    td.Usage            = D3D11_USAGE_DEFAULT;
    td.BindFlags        = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA sd = {};
    sd.pSysMem     = pixels.data();
    sd.SysMemPitch = w * 4;

    ID3D11Texture2D* tex = nullptr;
    device->CreateTexture2D(&td, &sd, &tex);

    ID3D11ShaderResourceView* srv = nullptr;
    if (tex)
    {
        device->CreateShaderResourceView(tex, nullptr, &srv);
        tex->Release();
    }

    conv->Release();
    frame->Release();
    decoder->Release();
    wicStream->Release();
    wic->Release();

    *outW = (int)w;
    *outH = (int)h;
    return srv;
}

// -------------------------------------------------------------
//  Construction / Style
// -------------------------------------------------------------

// -------------------------------------------------------------
//  Folder picker dialog (IFileDialog, FOS_PICKFOLDERS)
// -------------------------------------------------------------

static std::string BrowseForFolder()
{
    IFileDialog* pfd = nullptr;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
                                IID_PPV_ARGS(&pfd))))
        return {};

    DWORD opts = 0;
    pfd->GetOptions(&opts);
    pfd->SetOptions(opts | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);

    std::string result;
    if (SUCCEEDED(pfd->Show(nullptr)))
    {
        IShellItem* psi = nullptr;
        if (SUCCEEDED(pfd->GetResult(&psi)))
        {
            PWSTR path = nullptr;
            if (SUCCEEDED(psi->GetDisplayName(SIGDN_FILESYSPATH, &path)))
            {
                int len = WideCharToMultiByte(CP_UTF8, 0, path, -1, nullptr, 0, nullptr, nullptr);
                if (len > 1)
                {
                    result.resize(len - 1);
                    WideCharToMultiByte(CP_UTF8, 0, path, -1, &result[0], len, nullptr, nullptr);
                }
                CoTaskMemFree(path);
            }
            psi->Release();
        }
    }
    pfd->Release();
    return result;
}

App::App(ID3D11Device* device)
{
    ApplyStyle();
    Config::Get().Load();

    // Initialize extractor with configured root directory
    m_extractorView.SetDestFolder(Config::Get().data.movesetRootDir);

    // Auto-refresh moveset list after successful extraction
    m_extractorView.SetOnExtractSuccess([this]() {
        m_movesetView.ForceRefresh();
    });

    // Load InterfaceData labels (requirements, properties, commands)
    // Search for data directory relative to the exe, tolerating various build layouts
    {
        char exePath[MAX_PATH] = {};
        GetModuleFileNameA(nullptr, exePath, MAX_PATH);
        std::string exeDir = exePath;
        size_t sep = exeDir.rfind('\\');
        if (sep != std::string::npos) exeDir = exeDir.substr(0, sep);

        const std::string candidates[] = {
            exeDir + "\\data\\interfacedata",
            exeDir + "\\..\\data\\interfacedata",
            exeDir + "\\..\\..\\data\\interfacedata",
            exeDir + "\\..\\..\\..\\data\\interfacedata",
        };
        for (const auto& c : candidates)
        {
            LabelDB::Get().Load(c);
            if (LabelDB::Get().IsLoaded())
            {
                LabelDB::Get().LoadNames(c + "\\name_keys.json");
                LabelDB::Get().AddNames(c + "\\supplement_name_keys.json");
                LabelDB::Get().LoadAnimNames(c + "\\anim_keys.json");
                break;
            }
        }
    }

    // Load home screen logo from embedded resource (IDR_HOME_LOGO_PNG)
    int w = 0, h = 0;
    m_logoTex  = LoadTexturePNGFromResource(device, IDR_HOME_LOGO_PNG, &w, &h);
    m_logoSize = ImVec2((float)w, (float)h);
}

App::~App()
{
    if (m_logoTex) { m_logoTex->Release(); m_logoTex = nullptr; }
}

void App::ApplyStyle()
{
    ImGuiStyle& style  = ImGui::GetStyle();
    ImVec4*     colors = style.Colors;

    // Dark base with a subtle blue tint
    colors[ImGuiCol_WindowBg]             = ImVec4(0.10f, 0.10f, 0.13f, 1.00f);
    colors[ImGuiCol_ChildBg]              = ImVec4(0.12f, 0.12f, 0.16f, 1.00f);
    colors[ImGuiCol_PopupBg]              = ImVec4(0.15f, 0.15f, 0.20f, 1.00f);
    colors[ImGuiCol_Border]               = ImVec4(0.22f, 0.22f, 0.32f, 1.00f);
    colors[ImGuiCol_FrameBg]              = ImVec4(0.18f, 0.18f, 0.24f, 1.00f);
    colors[ImGuiCol_FrameBgHovered]       = ImVec4(0.26f, 0.26f, 0.36f, 1.00f);
    colors[ImGuiCol_FrameBgActive]        = ImVec4(0.30f, 0.30f, 0.42f, 1.00f);
    colors[ImGuiCol_TitleBg]              = ImVec4(0.08f, 0.08f, 0.12f, 1.00f);
    colors[ImGuiCol_TitleBgActive]        = ImVec4(0.13f, 0.13f, 0.20f, 1.00f);
    colors[ImGuiCol_MenuBarBg]            = ImVec4(0.10f, 0.10f, 0.14f, 1.00f);
    colors[ImGuiCol_ScrollbarBg]          = ImVec4(0.08f, 0.08f, 0.10f, 1.00f);
    colors[ImGuiCol_ScrollbarGrab]        = ImVec4(0.28f, 0.28f, 0.40f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.38f, 0.38f, 0.52f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive]  = ImVec4(0.44f, 0.44f, 0.60f, 1.00f);
    colors[ImGuiCol_Button]               = ImVec4(0.20f, 0.20f, 0.28f, 1.00f);
    colors[ImGuiCol_ButtonHovered]        = ImVec4(0.32f, 0.32f, 0.48f, 1.00f);
    colors[ImGuiCol_ButtonActive]         = ImVec4(0.38f, 0.38f, 0.56f, 1.00f);
    colors[ImGuiCol_Header]               = ImVec4(0.26f, 0.36f, 0.60f, 1.00f);
    colors[ImGuiCol_HeaderHovered]        = ImVec4(0.35f, 0.46f, 0.72f, 1.00f);
    colors[ImGuiCol_HeaderActive]         = ImVec4(0.40f, 0.52f, 0.80f, 1.00f);
    colors[ImGuiCol_Separator]            = ImVec4(0.22f, 0.22f, 0.32f, 1.00f);
    colors[ImGuiCol_SeparatorHovered]     = ImVec4(0.40f, 0.40f, 0.60f, 1.00f);
    colors[ImGuiCol_SeparatorActive]      = ImVec4(0.50f, 0.50f, 0.75f, 1.00f);
    colors[ImGuiCol_Text]                 = ImVec4(0.90f, 0.90f, 0.94f, 1.00f);
    colors[ImGuiCol_TextDisabled]         = ImVec4(0.40f, 0.40f, 0.52f, 1.00f);
    colors[ImGuiCol_CheckMark]            = ImVec4(0.55f, 0.75f, 1.00f, 1.00f);
    colors[ImGuiCol_SliderGrab]           = ImVec4(0.50f, 0.68f, 1.00f, 1.00f);
    colors[ImGuiCol_SliderGrabActive]     = ImVec4(0.60f, 0.80f, 1.00f, 1.00f);
    colors[ImGuiCol_TableHeaderBg]        = ImVec4(0.14f, 0.14f, 0.20f, 1.00f);
    colors[ImGuiCol_TableBorderStrong]    = ImVec4(0.28f, 0.28f, 0.40f, 1.00f);
    colors[ImGuiCol_TableBorderLight]     = ImVec4(0.20f, 0.20f, 0.28f, 1.00f);
    colors[ImGuiCol_TableRowBg]           = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_TableRowBgAlt]        = ImVec4(1.00f, 1.00f, 1.00f, 0.03f);

    style.WindowRounding    = 5.0f;
    style.ChildRounding     = 4.0f;
    style.FrameRounding     = 3.0f;
    style.PopupRounding     = 4.0f;
    style.ScrollbarRounding = 3.0f;
    style.GrabRounding      = 3.0f;
    style.TabRounding       = 3.0f;

    style.WindowPadding = ImVec2(10.0f, 10.0f);
    style.FramePadding  = ImVec2(6.0f,  4.0f);
    style.ItemSpacing   = ImVec2(8.0f,  6.0f);
    style.ScrollbarSize = 14.0f;
}

// -------------------------------------------------------------
//  Frame entry point
// -------------------------------------------------------------

void App::Render()
{
    // Prevent any DockNode host windows (internally created by ImGui's docking
    // system) from being brought to front on focus -- otherwise clicking the
    // docked content area would cover the floating moveset editor windows.
    {
        ImGuiContext* ctx = ImGui::GetCurrentContext();
        for (int i = 0; i < ctx->Windows.Size; ++i)
        {
            ImGuiWindow* w = ctx->Windows[i];
            if (w->Flags & ImGuiWindowFlags_DockNodeHost)
                w->Flags |= ImGuiWindowFlags_NoBringToFrontOnFocus;
        }
    }

    // Anchor the host window to the main OS viewport so it works correctly
    // with multi-viewport mode (windows dragged outside the main Win32 window)
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->WorkPos);
    ImGui::SetNextWindowSize(vp->WorkSize);
    ImGui::SetNextWindowViewport(vp->ID);

    constexpr ImGuiWindowFlags hostFlags =
        ImGuiWindowFlags_NoTitleBar           |
        ImGuiWindowFlags_NoCollapse           |
        ImGuiWindowFlags_NoResize             |
        ImGuiWindowFlags_NoMove               |
        ImGuiWindowFlags_NoScrollbar          |
        ImGuiWindowFlags_NoScrollWithMouse    |
        ImGuiWindowFlags_NoBringToFrontOnFocus|
        ImGuiWindowFlags_NoNavFocus           |
        ImGuiWindowFlags_NoDocking            |  // the host itself must not be dockable
        ImGuiWindowFlags_NoSavedSettings;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,   0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,    ImVec2(0.0f, 0.0f));
    ImGui::Begin("##Host", nullptr, hostFlags);
    ImGui::PopStyleVar(3);

    // -- DockSpace ----------------------------------------------
    ImGuiID dockspaceId = ImGui::GetID("##MainDockSpace");

    // Lock the built-in layout: no tab bar, no undocking, no resizing, no new splits
    // ImGuiDockNodeFlags_NoTabBar is an internal flag (imgui_internal.h) that removes
    // the tab bar completely -- including the chevron/menu button that AutoHideTabBar leaves.
    const ImGuiDockNodeFlags dockspaceFlags =
        (ImGuiDockNodeFlags)ImGuiDockNodeFlags_NoTabBar |   // completely remove tab bar UI
        ImGuiDockNodeFlags_NoUndocking                  |   // prevent dragging windows out
        ImGuiDockNodeFlags_NoResize                     |   // prevent moving the divider
        ImGuiDockNodeFlags_NoDockingSplit;                  // prevent creating new splits
    ImGui::DockSpace(dockspaceId, ImVec2(0.0f, 0.0f), dockspaceFlags);

    // Build the default layout once -- only when no .ini state has been loaded
    static bool layoutReady = false;
    if (!layoutReady)
    {
        layoutReady = true;
        ImGuiDockNode* node = ImGui::DockBuilderGetNode(dockspaceId);
        if (!node || !node->IsSplitNode())
        {
            ImGui::DockBuilderRemoveNode(dockspaceId);
            ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace);
            ImGui::DockBuilderSetNodeSize(dockspaceId, vp->WorkSize);

            ImGuiID sidebarNodeId, contentNodeId;
            ImGui::DockBuilderSplitNode(
                dockspaceId, ImGuiDir_Left,
                SIDEBAR_WIDTH / vp->WorkSize.x,
                &sidebarNodeId, &contentNodeId);

            ImGui::DockBuilderDockWindow("##Sidebar", sidebarNodeId);
            ImGui::DockBuilderDockWindow("##Content", contentNodeId);
            ImGui::DockBuilderFinish(dockspaceId);
        }
    }

    ImGui::End();

    RenderMainLayout();
    RenderSettingsWindow();

    // Render all open moveset editor windows; remove closed ones
    auto it = m_editorWindows.begin();
    while (it != m_editorWindows.end())
    {
        if (!it->Render())
            it = m_editorWindows.erase(it);
        else
            ++it;
    }
}

// -------------------------------------------------------------
//  Main layout: sidebar | divider | content
// -------------------------------------------------------------

void App::RenderMainLayout()
{
    // -- Sidebar window -----------------------------------------
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.07f, 0.07f, 0.10f, 1.00f));
    ImGui::Begin("##Sidebar", nullptr,
        ImGuiWindowFlags_NoTitleBar           |
        ImGuiWindowFlags_NoCollapse           |
        ImGuiWindowFlags_NoMove               |
        ImGuiWindowFlags_NoScrollbar          |
        ImGuiWindowFlags_NoScrollWithMouse    |
        ImGuiWindowFlags_NoBringToFrontOnFocus);
    ImGui::PopStyleColor();

    RenderSidebar(ImGui::GetContentRegionAvail().x);
    ImGui::End();

    // -- Content window -----------------------------------------
    ImGui::Begin("##Content", nullptr,
        ImGuiWindowFlags_NoTitleBar           |
        ImGuiWindowFlags_NoCollapse           |
        ImGuiWindowFlags_NoMove               |
        ImGuiWindowFlags_NoScrollbar          |
        ImGuiWindowFlags_NoScrollWithMouse    |
        ImGuiWindowFlags_NoBringToFrontOnFocus);

    switch (m_currentView)
    {
    case ContentView::Home:    RenderHomeView();    break;
    case ContentView::FbsData: RenderFbsDataView(); break;
    case ContentView::Moveset: RenderMovesetView(); break;
#ifdef _DEBUG
    case ContentView::FbsDevMode: RenderFbsDevView();     break;
    case ContentView::MotbinDiff: RenderMotbinDiffView(); break;
#endif
    }
    ImGui::End();
}

// -------------------------------------------------------------
//  Left sidebar
// -------------------------------------------------------------

void App::RenderSidebar(float sidebarWidth)
{
    const float buttonW  = sidebarWidth - 16.0f;
    const float buttonH  = 36.0f;
    const float paddingX = 8.0f;

    // -- App logo / short title --
    ImGui::SetCursorPosY(14.0f);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.65f, 0.82f, 1.00f, 1.00f));
    const char* logoText = "PolarisTK";
    ImGui::SetCursorPosX((sidebarWidth - ImGui::CalcTextSize(logoText).x) * 0.5f);
    ImGui::Text("%s", logoText);
    ImGui::PopStyleColor();

    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.38f, 0.38f, 0.50f, 1.00f));
    const char* subText = "Data Editor";
    ImGui::SetCursorPosX((sidebarWidth - ImGui::CalcTextSize(subText).x) * 0.5f);
    ImGui::Text("%s", subText);
    ImGui::PopStyleColor();

    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 8.0f);
    ImGui::Separator();
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 10.0f);

    // -- Helper: sidebar button with active highlight --
    auto SidebarBtn = [&](const char* label, ContentView view, bool enabled) -> bool
    {
        bool isActive = (m_currentView == view);
        bool clicked  = false;
        ImGui::SetCursorPosX(paddingX);

        if (isActive)
        {
            ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.22f, 0.40f, 0.72f, 1.00f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.28f, 0.48f, 0.82f, 1.00f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.25f, 0.44f, 0.78f, 1.00f));
        }

        if (!enabled)
            ImGui::BeginDisabled();

        if (ImGui::Button(label, ImVec2(buttonW, buttonH)))
            clicked = true;

        if (!enabled)
            ImGui::EndDisabled();

        if (isActive)
            ImGui::PopStyleColor(3);

        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4.0f);
        return clicked;
    };

    // -- Navigation buttons --
    if (SidebarBtn("Home",    ContentView::Home,    true))
        m_currentView = ContentView::Home;

    ImGui::Separator();
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 6.0f);

    if (SidebarBtn("fbsdata", ContentView::FbsData, true))
        m_currentView = ContentView::FbsData;

    if (SidebarBtn("moveset", ContentView::Moveset, true))
        m_currentView = ContentView::Moveset;

#ifdef _DEBUG
    ImGui::Separator();
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 6.0f);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.75f, 0.2f, 1.0f));
    ImGui::SetCursorPosX(paddingX);
    ImGui::TextUnformatted("-- DEV MODE --");
    ImGui::PopStyleColor();
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 2.0f);
    if (SidebarBtn("fbsdata (dev)", ContentView::FbsDevMode, true))
        m_currentView = ContentView::FbsDevMode;
    if (SidebarBtn("motbin diff",  ContentView::MotbinDiff, true))
        m_currentView = ContentView::MotbinDiff;
#endif

    // -- Settings button -- fixed at the bottom of the sidebar --
    {
        const float settingsH    = buttonH + 4.0f;
        const float versionH     = 22.0f;
        const float sepH         = ImGui::GetStyle().ItemSpacing.y + 1.0f + 6.0f;
        const float remaining    = ImGui::GetContentRegionAvail().y;
        const float reservedBot  = settingsH + sepH + versionH;

        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + remaining - reservedBot);
        ImGui::Separator();
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 6.0f);

        // Settings acts as a toggle, not a ContentView switch -- give it its own highlight
        if (m_showSettings)
        {
            ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.22f, 0.40f, 0.72f, 1.00f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.28f, 0.48f, 0.82f, 1.00f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.25f, 0.44f, 0.78f, 1.00f));
        }
        ImGui::SetCursorPosX(paddingX);
        if (ImGui::Button("Settings", ImVec2(buttonW, buttonH)))
        {
            m_showSettings       = !m_showSettings;
            m_settingsInitialized = false;  // re-init edit buffers on next open
        }
        if (m_showSettings)
            ImGui::PopStyleColor(3);
    }

    // -- Version label --
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.28f, 0.28f, 0.38f, 1.00f));
    const float verW = ImGui::CalcTextSize(APP_VERSION).x;
    ImGui::SetCursorPosX((sidebarWidth - verW) * 0.5f);
    ImGui::Text("%s", APP_VERSION);
    ImGui::PopStyleColor();
}

// -------------------------------------------------------------
//  Home view
// -------------------------------------------------------------

void App::RenderHomeView()
{
    const float contentW = ImGui::GetContentRegionAvail().x;
    const float contentH = ImGui::GetContentRegionAvail().y;

    // -- Logo / Title block (vertically centered at ~22%) --
    ImGui::SetCursorPosY(contentH * 0.22f);

    if (m_logoTex && m_logoSize.x > 0.0f)
    {
        // Scale down proportionally if the logo is wider than 60% of the content area
        float scale = 1.0f;
        const float maxW = contentW * 0.60f;
        if (m_logoSize.x > maxW)
            scale = maxW / m_logoSize.x;
        const ImVec2 displaySize(m_logoSize.x * scale, m_logoSize.y * scale);

        ImGui::SetCursorPosX((contentW - displaySize.x) * 0.5f);
        ImGui::Image((ImTextureID)(intptr_t)m_logoTex, displaySize);
    }
    else
    {
        // Fallback text title when logo texture is unavailable
        const char* mainTitle = "PolarisTK Data Editor";
        ImGui::SetWindowFontScale(2.0f);
        const float titleW = ImGui::CalcTextSize(mainTitle).x;
        ImGui::SetCursorPosX((contentW - titleW) * 0.5f);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.65f, 0.82f, 1.00f, 1.00f));
        ImGui::Text("%s", mainTitle);
        ImGui::PopStyleColor();
        ImGui::SetWindowFontScale(1.0f);
    }

    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 6.0f);

    const char* subtitle = "Tekken 8 mod data editor tool";
    const float subW = ImGui::CalcTextSize(subtitle).x;
    ImGui::SetCursorPosX((contentW - subW) * 0.5f);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.60f, 0.60f, 0.72f, 1.00f));
    ImGui::Text("%s", subtitle);
    ImGui::PopStyleColor();

    // -- Separator --
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 24.0f);
    ImGui::Separator();
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 20.0f);

    // -- Centered text helper --
    auto CenteredText = [&](const char* text, ImVec4 color)
    {
        float w = ImGui::CalcTextSize(text).x;
        ImGui::SetCursorPosX((contentW - w) * 0.5f);
        ImGui::PushStyleColor(ImGuiCol_Text, color);
        ImGui::Text("%s", text);
        ImGui::PopStyleColor();
    };

    // -- Supported modules --
    CenteredText("Supported Modules", ImVec4(0.75f, 0.75f, 0.85f, 1.00f));
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 8.0f);

    const float tableW = 440.0f;
    ImGui::SetCursorPosX((contentW - tableW) * 0.5f);

    if (ImGui::BeginTable("##modules", 2,
        ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersInnerH |
        ImGuiTableFlags_RowBg, ImVec2(tableW, 0)))
    {
        ImGui::TableSetupColumn("Module",      ImGuiTableColumnFlags_WidthFixed, 100.0f);
        ImGui::TableSetupColumn("Description", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();

        auto ModuleRow = [&](const char* name, const char* desc, bool ready)
        {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::PushStyleColor(ImGuiCol_Text,
                ready ? ImVec4(0.60f, 0.90f, 0.60f, 1.00f)
                      : ImVec4(0.45f, 0.45f, 0.58f, 1.00f));
            ImGui::Text("%s", name);
            ImGui::PopStyleColor();
            ImGui::TableSetColumnIndex(1);
            ImGui::PushStyleColor(ImGuiCol_Text,
                ready ? ImVec4(0.85f, 0.85f, 0.92f, 1.00f)
                      : ImVec4(0.42f, 0.42f, 0.54f, 1.00f));
            ImGui::Text("%s", desc);
            ImGui::PopStyleColor();
        };

        ModuleRow("fbsdata", "Customization item data editor (.tkmod)", true);
        ModuleRow("moveset", "Move / animation data editor (planned)",  false);
        ModuleRow("ghost",   "Ghost data editor (low priority)",         false);

        ImGui::EndTable();
    }

    // -- Credits / Links --
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 32.0f);
    ImGui::Separator();
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 16.0f);

    // Helper: clickable hyperlink text, centered
    auto LinkText = [&](const char* label, const char* url)
    {
        const ImVec4 colNormal = ImVec4(0.45f, 0.72f, 1.00f, 1.00f);
        const ImVec4 colHover  = ImVec4(0.70f, 0.88f, 1.00f, 1.00f);
        float w = ImGui::CalcTextSize(label).x;
        ImGui::SetCursorPosX((contentW - w) * 0.5f);
        bool hovered = false;
        ImGui::PushStyleColor(ImGuiCol_Text, colNormal);
        ImGui::TextUnformatted(label);
        ImGui::PopStyleColor();
        hovered = ImGui::IsItemHovered();
        if (hovered)
        {
            // Draw underline
            ImVec2 min = ImGui::GetItemRectMin();
            ImVec2 max = ImGui::GetItemRectMax();
            ImGui::GetWindowDrawList()->AddLine(
                ImVec2(min.x, max.y), ImVec2(max.x, max.y),
                ImGui::ColorConvertFloat4ToU32(colHover), 1.0f);
            ImGui::PushStyleColor(ImGuiCol_Text, colHover);
            // Redraw with hover color by overlaying (cursor already past item)
            ImGui::PopStyleColor();
            ImGui::SetTooltip("%s", url);
        }
        if (ImGui::IsItemClicked())
            ShellExecuteA(nullptr, "open", url, nullptr, nullptr, SW_SHOWNORMAL);
    };

    // Helper: contributor row, centered  "Name  -  Role"
    auto CreditRow = [&](const char* name, const char* role)
    {
        char buf[256];
        snprintf(buf, sizeof(buf), "%s  -  %s", name, role);
        float w = ImGui::CalcTextSize(buf).x;
        ImGui::SetCursorPosX((contentW - w) * 0.5f);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.65f, 0.82f, 1.00f, 1.00f));
        ImGui::TextUnformatted(name);
        ImGui::PopStyleColor();
        ImGui::SameLine(0.0f, 0.0f);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.50f, 0.50f, 0.62f, 1.00f));
        ImGui::Text("  -  %s", role);
        ImGui::PopStyleColor();
    };

    CenteredText("Credits", ImVec4(0.75f, 0.75f, 0.85f, 1.00f));
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 8.0f);
    CreditRow("UMIN",    "Editor development");
    CreditRow("Ali",     "Game Reversing");
    CreditRow("dawc17",  "Game reversing / Mod Loader development");

    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 20.0f);
    CenteredText("Links", ImVec4(0.75f, 0.75f, 0.85f, 1.00f));
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 8.0f);
    LinkText("TekkenMods",       "https://tekkenmods.com/");
    LinkText("ModdingZaibatsu",  "https://discord.gg/nCAeJE4z5U");
    LinkText("Github",           "https://github.com/umin135/Polaris_TKDataModEditor");
}

// -------------------------------------------------------------
//  FbsData editor view
// -------------------------------------------------------------

void App::OpenFile(const std::string& path)
{
    if (m_fbsDataView.LoadFromPath(path))
        m_currentView = ContentView::FbsData;
}

void App::RenderFbsDataView()
{
    m_fbsDataView.Render();
}

// -------------------------------------------------------------
//  Moveset editor view  (placeholder)
// -------------------------------------------------------------

#ifdef _DEBUG
void App::RenderFbsDevView()
{
    m_fbsDevView.Render();
}

void App::RenderMotbinDiffView()
{
    m_motbinDiffView.Render();
}
#endif

void App::RenderMovesetView()
{
    // -- Extract buttons (top) --------------------------------
    m_extractorView.RenderButtons();
    ImGui::Spacing();

    // -- Moveset folder list (height-capped to leave room for log) --
    const float logH  = ImGui::GetTextLineHeightWithSpacing() * 5.0f;
    const float listH = ImGui::GetContentRegionAvail().y - logH;
    ImGui::BeginChild("##movesetListRegion", ImVec2(0.0f, listH), false);
    m_movesetView.Render();
    std::string openPath, openName;
    const bool wantOpen = m_movesetView.TakePendingOpen(openPath, openName);
    ImGui::EndChild();

    // Open a new editor window if the user clicked Edit on a moveset
    if (wantOpen)
        m_editorWindows.emplace_back(openPath, openName, m_nextEditorUid++);

    // -- Extraction log (bottom) ------------------------------
    ImGui::Spacing();
    m_extractorView.RenderLog();
}

// -------------------------------------------------------------
//  Settings window (floating -- not docked)
// -------------------------------------------------------------

void App::ApplyAndSaveSettings()
{
    AppConfig& cfg      = Config::Get().data;
    cfg.movesetRootDir  = m_settingsMovesetRoot;
    Config::Get().Save();
    m_movesetView.ForceRefresh();
    m_extractorView.SetDestFolder(cfg.movesetRootDir);
}

void App::RenderSettingsWindow()
{
    if (!m_showSettings)
    {
        m_settingsInitialized = false;
        return;
    }

    // Populate edit buffers the first time the window appears
    if (!m_settingsInitialized)
    {
        m_settingsInitialized = true;
        const AppConfig& cfg = Config::Get().data;
        strncpy_s(m_settingsMovesetRoot, sizeof(m_settingsMovesetRoot),
                  cfg.movesetRootDir.c_str(), _TRUNCATE);
        m_settingsCat = 1;  // default to moveset category
    }

    ImGui::SetNextWindowSize(ImVec2(580.0f, 360.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(ImVec2(480.0f, 280.0f), ImVec2(FLT_MAX, FLT_MAX));

    // Prevent this window from accidentally docking into the main DockSpace
    constexpr ImGuiWindowFlags kSettingsFlags =
        ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoCollapse;

    if (!ImGui::Begin("Settings", &m_showSettings, kSettingsFlags))
    {
        ImGui::End();
        return;
    }

    const ImGuiStyle& style = ImGui::GetStyle();

    // -- Left: category list ---------------------------------
    const float catW = 110.0f;
    ImGui::BeginChild("##settingsCats", ImVec2(catW, -1.0f), true);

    const char* kCats[] = { "fbsdata", "moveset" };
    for (int i = 0; i < 2; ++i)
    {
        if (ImGui::Selectable(kCats[i], m_settingsCat == i))
            m_settingsCat = i;
    }

    ImGui::EndChild();
    ImGui::SameLine();

    // -- Right: content + buttons ----------------------------
    ImGui::BeginChild("##settingsRight", ImVec2(0.0f, 0.0f));
    {
        // Reserve bottom area for separator + buttons
        const float btnRowH  = ImGui::GetFrameHeightWithSpacing();
        const float botH     = style.ItemSpacing.y + 1.0f + style.ItemSpacing.y + btnRowH + style.WindowPadding.y;

        ImGui::BeginChild("##settingsContent", ImVec2(0.0f, -botH));

        if (m_settingsCat == 0)
        {
            // fbsdata settings -- placeholder
            ImGui::TextDisabled("No fbsdata settings available yet.");
        }
        else if (m_settingsCat == 1)
        {
            // Moveset settings
            ImGui::TextUnformatted("Moveset Root Directory");
            ImGui::Spacing();
            ImGui::TextDisabled(
                "The folder that contains your moveset subfolders.\n"
                "Each subfolder with a moveset.* file is treated as one moveset.");
            ImGui::Spacing();

            ImGui::SetNextItemWidth(-76.0f);
            ImGui::InputText("##movesetRoot", m_settingsMovesetRoot, sizeof(m_settingsMovesetRoot));
            ImGui::SameLine();
            if (ImGui::Button("Browse", ImVec2(68.0f, 0.0f)))
            {
                std::string folder = BrowseForFolder();
                if (!folder.empty())
                    strncpy_s(m_settingsMovesetRoot, sizeof(m_settingsMovesetRoot),
                              folder.c_str(), _TRUNCATE);
            }
        }

        ImGui::EndChild();

        // -- Bottom button row --------------------------------
        ImGui::Separator();
        ImGui::Spacing();

        const float btnW   = 130.0f;
        const float totalW = btnW * 2.0f + style.ItemSpacing.x;
        ImGui::SetCursorPosX(ImGui::GetContentRegionAvail().x - totalW + style.WindowPadding.x);

        if (ImGui::Button("Save", ImVec2(btnW, 0.0f)))
        {
            ApplyAndSaveSettings();
            m_showSettings = false;
        }
        ImGui::SameLine();
        if (ImGui::Button("Set as Default", ImVec2(btnW, 0.0f)))
        {
            ApplyAndSaveSettings();
        }
    }
    ImGui::EndChild();

    ImGui::End();
}
