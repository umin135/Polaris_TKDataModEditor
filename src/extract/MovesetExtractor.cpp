// MovesetExtractor.cpp
// Extracts moveset from Polaris-Win64-Shipping.exe and saves as .motbin
// Reference: OldTool2 (TekkenMovesetExtractor) game_addresses.txt + motbinExport.py
#include "MovesetExtractor.h"
#include "TkdataExtractor.h"
#include "moveset/data/AnmbinData.h"
#include "moveset/data/AnimNameDB.h"
#include "moveset/labels/LabelDB.h"
#include "moveset/serialize/MotbinSerialize.h"
#include <windows.h>
#include <cstring>
#include <cstdio>
#include <algorithm>

// -------------------------------------------------------------
//  WriteIni -- write moveset.ini alongside the extracted motbin
// -------------------------------------------------------------
static void WriteIni(const std::string& folder, uint32_t charaId, const std::string& charaName,
                     const std::string& gameVersion)
{
    // Resolve character code from characterList.txt (id,name,code); fall back to raw name.
    const char* mapped    = LabelDB::Get().CharaCode(charaId);
    const char* charaCode = mapped ? mapped : charaName.c_str();

    std::string path = folder + "\\moveset.ini";
    FILE* f = nullptr;
    fopen_s(&f, path.c_str(), "w");
    if (!f) return;

    const char* ver = gameVersion.empty() ? "Unknown" : gameVersion.c_str();
    fprintf(f, "[Info]\n");
    fprintf(f, "OriginalCharacter=%s\n", charaCode);
    fprintf(f, "Version=%s\n",           ver);
    fprintf(f, "DefaultTarget=%s\n",     charaCode);
    fclose(f);
}

// -------------------------------------------------------------
//  TK8 pointer chain constants (game_addresses.txt + Utils.py)
//
//  Player address resolution (getPlayerPointerPath in Utils.py):
//    ptr_list = [0x30 + playerId*8, 0]
//    readPointerPath(baseAddr, ptr_list):
//      step 1: currAddr = *(baseAddr)            + (0x30 + playerId*8)
//      step 2: currAddr = *(currAddr)            + 0
//    -> playerAddr = *(  *(moduleBase+0x9B7A950) + 0x30 + playerId*8  )
// -------------------------------------------------------------
static constexpr uintptr_t kP1AddrModuleOffset = 0x9B7A950; // moduleBase + this -> first dereference
static constexpr uintptr_t kP1SlotBase         = 0x30;       // add to first deref -> slot ptr array
// slot ptr: P1 = +0x00, P2 = +0x08 (playerId * 8)
static constexpr uintptr_t kMotbinOffset        = 0x38C8;    // playerAddr + this -> motbin ptr (8B)
static constexpr uintptr_t kCharaIdOffset       = 0x168;     // playerAddr + this -> uint32 chara id

// -------------------------------------------------------------
//  motbin header layout (t8_offsetTable, offsets 0x168-0x2B0)
//  Each entry: pointer_field_offset (8B ptr), count_field_offset (8B count)
// -------------------------------------------------------------
struct BlockDesc {
    size_t   ptrOff;    // offset in motbin header where block ptr is stored
    size_t   cntOff;    // offset in motbin header where block count is stored
    size_t   stride;    // sizeof one element in the block
    int      numPtrs;   // number of valid entries in elemPtrOffsets
    size_t   elemPtrOffsets[8]; // offsets of pointer fields within each element
};

