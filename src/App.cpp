// Main application rendering logic
#include "App.h"
#include "Config.h"
#include "LayoutStore.h"
#include "GameStatic.h"
#include "KamuiDictUpdater.h"
#include "MovesetDataDictUpdater.h"
#include "FbsDataDict.h"
#include "FbsDataDictUpdater.h"
#include "fbsdata/data/ExternalItemIdIndex.h"
#include "moveset/labels/LabelDB.h"
#include "moveset/data/MovesetDataDict.h"
#include "moveset/data/T7AliasDict.h"
#include "moveset/editor/ListKeybinds.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"  // DockBuilder API
#include <functional>
#include <chrono>

#include <d3d11.h>
#include <wincodec.h>
#include <windows.h>
#include <shobjidl.h>   // IFileDialog for folder picker
#include <vector>
#pragma comment(lib, "windowscodecs.lib")

#include "resource.h"
#include "AppStrings.h"
#include "AppUpdater.h"

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

// Game root -> tkdata.bin path
static std::string ResolveTkdataBinPath(const char* gameRoot)
{
    if (!gameRoot || gameRoot[0] == '\0') return {};
    return std::string(gameRoot) + "\\Polaris\\Content\\Binary\\pak\\tkdata.bin";
}

static bool TkdataBinExists(const char* gameRoot)
{
    std::string path = ResolveTkdataBinPath(gameRoot);
    if (path.empty()) return false;
    return GetFileAttributesA(path.c_str()) != INVALID_FILE_ATTRIBUTES;
}

// Read Steam installation directory from registry.
static std::string GetSteamDir()
{
    struct { HKEY root; const char* key; REGSAM extra; } kTry[] = {
        { HKEY_CURRENT_USER,  "SOFTWARE\\Valve\\Steam",              0             },
        { HKEY_LOCAL_MACHINE, "SOFTWARE\\WOW6432Node\\Valve\\Steam", 0             },
        { HKEY_LOCAL_MACHINE, "SOFTWARE\\Valve\\Steam",              KEY_WOW64_32KEY },
    };
    for (const auto& t : kTry)
    {
        HKEY h = nullptr;
        if (RegOpenKeyExA(t.root, t.key, 0, KEY_READ | t.extra, &h) != ERROR_SUCCESS)
            continue;
        char buf[MAX_PATH] = {};
        DWORD sz = sizeof(buf), type = REG_SZ;
        LONG r = RegQueryValueExA(h, "SteamPath", nullptr, &type, (LPBYTE)buf, &sz);
        RegCloseKey(h);
        if (r == ERROR_SUCCESS && buf[0])
        {
            for (char* p = buf; *p; ++p) if (*p == '/') *p = '\\';
            return buf;
        }
    }
    return {};
}

// Detect TEKKEN 8 game root via Steam registry + libraryfolders.vdf scan.
static std::string DetectSteamGameRoot()
{
    // Method 1: direct Uninstall key (fast path, not always present)
    struct { HKEY root; const char* key; REGSAM extra; } kUninstall[] = {
        { HKEY_LOCAL_MACHINE,
          "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Steam App 1778820", 0 },
        { HKEY_LOCAL_MACHINE,
          "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Steam App 1778820",
          KEY_WOW64_32KEY },
        { HKEY_LOCAL_MACHINE,
          "SOFTWARE\\WOW6432Node\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Steam App 1778820",
          0 },
    };
    for (const auto& e : kUninstall)
    {
        HKEY h = nullptr;
        if (RegOpenKeyExA(e.root, e.key, 0, KEY_READ | e.extra, &h) != ERROR_SUCCESS)
            continue;
        char buf[MAX_PATH] = {};
        DWORD sz = sizeof(buf), type = REG_SZ;
        LONG r = RegQueryValueExA(h, "InstallLocation", nullptr, &type, (LPBYTE)buf, &sz);
        RegCloseKey(h);
        if (r == ERROR_SUCCESS && buf[0]) return buf;
    }

    // Method 2: Steam directory + libraryfolders.vdf scan
    std::string steamDir = GetSteamDir();
    if (steamDir.empty()) return {};

    static const char kGameSub[] = "\\steamapps\\common\\TEKKEN 8";

    // Check default library (same location as Steam itself)
    {
        std::string candidate = steamDir + kGameSub;
        if (TkdataBinExists(candidate.c_str())) return candidate;
    }

    // Parse additional libraries from libraryfolders.vdf
    std::string vdfPath = steamDir + "\\steamapps\\libraryfolders.vdf";
    FILE* f = nullptr;
    fopen_s(&f, vdfPath.c_str(), "r");
    if (!f) return {};

    char line[1024];
    while (fgets(line, sizeof(line), f))
    {
        // Extract any quoted value on the line that looks like an absolute path (X:\...)
        const char* p = line;
        while (*p)
        {
            const char* q1 = strchr(p, '"');
            if (!q1) break;
            const char* q2 = strchr(q1 + 1, '"');
            if (!q2) break;
            p = q2 + 1;

            // Absolute path: first char = letter, second = ':'
            if ((size_t)(q2 - q1 - 1) < 3 || q1[2] != ':') continue;

            // Unescape \\ -> \ and normalize /
            std::string libPath;
            libPath.reserve((size_t)(q2 - q1));
            for (const char* c = q1 + 1; c < q2; ++c)
            {
                char ch = (*c == '/') ? '\\' : *c;
                if (ch == '\\' && *(c + 1) == '\\') { libPath += '\\'; ++c; }
                else libPath += ch;
            }

            std::string candidate = libPath + kGameSub;
            if (TkdataBinExists(candidate.c_str()))
            {
                fclose(f);
                return candidate;
            }
        }
    }
    fclose(f);
    return {};
}

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

