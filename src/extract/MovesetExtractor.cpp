// MovesetExtractor.cpp
// Extracts moveset from Polaris-Win64-Shipping.exe and saves as .motbin
// Reference: OldTool2 (TekkenMovesetExtractor) game_addresses.txt + motbinExport.py
#include "MovesetExtractor.h"
#include "../moveset/LabelDB.h"
#include "../moveset/MotbinSerialize.h"
#include <windows.h>
#include <cstring>
#include <cstdio>
#include <algorithm>

// ─────────────────────────────────────────────────────────────
//  WriteIni — write moveset.ini alongside the extracted motbin
// ─────────────────────────────────────────────────────────────
static void WriteIni(const std::string& folder, const std::string& charaName)
{
    std::string path = folder + "\\moveset.ini";
    FILE* f = nullptr;
    fopen_s(&f, path.c_str(), "w");
    if (!f) return;

    fprintf(f, "[Info]\n");
    fprintf(f, "OriginalCharacter=%s\n", charaName.c_str());
    fprintf(f, "Version=Unknown\n");
    fprintf(f, "DefaultTarget=%s\n",     charaName.c_str());
    fclose(f);
}

// ─────────────────────────────────────────────────────────────
//  TK8 pointer chain constants (game_addresses.txt + Utils.py)
//
//  Player address resolution (getPlayerPointerPath in Utils.py):
//    ptr_list = [0x30 + playerId*8, 0]
//    readPointerPath(baseAddr, ptr_list):
//      step 1: currAddr = *(baseAddr)            + (0x30 + playerId*8)
//      step 2: currAddr = *(currAddr)            + 0
//    → playerAddr = *(  *(moduleBase+0x9B7A950) + 0x30 + playerId*8  )
// ─────────────────────────────────────────────────────────────
static constexpr uintptr_t kP1AddrModuleOffset = 0x9B7A950; // moduleBase + this → first dereference
static constexpr uintptr_t kP1SlotBase         = 0x30;       // add to first deref → slot ptr array
// slot ptr: P1 = +0x00, P2 = +0x08 (playerId * 8)
static constexpr uintptr_t kMotbinOffset        = 0x38C8;    // playerAddr + this → motbin ptr (8B)
static constexpr uintptr_t kCharaIdOffset       = 0x168;     // playerAddr + this → uint32 chara id

// ─────────────────────────────────────────────────────────────
//  motbin header layout (t8_offsetTable, offsets 0x168-0x2B0)
//  Each entry: pointer_field_offset (8B ptr), count_field_offset (8B count)
// ─────────────────────────────────────────────────────────────
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
    // ReactionList has 8 × 8B pushback pointers starting at offset 0x00
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

// ─────────────────────────────────────────────────────────────
//  Helpers
// ─────────────────────────────────────────────────────────────

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

// ─────────────────────────────────────────────────────────────

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

// ─────────────────────────────────────────────────────────────
//  ReadSlot — follow 2-step pointer chain for P1/P2
//
//  Chain (from OldTool2 Utils.py getPlayerPointerPath):
//    step1 = *(moduleBase + 0x9B7A950)
//    slotPtr = step1 + 0x30 + slotIndex*8
//    playerAddr = *(slotPtr)
// ─────────────────────────────────────────────────────────────

bool MovesetExtractor::ReadSlot(int slotIndex, PlayerSlotInfo& slot)
{
    slot = {};
    slot.slotIndex = slotIndex;

    // Step 1: read first-level pointer at moduleBase + 0x9B7A950
    uintptr_t firstPtr = 0;
    if (!ReadGamePointer(m_proc, m_proc.moduleBase + kP1AddrModuleOffset, firstPtr) || firstPtr == 0)
        return false;

    // Step 2: slot pointer = firstPtr + 0x30 + slotIndex*8  →  dereference → playerAddr
    uintptr_t slotPtrAddr = firstPtr + kP1SlotBase + static_cast<uintptr_t>(slotIndex) * 8;
    uintptr_t playerAddr = 0;
    if (!ReadGamePointer(m_proc, slotPtrAddr, playerAddr) || playerAddr == 0)
        return false;

    slot.playerAddr = playerAddr;

    // motbin pointer: playerAddr + 0x38C8 → ptr → motbin base
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

    // Character name from ID — looked up via characterList.txt
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

// ─────────────────────────────────────────────────────────────
//  ReadAndFixupMotbin — raw memory dump + pointer fixup
// ─────────────────────────────────────────────────────────────

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
        // Sometimes the blocks aren't fully contiguous — try reading header
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

    // State-3 raw dump — pointers remain as absolute addresses.
    // moveset.base sidecar written by SaveMotbin so the editor can fixup.
    return true;
}

// ─────────────────────────────────────────────────────────────
//  SaveMotbin
// ─────────────────────────────────────────────────────────────

bool MovesetExtractor::SaveMotbin(const std::vector<uint8_t>& bytes,
                                   const std::string& destFolder,
                                   const std::string& charaName,
                                   uintptr_t motbinBase,
                                   const MotbinNameData* names,
                                   std::string& errorMsg)
{
    // Build path: destFolder/TK8_<charaName>/moveset.motbin
    std::string folder = destFolder;
    if (!folder.empty() && folder.back() != '\\' && folder.back() != '/')
        folder += '\\';
    folder += "TK8_";
    folder += charaName;

    CreateDirectoryA(folder.c_str(), nullptr);

    // Convert state-3 (absolute pointers) → index format for loader + editor
    std::vector<uint8_t> outBytes = ExportLoaderBin(bytes, motbinBase, names);
    if (outBytes.empty())
    {
        errorMsg = "ExportLoaderBin failed (empty result).";
        return false;
    }

    std::string path = folder + "\\moveset.motbin";
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

    // Write moveset.ini — character info sidecar (name DB is built into the editor)
    WriteIni(folder, charaName);

    return true;
}

// ─────────────────────────────────────────────────────────────
//  ExtractToFile — public API
// ─────────────────────────────────────────────────────────────

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

    // ── Build name data ───────────────────────────────────────────────
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
            }
        }
    }

    if (!SaveMotbin(bytes, destFolder, slot.charaName, slot.motbinAddr, &names, errorMsg))
        return false;

    m_statusMsg = "Extracted -> TK8_" + slot.charaName + "  (" +
                  std::to_string(bytes.size()) + " bytes)";
    return true;
}
