// AnmbinData.cpp -- LoadAnmbin implementation
#include "AnmbinData.h"
#include <cstdio>
#include <cstring>

static bool SeekAndRead(FILE* f, long offset, void* dst, size_t bytes, const char* what, AnmbinData& err)
{
    if (fseek(f, offset, SEEK_SET) != 0 || fread(dst, bytes, 1, f) != 1)
    {
        err.errorMsg = std::string("Read failed: ") + what;
        return false;
    }
    return true;
}

AnmbinData LoadAnmbin(const std::string& filePath)
{
    AnmbinData result;
    result.filePath = filePath;

    FILE* f = nullptr;
    if (fopen_s(&f, filePath.c_str(), "rb") != 0 || !f)
    {
        result.errorMsg = "Cannot open file: " + filePath;
        return result;
    }

    // -- poolCounts[6] at 0x04 ----------------------------------------
    if (!SeekAndRead(f, 0x04, result.poolCounts, sizeof(result.poolCounts), "poolCounts", result))
        { fclose(f); return result; }

    // -- moveCounts[6] at 0x1C ----------------------------------------
    if (!SeekAndRead(f, 0x1C, result.moveCounts, sizeof(result.moveCounts), "moveCounts", result))
        { fclose(f); return result; }

    // -- poolOffsets[6] at 0x38 ---------------------------------------
    if (!SeekAndRead(f, 0x38, result.poolOffsets, sizeof(result.poolOffsets), "poolOffsets", result))
        { fclose(f); return result; }

    // -- moveListOffsets[6] at 0x68 -----------------------------------
    if (!SeekAndRead(f, 0x68, result.moveListOffsets, sizeof(result.moveListOffsets), "moveListOffsets", result))
        { fclose(f); return result; }

    // -- pool entries (stride 0x38, from poolOffsets) -----------------
    for (int cat = 0; cat < 6; ++cat)
    {
        uint32_t count  = result.poolCounts[cat];
        uint64_t offset = result.poolOffsets[cat];
        if (count == 0 || offset == 0) continue;

        if (fseek(f, static_cast<long>(offset), SEEK_SET) != 0)
        {
            result.errorMsg = std::string("Pool seek failed: ") + AnmbinCategoryName(cat);
            fclose(f); return result;
        }

        result.pool[cat].resize(count);
        for (uint32_t i = 0; i < count; ++i)
        {
            AnmbinEntry& e = result.pool[cat][i];
            if (fread(&e.animKey,     sizeof(uint64_t), 1, f) != 1 ||
                fread(&e.animDataPtr, sizeof(uint64_t), 1, f) != 1 ||
                fread( e.extra,       0x28,             1, f) != 1)
            {
                result.errorMsg = std::string("Pool entry read failed: cat=") +
                                  AnmbinCategoryName(cat) + " idx=" + std::to_string(i);
                fclose(f); return result;
            }
        }
    }

    // -- per-move hash lists (u32 array, from moveListOffsets) --------
    for (int cat = 0; cat < 6; ++cat)
    {
        uint32_t count  = result.moveCounts[cat];
        uint64_t offset = result.moveListOffsets[cat];
        if (count == 0 || offset == 0) continue;

        if (fseek(f, static_cast<long>(offset), SEEK_SET) != 0)
        {
            result.errorMsg = std::string("MoveList seek failed: ") + AnmbinCategoryName(cat);
            fclose(f); return result;
        }

        result.moveList[cat].resize(count);
        if (fread(result.moveList[cat].data(), sizeof(uint32_t), count, f) != count)
        {
            result.errorMsg = std::string("MoveList read failed: ") + AnmbinCategoryName(cat);
            fclose(f); return result;
        }
    }

    fclose(f);
    result.loaded = true;
    return result;
}
