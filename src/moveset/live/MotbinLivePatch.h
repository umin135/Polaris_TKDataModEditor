#pragma once
#include "moveset/data/MotbinData.h"
#include <cstdint>

// -------------------------------------------------------------
//  MotbinLivePatch
//  Rewrite individual motbin elements into an already-injected
//  runtime blob while Live Editing is enabled.
// -------------------------------------------------------------

namespace MotbinLivePatch {

bool WriteRequirement(const MotbinData& data, int playerId, uint32_t idx);
bool WriteCancel(const MotbinData& data, int playerId, uint32_t idx);
bool WriteGroupCancel(const MotbinData& data, int playerId, uint32_t idx);
bool WriteCancelExtra(const MotbinData& data, int playerId, uint32_t idx);
bool WriteHitCondition(const MotbinData& data, int playerId, uint32_t idx);
bool WriteReaction(const MotbinData& data, int playerId, uint32_t idx);
bool WritePushback(const MotbinData& data, int playerId, uint32_t idx);
bool WritePushbackExtra(const MotbinData& data, int playerId, uint32_t idx);
bool WriteExtraProp(const MotbinData& data, int playerId, uint32_t idx);
bool WriteStartProp(const MotbinData& data, int playerId, uint32_t idx);
bool WriteEndProp(const MotbinData& data, int playerId, uint32_t idx);
bool WriteMove(const MotbinData& data, int playerId, uint32_t idx);
bool WriteVoiceclip(const MotbinData& data, int playerId, uint32_t idx);
bool WriteInput(const MotbinData& data, int playerId, uint32_t idx);
bool WriteInputSequence(const MotbinData& data, int playerId, uint32_t idx);
bool WriteProjectile(const MotbinData& data, int playerId, uint32_t idx);
bool WriteThrowExtra(const MotbinData& data, int playerId, uint32_t idx);
bool WriteThrow(const MotbinData& data, int playerId, uint32_t idx);
bool WriteParryable(const MotbinData& data, int playerId, uint32_t idx);
bool WriteDialogue(const MotbinData& data, int playerId, uint32_t idx);

} // namespace MotbinLivePatch
