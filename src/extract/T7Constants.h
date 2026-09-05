#pragma once
#include <cstdint>
#include <cstddef>

// -------------------------------------------------------------
//  Tekken 7 (Steam) runtime constants — all T7 magic lives here.
//  Target: TekkenGame-Win64-Shipping.exe (final Steam build).
// -------------------------------------------------------------
namespace T7 {

static constexpr wchar_t kProcessName[] = L"TekkenGame-Win64-Shipping.exe";

// Player layout (static, adjacent)
static constexpr uintptr_t kP1Rva            = 0x34EA7D0;
static constexpr uintptr_t kPlayerSize       = 0x3670;
static constexpr uintptr_t kFighterIdOffset  = 0xD8;
static constexpr uintptr_t kMovesetOffset    = 0x1520;

// MovesetInfo
static constexpr size_t kMovesetInfoSize     = 0x2E8;   // header / name-block base
static constexpr size_t kMagicOffset         = 0x04;    // "HID\0"
static constexpr char   kMagic[4]            = { 'H', 'I', 'D', '\0' };
static constexpr size_t kIsInitializedOff    = 0x02;
static constexpr size_t kOrigAliasesOff      = 0x28;    // uint16[56]
static constexpr size_t kCurrentAliasesOff   = 0x98;    // uint16[56]
static constexpr size_t kUnknownAliasesOff   = 0x108;   // uint16[36]
static constexpr size_t kEncodedCharIdOff    = 0x148;   // u32: charId*0xFFFF+1
static constexpr size_t kTableOff            = 0x150;   // MovesetTable
static constexpr size_t kMotasOff            = 0x280;   // MotaList (13 ptrs)
static constexpr size_t kCharNameOff         = 0x2E8;   // null-terminated, often [Name]

static constexpr size_t kAliasCountT7        = 56;
static constexpr size_t kAliasCountT8        = 60;
static constexpr size_t kUnknownAliasCount   = 36;

static constexpr uint32_t kPlaceholderCharId = 999;

// Hardcoded into moveset.ini Version= on extract (final Steam build).
static constexpr char kMovesetIniVersion[] = " 5.01.01";

// Character ID encode/decode (shared formula T7/T8)
inline uint32_t DecodeCharId(uint32_t encoded)
{
    if (encoded == 0) return 0;
    return (encoded - 1u) / 0xFFFFu;
}
inline uint32_t EncodeCharId(uint32_t charId)
{
    return charId * 0xFFFFu + 1u;
}

// MovesetTable: 19 entries of {ptr u64, count u64}
static constexpr int kTableEntryCount = 19;
static constexpr size_t kTableEntrySize = 16;

enum TableIndex : int {
    kTblReactions = 0,
    kTblRequirement,
    kTblHitCondition,
    kTblProjectile,
    kTblPushback,
    kTblPushbackExtra,
    kTblCancel,
    kTblGroupCancel,
    kTblCancelExtra,
    kTblExtraMoveProperty,
    kTblMoveBeginningProp,
    kTblMoveEndingProp,
    kTblMove,
    kTblVoiceclip,
    kTblInputSequence,
    kTblInput,
    kTblParryRelated,
    kTblCameraData,
    kTblThrowCamera,
};

static constexpr size_t kStride[] = {
    0x70, // reactions
    0x08, // requirement
    0x18, // hitCondition
    0xA8, // projectile
    0x10, // pushback
    0x02, // pushbackExtra
    0x28, // cancel
    0x28, // groupCancel
    0x04, // cancelExtra
    0x0C, // extraMoveProperty
    0x10, // moveBeginningProp
    0x10, // moveEndingProp
    0xB0, // move
    0x04, // voiceclip
    0x10, // inputSequence
    0x08, // input
    0x04, // parryRelated
    0x0C, // cameraData
    0x10, // throwCamera
};

// Sanity caps
static constexpr uint64_t kMaxBlockCount   = 200000;
static constexpr size_t   kMaxStringLen    = 256;
static constexpr size_t   kMaxReadBytes    = 64ull * 1024 * 1024;

} // namespace T7
