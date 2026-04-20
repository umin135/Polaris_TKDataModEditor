"""
Polaris TKDataMod MCP Server
Exposes moveset data (motbin/anmbin) as queryable tools for Claude.
"""

import json
import os
import struct
import configparser
from pathlib import Path
from typing import Any
from mcp.server.fastmcp import FastMCP

mcp = FastMCP("polaris-tkdata")

# ---------------------------------------------------------------------------
#  Server-side state
# ---------------------------------------------------------------------------

_moveset_folder: str = ""
_moves: list[dict] = []
_extraprops: list[dict] = []
_startprops: list[dict] = []
_endprops: list[dict] = []
_anmbin_pool: list[list[dict]] = [[] for _ in range(6)]
_anmbin_movelist: list[list[int]] = [[] for _ in range(6)]
_chara_code: str = ""
_move_count: int = 0
_kamui: dict[int, str] = {}
_props_meta: dict[str, Any] = {}
_reqs_meta: dict[str, Any] = {}

# Path to the data/ directory next to this script's project root
_PROJECT_ROOT = Path(__file__).resolve().parent.parent

# ---------------------------------------------------------------------------
#  Constants
# ---------------------------------------------------------------------------

MOTBIN_BASE = 0x318
MOVE_SIZE   = 0x448
XOR_KEYS    = [0x964f5b9e, 0xd88448a2, 0xa84b71e0, 0xa27d5221,
               0x9b81329f, 0xadfb76c8, 0x7def1f1c, 0x7ee2bc2c]

# Header ptr offsets for all blocks
HDR_PTRS = [0x168, 0x180, 0x190, 0x1A0, 0x1B0, 0x1C0, 0x1D0, 0x1E0,
            0x1F0, 0x200, 0x210, 0x220, 0x230, 0x240, 0x250, 0x260,
            0x270, 0x280, 0x290, 0x2A0]

# Block descriptors: (ptr_hdr_off, cnt_hdr_off, stride, [(elem_off, tgt_hdr_ptr, tgt_stride), ...])
EXPAND_BLOCKS = [
    (0x168, 0x178, 0x70, [(0x00,0x1B0,0x10),(0x08,0x1B0,0x10),(0x10,0x1B0,0x10),(0x18,0x1B0,0x10),
                           (0x20,0x1B0,0x10),(0x28,0x1B0,0x10),(0x30,0x1B0,0x10)]),
    (0x180, 0x188, 0x14, []),
    (0x190, 0x198, 0x18, [(0x00,0x180,0x14),(0x10,0x168,0x70)]),
    (0x1A0, 0x1A8, 0xE0, [(0x90,0x190,0x18),(0x98,0x1D0,0x28)]),
    (0x1B0, 0x1B8, 0x10, [(0x08,0x1C0,0x02)]),
    (0x1C0, 0x1C8, 0x02, []),
    (0x1D0, 0x1D8, 0x28, [(0x08,0x180,0x14),(0x10,0x1F0,0x04)]),
    (0x1E0, 0x1E8, 0x28, [(0x08,0x180,0x14),(0x10,0x1F0,0x04)]),
    (0x1F0, 0x1F8, 0x04, []),
    (0x200, 0x208, 0x28, [(0x08,0x180,0x14)]),
    (0x210, 0x218, 0x20, [(0x00,0x180,0x14)]),
    (0x220, 0x228, 0x20, [(0x00,0x180,0x14)]),
    (0x230, 0x238, 0x448, [(0x98,0x1D0,0x28),(0x110,0x190,0x18),
                            (0x130,0x240,0x0C),(0x138,0x200,0x28),
                            (0x140,0x210,0x20),(0x148,0x220,0x20)]),
    (0x240, 0x248, 0x0C, []),
    (0x250, 0x258, 0x10, [(0x08,0x260,0x08)]),
    (0x260, 0x268, 0x08, []),
    (0x270, 0x278, 0x04, []),
    (0x280, 0x288, 0x0C, []),
    (0x290, 0x298, 0x10, [(0x08,0x280,0x0C)]),
    (0x2A0, 0x2A8, 0x18, [(0x08,0x180,0x14)]),
]

