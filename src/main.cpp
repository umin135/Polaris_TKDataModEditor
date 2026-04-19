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
#include "AppStrings.h"
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

// -------------------------------------------------------------
//  Win+Arrow snap support via WH_KEYBOARD_LL
//
//  DWM handles Win+Arrow for WS_OVERLAPPEDWINDOW (main window) natively.
//  Secondary ImGui viewports (WS_POPUP) are skipped by DWM snap, so
//  we install a low-level keyboard hook that fires before DWM and handles
//  Win+Arrow for any window belonging to this process.
//
//  For the main window we pass through so DWM still owns it.
// -------------------------------------------------------------

static HWND   g_mainHwnd    = nullptr;
static HHOOK  g_kbHook      = nullptr;
static HANDLE g_hookThread  = nullptr;
static DWORD  g_hookThreadId = 0;
static DWORD  g_mainPid      = 0; // cached at startup, avoids per-call GetCurrentProcessId()

static void SnapWindowToHalf(HWND hwnd, bool left)
{
    HMONITOR hmon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi = { sizeof(mi) };
    if (!GetMonitorInfo(hmon, &mi)) return;
    int x = mi.rcWork.left + (left ? 0 : (mi.rcWork.right - mi.rcWork.left) / 2);
    int y = mi.rcWork.top;
    int w = (mi.rcWork.right - mi.rcWork.left) / 2;
    int h = mi.rcWork.bottom - mi.rcWork.top;
    if (IsZoomed(hwnd)) ShowWindow(hwnd, SW_RESTORE);
    SetWindowPos(hwnd, nullptr, x, y, w, h, SWP_NOZORDER | SWP_SHOWWINDOW);
}

static LRESULT CALLBACK LowLevelKbProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode != HC_ACTION)
        return CallNextHookEx(g_kbHook, nCode, wParam, lParam);

    // Fast path: if the foreground window does not belong to this process,
    // pass through immediately with zero extra work (game gets no added latency).
    HWND fg = GetForegroundWindow();
    if (!fg)
        return CallNextHookEx(g_kbHook, nCode, wParam, lParam);

    DWORD pid = 0;
    GetWindowThreadProcessId(fg, &pid);
    if (pid != g_mainPid)
        return CallNextHookEx(g_kbHook, nCode, wParam, lParam);

    // Our window is in the foreground — handle Win+Arrow snap.
    if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN)
    {
        auto* kb = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);
        if (kb->vkCode == VK_LEFT  || kb->vkCode == VK_RIGHT ||
            kb->vkCode == VK_UP    || kb->vkCode == VK_DOWN)
        {
            bool winHeld = (GetAsyncKeyState(VK_LWIN) & 0x8000) ||
                           (GetAsyncKeyState(VK_RWIN) & 0x8000);
            if (winHeld)
            {
                if (fg == g_mainHwnd)
                    return 1; // suppress Win+Arrow on main window (DWM would mis-snap it)

                switch (kb->vkCode)
                {
                case VK_UP:    ShowWindow(fg, SW_MAXIMIZE);                                   return 1;
                case VK_DOWN:  ShowWindow(fg, IsZoomed(fg) ? SW_RESTORE : SW_MINIMIZE);       return 1;
                case VK_LEFT:  SnapWindowToHalf(fg, true);                                    return 1;
                case VK_RIGHT: SnapWindowToHalf(fg, false);                                   return 1;
                }
            }
        }
    }
    return CallNextHookEx(g_kbHook, nCode, wParam, lParam);
}

// Dedicated hook thread: installs the LL keyboard hook and runs its own message pump.
// Running separately from the render thread means VSync blocking in Present() cannot
// delay the hook proc and cause perceived input lag in other applications (e.g. the game).
static DWORD WINAPI HookThreadProc(LPVOID)
{
    g_kbHook = SetWindowsHookExW(WH_KEYBOARD_LL, LowLevelKbProc,
                                  GetModuleHandleW(nullptr), 0);
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    if (g_kbHook) { UnhookWindowsHookEx(g_kbHook); g_kbHook = nullptr; }
    return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int)
{
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    RegisterFileAssociation();
    const std::string startupFile = ParseStartupFilePath();

    // Register window class
    HICON hIconBig   = LoadIconW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(IDI_APPICON));
    HICON hIconSmall = (HICON)LoadImageW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(IDI_APPICON),
                                         IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR);
    WNDCLASSEXW wc = {
        sizeof(wc), CS_CLASSDC, WndProc, 0L, 0L,
        GetModuleHandle(nullptr), hIconBig, nullptr, nullptr, nullptr,
        L"PolarisTKDataEditor", hIconSmall
    };
    ::RegisterClassExW(&wc);

    // Create the main window (wide rectangle: 1280x720)
    HWND hwnd = ::CreateWindowW(
        wc.lpszClassName, L"TEKKEN8 TKData Mod Editor - " APPSTR_VERSION_W,
        WS_OVERLAPPEDWINDOW,
        100, 100, 1280, 720,
        nullptr, nullptr, wc.hInstance, nullptr
    );

    // Explicitly set taskbar / title-bar icons (covers secondary viewports too)
    ::SendMessageW(hwnd, WM_SETICON, ICON_BIG,   (LPARAM)hIconBig);
    ::SendMessageW(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hIconSmall);

    if (!CreateDeviceD3D(hwnd))
    {
        CleanupDeviceD3D();
        ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return 1;
    }

    // Install LL keyboard hook on a dedicated thread so VSync waits in the render
    // loop cannot delay hook delivery and cause input lag in other processes.
    g_mainHwnd = hwnd;
    g_mainPid  = GetCurrentProcessId();
    g_hookThread = CreateThread(nullptr, 0, HookThreadProc, nullptr, 0, &g_hookThreadId);

    ::ShowWindow(hwnd, SW_SHOWNORMAL);
    ::UpdateWindow(hwnd);

    // Setup Dear ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
    io.IniFilename = nullptr; // disable imgui.ini -- window state is not persisted
    io.ConfigWindowsMoveFromTitleBarOnly = true;

    ImGui::StyleColorsDark();

    // Load custom fonts from embedded Win32 resources (RCDATA)
    // g_fontBold is used for highlighted items (e.g. generic-ID moves in the move list)
    {
        ImGuiIO& ioRef = ImGui::GetIO();
        constexpr float kFontSize = 15.0f;

        // Custom glyph ranges: Basic Latin + Latin Supplement + Arrows block (→ = U+2192)
        static const ImWchar kGlyphRanges[] = {
            0x0020, 0x00FF, // Basic Latin + Latin Supplement
            0x2190, 0x21FF, // Arrows
            0,
        };

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
            return ioRef.Fonts->AddFontFromMemoryTTF(data, (int)size, kFontSize, &cfg, kGlyphRanges);
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

    // Create application instance -- pass DX11 device + context for rendering
    App app(g_pd3dDevice, g_pd3dDeviceContext);

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

    // Cleanup: signal hook thread to exit, then wait for it
    if (g_hookThread)
    {
        PostThreadMessageW(g_hookThreadId, WM_QUIT, 0, 0);
        WaitForSingleObject(g_hookThread, 2000);
        CloseHandle(g_hookThread);
        g_hookThread = nullptr;
    }
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
