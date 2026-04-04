#pragma once
#include <cstdint>
#include <string>
#include <vector>

// -------------------------------------------------------------
//  AnmbinEntry  -- one entry in a category list (0x38 bytes)
// -------------------------------------------------------------
struct AnmbinEntry {
    uint64_t animKey;      // +0x00  lower 32 bits = animation hash key
    uint64_t animDataPtr;  // +0x08  absolute file offset to animation data
    uint8_t  extra[0x28];  // +0x10  remaining fields (preserved verbatim)
};

// -------------------------------------------------------------
//  Category info accessors
// -------------------------------------------------------------
inline const char* AnmbinCategoryName(int i)
{
    static const char* n[] = { "Fullbody","Hand","Facial","Swing","Camera","Extra" };
    return (i >= 0 && i < 6) ? n[i] : "?";
}
inline const char* AnmbinCategoryExt(int i)
{
    static const char* e[] = { ".bin",".anmhd",".anmfa",".anmsw",".anmca",".anmex" };
    return (i >= 0 && i < 6) ? e[i] : "";
}
inline const char* AnmbinCategoryFolder(int i)
{
    static const char* f[] = { "0_Fullbody","1_Hand","2_Facial","3_Swing","4_Camera","5_Extra" };
    return (i >= 0 && i < 6) ? f[i] : "?";
}

// -------------------------------------------------------------
//  AnmbinData  -- parsed .anmbin container
//
//  File layout:
//    0x04: uint32[6]  poolCounts    -- unique animation count per category
//    0x1C: uint32[6]  moveCounts    -- per-move animation ref count (= move count for each cat)
//    0x38: uint64[6]  poolOffsets   -- file offsets to unique animation entry lists
//    0x68: uint64[6]  moveListOffsets -- file offsets to per-move u32 hash arrays
//
//  pool[cat][i]     : unique animation entry {animKey, animDataPtr, extra}
//  moveList[cat][i] : u32 hash for move i's animation in this category
//                     move i  →  moveList[0][i]  (fullbody hash)
// -------------------------------------------------------------
struct AnmbinData {
    std::string filePath;
    bool        loaded   = false;
    std::string errorMsg;

    uint32_t poolCounts[6]      = {};   // 0x04: unique animation counts
    uint32_t moveCounts[6]      = {};   // 0x1C: per-move list sizes (= motbin move count)
    uint64_t poolOffsets[6]     = {};   // 0x38
    uint64_t moveListOffsets[6] = {};   // 0x68

    std::vector<AnmbinEntry>  pool[6];      // unique animation entries (from 0x38)
    std::vector<uint32_t>     moveList[6];  // per-move hash refs (from 0x68, one per move)
};

AnmbinData LoadAnmbin(const std::string& filePath);
