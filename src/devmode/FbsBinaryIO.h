#pragma once
#ifdef _DEBUG

#include "data/ModData.h"
#include <vector>
#include <string>
#include <cstdint>

// ─────────────────────────────────────────────────────────────────────────────
//  FlatBuffers binary reader/writer (dev-mode only)
//  Supports: customize_item_common_list.bin
// ─────────────────────────────────────────────────────────────────────────────

namespace FbsBinaryIO
{
    // Read a .bin file into ContentsBinData.
    // Detects bin type by filename (basename of path).
    // Returns false if the file cannot be read or the type is unsupported.
    bool ImportBin(const std::string& path, ContentsBinData& out);

    // Write ContentsBinData back to a .bin file at the given path.
    // Returns false on write failure.
    bool ExportBin(const std::string& path, const ContentsBinData& bin);

    // Open a Win32 file-open dialog filtered to *.bin and return the chosen path.
    // Returns empty string if cancelled.
    std::string OpenImportDialog();

    // Open a Win32 folder-picker dialog and return the chosen directory path.
    // Returns empty string if cancelled.
    std::string OpenExportFolderDialog();

    // Export all entries in bin to a tab-separated values (.tsv) file.
    // First row is a header with field names.
    bool ExportTsv(const std::string& path, const ContentsBinData& bin);

    // Import entries from a .tsv file, REPLACING existing entries in bin.
    // The first row must be a matching header.
    bool ImportTsv(const std::string& path, ContentsBinData& bin);

    // Open a Win32 save dialog for *.tsv files.
    std::string OpenTsvSaveDialog(const std::string& defaultName);

    // Open a Win32 open dialog for *.tsv files.
    std::string OpenTsvOpenDialog();
}

#endif // _DEBUG