ANMBIN_CAT_NAMES = ["Fullbody", "Hand", "Facial", "Swing", "Camera", "Extra"]
EXTRAPROP_TERMINATOR_ID = 0  # type==0 && id==0
FL_PROP_TERMINATOR_ID   = 1100

# ---------------------------------------------------------------------------
#  Binary helpers
# ---------------------------------------------------------------------------

def _u32(data: bytes, off: int) -> int:
    return struct.unpack_from("<I", data, off)[0]

def _u64(data: bytes, off: int) -> int:
    return struct.unpack_from("<Q", data, off)[0]

def _i32(data: bytes, off: int) -> int:
    return struct.unpack_from("<i", data, off)[0]

def _u16(data: bytes, off: int) -> int:
    return struct.unpack_from("<H", data, off)[0]

# ---------------------------------------------------------------------------
#  motbin parsing
# ---------------------------------------------------------------------------

def _expand_indexes(data: bytearray) -> None:
    sz = len(data)
    if sz < MOTBIN_BASE:
        return

    def w64(off: int, val: int) -> None:
        struct.pack_into("<Q", data, off, val)

    def r64(off: int) -> int:
        return struct.unpack_from("<Q", data, off)[0]

    # Pass 1: header block ptrs BASE-relative -> file offset
    for o in HDR_PTRS:
        if o + 8 > sz:
            continue
        w64(o, r64(o) + MOTBIN_BASE)

    # Pass 2: element pointer fields index -> file offset
    for ptr_off, cnt_off, stride, ptr_fields in EXPAND_BLOCKS:
        if not ptr_fields:
            continue
        if ptr_off + 8 > sz or cnt_off + 8 > sz:
            continue
        block_off = r64(ptr_off)
        cnt       = r64(cnt_off)
        if not block_off or not cnt or block_off + cnt * stride > sz:
            continue
        for i in range(cnt):
            elem = block_off + i * stride
            for elem_off, tgt_hdr, tgt_stride in ptr_fields:
                foff = elem + elem_off
                if foff + 8 > sz:
                    continue
                idx = r64(foff)
                if idx == 0xFFFFFFFFFFFFFFFF:
                    continue
                if tgt_hdr + 8 > sz:
                    continue
                tgt_base = r64(tgt_hdr)
                if not tgt_base:
                    continue
                w64(foff, tgt_base + idx * tgt_stride)


def _xor_decrypt(data: bytes, block_off: int, move_idx: int) -> int:
    slot = move_idx % 8
    enc = _u32(data, block_off + slot * 4)
    return enc ^ XOR_KEYS[slot]


def _read_extraprop_block(data: bytes, file_off: int, is_fl: bool) -> list[dict]:
    """Read a null-terminated extraprop list from file_off."""
    results = []
    if not file_off:
        return results
    stride = 0x20 if is_fl else 0x28
    off = file_off
    while off + stride <= len(data):
        if is_fl:
            req_addr  = _u64(data, off + 0x00)
            prop_id   = _u32(data, off + 0x08)
            p0        = _u32(data, off + 0x0C)
            p1        = _u32(data, off + 0x10)
            p2        = _u32(data, off + 0x14)
            p3        = _u32(data, off + 0x18)
            p4        = _u32(data, off + 0x1C)
            frame     = 0
            if prop_id == FL_PROP_TERMINATOR_ID:
                results.append({"id": prop_id, "frame": frame,
                                 "params": [p0, p1, p2, p3, p4], "_end": True})
                break
        else:
            frame     = _u32(data, off + 0x00)
            req_addr  = _u64(data, off + 0x08)
            prop_id   = _u32(data, off + 0x10)
            p0        = _u32(data, off + 0x14)
            p1        = _u32(data, off + 0x18)
            p2        = _u32(data, off + 0x1C)
            p3        = _u32(data, off + 0x20)
            p4        = _u32(data, off + 0x24)
            if frame == 0 and prop_id == 0:
                results.append({"id": 0, "id_hex": "0x0", "frame": 0,
                                 "params": [p0, p1, p2, p3, p4], "_terminator": True})
                break
        results.append({
            "id": prop_id,
            "id_hex": hex(prop_id),
            "frame": frame,
            "params": [p0, p1, p2, p3, p4],
        })
        off += stride
    return results


