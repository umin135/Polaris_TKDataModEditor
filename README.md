<img src="https://raw.githubusercontent.com/umin135/Polaris_TKDataModEditor/refs/heads/main/readme/banner_Editor.png" /><br>

# Polaris TKDataModEditor

A desktop tool for editing Tekken 8 character moveset data, FBS customization files, and `.tkmod` mod archives.

Built with C++17, ImGui, and DirectX 11.

---

## Features

### Moveset Editor
- Browse and edit all moves, cancels, hit reactions, and requirements
- Extraprop editor with per-prop parameter labeling and decoded displays:
  - Hand pose props (0x860A–0x860D): pose dropdown + blend duration input
  - 0x860E (Both Hands Pose): blend-only input, hardcoded Left #1 / Right #2
  - Hand animation props (0x860F–0x8613): pool index with direct navigation
  - Throw-extras, projectile, dialogue, sound, hurtbox, rage, heat, and many more labeled
- Requirement editor with human-readable labels for common req IDs
- Kamui hash dictionary — resolve animation/move name hashes automatically

### Animation Manager & 3D Preview
- Browse fullbody and hand animation pools (anmbin)
- Real-time 3D skeleton preview with orbit camera (DirectX 11 offscreen renderer)
  - Fullbody animations: full skeleton overlay
  - Hand animations (cat 1): finger bone overlay
  - Facial / swing / camera / extra: preview not supported (shown as disabled)
- Auto-fills animation length field from binary data

### FBS Customization Editor
- Edit `.tkmod` mod archives (ZIP of FBS binary files)
- Modify customization items, character lists, and other FBS structures

### Memory Extraction
- Extract live moveset data from a running `Polaris-Win64-Shipping.exe` process (Windows only)

---

## Building

Open `PolarisTKDataEditor.vcxproj` in **Visual Studio 2022**, target **x64 Release** or **x64 Debug**, and build.

Requirements:
- Windows SDK
- `third-party/zstd/libzstd_static.lib` (included)

Pre-build: `BuildEvent/pack_meshes.py` packs the preview mesh archive. Post-build copies `data/kamui-hashes/data.json` and `data/MovesetDatas/data.json` to the output directory.

---

## Runtime Setup

Place next to the executable:
- `config.ini` — set moveset root directory
- `GameStatic.ini` — game constants, must match your installed game version

---

## Recovering from a malformed customization mod

If a malformed `customize_item_*.bin` mod was equipped in-game, the game may keep referencing it from save data even after uninstalling the mod.

**Before anything:** Disable Steam Cloud sync for Tekken 8 (Properties → General → uncheck "Keep game saves in the Steam Cloud") — otherwise Steam re-downloads the corrupted files.

With the game closed, delete these save files:

```
customize_save_data_0_1.sav
customize_save_data_1_1.sav
customize_save_data_2_1.sav
customize_save_data_3_1.sav
customize_save_data_4_1.sav
global1.sav
```

`global1.sav` stores the unlock bitfield and equipped item references. `customize_savedata*` store the slot data with the corrupted item ID. The game recreates both on next boot.

**You will lose:** game settings, equipped customizations, some local unlock state.  
**You will NOT lose:** online rank, purchased items (server-side), replays, ghosts.
