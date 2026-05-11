// AppUpdater.cpp
// File-by-file update from the repo's _Release/ directory.
//   Check: fetches version.json synchronously (5s timeout) in init thread.
//   Download: GitHub Git Trees API → enumerate _Release/ blobs →
//             download each file to %TEMP%\PolarisUpdate\ preserving relative paths.
//   Apply: BAT + PS1 wait for this PID, Copy-Item to exeDir, relaunch.
#include "AppUpdater.h"
#include "AppStrings.h"
#define NOMINMAX
#include <windows.h>
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>

static const wchar_t* kAgent    = L"PolarisTKDataEditor/1.0";
static const wchar_t* kVerHost  = L"raw.githubusercontent.com";
static const wchar_t* kVerPath  = L"/umin135/Polaris_TKDataModEditor/main/data/app-version/version.json";
static const wchar_t* kApiHost  = L"api.github.com";
static const wchar_t* kApiTree  = L"/repos/umin135/Polaris_TKDataModEditor/git/trees/main?recursive=1";
static const char*    kRawBase  = "https://raw.githubusercontent.com/umin135/Polaris_TKDataModEditor/main/";
static const char*    kRelPrefix = "_Release/";

static std::atomic<AppUpdateStatus> s_status  { AppUpdateStatus::Idle };
static AppUpdateInfo                 s_info    {};
static std::atomic<float>           s_progress{ 0.0f };
static std::string                  s_exePath;
static std::string                  s_tempDir;
static std::string                  s_localVersionStr;
static int                          s_localVersion = 0;

// ---------------------------------------------------------------------------
//  HTTP helpers
// ---------------------------------------------------------------------------