def _load_motbin(folder: str) -> str:
    global _moves, _extraprops, _startprops, _endprops, _chara_code, _move_count

    path = os.path.join(folder, "moveset.motbin")
    if not os.path.exists(path):
        return f"moveset.motbin not found in {folder}"

    with open(path, "rb") as f:
        raw = bytearray(f.read())

    _expand_indexes(raw)
    data = bytes(raw)

    moves_ptr   = _u64(data, 0x230)
    moves_count = _u64(data, 0x238)

    # Load moveset.ini for display names and chara code
    names_by_key: dict[int, str] = {}
    ini_path = os.path.join(folder, "moveset.ini")
    _chara_code = ""
    if os.path.exists(ini_path):
        cfg = configparser.ConfigParser()
        cfg.read(ini_path, encoding="utf-8")
        _chara_code = cfg.get("Header", "OriginalCharacter", fallback="")

    # Load name_keys.json for display names
    nk_path = os.path.join(folder, "name_keys.json")
    if os.path.exists(nk_path):
        try:
            with open(nk_path, encoding="utf-8") as f:
                nk = json.load(f)
            for entry in nk.get("moves", []):
                k = entry.get("name_key", 0)
                n = entry.get("name", "")
                if k and n:
                    names_by_key[k] = n
        except Exception:
            pass

    _moves = []
    for i in range(int(moves_count)):
        base = moves_ptr + i * MOVE_SIZE
        if base + MOVE_SIZE > len(data):
            break

        anim_key    = _xor_decrypt(data, base + 0x20, i)
        name_key    = _xor_decrypt(data, base + 0x00, i)
        anim_len    = _i32(data, base + 0x120)
        startup     = _u32(data, base + 0x158)
        recovery    = _u32(data, base + 0x15C)
        ep_addr     = _u64(data, base + 0x138)
        sp_addr     = _u64(data, base + 0x140)
        np_addr     = _u64(data, base + 0x148)
        transition  = _u16(data, base + 0xCC)

        display_name = names_by_key.get(name_key, "")
        kamui_name   = _kamui.get(anim_key, "")

        _moves.append({
            "index":        i,
            "name":         display_name,
            "anim_key":     anim_key,
            "anim_key_hex": hex(anim_key),
            "anim_name":    kamui_name,
            "anim_len":     anim_len,
            "startup":      startup,
            "recovery":     recovery,
            "transition":   transition,
            "_ep_addr":     ep_addr,
            "_sp_addr":     sp_addr,
            "_np_addr":     np_addr,
        })

    _move_count = len(_moves)

    # Pre-parse ALL extraprop blocks into flat lists with move back-references
    _extraprops.clear()
    _startprops.clear()
    _endprops.clear()

    # Group extraprops by starting file offset so we can map move -> group index
    _ep_offset_to_idx: dict[int, int] = {}
    _sp_offset_to_idx: dict[int, int] = {}
    _np_offset_to_idx: dict[int, int] = {}

    def intern_block(addr, cache, store, is_fl):
        if not addr or addr in cache:
            return cache.get(addr, -1)
        idx = len(store)
        cache[addr] = idx
        entries = _read_extraprop_block(data, addr, is_fl)
        for e in entries:
            e["_move_refs"] = []
            store.append(e)
        return idx

    for mv in _moves:
        ep_start = intern_block(mv["_ep_addr"], _ep_offset_to_idx, _extraprops, False)
        sp_start = intern_block(mv["_sp_addr"], _sp_offset_to_idx, _startprops, True)
        np_start = intern_block(mv["_np_addr"], _np_offset_to_idx, _endprops,   True)
        mv["_ep_start"] = ep_start
        mv["_sp_start"] = sp_start
        mv["_np_start"] = np_start

    return ""


