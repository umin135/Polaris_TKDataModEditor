#pragma once
#include <windows.h>
#include <cstdint>
#include <string>

// -------------------------------------------------------------
//  GameProcess
//  Win32 helper to find, open, and read from a running process.
// -------------------------------------------------------------

struct GameProcessInfo {
    HANDLE      handle     = INVALID_HANDLE_VALUE;
    DWORD       pid        = 0;
    uintptr_t   moduleBase = 0;   // base address of the main .exe module
    uintptr_t   moduleSize = 0;
    bool        valid      = false;
};

// Find Polaris-Win64-Shipping.exe, open it for reading,
// and resolve its main module base address.
// Returns false if the process is not running or cannot be opened.
bool FindGameProcess(GameProcessInfo& out);

// Close the process handle. Safe to call multiple times.
void CloseGameProcess(GameProcessInfo& info);

// Read 'size' bytes from the target process at 'addr' into 'buf'.
// Returns false on error.
bool ReadGameMemory(const GameProcessInfo& info, uintptr_t addr, void* buf, size_t size);

// Typed read helper.
template<typename T>
inline bool ReadGameValue(const GameProcessInfo& info, uintptr_t addr, T& out)
{
    return ReadGameMemory(info, addr, &out, sizeof(T));
}

// Write 'size' bytes to the target process at 'addr' from 'buf'.
// Returns false on error.
bool WriteGameMemory(const GameProcessInfo& info, uintptr_t addr, const void* buf, size_t size);

// Typed write helper.
template<typename T>
inline bool WriteGameValue(const GameProcessInfo& info, uintptr_t addr, T val)
{
    return WriteGameMemory(info, addr, &val, sizeof(T));
}

// Dereference a pointer in the target process: read a uint64 at 'ptrAddr',
// then return the value it points to.
bool ReadGamePointer(const GameProcessInfo& info, uintptr_t ptrAddr, uintptr_t& out);

// Scan for a byte pattern in [startAddr, endAddr).
// Pattern format: "4C 89 35 ?? ?? ?? ??" -- '??' matches any byte.
// Returns the address of the first match, or 0 if not found.
uintptr_t AobScan(const GameProcessInfo& info,
                  const char* pattern,
                  uintptr_t startAddr,
                  uintptr_t endAddr);

// Follow a chain of pointer dereferences:
//   base -> [base + offsets[0]] -> [result + offsets[1]] -> ...
bool ReadPointerChain(const GameProcessInfo& info,
                      uintptr_t base,
                      const size_t* offsets,
                      int count,
                      uintptr_t& out);
