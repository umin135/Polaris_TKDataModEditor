<img src="https://raw.githubusercontent.com/umin135/Polaris_TKDataModEditor/refs/heads/main/readme/banner_Editor.png" /><br>
# Polaris_TKDataModEditor
wip....

## Recovering from malformed customization-item mods

If a malformed `customize_item_*.bin` mod item was equipped in-game, the game may keep referencing that item from save data even after uninstalling the mod.

Recommended recovery flow:
Before anything: Disable Steam Cloud sync for Tekken 8 (Properties → General → uncheck "Keep game saves in the Steam Cloud"). Otherwise Steam re-downloads corrupted files.

Then, with game closed, delete these files:

- customize_save_data_0_1.sav
- customize_save_data_1_1.sav
- customize_save_data_2_1.sav
- customize_save_data_3_1.sav
- customize_save_data_4_1.sav
- global1.sav

global1.sav stores the unlock bitfield + equipped item references. customize_savedata* store the actual slot data
with the corrupted hachimaki ID. Game will recreate both fresh on next boot.

You will lose: game settings, equipped customizations, some local unlock state. You will NOT lose: online rank,
purchased items (server-side), replays, ghosts
