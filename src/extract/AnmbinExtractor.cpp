// AnmbinExtractor.cpp
// Scans the running TK8 process for the anmbin animation container and extracts it.
#include "AnmbinExtractor.h"
#include <windows.h>
#include <cstring>
#include <cstdio>
#include <algorithm>

// -------------------------------------------------------------
//  Internal helpers
// -------------------------------------------------------------

template<typename T>
static T ReadLocal(const uint8_t* buf, size_t off)
{
    T v = {};
    memcpy(&v, buf + off, sizeof(T));
    return v;
}

template<typename T>
static void WriteLocal(uint8_t* buf, size_t off, T val)
{
    memcpy(buf + off, &val, sizeof(T));
}

// Header field accessors (little-endian, which x64 Windows always is)
static uint32_t HdrFirstAnimOff(const uint8_t* h)  { return ReadLocal<uint32_t>(h, 0x00); }
static uint32_t HdrCatCount(const uint8_t* h, int i){ return ReadLocal<uint32_t>(h, 0x04 + i * 4); }
static uint32_t HdrListSize(const uint8_t* h, int i) { return ReadLocal<uint32_t>(h, 0x1C + i * 4); }
static uint64_t HdrPoolPtr(const uint8_t* h, int i)  { return ReadLocal<uint64_t>(h, 0x38 + i * 8); }
static uint64_t HdrListPtr(const uint8_t* h, int i)  { return ReadLocal<uint64_t>(h, 0x68 + i * 8); }

// PANM file identifier (FlatBuffers, bytes 4-7 of each animation)
static constexpr uint8_t kPanmMagic[4] = { 0x50, 0x41, 0x4E, 0x4D };

// -------------------------------------------------------------
//  ValidateHeader
// -------------------------------------------------------------

bool AnmbinExtractor::ValidateHeader(const GameProcessInfo& proc,
                                     uintptr_t              candidateBase,
                                     const uint8_t*         hdr) const
{
    // 1. firstAnimOffset must be sane: at least the header size, at most 256 MB
    uint32_t firstAnimOff = HdrFirstAnimOff(hdr);
    if (firstAnimOff < kHeaderSize || firstAnimOff > 0x10000000u)
        return false;

    // 2. At least one category must have animations; no category exceeds cap
    uint32_t totalAnims = 0;
    for (int i = 0; i < 6; ++i)
    {
        uint32_t cnt = HdrCatCount(hdr, i);
        if (cnt > kMaxAnimCount) return false;
        totalAnims += cnt;
    }
    if (totalAnims == 0) return false;

    // 3. At least one pool pointer must be a valid address inside this container.
    //    Valid = >= candidateBase and < candidateBase + kMaxAnmbinBytes.
    bool hasValidPoolPtr = false;
    for (int i = 0; i < 6; ++i)
    {
        uint64_t ptr = HdrPoolPtr(hdr, i);
        if (ptr == 0) continue;
        if (ptr >= (uint64_t)candidateBase &&
            ptr <  (uint64_t)candidateBase + kMaxAnmbinBytes)
        {
            hasValidPoolPtr = true;
            break;
        }
    }
    if (!hasValidPoolPtr) return false;

    // 4. Decisive check: PANM magic at (candidateBase + firstAnimOffset + 4).
    //    FlatBuffer layout: [u32 root_offset][file_id "PANM"][...].
    uint8_t magic[4] = {};
    if (!ReadGameMemory(proc, candidateBase + firstAnimOff + 4, magic, 4))
        return false;

    if (memcmp(magic, kPanmMagic, 4) != 0)
        return false;

    return true;
}

// -------------------------------------------------------------
//  ComputeContainerSize
//
//  Iterates each animation pool to find the true end of the data.
//  Each animation is a size-prefixed FlatBuffer: [u32 size][data].
//  Falls back to (pool_ptr + rough_estimate) for categories we can't
//  fully walk (e.g. read failure mid-pool).
//
//  Also accounts for list arrays: their ends are pool_ptr + list_size.
// -------------------------------------------------------------