def _load_anmbin(folder: str) -> str:
    global _anmbin_pool, _anmbin_movelist

    path = os.path.join(folder, "moveset.anmbin")
    if not os.path.exists(path):
        return f"moveset.anmbin not found in {folder}"

    with open(path, "rb") as f:
        data = f.read()

    pool_counts      = struct.unpack_from("<6I", data, 0x04)
    move_counts      = struct.unpack_from("<6I", data, 0x1C)
    pool_offsets     = struct.unpack_from("<6Q", data, 0x38)
    movelist_offsets = struct.unpack_from("<6Q", data, 0x68)

    _anmbin_pool     = [[] for _ in range(6)]
    _anmbin_movelist = [[] for _ in range(6)]

    for cat in range(6):
        cnt = pool_counts[cat]
        off = pool_offsets[cat]
        if not cnt or not off:
            continue
        for i in range(cnt):
            base = off + i * 0x38
            if base + 0x38 > len(data):
                break
            anim_key     = struct.unpack_from("<Q", data, base)[0]
            anim_data_ptr= struct.unpack_from("<Q", data, base + 8)[0]
            _anmbin_pool[cat].append({
                "index":        i,
                "anim_key":     anim_key & 0xFFFFFFFF,
                "anim_key_hex": hex(anim_key & 0xFFFFFFFF),
                "has_data":     anim_data_ptr != 0,
            })

        cnt_ml = move_counts[cat]
        off_ml = movelist_offsets[cat]
        if cnt_ml and off_ml and off_ml + cnt_ml * 4 <= len(data):
            _anmbin_movelist[cat] = list(struct.unpack_from(f"<{cnt_ml}I", data, off_ml))

    return ""


def _load_data_jsons() -> None:
    global _kamui, _props_meta, _reqs_meta
    kamui_path = _PROJECT_ROOT / "data" / "kamui-hashes" / "data.json"
    if kamui_path.exists():
        with open(kamui_path, encoding="utf-8") as f:
            raw = json.load(f)
        _kamui = {int(k): v for k, v in raw.items() if k.isdigit()}

    movedata_path = _PROJECT_ROOT / "data" / "MovesetDatas" / "data.json"
    if movedata_path.exists():
        with open(movedata_path, encoding="utf-8") as f:
            md = json.load(f)
        _props_meta = {int(k): v for k, v in md.get("properties", {}).items()}
        _reqs_meta  = {int(k): v for k, v in md.get("requirements", {}).items()}


_load_data_jsons()


# ---------------------------------------------------------------------------
#  Helpers
# ---------------------------------------------------------------------------

def _prop_label(prop_id: int) -> str:
    meta = _props_meta.get(prop_id)
    if meta:
        return meta.get("Function", "") or meta.get("function", "")
    return ""


def _req_label(req_id: int) -> str:
    meta = _reqs_meta.get(req_id)
    if meta:
        return meta.get("Condition", "") or meta.get("condition", "")
    return ""


def _annotate_prop(e: dict) -> dict:
    label = _prop_label(e["id"])
    out = {k: v for k, v in e.items() if not k.startswith("_")}
    if label:
        out["label"] = label
    return out