// TK8 blocks in header order (t8_offsetTable)
// struct sizes from t8StructSizes in motbinExport.py
// Format: { ptrOff, cntOff, stride, numPtrs, { ptr_field_offsets... } }
static const BlockDesc kTK8Blocks[] = {
    // reaction_list: ptr@0x168 cnt@0x178 size=0x70
    // ReactionList has 8 ? 8B pushback pointers starting at offset 0x00
    { 0x168, 0x178, 0x70, 8, { 0x00, 0x08, 0x10, 0x18, 0x20, 0x28, 0x30, 0x38 } },

    // requirements: ptr@0x180 cnt@0x188 size=0x14 (no inner pointers)
    { 0x180, 0x188, 0x14, 0, {} },

    // hit_conditions: ptr@0x190 cnt@0x198 size=0x18
    // requirement_addr@0x00, reactions_addr@0x10
    { 0x190, 0x198, 0x18, 2, { 0x00, 0x10 } },

    // projectile: ptr@0x1A0 cnt@0x1A8 size=0xE0
    // hit_condition_addr@0x60, cancel_addr@0x68
    { 0x1A0, 0x1A8, 0xE0, 2, { 0x60, 0x68 } },

    // pushback: ptr@0x1B0 cnt@0x1B8 size=0x10
    // extra_addr@0x08
    { 0x1B0, 0x1B8, 0x10, 1, { 0x08 } },

    // pushback_extradata: ptr@0x1C0 cnt@0x1C8 size=0x02 (no inner pointers)
    { 0x1C0, 0x1C8, 0x02, 0, {} },

    // cancel: ptr@0x1D0 cnt@0x1D8 size=0x28
    // requirement_addr@0x08, extradata_addr@0x10
    { 0x1D0, 0x1D8, 0x28, 2, { 0x08, 0x10 } },

    // group_cancel: ptr@0x1E0 cnt@0x1E8 size=0x28
    { 0x1E0, 0x1E8, 0x28, 2, { 0x08, 0x10 } },

    // cancel_extradata: ptr@0x1F0 cnt@0x1F8 size=0x04 (no inner pointers)
    { 0x1F0, 0x1F8, 0x04, 0, {} },

    // extra_move_properties: ptr@0x200 cnt@0x208 size=0x28
    // requirement_addr@0x10 (TK8 ExtraMoveProperty: type@0x00, id@0x04, value@0x08, pad@0x0C, req_addr@0x10)
    { 0x200, 0x208, 0x28, 1, { 0x10 } },

    // move_start_props: ptr@0x210 cnt@0x218 size=0x20 (OtherMoveProperty)
    // requirement_addr@0x08
    { 0x210, 0x218, 0x20, 1, { 0x08 } },

    // move_end_props: ptr@0x220 cnt@0x228 size=0x20
    { 0x220, 0x228, 0x20, 1, { 0x08 } },

    // movelist: ptr@0x230 cnt@0x238 size=0x448
    // First 8 pointer fields (remaining 3 handled in extra pass below)
    // cancel_addr@0x98, cancel2_addr@0xA0, u1@0xA8, u2@0xB0,
    // u3@0xB8, u4@0xC0, hit_condition_addr@0x110, voicelip_addr@0x130
    { 0x230, 0x238, 0x448, 8, { 0x98, 0xA0, 0xA8, 0xB0, 0xB8, 0xC0, 0x110, 0x130 } },

    // voiceclip: ptr@0x240 cnt@0x248 size=0x0C (no inner pointers)
    { 0x240, 0x248, 0x0C, 0, {} },

    // input_sequence: ptr@0x250 cnt@0x258 size=0x10
    // extradata_addr@0x08
    { 0x250, 0x258, 0x10, 1, { 0x08 } },

    // input_extradata: ptr@0x260 cnt@0x268 size=0x08 (no inner pointers)
    { 0x260, 0x268, 0x08, 0, {} },

    // unknown_parryrelated: ptr@0x270 cnt@0x278 size=0x04 (no inner pointers)
    { 0x270, 0x278, 0x04, 0, {} },

    // throw_extras: ptr@0x280 cnt@0x288 size=0x0C (no inner pointers)
    { 0x280, 0x288, 0x0C, 0, {} },

    // throws: ptr@0x290 cnt@0x298 size=0x10
    // throwextra_addr@0x08
    { 0x290, 0x298, 0x10, 1, { 0x08 } },

    // dialogue_managers: ptr@0x2A0 cnt@0x2A8 size=0x18
    // requirement_addr@0x10
    { 0x2A0, 0x2A8, 0x18, 1, { 0x10 } },
};

// Pointer fields in Move that exceed 8 slots handled separately
static const size_t kMoveExtraPtrOffsets[] = { 0x138, 0x140, 0x148 };

// Header pointer fields (the _ptr fields, all 8B)
static const size_t kHeaderPtrOffsets[] = {
    0x168, 0x180, 0x190, 0x1A0, 0x1B0, 0x1C0, 0x1D0, 0x1E0,
    0x1F0, 0x200, 0x210, 0x220, 0x230, 0x240, 0x250, 0x260,
    0x270, 0x280, 0x290, 0x2A0,
};

static constexpr size_t kMotbinHeaderSize = 0x318; // BASE constant

// -------------------------------------------------------------
//  Helpers
// -------------------------------------------------------------

template<typename T>
static T ReadBuf(const uint8_t* buf, size_t offset)
{
    T v = {};
    memcpy(&v, buf + offset, sizeof(T));
    return v;
}

template<typename T>
static void WriteBuf(uint8_t* buf, size_t offset, T val)
{
    memcpy(buf + offset, &val, sizeof(T));
}

// -------------------------------------------------------------

MovesetExtractor::MovesetExtractor() = default;

bool MovesetExtractor::Connect()
{
    if (m_proc.valid) CloseGameProcess(m_proc);
    if (!FindGameProcess(m_proc))
    {
        m_statusMsg = "Game not running (Polaris-Win64-Shipping.exe not found).";
        return false;
    }
    m_statusMsg = "Connected to game.";
    return true;
}

void MovesetExtractor::Disconnect()
{
    CloseGameProcess(m_proc);
    m_slots[0] = {};
    m_slots[1] = {};
    m_statusMsg = "Disconnected.";
}

// -------------------------------------------------------------
//  ReadSlot -- follow 2-step pointer chain for P1/P2
//
//  Chain (from OldTool2 Utils.py getPlayerPointerPath):
//    step1 = *(moduleBase + 0x9B7A950)
//    slotPtr = step1 + 0x30 + slotIndex*8
//    playerAddr = *(slotPtr)
// -------------------------------------------------------------

bool MovesetExtractor::ReadSlot(int slotIndex, PlayerSlotInfo& slot)
{
    slot = {};
    slot.slotIndex = slotIndex;

    // Step 1: read first-level pointer at moduleBase + 0x9B7A950
    uintptr_t firstPtr = 0;
    if (!ReadGamePointer(m_proc, m_proc.moduleBase + kP1AddrModuleOffset, firstPtr) || firstPtr == 0)
        return false;

    // Step 2: slot pointer = firstPtr + 0x30 + slotIndex*8  ->  dereference -> playerAddr
    uintptr_t slotPtrAddr = firstPtr + kP1SlotBase + static_cast<uintptr_t>(slotIndex) * 8;
    uintptr_t playerAddr = 0;
    if (!ReadGamePointer(m_proc, slotPtrAddr, playerAddr) || playerAddr == 0)
        return false;

    slot.playerAddr = playerAddr;

    // motbin pointer: playerAddr + 0x38C8 -> ptr -> motbin base
    uintptr_t motbinPtrAddr = playerAddr + kMotbinOffset;
    uintptr_t motbinAddr = 0;
    if (!ReadGamePointer(m_proc, motbinPtrAddr, motbinAddr) || motbinAddr == 0)
        return false;

    slot.motbinAddr = motbinAddr;

    // Character ID
    uint32_t charaId = 0;
    ReadGameValue(m_proc, playerAddr + kCharaIdOffset, charaId);
    slot.charaId = charaId;

    // Move count from motbin header
    uint64_t moveCount = 0;
    ReadGameValue(m_proc, motbinAddr + 0x238, moveCount);
    slot.moveCount = static_cast<uint32_t>(moveCount);

    // Validate with TEK signature
    char sig[4] = {};
    if (ReadGameMemory(m_proc, motbinAddr + 0x08, sig, 4) &&
        sig[0] == 'T' && sig[1] == 'E' && sig[2] == 'K')
    {
        slot.valid = true;
    }

    // Character name from ID -- looked up via characterList.txt
    const char* charaNamePtr = LabelDB::Get().CharaName(charaId);
    if (charaNamePtr)
        slot.charaName = charaNamePtr;
    else {
        char nameBuf[32];
        snprintf(nameBuf, sizeof(nameBuf), "chara_%u", charaId);
        slot.charaName = nameBuf;
    }

    return slot.valid;
}