App::App(ID3D11Device* device, ID3D11DeviceContext* ctx, void* hwnd)
    : m_d3dDev(device), m_d3dCtx(ctx), m_hwnd(hwnd)
{
    char exeBuf[MAX_PATH] = {};
    GetModuleFileNameA(nullptr, exeBuf, MAX_PATH);
    m_exePath = exeBuf;

    // Read local version.json synchronously so version_str is ready for frame 1
    {
        std::string exeDir = m_exePath;
        size_t sep = exeDir.rfind('\\');
        if (sep != std::string::npos) exeDir = exeDir.substr(0, sep);
        AppVersionLoadLocal(exeDir);
    }

    // Load logo texture now (D3D main-thread op) so splash can display it
    {
        int w = 0, h = 0;
        m_logoTex  = LoadTexturePNGFromResource(device, IDR_HOME_LOGO_PNG, &w, &h);
        m_logoSize = ImVec2((float)w, (float)h);
    }

    ApplyStyle();

    // Reshape the OS window to a borderless 650×230 splash, centered on screen
    {
        const int splashW = 650, splashH = 230;
        const int screenW = GetSystemMetrics(SM_CXSCREEN);
        const int screenH = GetSystemMetrics(SM_CYSCREEN);
        SetWindowLongW((HWND)m_hwnd, GWL_STYLE, WS_POPUP | WS_VISIBLE);
        SetWindowPos((HWND)m_hwnd, HWND_TOP,
            (screenW - splashW) / 2, (screenH - splashH) / 2,
            splashW, splashH,
            SWP_FRAMECHANGED | SWP_NOZORDER | SWP_SHOWWINDOW);
    }

    m_initThread = std::thread([this]() { RunInitThread(); });
}