size_t AnmbinExtractor::ComputeContainerSize(const GameProcessInfo& proc,
                                              uintptr_t              containerBase,
                                              const uint8_t*         hdr,
                                              size_t                 regionSize) const
{
    // Start from firstAnimOffset as a baseline minimum.
    size_t contentEnd = HdrFirstAnimOff(hdr) + 4; // at least past first PANM magic

    for (int i = 0; i < 6; ++i)
    {
        uint32_t animCount = HdrCatCount(hdr, i);
        uint32_t listSize  = HdrListSize(hdr, i);
        uint64_t poolPtr   = HdrPoolPtr(hdr, i);
        uint64_t listPtr   = HdrListPtr(hdr, i);

        // --- List end ---
        if (listPtr >= (uint64_t)containerBase &&
            listPtr <  (uint64_t)containerBase + kMaxAnmbinBytes)
        {
            size_t listOff = (size_t)(listPtr - containerBase);
            size_t listEnd = listOff + listSize;
            if (listEnd > contentEnd && listEnd <= regionSize)
                contentEnd = listEnd;
        }

        // --- Pool end: walk animation size-prefixes ---
        if (animCount == 0) continue;
        if (poolPtr < (uint64_t)containerBase ||
            poolPtr >= (uint64_t)containerBase + kMaxAnmbinBytes)
            continue;

        size_t poolOff = (size_t)(poolPtr - containerBase);
        size_t cur     = poolOff;

        for (uint32_t j = 0; j < animCount; ++j)
        {
            // Read u32 size prefix
            uint32_t animSize = 0;
            if (!ReadGameMemory(proc, containerBase + cur, &animSize, sizeof(animSize)))
                break; // read failure — use what we have so far

            if (animSize == 0 || animSize > kMaxAnimBytes)
                break; // sanity guard

            cur += 4 + animSize;

            // FlatBuffers uses 8-byte file alignment between entries in a pool.
            // If unaligned, skip to next 8-byte boundary.
            cur = (cur + 7u) & ~size_t(7u);

            if (cur > regionSize)
                break;
        }

        if (cur > contentEnd && cur <= regionSize)
            contentEnd = cur;
    }

    // Round up to next page (4 KB) for a clean save boundary.
    constexpr size_t kPage = 4096;
    contentEnd = (contentEnd + kPage - 1) & ~(kPage - 1);
    if (contentEnd > regionSize)
        contentEnd = regionSize;

    return contentEnd;
}

// -------------------------------------------------------------
//  FindAndExtract
// -------------------------------------------------------------

