#pragma once
#include "moveset/data/MotbinData.h"
#include <cstdint>
#include <string>

// -------------------------------------------------------------
//  MovesetInjector
//  Allocate + populate a runtime motbin in the game process and
//  hang it on P1/P2 (five contiguous tk_moveset* slots).
// -------------------------------------------------------------

namespace MovesetInjector {

struct InjectResult {
    bool        ok = false;
    std::string message;
    uintptr_t   allocatedAddr = 0;
    size_t      blobSize      = 0;
};

// Inject the editor moveset into playerId (0=P1, 1=P2).
// expectedCharaId: if non-zero and slot char differs, refuse unless allowCharMismatch.
InjectResult Inject(MotbinData& data, int playerId,
                    uint32_t expectedCharaId = 0,
                    bool allowCharMismatch = false);

// Restore the pre-inject moveset pointer for playerId, if still live.
InjectResult Restore(int playerId);

// True if this slot has a tracked inject that Restore can attempt.
bool CanRestore(int playerId);

// True if addr looks like a populated moveset (is_written@+2, TEK@+8).
bool IsLiveMoveset(uintptr_t addr);

} // namespace MovesetInjector
