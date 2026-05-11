#pragma once
#include <string>

// ZIP-based app update flow:
//   1. AppUpdateCheck()       — synchronous, call from background init thread
//   2. AppUpdateGetStatus()   — poll from main thread each frame
//   3. AppUpdateBeginDownload() — called when user confirms
//   4. AppUpdateApply()       — called when Ready; caller must PostQuitMessage(0)
//   5. AppUpdateDecline()     — called when user says No

enum class AppUpdateStatus {
    Idle,           // not yet checked
    UpToDate,       // checked, no newer version
    Available,      // newer version found, awaiting user decision
    Downloading,    // user confirmed, ZIP download in progress
    Ready,          // ZIP downloaded and verified, ready to apply
    Declined,       // user chose not to update
    DownloadFailed, // download error
};

struct AppUpdateInfo {
    int         version    = 0;
    std::string versionStr;
};

void AppUpdateCheck(const std::string& exePath);  // synchronous — run in init thread
AppUpdateStatus      AppUpdateGetStatus();
const AppUpdateInfo& AppUpdateGetInfo();
float                AppUpdateGetDownloadProgress();  // 0..1
void AppUpdateBeginDownload();
void AppUpdateApply();     // writes batch+ps1, launches batch
void AppUpdateDecline();
