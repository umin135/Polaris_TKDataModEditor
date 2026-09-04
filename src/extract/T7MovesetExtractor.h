#pragma once
#include "GameProcess.h"
#include "T7Moveset.h"
#include <string>

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

    const T7PlayerSlotInfo& GetSlot(int i) const { return m_slots[i]; }
    const std::string& GetStatusMsg() const { return m_statusMsg; }

private:
    bool ReadSlot(int slotIndex, T7PlayerSlotInfo& slot);
    bool ExtractMoveset(uintptr_t movesetAddr, uint32_t fighterId,
                        T7::Moveset& out, std::string& errorMsg);

    GameProcessInfo  m_proc;
    T7PlayerSlotInfo m_slots[2];
    std::string      m_statusMsg;
};