bool AnmbinExtractor::FindAndExtract(const GameProcessInfo& proc,
                                     uintptr_t              motbinAddr,
                                     std::vector<uint8_t>&  outBytes,
                                     std::string&           errorMsg)
{
    if (!proc.valid)
    {
        errorMsg = "Not connected to game.";
        return false;
    }

    // --- Enumerate all committed virtual memory regions ---
    uintptr_t   bestBase     = 0;
    size_t      bestRegionSz = 0;
    uint64_t    bestDist     = UINT64_MAX;

    uint8_t hdrBuf[kHeaderSize] = {};

    MEMORY_BASIC_INFORMATION mbi = {};
    uintptr_t addr = 0x10000; // skip the first null-guard page

    while (VirtualQueryEx(proc.handle,
                          reinterpret_cast<LPCVOID>(addr),
                          &mbi, sizeof(mbi)) == sizeof(mbi))
    {
        uintptr_t regionBase = reinterpret_cast<uintptr_t>(mbi.BaseAddress);
        addr = regionBase + mbi.RegionSize;

        // Only interested in committed, readable, large-enough pages.
        if (mbi.State != MEM_COMMIT)                           continue;
        if (mbi.Protect & PAGE_GUARD)                          continue;
        if (mbi.Protect & PAGE_NOACCESS)                       continue;
        if (!(mbi.Protect & (PAGE_READONLY | PAGE_READWRITE |
                              PAGE_WRITECOPY |
                              PAGE_EXECUTE_READ |
                              PAGE_EXECUTE_READWRITE)))         continue;
        if (mbi.RegionSize < kMinAnmbinBytes)                  continue;
        if (mbi.RegionSize > kMaxAnmbinBytes)                  continue;

        // Read header candidate
        if (!ReadGameMemory(proc, regionBase, hdrBuf, kHeaderSize))
            continue;

        if (!ValidateHeader(proc, regionBase, hdrBuf))
            continue;

        // Score by proximity to motbinAddr (helps choose right player slot)
        uint64_t dist = (regionBase >= motbinAddr)
            ? regionBase - motbinAddr
            : motbinAddr - regionBase;

        if (dist < bestDist)
        {
            bestDist     = dist;
            bestBase     = regionBase;
            bestRegionSz = mbi.RegionSize;
            memcpy(hdrBuf, hdrBuf, kHeaderSize); // keep header for size computation below
            // Re-read header into a separate buffer for ComputeContainerSize
        }
    }

    if (bestBase == 0)
    {
        errorMsg = "anmbin container not found in game memory.\n"
                   "Make sure a character moveset is fully loaded.";
        return false;
    }

    // Re-read header for the winner (in case multiple candidates were found)
    if (!ReadGameMemory(proc, bestBase, hdrBuf, kHeaderSize))
    {
        errorMsg = "Failed to re-read anmbin header.";
        return false;
    }

    // --- Determine actual container size ---
    size_t containerSize = ComputeContainerSize(proc, bestBase, hdrBuf, bestRegionSz);

    if (containerSize < kHeaderSize)
    {
        errorMsg = "anmbin container size computation failed.";
        return false;
    }

    // --- Read the container ---
    outBytes.resize(containerSize);
    if (!ReadGameMemory(proc, bestBase, outBytes.data(), containerSize))
    {
        // Partial read fallback: read header + each pool/list individually
        outBytes.assign(containerSize, 0);
        ReadGameMemory(proc, bestBase, outBytes.data(), kHeaderSize);

        for (int i = 0; i < 6; ++i)
        {
            uint64_t poolPtr = HdrPoolPtr(hdrBuf, i);
            uint32_t cnt     = HdrCatCount(hdrBuf, i);
            if (poolPtr == 0 || cnt == 0) continue;
            if (poolPtr < (uint64_t)bestBase ||
                poolPtr >= (uint64_t)bestBase + containerSize) continue;

            size_t poolOff = (size_t)(poolPtr - bestBase);
            // Estimate pool byte size by walking (best effort — ignore read errors)
            size_t cur = poolOff;
            for (uint32_t j = 0; j < cnt; ++j)
            {
                uint32_t sz = 0;
                if (!ReadGameMemory(proc, bestBase + cur, &sz, 4) || sz == 0 || sz > kMaxAnimBytes)
                    break;
                size_t chunkSize = 4 + sz;
                if (poolOff + chunkSize > containerSize) break;
                ReadGameMemory(proc, bestBase + cur, outBytes.data() + cur, chunkSize);
                cur += chunkSize;
                cur = (cur + 7u) & ~size_t(7u);
            }

            uint64_t listPtr  = HdrListPtr(hdrBuf, i);
            uint32_t listSize = HdrListSize(hdrBuf, i);
            if (listPtr != 0 && listPtr >= (uint64_t)bestBase &&
                listPtr < (uint64_t)bestBase + containerSize)
            {
                size_t listOff = (size_t)(listPtr - bestBase);
                if (listOff + listSize <= containerSize)
                    ReadGameMemory(proc, listPtr, outBytes.data() + listOff, listSize);
            }
        }
    }

    // --- Fix up absolute pointers → container-relative offsets ---
    // Pool pointers at 0x38 (6 entries) and list pointers at 0x68 (6 entries)
    for (int i = 0; i < 12; ++i)
    {
        size_t fieldOff = (i < 6) ? (0x38 + (size_t)i * 8)
                                  : (0x68 + (size_t)(i - 6) * 8);
        uint64_t absPtr = ReadLocal<uint64_t>(outBytes.data(), fieldOff);
        if (absPtr == 0) continue;

        if (absPtr < (uint64_t)bestBase || absPtr >= (uint64_t)bestBase + containerSize)
            continue; // pointer outside our buffer — leave as-is

        uint64_t relOff = absPtr - (uint64_t)bestBase;
        WriteLocal<uint64_t>(outBytes.data(), fieldOff, relOff);
    }

    return true;
}

// -------------------------------------------------------------
//  SaveToFile
// -------------------------------------------------------------

bool AnmbinExtractor::SaveToFile(const std::vector<uint8_t>& bytes,
                                  const std::string&           destFolder,
                                  std::string&                 errorMsg)
{
    std::string path = destFolder;
    if (!path.empty() && path.back() != '\\' && path.back() != '/')
        path += '\\';
    path += "moveset.anmbin";

    HANDLE h = CreateFileA(path.c_str(), GENERIC_WRITE, 0, nullptr,
                           CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE)
    {
        errorMsg = "Cannot create file: " + path;
        return false;
    }

    DWORD written = 0;
    BOOL ok = WriteFile(h, bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr);
    CloseHandle(h);

    if (!ok || written != (DWORD)bytes.size())
    {
        errorMsg = "Write failed: " + path;
        return false;
    }

    return true;
}
