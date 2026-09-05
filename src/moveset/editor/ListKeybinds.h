#pragma once
// -------------------------------------------------------------
//  ListKeybinds -- user-customizable keyboard shortcuts for the per-row list controls
//  (insert / duplicate / remove / move-up / move-down) shared by every moveset sub-window.
//
//  Header-only (inline singleton) so both MovesetEditorWindow.cpp (the consumer) and
//  App.cpp (the Settings editor) share one instance without a separate translation unit.
//  Persistence lives in Config (a [Keybindings] section of config.ini) via Encode/Decode.
// -------------------------------------------------------------
#include <string>
#include <cstring>
#include "imgui/imgui.h"

enum class ListShortcut { Insert, Duplicate, Remove, MoveUp, MoveDown, COUNT };

struct KeyBind {
    ImGuiKey key   = ImGuiKey_None;
    bool     ctrl  = false;
    bool     shift = false;
    bool     alt   = false;
};

class ListKeybinds {
public:
    static ListKeybinds& Get() { static ListKeybinds s; return s; }

    KeyBind binds[(int)ListShortcut::COUNT];

    ListKeybinds() { SetDefaults(); }

    void SetDefaults() {
        binds[(int)ListShortcut::Insert]    = KeyBind{ ImGuiKey_Insert };
        binds[(int)ListShortcut::Duplicate] = KeyBind{ ImGuiKey_D, /*ctrl*/true };
        binds[(int)ListShortcut::Remove]    = KeyBind{ ImGuiKey_Delete };
        binds[(int)ListShortcut::MoveUp]    = KeyBind{ ImGuiKey_UpArrow,   false, false, /*alt*/true };
        binds[(int)ListShortcut::MoveDown]  = KeyBind{ ImGuiKey_DownArrow, false, false, /*alt*/true };
    }

    // True when this shortcut's chord (modifiers + key) is freshly pressed this frame.
    bool Pressed(ListShortcut sc) const {
        const KeyBind& b = binds[(int)sc];
        if (b.key == ImGuiKey_None) return false;
        ImGuiIO& io = ImGui::GetIO();
        if (b.ctrl != io.KeyCtrl)  return false;
        if (b.shift != io.KeyShift) return false;
        if (b.alt != io.KeyAlt)    return false;
        return ImGui::IsKeyPressed(b.key, /*repeat*/false);
    }

    // Human-readable chord, e.g. "Ctrl+D", "Insert", "Alt+UpArrow", or "(none)".
    std::string Encode(ListShortcut sc) const { return EncodeBind(binds[(int)sc]); }

    static std::string EncodeBind(const KeyBind& b) {
        if (b.key == ImGuiKey_None) return "(none)";
        std::string s;
        if (b.ctrl)  s += "Ctrl+";
        if (b.shift) s += "Shift+";
        if (b.alt)   s += "Alt+";
        const char* name = ImGui::GetKeyName(b.key);
        s += (name && name[0]) ? name : "?";
        return s;
    }

    // Parse a chord string produced by EncodeBind back into `binds[sc]`. Returns false on failure.
    bool Decode(ListShortcut sc, const std::string& str) {
        KeyBind b;
        std::string s = str;
        auto eat = [&](const char* tok, bool& flag) {
            size_t n = std::strlen(tok);
            if (s.size() >= n && _strnicmp(s.c_str(), tok, (int)n) == 0) { flag = true; s = s.substr(n); return true; }
            return false;
        };
        bool more = true;
        while (more) { more = false; more |= eat("Ctrl+", b.ctrl); more |= eat("Shift+", b.shift); more |= eat("Alt+", b.alt); }
        if (s.empty() || s == "(none)") { b.key = ImGuiKey_None; binds[(int)sc] = b; return true; }
        b.key = KeyFromName(s);
        if (b.key == ImGuiKey_None) return false;
        binds[(int)sc] = b;
        return true;
    }

    // Reverse of ImGui::GetKeyName: linear scan over the named-key range.
    static ImGuiKey KeyFromName(const std::string& name) {
        for (int k = ImGuiKey_NamedKey_BEGIN; k < ImGuiKey_NamedKey_END; ++k) {
            const char* n = ImGui::GetKeyName((ImGuiKey)k);
            if (n && _stricmp(n, name.c_str()) == 0) return (ImGuiKey)k;
        }
        return ImGuiKey_None;
    }

    static const char* Label(ListShortcut sc) {
        switch (sc) {
            case ListShortcut::Insert:    return "Insert item";
            case ListShortcut::Duplicate: return "Duplicate item";
            case ListShortcut::Remove:    return "Remove item";
            case ListShortcut::MoveUp:    return "Move item up";
            case ListShortcut::MoveDown:  return "Move item down";
            default:                      return "?";
        }
    }
};
