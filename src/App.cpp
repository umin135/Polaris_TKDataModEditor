// Main application rendering logic
#include "App.h"
#include "imgui/imgui.h"

#include <d3d11.h>
#include <wincodec.h>
#include <windows.h>
#include <vector>
#pragma comment(lib, "windowscodecs.lib")

#include "resource.h"

static const char* APP_VERSION   = "v0.1.0";
static const float SIDEBAR_WIDTH = 200.0f;

// ─────────────────────────────────────────────────────────────
//  PNG texture loader from embedded Win32 resource (RCDATA)
//  Uses WIC memory stream — no external file needed.
// ─────────────────────────────────────────────────────────────

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

// ─────────────────────────────────────────────────────────────
//  Construction / Style
// ─────────────────────────────────────────────────────────────

App::App(ID3D11Device* device)
{
    ApplyStyle();

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

// ─────────────────────────────────────────────────────────────
//  Frame entry point
// ─────────────────────────────────────────────────────────────

void App::Render()
{
    ImGuiIO& io = ImGui::GetIO();

    // Full-screen borderless host window (acts as the application canvas)
    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::SetNextWindowBgAlpha(1.0f);

    constexpr ImGuiWindowFlags hostFlags =
        ImGuiWindowFlags_NoTitleBar           |
        ImGuiWindowFlags_NoResize             |
        ImGuiWindowFlags_NoMove               |
        ImGuiWindowFlags_NoScrollbar          |
        ImGuiWindowFlags_NoScrollWithMouse    |
        ImGuiWindowFlags_NoBringToFrontOnFocus|
        ImGuiWindowFlags_NoCollapse           |
        ImGuiWindowFlags_NoSavedSettings;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,   0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,    ImVec2(0.0f, 0.0f));
    ImGui::Begin("##Host", nullptr, hostFlags);
    ImGui::PopStyleVar(3);

    RenderMainLayout();

    ImGui::End();
}

// ─────────────────────────────────────────────────────────────
//  Main layout: sidebar | divider | content
// ─────────────────────────────────────────────────────────────

void App::RenderMainLayout()
{
    const float totalHeight = ImGui::GetContentRegionAvail().y;

    // Left sidebar panel (darker background)
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.07f, 0.07f, 0.10f, 1.00f));
    ImGui::BeginChild("##Sidebar", ImVec2(SIDEBAR_WIDTH, totalHeight), false,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PopStyleColor();
    RenderSidebar(SIDEBAR_WIDTH);
    ImGui::EndChild();

    ImGui::SameLine(0, 0);

    // 1px vertical divider
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.20f, 0.20f, 0.28f, 1.00f));
    ImGui::BeginChild("##Divider", ImVec2(1.0f, totalHeight), false);
    ImGui::PopStyleColor();
    ImGui::EndChild();

    ImGui::SameLine(0, 0);

    // Right content area — switches based on active view
    ImGui::BeginChild("##Content", ImVec2(0.0f, totalHeight), false);
    switch (m_currentView)
    {
    case ContentView::Home:    RenderHomeView();    break;
    case ContentView::FbsData: RenderFbsDataView(); break;
    case ContentView::Moveset: RenderMovesetView(); break;
#ifdef _DEBUG
    case ContentView::FbsDevMode: RenderFbsDevView(); break;
#endif
    }
    ImGui::EndChild();
}

// ─────────────────────────────────────────────────────────────
//  Left sidebar
// ─────────────────────────────────────────────────────────────

void App::RenderSidebar(float sidebarWidth)
{
    const float buttonW  = sidebarWidth - 16.0f;
    const float buttonH  = 36.0f;
    const float paddingX = 8.0f;

    // ── App logo / short title ──
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

    // ── Helper: sidebar button with active highlight ──
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

    // ── Navigation buttons ──
    if (SidebarBtn("Home",    ContentView::Home,    true))
        m_currentView = ContentView::Home;

    ImGui::Separator();
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 6.0f);

    if (SidebarBtn("fbsdata", ContentView::FbsData, true))
        m_currentView = ContentView::FbsData;

    if (SidebarBtn("moveset", ContentView::Moveset, false))
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
#endif

    // ── Version label at the very bottom ──
    const float remainingY = ImGui::GetContentRegionAvail().y;
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + remainingY - 22.0f);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.28f, 0.28f, 0.38f, 1.00f));
    const float verW = ImGui::CalcTextSize(APP_VERSION).x;
    ImGui::SetCursorPosX((sidebarWidth - verW) * 0.5f);
    ImGui::Text("%s", APP_VERSION);
    ImGui::PopStyleColor();
}

// ─────────────────────────────────────────────────────────────
//  Home view
// ─────────────────────────────────────────────────────────────

void App::RenderHomeView()
{
    const float contentW = ImGui::GetContentRegionAvail().x;
    const float contentH = ImGui::GetContentRegionAvail().y;

    // ── Logo / Title block (vertically centered at ~22%) ──
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

    // ── Separator ──
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 24.0f);
    ImGui::Separator();
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 20.0f);

    // ── Centered text helper ──
    auto CenteredText = [&](const char* text, ImVec4 color)
    {
        float w = ImGui::CalcTextSize(text).x;
        ImGui::SetCursorPosX((contentW - w) * 0.5f);
        ImGui::PushStyleColor(ImGuiCol_Text, color);
        ImGui::Text("%s", text);
        ImGui::PopStyleColor();
    };

    // ── Supported modules ──
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

    // ── Author / Links ──
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 32.0f);
    ImGui::Separator();
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 16.0f);

    CenteredText("Author", ImVec4(0.75f, 0.75f, 0.85f, 1.00f));
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 6.0f);
    CenteredText("Polaris", ImVec4(0.65f, 0.82f, 1.00f, 1.00f));

    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 20.0f);
    CenteredText("Links", ImVec4(0.75f, 0.75f, 0.85f, 1.00f));
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 6.0f);
    CenteredText("(links will be added here)", ImVec4(0.38f, 0.38f, 0.50f, 1.00f));
}

// ─────────────────────────────────────────────────────────────
//  FbsData editor view
// ─────────────────────────────────────────────────────────────

void App::OpenFile(const std::string& path)
{
    if (m_fbsDataView.LoadFromPath(path))
        m_currentView = ContentView::FbsData;
}

void App::RenderFbsDataView()
{
    m_fbsDataView.Render();
}

// ─────────────────────────────────────────────────────────────
//  Moveset editor view  (placeholder)
// ─────────────────────────────────────────────────────────────

#ifdef _DEBUG
void App::RenderFbsDevView()
{
    m_fbsDevView.Render();
}
#endif

void App::RenderMovesetView()
{
    const float contentW = ImGui::GetContentRegionAvail().x;
    const float contentH = ImGui::GetContentRegionAvail().y;

    ImGui::SetCursorPosY(contentH * 0.40f);

    const char* msg = "Moveset editor — not yet implemented";
    ImGui::SetCursorPosX((contentW - ImGui::CalcTextSize(msg).x) * 0.5f);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.40f, 0.40f, 0.52f, 1.00f));
    ImGui::Text("%s", msg);
    ImGui::PopStyleColor();
}