def _get_move_extraprops(move_idx: int) -> dict:
    if move_idx < 0 or move_idx >= len(_moves):
        return {"error": f"move index {move_idx} out of range (0–{len(_moves)-1})"}
    mv = _moves[move_idx]
    ep_start = mv.get("_ep_start", -1)
    sp_start = mv.get("_sp_start", -1)
    np_start = mv.get("_np_start", -1)

    def collect(block, start):
        if start < 0:
            return []
        result = []
        i = start
        while i < len(block):
            e = block[i]
            if e.get("_end") or e.get("_terminator"):
                break
            if e["id"] == 0 and e["frame"] == 0:
                break
            result.append(_annotate_prop(e))
            i += 1
        return result

    return {
        "extra":  collect(_extraprops, ep_start),
        "start":  collect(_startprops, sp_start),
        "end":    collect(_endprops,   np_start),
    }


# ---------------------------------------------------------------------------
#  MCP tools
# ---------------------------------------------------------------------------

@mcp.tool()
def set_moveset(folder_path: str) -> str:
    """Load a moveset from a folder path (must contain moveset.motbin and moveset.anmbin)."""
    global _moveset_folder
    folder_path = folder_path.strip('"').strip("'")
    if not os.path.isdir(folder_path):
        return f"Folder not found: {folder_path}"
    _moveset_folder = folder_path
    err = _load_motbin(folder_path)
    if err:
        return f"motbin error: {err}"
    err = _load_anmbin(folder_path)
    if err:
        return f"anmbin error: {err}"
    return (f"Loaded moveset for '{_chara_code}': {_move_count} moves, "
            f"hand pool {len(_anmbin_pool[1])} entries.")


@mcp.tool()
def get_moveset_info() -> dict:
    """Return basic info about the currently loaded moveset."""
    if not _moveset_folder:
        return {"error": "No moveset loaded. Call set_moveset() first."}
    return {
        "folder":      _moveset_folder,
        "chara_code":  _chara_code,
        "move_count":  _move_count,
        "pool_counts": {ANMBIN_CAT_NAMES[i]: len(_anmbin_pool[i]) for i in range(6)},
    }


@mcp.tool()
def get_move(index: int) -> dict:
    """Get move info by index."""
    if not _moveset_folder:
        return {"error": "No moveset loaded."}
    if index < 0 or index >= len(_moves):
        return {"error": f"Index {index} out of range (0–{len(_moves)-1})"}
    mv = _moves[index]
    return {k: v for k, v in mv.items() if not k.startswith("_")}


@mcp.tool()
def search_moves(query: str, limit: int = 30) -> list[dict]:
    """Search moves by display name or anim name (case-insensitive substring)."""
    if not _moveset_folder:
        return [{"error": "No moveset loaded."}]
    q = query.lower()
    results = []
    for mv in _moves:
        if q in mv["name"].lower() or q in mv["anim_name"].lower():
            results.append({k: v for k, v in mv.items() if not k.startswith("_")})
            if len(results) >= limit:
                break
    return results


@mcp.tool()
def get_extraprops(move_index: int) -> dict:
    """Get all extra/start/end props for a move by index."""
    if not _moveset_folder:
        return {"error": "No moveset loaded."}
    return _get_move_extraprops(move_index)


@mcp.tool()
def find_moves_with_prop(prop_id: int, limit: int = 50) -> dict:
    """Find all moves that have a given extraprop ID (decimal or use int())."""
    if not _moveset_folder:
        return {"error": "No moveset loaded."}
    results = []
    label = _prop_label(prop_id)
    for mv in _moves:
        props = _get_move_extraprops(mv["index"])
        all_props = props["extra"] + props["start"] + props["end"]
        matching = [p for p in all_props if p["id"] == prop_id]
        if matching:
            results.append({
                "move_index": mv["index"],
                "move_name":  mv["name"],
                "anim_name":  mv["anim_name"],
                "matching_props": matching,
            })
            if len(results) >= limit:
                break
    return {"prop_id": hex(prop_id), "label": label, "results": results}


