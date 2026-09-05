// T7MemorySource.cpp — T7DUMP01 offline dump loader
#include "T7MemorySource.h"
#include <cstdio>
#include <cstring>

namespace {

constexpr char kDumpMagic[8] = { 'T','7','D','U','M','P','0','1' };
constexpr uint32_t kDumpVersion = 1;

} // namespace

bool T7DumpFile::Load(const std::string& binPath, std::string& errorMsg)
{
    *this = {};
    path = binPath;

    FILE* f = nullptr;
    if (fopen_s(&f, binPath.c_str(), "rb") != 0 || !f) {
        errorMsg = "Failed to open dump: " + binPath;
        return false;
    }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz < 120) {
        fclose(f);
        errorMsg = "Dump file too small.";
        return false;
    }
    m_bytes.resize(static_cast<size_t>(sz));
    if (fread(m_bytes.data(), 1, m_bytes.size(), f) != m_bytes.size()) {
        fclose(f);
        errorMsg = "Failed to read dump file.";
        return false;
    }
    fclose(f);

    const uint8_t* b = m_bytes.data();
    if (memcmp(b, kDumpMagic, 8) != 0) {
        errorMsg = "Not a T7DUMP01 file (bad magic).";
        return false;
    }
    uint32_t ver = 0;
    memcpy(&ver, b + 8, 4);
    if (ver != kDumpVersion) {
        errorMsg = "Unsupported T7DUMP version: " + std::to_string(ver);
        return false;
    }

    memcpy(&slot, b + 12, 4);
    memcpy(&moduleBase, b + 16, 8);
    memcpy(&movesetBase, b + 24, 8);
    memcpy(&fighterId, b + 32, 4);
    memcpy(&moveCount, b + 36, 4);
    memcpy(&encodedCharId, b + 40, 8);

    char nameBuf[65] = {};
    memcpy(nameBuf, b + 48, 64);
    characterName = nameBuf;

    uint32_t regionCount = 0;
    memcpy(&regionCount, b + 112, 4);
    if (regionCount == 0 || regionCount > 1024) {
        errorMsg = "Invalid region count in dump.";
        return false;
    }

    const size_t tableOff = 120;
    const size_t need = tableOff + static_cast<size_t>(regionCount) * 24;
    if (m_bytes.size() < need) {
        errorMsg = "Dump truncated (region table).";
        return false;
    }

    m_regions.resize(regionCount);
    for (uint32_t i = 0; i < regionCount; ++i) {
        const size_t off = tableOff + static_cast<size_t>(i) * 24;
        uint64_t absAddr = 0, size = 0, fileOff = 0;
        memcpy(&absAddr, b + off, 8);
        memcpy(&size, b + off + 8, 8);
        memcpy(&fileOff, b + off + 16, 8);
        if (size == 0 || fileOff + size > m_bytes.size()) {
            errorMsg = "Corrupt dump region " + std::to_string(i);
            m_regions.clear();
            return false;
        }
        m_regions[i].absAddr = static_cast<uintptr_t>(absAddr);
        m_regions[i].size = static_cast<size_t>(size);
        m_regions[i].fileOffset = static_cast<size_t>(fileOff);
    }

    if (movesetBase == 0) {
        errorMsg = "Dump has null moveset_base.";
        return false;
    }
    return true;
}

bool T7DumpFile::Read(uintptr_t addr, void* buf, size_t size) const
{
    if (!buf || size == 0) return false;
    uint8_t* out = static_cast<uint8_t*>(buf);
    size_t done = 0;
    while (done < size) {
        const uintptr_t cur = addr + done;
        const Region* hit = nullptr;
        for (const Region& r : m_regions) {
            if (cur >= r.absAddr && cur < r.absAddr + r.size) {
                hit = &r;
                break;
            }
        }
        if (!hit) return false;
        const size_t local = static_cast<size_t>(cur - hit->absAddr);
        const size_t avail = hit->size - local;
        const size_t take = (size - done < avail) ? (size - done) : avail;
        memcpy(out + done, m_bytes.data() + hit->fileOffset + local, take);
        done += take;
    }
    return true;
}
