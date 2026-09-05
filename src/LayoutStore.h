#pragma once
// -------------------------------------------------------------
//  LayoutStore -- persists editor sub-window sizes and per-section (splitter) widths so the
//  layout is remembered across window close / app restart. Values are keyed by a stable,
//  instance-independent string (e.g. "win.Requirements.w", "sec.req_outer"). Backed by a plain
//  key=value file at res/layout.ini. Populated in-memory every frame during rendering; the file
//  is written on Save() (called when an editor window closes and on app shutdown).
// -------------------------------------------------------------
#include <string>
#include <map>
#include "imgui/imgui.h"

class LayoutStore {
public:
    static LayoutStore& Get();

    float Get(const std::string& key, float def) const {
        auto it = m_vals.find(key);
        return it == m_vals.end() ? def : it->second;
    }
    void Set(const std::string& key, float v) { m_vals[key] = v; }

    // Window size helpers (keys: "win.<name>.w" / ".h").
    ImVec2 GetWin(const char* name, float dw, float dh) const {
        return ImVec2(Get(std::string("win.") + name + ".w", dw),
                      Get(std::string("win.") + name + ".h", dh));
    }
    void SetWin(const char* name, ImVec2 sz) {
        Set(std::string("win.") + name + ".w", sz.x);
        Set(std::string("win.") + name + ".h", sz.y);
    }

    // Section (splitter) width helpers (key: "sec.<name>").
    float GetSec(const std::string& name, float def) const { return Get("sec." + name, def); }
    void  SetSec(const std::string& name, float v)         { Set("sec." + name, v); }

    void Load();
    void Save() const;

private:
    LayoutStore() = default;
    std::map<std::string, float> m_vals;
    static std::string FilePath();
};
