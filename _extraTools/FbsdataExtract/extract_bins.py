"""
FbsdataExtract -- Tekken 8 FlatBuffers bin extractor
Usage : python extract_bins.py           (processes all .bin in bins/)
        python extract_bins.py <file>    (single file)
Output: extracted/<stem>.json
"""

import os, sys, json, struct

# ---------------------------------------------------------------------------
#  Low-level FlatBuffers reader
# ---------------------------------------------------------------------------

class FbsBuf:
    def __init__(self, data: bytes):
        self.d = data

    def u8  (self, p):        return self.d[p]
    def u16 (self, p):        return struct.unpack_from('<H', self.d, p)[0]
    def u32 (self, p):        return struct.unpack_from('<I', self.d, p)[0]
    def i32 (self, p):        return struct.unpack_from('<i', self.d, p)[0]
    def i16 (self, p):        return struct.unpack_from('<h', self.d, p)[0]
    def f32 (self, p):        return struct.unpack_from('<f', self.d, p)[0]

    def string(self, p):
        length = self.u32(p)
        return self.d[p+4:p+4+length].decode('utf-8', errors='replace')

    # ---- vtable helpers ----

    def vtable_pos(self, table_pos: int) -> int:
        soff = self.i32(table_pos)   # signed, vtable is at table_pos - soff
        return table_pos - soff

    def field_offset(self, table_pos: int, field_id: int) -> int:
        """Returns the byte offset of field_id within the table (0 = missing)."""
        vt = self.vtable_pos(table_pos)
        vt_size = self.u16(vt)
        n = (vt_size - 4) // 2
        if field_id >= n:
            return 0
        return self.u16(vt + 4 + field_id * 2)

    # ---- field readers ----

    def f_u8  (self, tp, fid, default=0):
        fo = self.field_offset(tp, fid); return self.u8(tp+fo)  if fo else default
    def f_bool(self, tp, fid, default=False):
        fo = self.field_offset(tp, fid); return bool(self.u8(tp+fo)) if fo else default
    def f_i16 (self, tp, fid, default=0):
        fo = self.field_offset(tp, fid); return self.i16(tp+fo) if fo else default
    def f_u32 (self, tp, fid, default=0):
        fo = self.field_offset(tp, fid); return self.u32(tp+fo) if fo else default
    def f_i32 (self, tp, fid, default=0):
        fo = self.field_offset(tp, fid); return self.i32(tp+fo) if fo else default
    def f_f32 (self, tp, fid, default=0.0):
        fo = self.field_offset(tp, fid); return round(self.f32(tp+fo), 6) if fo else default
    def f_str (self, tp, fid, default=''):
        fo = self.field_offset(tp, fid)
        if not fo: return default
        abs_f = tp + fo
        return self.string(abs_f + self.u32(abs_f))
    def f_table(self, tp, fid):
        """Returns absolute position of nested table, or None."""
        fo = self.field_offset(tp, fid)
        if not fo: return None
        abs_f = tp + fo
        return abs_f + self.u32(abs_f)
    def f_vec(self, tp, fid):
        """Returns (count, abs_vec_start) or (0, None)."""
        fo = self.field_offset(tp, fid)
        if not fo: return 0, None
        abs_f = tp + fo
        vec = abs_f + self.u32(abs_f)
        return self.u32(vec), vec

    def vec_table(self, tp, fid):
        """Iterate tables in a vector field. Yields each table's absolute position."""
        count, vec = self.f_vec(tp, fid)
        if not count: return
        for i in range(count):
            elem_pos = vec + 4 + i * 4
            yield elem_pos + self.u32(elem_pos)

    def vec_u32(self, tp, fid):
        count, vec = self.f_vec(tp, fid)
        return [self.u32(vec + 4 + i * 4) for i in range(count)] if count else []

    def vec_i32(self, tp, fid):
        count, vec = self.f_vec(tp, fid)
        return [self.i32(vec + 4 + i * 4) for i in range(count)] if count else []

    def root(self) -> int:
        return self.u32(0)

    def data_table(self) -> int:
        """Navigate root → data sub-table (root.field[1])."""
        r = self.root()
        return self.f_table(r, 1)