void MovesetExtractor::RefreshSlots()
{
    if (!m_proc.valid) return;
    ReadSlot(0, m_slots[0]);
    ReadSlot(1, m_slots[1]);
}

// -------------------------------------------------------------
//  ReadAndFixupMotbin -- raw memory dump + pointer fixup
// -------------------------------------------------------------

bool MovesetExtractor::ReadMotbin(uintptr_t motbinAddr,
                                   std::vector<uint8_t>& outBytes,
                                   std::string& errorMsg)
{
    // Step 1: Read just the header to get all block ptrs/counts
    std::vector<uint8_t> hdr(kMotbinHeaderSize);
    if (!ReadGameMemory(m_proc, motbinAddr, hdr.data(), kMotbinHeaderSize))
    {
        errorMsg = "Failed to read motbin header.";
        return false;
    }

    // Validate TEK signature
    if (hdr.size() > 0x0C &&
        (hdr[0x08] != 'T' || hdr[0x09] != 'E' || hdr[0x0A] != 'K'))
    {
        errorMsg = "Invalid motbin signature (expected TEK).";
        return false;
    }

    // Step 2: Compute total data size from all block extents
    static constexpr size_t kMaxMotbinSize = 64ULL * 1024 * 1024; // 64 MB hard cap

    uintptr_t maxEnd = motbinAddr + kMotbinHeaderSize;

    for (const BlockDesc& bd : kTK8Blocks)
    {
        if (bd.ptrOff + 8 > hdr.size() || bd.cntOff + 8 > hdr.size())
            continue;

        uintptr_t blockPtr   = ReadBuf<uint64_t>(hdr.data(), bd.ptrOff);
        uint64_t  blockCount = ReadBuf<uint64_t>(hdr.data(), bd.cntOff);

        if (blockPtr == 0 || blockCount == 0) continue;

        // Block must be within a sane distance from header
        if (blockPtr < motbinAddr + kMotbinHeaderSize) continue;
        if (blockPtr > motbinAddr + kMaxMotbinSize)    continue;
        if (blockCount > 100000) continue; // sanity cap on element count

        uintptr_t blockEnd = blockPtr + blockCount * bd.stride;
        if (blockEnd > motbinAddr + kMaxMotbinSize) continue; // overflow guard
        if (blockEnd > maxEnd) maxEnd = blockEnd;
    }

    size_t totalSize = maxEnd - motbinAddr;

    if (totalSize < kMotbinHeaderSize || totalSize > kMaxMotbinSize)
    {
        errorMsg = "Motbin size out of range (computed: " +
                   std::to_string(totalSize) + " bytes).";
        return false;
    }

    // Step 3: Read the full block
    outBytes.resize(totalSize);
    if (!ReadGameMemory(m_proc, motbinAddr, outBytes.data(), totalSize))
    {
        // Sometimes the blocks aren't fully contiguous -- try reading header
        // separately and each block individually
        outBytes.assign(totalSize, 0);
        ReadGameMemory(m_proc, motbinAddr, outBytes.data(), kMotbinHeaderSize);

        for (const BlockDesc& bd : kTK8Blocks)
        {
            if (bd.ptrOff + 8 > kMotbinHeaderSize) continue;
            uintptr_t blockPtr   = ReadBuf<uint64_t>(outBytes.data(), bd.ptrOff);
            uint64_t  blockCount = ReadBuf<uint64_t>(outBytes.data(), bd.cntOff);
            if (blockPtr == 0 || blockCount == 0) continue;
            if (blockPtr < motbinAddr || blockPtr >= motbinAddr + totalSize) continue;

            size_t   blockOff  = static_cast<size_t>(blockPtr - motbinAddr);
            size_t   blockSize = static_cast<size_t>(blockCount * bd.stride);
            if (blockOff + blockSize > totalSize) continue;

            ReadGameMemory(m_proc, blockPtr, outBytes.data() + blockOff, blockSize);
        }
    }

    // State-3 raw dump -- pointers remain as absolute addresses.
    // moveset.base sidecar written by SaveMotbin so the editor can fixup.
    return true;
}

// -------------------------------------------------------------
//  SaveMotbin
// -------------------------------------------------------------

