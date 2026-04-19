#pragma once
#include <string>

// Checks the remote MovesetDatas version and downloads data.json if newer.
// resDir: path to the exe's res/ directory (e.g. "C:\\path\\to\\res").
// Returns true if data.json was updated and MovesetDataDict should be reloaded.
bool MovesetDataDictCheckAndUpdate(const std::string& resDir);
