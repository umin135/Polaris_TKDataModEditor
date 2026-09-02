#pragma once
#include <string>

struct AppConfig {
    std::string gameRootDir;        // TEKKEN 8 game installation root (e.g. ...\TEKKEN 8)
    std::string tkmodManagerDir;    // directory scanned by the Manage tkmods window

    // Root of a ripped cinematics export tree (…\Exports\Polaris\Content\cinematics).
    // When set, moveset extraction resolves each level-sequence's real season folder
    // (polaris / polaris01 / …) and existence against this dump. Optional; empty = assume "polaris".
    std::string cinematicExportRoot;

    // Per-row list edit shortcuts, encoded as chord strings (e.g. "Ctrl+D", "Insert").
    // Index order matches ListShortcut: 0=Insert 1=Duplicate 2=Remove 3=MoveUp 4=MoveDown.
    // Empty string means "use built-in default".
    std::string listKeys[5];

    // Returns the full moveset output directory derived from gameRootDir.
    std::string MovesetDir() const {
        if (gameRootDir.empty()) return {};
        return gameRootDir + "\\Polaris\\Content\\Binary\\Mods\\Movesets";
    }
};

// Singleton config -- loaded from / saved to config.ini next to the exe.
class Config {
public:
    static Config& Get();

    void Load();
    void Save() const;

    AppConfig data;

private:
    Config() = default;
    static std::string GetConfigPath();
};
