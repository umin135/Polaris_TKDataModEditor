#pragma once
#include <string>

// Checks the remote data/<subDir>/version.json in the repository and downloads data/<subDir>/data.json
// into <resDir>\<subDir>\ when the remote version is newer (same scheme as KamuiDictUpdater /
// FbsDataDictUpdater). resDir: path to the exe's res/ directory.
// Returns true if data.json was updated and should be reloaded.
bool RemoteDataCheckAndUpdate(const std::string& resDir, const char* subDir);
