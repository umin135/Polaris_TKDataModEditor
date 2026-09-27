// MovesetInjector.cpp
#include "moveset/live/MovesetInjector.h"
#include "moveset/live/GameLiveEdit.h"
#include "moveset/serialize/MotbinRuntime.h"
#include "extract/GameProcess.h"
#include <algorithm>
#include <cstring>
#include <cstdio>
#include <vector>

namespace {

// Four motbin slots relative to AoB parent (+0x39E8):
// +0x00, +0x08, +0x10, +0x20 — NOT +0x18 (gap / non-pointer field).
// static constexpr uintptr_t kMotbinSlotRel[4] = { 0x00, 0x08, 0x10, 0x20 };
static constexpr uintptr_t kMotbinSlotRel[] = { 0x0 };

struct SlotState {
    uintptr_t injectedAddr  = 0;
    uintptr_t originalAddr  = 0;
    size_t    allocSize     = 0;
    bool      active        = false;
};

static SlotState s_slots[2];

static bool WriteFiveSlots(const GameProcessInfo& gp, uintptr_t playerAddr,
                           uintptr_t parentOff, uintptr_t value)
{
    for (uintptr_t rel : kMotbinSlotRel)
    {
        if (!WriteGameValue(gp, playerAddr + parentOff + rel, static_cast<uint64_t>(value)))
            return false;
    }
    return true;
}

static bool ReadTekAndWritten(const GameProcessInfo& gp, uintptr_t addr,
                              bool& outWritten, char sig[4])
{
    uint8_t written = 0;
    if (!ReadGameValue(gp, addr + 0x02, written)) return false;
    outWritten = (written != 0);
    memset(sig, 0, 4);
    return ReadGameMemory(gp, addr + 0x08, sig, 3);
}

} // namespace