bool MovesetExtractor::SaveMotbin(const std::vector<uint8_t>& bytes,
                                   const std::string& destFolder,
                                   uint32_t charaId,
                                   const std::string& charaName,
                                   uintptr_t motbinBase,
                                   const MotbinNameData* names,
                                   const std::string& gameVersion,
                                   std::string& errorMsg)
{
    // Build path: destFolder/TK8_<charaName>/moveset.motbin
    std::string folder = destFolder;
    if (!folder.empty() && folder.back() != '\\' && folder.back() != '/')
        folder += '\\';
    folder += "TK8_";
    folder += charaName;

    CreateDirectoryA(folder.c_str(), nullptr);

    // Convert state-3 (absolute pointers) -> index format for loader + editor
    std::vector<uint8_t> outBytes = ExportLoaderBin(bytes, motbinBase, names);
    if (outBytes.empty())
    {
        errorMsg = "ExportLoaderBin failed (empty result).";
        return false;
    }

    // Use charaCode (e.g. "grf") as filename, fall back to charaName
    const char* cCode   = LabelDB::Get().CharaCode(charaId);
    const char* codeStr = cCode ? cCode : charaName.c_str();
    std::string path = folder + "\\" + codeStr + ".motbin";
    HANDLE h = CreateFileA(path.c_str(), GENERIC_WRITE, 0, nullptr,
                           CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE)
    {
        errorMsg = "Cannot create file: " + path;
        return false;
    }
    DWORD written = 0;
    BOOL ok = WriteFile(h, outBytes.data(), static_cast<DWORD>(outBytes.size()), &written, nullptr);
    CloseHandle(h);
    if (!ok || written != outBytes.size())
    {
        errorMsg = "Write failed: " + path;
        return false;
    }

    // Write moveset.ini -- character info sidecar (name DB is built into the editor)
    WriteIni(folder, charaId, charaName, gameVersion);

    return true;
}

// -------------------------------------------------------------
//  ResolveTkdataPath
//  Derives tkdata.bin path from the moveset root directory.
//  moveset root:  ...\TEKKEN 8\Polaris\Content\Binary\Mods\Movesets
//  tkdata.bin:    ...\TEKKEN 8\Polaris\Content\Binary\pak\tkdata.bin
// -------------------------------------------------------------
static std::string ResolveTkdataPath(const std::string& movesetRoot)
{
    if (movesetRoot.empty()) return {};
    std::string combined = movesetRoot + "\\..\\..\\pak\\tkdata.bin";
    char resolved[MAX_PATH] = {};
    if (!GetFullPathNameA(combined.c_str(), MAX_PATH, resolved, nullptr))
        return combined;
    return resolved;
}

