#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <cstdio>

// -------------------------------------------------------------
//  TkdataExtractor
//
//  Selectively extracts individual files from tkdata.bin without
//  loading the full archive into memory.
//
//  Pipeline (mirrors tkdata.py logic):
//    1. Verify header magic "__TEKKEN8FILES__"
//    2. XOR-decrypt 128-byte footer -> read TOC location
//    3. Read TOC -> AES-256-ECB decrypt -> zstd decompress
//    4. Parse 64-byte TOC entries -> build hash -> entry map
//
//  Per-file extraction (ExtractFile):
//    fseek + fread -> AES-256-ECB decrypt -> zstd decompress
//
//  If a file is not found in the TOC (hash mismatch due to a game
//  update), ExtractFile returns an empty vector. Callers should
//  treat this as a non-fatal skip.
// -------------------------------------------------------------
class TkdataExtractor
{
public:
    TkdataExtractor()  = default;
    ~TkdataExtractor() { Close(); }

    // Open tkdata.bin and parse the TOC.
    // Returns false and sets errorMsg on failure.
    bool Open(const std::string& path, std::string& errorMsg);

    void Close();
    bool IsOpen() const { return m_file != nullptr; }

    // Extract a file by VFS path (e.g., "mothead/bin/ant.anmbin").
    // Returns decompressed bytes, or empty vector if not found / error.
    std::vector<uint8_t> ExtractFile(const std::string& vfsPath) const;

    size_t EntryCount() const { return m_entries.size(); }

private:
    struct TocEntry {
        uint8_t  flag;       // if nonzero: decompress full decrypted buf
        uint8_t  key;        // AES key seed (index into key pool)
        uint64_t fileOffset; // data start = 0x10 + fileOffset
        uint64_t fileSize;   // AES-padded (encrypted) byte count to read
        uint64_t size2;      // actual compressed data length (used when flag==0)
    };

    FILE* m_file = nullptr;
    uint64_t m_fileSize = 0;
    std::unordered_map<uint64_t, TocEntry> m_entries; // hash -> entry

    bool ParseToc(std::string& errorMsg);
};
