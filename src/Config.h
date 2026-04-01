#pragma once
#include <string>

struct AppConfig {
    std::string movesetRootDir;
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
