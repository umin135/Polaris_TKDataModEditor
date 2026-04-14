#pragma once
#include <string>

// -------------------------------------------------------------
//  GameLiveEdit
//  Live interaction with the running Tekken 8 game process.
//  Addresses: game_addresses.txt version 27 (T8 / Polaris)
// -------------------------------------------------------------

struct MotbinData; // forward declaration; full definition in moveset/data/MotbinData.h

namespace GameLiveEdit {

// Read the current move ID being played by the given player.
//   playerId: 0 = 1P, 1 = 2P
//   outMoveId: receives the move index on success
// Returns false if the game process is not found or memory access fails.
bool GetPlayerMoveId(int playerId, int& outMoveId);

// Force player 1P to immediately play the given move.
// Returns false if the game process is not found or memory access fails.
bool PlayMove(int moveIdx);

// Invalidate cached offsets (call after a game update is detected).
void InvalidateCache();

// -------------------------------------------------------------
//  InjectMoveset
//  Syncs the current MotbinData edits to game memory in-place.
//  Patches all non-encrypted, non-pointer fields of existing
//  block entries without structural reallocations.
//  Structural changes (added / removed entries) are reflected
//  only for blocks where pointer fixup is not needed.
//  playerId: 0 = 1P, 1 = 2P
// -------------------------------------------------------------
struct InjectResult {
    bool        success        = false;
    std::string errorMsg;
    int         patchedMoves   = 0;
    int         patchedCancels = 0;
};

InjectResult InjectMoveset(const MotbinData& data, int playerId = 0);

} // namespace GameLiveEdit
