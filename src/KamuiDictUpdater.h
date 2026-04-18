#pragma once
#include <string>

// Checks the remote kamui-hashes version and downloads data.json if newer.
// resDir: path to the exe's res/ directory (e.g. "C:\\path\\to\\res").
// Returns true if data.json was updated and LabelDB should be reloaded.
bool KamuiDictCheckAndUpdate(const std::string& resDir);
