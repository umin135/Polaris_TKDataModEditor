// GameProcess.cpp -- Win32 process read utilities
#include "GameProcess.h"
#include <tlhelp32.h>
#include <psapi.h>
#include <cstring>
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
        PROCESS_VM_READ | PROCESS_QUERY_INFORMATION,
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
