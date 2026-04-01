#pragma once

// -------------------------------------------------------------
//  GameLiveEdit
//  Live interaction with the running Tekken 8 game process.
//  Addresses: game_addresses.txt version 27 (T8 / Polaris)
// -------------------------------------------------------------

namespace GameLiveEdit {

// Read the current move ID being played by the given player.
//   playerId: 0 = 1P, 1 = 2P
//   outMoveId: receives the move index on success
// Returns false if the game process is not found or memory access fails.
bool GetPlayerMoveId(int playerId, int& outMoveId);

// Force player 1P to immediately play the given move.
// Returns false if the game process is not found or memory access fails.
bool PlayMove(int moveIdx);

} // namespace GameLiveEdit
