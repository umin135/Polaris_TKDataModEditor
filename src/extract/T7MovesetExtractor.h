#pragma once
#include "GameProcess.h"
#include "T7Moveset.h"
#include "T7MemorySource.h"
#include <string>
#include <vector>

struct AnmbinPanmEntry;

// -------------------------------------------------------------
//  Player slot info for T7 (mirrors T8 PlayerSlotInfo shape).
// -------------------------------------------------------------
struct T7PlayerSlotInfo {
    bool      valid        = false;
    int       slotIndex    = -1;
    uint32_t  charaId      = 0xFFFF;
    uintptr_t playerAddr   = 0;
    uintptr_t movesetAddr  = 0;
    uint32_t  moveCount    = 0;
    std::string charaName;
};

class T7MovesetExtractor {
public:
    T7MovesetExtractor() = default;

    bool Connect();
    void Disconnect();
    bool IsConnected() const { return m_proc.valid; }

    void RefreshSlots();

    // Extract slot → TK7_<name>/moveset.motbin under destFolder.
    bool ExtractToFile(int slotIndex,
                       const std::string& destFolder,
                       std::string& errorMsg);

    // Offline: T7DUMP01 .bin → same TK7_<name>/moveset.motbin path (no anims).
    bool ConvertDumpToFile(const std::string& dumpBinPath,
                           const std::string& destFolder,
                           std::string& errorMsg);

    const T7PlayerSlotInfo& GetSlot(int i) const { return m_slots[i]; }
    const std::string& GetStatusMsg() const { return m_statusMsg; }

    // Parse moveset tables from any byte source (live process or dump).
    static bool ExtractMoveset(const T7MemorySource& mem,
                               uintptr_t movesetAddr, uint32_t fighterId,
                               T7::Moveset& out, std::string& errorMsg);

private:
    bool ReadSlot(int slotIndex, T7PlayerSlotInfo& slot);
    bool WriteConvertedFolder(const T7::Moveset& t7,
                              uint32_t fighterId,
                              const std::string& charaName,
                              const std::string& destFolder,
                              std::string& errorMsg,
                              const std::vector<uint32_t>* animCrcByMove,
                              const std::vector<AnmbinPanmEntry>* uniquePanms,
                              int animOk, int animFail, int animSkip);

    GameProcessInfo  m_proc;
    T7PlayerSlotInfo m_slots[2];
    std::string      m_statusMsg;
};