namespace MovesetInjector {

bool CanRestore(int playerId)
{
    if (playerId < 0 || playerId > 1) return false;
    const SlotState& st = s_slots[playerId];
    return st.active && st.originalAddr != 0;
}

bool IsLiveMoveset(uintptr_t addr)
{
    if (!addr) return false;
    GameProcessInfo gp;
    if (!FindGameProcess(gp)) return false;
    bool written = false;
    char sig[4] = {};
    bool ok = ReadTekAndWritten(gp, addr, written, sig);
    CloseGameProcess(gp);
    return ok && written && sig[0] == 'T' && sig[1] == 'E' && sig[2] == 'K';
}

InjectResult Inject(MotbinData& data, int playerId,
                    uint32_t expectedCharaId, bool allowCharMismatch)
{
    InjectResult r;
    if (playerId < 0 || playerId > 1)
    {
        r.message = "Invalid player id.";
        return r;
    }
    if (!data.loaded || data.moves.empty())
    {
        r.message = "No moveset loaded in editor.";
        return r;
    }

    GameProcessInfo gp;
    if (!FindGameProcess(gp))
    {
        r.message = "Game not running (Polaris-Win64-Shipping.exe).";
        return r;
    }

    GameLiveEdit::PlayerLive live;
    if (!GameLiveEdit::ResolvePlayerWithProcess(gp, playerId, live) || !live.playerAddr)
    {
        CloseGameProcess(gp);
        r.message = "Failed to resolve player slot.";
        return r;
    }

    // Validate current moveset TEK
    if (live.motbinAddr)
    {
        bool written = false;
        char sig[4] = {};
        if (!ReadTekAndWritten(gp, live.motbinAddr, written, sig) ||
            sig[0] != 'T' || sig[1] != 'E' || sig[2] != 'K')
        {
            CloseGameProcess(gp);
            r.message = "Current player moveset has invalid TEK signature.";
            return r;
        }
    }

    if (!allowCharMismatch && expectedCharaId != 0 && live.charaId != expectedCharaId)
    {
        CloseGameProcess(gp);
        r.message = "Character mismatch: slot char_id=" + std::to_string(live.charaId) +
                    " vs moveset expected=" + std::to_string(expectedCharaId) +
                    ". Anim handles will not resolve.";
        return r;
    }

    // If we already injected into this slot and game still points at us, free later after swap.
    // If game reloaded (parent != our inject), free orphaned inject if still readable.
    SlotState& st = s_slots[playerId];
    if (st.active && st.injectedAddr && st.injectedAddr != live.motbinAddr)
    {
        bool written = false;
        char sig[4] = {};
        if (ReadTekAndWritten(gp, st.injectedAddr, written, sig) &&
            written && sig[0] == 'T')
        {
            FreeGameMemory(gp, st.injectedAddr);
        }
        st = {};
    }

    // Pull live anim handles (move+0x50/0x54) from the currently loaded motbin.
    // These are body-manager IDs — distinct from anim_key (+0x20). Prefer live over
    // anim_runtime.json so injection still works if the JSON lo was never loaded/saved.
    if (live.motbinAddr)
    {
        uint64_t movesAbs = 0, movesCnt = 0;
        if (ReadGameValue(gp, live.motbinAddr + 0x230, movesAbs) &&
            ReadGameValue(gp, live.motbinAddr + 0x238, movesCnt) &&
            movesAbs && movesCnt)
        {
            const size_t n = (std::min)(data.moves.size(), static_cast<size_t>(movesCnt));
            for (size_t i = 0; i < n; ++i)
            {
                const uintptr_t e = static_cast<uintptr_t>(movesAbs + i * 0x448ull);
                uint32_t lo = 0, hi = 0;
                if (ReadGameValue(gp, e + 0x50, lo) && lo)
                    data.moves[i].anim_handle_lo = lo;
                if (ReadGameValue(gp, e + 0x54, hi) && hi)
                    data.moves[i].anim_handle_hi = hi;
            }
        }
    }

    auto state1 = RebuildMotbinBytes(data);
    if (state1.empty())
    {
        CloseGameProcess(gp);
        r.message = "RebuildMotbinBytes failed.";
        return r;
    }

    // Snapshot the live motbin so BuildRuntimeBlob can reuse validated
    // encryption blocks and the game's static "?" string pointers.
    std::vector<uint8_t> liveBytes;
    if (live.motbinAddr)
    {
        liveBytes.resize(state1.size());
        if (!ReadGameMemory(gp, live.motbinAddr, liveBytes.data(), liveBytes.size()))
            liveBytes.clear();
    }

    // Allocate first so BuildRuntimeBlob can target the real base.
    // Size = state1 + "?" string; BuildRuntimeBlob appends 2 bytes.
    const size_t allocSize = state1.size() + 2;
    uintptr_t target = AllocGameMemory(gp, allocSize);
    if (!target)
    {
        CloseGameProcess(gp);
        r.message = "VirtualAllocEx failed.";
        return r;
    }

    std::string err;
    auto blob = BuildRuntimeBlob(state1, data, static_cast<uint64_t>(target),
                                 live.charaId, err,
                                 liveBytes.empty() ? nullptr : liveBytes.data(),
                                 liveBytes.size(),
                                 liveBytes.empty() ? 0 : static_cast<uint64_t>(live.motbinAddr));
    if (blob.empty() || blob.size() > allocSize)
    {
        FreeGameMemory(gp, target);
        CloseGameProcess(gp);
        r.message = err.empty() ? "BuildRuntimeBlob failed." : err;
        return r;
    }

    if (!WriteGameMemory(gp, target, blob.data(), blob.size()))
    {
        FreeGameMemory(gp, target);
        CloseGameProcess(gp);
        r.message = "WriteProcessMemory failed.";
        return r;
    }

    // Verify TEK + is_written
    {
        bool written = false;
        char sig[4] = {};
        if (!ReadTekAndWritten(gp, target, written, sig) || !written ||
            sig[0] != 'T' || sig[1] != 'E' || sig[2] != 'K')
        {
            char detail[128];
            snprintf(detail, sizeof(detail),
                     "Post-write verification failed (is_written=%u sig=%.3s).",
                     (unsigned)written, sig[0] ? sig : "???");
            FreeGameMemory(gp, target);
            CloseGameProcess(gp);
            r.message = detail;
            return r;
        }
    }

    uintptr_t oldPtr = live.motbinAddr;
    if (!WriteFiveSlots(gp, live.playerAddr, live.motbinOffset, target))
    {
        FreeGameMemory(gp, target);
        CloseGameProcess(gp);
        r.message = "Failed to write player moveset pointers.";
        return r;
    }

    // Free previous inject if we replaced our own
    if (st.active && st.injectedAddr && st.injectedAddr != target &&
        st.injectedAddr != oldPtr)
    {
        FreeGameMemory(gp, st.injectedAddr);
    }

    st.injectedAddr = target;
    st.originalAddr = oldPtr;
    st.allocSize    = allocSize;
    st.active       = true;

    // Don't retarget next_move here: current move id may be an alias (>=0x8000).
    // Ton-Chan's importer only swaps the motbin pointer; the game continues fine.
    // GameLiveEdit::RetriggerCurrentMove(gp, playerId);

    CloseGameProcess(gp);

    r.ok            = true;
    r.allocatedAddr = target;
    r.blobSize      = blob.size();
    r.message       = "Injected " + std::to_string(blob.size()) + " bytes at 0x" +
                      [&]() {
                          char buf[32];
                          snprintf(buf, sizeof(buf), "%llX", (unsigned long long)target);
                          return std::string(buf);
                      }() +
                      " -> P" + std::to_string(playerId + 1);
    return r;
}

InjectResult Restore(int playerId)
{
    InjectResult r;
    if (playerId < 0 || playerId > 1)
    {
        r.message = "Invalid player id.";
        return r;
    }

    SlotState& st = s_slots[playerId];
    if (!st.active || !st.originalAddr)
    {
        r.message = "Nothing to restore for this slot.";
        return r;
    }

    GameProcessInfo gp;
    if (!FindGameProcess(gp))
    {
        r.message = "Game not running.";
        return r;
    }

    // Validate original still looks like a live moveset
    {
        bool written = false;
        char sig[4] = {};
        if (!ReadTekAndWritten(gp, st.originalAddr, written, sig) || !written ||
            sig[0] != 'T' || sig[1] != 'E' || sig[2] != 'K')
        {
            // Original gone (quick-select reload). Drop restore; free inject if orphaned.
            GameLiveEdit::PlayerLive live;
            if (GameLiveEdit::ResolvePlayerWithProcess(gp, playerId, live) &&
                live.motbinAddr == st.injectedAddr)
            {
                // Still pointing at us but original dead — cannot restore.
                r.message = "Previous moveset is gone (game reloaded). Restore aborted.";
            }
            else
            {
                if (st.injectedAddr)
                {
                    bool w2 = false; char s2[4] = {};
                    if (ReadTekAndWritten(gp, st.injectedAddr, w2, s2) && w2)
                        FreeGameMemory(gp, st.injectedAddr);
                }
                st = {};
                r.message = "Previous moveset is gone (game reloaded). Cleared inject state.";
            }
            CloseGameProcess(gp);
            return r;
        }
    }

    GameLiveEdit::PlayerLive live;
    if (!GameLiveEdit::ResolvePlayerWithProcess(gp, playerId, live))
    {
        CloseGameProcess(gp);
        r.message = "Failed to resolve player.";
        return r;
    }

    if (!WriteFiveSlots(gp, live.playerAddr, live.motbinOffset, st.originalAddr))
    {
        CloseGameProcess(gp);
        r.message = "Failed to write restore pointers.";
        return r;
    }

    if (st.injectedAddr)
        FreeGameMemory(gp, st.injectedAddr);

    // GameLiveEdit::RetriggerCurrentMove(gp, playerId);
    CloseGameProcess(gp);

    r.ok = true;
    r.message = "Restored original moveset for P" + std::to_string(playerId + 1);
    st = {};
    return r;
}

} // namespace MovesetInjector
