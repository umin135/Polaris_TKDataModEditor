#pragma once
#include "moveset/data/MotbinData.h"
#include <cstdint>
#include <string>
#include <vector>

// -------------------------------------------------------------
//  MotbinRuntime
//  Convert state-1 (index-format) motbin bytes into state-3
//  (absolute-pointer, game-memory) form ready to inject.
//  Mirrors TK__populateMotbin, skipping anim-manager lookup.
// -------------------------------------------------------------

static constexpr uint64_t kTkDefaultEncKey = 0xEDCCFB96DCA40FBAULL;

// Port of TK__encrypt32BitWith32BitChecksum.
// Writes tk_encrypted {value, key} at dst (16 bytes). Uses kTkDefaultEncKey
// unless encKey is non-zero. Returns false if round-trip decrypt fails.
bool TkEncrypt32(uint8_t* dst16, uint32_t originalValue, uint64_t encKey = kTkDefaultEncKey);

// Inverse of TkEncrypt32 / ValidateAndDecrypt64 (game-memory scheme).
uint32_t TkDecrypt32(const uint8_t* src16);

// Build a fully-populated runtime blob at targetBase.
// state1  : index-format bytes from RebuildMotbinBytes
// model   : MotbinData (decrypted fields + anim_handle_lo/hi / anim_len)
// charId  : fighter id (Dummy=116 special-case)
// liveState3 / liveBase : optional currently-loaded motbin; when provided,
//   header/move "?" string pointers are copied from live (game static).
// Returns empty on failure; err is set.
std::vector<uint8_t> BuildRuntimeBlob(const std::vector<uint8_t>& state1,
                                      const MotbinData& model,
                                      uint64_t targetBase,
                                      uint32_t charId,
                                      std::string& err,
                                      const uint8_t* liveState3 = nullptr,
                                      size_t liveSize = 0,
                                      uint64_t liveBase = 0);

// Round-trip helper for verification: state3 live dump -> state1 -> state3
// at originalBase. Returns rebuilt state-3 bytes (or empty on failure).
std::vector<uint8_t> RoundTripRuntimeBlob(const std::vector<uint8_t>& state3Live,
                                          uint64_t originalBase,
                                          const MotbinData* modelOrNull,
                                          uint32_t charId,
                                          std::string& err);