# ---------------------------------------------------------------------------
#  Per-type entry parsers  (return a dict per entry)
# ---------------------------------------------------------------------------

def parse_common_item(b: FbsBuf, tp: int) -> dict:
    return {
        'item_id':        b.f_u32 (tp,  0),
        'item_no':        b.f_i32 (tp,  1),
        'ItemPrefab':     b.f_str (tp,  2),
        'Char_hash':      b.f_u32 (tp,  3),
        'ItemPos_hash':   b.f_u32 (tp,  4),
        'ItemName_Key':   b.f_str (tp,  5),
        'package_id':     b.f_str (tp,  6),
        'package_sub_id': b.f_str (tp,  7),
        'unk_8':          b.f_u32 (tp,  8),
        'shop_sort_id':   b.f_i32 (tp,  9),
        'is_enabled':     b.f_bool(tp, 10),
        'rarity':         b.f_u32 (tp, 11),
        'price':          b.f_i32 (tp, 12),
        'unk_13':         b.f_bool(tp, 13),
        'category_no':    b.f_i32 (tp, 14),
        'hash_2':         b.f_u32 (tp, 15),
        'unk_16':         b.f_bool(tp, 16),
        'unk_17':         b.f_u32 (tp, 17),
        'hash_3':         b.f_u32 (tp, 18),
        'unk_19':         b.f_u32 (tp, 19),
        'unk_20':         b.f_u32 (tp, 20),
        'unk_21':         b.f_u32 (tp, 21),
        'unk_22':         b.f_u32 (tp, 22),
        'hash_4':         b.f_u32 (tp, 23),
        'unk_24':         b.f_u32 (tp, 24),
        'sort_group':     b.f_i32 (tp, 25),
    }

def parse_character(b: FbsBuf, tp: int) -> dict:
    return {
        'character_code':    b.f_str (tp,  0),
        'name_hash':         b.f_u32 (tp,  1),
        'is_enabled':        b.f_bool(tp,  2),
        'is_selectable':     b.f_bool(tp,  3),
        'group':             b.f_str (tp,  4),
        'camera_offset':     b.f_f32 (tp,  5),
        'is_playable':       b.f_bool(tp,  6),
        'sort_order':        b.f_u32 (tp,  7),
        'full_name_key':     b.f_str (tp,  8),
        'short_name_jp_key': b.f_str (tp,  9),
        'short_name_key':    b.f_str (tp, 10),
        'origin_key':        b.f_str (tp, 11),
        'fighting_style_key':b.f_str (tp, 12),
        'height_key':        b.f_str (tp, 13),
        'weight_key':        b.f_str (tp, 14),
    }

def parse_exclusive_rule(b: FbsBuf, tp: int) -> dict:
    return {'item_id': b.f_u32(tp,0), 'hash': b.f_u32(tp,1),
            'link_type': b.f_u32(tp,2), 'ref_item_id': b.f_u32(tp,3)}

def parse_exclusive_pair(b: FbsBuf, tp: int) -> dict:
    return {'item_id_a': b.f_u32(tp,0), 'item_id_b': b.f_u32(tp,1), 'flag': b.f_u32(tp,2)}

def parse_area(b: FbsBuf, tp: int) -> dict:
    return {'area_hash': b.f_u32(tp,0), 'area_code': b.f_str(tp,1)}

def parse_battle_subtitle(b: FbsBuf, tp: int) -> dict:
    return {'subtitle_hash': b.f_u32(tp,0), 'subtitle_type': b.f_u32(tp,1)}

def parse_fate_drama(b: FbsBuf, tp: int) -> dict:
    return {
        'character1_hash': b.f_u32 (tp,0), 'character2_hash': b.f_u32 (tp,1),
        'value_0':         b.f_u32 (tp,2), 'hash_2':          b.f_u32 (tp,3),
        'value_4':         b.f_bool(tp,4),
    }

def parse_jukebox(b: FbsBuf, tp: int) -> dict:
    return {
        'bgm_hash':        b.f_u32(tp,0), 'series_hash':    b.f_u32(tp,1),
        'unk_2':           b.f_u32(tp,2), 'cue_name':       b.f_str(tp,3),
        'arrangement':     b.f_str(tp,4), 'alt_cue_name_1': b.f_str(tp,5),
        'alt_cue_name_2':  b.f_str(tp,6), 'alt_cue_name_3': b.f_str(tp,7),
        'display_text_key':b.f_str(tp,8),
    }

