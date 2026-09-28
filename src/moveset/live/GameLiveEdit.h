#pragma once
#include "extract/GameProcess.h"
#include <cstdint>

// -------------------------------------------------------------
//  GameLiveEdit
//  Live interaction with the running Tekken 8 game process.
// -------------------------------------------------------------

namespace GameLiveEdit {

struct PlayerLive {
    uintptr_t playerAddr    = 0;
    uintptr_t motbinOffset  = 0;   // AOB-resolved parent_moveset offset (+0x39E8)
    uintptr_t motbinAddr    = 0;   // current parent moveset pointer
    uint32_t  charaId       = 0;
};

// Resolve P1 (0) / P2 (1). Opens and closes the process internally.
bool ResolvePlayer(int playerId, PlayerLive& out);

// Resolve using an already-open process handle (for injection).
bool ResolvePlayerWithProcess(const GameProcessInfo& gp, int playerId, PlayerLive& out);

bool GetPlayerMoveId(int playerId, int& outMoveId);
bool PlayMove(int moveIdx);
bool PlayMoveOnPlayer(int playerId, int moveIdx);

// Force current move re-trigger on an open process (after moveset swap).
// Resolves alias IDs (>= 0x8000) via current_aliases / original_aliases.
bool RetriggerCurrentMove(const GameProcessInfo& gp, int playerId);

void InvalidateCache();

} // namespace GameLiveEdit
