// PolarisTK Data Editor - Entry point
// Win32 + DirectX 11 + ImGui application

#include <d3d11.h>
#include <tchar.h>
#include <objbase.h>
#include <shlobj.h>
#include <string>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "shell32.lib")


#include "imgui/imgui.h"
#include "imgui/backends/imgui_impl_win32.h"
#include "imgui/backends/imgui_impl_dx11.h"
#include "App.h"
#include "../resource.h"

// Global bold font -- loaded once at startup, used throughout the UI
ImFont* g_fontBold = nullptr;

// DX11 global state
static ID3D11Device*            g_pd3dDevice            = nullptr;
static ID3D11DeviceContext*     g_pd3dDeviceContext     = nullptr;
static IDXGISwapChain*          g_pSwapChain            = nullptr;
static UINT                     g_ResizeWidth           = 0;
static UINT                     g_ResizeHeight          = 0;
static ID3D11RenderTargetView*  g_mainRenderTargetView  = nullptr;

// -------------------------------------------------------------
//  .tkmod file association registration (HKCU, no admin required)
// -------------------------------------------------------------

static void RegisterFileAssociation()
{
    wchar_t exePath[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);

    // Build open command: "<exepath>" "%1"
    std::wstring openCmd = L"\"";
    openCmd += exePath;
    openCmd += L"\" \"%1\"";

    // Build icon path: "<exepath>",-101  (resource ID 101 = IDI_TKMOD_FILE)
    std::wstring iconPath = L"\"";
    iconPath += exePath;
    iconPath += L"\",-101";

    // Helper to create/open a key and optionally set its default value
    auto SetKey = [](const wchar_t* subKey, const wchar_t* value)
    {
        HKEY hKey = nullptr;
        RegCreateKeyExW(HKEY_CURRENT_USER, subKey, 0, nullptr,
                        REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, nullptr, &hKey, nullptr);
        if (hKey)
        {
            if (value)
                RegSetValueExW(hKey, nullptr, 0, REG_SZ,
                               reinterpret_cast<const BYTE*>(value),
                               static_cast<DWORD>((wcslen(value) + 1) * sizeof(wchar_t)));
            RegCloseKey(hKey);
        }
    };

    // .tkmod -> ProgID
    SetKey(L"Software\\Classes\\.tkmod",                                                       L"PolarisTKDataEditor.Document");
    // ProgID display name
    SetKey(L"Software\\Classes\\PolarisTKDataEditor.Document",                                 L"PolarisTK Mod Data");
    // File icon (embedded in this exe, resource ID 101)
    SetKey(L"Software\\Classes\\PolarisTKDataEditor.Document\\DefaultIcon",                    iconPath.c_str());
    // Open verb
    SetKey(L"Software\\Classes\\PolarisTKDataEditor.Document\\shell\\open\\command",           openCmd.c_str());

    // Notify Explorer to refresh file type associations
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
}

// -------------------------------------------------------------
//  Command-line argument parser
//  Returns the first argument as UTF-8 if one exists, else empty.
// -------------------------------------------------------------

static std::string ParseStartupFilePath()
{
    int argc = 0;
    wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv) return {};

    std::string result;
    if (argc >= 2)
    {
        const wchar_t* w = argv[1];
        const int len = WideCharToMultiByte(CP_UTF8, 0, w, -1, nullptr, 0, nullptr, nullptr);
        if (len > 1)
        {
            result.resize(len - 1);
            WideCharToMultiByte(CP_UTF8, 0, w, -1, &result[0], len, nullptr, nullptr);
        }
    }
    LocalFree(argv);
    return result;
}