void App::RunInitThread()
{
    m_initStatus.store("Loading config...");
    Config::Get().Load();
    LayoutStore::Get().Load();
    GameStatic::Get().Load();

    // Apply persisted list-edit keybindings (falling back to defaults for any unset chord).
    {
        ListKeybinds& kb = ListKeybinds::Get();
        const std::string* lk = Config::Get().data.listKeys;
        for (int i = 0; i < (int)ListShortcut::COUNT; ++i)
            if (!lk[i].empty()) kb.Decode((ListShortcut)i, lk[i]);
    }

    // Auto-detect game root if not set or if tkdata.bin is no longer found there
    AppConfig& cfg = Config::Get().data;
    if (!TkdataBinExists(cfg.gameRootDir.c_str()))
    {
        std::string detected = DetectSteamGameRoot();
        if (!detected.empty() && TkdataBinExists(detected.c_str()))
        {
            cfg.gameRootDir = detected;
            Config::Get().Save();
        }
    }
    m_extractorView.SetDestFolder(cfg.MovesetDir());
    m_extractorView.SetOnExtractSuccess([this]() {
        m_movesetView.ForceRefresh();
    });

    std::string exeDir = m_exePath;
    size_t sep = exeDir.rfind('\\');
    if (sep != std::string::npos) exeDir = exeDir.substr(0, sep);

    const std::string candidates[] = {
        exeDir + "\\data\\interfacedata",
        exeDir + "\\..\\data\\interfacedata",
        exeDir + "\\..\\..\\data\\interfacedata",
        exeDir + "\\..\\..\\..\\data\\interfacedata",
    };

    m_initStatus.store("Loading FBS dictionary...");
    {
        std::string fbsDataPath;
        for (const auto& c : candidates)
        {
            std::string p = c + "\\..\\fbsdatas\\data.json";
            FILE* f = nullptr; fopen_s(&f, p.c_str(), "rb");
            if (f) { fclose(f); fbsDataPath = p; break; }
        }
        if (!fbsDataPath.empty())
            FbsDataDict::Get().Load(fbsDataPath);
        else
            FbsDataDict::Get().LoadFromResources();
    }

    // Index item ids used by other tkmods in the manager directory (startup-only),
    // so the fbsdata editors can warn about cross-tkmod id collisions.
    m_initStatus.store("Indexing tkmod item ids...");
    ExternalItemIdIndex::Get().BuildFromDir(Config::Get().data.tkmodManagerDir);

    m_initStatus.store("Loading label database...");
    {
        bool loadedFromDisk = false;
        for (const auto& c : candidates)
        {
            LabelDB::Get().Load(c);
            if (LabelDB::Get().IsLoaded())
            {
                loadedFromDisk = true;
                break;
            }
        }
        if (!loadedFromDisk)
            LabelDB::Get().LoadFromResources();
    }

    m_initStatus.store("Checking for dictionary updates...");
    {
        const std::string resCandidates[] = {
            exeDir + "\\res",
            exeDir + "\\..\\res",
            exeDir + "\\..\\..\\res",
            exeDir + "\\..\\..\\..\\res",
        };
        std::string resDir;
        for (const auto& r : resCandidates)
        {
            DWORD attr = GetFileAttributesA(r.c_str());
            if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY))
            { resDir = r; break; }
        }

        if (!resDir.empty())
        {
            KamuiDictCheckAndUpdate(resDir);
            {
                std::string p = resDir + "\\kamui-hashes\\data.json";
                FILE* f = nullptr; fopen_s(&f, p.c_str(), "rb");
                if (f) { fclose(f); LabelDB::Get().LoadNames(p); }
            }

            MovesetDataDictCheckAndUpdate(resDir);
            {
                std::string p = resDir + "\\MovesetDatas\\data.json";
                FILE* f = nullptr; fopen_s(&f, p.c_str(), "rb");
                if (f) { fclose(f); MovesetDataDict::Get().Load(p); }
            }

            {
                std::string p = resDir + "\\MovesetDatas\\t7_aliases.json";
                FILE* f = nullptr; fopen_s(&f, p.c_str(), "rb");
                if (f) { fclose(f); T7AliasDict::Get().Load(p); }
            }
            if (!T7AliasDict::Get().IsLoaded())
                T7AliasDict::Get().EnsureLoaded();

            if (FbsDataDictCheckAndUpdate(resDir))
            {
                std::string p = resDir + "\\fbsdatas\\data.json";
                FILE* f = nullptr; fopen_s(&f, p.c_str(), "rb");
                if (f) { fclose(f); FbsDataDict::Get().Load(p); }
            }
        }
        if (!T7AliasDict::Get().IsLoaded())
            T7AliasDict::Get().EnsureLoaded();
    }

    m_initStatus.store("Checking for updates...");
    AppUpdateCheck(m_exePath);

    m_initDone.store(true, std::memory_order_release);
}

App::~App()
{
    if (m_initThread.joinable())
        m_initThread.join();
    LayoutStore::Get().Save();   // persist layout on app exit (editors may still be open)
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

    // Disable ImGui's built-in modal dim (it affects all viewports with ViewportsEnable).
    // MovesetEditorWindow draws its own dim on the editor viewport only.
    colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0, 0, 0, 0);
}

// -------------------------------------------------------------
//  Frame entry point
// -------------------------------------------------------------

static void RenderChangelogText(const char* text)
{
    const char* p = text;
    while (*p) {
        const char* nl = strchr(p, '\n');
        if (nl) { ImGui::TextUnformatted(p, nl); p = nl + 1; }
        else     { ImGui::TextUnformatted(p); break; }
    }
}