def parse_series(b: FbsBuf, tp: int) -> dict:
    return {
        'series_hash':     b.f_u32(tp,0), 'jacket_text_key': b.f_str(tp,1),
        'jacket_icon_key': b.f_str(tp,2), 'logo_text_key':   b.f_str(tp,3),
        'logo_icon_key':   b.f_str(tp,4),
    }

def parse_tam_mission(b: FbsBuf, tp: int) -> dict:
    return {
        'mission_id': b.f_u32(tp, 0), 'value_1':  b.f_u32(tp, 1),
        'value_2':    b.f_u32(tp, 2), 'location': b.f_str(tp, 3),
        'hash_0':     b.f_u32(tp, 4), 'hash_1':   b.f_u32(tp, 5),
        'hash_2':     b.f_u32(tp, 6), 'hash_3':   b.f_u32(tp, 7),
        'hash_4':     b.f_u32(tp, 8), 'value_9':  b.f_u32(tp, 9),
        'value_10':   b.f_u32(tp,10), 'value_11': b.f_u32(tp,11),
    }

def parse_drama_player_start(b: FbsBuf, tp: int) -> dict:
    fields = [
        'character_hash','hash_1','index','scene_hash','config_hash',
        'unk_float_5','pos_x','pos_y','state_hash','unk_9','scale','ref_hash',
        'unk_float_12','unk_float_13','unk_float_14','unk_float_15',
        'unk_16','unk_17','unk_float_18','rate',
        'blk1_marker','blk1_scale','blk1_field_22','blk1_field_23','blk1_field_24',
        'blk1_field_25','blk1_field_26','blk1_field_27','blk1_field_28','blk1_angle',
        'blk1_hash_a','blk1_hash_b',
        'blk2_marker','blk2_scale','blk2_field_34','blk2_field_35','blk2_field_36',
        'blk2_field_37','blk2_field_38','blk2_field_39','blk2_field_40','blk2_angle',
        'blk2_hash_a','blk2_hash_b',
        'blk3_marker','blk3_scale','blk3_field_46','blk3_field_47','blk3_field_48',
        'blk3_field_49','blk3_field_50','blk3_field_51','blk3_field_52','blk3_angle',
        'blk3_hash_a','blk3_hash_b',
        'end_marker','unk_float_57','extra_range',
        'extra_param_a','extra_param_b','extra_param_c','extra_param_d',
    ]
    float_ids = {5,6,7,10,12,13,14,15,18,19,21,22,23,24,25,26,27,28,29,
                 33,34,35,36,37,38,39,40,41,45,46,47,48,49,50,51,52,53,
                 57,58,59,60,61,62}
    uint_ids  = {0,1,2,3,4,8,9,11,16,17,20,30,31,32,42,43,44,54,55,56}
    out = {}
    for i, name in enumerate(fields):
        if i in float_ids:  out[name] = b.f_f32(tp, i)
        else:               out[name] = b.f_u32(tp, i)
    return out

def parse_stage(b: FbsBuf, tp: int) -> dict:
    return {
        'stage_code':         b.f_str (tp,  0), 'stage_hash':          b.f_u32 (tp,  1),
        'is_selectable':      b.f_bool(tp,  2), 'camera_offset':       b.f_f32 (tp,  3),
        'parent_stage_index': b.f_u32 (tp,  4), 'variant_hash':        b.f_u32 (tp,  5),
        'has_weather':        b.f_bool(tp,  6), 'is_active':           b.f_bool(tp,  7),
        'flag_interlocked':   b.f_bool(tp,  8), 'flag_ocean':          b.f_bool(tp,  9),
        'flag_10':            b.f_bool(tp, 10), 'flag_infinite':       b.f_bool(tp, 11),
        'flag_battle':        b.f_bool(tp, 12), 'flag_13':             b.f_bool(tp, 13),
        'flag_balcony':       b.f_bool(tp, 14), 'flag_15':             b.f_bool(tp, 15),
        'reserved_16':        b.f_bool(tp, 16), 'is_online_enabled':   b.f_bool(tp, 17),
        'is_ranked_enabled':  b.f_bool(tp, 18), 'reserved_19':         b.f_bool(tp, 19),
        'reserved_20':        b.f_bool(tp, 20), 'arena_width':         b.f_u32 (tp, 21),
        'arena_depth':        b.f_u32 (tp, 22), 'reserved_23':         b.f_u32 (tp, 23),
        'arena_param':        b.f_u32 (tp, 24), 'extra_width':         b.f_u32 (tp, 25),
        'extra_group':        b.f_str (tp, 26), 'extra_depth':         b.f_u32 (tp, 27),
        'group_id':           b.f_str (tp, 28), 'stage_name_key':      b.f_str (tp, 29),
        'level_name':         b.f_str (tp, 30), 'sound_bank':          b.f_str (tp, 31),
        'wall_distance_a':    b.f_u32 (tp, 32), 'wall_distance_b':     b.f_u32 (tp, 33),
        'stage_mode':         b.f_u32 (tp, 34), 'reserved_35':         b.f_u32 (tp, 35),
        'is_default_variant': b.f_bool(tp, 36),
    }

