#pragma once
#include "fbsdata/data/ModData.h"
#include <string>
#include <vector>

namespace TkmodIO
{
    // Opens a Save File dialog and writes the mod as an uncompressed .tkmod ZIP.
    // Validates all entries before writing; shows MessageBox on errors.
    // On success, outPath receives the file path that was written.
    // Returns true on success.
    bool SaveDialog(const ModData& data, std::string& outPath);

    // Saves to an explicit path without a dialog.
    // Validates entries before writing; shows MessageBox on errors.
    // Returns true on success.
    bool SaveToPath(const ModData& data, const std::string& path);

    // Opens an Open File dialog and reads a .tkmod ZIP into 'data'.
    // On success, outPath receives the file path that was loaded.
    // Returns true on success.
    bool LoadDialog(ModData& data, std::string& outPath);

    // Loads a .tkmod ZIP from an explicit path (no dialog).
    // Used when the app is launched by double-clicking a .tkmod file.
    bool LoadFromPath(const std::string& path, ModData& data);

    // Validate all entries in the mod data.
    // Returns a list of error strings (empty = valid).
    std::vector<std::string> Validate(const ModData& data);
}
