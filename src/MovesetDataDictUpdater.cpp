// MovesetDataDictUpdater.cpp
// Compares the remote MovesetDatas version against the local cache and
// downloads a fresh data.json when the remote version is newer.
//
// Remote host : raw.githubusercontent.com (HTTPS)
// Remote paths:
//   /umin135/Polaris_TKDataModEditor/main/data/MovesetDatas/version.json
//   /umin135/Polaris_TKDataModEditor/main/data/MovesetDatas/data.json
#include "MovesetDataDictUpdater.h"
#define NOMINMAX
#include <windows.h>
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")
#include <string>
#include <cstdio>
#include <cstdlib>

// ---------------------------------------------------------------------------
//  Helpers
// ---------------------------------------------------------------------------

static const wchar_t* kHost    = L"raw.githubusercontent.com";
static const wchar_t* kVerPath = L"/umin135/Polaris_TKDataModEditor/main/data/MovesetDatas/version.json";
static const wchar_t* kDatPath = L"/umin135/Polaris_TKDataModEditor/main/data/MovesetDatas/data.json";
static const wchar_t* kAgent   = L"PolarisTKDataEditor/1.0";

static std::string HttpsGet(const wchar_t* path, int timeoutMs = 8000)
{
    std::string result;

    HINTERNET hSess = WinHttpOpen(kAgent,
        WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSess) return result;

    HINTERNET hConn = WinHttpConnect(hSess, kHost,
        INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!hConn) { WinHttpCloseHandle(hSess); return result; }

    HINTERNET hReq = WinHttpOpenRequest(hConn, L"GET", path,
        nullptr, WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        WINHTTP_FLAG_SECURE);
    if (!hReq) {
        WinHttpCloseHandle(hConn);
        WinHttpCloseHandle(hSess);
        return result;
    }

    WinHttpSetTimeouts(hReq, timeoutMs, timeoutMs, timeoutMs, timeoutMs);

    if (WinHttpSendRequest(hReq, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                           WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
        WinHttpReceiveResponse(hReq, nullptr))
    {
        DWORD statusCode = 0;
        DWORD statusSize = sizeof(statusCode);
        WinHttpQueryHeaders(hReq,
            WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX,
            &statusCode, &statusSize, WINHTTP_NO_HEADER_INDEX);

        if (statusCode == 200)
        {
            DWORD read = 0;
            char buf[4096];
            while (WinHttpReadData(hReq, buf, sizeof(buf), &read) && read > 0)
                result.append(buf, read);
        }
    }

    WinHttpCloseHandle(hReq);
    WinHttpCloseHandle(hConn);
    WinHttpCloseHandle(hSess);
    return result;
}

static int ParseVersion(const std::string& json)
{
    size_t pos = json.find("\"version\"");
    if (pos == std::string::npos) return -1;
    pos = json.find(':', pos);
    if (pos == std::string::npos) return -1;
    ++pos;
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t')) ++pos;
    if (pos >= json.size()) return -1;
    return std::atoi(json.c_str() + pos);
}

static int ReadLocalVersion(const std::string& verPath)
{
    FILE* f = nullptr;
    fopen_s(&f, verPath.c_str(), "rb");
    if (!f) return -1;
    char buf[128] = {};
    fread(buf, 1, sizeof(buf) - 1, f);
    fclose(f);
    return ParseVersion(buf);
}

static bool WriteFileToDisk(const std::string& path, const std::string& data)
{
    std::string tmp = path + ".tmp";
    FILE* f = nullptr;
    fopen_s(&f, tmp.c_str(), "wb");
    if (!f) return false;
    bool ok = (fwrite(data.data(), 1, data.size(), f) == data.size());
    fclose(f);
    if (!ok) { remove(tmp.c_str()); return false; }
    remove(path.c_str());
    return (rename(tmp.c_str(), path.c_str()) == 0);
}

// ---------------------------------------------------------------------------
//  Public entry point
// ---------------------------------------------------------------------------

bool MovesetDataDictCheckAndUpdate(const std::string& resDir)
{
    std::string msdDir = resDir + "\\MovesetDatas";
    CreateDirectoryA(msdDir.c_str(), nullptr);

    std::string localVerPath  = msdDir + "\\version.json";
    std::string localDataPath = msdDir + "\\data.json";

    int localVer = ReadLocalVersion(localVerPath);

    std::string remoteVerJson = HttpsGet(kVerPath);
    if (remoteVerJson.empty()) return false;

    int remoteVer = ParseVersion(remoteVerJson);
    if (remoteVer <= 0 || remoteVer <= localVer) return false;

    std::string remoteData = HttpsGet(kDatPath, 30000);
    if (remoteData.empty()) return false;

    if (remoteData.front() != '{' || remoteData.find(':') == std::string::npos)
        return false;

    if (!WriteFileToDisk(localDataPath, remoteData))  return false;
    if (!WriteFileToDisk(localVerPath,  remoteVerJson)) return false;

    return true;
}
