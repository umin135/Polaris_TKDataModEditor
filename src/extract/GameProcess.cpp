// GameProcess.cpp -- Win32 process read utilities
#include "GameProcess.h"
#include <tlhelp32.h>
#include <psapi.h>
#include <cstring>
#include <vector>
#pragma comment(lib, "psapi.lib")

static constexpr wchar_t kProcessName[] = L"Polaris-Win64-Shipping.exe";

// -------------------------------------------------------------
//  FindGameProcess
// -------------------------------------------------------------

bool FindGameProcess(GameProcessInfo& out)
{
    out = {};

    // Snapshot of all running processes
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return false;

    PROCESSENTRY32W pe = {};
    pe.dwSize = sizeof(pe);

    DWORD foundPid = 0;
    if (Process32FirstW(snap, &pe))
    {
        do {
            if (_wcsicmp(pe.szExeFile, kProcessName) == 0)
            {
                foundPid = pe.th32ProcessID;
                break;
            }
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);

    if (foundPid == 0) return false;

    HANDLE hProc = OpenProcess(
        PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_VM_OPERATION | PROCESS_QUERY_INFORMATION,
        FALSE, foundPid);
    if (hProc == nullptr || hProc == INVALID_HANDLE_VALUE) return false;

    // Find the main module (.exe) base address
    HMODULE modules[1024] = {};
    DWORD   cbNeeded = 0;
    if (!EnumProcessModules(hProc, modules, sizeof(modules), &cbNeeded))
    {
        CloseHandle(hProc);
        return false;
    }

    uintptr_t mainBase = 0;
    uintptr_t mainSize = 0;
    const DWORD count = cbNeeded / sizeof(HMODULE);

    for (DWORD i = 0; i < count; ++i)
    {
        wchar_t name[MAX_PATH] = {};
        GetModuleFileNameExW(hProc, modules[i], name, MAX_PATH);
        if (_wcsicmp(name + (wcslen(name) > 4 ? wcslen(name) - 4 : 0),
                     L".exe") != 0)
            continue;
        // Use first .exe module (the main executable)
        MODULEINFO mi = {};
        if (GetModuleInformation(hProc, modules[i], &mi, sizeof(mi)))
        {
            mainBase = reinterpret_cast<uintptr_t>(mi.lpBaseOfDll);
            mainSize = mi.SizeOfImage;
        }
        break;
    }

    if (mainBase == 0)
    {
        CloseHandle(hProc);
        return false;
    }

    out.handle     = hProc;
    out.pid        = foundPid;
    out.moduleBase = mainBase;
    out.moduleSize = mainSize;
    out.valid      = true;
    return true;
}

// -------------------------------------------------------------

void CloseGameProcess(GameProcessInfo& info)
{
    if (info.handle != INVALID_HANDLE_VALUE && info.handle != nullptr)
    {
        CloseHandle(info.handle);
        info.handle = INVALID_HANDLE_VALUE;
    }
    info.valid = false;
}

bool ReadGameMemory(const GameProcessInfo& info, uintptr_t addr, void* buf, size_t size)
{
    if (!info.valid || size == 0) return false;
    SIZE_T bytesRead = 0;
    if (!ReadProcessMemory(info.handle,
                           reinterpret_cast<LPCVOID>(addr),
                           buf, size, &bytesRead))
        return false;
    return bytesRead == size;
}

bool ReadGamePointer(const GameProcessInfo& info, uintptr_t ptrAddr, uintptr_t& out)
{
    uint64_t val = 0;
    if (!ReadGameValue(info, ptrAddr, val)) return false;
    out = static_cast<uintptr_t>(val);
    return true;
}

bool WriteGameMemory(const GameProcessInfo& info, uintptr_t addr, const void* buf, size_t size)
{
    if (!info.valid || size == 0) return false;
    SIZE_T bytesWritten = 0;
    if (!WriteProcessMemory(info.handle,
                            reinterpret_cast<LPVOID>(addr),
                            buf, size, &bytesWritten))
        return false;
    return bytesWritten == size;
}

bool ReadPointerChain(const GameProcessInfo& info,
                      uintptr_t base,
                      const size_t* offsets,
                      int count,
                      uintptr_t& out)
{
    uintptr_t cur = base;
    for (int i = 0; i < count; ++i)
    {
        if (!ReadGamePointer(info, cur + offsets[i], cur)) return false;
        if (cur == 0) return false;
    }
    out = cur;
    return true;
}

uintptr_t AobScan(const GameProcessInfo& info,
                  const char* pattern,
                  uintptr_t startAddr,
                  uintptr_t endAddr)
{
    if (!info.valid || !pattern || startAddr >= endAddr) return 0;

    // Parse "XX XX ?? XX ..." into byte / wildcard list
    std::vector<int> pat; // -1 = wildcard, 0-255 = exact byte
    for (const char* p = pattern; *p; )
    {
        while (*p == ' ') ++p;
        if (!*p) break;
        if (p[0] == '?' && p[1] == '?')
        {
            pat.push_back(-1);
            p += 2;
        }
        else
        {
            char buf[3] = { p[0], p[1], '\0' };
            pat.push_back((int)strtol(buf, nullptr, 16));
            p += 2;
        }
    }
    if (pat.empty()) return 0;

    const size_t patLen   = pat.size();
    const size_t kChunk   = 0x10000; // 64 KB
    std::vector<uint8_t> chunk(kChunk + patLen);

    uintptr_t cur = startAddr;
    while (cur < endAddr)
    {
        // Overlap by (patLen-1) to catch matches that span chunk boundaries
        if (cur != startAddr) cur -= (patLen - 1);

        const size_t readSize = (size_t)min((uintptr_t)kChunk, endAddr - cur);
        SIZE_T got = 0;
        if (!ReadProcessMemory(info.handle,
                               reinterpret_cast<LPCVOID>(cur),
                               chunk.data(), readSize, &got) || got == 0)
        {
            cur += readSize + (patLen - 1);
            continue;
        }

        for (size_t i = 0; i + patLen <= got; ++i)
        {
            bool match = true;
            for (size_t j = 0; j < patLen; ++j)
            {
                if (pat[j] != -1 && chunk[i + j] != (uint8_t)pat[j])
                {
                    match = false;
                    break;
                }
            }
            if (match) return cur + i;
        }
        cur += readSize;
    }
    return 0;
}