def parse_ball_property(b: FbsBuf, tp: int) -> dict:
    return {
        'ball_hash':   b.f_u32(tp, 0), 'ball_code':   b.f_str(tp, 1),
        'effect_name': b.f_str(tp, 2), 'hash_3':      b.f_u32(tp, 3),
        'hash_4':      b.f_u32(tp, 4), 'unk_5':       b.f_u32(tp, 5),
        'unk_6':       b.f_u32(tp, 6), 'hash_7':      b.f_u32(tp, 7),
        'item_no':     b.f_u32(tp, 8), 'rarity':      b.f_u32(tp, 9),
        'value_10':    b.f_f32(tp,10), 'value_11':    b.f_f32(tp,11),
        'value_12':    b.f_f32(tp,12), 'value_13':    b.f_f32(tp,13),
        'value_14':    b.f_f32(tp,14), 'value_15':    b.f_f32(tp,15),
        'value_16':    b.f_f32(tp,16), 'value_17':    b.f_f32(tp,17),
        'value_18':    b.f_f32(tp,18),
    }

def parse_body_cylinder(b: FbsBuf, tp: int) -> dict:
    return {
        'character_hash': b.f_u32(tp, 0),
        'cyl0_radius':    b.f_f32(tp, 1), 'cyl0_height':   b.f_f32(tp, 2),
        'cyl0_offset_y':  b.f_f32(tp, 3), 'cyl0_unk_hash': b.f_u32(tp, 4),
        'unk_5': b.f_u32(tp,5), 'unk_6': b.f_u32(tp,6), 'unk_7': b.f_u32(tp,7),
        'cyl1_radius':    b.f_f32(tp, 8), 'cyl1_height':   b.f_f32(tp, 9),
        'cyl1_offset_y':  b.f_f32(tp,10), 'cyl1_unk_hash': b.f_u32(tp,11),
        'unk_12': b.f_u32(tp,12), 'unk_13': b.f_u32(tp,13), 'unk_14': b.f_u32(tp,14),
        'cyl2_radius':    b.f_f32(tp,15), 'cyl2_height':   b.f_f32(tp,16),
        'cyl2_offset_y':  b.f_f32(tp,17), 'cyl2_unk_hash': b.f_u32(tp,18),
    }

def parse_unique_item(b: FbsBuf, tp: int) -> dict:
    return {
        'char_item_id':    b.f_u32 (tp,  0), 'asset_name':      b.f_str (tp,  1),
        'character_hash':  b.f_u32 (tp,  2), 'hash_1':          b.f_u32 (tp,  3),
        'text_key':        b.f_str (tp,  4), 'extra_text_key_1':b.f_str (tp,  5),
        'extra_text_key_2':b.f_str (tp,  6), 'flag_7':          b.f_bool(tp,  7),
        'unk_8':           b.f_u32 (tp,  8), 'flag_9':          b.f_bool(tp,  9),
        'unk_10':          b.f_u32 (tp, 10), 'price':           b.f_u32 (tp, 11),
        'unk_12':          b.f_u32 (tp, 12), 'unk_13':          b.f_u32 (tp, 13),
        'hash_2':          b.f_u32 (tp, 14), 'flag_15':         b.f_bool(tp, 15),
        'unk_16':          b.f_u32 (tp, 16), 'hash_3':          b.f_u32 (tp, 17),
        'unk_18':          b.f_u32 (tp, 18), 'unk_19':          b.f_u32 (tp, 19),
        'unk_20':          b.f_u32 (tp, 20), 'unk_21':          b.f_u32 (tp, 21),
    }