void App::RenderSplash()
{
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->WorkPos);
    ImGui::SetNextWindowSize(vp->WorkSize);
    ImGui::SetNextWindowViewport(vp->ID);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,   0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,    ImVec2(0.0f, 0.0f));
    ImGui::Begin("##Splash", nullptr,
        ImGuiWindowFlags_NoTitleBar           |
        ImGuiWindowFlags_NoCollapse           |
        ImGuiWindowFlags_NoResize             |
        ImGuiWindowFlags_NoMove               |
        ImGuiWindowFlags_NoScrollbar          |
        ImGuiWindowFlags_NoBringToFrontOnFocus|
        ImGuiWindowFlags_NoNavFocus           |
        ImGuiWindowFlags_NoDocking            |
        ImGuiWindowFlags_NoSavedSettings);
    ImGui::PopStyleVar(3);

    const float W     = vp->WorkSize.x;   // 650
    const float H     = vp->WorkSize.y;   // 230
    const float logH  = 46.0f;
    const float imgH  = H - logH;         // 184

    // ── Image area ──────────────────────────────────────────────
    if (m_logoTex && m_logoSize.x > 0.0f && m_logoSize.y > 0.0f)
    {
        // Scale to fit while preserving aspect ratio
        float scaleX = W      / m_logoSize.x;
        float scaleY = imgH   / m_logoSize.y;
        float scale  = (scaleX < scaleY) ? scaleX : scaleY;
        ImVec2 drawSize = { m_logoSize.x * scale, m_logoSize.y * scale };

        ImGui::SetCursorPos({
            (W    - drawSize.x) * 0.5f,
            (imgH - drawSize.y) * 0.5f
        });
        ImGui::Image((ImTextureID)(intptr_t)m_logoTex, drawSize);
    }
    else
    {
        // Text fallback when texture is unavailable
        ImGui::SetWindowFontScale(2.0f);
        const char* t = AppStr::SidebarLogoText;
        ImGui::SetCursorPos({
            (W - ImGui::CalcTextSize(t).x) * 0.5f,
            imgH * 0.30f
        });
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.65f, 0.82f, 1.00f, 1.00f));
        ImGui::TextUnformatted(t);
        ImGui::PopStyleColor();
        ImGui::SetWindowFontScale(1.0f);
    }

    // ── Log bar (dark strip at the bottom) ──────────────────────
    ImVec2 winPos = ImGui::GetWindowPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(
        { winPos.x,     winPos.y + imgH },
        { winPos.x + W, winPos.y + H    },
        IM_COL32(14, 14, 18, 255));

    const float textY = imgH + (logH - ImGui::GetTextLineHeight()) * 0.5f;

    AppUpdateStatus updStatus = AppUpdateGetStatus();
    if (updStatus == AppUpdateStatus::Downloading)
    {
        // Green progress fill behind the log bar
        float prog = AppUpdateGetDownloadProgress();
        dl->AddRectFilled(
            { winPos.x,              winPos.y + imgH },
            { winPos.x + W * prog,   winPos.y + H    },
            IM_COL32(20, 90, 40, 200));

        char progBuf[64];
        snprintf(progBuf, sizeof(progBuf), "Downloading update... %.0f%%", prog * 100.0f);
        ImGui::SetCursorPos({ 14.0f, textY });
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.60f, 0.90f, 0.65f, 1.00f));
        ImGui::TextUnformatted(progBuf);
        ImGui::PopStyleColor();
    }
    else if (updStatus == AppUpdateStatus::DownloadFailed)
    {
        ImGui::SetCursorPos({ 14.0f, textY });
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.90f, 0.40f, 0.40f, 1.00f));
        ImGui::TextUnformatted("Download failed. Continuing without update.");
        ImGui::PopStyleColor();
    }
    else
    {
        ImGui::SetCursorPos({ 14.0f, textY });
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.55f, 0.68f, 1.00f));
        ImGui::TextUnformatted(m_initStatus.load());
        ImGui::PopStyleColor();
    }

    // Version — right side
    const float verW = ImGui::CalcTextSize(AppVersionGetStr()).x;
    ImGui::SetCursorPos({ W - verW - 14.0f, textY });
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.32f, 0.32f, 0.42f, 1.00f));
    ImGui::TextUnformatted(AppVersionGetStr());
    ImGui::PopStyleColor();

    ImGui::End();

    // ── Update confirmation dialog (drawn on top when update is available) ──
    if (updStatus == AppUpdateStatus::Available)
    {
        const AppUpdateInfo& info = AppUpdateGetInfo();
        const float dlgH = info.changelog.empty() ? 148.0f : 320.0f;

        const ImGuiViewport* dvp = ImGui::GetMainViewport();
        const ImVec2 dlgSize = { 360.0f, dlgH };
        ImGui::SetNextWindowPos(
            { dvp->WorkPos.x + (dvp->WorkSize.x - dlgSize.x) * 0.5f,
              dvp->WorkPos.y + (dvp->WorkSize.y - dlgSize.y) * 0.5f },
            ImGuiCond_Always);
        ImGui::SetNextWindowSize(dlgSize, ImGuiCond_Always);

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,   6.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,    ImVec2(16.0f, 14.0f));
        ImGui::Begin("##UpdateDlg", nullptr,
            ImGuiWindowFlags_NoTitleBar    | ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoResize      | ImGuiWindowFlags_NoMove     |
            ImGuiWindowFlags_NoScrollbar   | ImGuiWindowFlags_NoDocking  |
            ImGuiWindowFlags_NoSavedSettings);
        ImGui::PopStyleVar(3);

        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.65f, 0.82f, 1.00f, 1.00f));
        ImGui::TextUnformatted("Update Available");
        ImGui::PopStyleColor();
        ImGui::Separator();
        ImGui::Spacing();

        char msgBuf[128];
        if (!info.versionStr.empty())
            snprintf(msgBuf, sizeof(msgBuf), "Version %s is available.", info.versionStr.c_str());
        else
            snprintf(msgBuf, sizeof(msgBuf), "A new version is available.");
        ImGui::TextUnformatted(msgBuf);
        ImGui::TextUnformatted("Would you like to update and restart?");

        if (!info.changelog.empty())
        {
            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.55f, 0.68f, 1.00f));
            ImGui::TextUnformatted("What's new:");
            ImGui::PopStyleColor();
            ImGui::Spacing();

            const float btnRowH = ImGui::GetStyle().ItemSpacing.y * 2.0f
                                + 1.0f + ImGui::GetFrameHeight()
                                + ImGui::GetStyle().WindowPadding.y;
            const float childH  = ImGui::GetContentRegionAvail().y - btnRowH;

            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.08f, 0.08f, 0.11f, 1.00f));
            if (ImGui::BeginChild("##ChangelogScroll", ImVec2(0.f, childH), ImGuiChildFlags_Borders))
            {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.78f, 0.78f, 0.85f, 1.00f));
                RenderChangelogText(info.changelog.c_str());
                ImGui::PopStyleColor();
            }
            ImGui::EndChild();
            ImGui::PopStyleColor();
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        const float btnW = (dlgSize.x - 16.0f * 2.0f - ImGui::GetStyle().ItemSpacing.x) * 0.5f;

        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.12f, 0.55f, 0.22f, 1.00f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.18f, 0.72f, 0.32f, 1.00f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.15f, 0.64f, 0.28f, 1.00f));
        if (ImGui::Button("Update", ImVec2(btnW, 0)))
            AppUpdateBeginDownload();
        ImGui::PopStyleColor(3);

        ImGui::SameLine();

        if (ImGui::Button("No", ImVec2(btnW, 0)))
            AppUpdateDecline();

        ImGui::End();
    }
}