@mcp.tool()
def count_prop_usage(prop_id: int) -> dict:
    """Count how many moves reference a given extraprop ID, and list unique param[0] values used."""
    if not _moveset_folder:
        return {"error": "No moveset loaded."}
    label = _prop_label(prop_id)
    move_count = 0
    instance_count = 0
    param0_values: set[int] = set()
    for mv in _moves:
        props = _get_move_extraprops(mv["index"])
        all_props = props["extra"] + props["start"] + props["end"]
        matching = [p for p in all_props if p["id"] == prop_id]
        if matching:
            move_count += 1
            instance_count += len(matching)
            for p in matching:
                param0_values.add(p["params"][0])
    return {
        "prop_id": hex(prop_id),
        "label": label,
        "moves_with_prop": move_count,
        "total_instances": instance_count,
        "unique_param0_values": sorted(param0_values),
        "param0_range": [min(param0_values), max(param0_values)] if param0_values else [],
    }


@mcp.tool()
def get_pool_entry(cat: int, index: int) -> dict:
    """Get an anmbin pool entry by category (0=Fullbody,1=Hand,2=Facial,3=Swing,4=Camera,5=Extra) and index."""
    if not _moveset_folder:
        return {"error": "No moveset loaded."}
    if cat < 0 or cat >= 6:
        return {"error": "cat must be 0–5"}
    pool = _anmbin_pool[cat]
    if index < 0 or index >= len(pool):
        return {"error": f"Index {index} out of range (0–{len(pool)-1}) for cat {ANMBIN_CAT_NAMES[cat]}"}
    entry = dict(pool[index])
    key = entry["anim_key"]
    entry["kamui_name"] = _kamui.get(key, "")
    return entry


@mcp.tool()
def get_pool_list(cat: int, limit: int = 50, offset: int = 0) -> dict:
    """List anmbin pool entries for a category."""
    if not _moveset_folder:
        return {"error": "No moveset loaded."}
    if cat < 0 or cat >= 6:
        return {"error": "cat must be 0–5"}
    pool = _anmbin_pool[cat]
    entries = []
    for e in pool[offset: offset + limit]:
        row = dict(e)
        row["kamui_name"] = _kamui.get(e["anim_key"], "")
        entries.append(row)
    return {
        "category":    ANMBIN_CAT_NAMES[cat],
        "total":       len(pool),
        "offset":      offset,
        "entries":     entries,
    }


@mcp.tool()
def get_movelist_entry(cat: int, index: int) -> dict:
    """Get anmbin moveList[cat][index] — the hash assigned to move N for a given category."""
    if not _moveset_folder:
        return {"error": "No moveset loaded."}
    if cat < 0 or cat >= 6:
        return {"error": "cat must be 0–5"}
    ml = _anmbin_movelist[cat]
    if index < 0 or index >= len(ml):
        return {"error": f"Index {index} out of range (0–{len(ml)-1})"}
    h = ml[index]
    pool = _anmbin_pool[cat]
    pool_idx = next((e["index"] for e in pool if e["anim_key"] == h), -1)
    return {
        "move_index":  index,
        "hash":        h,
        "hash_hex":    hex(h),
        "pool_index":  pool_idx,
        "kamui_name":  _kamui.get(h, ""),
    }


@mcp.tool()
def kamui_lookup(hash_value: int) -> str:
    """Look up an animation hash in the kamui dictionary. Returns name or empty string."""
    return _kamui.get(hash_value, "")


@mcp.tool()
def get_prop_info(prop_id: int) -> dict:
    """Look up extraprop metadata (function name, tooltip, param description)."""
    meta = _props_meta.get(prop_id)
    if not meta:
        return {"prop_id": hex(prop_id), "known": False}
    return {"prop_id": hex(prop_id), "known": True, **meta}


@mcp.tool()
def get_req_info(req_id: int) -> dict:
    """Look up requirement metadata (condition name, tooltip, param description)."""
    meta = _reqs_meta.get(req_id)
    if not meta:
        return {"req_id": req_id, "known": False}
    return {"req_id": req_id, "known": True, **meta}


if __name__ == "__main__":
    mcp.run()
