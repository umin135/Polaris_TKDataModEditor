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

// The move-start code stores next_move and copies the frame timer right after it:
//   mov [rdi+next_move],rax / mov rcx,rdi / mov eax,[rdi+frame_timer] / mov [rdi+next_move+10h],eax
// next_move moved 0x2870 -> 0x28F0 in game 3.03 (the player struct grew), so both offsets are
// read from this site instead of being hardcoded. Unique in 3.02 and 3.03.
static constexpr const char* kPatternNextMove =
    "48 89 87 ?? ?? ?? 00 "
    "48 8B CF "
    "8B 87 ?? ?? 00 00 "
    "89 87 ?? ?? ?? 00";

static constexpr uintptr_t kMovelistOffset   = 0x230;
static constexpr uintptr_t kMoveSize         = 0x448;
static constexpr uintptr_t kCurrMoveOffset   = 0x550;
static constexpr uintptr_t kCharaIdOffset    = 0x168;

static uintptr_t s_p1BaseOffset     = 0;
static uintptr_t s_motbinOffset     = 0;
static uintptr_t s_nextMoveOffset   = 0;   // 0x28F0 on 3.03 (0x2870 before)
static uintptr_t s_frameTimerOffset = 0;   // 0x390

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

static bool ScanMoveOffsets(const GameProcessInfo& gp, uintptr_t& outNextMove, uintptr_t& outTimer)
{
    uintptr_t base  = gp.moduleBase;
    uintptr_t match = AobScan(gp, kPatternNextMove, base + 0x1800000, base + 0x2800000);
    if (!match) return false;
    uint32_t nextMove = 0, timer = 0, copyDst = 0;
    if (!ReadGameValue(gp, match + 3, nextMove) || !ReadGameValue(gp, match + 12, timer)
        || !ReadGameValue(gp, match + 18, copyDst))
        return false;
    // Sanity: the timer copy lands 0x10 past next_move, and both sit inside the player struct.
    if (copyDst != nextMove + 0x10 || nextMove < 0x1000 || nextMove > 0x8000 || timer > 0x1000)
        return false;
    outNextMove = nextMove;
    outTimer    = timer;
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

// Play Move / retrigger also need next_move + frame_timer. Kept apart so a miss here only
// disables those two, not player resolution / moveset injection.
static bool EnsureMoveOffsets(const GameProcessInfo& gp)
{
    if (!EnsureAddresses(gp)) return false;
    if (s_nextMoveOffset == 0 || s_frameTimerOffset == 0)
        ScanMoveOffsets(gp, s_nextMoveOffset, s_frameTimerOffset);
    return s_nextMoveOffset != 0 && s_frameTimerOffset != 0;
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
    s_p1BaseOffset     = 0;
    s_motbinOffset     = 0;
    s_nextMoveOffset   = 0;
    s_frameTimerOffset = 0;
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
    if (!gp.valid || !EnsureMoveOffsets(gp)) return false;
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

    // Alias IDs (>= 0x8000) index current_aliases / original_aliases, not the move array.
    uint32_t moveIdx = moveId;
    if (moveId >= 0x8000u)
    {
        const uint32_t aliasIdx = moveId - 0x8000u;
        if (aliasIdx >= 60u)
            return false;
        uint16_t curAlias = 0, origAlias = 0;
        if (!ReadGameValue(gp, movesetPtr + 0xA8 + aliasIdx * 2ull, curAlias))
            return false;
        if (!ReadGameValue(gp, movesetPtr + 0x30 + aliasIdx * 2ull, origAlias))
            return false;
        moveIdx = curAlias ? curAlias : origAlias;
    }

    uint64_t movesCnt = 0;
    if (!ReadGameValue(gp, movesetPtr + 0x238, movesCnt) || moveIdx >= movesCnt)
        return false;

    uintptr_t moveAddr = movelistPtr + (uintptr_t)moveIdx * kMoveSize;
    uint32_t  timer    = 99999;
    WriteGameValue(gp, playerAddr + s_frameTimerOffset, timer);
    WriteGameValue(gp, playerAddr + s_nextMoveOffset,  moveAddr);
    // Keep the raw id (may be an alias) — only next_move must be a real move pointer.
    WriteGameValue(gp, playerAddr + kCurrMoveOffset,   moveId);
    return true;
}

bool PlayMoveOnPlayer(int playerId, int moveIdx)
{
    GameProcessInfo gp;
    if (!FindGameProcess(gp)) return false;
    bool ok = EnsureMoveOffsets(gp);
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
                    WriteGameValue(gp, playerAddr + s_frameTimerOffset, timer);
                    WriteGameValue(gp, playerAddr + s_nextMoveOffset,  moveAddr);
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