void App::Render()
{
    // Splash phase: show loading screen until background init completes
    if (m_appState == AppState::Splash)
    {
        AppUpdateStatus updStatus = AppUpdateGetStatus();

        // Download finished — apply and quit immediately
        if (updStatus == AppUpdateStatus::Ready)
        {
            if (m_initThread.joinable()) m_initThread.join();
            AppUpdateApply();
            PostQuitMessage(0);
            return;
        }

        if (m_initDone.load(std::memory_order_acquire))
        {
            // Keep showing splash while user is deciding or download is in progress
            if (updStatus == AppUpdateStatus::Available ||
                updStatus == AppUpdateStatus::Downloading)
            {
                RenderSplash();
                return;
            }

            // No update / up to date / declined / failed — start 2-second hold
            if (m_splashEndTime < 0.0)
            {
                m_initStatus.store("Ready.");
                // Transition immediately when user explicitly declined
                double hold = (updStatus == AppUpdateStatus::Declined) ? 0.0 : 2.0;
                m_splashEndTime = ImGui::GetTime() + hold;
            }

            if (ImGui::GetTime() < m_splashEndTime)
            {
                RenderSplash();
                return;
            }

            if (m_initThread.joinable())
                m_initThread.join();

            // Restore to full-size app window (WS_OVERLAPPEDWINDOW, 1280×720, centered)
            {
                const int appW = 1280, appH = 720;
                const int screenW = GetSystemMetrics(SM_CXSCREEN);
                const int screenH = GetSystemMetrics(SM_CYSCREEN);
                SetWindowLongW((HWND)m_hwnd, GWL_STYLE, WS_OVERLAPPEDWINDOW);
                SetWindowPos((HWND)m_hwnd, nullptr,
                    (screenW - appW) / 2, (screenH - appH) / 2,
                    appW, appH,
                    SWP_FRAMECHANGED | SWP_NOZORDER | SWP_SHOWWINDOW);
            }

            m_appState = AppState::Running;
            // fall through to render the first Running frame immediately
        }
        else
        {
            RenderSplash();
            return;
        }
    }

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

    // Build the default layout once per run (imgui.ini is disabled, so always fresh)
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
                AppStr::SidebarWidth / vp->WorkSize.x,
                &sidebarNodeId, &contentNodeId);

            ImGui::DockBuilderDockWindow("##Sidebar", sidebarNodeId);
            ImGui::DockBuilderDockWindow("##Content", contentNodeId);
            ImGui::DockBuilderFinish(dockspaceId);
        }
    }

    ImGui::End();

    // -- Process completed async operations (before any rendering) --
    m_extractorView.CheckThread();

    if (m_pendingEditor.valid() &&
        m_pendingEditor.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
    {
        m_editorWindows.push_back(std::make_unique<MovesetEditorWindow>(m_pendingEditor.get()));
        m_editorWindows.back()->SetD3DContext(m_d3dDev, m_d3dCtx);
        m_loadingActive = false;
    }

    RenderMainLayout();
    RenderSettingsWindow();

    // Render all open moveset editor windows; remove closed ones
    auto it = m_editorWindows.begin();
    while (it != m_editorWindows.end())
    {
        if (!(*it)->Render())
        {
            LayoutStore::Get().Save();   // persist window sizes / section widths on close
            it = m_editorWindows.erase(it);
        }
        else
            ++it;
    }

    // -- Loading overlay (rendered last so it appears on top) --
    const bool anyLoading = m_loadingActive || m_extractorView.IsLoading();
    if (anyLoading)
    {
        const char* msg = m_extractorView.IsLoading()
            ? "Extracting moveset..."
            : m_loadingMessage.c_str();
        RenderLoadingOverlay(msg);
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
    const char* logoText = AppStr::SidebarLogoText;
    ImGui::SetCursorPosX((sidebarWidth - ImGui::CalcTextSize(logoText).x) * 0.5f);
    ImGui::Text("%s", logoText);
    ImGui::PopStyleColor();

    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.38f, 0.38f, 0.50f, 1.00f));
    const char* subText = AppStr::SidebarSubText;
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
    if (SidebarBtn(AppStr::BtnHome,    ContentView::Home,    true))
        m_currentView = ContentView::Home;

    ImGui::Separator();
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 6.0f);

    if (SidebarBtn(AppStr::BtnFbsData, ContentView::FbsData, true))
        m_currentView = ContentView::FbsData;

    if (SidebarBtn(AppStr::BtnMoveset, ContentView::Moveset, true))
        m_currentView = ContentView::Moveset;

