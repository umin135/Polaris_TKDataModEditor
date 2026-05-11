// AppUpdater.cpp
// Checks data/app-version/version.json on raw.githubusercontent.com.
// If remote version > APPSTR_VERSION_INT, downloads the exe from download_url
// (follows GitHub Releases redirect) and saves it as <exePath>.new.
// AppUpdateApply() writes a batch script that waits for this process to exit,
// replaces the exe, then re-launches it.
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

static const wchar_t* kAgent   = L"PolarisTKDataEditor/1.0";
static const wchar_t* kVerHost = L"raw.githubusercontent.com";
static const wchar_t* kVerPath = L"/umin135/Polaris_TKDataModEditor/main/data/app-version/version.json";

static std::atomic<bool> s_ready{false};

// ---------------------------------------------------------------------------
//  HTTP helpers
// ---------------------------------------------------------------------------

static std::string HttpsGetStr(const wchar_t* host, const wchar_t* path, int timeoutMs = 8000)
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

// Download binary from an arbitrary HTTPS URL, following cross-host redirects
// (required for GitHub Releases assets which redirect to CDN).
static std::vector<uint8_t> HttpsGetBinary(const std::wstring& url, int timeoutMs = 60000)
{
    std::vector<uint8_t> result;
    URL_COMPONENTS uc = {};
    uc.dwStructSize = sizeof(uc);
    wchar_t hostBuf[512] = {}, pathBuf[4096] = {};
    uc.lpszHostName = hostBuf; uc.dwHostNameLength = 512;
    uc.lpszUrlPath  = pathBuf; uc.dwUrlPathLength  = 4096;
    if (!WinHttpCrackUrl(url.c_str(), 0, 0, &uc)) return result;

    HINTERNET hSess = WinHttpOpen(kAgent, WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSess) return result;
    HINTERNET hConn = WinHttpConnect(hSess, hostBuf, INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!hConn) { WinHttpCloseHandle(hSess); return result; }
    HINTERNET hReq  = WinHttpOpenRequest(hConn, L"GET", pathBuf, nullptr,
        WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
    if (!hReq) { WinHttpCloseHandle(hConn); WinHttpCloseHandle(hSess); return result; }

    DWORD policy = WINHTTP_OPTION_REDIRECT_POLICY_ALWAYS;
    WinHttpSetOption(hReq, WINHTTP_OPTION_REDIRECT_POLICY, &policy, sizeof(policy));
    WinHttpSetTimeouts(hReq, timeoutMs, timeoutMs, timeoutMs, timeoutMs);

    if (WinHttpSendRequest(hReq, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                           WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
        WinHttpReceiveResponse(hReq, nullptr))
    {
        DWORD code = 0, codeLen = sizeof(code);
        WinHttpQueryHeaders(hReq, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX, &code, &codeLen, WINHTTP_NO_HEADER_INDEX);
        if (code == 200) {
            DWORD read = 0; uint8_t buf[65536];
            while (WinHttpReadData(hReq, buf, sizeof(buf), &read) && read > 0)
                result.insert(result.end(), buf, buf + read);
        }
    }
    WinHttpCloseHandle(hReq); WinHttpCloseHandle(hConn); WinHttpCloseHandle(hSess);
    return result;
}

// ---------------------------------------------------------------------------
//  JSON mini-parsers
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

static std::string ParseDownloadUrl(const std::string& json)
{
    size_t pos = json.find("\"download_url\"");
    if (pos == std::string::npos) return {};
    pos = json.find(':', pos);
    if (pos == std::string::npos) return {};
    while (pos < json.size() && json[pos] != '"') ++pos;
    if (pos >= json.size()) return {};
    ++pos; // skip opening quote
    size_t end = json.find('"', pos);
    if (end == std::string::npos) return {};
    return json.substr(pos, end - pos);
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
//  Background thread
// ---------------------------------------------------------------------------

static void UpdateThread(std::string exePath)
{
    std::string verJson = HttpsGetStr(kVerHost, kVerPath);
    if (verJson.empty()) return;

    int remoteVer = ParseVersion(verJson);
    if (remoteVer <= 0 || remoteVer <= APPSTR_VERSION_INT) return;

    std::string downloadUrl = ParseDownloadUrl(verJson);
    if (downloadUrl.empty()) return;

    std::wstring wUrl = Utf8ToWide(downloadUrl);
    if (wUrl.empty()) return;

    std::vector<uint8_t> data = HttpsGetBinary(wUrl);
    if (data.size() < 2 || data[0] != 'M' || data[1] != 'Z') return;

    std::string newPath = exePath + ".new";
    FILE* f = nullptr;
    fopen_s(&f, newPath.c_str(), "wb");
    if (!f) return;
    bool ok = (fwrite(data.data(), 1, data.size(), f) == data.size());
    fclose(f);
    if (!ok) { remove(newPath.c_str()); return; }

    s_ready.store(true, std::memory_order_release);
}

// ---------------------------------------------------------------------------
//  Public API
// ---------------------------------------------------------------------------

void AppUpdateCheckAndDownload(const std::string& exePath)
{
    std::thread(UpdateThread, exePath).detach();
}

bool AppUpdateIsReady()
{
    return s_ready.load(std::memory_order_acquire);
}

void AppUpdateApply(const std::string& exePath)
{
    std::string exeDir = exePath;
    size_t sep = exeDir.rfind('\\');
    if (sep != std::string::npos) exeDir = exeDir.substr(0, sep);

    std::string newPath = exePath + ".new";
    std::string batPath = exeDir + "\\update_apply.bat";
    DWORD pid = GetCurrentProcessId();

    // Batch: wait for this PID to exit, replace exe, relaunch.
    char bat[2048];
    snprintf(bat, sizeof(bat),
        "@echo off\r\n"
        ":wait\r\n"
        "tasklist /fi \"PID eq %lu\" 2>nul | find \"%lu\" >nul\r\n"
        "if not errorlevel 1 (\r\n"
        "    timeout /t 1 /nobreak >nul\r\n"
        "    goto wait\r\n"
        ")\r\n"
        "move /y \"%s\" \"%s\"\r\n"
        "if errorlevel 1 goto end\r\n"
        "start \"\" \"%s\"\r\n"
        ":end\r\n"
        "del \"%%~f0\"\r\n",
        (unsigned long)pid, (unsigned long)pid,
        newPath.c_str(), exePath.c_str(),
        exePath.c_str());

    FILE* f = nullptr;
    fopen_s(&f, batPath.c_str(), "w");
    if (f) { fputs(bat, f); fclose(f); }

    ShellExecuteA(nullptr, "open", batPath.c_str(), nullptr, exeDir.c_str(), SW_HIDE);
}
