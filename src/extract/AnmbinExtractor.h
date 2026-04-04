#pragma once
#include "GameProcess.h"
#include <string>
#include <vector>

// -------------------------------------------------------------
//  AnmbinExtractor
//  Scans game virtual memory for the anmbin animation container
//  and extracts it to a file.
//
//  TK8 anmbin container header layout (runtime, 0x98 bytes):
//    0x00: u32    — offset from container start to first animation
//    0x04: u32[6] — per-category animation counts
//    0x1C: u32[6] — per-category list byte sizes
//    0x34: u32    — padding / unknown
//    0x38: u64[6] — per-category animation pool absolute addresses
//    0x68: u64[6] — per-category animation list absolute addresses
//
//  Each animation inside a pool is a size-prefixed FlatBuffers binary:
//    [u32 size][FlatBuffer data]
//  The FlatBuffer file identifier "PANM" appears at bytes 4-7 of the data.
//
//  Extraction strategy:
//    1. Enumerate committed VM regions via VirtualQueryEx.
//    2. Validate each region's first 0x98 bytes as an anmbin header.
//    3. Score candidates by proximity to the known motbinAddr.
//    4. Determine container size by iterating all animation size-prefixes.
//    5. Fix up the 12 absolute pointers to container-relative offsets.
//    6. Save as moveset.anmbin.
// -------------------------------------------------------------

class AnmbinExtractor {
public:
    // Scan game memory for the anmbin container.
    // motbinAddr is used as a proximity hint to pick the right player slot
    // when both P1 and P2 have anmbins loaded.
    // Returns true and fills outBytes (fixed-up) on success.
    bool FindAndExtract(const GameProcessInfo& proc,
                        uintptr_t             motbinAddr,
                        std::vector<uint8_t>& outBytes,
                        std::string&          errorMsg);

    // Save the extracted bytes to <destFolder>/moveset.anmbin.
    bool SaveToFile(const std::vector<uint8_t>& bytes,
                    const std::string&           destFolder,
                    std::string&                 errorMsg);

private:
    // Validate a candidate region as an anmbin container.
    // hdr must be exactly kHeaderSize bytes read from candidateBase.
    bool ValidateHeader(const GameProcessInfo& proc,
                        uintptr_t              candidateBase,
                        const uint8_t*         hdr) const;

    // Walk through all animation pools to find the last byte of content.
    // Returns the offset (from containerBase) of the last data byte + 1.
    size_t ComputeContainerSize(const GameProcessInfo& proc,
                                uintptr_t              containerBase,
                                const uint8_t*         hdr,
                                size_t                 regionSize) const;

    static constexpr size_t   kHeaderSize     = 0x98;
    static constexpr size_t   kMinAnmbinBytes = 10ULL * 1024 * 1024; // 10 MB min region
    static constexpr size_t   kMaxAnmbinBytes = 512ULL * 1024 * 1024; // 512 MB hard cap
    static constexpr uint32_t kMaxAnimCount   = 200000;
    static constexpr uint32_t kMaxAnimBytes   = 64ULL * 1024 * 1024; // single anim ≤ 64 MB
};
