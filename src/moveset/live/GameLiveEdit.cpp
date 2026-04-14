// GameLiveEdit.cpp
// Live memory interaction with Tekken 8 (Polaris-Win64-Shipping.exe).
// Player base address and motbin offset are resolved via AoB scan at runtime
// and cached for the lifetime of the process so subsequent calls are instant.
#include "moveset/live/GameLiveEdit.h"
#include "extract/GameProcess.h"
#include "moveset/data/MotbinData.h"
#include <cstdint>
#include <cstring>
#include <algorithm>

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

InjectResult InjectMoveset(const MotbinData& data, int playerId)
{
    InjectResult res;
    if (!data.loaded) { res.errorMsg = "No moveset loaded"; return res; }

    GameProcessInfo gp;
    if (!FindGameProcess(gp)) { res.errorMsg = "Game not running"; return res; }

    bool ok = EnsureAddresses(gp);
    uintptr_t playerAddr = 0;
    uintptr_t motbinAddr = 0;

    if (ok) ok = GetPlayerAddr(gp, playerId, playerAddr);
    if (ok) ok = ReadGamePointer(gp, playerAddr + s_motbinOffset, motbinAddr) && motbinAddr != 0;

    if (!ok)
    {
        CloseGameProcess(gp);
        res.errorMsg = "Could not locate player moveset in game memory";
        return res;
    }

    // Read (absolute ptr, count) from a motbin header pair of offsets.
    auto readBlockHdr = [&](uintptr_t ptrOff, uintptr_t cntOff,
                             uint64_t& outPtr, uint64_t& outCnt)
    {
        ReadGameValue(gp, motbinAddr + ptrOff, outPtr);
        ReadGameValue(gp, motbinAddr + cntOff, outCnt);
    };

    // --- Moves ---
    {
        uint64_t moveListPtr = 0, moveCount = 0;
        readBlockHdr(0x230, 0x238, moveListPtr, moveCount);

        size_t n = (std::min)((size_t)moveCount, data.moves.size());
        for (size_t i = 0; i < n; ++i)
        {
            const ParsedMove& m   = data.moves[i];
            uintptr_t moveAddr = (uintptr_t)moveListPtr + i * kMoveSize;

            WriteGameValue(gp, moveAddr + 0xC8,  m.u6);
            WriteGameValue(gp, moveAddr + 0xCC,  m.transition);
            WriteGameValue(gp, moveAddr + 0xCE,  m._0xCE);
            WriteGameValue(gp, moveAddr + 0x118, m._0x118);
            WriteGameValue(gp, moveAddr + 0x11C, m._0x11C);
            WriteGameValue(gp, moveAddr + 0x120, m.anim_len);
            WriteGameValue(gp, moveAddr + 0x124, m.airborne_start);
            WriteGameValue(gp, moveAddr + 0x128, m.airborne_end);
            WriteGameValue(gp, moveAddr + 0x12C, m.ground_fall);
            WriteGameValue(gp, moveAddr + 0x150, m.u15);
            WriteGameValue(gp, moveAddr + 0x154, m._0x154);
            WriteGameValue(gp, moveAddr + 0x158, m.startup);
            WriteGameValue(gp, moveAddr + 0x15C, m.recovery);

            // Hitboxes: game stores AOS (8 slots × 0x30 bytes starting at +0x160).
            // ParsedMove stores them transposed (SOA), so rebuild each slot inline.
            uint8_t hslot[0x30];
            for (int h = 0; h < 8; ++h)
            {
                memset(hslot, 0, sizeof(hslot));
                memcpy(hslot + 0x00, &m.hitbox_active_start[h], 4);
                memcpy(hslot + 0x04, &m.hitbox_active_last[h],  4);
                memcpy(hslot + 0x08, &m.hitbox_location[h],     4);
                for (int f = 0; f < 9; ++f)
                    memcpy(hslot + 0x0C + f * 4, &m.hitbox_floats[h][f], 4);
                WriteGameMemory(gp, moveAddr + 0x160 + (uintptr_t)h * 0x30, hslot, 0x30);
            }

            WriteGameValue (gp, moveAddr + 0x2E0, m.collision);
            WriteGameValue (gp, moveAddr + 0x2E2, m.distance);
            WriteGameMemory(gp, moveAddr + 0x2E4, m.unk5, sizeof(m.unk5));
            WriteGameValue (gp, moveAddr + 0x444, m.u18);

            ++res.patchedMoves;
        }
    }

    // --- Cancels ---
    {
        uint64_t ptr = 0, cnt = 0;
        readBlockHdr(0x1D0, 0x1D8, ptr, cnt);
        size_t n = (std::min)((size_t)cnt, data.cancelBlock.size());
        for (size_t i = 0; i < n; ++i)
        {
            const ParsedCancel& c = data.cancelBlock[i];
            uintptr_t addr = (uintptr_t)ptr + i * 0x28;
            WriteGameValue(gp, addr + 0x00, c.command);
            WriteGameValue(gp, addr + 0x18, c.frame_window_start);
            WriteGameValue(gp, addr + 0x1C, c.frame_window_end);
            WriteGameValue(gp, addr + 0x20, c.starting_frame);
            WriteGameValue(gp, addr + 0x24, c.move_id);
            WriteGameValue(gp, addr + 0x26, c.cancel_option);
            ++res.patchedCancels;
        }
    }

    // --- Group cancels ---
    {
        uint64_t ptr = 0, cnt = 0;
        readBlockHdr(0x1E0, 0x1E8, ptr, cnt);
        size_t n = (std::min)((size_t)cnt, data.groupCancelBlock.size());
        for (size_t i = 0; i < n; ++i)
        {
            const ParsedCancel& c = data.groupCancelBlock[i];
            uintptr_t addr = (uintptr_t)ptr + i * 0x28;
            WriteGameValue(gp, addr + 0x00, c.command);
            WriteGameValue(gp, addr + 0x18, c.frame_window_start);
            WriteGameValue(gp, addr + 0x1C, c.frame_window_end);
            WriteGameValue(gp, addr + 0x20, c.starting_frame);
            WriteGameValue(gp, addr + 0x24, c.move_id);
            WriteGameValue(gp, addr + 0x26, c.cancel_option);
        }
    }

    // --- Requirements ---
    {
        uint64_t ptr = 0, cnt = 0;
        readBlockHdr(0x180, 0x188, ptr, cnt);
        size_t n = (std::min)((size_t)cnt, data.requirementBlock.size());
        for (size_t i = 0; i < n; ++i)
        {
            const ParsedRequirement& r = data.requirementBlock[i];
            uint32_t buf[5] = { r.req, r.param, r.param2, r.param3, r.param4 };
            WriteGameMemory(gp, (uintptr_t)ptr + i * 0x14, buf, 0x14);
        }
    }

    // --- Hit conditions (damage + unknown; pointer fields at +0x00/+0x10 skipped) ---
    {
        uint64_t ptr = 0, cnt = 0;
        readBlockHdr(0x190, 0x198, ptr, cnt);
        size_t n = (std::min)((size_t)cnt, data.hitConditionBlock.size());
        for (size_t i = 0; i < n; ++i)
        {
            const ParsedHitCondition& h = data.hitConditionBlock[i];
            uintptr_t addr = (uintptr_t)ptr + i * 0x18;
            WriteGameValue(gp, addr + 0x08, h.damage);
            WriteGameValue(gp, addr + 0x0C, h._0x0C);
        }
    }

    // --- Reaction lists (direction / rotation / move-id fields; pushback ptrs skipped) ---
    {
        uint64_t ptr = 0, cnt = 0;
        readBlockHdr(0x168, 0x178, ptr, cnt);
        size_t n = (std::min)((size_t)cnt, data.reactionListBlock.size());
        for (size_t i = 0; i < n; ++i)
        {
            const ParsedReactionList& r = data.reactionListBlock[i];
            uintptr_t base = (uintptr_t)ptr + i * 0x70;
            WriteGameValue(gp, base + 0x38, r.front_direction);
            WriteGameValue(gp, base + 0x3A, r.back_direction);
            WriteGameValue(gp, base + 0x3C, r.left_side_direction);
            WriteGameValue(gp, base + 0x3E, r.right_side_direction);
            WriteGameValue(gp, base + 0x40, r.front_ch_direction);
            WriteGameValue(gp, base + 0x42, r.downed_direction);
            WriteGameValue(gp, base + 0x44, r.front_rotation);
            WriteGameValue(gp, base + 0x46, r.back_rotation);
            WriteGameValue(gp, base + 0x48, r.left_side_rotation);
            WriteGameValue(gp, base + 0x4A, r.right_side_rotation);
            WriteGameValue(gp, base + 0x4C, r.vertical_pushback);
            WriteGameValue(gp, base + 0x4E, r.downed_rotation);
            WriteGameValue(gp, base + 0x50, r.standing);
            WriteGameValue(gp, base + 0x52, r.crouch);
            WriteGameValue(gp, base + 0x54, r.ch);
            WriteGameValue(gp, base + 0x56, r.crouch_ch);
            WriteGameValue(gp, base + 0x58, r.left_side);
            WriteGameValue(gp, base + 0x5A, r.left_side_crouch);
            WriteGameValue(gp, base + 0x5C, r.right_side);
            WriteGameValue(gp, base + 0x5E, r.right_side_crouch);
            WriteGameValue(gp, base + 0x60, r.back);
            WriteGameValue(gp, base + 0x62, r.back_crouch);
            WriteGameValue(gp, base + 0x64, r.block);
            WriteGameValue(gp, base + 0x66, r.crouch_block);
            WriteGameValue(gp, base + 0x68, r.wallslump);
            WriteGameValue(gp, base + 0x6A, r.downed);
        }
    }

    // --- Pushbacks (extra-ptr at +0x08 skipped) ---
    {
        uint64_t ptr = 0, cnt = 0;
        readBlockHdr(0x1B0, 0x1B8, ptr, cnt);
        size_t n = (std::min)((size_t)cnt, data.pushbackBlock.size());
        for (size_t i = 0; i < n; ++i)
        {
            const ParsedPushback& p = data.pushbackBlock[i];
            uintptr_t addr = (uintptr_t)ptr + i * 0x10;
            WriteGameValue(gp, addr + 0x00, p.val1);
            WriteGameValue(gp, addr + 0x02, p.val2);
            WriteGameValue(gp, addr + 0x04, p.val3);
        }
    }

    // --- Pushback extras ---
    {
        uint64_t ptr = 0, cnt = 0;
        readBlockHdr(0x1C0, 0x1C8, ptr, cnt);
        size_t n = (std::min)((size_t)cnt, data.pushbackExtraBlock.size());
        for (size_t i = 0; i < n; ++i)
            WriteGameValue(gp, (uintptr_t)ptr + i * 0x02, data.pushbackExtraBlock[i].value);
    }

    // --- Cancel extras ---
    {
        uint64_t ptr = 0, cnt = 0;
        readBlockHdr(0x1F0, 0x1F8, ptr, cnt);
        size_t n = (std::min)((size_t)cnt, data.cancelExtraBlock.size());
        for (size_t i = 0; i < n; ++i)
            WriteGameValue(gp, (uintptr_t)ptr + i * 0x04, data.cancelExtraBlock[i]);
    }

    // --- Extra props (timed, stride 0x28; req ptr at +0x08 skipped) ---
    {
        uint64_t ptr = 0, cnt = 0;
        readBlockHdr(0x200, 0x208, ptr, cnt);
        size_t n = (std::min)((size_t)cnt, data.extraPropBlock.size());
        for (size_t i = 0; i < n; ++i)
        {
            const ParsedExtraProp& e = data.extraPropBlock[i];
            uintptr_t addr = (uintptr_t)ptr + i * 0x28;
            WriteGameValue(gp, addr + 0x00, e.type);
            WriteGameValue(gp, addr + 0x04, e._0x4);
            WriteGameValue(gp, addr + 0x10, e.id);
            WriteGameValue(gp, addr + 0x14, e.value);
            WriteGameValue(gp, addr + 0x18, e.value2);
            WriteGameValue(gp, addr + 0x1C, e.value3);
            WriteGameValue(gp, addr + 0x20, e.value4);
            WriteGameValue(gp, addr + 0x24, e.value5);
        }
    }

    // --- Start props (untimed, stride 0x20; req ptr at +0x00 skipped) ---
    {
        uint64_t ptr = 0, cnt = 0;
        readBlockHdr(0x210, 0x218, ptr, cnt);
        size_t n = (std::min)((size_t)cnt, data.startPropBlock.size());
        for (size_t i = 0; i < n; ++i)
        {
            const ParsedExtraProp& e = data.startPropBlock[i];
            uintptr_t addr = (uintptr_t)ptr + i * 0x20;
            WriteGameValue(gp, addr + 0x08, e.id);
            WriteGameValue(gp, addr + 0x0C, e.value);
            WriteGameValue(gp, addr + 0x10, e.value2);
            WriteGameValue(gp, addr + 0x14, e.value3);
            WriteGameValue(gp, addr + 0x18, e.value4);
            WriteGameValue(gp, addr + 0x1C, e.value5);
        }
    }

    // --- End props (untimed, stride 0x20; req ptr at +0x00 skipped) ---
    {
        uint64_t ptr = 0, cnt = 0;
        readBlockHdr(0x220, 0x228, ptr, cnt);
        size_t n = (std::min)((size_t)cnt, data.endPropBlock.size());
        for (size_t i = 0; i < n; ++i)
        {
            const ParsedExtraProp& e = data.endPropBlock[i];
            uintptr_t addr = (uintptr_t)ptr + i * 0x20;
            WriteGameValue(gp, addr + 0x08, e.id);
            WriteGameValue(gp, addr + 0x0C, e.value);
            WriteGameValue(gp, addr + 0x10, e.value2);
            WriteGameValue(gp, addr + 0x14, e.value3);
            WriteGameValue(gp, addr + 0x18, e.value4);
            WriteGameValue(gp, addr + 0x1C, e.value5);
        }
    }

    // --- Voiceclips ---
    {
        uint64_t ptr = 0, cnt = 0;
        readBlockHdr(0x240, 0x248, ptr, cnt);
        size_t n = (std::min)((size_t)cnt, data.voiceclipBlock.size());
        for (size_t i = 0; i < n; ++i)
        {
            const ParsedVoiceclip& v = data.voiceclipBlock[i];
            uintptr_t addr = (uintptr_t)ptr + i * 0x0C;
            WriteGameValue(gp, addr + 0x00, v.val1);
            WriteGameValue(gp, addr + 0x04, v.val2);
            WriteGameValue(gp, addr + 0x08, v.val3);
        }
    }

    CloseGameProcess(gp);
    res.success = true;
    return res;
}

} // namespace GameLiveEdit