def parse_unique_body(b: FbsBuf, tp: int) -> dict:
    return {'asset_name': b.f_str(tp,0), 'char_item_id': b.f_u32(tp,1)}

def parse_char_select_hash(b: FbsBuf, tp: int) -> dict:
    return {'character_hash': b.f_u32(tp,0)}

def parse_char_select_param(b: FbsBuf, tp: int) -> dict:
    return {'game_version': b.f_u32(tp,0), 'value_1': b.f_u32(tp,1)}

def parse_prohibit_drama(b: FbsBuf, tp: int) -> dict:
    return {'value_0': b.f_i32(tp,0), 'value_1': b.f_i32(tp,1)}

def parse_battle_motion(b: FbsBuf, tp: int) -> dict:
    return {'motion_id': b.f_u8(tp,0), 'value_1': b.f_u32(tp,1), 'value_2': b.f_u32(tp,2)}

def parse_arcade_cpu_char(b: FbsBuf, tp: int) -> dict:
    return {
        'character_hash': b.f_u32(tp,0), 'ai_level': b.f_u32(tp,1),
        'float_1': b.f_f32(tp,2), 'uint_2': b.f_u32(tp,3),
        'float_2': b.f_f32(tp,4), 'uint_3': b.f_u32(tp,5), 'float_3': b.f_f32(tp,6),
    }

def parse_arcade_cpu_hash(b: FbsBuf, tp: int) -> dict:
    return {'value_hash': b.f_u32(tp,0)}

def parse_arcade_cpu_rule(b: FbsBuf, tp: int) -> dict:
    return {
        'flag_0': b.f_u8(tp,0), 'flag_1': b.f_u8(tp,1),
        'value_2': b.f_u32(tp,2), 'value_3': b.f_u32(tp,3),
    }

def parse_ball_recommend(b: FbsBuf, tp: int) -> dict:
    return {
        'character_hash':   b.f_u32(tp,0), 'move_name_key':    b.f_str(tp,1),
        'command_text_key': b.f_str(tp,2), 'unk_3':            b.f_u32(tp,3),
        'unk_4':            b.f_u32(tp,4),
    }

def parse_battle_common_single(b: FbsBuf, tp: int) -> dict:
    return {'value': b.f_u32(tp,0)}

def parse_battle_common_char_scale(b: FbsBuf, tp: int) -> dict:
    return {
        'hash_0':  b.f_u32(tp,0),
        'value_1': b.f_f32(tp,1), 'value_2': b.f_f32(tp,2), 'value_3': b.f_f32(tp,3),
        'value_4': b.f_f32(tp,4), 'value_5': b.f_f32(tp,5), 'value_6': b.f_f32(tp,6),
        'value_7': b.f_f32(tp,7),
    }

def parse_battle_common_pair(b: FbsBuf, tp: int) -> dict:
    return {'value_0': b.f_u32(tp,0), 'value_1': b.f_u32(tp,1), 'value_2': b.f_u32(tp,2)}

def parse_battle_common_misc(b: FbsBuf, tp: int) -> dict:
    return {'value_0': b.f_f32(tp,0), 'value_1': b.f_f32(tp,1), 'value_2': b.f_f32(tp,2)}

def parse_battle_cpu_rank(b: FbsBuf, tp: int) -> dict:
    out = {f'value_{i}': b.f_u32(tp, i) for i in range(47)}
    out['rank_label'] = b.f_str(tp, 47)
    return out

def parse_battle_cpu_step(b: FbsBuf, tp: int) -> dict:
    return {'value_0': b.f_u32(tp,0), 'value_1': b.f_u32(tp,1),
            'value_2': b.f_u32(tp,2), 'value_3': b.f_u32(tp,3)}

