#pragma once
#include <string>

// Background app-update check against GitHub.
// Call AppUpdateCheckAndDownload() once at startup (spawns detached thread).
// Poll AppUpdateIsReady() each frame; when true, call AppUpdateApply() + PostQuitMessage(0).

void AppUpdateCheckAndDownload(const std::string& exePath);
bool AppUpdateIsReady();
void AppUpdateApply(const std::string& exePath);
