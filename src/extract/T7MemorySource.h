#pragma once
#include "GameProcess.h"
#include <cstdint>
#include <string>
#include <vector>

// -------------------------------------------------------------
//  Abstract byte source for T7 moveset parsing (live process or dump).
// -------------------------------------------------------------
struct T7MemorySource {
    virtual ~T7MemorySource() = default;
    virtual bool Read(uintptr_t addr, void* buf, size_t size) const = 0;

    template<typename T>
    bool ReadValue(uintptr_t addr, T& out) const
    {
        return Read(addr, &out, sizeof(T));
    }
};

// Live game process.
struct T7ProcessMemorySource : T7MemorySource {
    explicit T7ProcessMemorySource(const GameProcessInfo& proc) : m_proc(proc) {}
    bool Read(uintptr_t addr, void* buf, size_t size) const override
    {
        return ReadGameMemory(m_proc, addr, buf, size);
    }
private:
    const GameProcessInfo& m_proc;
};

// -------------------------------------------------------------
//  Offline T7DUMP01 file produced by res-wip/dump_t7_motbin.py
// -------------------------------------------------------------
struct T7DumpFile : T7MemorySource {
    bool Load(const std::string& binPath, std::string& errorMsg);
    bool Read(uintptr_t addr, void* buf, size_t size) const override;

    uint32_t    slot = 0;
    uint32_t    fighterId = 0xFFFF;
    uint32_t    moveCount = 0;
    uint64_t    encodedCharId = 0;
    uintptr_t   moduleBase = 0;
    uintptr_t   movesetBase = 0;
    std::string characterName;
    std::string path;

private:
    struct Region {
        uintptr_t absAddr = 0;
        size_t    size = 0;
        size_t    fileOffset = 0; // into m_bytes
    };
    std::vector<uint8_t> m_bytes;
    std::vector<Region>  m_regions;
};
