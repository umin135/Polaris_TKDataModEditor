#pragma once
#include "fbsdata/data/ModData.h"
#include <string>
#include <vector>

namespace TkmodIO
{
    // Opens a Save File dialog and writes the mod as an uncompressed .tkmod ZIP.
    // Validates all entries before writing; shows MessageBox on errors.
    // Returns true on success.
    bool SaveDialog(const ModData& data);

    // Opens an Open File dialog and reads a .tkmod ZIP into 'data'.
    // Returns true on success.
    bool LoadDialog(ModData& data);

    // Loads a .tkmod ZIP from an explicit path (no dialog).
    // Used when the app is launched by double-clicking a .tkmod file.
    bool LoadFromPath(const std::string& path, ModData& data);

    // Validate all entries in the mod data.
    // Returns a list of error strings (empty = valid).
    std::vector<std::string> Validate(const ModData& data);
}
