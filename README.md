<img src="https://raw.githubusercontent.com/umin135/Polaris_TKDataModEditor/refs/heads/main/readme/banner_Editor.png" /><br>
# Polaris_TKDataModEditor
wip....

## Recovering from malformed customization-item mods

If a malformed `customize_item_*.bin` mod item was equipped in-game, the game may keep referencing that item from save data even after uninstalling the mod.

Recommended recovery flow:
1. Re-enable the mod that contains the equipped item.
2. Enter customization and switch every equipped modded part back to a valid vanilla item.
3. Save, close the game, then remove the mod again.
4. If the error still persists, restore a backup save from before the malformed item was equipped (or delete the affected save/profile data and start from a clean save).
