#pragma once
#include <string>

struct AppConfig {
    std::string gameRootDir;   // TEKKEN 8 game installation root (e.g. ...\TEKKEN 8)

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
