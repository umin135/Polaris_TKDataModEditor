#pragma once
#include <cstdint>
#include <string>

struct GameStaticData {
    // Cancel command special values
    uint32_t groupCancelStart  = 0x8012;  // command == this  ->  group cancel list start
    uint32_t groupCancelEnd    = 0x8013;  // command == this  ->  group cancel list end
    uint32_t inputSeqStart     = 0x8014;  // command >= this  ->  input_sequence[command - inputSeqStart]

    // Requirement list terminator
    uint32_t reqListEnd        = 1100;    // req == this  ->  list terminator

    // ExtraProp special IDs
    uint32_t extrapropDialogue1  = 0x877b;
    uint32_t extrapropDialogue2  = 0x87f8;
    uint32_t extrapropProjectile = 0x827b;
    uint32_t extrapropThrow      = 0x868f;
};

// Singleton -- loaded from GameStatic.ini next to the exe.
// Read-only at runtime; edit the file manually when the game updates.
class GameStatic {
public:
    static GameStatic& Get();

    // Load from GameStatic.ini (creates a default file if missing).
    void Load();

    GameStaticData data;

private:
    GameStatic() = default;
    static std::string GetIniPath();
};