#ifdef _DEBUG
    ImGui::Separator();
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 6.0f);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.75f, 0.2f, 1.0f));
    ImGui::SetCursorPosX(paddingX);
    ImGui::TextUnformatted(AppStr::DevModeLabel);
    ImGui::PopStyleColor();
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 2.0f);
    if (SidebarBtn(AppStr::BtnFbsDev,     ContentView::FbsDevMode, true))
        m_currentView = ContentView::FbsDevMode;
    if (SidebarBtn(AppStr::BtnMotbinDiff, ContentView::MotbinDiff, true))
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
        if (ImGui::Button(AppStr::BtnSettings, ImVec2(buttonW, buttonH)))
        {
            m_showSettings       = !m_showSettings;
            m_settingsInitialized = false;  // re-init edit buffers on next open
        }
        if (m_showSettings)
            ImGui::PopStyleColor(3);
    }

    // -- Version label --
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.28f, 0.28f, 0.38f, 1.00f));
    const float verW = ImGui::CalcTextSize(AppVersionGetStr()).x;
    ImGui::SetCursorPosX((sidebarWidth - verW) * 0.5f);
    ImGui::Text("%s", AppVersionGetStr());
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
        ImGui::SetWindowFontScale(2.0f);
        const float titleW = ImGui::CalcTextSize(AppStr::AppTitle).x;
        ImGui::SetCursorPosX((contentW - titleW) * 0.5f);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.65f, 0.82f, 1.00f, 1.00f));
        ImGui::Text("%s", AppStr::AppTitle);
        ImGui::PopStyleColor();
        ImGui::SetWindowFontScale(1.0f);
    }

    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 6.0f);

    char titleWithVer[128];
    snprintf(titleWithVer, sizeof(titleWithVer), "%s - %s", AppStr::AppTitle, AppVersionGetStr());
    const float subW = ImGui::CalcTextSize(titleWithVer).x;
    ImGui::SetCursorPosX((contentW - subW) * 0.5f);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.60f, 0.60f, 0.72f, 1.00f));
    ImGui::Text("%s", titleWithVer);
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
    CenteredText(AppStr::SectionModules, ImVec4(0.75f, 0.75f, 0.85f, 1.00f));
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

        for (const auto& m : AppStr::Modules)
            ModuleRow(m.name, m.desc, m.ready);

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

    CenteredText(AppStr::SectionCredits, ImVec4(0.75f, 0.75f, 0.85f, 1.00f));
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 8.0f);
    for (const auto& c : AppStr::Credits)
        CreditRow(c.name, c.role);

    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 20.0f);
    CenteredText(AppStr::SectionLinks, ImVec4(0.75f, 0.75f, 0.85f, 1.00f));
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 8.0f);
    for (const auto& l : AppStr::Links)
        LinkText(l.label, l.url);
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
    m_fbsDevView.Render(&m_fbsDataView.GetModData());
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

    // Open a new editor window asynchronously to avoid blocking the render loop.
    if (wantOpen && !m_loadingActive && !m_pendingEditor.valid())
    {
        m_loadingActive  = true;
        m_loadingMessage = "Loading moveset...";
        const int uid = m_nextEditorUid++;
        m_pendingEditor = std::async(std::launch::async,
            [openPath, openName, uid]()
            {
                return MovesetEditorWindow(openPath, openName, uid);
            });
    }

    // -- Extraction log (bottom) ------------------------------
    ImGui::Spacing();
    m_extractorView.RenderLog();
}

// -------------------------------------------------------------
//  Settings window (floating -- not docked)
// -------------------------------------------------------------

void App::ApplyAndSaveSettings()
{
    AppConfig& cfg   = Config::Get().data;
    cfg.gameRootDir  = m_settingsGameRoot;
    cfg.cinematicExportRoot = m_settingsCineExport;

    // Persist the current list-edit keybindings as encoded chord strings.
    {
        const ListKeybinds& kb = ListKeybinds::Get();
        for (int i = 0; i < (int)ListShortcut::COUNT; ++i)
            cfg.listKeys[i] = kb.Encode((ListShortcut)i);
    }

    Config::Get().Save();
    m_movesetView.ForceRefresh();
    m_extractorView.SetDestFolder(cfg.MovesetDir());
}