// Save a raw byte buffer to a file, creating or overwriting it.
static bool SaveBytes(const std::string& path,
                      const std::vector<uint8_t>& data)
{
    HANDLE h = CreateFileA(path.c_str(), GENERIC_WRITE, 0, nullptr,
                           CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    DWORD written = 0;
    BOOL ok = WriteFile(h, data.data(), static_cast<DWORD>(data.size()), &written, nullptr);
    CloseHandle(h);
    return ok && written == static_cast<DWORD>(data.size());
}

// Extract individual PANM animation files from moveset.anmbin (tkdata.bin file format).
//
// File format: 0x04 = u32[6] pool counts per category
//              0x38 = u64[6] pool entry metadata array offsets
//              Each entry (stride 0x38): {animKey u64, animDataPtr u64, extra u8[0x28]}
//              animDataPtr == 0  →  animation lives in com.anmbin (skip)
//              animDataPtr != 0  →  PANM FlatBuffer at that file offset
//              Size = next_entry.animDataPtr - this.animDataPtr (sorted by pointer)
//              PANM magic: data[4:8] == "PANM" (FlatBuffer file identifier)
//
// Saved as: charFolder\anim\<cat_folder>\anim_<poolIdx>.<ext>
// Returns a short diagnostic string.
static std::string ExtractAnimFilesFromAnmbin(const std::string& charFolder,
                                               const char*        charaCode)
{
    std::string anmbinPath = charFolder + "\\moveset.anmbin";

    FILE* f = nullptr;
    if (fopen_s(&f, anmbinPath.c_str(), "rb") != 0 || !f)
        return " | anim: no moveset.anmbin";

    fseek(f, 0, SEEK_END);
    long fileSize = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (fileSize < 0x98) { fclose(f); return " | anim: anmbin too small"; }

    std::vector<uint8_t> bytes(static_cast<size_t>(fileSize));
    fread(bytes.data(), 1, bytes.size(), f);
    fclose(f);

    auto rdU32 = [&](size_t off) -> uint32_t {
        if (off + 4 > bytes.size()) return 0;
        uint32_t v; memcpy(&v, bytes.data() + off, 4); return v;
    };
    auto rdU64 = [&](size_t off) -> uint64_t {
        if (off + 8 > bytes.size()) return 0;
        uint64_t v; memcpy(&v, bytes.data() + off, 8); return v;
    };

    // Collect all pool entries that have local PANM data (animDataPtr != 0)
    struct PanmEntry { uint64_t dataPtr; uint64_t animKey; int cat; int poolIdx; };
    std::vector<PanmEntry> entries;

    for (int cat = 0; cat < 6; ++cat)
    {
        uint32_t count   = rdU32(0x04 + cat * 4);
        uint64_t listOff = rdU64(0x38 + cat * 8);  // pool entry metadata array
        if (count == 0 || listOff == 0) continue;
        if (listOff + (uint64_t)count * 0x38 > (uint64_t)bytes.size()) continue;

        for (uint32_t j = 0; j < count; ++j)
        {
            size_t   entOff      = static_cast<size_t>(listOff) + j * 0x38;
            uint64_t animKey     = rdU64(entOff);
            uint64_t animDataPtr = rdU64(entOff + 8);

            // animDataPtr == 0 → animation is in com.anmbin, skip
            if (animDataPtr == 0 || animDataPtr >= (uint64_t)bytes.size()) continue;

            entries.push_back({ animDataPtr, animKey, cat, (int)j });
        }
    }

    if (entries.empty()) return " | anim: 0 local (all in com.anmbin?)";

    // Sort by animDataPtr; compute each PANM blob's size from adjacent pointers
    std::sort(entries.begin(), entries.end(),
              [](const PanmEntry& a, const PanmEntry& b){ return a.dataPtr < b.dataPtr; });

    // Create output directories
    std::string animRoot = charFolder + "\\anim";
    CreateDirectoryA(animRoot.c_str(), nullptr);
    for (int cat = 0; cat < 6; ++cat)
        CreateDirectoryA((animRoot + "\\" + AnmbinCategoryFolder(cat)).c_str(), nullptr);

    int totalSaved = 0;
    for (int i = 0; i < (int)entries.size(); ++i)
    {
        uint64_t dataStart = entries[i].dataPtr;
        uint64_t dataEnd   = (i + 1 < (int)entries.size())
                           ? entries[i + 1].dataPtr
                           : static_cast<uint64_t>(bytes.size());

        if (dataEnd <= dataStart || dataStart >= (uint64_t)bytes.size()) continue;
        size_t dataSize = static_cast<size_t>(dataEnd - dataStart);

        const uint8_t* panm = bytes.data() + static_cast<size_t>(dataStart);

        // Validate PANM magic at bytes [4:8] of the FlatBuffer
        if (dataSize >= 8 && memcmp(panm + 4, "PANM", 4) == 0)
        {
            int cat = entries[i].cat;
            int j   = entries[i].poolIdx;
            char fname[64];
            if (charaCode && charaCode[0])
                snprintf(fname, sizeof(fname), "anim_%s_%d%s", charaCode, j, AnmbinCategoryExt(cat));
            else
                snprintf(fname, sizeof(fname), "anim_%d%s", j, AnmbinCategoryExt(cat));
            std::string catDir = animRoot + "\\" + AnmbinCategoryFolder(cat);
            SaveBytes(catDir + "\\" + fname,
                      std::vector<uint8_t>(panm, panm + dataSize));
            ++totalSaved;
        }
    }

    if (totalSaved == 0) return " | anim: no PANM found";

    // Delete the anim/ folder: animations are now managed via AnimationManagerWindow.
    // Users can re-extract individual files at any time using the "Extract" / "Extract All" buttons.
    {
        // Recursive delete: clear each category subfolder, then the root
        for (int cat = 0; cat < 6; ++cat)
        {
            std::string catDir = animRoot + "\\" + AnmbinCategoryFolder(cat);
            std::string delPat = catDir + "\\*";
            WIN32_FIND_DATAA fd = {};
            HANDLE h = FindFirstFileA(delPat.c_str(), &fd);
            if (h != INVALID_HANDLE_VALUE)
            {
                do {
                    if (strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..") == 0) continue;
                    DeleteFileA((catDir + "\\" + fd.cFileName).c_str());
                } while (FindNextFileA(h, &fd));
                FindClose(h);
            }
            RemoveDirectoryA(catDir.c_str());
        }
        RemoveDirectoryA(animRoot.c_str());
    }

    return " | anim: +" + std::to_string(totalSaved) + " (cleaned)";
}

// -------------------------------------------------------------
//  BakeCharAnmbin
//
//  Merges {charaCode}.anmbin and com.anmbin into a single
//  moveset.anmbin:
//    1. For each pool entry in {charaCode}.anmbin where animDataPtr == 0
//       (com.anmbin reference), find the matching PANM blob in com.anmbin
//       by animKey (same category, same lower-32-bit hash), append it to
//       the buffer and patch animDataPtr.
//    2. Set characterFlags (+0x18..+0x24) to 0xFFFFFFFF for every entry
//       in all 6 categories.
//    3. Save the patched buffer as moveset.anmbin.
//    4. Copy {charaCode}.motbin → moveset.motbin.
//    5. Build anim_names.json via AnimNameDB::BuildAndSave.
//    6. Delete {charaCode}.anmbin, com.anmbin, {charaCode}.motbin.
//
//  Returns a short diagnostic string (for status bar).
// -------------------------------------------------------------
static std::string BakeCharAnmbin(const std::string&           charFolder,
                                   const char*                  charaCode,
                                   const std::vector<uint32_t>& motbinAnimKeys)
{
    std::string base = charFolder;
    if (!base.empty() && base.back() != '\\' && base.back() != '/') base += '\\';

    std::string charAnmbinPath = base + charaCode + ".anmbin";
    std::string comAnmbinPath  = base + "com.anmbin";
    std::string charMotbinPath = base + charaCode + ".motbin";
    std::string outAnmbinPath  = base + "moveset.anmbin";
    std::string outMotbinPath  = base + "moveset.motbin";

    // File loader helper
    auto loadFile = [](const std::string& path, std::vector<uint8_t>& out) -> bool {
        FILE* f = nullptr;
        if (fopen_s(&f, path.c_str(), "rb") != 0 || !f) return false;
        fseek(f, 0, SEEK_END);
        long sz = ftell(f); fseek(f, 0, SEEK_SET);
        if (sz < 0x98) { fclose(f); return false; }
        out.resize(static_cast<size_t>(sz));
        fread(out.data(), 1, out.size(), f);
        fclose(f);
        return true;
    };

    std::vector<uint8_t> bytes;
    if (!loadFile(charAnmbinPath, bytes))
        return " | bake: cannot load " + std::string(charaCode) + ".anmbin";

    std::vector<uint8_t> comBytes;
    const bool hasCom = loadFile(comAnmbinPath, comBytes);

    // Read helpers (explicit buffer parameter — safe across vector reallocations)
    auto rdU32 = [](const std::vector<uint8_t>& buf, size_t off) -> uint32_t {
        if (off + 4 > buf.size()) return 0;
        uint32_t v; memcpy(&v, buf.data() + off, 4); return v;
    };
    auto rdU64 = [](const std::vector<uint8_t>& buf, size_t off) -> uint64_t {
        if (off + 8 > buf.size()) return 0;
        uint64_t v; memcpy(&v, buf.data() + off, 8); return v;
    };

    int embedded = 0;

    if (hasCom)
    {
        // --- Build com PANM lookup: cat -> (hash -> PANM bytes) ---
        // Collect every com.anmbin pool entry that has actual data (animDataPtr != 0),
        // sort globally by animDataPtr to compute blob sizes from pointer differences.
        struct ComEntry { uint64_t dataPtr; uint32_t hash; int cat; };
        std::vector<ComEntry> comAll;

        for (int cat = 0; cat < 6; ++cat)
        {
            uint32_t count   = rdU32(comBytes, 0x04 + (size_t)cat * 4);
            uint64_t listOff = rdU64(comBytes, 0x38 + (size_t)cat * 8);
            if (count == 0 || listOff == 0) continue;
            if (listOff + (uint64_t)count * 0x38 > comBytes.size()) continue;
            for (uint32_t j = 0; j < count; ++j)
            {
                size_t   entOff  = static_cast<size_t>(listOff) + j * 0x38;
                uint64_t animKey = rdU64(comBytes, entOff);
                uint64_t dataPtr = rdU64(comBytes, entOff + 8);
                if (dataPtr == 0 || dataPtr >= comBytes.size()) continue;
                comAll.push_back({ dataPtr, (uint32_t)(animKey & 0xFFFFFFFFu), cat });
            }
        }

        std::sort(comAll.begin(), comAll.end(),
                  [](const ComEntry& a, const ComEntry& b){ return a.dataPtr < b.dataPtr; });

        // Map: cat -> hash -> PANM bytes
        std::unordered_map<uint32_t, std::vector<uint8_t>> comPanm[6];
        for (int i = 0; i < (int)comAll.size(); ++i)
        {
            uint64_t start = comAll[i].dataPtr;
            uint64_t end   = (i + 1 < (int)comAll.size())
                           ? comAll[i + 1].dataPtr
                           : static_cast<uint64_t>(comBytes.size());
            if (end <= start || start >= comBytes.size()) continue;
            if (end > comBytes.size()) end = comBytes.size();
            size_t sz = static_cast<size_t>(end - start);
            comPanm[comAll[i].cat][comAll[i].hash] =
                std::vector<uint8_t>(comBytes.data() + static_cast<size_t>(start),
                                      comBytes.data() + static_cast<size_t>(start) + sz);
        }

        // --- Embed com refs into bytes[] ---
        // For each pool entry with animDataPtr == 0: look up PANM in comPanm
        // by the same category and hash, append blob, patch animDataPtr.
        for (int cat = 0; cat < 6; ++cat)
        {
            uint32_t count   = rdU32(bytes, 0x04 + (size_t)cat * 4);
            uint64_t listOff = rdU64(bytes, 0x38 + (size_t)cat * 8);
            if (count == 0 || listOff == 0) continue;
            if (listOff + (uint64_t)count * 0x38 > bytes.size()) continue;
            for (uint32_t j = 0; j < count; ++j)
            {
                size_t   entOff  = static_cast<size_t>(listOff) + j * 0x38;
                uint64_t dataPtr = rdU64(bytes, entOff + 8);
                if (dataPtr != 0) continue; // already has embedded data
                uint64_t animKey = rdU64(bytes, entOff);
                uint32_t hash    = (uint32_t)(animKey & 0xFFFFFFFFu);
                auto it = comPanm[cat].find(hash);
                if (it == comPanm[cat].end()) continue; // not in com.anmbin
                // Append PANM blob and patch animDataPtr (bytes[] may reallocate)
                uint64_t newOff = static_cast<uint64_t>(bytes.size());
                bytes.insert(bytes.end(), it->second.begin(), it->second.end());
                memcpy(bytes.data() + entOff + 8, &newOff, 8);
                ++embedded;
            }
        }
    }

    // --- Set characterFlags to 0xFFFFFFFF for all entries (all cats) ---
    // Pool entry layout: +0x18..+0x24 = four u32 characterFlags fields.
    static const size_t kFlagOff[4] = { 0x18, 0x1C, 0x20, 0x24 };
    for (int cat = 0; cat < 6; ++cat)
    {
        uint32_t count   = rdU32(bytes, 0x04 + (size_t)cat * 4);
        uint64_t listOff = rdU64(bytes, 0x38 + (size_t)cat * 8);
        if (count == 0 || listOff == 0) continue;
        if (listOff + (uint64_t)count * 0x38 > bytes.size()) continue;
        for (uint32_t j = 0; j < count; ++j)
        {
            size_t entOff = static_cast<size_t>(listOff) + j * 0x38;
            if (entOff + 0x28 > bytes.size()) break;
            for (size_t fo : kFlagOff) {
                uint32_t ff = 0xFFFFFFFFu;
                memcpy(bytes.data() + entOff + fo, &ff, 4);
            }
        }
    }

    // --- Save moveset.anmbin ---
    if (!SaveBytes(outAnmbinPath, bytes))
        return " | bake: cannot save moveset.anmbin";

    // --- Copy {charaCode}.motbin → moveset.motbin ---
    {
        std::vector<uint8_t> motBytes;
        if (!loadFile(charMotbinPath, motBytes) || !SaveBytes(outMotbinPath, motBytes))
            return " | bake: cannot save moveset.motbin";
    }

    // --- Build anim_names.json ---
    if (!motbinAnimKeys.empty())
    {
        AnmbinData newAnmbin = LoadAnmbin(outAnmbinPath);
        if (newAnmbin.loaded)
        {
            AnimNameDB nameDB;
            nameDB.BuildAndSave(charFolder, newAnmbin, motbinAnimKeys, charaCode);
        }
    }

    // --- Delete intermediate files ---
    DeleteFileA(charAnmbinPath.c_str());
    DeleteFileA(comAnmbinPath.c_str());
    DeleteFileA(charMotbinPath.c_str());

    return " | bake: +" + std::to_string(embedded) + " com embedded";
}

// Extract anmbin / stllstb / mvl for a character from tkdata.bin.
// Returns a short diagnostic string for display in the status bar.
// Non-fatal: missing or unrecognised files are skipped gracefully.
static std::string TryExtractTkdataFiles(const std::string& movesetRoot,
                                          const std::string& charFolder,
                                          const char*        charaCode)
{
    if (!charaCode || charaCode[0] == '\0')
        return " | tkdata: no chara code";

    std::string tkdataPath = ResolveTkdataPath(movesetRoot);
    if (tkdataPath.empty())
        return " | tkdata: path empty";

    TkdataExtractor tkext;
    std::string tkErr;
    if (!tkext.Open(tkdataPath, tkErr))
        return " | tkdata: " + tkErr;

    struct FileSpec { const char* dir; const char* ext; const char* outName; };
    static const FileSpec kFiles[] = {
        { "bin",      ".stllstb", "moveset.stllstb" },
        { "movelist", ".mvl",     "moveset.mvl"     },
    };

    std::string extracted;
    std::string missing;

    // Extract char-specific anmbin as {charaCode}.anmbin
    {
        std::string vfsPath = std::string("mothead/bin/") + charaCode + ".anmbin";
        std::vector<uint8_t> data = tkext.ExtractFile(vfsPath);
        if (data.empty()) {
            missing += ".anmbin";
        } else {
            std::string outPath = charFolder + "\\" + charaCode + ".anmbin";
            if (SaveBytes(outPath, data)) extracted += ".anmbin";
            else missing += ".anmbin";
        }
    }

    // Extract com.anmbin (always needed for bake; non-fatal if missing)
    {
        std::vector<uint8_t> comData = tkext.ExtractFile("mothead/bin/com.anmbin");
        if (!comData.empty())
            SaveBytes(charFolder + "\\com.anmbin", comData);
    }

    for (const auto& f : kFiles) {
        std::string vfsPath = std::string("mothead/") + f.dir + "/" + charaCode + f.ext;
        std::vector<uint8_t> data = tkext.ExtractFile(vfsPath);
        if (data.empty()) {
            if (!missing.empty()) missing += ',';
            missing += f.ext;
            continue;
        }
        std::string outPath = charFolder + "\\" + f.outName;
        if (SaveBytes(outPath, data)) {
            if (!extracted.empty()) extracted += ',';
            extracted += f.ext;
        }
    }

    std::string result;
    if (!extracted.empty()) result += " | +tkdata(" + extracted + ")";
    if (!missing.empty())   result += " | missing(" + missing + ")";
    if (result.empty())     result =  " | tkdata: no files extracted";
    return result;
}

// -------------------------------------------------------------
//  ReadGameVersion -- AoB scan for version string
//
//  Pattern: 4C 8D 2D ?? ?? ?? ?? 49 8B CD E8 ...
//  The 4-byte signed offset at +3 is a RIP-relative displacement
//  (RIP = match address + 7 = start of next instruction).
//  The resolved address holds a null-terminated ASCII version string
//  such as "3.00.01(479287)FL12_2SM6(6.7)".
//  We extract only the prefix before the first '('.
// -------------------------------------------------------------
static constexpr char kVersionAob[] =
    "4C 8D 2D ?? ?? ?? ?? 49 8B CD E8 ?? ?? ?? ?? 41 B9 ?? ?? ?? ?? 4C 8D 05";

std::string MovesetExtractor::ReadGameVersion()
{
    if (!m_proc.valid) return {};

    uintptr_t match = AobScan(m_proc, kVersionAob,
                               m_proc.moduleBase,
                               m_proc.moduleBase + m_proc.moduleSize);
    if (!match) return {};

    // Read the 4-byte signed RIP-relative offset at match+3
    int32_t rel32 = 0;
    if (!ReadGameMemory(m_proc, match + 3, &rel32, sizeof(rel32))) return {};

    // RIP points to the byte after the 7-byte LEA instruction
    uintptr_t strAddr = match + 7 + static_cast<uintptr_t>(static_cast<intptr_t>(rel32));

    // Read up to 64 bytes of the version string
    char buf[64] = {};
    if (!ReadGameMemory(m_proc, strAddr, buf, sizeof(buf) - 1)) return {};
    buf[sizeof(buf) - 1] = '\0';

    // Extract prefix before first '('
    std::string full(buf);
    auto paren = full.find('(');
    if (paren != std::string::npos)
        return full.substr(0, paren);
    return full;
}

// -------------------------------------------------------------
//  ExtractToFile -- public API
// -------------------------------------------------------------

bool MovesetExtractor::ExtractToFile(int slotIndex,
                                      const std::string& destFolder,
                                      std::string& errorMsg)
{
    if (!m_proc.valid)
    {
        errorMsg = "Not connected to game.";
        return false;
    }

    PlayerSlotInfo& slot = m_slots[slotIndex];
    if (!slot.valid)
    {
        errorMsg = "Slot " + std::to_string(slotIndex + 1) + " has no valid moveset.";
        return false;
    }

    std::vector<uint8_t> bytes;
    if (!ReadMotbin(slot.motbinAddr, bytes, errorMsg))
        return false;

    // -- Build name data -----------------------------------------------
    // motbinAnimKeys: moves[i].anim_key — collected here, passed to BakeCharAnmbin
    // for AnimNameDB::BuildAndSave after the bake.
    std::vector<uint32_t> motbinAnimKeys;
    std::vector<uint32_t> motbinNameKeys; // for Original_Moves.json

    // Header string fields (char_name_addr at 0x10/0x18/0x20/0x28) are
    // "no longer used" in TK8 and always point to "?" in game memory.
    // We generate deterministic header strings from the character ID and
    // compile_date (header 0x04) to reproduce the correct virtual string
    // block offsets matching the reference file format:
    //   charName  = characterName + "_n"  (e.g. "JIN_n" = 5 chars)
    //   creator   = "Polaris8"            (8 chars)
    //   date      = "YYYYMMDD.000000"     (15 chars from compile_date)
    //   fullDate  = "YYYYMMDD.000000 00:00:00.000" (28 chars)
    //
    // Move name strings come from name_keys.json (and supplement_name_keys.json
    // for missing entries) via LabelDB::GetMoveName(name_key).
    // Anim strings come from anim_keys.json via LabelDB::GetAnimName(anim_key).
    MotbinNameData names;
    {
        // Build header strings from compile_date (header 0x04)
        uint32_t compileDate = ReadBuf<uint32_t>(bytes.data(), 0x04);
        char dateBuf[32] = {};
        char fullDateBuf[32] = {};
        snprintf(dateBuf,     sizeof(dateBuf),     "%08u.000000",                compileDate);
        snprintf(fullDateBuf, sizeof(fullDateBuf), "%08u.000000 00:00:00.000",   compileDate);

        // charName = CharaName (from characterList.txt lookup) + "_n"
        const char* cn = LabelDB::Get().CharaName(slot.charaId);
        names.charName    = std::string(cn ? cn : slot.charaName) + "_n";
        names.charCreator = "Polaris8";
        names.date        = std::string(dateBuf);
        names.fullDate    = std::string(fullDateBuf);

        uint64_t moveBlockAbs = ReadBuf<uint64_t>(bytes.data(), 0x230);
        uint64_t moveCount    = ReadBuf<uint64_t>(bytes.data(), 0x238);
        if (moveBlockAbs >= static_cast<uint64_t>(slot.motbinAddr)) {
            size_t moveOff = static_cast<size_t>(moveBlockAbs - slot.motbinAddr);
            names.moves.reserve(static_cast<size_t>(moveCount));
            for (uint64_t mi = 0; mi < moveCount; ++mi) {
                size_t e = moveOff + static_cast<size_t>(mi * 0x448);
                if (e + 0x40 > bytes.size()) break;
                uint32_t nk = DecryptMotbinMoveKey(bytes.data() + e, 0x00);
                uint32_t ak = DecryptMotbinMoveKey(bytes.data() + e, 0x20);
                MotbinNameData::MoveNameEntry entry;
                const char* nStr = LabelDB::Get().GetMoveName(nk);
                const char* aStr = LabelDB::Get().GetAnimName(ak);
                entry.name = nStr ? nStr : "";
                entry.anim = aStr ? aStr : "";
                names.moves.push_back(std::move(entry));
                motbinAnimKeys.push_back(ak);
                motbinNameKeys.push_back(nk);
            }
        }
    }

    std::string gameVersion = ReadGameVersion();

    if (!SaveMotbin(bytes, destFolder, slot.charaId, slot.charaName, slot.motbinAddr, &names, gameVersion, errorMsg))
        return false;

    // Build charFolder path (used by both tkdata and bake)
    std::string charFolder = destFolder;
    if (!charFolder.empty() && charFolder.back() != '\\' && charFolder.back() != '/')
        charFolder += '\\';
    charFolder += "TK8_";
    charFolder += slot.charaName;

    // Write .tkedit/Original_Moves.json — records which name_keys belong to original game moves.
    // LoadMotbin reads this to set ParsedMove::isNew=false on matching moves; all others are new.
    {
        std::string tkeditDir = charFolder + "\\.tkedit";
        CreateDirectoryA(tkeditDir.c_str(), nullptr);
        std::string jsonPath = tkeditDir + "\\Original_Moves.json";
        FILE* jf = nullptr;
        if (fopen_s(&jf, jsonPath.c_str(), "w") == 0 && jf) {
            fprintf(jf, "{\"name_keys\":[");
            for (size_t i = 0; i < motbinNameKeys.size(); ++i) {
                if (i > 0) fprintf(jf, ",");
                fprintf(jf, "%u", motbinNameKeys[i]);
            }
            fprintf(jf, "]}");
            fclose(jf);
        }
    }

    // Extract companion files from tkdata.bin: {charaCode}.anmbin, com.anmbin, stllstb, mvl
    const char* charaCode = LabelDB::Get().CharaCode(slot.charaId);
    std::string tkInfo = TryExtractTkdataFiles(destFolder, charFolder, charaCode);

    // Bake: merge com.anmbin refs into {charaCode}.anmbin, set characterFlags,
    // save moveset.anmbin + moveset.motbin, build anim_names.json, delete intermediates.
    std::string bakeInfo;
    if (charaCode)
        bakeInfo = BakeCharAnmbin(charFolder, charaCode, motbinAnimKeys);

    // Extract individual PANM animation files from baked moveset.anmbin.
    // After bake, all entries have animDataPtr != 0 (com refs are now embedded).
    std::string animInfo = ExtractAnimFilesFromAnmbin(charFolder, charaCode);

    m_statusMsg = "Extracted -> TK8_" + slot.charaName + "  (" +
                  std::to_string(bytes.size()) + " bytes)" + tkInfo + bakeInfo + animInfo;
    return true;
}
