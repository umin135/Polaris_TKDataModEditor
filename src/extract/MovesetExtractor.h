#pragma once
#include "GameProcess.h"
#include <string>
#include <vector>

struct MotbinNameData;  // defined in MotbinSerialize.h

// -------------------------------------------------------------
//  Player slot info (resolved from game memory)
// -------------------------------------------------------------
struct PlayerSlotInfo {
    bool      valid        = false;
    int       slotIndex    = -1;    // 0 = P1, 1 = P2
    uint32_t  charaId      = 0xFFFF;
    uintptr_t playerAddr   = 0;
    uintptr_t motbinAddr   = 0;
    uint32_t  moveCount    = 0;
    std::string charaName;
};

// -------------------------------------------------------------
//  MovesetExtractor
//  Reads moveset data from the running game and saves .motbin.
//
//  Pointer chain (TK8):
//    moduleBase + 0x9B7A950  -> P1 player ptr
//    playerAddr + 0x38C8     -> motbin ptr
//    playerAddr + 0x168      -> character_id (uint32)
//
//  Extraction saves state-3 (populated, absolute pointers) raw dump.
//  Original base address is written to moveset.base so the editor can
//  apply fixup at load time. The loader receives the raw file as-is.
// -------------------------------------------------------------
class MovesetExtractor {
public:
    MovesetExtractor();

    // Try to connect to the running game.
    // Returns true if the game process is found.
    bool Connect();
    void Disconnect();

    bool IsConnected() const { return m_proc.valid; }

    // Scan P1 / P2 slots from game memory. Call after Connect().
    // Fills m_slots[0] and m_slots[1].
    void RefreshSlots();

    // Extract the moveset for the given slot and save it to
    // "<destFolder>/<charaName>/moveset.motbin".
    // Returns true on success; errorMsg is set on failure.
    bool ExtractToFile(int slotIndex,
                       const std::string& destFolder,
                       std::string& errorMsg);

    const PlayerSlotInfo& GetSlot(int i) const { return m_slots[i]; }
    const std::string& GetStatusMsg() const     { return m_statusMsg; }

private:
    bool ReadSlot(int slotIndex, PlayerSlotInfo& slot);
    bool ReadMotbin(uintptr_t motbinAddr,
                    std::vector<uint8_t>& outBytes,
                    std::string& errorMsg);
    bool SaveMotbin(const std::vector<uint8_t>& bytes,
                    const std::string& destFolder,
                    uint32_t charaId,
                    const std::string& charaName,
                    uintptr_t motbinBase,
                    const MotbinNameData* names,
                    const std::string& gameVersion,
                    std::string& errorMsg);

    std::string ReadGameVersion();

    GameProcessInfo m_proc;
    PlayerSlotInfo  m_slots[2];
    std::string     m_statusMsg;
};