// -------------------------------------------------------------
//  RenderLoadingOverlay
//  Dim background + centered spinner popup.
//  Rendered after all other UI so it appears on top.
// -------------------------------------------------------------

void App::RenderLoadingOverlay(const char* msg)
{
    ImGuiViewport* vp     = ImGui::GetMainViewport();
    const ImVec2   vpPos  = vp->Pos;
    const ImVec2   vpSize = vp->Size;
    const ImVec2   center(vpPos.x + vpSize.x * 0.5f, vpPos.y + vpSize.y * 0.5f);

    constexpr ImGuiWindowFlags kOverlayFlags =
        ImGuiWindowFlags_NoTitleBar      | ImGuiWindowFlags_NoResize      |
        ImGuiWindowFlags_NoMove          | ImGuiWindowFlags_NoDocking     |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar   |
        ImGuiWindowFlags_NoNav           | ImGuiWindowFlags_NoFocusOnAppearing;

    // Semi-transparent dim over the entire main viewport
    ImGui::SetNextWindowPos(vpPos);
    ImGui::SetNextWindowSize(vpSize);
    ImGui::SetNextWindowBgAlpha(0.28f);
    ImGui::Begin("##ldimBg", nullptr, kOverlayFlags | ImGuiWindowFlags_NoInputs);
    ImGui::End();

    // Centered info box
    static const char* kSpin = "|/-\\";
    const int spinIdx = static_cast<int>(ImGui::GetTime() * 8.0) & 3;

    // Measure content to size the box
    char spinMsg[256];
    snprintf(spinMsg, sizeof(spinMsg), "%c  %s", kSpin[spinIdx], msg);
    const float textW   = ImGui::CalcTextSize(spinMsg).x;
    const float pad     = ImGui::GetStyle().WindowPadding.x;
    const float boxW    = textW + pad * 2.0f + 16.0f;
    const float lineH   = ImGui::GetTextLineHeightWithSpacing();

    ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(boxW, lineH + pad * 2.0f + 8.0f), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.96f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
    ImGui::Begin("##ldimInfo", nullptr, kOverlayFlags);
    ImGui::PopStyleVar();

    ImGui::SetCursorPosY(ImGui::GetStyle().WindowPadding.y + 4.0f);
    ImGui::Text("%s", spinMsg);

    ImGui::End();
}

// -------------------------------------------------------------

