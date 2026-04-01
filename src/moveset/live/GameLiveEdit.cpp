// GameLiveEdit.cpp
// Live memory interaction with Tekken 8 (Polaris-Win64-Shipping.exe).
// Player base address and motbin offset are resolved via AoB scan at runtime
// and cached for the lifetime of the process so subsequent calls are instant.
#include "moveset/live/GameLiveEdit.h"
#include "extract/GameProcess.h"
#include <cstdint>

namespace {

// AoB patterns from OldTool2/Utils.py
// Scans the module code section to find the pointer and offset dynamically.

static constexpr const char* kPatternP1 =
    "4C 89 35 ?? ?? ?? ?? "
    "41 88 5E 28 "
    "66 41 89 9E 88 00 00 00 "
    "E8 ?? ?? ?? ?? "
    "41 88 86 8A 00 00 00";

static constexpr const char* kPatternMotbin =
    "48 89 91 ?? ?? ?? 00 "
    "4C 8B D9 "
    "48 89 91 ?? ?? ?? 00 "
    "48 8B DA "
    "48 89 91 ?? ?? ?? 00 "
    "48 89 91 ?? ?? ?? 00 "
    "0F B7 02 "
    "89 81 ?? ?? ?? 00 "
    "B8 01 80 00 80";

// Fixed offsets that don't change between game versions
static constexpr uintptr_t kMovelistOffset   = 0x230;
static constexpr uintptr_t kMoveSize         = 0x448;
static constexpr uintptr_t kCurrMoveOffset   = 0x550;
static constexpr uintptr_t kNextMoveOffset   = 0x27A0;
static constexpr uintptr_t kFrameTimerOffset = 0x390;

// Cached scan results (0 = not yet resolved)
static uintptr_t s_p1BaseOffset  = 0; // module-relative: moduleBase + this -> root ptr
static uintptr_t s_motbinOffset  = 0; // player struct offset -> moveset ptr

// Scan for p1 base offset (module-relative).
// Pattern contains a RIP-relative MOV instruction; the 32-bit displacement
// at byte+3 encodes: effective_addr = (match + 7) + disp32.
// p1BaseOffset = effective_addr - moduleBase.
static bool ScanP1BaseOffset(const GameProcessInfo& gp, uintptr_t& outOffset)
{
    uintptr_t base  = gp.moduleBase;
    uintptr_t match = AobScan(gp, kPatternP1,
                               base + 0x5A00000,
                               base + 0x6F00000);
    if (!match) return false;

    int32_t disp32 = 0;
    if (!ReadGameValue(gp, match + 3, disp32)) return false;

    outOffset = (uintptr_t)((intptr_t)(match + 7) + disp32) - base;
    return true;
}

// Scan for motbin offset.
// The 4-byte value at byte+3 of the pattern is the offset itself.
static bool ScanMotbinOffset(const GameProcessInfo& gp, uintptr_t& outOffset)
{
    uintptr_t base  = gp.moduleBase;
    uintptr_t match = AobScan(gp, kPatternMotbin,
                               base + 0x1800000,
                               base + 0x2800000);
    if (!match) return false;

    uint32_t offset = 0;
    if (!ReadGameValue(gp, match + 3, offset)) return false;

    outOffset = offset;
    return true;
}

// Ensure cached addresses are resolved.
// Returns true if both offsets are available (scanned or already cached).
static bool EnsureAddresses(const GameProcessInfo& gp)
{
    if (s_p1BaseOffset == 0)
        ScanP1BaseOffset(gp, s_p1BaseOffset);

    if (s_motbinOffset == 0)
        ScanMotbinOffset(gp, s_motbinOffset);

    return s_p1BaseOffset != 0 && s_motbinOffset != 0;
}

// Resolve player struct address.
// Python: readPointerPath(moduleBase+p1Offset, [0x30 + playerId*8, 0])
//   step1: *(moduleBase + p1Offset) + (0x30 + playerId*8)
//   step2: *(step1) + 0  ->  playerAddr
static bool GetPlayerAddr(const GameProcessInfo& gp, int playerId, uintptr_t& outAddr)
{
    uintptr_t root = 0;
    if (!ReadGamePointer(gp, gp.moduleBase + s_p1BaseOffset, root) || !root)
        return false;

    uintptr_t playerAddr = 0;
    if (!ReadGamePointer(gp, root + 0x30 + (uintptr_t)playerId * 8, playerAddr) || !playerAddr)
        return false;

    outAddr = playerAddr;
    return true;
}

} // namespace

namespace GameLiveEdit {

// Invalidate cached offsets (call after a game update is detected).
void InvalidateCache()
{
    s_p1BaseOffset = 0;
    s_motbinOffset = 0;
}

bool GetPlayerMoveId(int playerId, int& outMoveId)
{
    GameProcessInfo gp;
    if (!FindGameProcess(gp)) return false;

    bool ok = EnsureAddresses(gp);
    if (ok)
    {
        uintptr_t playerAddr = 0;
        ok = GetPlayerAddr(gp, playerId, playerAddr);
        if (ok)
        {
            uint32_t moveId = 0;
            ok = ReadGameValue(gp, playerAddr + kCurrMoveOffset, moveId);
            if (ok) outMoveId = (int)moveId;
        }
    }

    CloseGameProcess(gp);
    return ok;
}

bool PlayMove(int moveIdx)
{
    GameProcessInfo gp;
    if (!FindGameProcess(gp)) return false;

    bool ok = EnsureAddresses(gp);
    if (ok)
    {
        uintptr_t playerAddr = 0;
        ok = GetPlayerAddr(gp, 0, playerAddr);
        if (ok)
        {
            uintptr_t movesetPtr = 0;
            ok = ReadGamePointer(gp, playerAddr + s_motbinOffset, movesetPtr) && movesetPtr;
            if (ok)
            {
                uintptr_t movelistPtr = 0;
                ok = ReadGamePointer(gp, movesetPtr + kMovelistOffset, movelistPtr) && movelistPtr;
                if (ok)
                {
                    uintptr_t moveAddr = movelistPtr + (uintptr_t)moveIdx * kMoveSize;
                    uint32_t  timer    = 99999;
                    uint32_t  mid      = (uint32_t)moveIdx;
                    WriteGameValue(gp, playerAddr + kFrameTimerOffset, timer);
                    WriteGameValue(gp, playerAddr + kNextMoveOffset,   moveAddr);
                    WriteGameValue(gp, playerAddr + kCurrMoveOffset,   mid);
                }
            }
        }
    }

    CloseGameProcess(gp);
    return ok;
}

} // namespace GameLiveEdit