def parse_rank_item(b: FbsBuf, tp: int) -> dict:
    return {'hash': b.f_u32(tp,0), 'text_key': b.f_str(tp,1),
            'name': b.f_str(tp,2),  'rank':     b.f_u32(tp,3)}

def parse_assist_input(b: FbsBuf, tp: int) -> dict:
    out = {'character_hash': b.f_u32(tp,0)}
    for i in range(1, 59):
        out[f'value_{i}'] = b.f_i32(tp, i)
    return out


# ---------------------------------------------------------------------------
#  Per-bin-type extraction  (returns a dict to be JSON-serialised)
# ---------------------------------------------------------------------------

def extract_simple_list(b: FbsBuf, parse_fn) -> dict:
    """data.field[0] = vector of entry tables."""
    dt = b.data_table()
    entries = [parse_fn(b, tp) for tp in b.vec_table(dt, 0)]
    return {'entries': entries}

def extract_customize_item_common_list(b: FbsBuf) -> dict:
    return extract_simple_list(b, parse_common_item)

def extract_character_list(b: FbsBuf) -> dict:
    return extract_simple_list(b, parse_character)

def extract_customize_item_exclusive_list(b: FbsBuf) -> dict:
    dt = b.data_table()
    return {
        'rule_entries':       [parse_exclusive_rule(b,tp) for tp in b.vec_table(dt, 0)],
        'pair_entries':       [parse_exclusive_pair(b,tp) for tp in b.vec_table(dt, 1)],
        'group_rule_entries': [parse_exclusive_rule(b,tp) for tp in b.vec_table(dt, 2)],
        'group_pair_entries': [parse_exclusive_pair(b,tp) for tp in b.vec_table(dt, 3)],
        'set_rule_entries':   [parse_exclusive_rule(b,tp) for tp in b.vec_table(dt, 4)],
    }

def extract_area_list(b: FbsBuf) -> dict:
    return extract_simple_list(b, parse_area)

def extract_battle_subtitle_info_list(b: FbsBuf) -> dict:
    return extract_simple_list(b, parse_battle_subtitle)

def extract_fate_drama_player_start_list(b: FbsBuf) -> dict:
    return extract_simple_list(b, parse_fate_drama)

def extract_jukebox_list(b: FbsBuf) -> dict:
    dt = b.data_table()
    return {
        'entries':  [parse_jukebox(b, tp) for tp in b.vec_table(dt, 0)],
        'unk_value_1': b.f_u32(dt, 1),
    }

def extract_series_list(b: FbsBuf) -> dict:
    return extract_simple_list(b, parse_series)

def extract_tam_mission_list(b: FbsBuf) -> dict:
    return extract_simple_list(b, parse_tam_mission)

def extract_drama_player_start_list(b: FbsBuf) -> dict:
    return extract_simple_list(b, parse_drama_player_start)

def extract_stage_list(b: FbsBuf) -> dict:
    return extract_simple_list(b, parse_stage)

def extract_ball_property_list(b: FbsBuf) -> dict:
    dt = b.data_table()
    return {
        'entries':     [parse_ball_property(b, tp) for tp in b.vec_table(dt, 0)],
        'unk_value_1': b.f_u32(dt, 1),
    }

def extract_body_cylinder_data_list(b: FbsBuf) -> dict:
    dt = b.data_table()
    return {
        'entries':             [parse_body_cylinder(b, tp) for tp in b.vec_table(dt, 0)],
        'global_scale':        b.f_f32(dt, 0),  # list-level id:0 (before entries? check schema)
    }

def extract_customize_item_unique_list(b: FbsBuf) -> dict:
    dt = b.data_table()
    return {
        'entries':      [parse_unique_item(b, tp) for tp in b.vec_table(dt, 0)],
        'body_entries': [parse_unique_body(b, tp) for tp in b.vec_table(dt, 1)],
    }

def extract_character_select_list(b: FbsBuf) -> dict:
    dt = b.data_table()
    return {
        'hash_entries':  [parse_char_select_hash(b, tp) for tp in b.vec_table(dt, 0)],
        'param_entries': [parse_char_select_param(b, tp) for tp in b.vec_table(dt, 1)],
    }