void App::RenderSettingsWindow()
{
    if (!m_showSettings)
    {
        m_settingsInitialized = false;
        m_capturingKey = -1;
        return;
    }

    // Populate edit buffers the first time the window appears
    if (!m_settingsInitialized)
    {
        m_settingsInitialized = true;
        const AppConfig& cfg = Config::Get().data;
        strncpy_s(m_settingsGameRoot, sizeof(m_settingsGameRoot),
                  cfg.gameRootDir.c_str(), _TRUNCATE);
        strncpy_s(m_settingsCineExport, sizeof(m_settingsCineExport),
                  cfg.cinematicExportRoot.c_str(), _TRUNCATE);
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
            ImGui::TextUnformatted("Game Root Directory");
            ImGui::Spacing();
            ImGui::TextDisabled(
                "TEKKEN 8 game installation root folder.\n"
                "Subpath Polaris\\Content\\Binary\\Mods\\Movesets is resolved automatically.");
            ImGui::Spacing();

            ImGui::SetNextItemWidth(-76.0f);
            ImGui::InputText("##gameRoot", m_settingsGameRoot, sizeof(m_settingsGameRoot));
            ImGui::SameLine();
            if (ImGui::Button("Browse", ImVec2(68.0f, 0.0f)))
            {
                std::string folder = BrowseForFolder();
                if (!folder.empty())
                    strncpy_s(m_settingsGameRoot, sizeof(m_settingsGameRoot),
                              folder.c_str(), _TRUNCATE);
            }

            // tkdata.bin detection status
            ImGui::Spacing();
            if (m_settingsGameRoot[0] == '\0')
            {
                ImGui::TextDisabled("tkdata.bin: (no game root set)");
            }
            else if (TkdataBinExists(m_settingsGameRoot))
            {
                std::string tkPath = ResolveTkdataBinPath(m_settingsGameRoot);
                ImGui::TextColored(ImVec4(0.35f, 1.0f, 0.50f, 1.0f),
                                   "tkdata.bin: Found");
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
                    ImGui::SetTooltip("%s", tkPath.c_str());
            }
            else
            {
                std::string tkPath = ResolveTkdataBinPath(m_settingsGameRoot);
                ImGui::TextColored(ImVec4(1.0f, 0.40f, 0.40f, 1.0f),
                                   "tkdata.bin: Not found");
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
                    ImGui::SetTooltip("%s", tkPath.c_str());
            }

            // -- Cinematic export root ---------------------------
            ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
            ImGui::TextUnformatted("Cinematic Export Root (optional)");
            ImGui::Spacing();
            ImGui::TextDisabled(
                "Root of a ripped cinematics dump (...\\Exports\\Polaris\\Content\\cinematics).\n"
                "When set, extraction resolves each sequence's real season folder\n"
                "(polaris / polaris01 / ...) and existence against the dump.");
            ImGui::Spacing();

            ImGui::SetNextItemWidth(-76.0f);
            ImGui::InputText("##cineExport", m_settingsCineExport, sizeof(m_settingsCineExport));
            ImGui::SameLine();
            if (ImGui::Button("Browse##cine", ImVec2(68.0f, 0.0f)))
            {
                std::string folder = BrowseForFolder();
                if (!folder.empty())
                    strncpy_s(m_settingsCineExport, sizeof(m_settingsCineExport),
                              folder.c_str(), _TRUNCATE);
            }
            ImGui::Spacing();
            if (m_settingsCineExport[0] == '\0')
            {
                ImGui::TextDisabled("cinematics dump: (not set -> folder assumed \"polaris\")");
            }
            else
            {
                std::string gameDir = std::string(m_settingsCineExport) + "\\game";
                DWORD a = GetFileAttributesA(gameDir.c_str());
                if (a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY))
                    ImGui::TextColored(ImVec4(0.35f, 1.0f, 0.50f, 1.0f), "cinematics dump: Found");
                else
                    ImGui::TextColored(ImVec4(1.0f, 0.40f, 0.40f, 1.0f),
                                       "cinematics dump: 'game' subfolder not found");
            }

            // -- List edit shortcuts -----------------------------
            ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
            ImGui::TextUnformatted("List Edit Shortcuts");
            ImGui::Spacing();
            ImGui::TextDisabled(
                "Shortcuts for the per-row controls in moveset list sub-windows.\n"
                "They fire only while that specific list window is focused and hovered.\n"
                "Click a shortcut, then press the desired key combo (Esc cancels).");
            ImGui::Spacing();

            ListKeybinds& kb = ListKeybinds::Get();

            // Resolve an in-progress capture from this frame's key presses.
            if (m_capturingKey >= 0)
            {
                if (ImGui::IsKeyPressed(ImGuiKey_Escape, false))
                {
                    m_capturingKey = -1;
                }
                else
                {
                    for (int k = ImGuiKey_NamedKey_BEGIN; k < ImGuiKey_NamedKey_END; ++k)
                    {
                        ImGuiKey key = (ImGuiKey)k;
                        if (key == ImGuiKey_LeftCtrl  || key == ImGuiKey_RightCtrl  ||
                            key == ImGuiKey_LeftShift || key == ImGuiKey_RightShift ||
                            key == ImGuiKey_LeftAlt   || key == ImGuiKey_RightAlt   ||
                            key == ImGuiKey_LeftSuper || key == ImGuiKey_RightSuper)
                            continue;   // ignore the modifier keys themselves
                        if (ImGui::IsKeyPressed(key, false))
                        {
                            KeyBind b;
                            b.key   = key;
                            b.ctrl  = ImGui::GetIO().KeyCtrl;
                            b.shift = ImGui::GetIO().KeyShift;
                            b.alt   = ImGui::GetIO().KeyAlt;
                            kb.binds[m_capturingKey] = b;
                            m_capturingKey = -1;
                            break;
                        }
                    }
                }
            }

            if (ImGui::BeginTable("##kbtbl", 2,
                                  ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_BordersInnerH))
            {
                ImGui::TableSetupColumn("Action",   ImGuiTableColumnFlags_WidthFixed, 130.0f);
                ImGui::TableSetupColumn("Shortcut", ImGuiTableColumnFlags_WidthStretch);
                for (int i = 0; i < (int)ListShortcut::COUNT; ++i)
                {
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::AlignTextToFramePadding();
                    ImGui::TextUnformatted(ListKeybinds::Label((ListShortcut)i));
                    ImGui::TableSetColumnIndex(1);
                    ImGui::PushID(i);
                    const bool capturing = (m_capturingKey == i);
                    std::string chord = capturing ? "Press a key..." : kb.Encode((ListShortcut)i);
                    if (ImGui::Button(chord.c_str(), ImVec2(150.0f, 0.0f)))
                        m_capturingKey = capturing ? -1 : i;
                    ImGui::SameLine();
                    if (ImGui::SmallButton("Clear"))
                    {
                        kb.binds[i].key = ImGuiKey_None;
                        if (capturing) m_capturingKey = -1;
                    }
                    ImGui::PopID();
                }
                ImGui::EndTable();
            }
            ImGui::Spacing();
            if (ImGui::Button("Reset shortcuts to defaults"))
            {
                kb.SetDefaults();
                m_capturingKey = -1;
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
