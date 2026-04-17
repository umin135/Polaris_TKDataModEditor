## Reading Kamui Dictionary

This document describes the public Kamui Hashes dictionary, its format, and how the TkDataEditor can use it to restore readable names from Tekken 8's hashed identifiers.

### Overview
Kamui Hash is Tekken 8's string-hashing scheme (a variant of MurmurHash3). The project hosts a public dictionary that maps numeric hash values back to human-readable names (moves, animations, voice clips, file names, etc.). The authoritative public copy is available in the Polaris TKDataModLoader repository:

- https://github.com/umin135/Polaris_TKDataModLoader

### Where Kamui Hashes are used
- Move names: `tk_move::name_key`
- Animation names: `tk_move::anim_name_key`
- Global ordinal ID of a move: `tk_move::ordinal_id`
  - Typical original format: `<3-letter-code>_<move-name>` (example: `GRL_Kz_66rp`)
- Voice clip names: `tk_dialogue::voiceclip_key`
- Filenames inside `tkdata.bin` (e.g. `mothead/bin/ant.motbin`)
- Each `tk_parryable_move` entry is a `tk_move::ordinal_id` hash.

### Public files and layout
The dictionary is published under `public-res/kamui-hashes/` and contains two files:

- `version.json` — indicates the dictionary version
- `data.json` — the actual hash → name mapping

Example `version.json`:
```json
{"version": 1}
```

Example `data.json` (short excerpt):
```json
{
  "259035": "Kg_back_y",
  "316653": "Lw_dh_comWP"
}
```

Notes:
- Keys are decimal string representations of the numeric hash values.
- Values are the restored human-readable names.

### Integration idea for TkDataEditor

**Sync**
1. Fetch `version.json` from the public host.
2. Compare the remote `version` value to the cached copy.
3. If they differ, download `data.json`, replace the local dictionary, and write the new `version.json` beside it. If they match, stop—no need to pull the large mapping again.

**Export**
When exporting a moveset, resolve hashes through the cached dictionary wherever possible:
- **`tk_move::name_key`** — if the map contains the name, persist it on `ParsedMove`. The global move string (`<3-letter-code>_<move-name>`, e.g. `GRL_Kz_66rp`) is what feeds `tk_move::ordinal_id`; knowing the restored name (and character code) lets you reconstruct that string and keep ordinal-related fields consistent with the original layout.
- **`tk_move::anim_name_key`** — same lookup and restore pattern.
- **`tk_dialogue::voiceclip_key`** — same.
- **`tk_parryable_move`** — each entry is a `tk_move::ordinal_id` hash; resolve it the same way when the dictionary has a hit.

### Why are we not storing ordinal IDs in the dictionary file, even though they're also Kamui Hashes?
- To keep the size of our dictionary small.
- Because ordinal IDs can be generated on the fly.
- Because otherwise each common move would have N entries — N is the number of characters that has that common move.

### What the dictionary stores
- Move names mapped to `tk_move::name_key`
- Animation names mapped to `tk_move::anim_name_key`
- Voice clip names mapped to `tk_dialogue::voiceclip_key`
- Original file names from `tkdata.bin`

### Implementation notes / suggestions
- Use HTTP `If-Modified-Since` or ETag headers for efficient checks when hosting supports them.
- Validate `data.json` after download (e.g., ensure it's valid JSON and contains string values).
- Keep a small local cache (e.g., `%APPDATA%/PolarisTKDataEditor/kamui-hashes/`) with the `version.json` and `data.json` files.
- Use the [nlohmann JSON library](https://github.com/nlohmann/json) for JSON reading / writing.

### Next steps
- Add code to TkDataEditor to perform the version check and safe update flow described above.
- Consider adding a checksum or signature to ensure data integrity.

---