// Forward declarations
static bool    CreateDeviceD3D(HWND hWnd);
static void    CleanupDeviceD3D();
static void    CreateRenderTarget();
static void    CleanupRenderTarget();
static LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int)
{
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    RegisterFileAssociation();
    const std::string startupFile = ParseStartupFilePath();

    // Register window class
    WNDCLASSEXW wc = {
        sizeof(wc), CS_CLASSDC, WndProc, 0L, 0L,
        GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr,
        L"PolarisTKDataEditor", nullptr
    };
    ::RegisterClassExW(&wc);

    // Create the main window (wide rectangle: 1280x720)
    HWND hwnd = ::CreateWindowW(
        wc.lpszClassName, L"PolarisTK Data Editor",
        WS_OVERLAPPEDWINDOW,
        100, 100, 1280, 720,
        nullptr, nullptr, wc.hInstance, nullptr
    );

    if (!CreateDeviceD3D(hwnd))
    {
        CleanupDeviceD3D();
        ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return 1;
    }

    ::ShowWindow(hwnd, SW_SHOWMAXIMIZED);
    ::UpdateWindow(hwnd);

    // Setup Dear ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
    io.IniFilename = "imgui.ini";

    ImGui::StyleColorsDark();

    // Load custom fonts from embedded Win32 resources (RCDATA)
    // g_fontBold is used for highlighted items (e.g. generic-ID moves in the move list)
    {
        ImGuiIO& ioRef = ImGui::GetIO();
        constexpr float kFontSize = 15.0f;

        auto LoadFontFromResource = [&](int resourceId) -> ImFont*
        {
            HRSRC   hRes  = FindResource(nullptr, MAKEINTRESOURCE(resourceId), RT_RCDATA);
            if (!hRes) return nullptr;
            HGLOBAL hGlob = LoadResource(nullptr, hRes);
            if (!hGlob) return nullptr;
            void*  data = LockResource(hGlob);
            DWORD  size = SizeofResource(nullptr, hRes);
            if (!data || size == 0) return nullptr;
            // ImGui takes ownership of a copy -- pass font_data_owned_by_atlas = false
            // so we keep the resource memory alive (it stays for the process lifetime)
            ImFontConfig cfg;
            cfg.FontDataOwnedByAtlas = false;
            return ioRef.Fonts->AddFontFromMemoryTTF(data, (int)size, kFontSize, &cfg);
        };

        // Regular -- becomes the ImGui default font (index 0)
        ImFont* fontRegular = LoadFontFromResource(IDR_FONT_REGULAR);
        if (!fontRegular)
            ioRef.Fonts->AddFontDefault();

        // Bold
        g_fontBold = LoadFontFromResource(IDR_FONT_BOLD);
        if (!g_fontBold)
            g_fontBold = ioRef.Fonts->Fonts[0]; // fallback to regular
    }

    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    // Create application instance -- pass DX11 device for texture loading
    App app(g_pd3dDevice);

    // When viewports are enabled, platform windows must have no rounding
    // and a fully opaque background (DWM handles transparency separately)
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        ImGuiStyle& style = ImGui::GetStyle();
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }

    // If launched by double-clicking a .tkmod file, load it immediately
    if (!startupFile.empty())
        app.OpenFile(startupFile);

    // Main loop
    bool done = false;
    while (!done)
    {
        MSG msg;
        while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE))
        {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            if (msg.message == WM_QUIT)
                done = true;
        }
        if (done)
            break;

        // Handle window resize
        if (g_ResizeWidth != 0 && g_ResizeHeight != 0)
        {
            CleanupRenderTarget();
            g_pSwapChain->ResizeBuffers(0, g_ResizeWidth, g_ResizeHeight, DXGI_FORMAT_UNKNOWN, 0);
            g_ResizeWidth = g_ResizeHeight = 0;
            CreateRenderTarget();
        }

        // Start ImGui frame
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        // Render application UI
        app.Render();

        // Finalize and present
        ImGui::Render();
        const float clearColor[4] = { 0.08f, 0.08f, 0.10f, 1.00f };
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clearColor);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        // Render additional platform windows (ImGui windows dragged outside the main OS window)
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
        }

        g_pSwapChain->Present(1, 0);
    }

    // Cleanup
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    CleanupDeviceD3D();
    ::DestroyWindow(hwnd);
    ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
    CoUninitialize();

    return 0;
}

static bool CreateDeviceD3D(HWND hWnd)
{
    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount                        = 2;
    sd.BufferDesc.Width                   = 0;
    sd.BufferDesc.Height                  = 0;
    sd.BufferDesc.Format                  = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator   = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags                              = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage                        = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow                       = hWnd;
    sd.SampleDesc.Count                   = 1;
    sd.SampleDesc.Quality                 = 0;
    sd.Windowed                           = TRUE;
    sd.SwapEffect                         = DXGI_SWAP_EFFECT_DISCARD;

    const D3D_FEATURE_LEVEL featureLevels[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };
    D3D_FEATURE_LEVEL featureLevel;

    HRESULT res = D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
        featureLevels, 2, D3D11_SDK_VERSION,
        &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext
    );
    // Fallback to WARP if hardware is unavailable
    if (res == DXGI_ERROR_UNSUPPORTED)
    {
        res = D3D11CreateDeviceAndSwapChain(
            nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0,
            featureLevels, 2, D3D11_SDK_VERSION,
            &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext
        );
    }
    if (res != S_OK)
        return false;

    CreateRenderTarget();
    return true;
}

static void CleanupDeviceD3D()
{
    CleanupRenderTarget();
    if (g_pSwapChain)        { g_pSwapChain->Release();        g_pSwapChain = nullptr; }
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = nullptr; }
    if (g_pd3dDevice)        { g_pd3dDevice->Release();        g_pd3dDevice = nullptr; }
}

static void CreateRenderTarget()
{
    ID3D11Texture2D* pBackBuffer = nullptr;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    if (pBackBuffer)
    {
        g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTargetView);
        pBackBuffer->Release();
    }
}

static void CleanupRenderTarget()
{
    if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = nullptr; }
}

static LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg)
    {
    case WM_SIZE:
        if (wParam != SIZE_MINIMIZED)
        {
            g_ResizeWidth  = LOWORD(lParam);
            g_ResizeHeight = HIWORD(lParam);
        }
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU)
            return 0;
        break;
    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    }
    return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}
