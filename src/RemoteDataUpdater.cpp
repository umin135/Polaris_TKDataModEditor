// RemoteDataUpdater.cpp
// Generic version-checked download of a repository data folder:
//   https://raw.githubusercontent.com/umin135/Polaris_TKDataModEditor/main/data/<subDir>/version.json
//   https://raw.githubusercontent.com/umin135/Polaris_TKDataModEditor/main/data/<subDir>/data.json
// Local cache: <resDir>\<subDir>\version.json + data.json.
#include "RemoteDataUpdater.h"
#define NOMINMAX
#include <windows.h>
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")
#include <string>
#include <cstdio>
#include <cstdlib>

static const wchar_t* kHost  = L"raw.githubusercontent.com";
static const char*    kBase  = "/umin135/Polaris_TKDataModEditor/main/data/";
static const wchar_t* kAgent = L"PolarisTKDataEditor/1.0";

static std::string HttpsGet(const std::string& path, int timeoutMs = 8000)
{
    std::string result;
    std::wstring wpath(path.begin(), path.end());

    HINTERNET hSess = WinHttpOpen(kAgent, WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                                  WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSess) return result;
    HINTERNET hConn = WinHttpConnect(hSess, kHost, INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!hConn) { WinHttpCloseHandle(hSess); return result; }
    HINTERNET hReq = WinHttpOpenRequest(hConn, L"GET", wpath.c_str(), nullptr, WINHTTP_NO_REFERER,
                                        WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
    if (!hReq) { WinHttpCloseHandle(hConn); WinHttpCloseHandle(hSess); return result; }

    WinHttpSetTimeouts(hReq, timeoutMs, timeoutMs, timeoutMs, timeoutMs);
    if (WinHttpSendRequest(hReq, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
        WinHttpReceiveResponse(hReq, nullptr))
    {
        DWORD statusCode = 0, statusSize = sizeof(statusCode);
        WinHttpQueryHeaders(hReq, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                            WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusSize, WINHTTP_NO_HEADER_INDEX);
        if (statusCode == 200) {
            DWORD read = 0; char buf[4096];
            while (WinHttpReadData(hReq, buf, sizeof(buf), &read) && read > 0) result.append(buf, read);
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
    return std::atoi(json.c_str() + pos + 1);
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

static bool WriteFileAtomic(const std::string& path, const std::string& data)
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

bool RemoteDataCheckAndUpdate(const std::string& resDir, const char* subDir)
{
    std::string dir = resDir + "\\" + subDir;
    CreateDirectoryA(dir.c_str(), nullptr);
    const std::string localVerPath  = dir + "\\version.json";
    const std::string localDataPath = dir + "\\data.json";
    const std::string remote        = std::string(kBase) + subDir + "/";

    int localVer = ReadLocalVersion(localVerPath);

    std::string remoteVerJson = HttpsGet(remote + "version.json");
    if (remoteVerJson.empty()) return false;
    int remoteVer = ParseVersion(remoteVerJson);
    if (remoteVer <= 0 || remoteVer <= localVer) return false;

    std::string remoteData = HttpsGet(remote + "data.json", 30000);
    if (remoteData.empty() || remoteData.front() != '{' || remoteData.find(':') == std::string::npos)
        return false;

    if (!WriteFileAtomic(localDataPath, remoteData))   return false;
    if (!WriteFileAtomic(localVerPath,  remoteVerJson)) return false;
    return true;
}
