// GameLiveEdit.cpp
#include "moveset/live/GameLiveEdit.h"
#include <cstdint>

namespace {

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

static constexpr uintptr_t kMovelistOffset   = 0x230;
static constexpr uintptr_t kMoveSize         = 0x448;
static constexpr uintptr_t kCurrMoveOffset   = 0x550;
static constexpr uintptr_t kNextMoveOffset   = 0x2870;
static constexpr uintptr_t kFrameTimerOffset = 0x390;
static constexpr uintptr_t kCharaIdOffset    = 0x168;

static uintptr_t s_p1BaseOffset = 0;
static uintptr_t s_motbinOffset = 0;

static bool ScanP1BaseOffset(const GameProcessInfo& gp, uintptr_t& outOffset)
{
    uintptr_t base  = gp.moduleBase;
    uintptr_t match = AobScan(gp, kPatternP1, base + 0x5A00000, base + 0x6F00000);
    if (!match) return false;
    int32_t disp32 = 0;
    if (!ReadGameValue(gp, match + 3, disp32)) return false;
    outOffset = (uintptr_t)((intptr_t)(match + 7) + disp32) - base;
    return true;
}

static bool ScanMotbinOffset(const GameProcessInfo& gp, uintptr_t& outOffset)
{
    uintptr_t base  = gp.moduleBase;
    uintptr_t match = AobScan(gp, kPatternMotbin, base + 0x1800000, base + 0x2800000);
    if (!match) return false;
    uint32_t offset = 0;
    if (!ReadGameValue(gp, match + 3, offset)) return false;
    outOffset = offset;
    return true;
}

static bool EnsureAddresses(const GameProcessInfo& gp)
{
    if (s_p1BaseOffset == 0)
        ScanP1BaseOffset(gp, s_p1BaseOffset);
    if (s_motbinOffset == 0)
        ScanMotbinOffset(gp, s_motbinOffset);
    return s_p1BaseOffset != 0 && s_motbinOffset != 0;
}

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

void InvalidateCache()
{
    s_p1BaseOffset = 0;
    s_motbinOffset = 0;
}

bool ResolvePlayerWithProcess(const GameProcessInfo& gp, int playerId, PlayerLive& out)
{
    out = {};
    if (!gp.valid || !EnsureAddresses(gp)) return false;
    uintptr_t playerAddr = 0;
    if (!GetPlayerAddr(gp, playerId, playerAddr)) return false;
    out.playerAddr   = playerAddr;
    out.motbinOffset = s_motbinOffset;
    ReadGamePointer(gp, playerAddr + s_motbinOffset, out.motbinAddr);
    ReadGameValue(gp, playerAddr + kCharaIdOffset, out.charaId);
    return out.playerAddr != 0;
}

bool ResolvePlayer(int playerId, PlayerLive& out)
{
    GameProcessInfo gp;
    if (!FindGameProcess(gp)) return false;
    bool ok = ResolvePlayerWithProcess(gp, playerId, out);
    CloseGameProcess(gp);
    return ok;
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

bool RetriggerCurrentMove(const GameProcessInfo& gp, int playerId)
{
    if (!gp.valid || !EnsureAddresses(gp)) return false;
    uintptr_t playerAddr = 0;
    if (!GetPlayerAddr(gp, playerId, playerAddr)) return false;

    uint32_t moveId = 0;
    if (!ReadGameValue(gp, playerAddr + kCurrMoveOffset, moveId))
        return false;

    uintptr_t movesetPtr = 0;
    if (!ReadGamePointer(gp, playerAddr + s_motbinOffset, movesetPtr) || !movesetPtr)
        return false;

    uintptr_t movelistPtr = 0;
    if (!ReadGamePointer(gp, movesetPtr + kMovelistOffset, movelistPtr) || !movelistPtr)
        return false;

    uintptr_t moveAddr = movelistPtr + (uintptr_t)moveId * kMoveSize;
    uint32_t  timer    = 99999;
    WriteGameValue(gp, playerAddr + kFrameTimerOffset, timer);
    WriteGameValue(gp, playerAddr + kNextMoveOffset,   moveAddr);
    WriteGameValue(gp, playerAddr + kCurrMoveOffset,   moveId);
    return true;
}

bool PlayMoveOnPlayer(int playerId, int moveIdx)
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

bool PlayMove(int moveIdx)
{
    return PlayMoveOnPlayer(0, moveIdx);
}

} // namespace GameLiveEdit