def extract_customize_item_prohibit_drama_list(b: FbsBuf) -> dict:
    dt = b.data_table()
    return {
        'group_0':         [parse_prohibit_drama(b, tp) for tp in b.vec_table(dt, 0)],
        'group_1':         [parse_prohibit_drama(b, tp) for tp in b.vec_table(dt, 1)],
        'category_values': b.vec_u32(dt, 2),
    }

def extract_battle_motion_list(b: FbsBuf) -> dict:
    dt = b.data_table()
    return {
        'entries':     [parse_battle_motion(b, tp) for tp in b.vec_table(dt, 0)],
        'value_1':  b.f_f32(dt,  1), 'value_2':  b.f_f32(dt,  2),
        'value_3':  b.f_f32(dt,  3), 'value_4':  b.f_f32(dt,  4),
        'value_5':  b.f_f32(dt,  5), 'value_6':  b.f_f32(dt,  6),
        'value_7':  b.f_f32(dt,  7), 'value_8':  b.f_f32(dt,  8),
        'value_9':  b.f_f32(dt,  9), 'value_10': b.f_f32(dt, 10),
        'value_11': b.f_u32(dt, 11),
        'entries_alt': [parse_battle_motion(b, tp) for tp in b.vec_table(dt, 12)],
    }

def extract_arcade_cpu_list(b: FbsBuf) -> dict:
    dt = b.data_table()
    settings_tp = b.f_table(dt, 0)
    settings = {}
    if settings_tp:
        settings = {'unk_0': b.f_u32(settings_tp,0),
                    'unk_1': b.f_u32(settings_tp,1),
                    'unk_2': b.f_u32(settings_tp,2)}
    return {
        'settings':          settings,
        'character_entries': [parse_arcade_cpu_char(b, tp) for tp in b.vec_table(dt, 1)],
        'hash_group_a':      [parse_arcade_cpu_hash(b, tp) for tp in b.vec_table(dt, 2)],
        'hash_group_b':      [parse_arcade_cpu_hash(b, tp) for tp in b.vec_table(dt, 3)],
        'rule_entries':      [parse_arcade_cpu_rule(b, tp) for tp in b.vec_table(dt, 4)],
    }

def extract_ball_recommend_list(b: FbsBuf) -> dict:
    dt = b.data_table()
    return {
        'group_0':   [parse_ball_recommend(b, tp) for tp in b.vec_table(dt, 0)],
        'group_1':   [parse_ball_recommend(b, tp) for tp in b.vec_table(dt, 1)],
        'group_2':   [parse_ball_recommend(b, tp) for tp in b.vec_table(dt, 2)],
        'unk_values': b.vec_u32(dt, 3),
    }

def extract_ball_setting_list(b: FbsBuf) -> dict:
    dt = b.data_table()
    float_ids = {0,1,2,3,4,5,6,7,8,11,12,13,14,16,18,19,20,21,22,23,
                 30,32,41,42,43,46,53,54,56,57,61,62}
    return {f'value_{i}': (b.f_f32(dt,i) if i in float_ids else b.f_u32(dt,i))
            for i in range(72)}

def extract_battle_common_list(b: FbsBuf) -> dict:
    dt = b.data_table()
    return {
        'single_value_entries':    [parse_battle_common_single(b,tp)     for tp in b.vec_table(dt, 0)],
        'char_scale_entries':      [parse_battle_common_char_scale(b,tp) for tp in b.vec_table(dt, 1)],
        'pair_entries':            [parse_battle_common_pair(b,tp)       for tp in b.vec_table(dt, 2)],
        'value_3':  b.f_u32(dt, 3), 'value_4':  b.f_u32(dt, 4),
        'misc_entries':            [parse_battle_common_misc(b,tp)       for tp in b.vec_table(dt, 5)],
        'value_6':  b.f_f32(dt, 6), 'value_7':  b.f_f32(dt, 7),
        'value_8':  b.f_f32(dt, 8), 'value_9':  b.f_u32(dt, 9),
        'value_10': b.f_f32(dt,10), 'value_11': b.f_u32(dt,11),
        'value_12': b.f_u32(dt,12), 'value_13': b.f_u32(dt,13),
    }

