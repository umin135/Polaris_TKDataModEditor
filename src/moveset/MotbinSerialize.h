#pragma once
#include <cstdint>
#include <string>
#include <vector>

// ─────────────────────────────────────────────────────────────
//  MotbinNameData
//
//  String data extracted from the game's virtual string block.
//  In state-3 (game memory) the move name/anim fields (move+0x040,
//  move+0x048) are absolute pointers into a SEPARATE game allocation
//  that is NOT part of the motbin block itself.  ExportLoaderBin uses
//  this struct to rebuild the virtual string-block byte offsets that
//  the state-1 file format stores in those fields.
//
//  Header virtual string-block layout (consecutive null-terminated):
//    offset 0              : charName
//    offset creatorOff     : charCreator   (header 0x18)
//    offset dateOff        : date          (header 0x20)
//    offset fullDateOff    : fullDate      (header 0x28)
//    offset moves[0].nameOff : move[0] name
//    offset moves[0].animOff : move[0] anim name
//    ...interleaved per move...
//  Total size → header 0x170 (string_block_end_offset).
// ─────────────────────────────────────────────────────────────
struct MotbinNameData {
    std::string charName;
    std::string charCreator;
    std::string date;
    std::string fullDate;
    struct MoveNameEntry { std::string name; std::string anim; };
    std::vector<MoveNameEntry> moves;
};

// ─────────────────────────────────────────────────────────────
//  ExportLoaderBin
//
//  Converts a raw state-3 motbin dump (absolute pointers from game
//  memory) to state-1 binary format compatible with TK8 loaders.
//  Mirrors OldTool2 jsonToBin.py output.
//
//  State-1 differences from state-3:
//    - Header block ptrs: file_offset - 0x318 (BASE-relative)
//    - Struct pointer fields: element index into target block array
//    - Move encrypted fields: XOR_KEYS scheme (8-key XOR)
//
//  rawBytes  : raw state-3 bytes (absolute pointers, no fixup applied)
//  motbinBase: original game memory base address of the motbin
//  names     : optional; when provided, move name_idx_abs/anim_idx_abs
//              fields (move+0x040/0x048) and header string-block offsets
//              (0x18/0x20/0x28) and string_block_end (0x170) are written.
//  Returns   : state-1 binary, or empty on failure
// ─────────────────────────────────────────────────────────────
// ─────────────────────────────────────────────────────────────
//  DecryptMotbinMoveKey
//
//  Decrypts one 16-byte XOR-encoded field block from a move buffer.
//  moveBuf : pointer to the start of a single move (0x448 bytes)
//  blockOff: byte offset of the block within the move
//            (0x00 = name_key, 0x20 = anim_key, 0x58 = vuln, ...)
//  Returns the decrypted uint32 value.
// ─────────────────────────────────────────────────────────────
uint32_t DecryptMotbinMoveKey(const uint8_t* moveBuf, size_t blockOff);

std::vector<uint8_t> ExportLoaderBin(const std::vector<uint8_t>& rawBytes,
                                      uint64_t motbinBase,
                                      const MotbinNameData* names = nullptr);
