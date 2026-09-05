#pragma once
#include <cstdint>
#include <string>
#include <vector>

// -------------------------------------------------------------
//  Physical string-block gate (Polaris / editor extension)
//
//  Header 0x0C is unused padding in stock Tekken 8.  We set it to
//  kMotbinPhysicalStringBlockFlag when the file embeds the string
//  block bytes immediately after the 0x318 header.  Old extracts
//  leave 0x0C = 0, so LoadMotbin keeps the kamui-hashes name path.
//
//  Header 0x170 = string_block_end_offset (byte size of that block,
//  padded to 8 so file offset 0x318+size stays 8-byte aligned; header
//  is already 0x318-aligned). Trailing pad bytes are zeros.
//  Move +0x40 / +0x48 and header 0x10/0x18/0x20/0x28 are offsets
//  into that block (0 = first string), not file offsets.
// -------------------------------------------------------------
static constexpr size_t   kMotbinHdrSize                 = 0x318;
static constexpr size_t   kMotbinStringBlockFlagOff      = 0x0C;
static constexpr uint32_t kMotbinPhysicalStringBlockFlag = 1u;
static constexpr size_t   kMotbinStringBlockEndOff       = 0x170;

// Pad physical string-block bytes so the next section starts 8-aligned.
inline void PadMotbinStringBlockTo8(std::vector<uint8_t>& bytes)
{
    const size_t n = bytes.size();
    const size_t padded = (n + 7u) & ~size_t(7u);
    if (padded > n)
        bytes.resize(padded, 0);
}

// -------------------------------------------------------------
//  MotbinNameData
//
//  String data for the string block.
//  In state-3 (game memory) the move name/anim fields (move+0x040,
//  move+0x048) are absolute pointers into a SEPARATE game allocation
//  that is NOT part of the motbin block itself.  ExportLoaderBin uses
//  this struct to rebuild string-block byte offsets and, when writing
//  a gated file, the physical string bytes after the header.
//
//  String-block layout (consecutive null-terminated):
//    offset 0              : charName
//    offset creatorOff     : charCreator   (header 0x18)
//    offset dateOff        : date          (header 0x20)
//    offset fullDateOff    : fullDate      (header 0x28)
//    offset moves[0].nameOff : move[0] name
//    offset moves[0].animOff : move[0] anim name
//    ...interleaved per move...
//  Total size -> header 0x170 (string_block_end_offset).
// -------------------------------------------------------------
struct MotbinNameData {
    std::string charName;
    std::string charCreator;
    std::string date;
    std::string fullDate;
    struct MoveNameEntry { std::string name; std::string anim; };
    std::vector<MoveNameEntry> moves;
};

// Build the packed \0-separated string-block bytes + per-string offsets.
struct MotbinStringBlockBuilt {
    std::vector<uint8_t> bytes;
    uint64_t creatorOff  = 0;
    uint64_t dateOff     = 0;
    uint64_t fullDateOff = 0;
    std::vector<uint64_t> nameOff;
    std::vector<uint64_t> animOff;
};
MotbinStringBlockBuilt BuildMotbinStringBlock(const MotbinNameData& names);

// -------------------------------------------------------------
//  ExportLoaderBin
//
//  Converts a raw state-3 motbin dump (absolute pointers from game
//  memory) to state-1 loader format (index-relative offsets).
//
//  When names is provided: writes string offsets, embeds the physical
//  string block after 0x318, sets header 0x0C flag, and shifts all
//  BASE-relative block ptrs past the string blob.
// -------------------------------------------------------------
uint32_t DecryptMotbinMoveKey(const uint8_t* moveBuf, size_t blockOff);

// Write one 0x20-byte XOR-encoded key block (8 × uint32) used by the
// index-format .motbin file. Slot [moveIdx % 8] holds (value ^ key[slot]);
// other slots hold ((0x765 + moveIdx) ^ key[j]).
void MotbinXorEncryptBlock(uint8_t* dest20, uint32_t value, uint32_t moveIdx);

std::vector<uint8_t> ExportLoaderBin(const std::vector<uint8_t>& rawBytes,
                                      uint64_t motbinBase,
                                      const MotbinNameData* names = nullptr);
