#pragma once
#include <string>

// Checks the remote fbsdatas version and downloads data.json if newer.
// resDir: path to the exe's res/ directory (e.g. "C:\\path\\to\\res").
// Returns true if data.json was updated and FbsDataDict should be reloaded.
bool FbsDataDictCheckAndUpdate(const std::string& resDir);