def extract_battle_cpu_list(b: FbsBuf) -> dict:
    dt = b.data_table()
    # rank_ex_entry is a single table at some field
    rank_ex_tp = b.f_table(dt, 3)
    return {
        'rank_entries':  [parse_battle_cpu_rank(b,tp) for tp in b.vec_table(dt, 0)],
        'step_entries':  [parse_battle_cpu_step(b,tp) for tp in b.vec_table(dt, 1)],
        'param_values':  b.vec_i32(dt, 2),
        'rank_ex_entry': parse_battle_cpu_rank(b, rank_ex_tp) if rank_ex_tp else {},
    }

def extract_rank_list(b: FbsBuf) -> dict:
    dt = b.data_table()
    groups = []
    for gtp in b.vec_table(dt, 0):
        group_id = b.f_u32(gtp, 0)
        items    = [parse_rank_item(b, itp) for itp in b.vec_table(gtp, 1)]
        groups.append({'group_id': group_id, 'entries': items})
    return {'rank_groups': groups}

def extract_assist_input_list(b: FbsBuf) -> dict:
    return extract_simple_list(b, parse_assist_input)


# ---------------------------------------------------------------------------
#  Bin-name → extractor dispatch
# ---------------------------------------------------------------------------

EXTRACTORS = {
    'customize_item_common_list':          extract_customize_item_common_list,
    'character_list':                      extract_character_list,
    'customize_item_exclusive_list':       extract_customize_item_exclusive_list,
    'area_list':                           extract_area_list,
    'battle_subtitle_info_list':           extract_battle_subtitle_info_list,
    'fate_drama_player_start_list':        extract_fate_drama_player_start_list,
    'jukebox_list':                        extract_jukebox_list,
    'series_list':                         extract_series_list,
    'tam_mission_list':                    extract_tam_mission_list,
    'drama_player_start_list':             extract_drama_player_start_list,
    'stage_list':                          extract_stage_list,
    'ball_property_list':                  extract_ball_property_list,
    'body_cylinder_data_list':             extract_body_cylinder_data_list,
    'customize_item_unique_list':          extract_customize_item_unique_list,
    'character_select_list':               extract_character_select_list,
    'customize_item_prohibit_drama_list':  extract_customize_item_prohibit_drama_list,
    'battle_motion_list':                  extract_battle_motion_list,
    'arcade_cpu_list':                     extract_arcade_cpu_list,
    'ball_recommend_list':                 extract_ball_recommend_list,
    'ball_setting_list':                   extract_ball_setting_list,
    'battle_common_list':                  extract_battle_common_list,
    'battle_cpu_list':                     extract_battle_cpu_list,
    'rank_list':                           extract_rank_list,
    'assist_input_list':                   extract_assist_input_list,
}


# ---------------------------------------------------------------------------
#  Main
# ---------------------------------------------------------------------------

def extract_file(bin_path: str, out_dir: str):
    stem = os.path.splitext(os.path.basename(bin_path))[0]
    extractor = EXTRACTORS.get(stem)
    if extractor is None:
        print(f'  [SKIP] unknown bin type: {stem}')
        return

    with open(bin_path, 'rb') as f:
        data = f.read()

    b = FbsBuf(data)
    try:
        result = extractor(b)
    except Exception as e:
        print(f'  [ERROR] {stem}: {e}')
        import traceback; traceback.print_exc()
        return

    out_path = os.path.join(out_dir, stem + '.json')
    with open(out_path, 'w', encoding='utf-8') as f:
        json.dump(result, f, indent=2, ensure_ascii=False)

    # Count primary entries for the summary line
    count = (len(result.get('entries', result.get('rank_groups',
             result.get('rule_entries', result.get('single_value_entries',
             result.get('group_0', [])))))))
    print(f'  [OK]   {stem}.json  ({count} entries)')


def main():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    bins_dir   = os.path.join(script_dir, 'bins')
    out_dir    = os.path.join(script_dir, 'extracted')
    os.makedirs(out_dir, exist_ok=True)

    if len(sys.argv) >= 2:
        targets = sys.argv[1:]
    else:
        targets = sorted(
            os.path.join(bins_dir, f)
            for f in os.listdir(bins_dir) if f.endswith('.bin')
        )

    print(f'Extracting {len(targets)} bin(s) → {out_dir}')
    for path in targets:
        extract_file(path, out_dir)
    print('Done.')


if __name__ == '__main__':
    main()
