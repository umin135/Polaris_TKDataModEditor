# Creating Custom Movesets with Polaris TKData Mod Editor

This tutorial walks you through creating and editing custom movesets for Tekken 8 using the Polaris TKData Mod Editor. By the end, you will understand the moveset data format, how to modify individual moves, and how to build entirely new attacks from scratch.

---

## Table of Contents

1. [Prerequisites](#prerequisites)
2. [Understanding the Moveset Format](#understanding-the-moveset-format)
3. [Getting Started: Extracting a Moveset](#getting-started-extracting-a-moveset)
4. [Anatomy of a Move](#anatomy-of-a-move)
5. [Editing an Existing Move](#editing-an-existing-move)
6. [Creating a New Move from Scratch](#creating-a-new-move-from-scratch)
7. [Setting Up Cancels (Move Links)](#setting-up-cancels-move-links)
8. [Configuring Hit Conditions and Damage](#configuring-hit-conditions-and-damage)
9. [Working with Extra Properties](#working-with-extra-properties)
10. [Input Commands Reference](#input-commands-reference)
11. [Requirements Reference](#requirements-reference)
12. [Saving and Testing](#saving-and-testing)
13. [Tips and Common Pitfalls](#tips-and-common-pitfalls)

---

## Prerequisites

- Polaris TKData Mod Editor (built from this repository)
- Tekken 8 installed on PC
- A basic understanding of Tekken frame data terminology (startup, recovery, hit levels, etc.)
- An extracted moveset to work with (`.motbin` file)

---

## Understanding the Moveset Format

Every character's moveset in Tekken 8 is stored in a binary `.motbin` file. This file contains:


| Data Block           | Description                                                             |
| -------------------- | ----------------------------------------------------------------------- |
| **Moves**            | The core move list — every action a character can perform               |
| **Cancels**          | Rules for transitioning between moves (input cancels, auto-transitions) |
| **Hit Conditions**   | Damage values and what happens the attack connects (hit/block)          |
| **Reaction Lists**   | Knockback directions, hit reactions, and stun animations                |
| **Pushbacks**        | Displacement data — how far moves push on hit/block                     |
| **Requirements**     | Conditional logic (e.g., "only on hit", "only when airborne")           |
| **Extra Properties** | Timed effects during a move (VFX, camera shake, state changes)          |
| **Input Sequences**  | Multi-input command definitions (e.g., qcf, hcb motions)                |
| **Voiceclips**       | Sound effects and voice lines tied to moves                             |
| **Projectiles**      | Projectile behavior and hit conditions                                  |
| **Throws**           | Throw mechanics (side, camera, probability)                             |
| **Dialogues**        | Dialogue triggers and facial animations                                 |


Each move references entries in these shared data blocks by index. For example, a move points to cancel block index 42, which contains the list of cancels for that move.

---

## Getting Started: Extracting a Moveset

1. Launch Tekken 8 and load into Practice Mode with the character you want to mod.
2. Open Polaris TKData Mod Editor.
3. Use the **Extractor** to pull the moveset from the running game:
  - Select the player slot (P1 or P2).
  - Click **Extract**. The tool reads the moveset directly from game memory.
4. The extracted moveset is saved as a folder containing:
  - `moveset.motbin` — the move data
  - `moveset.anmbin` — the animation data
  - `moveset.ini` — metadata (character code, etc.)

You can now open this folder in the editor.

---

## Anatomy of a Move

Each move is a 0x448-byte structure. Here are the key fields you will work with:

### Identity


| Field        | Description                                                                            |
| ------------ | -------------------------------------------------------------------------------------- |
| `name_key`   | Index into the move name database — determines the move's display name. Always Unique. |
| `anim_key`   | Index into the animation database — determines which animation plays                   |
| `ordinal_id` | Unique global identifier                                                               |


### Frame Data


| Field            | Description                                            |
| ---------------- | ------------------------------------------------------ |
| `anim_len`       | Total length of the animation in frames                |
| `startup`        | Number of frames before the move becomes active        |
| `recovery`       | The last frame after which the move stops being active |
| `airborne_start` | Frame where the character becomes airborne (0 = never) |
| `airborne_end`   | Frame where the character lands                        |
| `ground_fall`    | Frame where the character becomes "grounded" again     |


### Hit Properties


| Field       | Description                                                                           |
| ----------- | ------------------------------------------------------------------------------------- |
| `vuln`      | Vulnerability / Hurtbox state — defines how the character can be hit during this move |
| `hitlevel`  | The hit level of the attack (high, mid, low, etc.)                                    |
| `collision` | Collision state identifier                                                            |
| `distance`  | Distance travelled when the move has airborne status                                  |


### Hitboxes (8 slots)

Each move supports up to **8 independent hitboxes**. Per hitbox:


| Field                    | Description                                                 |
| ------------------------ | ----------------------------------------------------------- |
| `hitbox_active_start[i]` | Frame when hitbox `i` activates                             |
| `hitbox_active_last[i]`  | Frame when hitbox `i` deactivates                           |
| `hitbox_location[i]`     | Body part / location of the hitbox                          |
| `hitbox_floats[i][0..8]` | 9 float parameters controlling position, size, and behavior |


### Block References

These fields link to entries in the shared data blocks:


| Field               | Description                                                                |
| ------------------- | -------------------------------------------------------------------------- |
| `cancel_idx`        | Index into the Cancel block — defines what moves this move can cancel into |
| `hit_condition_idx` | Index into the Hit Condition block — defines damage and reactions          |
| `extra_prop_idx`    | Index into the Extra Properties block — timed effects                      |
| `start_prop_idx`    | Index into the Start Properties block — effects at move start              |
| `end_prop_idx`      | Index into the End Properties block — effects at move end                  |
| `voiceclip_idx`     | Index into the Voiceclip block — sound effects                             |
| `transition`        | Move ID to automatically transition to when this move finishes             |


---

## Editing an Existing Move

The simplest way to start modding is to modify an existing move.

### Example: Changing a Move's Frame Data

1. Open the moveset in the editor.
2. Find the move in the **Move List** sidebar (use the search bar).
3. Select the move to open its properties panel.
4. Modify the frame data fields:
  - **startup**: If a move has startup value of 15 that means it's an i15 move, changing to 12 would make it i12.
  - **recovery**: Increasing this value would increase the active hitbox duration. E.g, EWGF has active hitbox b/w frames 11 & 12. If you change this value to 13, then the active frames become 11-13.
  - **anim_len**: Adjust the total animation length. Be careful — this should be >= startup + active frames + recovery.
  - **last cancel**: The last cancel of a move determines when it transitions back to the idle stance
5. Modify hitbox timing:
  - `hitbox_active_start[0]`: Set to the frame your attack should start hitting.
  - `hitbox_active_last[0]`: Set to the last active frame.
  - Example: For a move with 12f startup, set `active_start = 12` and `active_last = 14` for a 3-frame active window.

### Example: Changing Damage

1. Find the move's `hit_condition_idx`.
2. Open the **Hit Conditions** window.
3. Navigate to that index.
4. Change the `damage` field to your desired value.
5. Each hit condition entry also has a `requirement` that determines when it applies (on hit, on block, on counter hit, etc.).

### Example: Changing Hit Level

Modify the `hitlevel` field on the move. Common values:


| Value        | Hit Level   |
| ------------ | ----------- |
| 1298 (0x512) | High        |
| 535 (0x217)  | Mid         |
| 271 (0x10F)  | Low         |
| 1055 (0x41F) | Special Mid |


---

## Creating a New Move from Scratch

Building a move from nothing requires setting up several interconnected pieces.

### Step 1: Create the Move Entry

The editor allows you to add a new move to the move list. The new move starts with default/zeroed values. You need to fill in:

1. **name_key** — Assign a name index. You can reuse an existing name or add a new entry.
2. **anim_key** — Point to an animation. You can reuse an animation from another move or import a custom one via the Animation Manager.
3. **transition** — Set the move ID to transition to when the animation completes (usually a standing idle move).

### Step 2: Set Frame Data

Define the timing of your move:

```
anim_len   = 40     (total frames)
startup    = 14     (active on frame 14)
recovery   = 18     (18 frames of recovery after active window)
```

### Step 3: Configure Hitboxes

Set up at least one hitbox:

```
hitbox_active_start[0] = 14    (matches startup)
hitbox_active_last[0]  = 16    (3-frame active window)
hitbox_location[0]     = <body part ID>
hitbox_floats[0][...]  = <position and size parameters>
```

A practical approach: copy the hitbox float values from a similar existing move, then adjust.

### Step 4: Set Hit Properties

```
vuln      = <vulnerability state>
hitlevel  = 1       (mid attack)
collision = <collision type>
distance  = <range>
```

### Step 5: Create Cancels

Each move must have a cancel list with atleast one cancel – with the command value `0x8000` which determines when this move will transition back to a generic move. In the steps below, we're assuming you've created a 1-hit move that goes back to neutral / idle stance.

1. Create a new cancel list entry
2. Set the command to `0x8000` as that's used to denote an "End of List."
3. Set the extradata idx and the requirement index to 0.
4. Set the `Frame Window End` value to 32767.
5. Set the `Starting Frame` and `Frame Window Start` values to the frame at which the move will cancel back to idle. E.g, EWGF is 50 frames in total but if you check the last cancel, it cancels back to neutral at frame # 36
6. Set the move ID to 32769, which is the ID for the idle stance.
7. Set the `Cancel Option` field value to 336 – That's always the value set for EoL cancels.

### Step 6: Create Hit Conditions

Add entries to the Hit Condition block:

1. Create a new hit condition entry.
2. Set `damage` (e.g., 20 for a standard mid).
3. Link a **Requirement** that defines when this applies:
  - Requirement `724` = "Tornado Available / The Opponent is Airborne and Tornado is available"
  - Requirement `723` = "Juggle / The Opponent is Airborne"
  - Requirement `1100` = "End Of List – Hit Reaction on non-airborne opponents"
4. Link a **Reaction List** for knockback behavior.
5. Set the move's `hit_condition_idx` to point to your new entry.

Terminate the hit condition list with an entry where `requirement = 1100` (End-of-List).

### Step 7: Add Extra Properties (Optional)

Add visual flair with extra properties:

- Camera shake on hit
- VFX effects (sparks, fire, electric)
- Sound cues
- State Updates (mark tornado as consumed, enable Devil mode, activate install etc...)

---

## Setting Up Cancels (Move Links)

Cancels are the backbone of a moveset's flow. They define when and how a player can transition from one move to another.

### Cancel Structure

Each cancel entry has:


| Field                | Description                                     |
| -------------------- | ----------------------------------------------- |
| `command`            | The input required (direction + button bitmask) |
| `move_id`            | The destination move index                      |
| `frame_window_start` | First frame the cancel input is accepted        |
| `frame_window_end`   | Last frame the cancel input is accepted         |
| `starting_frame`     | Frame when the cancel becomes available         |
| `cancel_option`      | Flags / behavior modifiers                      |
| `requirement_addr`   | Conditional requirements (optional)             |


### Example: Adding a Simple Cancel

To let your move cancel into move #150 when the player presses `f+RP` (forward + right punch) between frames 10 and 25:

```
command            = <bitmask for f+RP>
move_id            = 150
frame_window_start = 10
frame_window_end   = 25
starting_frame     = 10
cancel_option      = 0
```

### Cancel List Terminator

Every cancel list must end with a terminator entry where `command = 0x8000`.

### Common Cancel Patterns

- **String continuation**: Cancel on a specific button press within a narrow frame window.
- **Auto-transition**: Set `command = 0` with a wide frame window to auto-cancel at a specific frame.
- **On-hit only cancel**: Attach a Requirement with `req = 43 or 44` (On Hit) to the cancel entry.

---

## Configuring Hit Conditions and Damage

Hit conditions define what happens when a move connects.

### Hit Condition Chain

A move's hit condition list is processed in order. Each entry has:

1. **Requirements** — Conditions that must be met (hit, block, CH, etc.)
2. **Damage** — The damage dealt when this condition matches
3. **Reaction List** — The knockback and stun behavior

### Reaction Lists

Each reaction list defines knockback for 7 directions:


| Index | Direction           |
| ----- | ------------------- |
| 0     | Front               |
| 1     | Back                |
| 2     | Left side           |
| 3     | Right side          |
| 4     | Front (counter hit) |
| 5     | Downed              |


Each direction links to a **Pushback** entry that controls displacement distance and velocity.

The reaction list also specifies which reaction move the opponent plays for each situation:

- `standing`, `crouch` — normal hit
- `ch`, `crouch_ch` — counter hit
- `left_side`, `right_side`, `left_side_crouch`, `right_side_crouch` — side hits
- `back`, `back_crouch` — back hit
- `block`, `crouch_block` — blocked
- `wallslump` — wall collision
- `downed` — ground hit

---

## Working with Extra Properties

Extra properties are timed effects that activate during a move's animation.

### Structure


| Field                    | Description                                                                           |
| ------------------------ | ------------------------------------------------------------------------------------- |
| `type`                   | The frame this property activates on                                                  |
| `id`                     | Property ID (see reference below)                                                     |
| `value` through `value5` | Up to 5 parameters (interpreted as uint32, int32, or float depending on the property) |
| `requirement_addr`       | Optional condition for this property to activate                                      |


#### Note:

- Extraprops and Requirements, both can be used within the requirement lists, both are Integer ENUMs and the only difference between those 2 is that Extraprop ENUMs are always >= 0x8000.
- `type`, which is the "frame" property has some interesting patterns:
  - Value `0x4AAA` means to trigger the property every 4th frame starting from frame `AAA` – E.g, 0x4005 would mean every 4th frame after frame # 5
  - Value `0x8AAA` means to trigger the property on frame `AAA` after this move starts. We don't always transition to other moves on the 1st frame, if we go from Move A to Move B on 5th frame, the property on frame 1 will be skipped. To handle that issue, the frame value on that move will be `0x8001`

### Common Property IDs


| ID       | Effect                     |
| -------- | -------------------------- |
| `0x8001` | Lightest camera shake      |
| `0x8002` | Light camera shake         |
| `0x8003` | Moderate camera shake      |
| `0x8004` | Heavy camera shake         |
| `0x800b` | Ground ripple VFX          |
| `0x8011` | Fire burn VFX (self)       |
| `0x8012` | Fire burn VFX (opponent)   |
| `0x8019` | Electric VFX (self)        |
| `0x801a` | Electric VFX (opponent)    |
| `0x801b` | Heat smash VFX (self)      |
| `0x8039` | Character trail VFX (self) |
| `0x8049` | Enable homing VFX          |
| `0x8087` | Deal permanent self-damage |
| `0x8088` | Heal permanent health      |


### List Terminator

Extra property lists must end with an entry where `id = 0x0` (Properties end).

### Start/End Properties

- **Start properties** (`start_prop_idx`) fire once when the move begins. These do NOT have a `type`/frame field — they activate immediately.
- **End properties** (`end_prop_idx`) fire once when the move ends. Same format as start properties.
- **Extra properties** (`extra_prop_idx`) fire at the specified frame during the animation.

---

## Input Commands Reference

Inputs are encoded as a 64-bit bitmask: the lower 32 bits represent directional input and the upper 32 bits represent button presses.

### Directional Notation


| Hex     | Direction   |
| ------- | ----------- |
| `0x04`  | d           |
| `0x08`  | d/f         |
| `0x10`  | b           |
| `0x20`  | N (neutral) |
| `0x40`  | f           |
| `0x80`  | u/b         |
| `0x100` | u           |
| `0x200` | u/f         |
| `0x02`  | d/b         |


These values can be OR'd together to accept multiple directions (e.g., `0x04 | 0x08 = 0x0C` accepts both d and d/f).

### Button Notation

Buttons are stored in the upper 32 bits. Common button masks:


| Button | Meaning         |
| ------ | --------------- |
| LP     | Left Punch (1)  |
| RP     | Right Punch (2) |
| LK     | Left Kick (3)   |
| RK     | Right Kick (4)  |


Refer to `data/interfacedata/editorCommands.txt` for the complete 512-entry command lookup table.

---

## Requirements Reference

Requirements are conditional checks used by cancels, hit conditions, and extra properties.

### Common Requirement IDs


|   ID    | Description                                 |
|:-------:|:--------------------------------------------|
| `0`     | Always true (no condition)                  |
| `35`    | Opponent distance ≤ param                   |
| `36`    | Opponent distance ≥ param                   |
| `43`    | On hit                                      |
| `44`    | On hit (alternate)                          |
| `47`    | On block                                    |
| `48`    | On whiff                                    |
| `49`    | On hit or block                             |
| `51`    | On hit                                      |
| `723`   | Opponent is airborne                        |
| `724`   | Tornado available                           |
| `1100`  | **List terminator** — end of requirement list |


### Requirement Parameters

Each requirement has up to 4 parameters (`param`, `param2`, `param3`, `param4`). Their meaning depends on the requirement ID. For example, requirement 35 (distance check) uses `param` as the distance threshold.

Refer to `data/interfacedata/editorRequirements.txt` for the full 450+ requirement list.

---

## Saving and Testing

### Saving

1. After making your changes, use the editor's **Save** function.
2. The editor converts your modified data back to `.motbin` format:
  - Re-encrypts fields (name_key, anim_key, vuln, hitlevel, ordinal_id)
  - Rebuilds all data blocks
  - Converts from file-offset format back to index format
3. The saved file is written to the same folder you loaded from.

### Testing In-Game

1. Launch Tekken 8 and enter Practice Mode.
2. Use the editor's live features to load your modified moveset into the running game.
3. Test your changes:
  - Verify frame data behaves as expected.
  - Check that cancels work with the right inputs and timing.
  - Confirm damage values, hit reactions, and VFX.

### Iteration Tips

- Make small changes and test frequently.
- Keep backups of your original moveset files before making edits.
- Use an existing similar move as a template when creating new moves.

---

## Tips and Common Pitfalls

### Do

- **Always terminate lists.** Cancel lists end with `move_id = 0x8000`. Requirement lists end with `req = 1100`. Extra property lists end with `id = 0x0`. Missing terminators will cause crashes.
- **Copy from existing moves.** The safest way to create something new is to duplicate a similar existing move and modify it.
- **Match hitbox timing to startup.** Your `hitbox_active_start` should align with `startup`. If startup is 14, the first active hitbox frame should be 14.
- **Keep anim_len consistent.** The total animation length should accommodate startup + active frames + recovery.
- **Back up your files.** Always keep copies of the original `.motbin` and `.anmbin` before editing.

### Don't

- **Don't leave dangling references.** If a move points to cancel index 42, make sure index 42 actually contains valid cancel data.
- **Don't forget the encryption system.** Fields like `name_key`, `anim_key`, `vuln`, `hitlevel`, and `ordinal_id` are encrypted with XOR keys. The editor handles this automatically — never edit encrypted field values directly.
- **Don't exceed 8 hitboxes per move.** The format supports exactly 8 hitbox slots. You cannot add more.
- **Don't set conflicting frame data.** For example, a move with `startup = 20` but `anim_len = 15` will not work correctly.
- **Don't ignore the Generic Move system.** The header contains 60 "original alias" entries that map generic move IDs (0x8000+n) to real move indices. Breaking these mappings can cause fallback moves to stop working.

### Debugging Checklist

If your modded move isn't working:

1. Is the cancel list properly terminated with `move_id = 0x8000`?
2. Do all requirement lists end with `req = 1100`?
3. Do all extra property lists end with `id = 0x0`?
4. Does the `hit_condition_idx` point to valid data?
5. Is the `transition` move ID valid?
6. Are hitbox frames within the range of `anim_len`?
7. Is the animation (`anim_key`) valid and present in the `.anmbin`?

---

## Further Reference

- `data/interfacedata/editorCommands.txt` — Full input command bitmask lookup (512 entries)
- `data/interfacedata/editorRequirements.txt` — All requirement IDs and descriptions (450+ entries)
- `data/interfacedata/editorProperties.txt` — All extra property IDs and descriptions (300+ entries)
- `src/moveset/data/MotbinData.h` — C++ struct definitions for all data types
- `src/moveset/data/EditorFieldLabel.h` — Field display names used in the editor UI
- `src/moveset/labels/FieldTooltips.h` — Tooltip text for editor fields