static std::string HttpsGetStr(const wchar_t* host, const wchar_t* path, int timeoutMs = 5000)
{
    std::string result;
    HINTERNET hSess = WinHttpOpen(kAgent, WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSess) return result;
    HINTERNET hConn = WinHttpConnect(hSess, host, INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!hConn) { WinHttpCloseHandle(hSess); return result; }
    HINTERNET hReq  = WinHttpOpenRequest(hConn, L"GET", path, nullptr,
        WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
    if (!hReq) { WinHttpCloseHandle(hConn); WinHttpCloseHandle(hSess); return result; }

    WinHttpSetTimeouts(hReq, timeoutMs, timeoutMs, timeoutMs, timeoutMs);
    if (WinHttpSendRequest(hReq, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                           WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
        WinHttpReceiveResponse(hReq, nullptr))
    {
        DWORD code = 0, codeLen = sizeof(code);
        WinHttpQueryHeaders(hReq, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX, &code, &codeLen, WINHTTP_NO_HEADER_INDEX);
        if (code == 200) {
            DWORD read = 0; char buf[4096];
            while (WinHttpReadData(hReq, buf, sizeof(buf), &read) && read > 0)
                result.append(buf, read);
        }
    }
    WinHttpCloseHandle(hReq); WinHttpCloseHandle(hConn); WinHttpCloseHandle(hSess);
    return result;
}

// Downloads an arbitrary HTTPS URL to a file path, following redirects.
static bool HttpsDownloadToFile(const std::wstring& url, const std::string& destPath, int timeoutMs = 30000)
{
    URL_COMPONENTS uc = {};
    uc.dwStructSize = sizeof(uc);
    wchar_t hostBuf[512] = {}, pathBuf[4096] = {};
    uc.lpszHostName = hostBuf; uc.dwHostNameLength = 512;
    uc.lpszUrlPath  = pathBuf; uc.dwUrlPathLength  = 4096;
    if (!WinHttpCrackUrl(url.c_str(), 0, 0, &uc)) return false;

    HINTERNET hSess = WinHttpOpen(kAgent, WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSess) return false;
    HINTERNET hConn = WinHttpConnect(hSess, hostBuf, INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!hConn) { WinHttpCloseHandle(hSess); return false; }
    HINTERNET hReq  = WinHttpOpenRequest(hConn, L"GET", pathBuf, nullptr,
        WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
    if (!hReq) { WinHttpCloseHandle(hConn); WinHttpCloseHandle(hSess); return false; }

    DWORD policy = WINHTTP_OPTION_REDIRECT_POLICY_ALWAYS;
    WinHttpSetOption(hReq, WINHTTP_OPTION_REDIRECT_POLICY, &policy, sizeof(policy));
    WinHttpSetTimeouts(hReq, timeoutMs, timeoutMs, timeoutMs, timeoutMs);

    bool success = false;
    if (WinHttpSendRequest(hReq, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                           WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
        WinHttpReceiveResponse(hReq, nullptr))
    {
        DWORD code = 0, codeLen = sizeof(code);
        WinHttpQueryHeaders(hReq, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX, &code, &codeLen, WINHTTP_NO_HEADER_INDEX);
        if (code == 200) {
            FILE* f = nullptr;
            fopen_s(&f, destPath.c_str(), "wb");
            if (f) {
                DWORD read = 0; uint8_t buf[65536];
                success = true;
                while (WinHttpReadData(hReq, buf, sizeof(buf), &read) && read > 0) {
                    if (fwrite(buf, 1, read, f) != read) { success = false; break; }
                }
                fclose(f);
                if (!success) remove(destPath.c_str());
            }
        }
    }
    WinHttpCloseHandle(hReq); WinHttpCloseHandle(hConn); WinHttpCloseHandle(hSess);
    return success;
}

// ---------------------------------------------------------------------------
//  JSON parsers
// ---------------------------------------------------------------------------

static int ParseVersion(const std::string& json)
{
    size_t pos = json.find("\"version\"");
    if (pos == std::string::npos) return -1;
    pos = json.find(':', pos);
    if (pos == std::string::npos) return -1;
    ++pos;
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t')) ++pos;
    return (pos < json.size()) ? std::atoi(json.c_str() + pos) : -1;
}

static std::string ParseStringField(const std::string& json, const char* field)
{
    size_t pos = json.find(field);
    if (pos == std::string::npos) return {};
    pos = json.find(':', pos);
    if (pos == std::string::npos) return {};
    while (pos < json.size() && json[pos] != '"') ++pos;
    if (pos >= json.size()) return {};
    ++pos;
    size_t end = json.find('"', pos);
    if (end == std::string::npos) return {};
    return json.substr(pos, end - pos);
}

// Extract { "path": "...", "type": "blob" } entries from git/trees JSON
// where path starts with "_Release/".
static std::vector<std::string> ParseReleasePaths(const std::string& json)
{
    std::vector<std::string> result;
    const std::string relPrefix = kRelPrefix;

    size_t treePos = json.find("\"tree\":");
    if (treePos == std::string::npos) return result;
    size_t pos = json.find('[', treePos);
    if (pos == std::string::npos) return result;

    while (true)
    {
        size_t obj0 = json.find('{', pos);
        if (obj0 == std::string::npos) break;
        size_t obj1 = json.find('}', obj0);
        if (obj1 == std::string::npos) break;

        std::string obj = json.substr(obj0, obj1 - obj0 + 1);

        std::string objPath, objType;

        size_t pPos = obj.find("\"path\":\"");
        if (pPos != std::string::npos) {
            pPos += 8;
            size_t pEnd = obj.find('"', pPos);
            if (pEnd != std::string::npos)
                objPath = obj.substr(pPos, pEnd - pPos);
        }

        size_t tPos = obj.find("\"type\":\"");
        if (tPos != std::string::npos) {
            tPos += 8;
            size_t tEnd = obj.find('"', tPos);
            if (tEnd != std::string::npos)
                objType = obj.substr(tPos, tEnd - tPos);
        }

        if (objType == "blob" &&
            objPath.size() > relPrefix.size() &&
            objPath.compare(0, relPrefix.size(), relPrefix) == 0)
        {
            result.push_back(objPath);
        }

        pos = obj1 + 1;
    }
    return result;
}

static std::wstring Utf8ToWide(const std::string& s)
{
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    if (n <= 1) return {};
    std::wstring w(n - 1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &w[0], n);
    return w;
}

// ---------------------------------------------------------------------------
//  Filesystem helpers
// ---------------------------------------------------------------------------

// Create all directory components of a path (no trailing separator).
static void CreateDirRecursive(const std::string& path)
{
    for (size_t i = 1; i < path.size(); ++i) {
        if (path[i] == '\\' || path[i] == '/') {
            std::string sub = path.substr(0, i);
            CreateDirectoryA(sub.c_str(), nullptr);
        }
    }
    CreateDirectoryA(path.c_str(), nullptr);
}

// Replace all '/' with '\\' in place.
static void ForwardToBackslash(std::string& s)
{
    for (char& c : s)
        if (c == '/') c = '\\';
}

// ---------------------------------------------------------------------------
//  Background download thread
// ---------------------------------------------------------------------------

static void DownloadThread()
{
    // 1. Enumerate _Release/ files via Git Trees API
    std::string treeJson = HttpsGetStr(kApiHost, kApiTree, 15000);
    if (treeJson.empty()) {
        s_status.store(AppUpdateStatus::DownloadFailed, std::memory_order_release);
        return;
    }

    std::vector<std::string> paths = ParseReleasePaths(treeJson);
    if (paths.empty()) {
        s_status.store(AppUpdateStatus::DownloadFailed, std::memory_order_release);
        return;
    }

    // 2. Download each file to s_tempDir, maintaining relative path
    const size_t prefixLen = strlen(kRelPrefix);

    for (int i = 0; i < (int)paths.size(); ++i)
    {
        // relPath: strip "_Release/" prefix, convert to backslashes
        std::string relPath = paths[i].substr(prefixLen);
        ForwardToBackslash(relPath);

        std::string destPath = s_tempDir + relPath;

        // Ensure parent directory exists
        size_t lastSep = destPath.rfind('\\');
        if (lastSep != std::string::npos)
            CreateDirRecursive(destPath.substr(0, lastSep));

        // Download from raw.githubusercontent.com
        std::wstring url = Utf8ToWide(std::string(kRawBase) + paths[i]);
        if (!HttpsDownloadToFile(url, destPath)) {
            s_status.store(AppUpdateStatus::DownloadFailed, std::memory_order_release);
            return;
        }

        s_progress.store((float)(i + 1) / (float)paths.size(), std::memory_order_relaxed);
    }

    s_status.store(AppUpdateStatus::Ready, std::memory_order_release);
}

// ---------------------------------------------------------------------------
//  Public API
// ---------------------------------------------------------------------------

void AppVersionLoadLocal(const std::string& exeDir)
{
    const std::string candidates[] = {
        exeDir + "\\res\\version.json",
        exeDir + "\\..\\res\\version.json",
        exeDir + "\\..\\..\\res\\version.json",
        exeDir + "\\..\\..\\..\\res\\version.json",
    };
    for (const auto& path : candidates) {
        FILE* f = nullptr;
        fopen_s(&f, path.c_str(), "rb");
        if (!f) continue;
        std::string json;
        char buf[512]; size_t n;
        while ((n = fread(buf, 1, sizeof(buf), f)) > 0) json.append(buf, n);
        fclose(f);
        int v = ParseVersion(json);
        if (v > 0) s_localVersion = v;
        std::string vs = ParseStringField(json, "\"version_str\"");
        if (!vs.empty()) s_localVersionStr = vs;
        break;
    }
}

const char* AppVersionGetStr()
{
    return s_localVersionStr.empty() ? APPSTR_VERSION : s_localVersionStr.c_str();
}

void AppUpdateCheck(const std::string& exePath)
{
    s_exePath = exePath;

    std::string verJson = HttpsGetStr(kVerHost, kVerPath);
    if (verJson.empty()) return;

    int remoteVer = ParseVersion(verJson);
    int localVer  = (s_localVersion > 0) ? s_localVersion : APPSTR_VERSION_INT;
    if (remoteVer <= 0 || remoteVer == localVer) {
        s_status.store(AppUpdateStatus::UpToDate, std::memory_order_release);
        return;
    }

    s_info.version    = remoteVer;
    s_info.versionStr = ParseStringField(verJson, "\"version_str\"");

    s_status.store(AppUpdateStatus::Available, std::memory_order_release);
}

AppUpdateStatus AppUpdateGetStatus()
{
    return s_status.load(std::memory_order_acquire);
}

const AppUpdateInfo& AppUpdateGetInfo()
{
    return s_info;
}

float AppUpdateGetDownloadProgress()
{
    return s_progress.load(std::memory_order_relaxed);
}

void AppUpdateBeginDownload()
{
    char tempBuf[MAX_PATH] = {};
    GetTempPathA(MAX_PATH, tempBuf);
    s_tempDir = std::string(tempBuf) + "PolarisUpdate\\";
    s_progress.store(0.0f, std::memory_order_relaxed);
    s_status.store(AppUpdateStatus::Downloading, std::memory_order_release);
    std::thread(DownloadThread).detach();
}

void AppUpdateApply()
{
    std::string exeDir = s_exePath;
    size_t sep = exeDir.rfind('\\');
    if (sep != std::string::npos) exeDir = exeDir.substr(0, sep);

    DWORD pid = GetCurrentProcessId();

    std::string batPath = exeDir + "\\update_apply.bat";
    std::string ps1Path = exeDir + "\\update_apply.ps1";

    // PS1: copy downloaded files to exe dir, then clean up temp
    char ps1[2048];
    snprintf(ps1, sizeof(ps1),
        "$src  = '%s'\r\n"
        "$dest = '%s'\r\n"
        "Copy-Item -Path \"$src*\" -Destination $dest -Recurse -Force\r\n"
        "Remove-Item $src -Recurse -Force -ErrorAction SilentlyContinue\r\n",
        s_tempDir.c_str(), exeDir.c_str());

    {
        FILE* f = nullptr;
        fopen_s(&f, ps1Path.c_str(), "w");
        if (f) { fputs(ps1, f); fclose(f); }
    }

    // BAT: wait for this PID, run PS1, relaunch exe
    char bat[2048];
    snprintf(bat, sizeof(bat),
        "@echo off\r\n"
        ":wait\r\n"
        "tasklist /fi \"PID eq %lu\" 2>nul | find \"%lu\" >nul\r\n"
        "if not errorlevel 1 (\r\n"
        "    timeout /t 1 /nobreak >nul\r\n"
        "    goto wait\r\n"
        ")\r\n"
        "powershell -NoProfile -ExecutionPolicy Bypass -File \"%s\"\r\n"
        "if errorlevel 1 goto end\r\n"
        "start \"\" \"%s\"\r\n"
        ":end\r\n"
        "del \"%s\" 2>nul\r\n"
        "del \"%%~f0\"\r\n",
        (unsigned long)pid, (unsigned long)pid,
        ps1Path.c_str(),
        s_exePath.c_str(),
        ps1Path.c_str());

    {
        FILE* f = nullptr;
        fopen_s(&f, batPath.c_str(), "w");
        if (f) { fputs(bat, f); fclose(f); }
    }

    ShellExecuteA(nullptr, "open", batPath.c_str(), nullptr, exeDir.c_str(), SW_HIDE);
}

void AppUpdateDecline()
{
    s_status.store(AppUpdateStatus::Declined, std::memory_order_release);
}
