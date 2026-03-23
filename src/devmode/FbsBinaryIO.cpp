#ifdef _DEBUG
// FlatBuffers binary I/O for developer mode
// Handles customize_item_common_list.bin read/write without the flatc compiler.
//
// FlatBuffers format conventions used here:
//   - All integers are little-endian
//   - Root: first 4 bytes = uoffset_t pointing to root table
//   - Table: first int32 = soffset to vtable (vtable_ptr = table_ptr + soffset)
//   - Vtable: uint16 vtable_size, uint16 obj_size, uint16 field_offsets[]
//   - Reference fields: uint32 forward offset from the field's own position
//   - Strings: uint32 length, then UTF-8 bytes, then '\0'
//   - Vectors: uint32 count, then elements

#include "devmode/FbsBinaryIO.h"
#include "data/FieldNames.h"
#include <windows.h>
#include <shobjidl.h>
#include <commdlg.h>
#include <cstdio>
#include <cstring>
#include <algorithm>

#pragma comment(lib, "comdlg32.lib")

// ?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€
//  Low-level read helpers
// ?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€

static uint16_t R16(const uint8_t* b, uint32_t p)
{
    return static_cast<uint16_t>(b[p]) | (static_cast<uint16_t>(b[p + 1]) << 8);
}

static uint32_t R32(const uint8_t* b, uint32_t p)
{
    return static_cast<uint32_t>(b[p])
         | (static_cast<uint32_t>(b[p + 1]) << 8)
         | (static_cast<uint32_t>(b[p + 2]) << 16)
         | (static_cast<uint32_t>(b[p + 3]) << 24);
}

// Returns the byte offset of field[field_id] within table at table_pos.
// Returns 0 if the field is absent.
static uint16_t VtableFieldOff(const uint8_t* b, size_t bufSize,
                                uint32_t table_pos, int field_id)
{
    if (table_pos + 4 > bufSize) return 0;
    int32_t  soff     = static_cast<int32_t>(R32(b, table_pos));
    // FlatBuffers spec: vtable_ptr = table_ptr - soffset
    uint32_t vtab_pos = static_cast<uint32_t>(static_cast<int32_t>(table_pos) - soff);
    if (vtab_pos + 4 > bufSize) return 0;
    uint16_t vtab_size = R16(b, vtab_pos);
    uint32_t foff_pos  = vtab_pos + 4 + static_cast<uint32_t>(field_id) * 2;
    if (foff_pos + 2 > vtab_pos + vtab_size) return 0;
    return R16(b, foff_pos);
}

static uint32_t ReadU32F(const uint8_t* b, size_t sz,
                          uint32_t table_pos, int id, uint32_t def = 0)
{
    uint16_t off = VtableFieldOff(b, sz, table_pos, id);
    if (!off || table_pos + off + 4 > sz) return def;
    return R32(b, table_pos + off);
}

static uint8_t ReadU8F(const uint8_t* b, size_t sz,
                        uint32_t table_pos, int id, uint8_t def = 0)
{
    uint16_t off = VtableFieldOff(b, sz, table_pos, id);
    if (!off || table_pos + off >= sz) return def;
    return b[table_pos + off];
}

static bool ReadBoolF(const uint8_t* b, size_t sz,
                       uint32_t table_pos, int id, bool def = false)
{
    return ReadU8F(b, sz, table_pos, id, def ? 1 : 0) != 0;
}

// Follows a reference field and returns the absolute position of the target.
// Returns 0 if absent.
static uint32_t ReadRef(const uint8_t* b, size_t sz,
                         uint32_t table_pos, int id)
{
    uint16_t off = VtableFieldOff(b, sz, table_pos, id);
    if (!off || table_pos + off + 4 > sz) return 0;
    uint32_t field_pos = table_pos + off;
    uint32_t ref       = R32(b, field_pos);
    return ref ? (field_pos + ref) : 0;
}

static std::string ReadStr(const uint8_t* b, size_t sz,
                            uint32_t table_pos, int id)
{
    uint32_t str_pos = ReadRef(b, sz, table_pos, id);
    if (!str_pos || str_pos + 4 > sz) return {};
    uint32_t len = R32(b, str_pos);
    if (str_pos + 4 + len > sz) return {};
    return std::string(reinterpret_cast<const char*>(b + str_pos + 4), len);
}

static uint32_t VecLength(const uint8_t* b, size_t sz, uint32_t vec_pos)
{
    if (!vec_pos || vec_pos + 4 > sz) return 0;
    return R32(b, vec_pos);
}

// Returns absolute position of the i-th table element in a vector of tables.
static uint32_t VecTableElem(const uint8_t* b, size_t sz,
                              uint32_t vec_pos, uint32_t idx)
{
    uint32_t slot_pos = vec_pos + 4 + idx * 4;
    if (slot_pos + 4 > sz) return 0;
    uint32_t ref = R32(b, slot_pos);
    return ref ? (slot_pos + ref) : 0;
}

// ?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€
//  Binary reader: customize_item_common_list
// ?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€

static bool ReadCustomizeItemCommonBin(const std::vector<uint8_t>& data,
                                       ContentsBinData& bin)
{
    const uint8_t* b  = data.data();
    const size_t   sz = data.size();
    if (sz < 8) return false;

    // Root table
    uint32_t root_pos = R32(b, 0);
    if (!root_pos) return false;

    // field 1 of root = CustomizeItemCommonList table
    uint32_t list_pos = ReadRef(b, sz, root_pos, 1);
    if (!list_pos) return false;

    // field 0 of list = entries vector
    uint32_t vec_pos = ReadRef(b, sz, list_pos, 0);
    if (!vec_pos) return false;

    uint32_t count = VecLength(b, sz, vec_pos);
    bin.commonEntries.reserve(count);

    for (uint32_t i = 0; i < count; ++i)
    {
        uint32_t ep = VecTableElem(b, sz, vec_pos, i);
        if (!ep) continue;

        CustomizeItemCommonEntry e;
        e.item_id      = ReadU32F(b, sz, ep,  0);
        e.item_no      = static_cast<int32_t>(ReadU32F(b, sz, ep,  1));
        {
            auto s = ReadStr(b, sz, ep, 2);
            strncpy_s(e.item_code, s.c_str(), sizeof(e.item_code) - 1);
        }
        e.hash_0       = ReadU32F(b, sz, ep,  3);
        e.hash_1       = ReadU32F(b, sz, ep,  4);
        {
            auto s = ReadStr(b, sz, ep, 5);
            strncpy_s(e.text_key, s.c_str(), sizeof(e.text_key) - 1);
        }
        {
            auto s = ReadStr(b, sz, ep, 6);
            strncpy_s(e.package_id, s.c_str(), sizeof(e.package_id) - 1);
        }
        {
            auto s = ReadStr(b, sz, ep, 7);
            strncpy_s(e.package_sub_id, s.c_str(), sizeof(e.package_sub_id) - 1);
        }
        e.unk_8        = ReadU32F(b, sz, ep,  8);
        e.shop_sort_id = static_cast<int32_t>(ReadU32F(b, sz, ep,  9));
        e.is_enabled   = ReadBoolF(b, sz, ep, 10, true);
        e.unk_11       = ReadU32F(b, sz, ep, 11);
        e.price        = static_cast<int32_t>(ReadU32F(b, sz, ep, 12));
        e.unk_13       = ReadBoolF(b, sz, ep, 13);
        e.category_no  = static_cast<int32_t>(ReadU32F(b, sz, ep, 14));
        e.hash_2       = ReadU32F(b, sz, ep, 15);
        e.unk_16       = ReadBoolF(b, sz, ep, 16);
        e.unk_17       = ReadU32F(b, sz, ep, 17);
        e.hash_3       = ReadU32F(b, sz, ep, 18);
        e.unk_19       = ReadU32F(b, sz, ep, 19);
        e.unk_20       = ReadU32F(b, sz, ep, 20);
        e.unk_21       = ReadU32F(b, sz, ep, 21);
        e.unk_22       = ReadU32F(b, sz, ep, 22);
        e.hash_4       = ReadU32F(b, sz, ep, 23);
        e.rarity       = static_cast<int32_t>(ReadU32F(b, sz, ep, 24));
        e.sort_group   = static_cast<int32_t>(ReadU32F(b, sz, ep, 25));

        bin.commonEntries.push_back(e);
    }

    bin.type = BinType::CustomizeItemCommonList;
    return !bin.commonEntries.empty();
}

// ?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€
//  Low-level write helpers
// ?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€

static void W8(std::vector<uint8_t>& b, uint8_t v) { b.push_back(v); }

static void W16(std::vector<uint8_t>& b, uint16_t v)
{
    b.push_back(v & 0xFF);
    b.push_back((v >> 8) & 0xFF);
}

static void W32(std::vector<uint8_t>& b, uint32_t v)
{
    b.push_back(v & 0xFF);
    b.push_back((v >> 8)  & 0xFF);
    b.push_back((v >> 16) & 0xFF);
    b.push_back((v >> 24) & 0xFF);
}

static void WI32(std::vector<uint8_t>& b, int32_t v) { W32(b, static_cast<uint32_t>(v)); }

// Pad to alignment (counted from start of buffer)
static void Align(std::vector<uint8_t>& b, uint32_t align)
{
    while (b.size() % align) W8(b, 0);
}

// Write uint32 at a previously reserved position
static void PatchU32(std::vector<uint8_t>& b, uint32_t pos, uint32_t val)
{
    b[pos]     =  val        & 0xFF;
    b[pos + 1] = (val >> 8)  & 0xFF;
    b[pos + 2] = (val >> 16) & 0xFF;
    b[pos + 3] = (val >> 24) & 0xFF;
}

// Patch a reference field: store (target_pos - field_pos) at field_pos
static void PatchRef(std::vector<uint8_t>& b, uint32_t field_pos, uint32_t target_pos)
{
    PatchU32(b, field_pos, target_pos - field_pos);
}

// Write a vtable and return its start position
static uint32_t WriteVtable(std::vector<uint8_t>& b,
                              uint16_t obj_size,
                              const uint16_t* fields, int num_fields)
{
    Align(b, 2);
    uint32_t vtab_pos  = static_cast<uint32_t>(b.size());
    uint16_t vtab_size = static_cast<uint16_t>(4 + num_fields * 2);
    W16(b, vtab_size);
    W16(b, obj_size);
    for (int i = 0; i < num_fields; ++i) W16(b, fields[i]);
    return vtab_pos;
}

// Write table soffset and return table start position
static uint32_t BeginTable(std::vector<uint8_t>& b, uint32_t vtab_pos)
{
    Align(b, 4);
    uint32_t table_pos = static_cast<uint32_t>(b.size());
    // FlatBuffers spec: soffset = table_pos - vtab_pos (positive when vtable precedes table)
    WI32(b, static_cast<int32_t>(table_pos) - static_cast<int32_t>(vtab_pos));
    return table_pos;
}

// Write string, return its start position (the length field)
static uint32_t WriteString(std::vector<uint8_t>& b, const char* s)
{
    Align(b, 4);
    uint32_t str_pos = static_cast<uint32_t>(b.size());
    uint32_t len     = static_cast<uint32_t>(strlen(s));
    W32(b, len);
    for (uint32_t i = 0; i < len; ++i) W8(b, static_cast<uint8_t>(s[i]));
    W8(b, 0);  // null terminator
    return str_pos;
}

// ?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€
//  Binary writer: customize_item_common_list
//
//  Entry table field layout (all fields present, sorted by alignment):
//    Offset  4: item_id        (u32, id 0)
//    Offset  8: item_no        (u32, id 1)
//    Offset 12: item_code ref  (u32, id 2)
//    Offset 16: hash_0         (u32, id 3)
//    Offset 20: hash_1         (u32, id 4)
//    Offset 24: text_key ref   (u32, id 5)
//    Offset 28: package_id ref (u32, id 6)
//    Offset 32: pkg_sub ref    (u32, id 7)
//    Offset 36: unk_8          (u32, id 8)
//    Offset 40: shop_sort_id   (u32, id 9)
//    Offset 44: unk_11         (u32, id 11)
//    Offset 48: price          (u32, id 12)
//    Offset 52: category_no    (u32, id 14)
//    Offset 56: hash_2         (u32, id 15)
//    Offset 60: unk_17         (u32, id 17)
//    Offset 64: hash_3         (u32, id 18)
//    Offset 68: unk_19         (u32, id 19)
//    Offset 72: unk_20         (u32, id 20)
//    Offset 76: unk_21         (u32, id 21)
//    Offset 80: unk_22         (u32, id 22)
//    Offset 84: hash_4         (u32, id 23)
//    Offset 88: rarity         (u32, id 24)
//    Offset 92: sort_group     (u32, id 25)
//    Offset 96: is_enabled     (bool, id 10)
//    Offset 97: unk_13         (bool, id 13)
//    Offset 98: unk_16         (bool, id 16)
//    Offset 99: pad
//    Total object size: 100
// ?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€

static std::vector<uint8_t> BuildCustomizeItemCommonBin(const ContentsBinData& bin,
                                                         uint8_t version = 1)
{
    std::vector<uint8_t> b;
    b.reserve(64 * 1024);

    // Placeholder for root offset at position 0
    uint32_t root_offset_pos = 0;
    W32(b, 0);

    // Root vtable: 2 fields (version@8, data@4), obj_size=12
    static const uint16_t k_root_fields[2] = { 8, 4 };
    uint32_t root_vtab = WriteVtable(b, 12, k_root_fields, 2);

    // Root table
    uint32_t root_table = BeginTable(b, root_vtab);
    uint32_t root_data_ref_pos = static_cast<uint32_t>(b.size());
    W32(b, 0);                  // placeholder: data ref (field 1, offset 4)
    W8(b, version);             // field 0: version (offset 8)
    Align(b, 4);                // pad to 12 total

    // List vtable: 1 field (entries@4), obj_size=8
    static const uint16_t k_list_fields[1] = { 4 };
    uint32_t list_vtab = WriteVtable(b, 8, k_list_fields, 1);

    // List table
    uint32_t list_table = BeginTable(b, list_vtab);
    uint32_t list_vec_ref_pos = static_cast<uint32_t>(b.size());
    W32(b, 0);  // placeholder: entries vector ref

    // Fix root_data_ref ??list_table
    PatchRef(b, root_data_ref_pos, list_table);

    // Entries vector
    Align(b, 4);
    uint32_t vec_pos = static_cast<uint32_t>(b.size());
    const uint32_t N = static_cast<uint32_t>(bin.commonEntries.size());
    W32(b, N);

    // Reserve N entry-ref slots
    std::vector<uint32_t> entry_ref_slots(N);
    for (uint32_t i = 0; i < N; ++i)
    {
        entry_ref_slots[i] = static_cast<uint32_t>(b.size());
        W32(b, 0);
    }

    // Fix list_vec_ref ??vector
    PatchRef(b, list_vec_ref_pos, vec_pos);

    // Entry vtable field offsets (26 fields, ordered by id)
    static const uint16_t k_entry_fields[26] = {
         4,  8, 12, 16, 20, 24, 28, 32,   // id 0-7
        36, 40, 96, 44, 48, 97, 52, 56,   // id 8-15
        98, 60, 64, 68, 72, 76, 80, 84,   // id 16-23
        88, 92                             // id 24-25
    };

    for (uint32_t i = 0; i < N; ++i)
    {
        const auto& e = bin.commonEntries[i];

        // Entry vtable
        uint32_t entry_vtab = WriteVtable(b, 100, k_entry_fields, 26);

        // Entry table
        uint32_t entry_table = BeginTable(b, entry_vtab);

        // Fix vector slot ??entry table
        PatchRef(b, entry_ref_slots[i], entry_table);

        // 23 Ã— uint32 fields + 4 string-ref placeholders (all 4 bytes each)
        W32(b, e.item_id);                          // id 0, offset 4
        W32(b, static_cast<uint32_t>(e.item_no));   // id 1, offset 8
        uint32_t item_code_ref = static_cast<uint32_t>(b.size());
        W32(b, 0);                                  // id 2 (string ref), offset 12
        W32(b, e.hash_0);                           // id 3, offset 16
        W32(b, e.hash_1);                           // id 4, offset 20
        uint32_t text_key_ref = static_cast<uint32_t>(b.size());
        W32(b, 0);                                  // id 5 (string ref), offset 24
        uint32_t pkg_id_ref = static_cast<uint32_t>(b.size());
        W32(b, 0);                                  // id 6 (string ref), offset 28
        uint32_t pkg_sub_ref = static_cast<uint32_t>(b.size());
        W32(b, 0);                                  // id 7 (string ref), offset 32
        W32(b, e.unk_8);                            // id 8, offset 36
        W32(b, static_cast<uint32_t>(e.shop_sort_id)); // id 9, offset 40
        // (id 10 is bool @ offset 96)
        W32(b, e.unk_11);                           // id 11, offset 44
        W32(b, static_cast<uint32_t>(e.price));     // id 12, offset 48
        // (id 13 is bool @ offset 97)
        W32(b, static_cast<uint32_t>(e.category_no)); // id 14, offset 52
        W32(b, e.hash_2);                           // id 15, offset 56
        // (id 16 is bool @ offset 98)
        W32(b, e.unk_17);                           // id 17, offset 60
        W32(b, e.hash_3);                           // id 18, offset 64
        W32(b, e.unk_19);                           // id 19, offset 68
        W32(b, e.unk_20);                           // id 20, offset 72
        W32(b, e.unk_21);                           // id 21, offset 76
        W32(b, e.unk_22);                           // id 22, offset 80
        W32(b, e.hash_4);                           // id 23, offset 84
        W32(b, static_cast<uint32_t>(e.rarity));    // id 24, offset 88
        W32(b, static_cast<uint32_t>(e.sort_group)); // id 25, offset 92

        // Bool fields at offsets 96, 97, 98
        W8(b, e.is_enabled ? 1 : 0);  // id 10
        W8(b, e.unk_13    ? 1 : 0);   // id 13
        W8(b, e.unk_16    ? 1 : 0);   // id 16
        W8(b, 0);                      // padding ??total 100 bytes

        // Write strings and patch refs
        PatchRef(b, item_code_ref, WriteString(b, e.item_code));
        PatchRef(b, text_key_ref,  WriteString(b, e.text_key));
        PatchRef(b, pkg_id_ref,    WriteString(b, e.package_id));
        PatchRef(b, pkg_sub_ref,   WriteString(b, e.package_sub_id));
    }

    // Patch root offset (absolute position of root table from byte 0)
    PatchU32(b, root_offset_pos, root_table);

    return b;
}

// ?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€
//  Wide string helpers
// ?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€

static std::string WideToUtf8(const wchar_t* w)
{
    if (!w || !*w) return {};
    const int len = WideCharToMultiByte(CP_UTF8, 0, w, -1, nullptr, 0, nullptr, nullptr);
    std::string s(len - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w, -1, &s[0], len, nullptr, nullptr);
    return s;
}

static std::wstring Utf8ToWide(const std::string& s)
{
    if (s.empty()) return {};
    const int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    std::wstring w(len - 1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &w[0], len);
    return w;
}

// ?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€
//  TSV helpers
// ?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€

// Build TSV header from FieldNames::CommonItem (display names, not JSON keys)
static std::string BuildTsvHeader()
{
    std::string h;
    for (int i = 0; i < FieldNames::CommonItemCount; ++i)
    {
        if (i > 0) h += '\t';
        h += FieldNames::CommonItem[i];
    }
    return h;
}

// Replace any tab or newline in a string field with a space (TSV safety)
static std::string TsvEsc(const char* s)
{
    std::string r(s);
    for (char& c : r)
        if (c == '\t' || c == '\n' || c == '\r') c = ' ';
    return r;
}

static bool WriteTsvCustomizeItemCommon(FILE* f, const ContentsBinData& bin)
{
    const std::string hdr = BuildTsvHeader();
    fprintf(f, "%s\n", hdr.c_str());
    for (const auto& e : bin.commonEntries)
    {
        fprintf(f,
            "%u\t%d\t%s\t%u\t%u\t%s\t%s\t%s"
            "\t%u\t%d\t%d\t%u\t%d\t%d\t%d"
            "\t%u\t%d\t%u\t%u\t%u\t%u\t%u\t%u"
            "\t%u\t%d\t%d\n",
            e.item_id,
            e.item_no,
            TsvEsc(e.item_code).c_str(),
            e.hash_0,
            e.hash_1,
            TsvEsc(e.text_key).c_str(),
            TsvEsc(e.package_id).c_str(),
            TsvEsc(e.package_sub_id).c_str(),
            e.unk_8,
            e.shop_sort_id,
            e.is_enabled ? 1 : 0,
            e.unk_11,
            e.price,
            e.unk_13 ? 1 : 0,
            e.category_no,
            e.hash_2,
            e.unk_16 ? 1 : 0,
            e.unk_17,
            e.hash_3,
            e.unk_19,
            e.unk_20,
            e.unk_21,
            e.unk_22,
            e.hash_4,
            e.rarity,
            e.sort_group
        );
    }
    return true;
}

// Split a tab-separated line into fields (modifies the string in-place)
static std::vector<std::string> SplitTsv(const std::string& line)
{
    std::vector<std::string> fields;
    size_t start = 0;
    for (size_t i = 0; i <= line.size(); ++i)
    {
        if (i == line.size() || line[i] == '\t')
        {
            fields.push_back(line.substr(start, i - start));
            start = i + 1;
        }
    }
    return fields;
}

static bool ReadTsvCustomizeItemCommon(FILE* f, ContentsBinData& bin)
{
    char lineBuf[4096];
    size_t len;

    // Read header row and validate column count (names may differ if renamed)
    if (!fgets(lineBuf, sizeof(lineBuf), f)) return false;
    {
        len = strlen(lineBuf);
        while (len > 0 && (lineBuf[len - 1] == '\n' || lineBuf[len - 1] == '\r'))
            lineBuf[--len] = '\0';
        auto headerFields = SplitTsv(std::string(lineBuf));
        if ((int)headerFields.size() < FieldNames::CommonItemCount)
            return false;  // too few columns
    }

    bin.commonEntries.clear();

    while (fgets(lineBuf, sizeof(lineBuf), f))
    {
        len = strlen(lineBuf);
        while (len > 0 && (lineBuf[len - 1] == '\n' || lineBuf[len - 1] == '\r'))
            lineBuf[--len] = '\0';
        if (len == 0) continue;  // skip blank lines

        auto fields = SplitTsv(std::string(lineBuf));
        if (fields.size() < 26) continue;  // malformed row

        CustomizeItemCommonEntry e;
        int fi = 0;
        auto U32  = [&]() -> uint32_t { return (uint32_t)strtoul(fields[fi++].c_str(), nullptr, 10); };
        auto I32  = [&]() -> int32_t  { return (int32_t)strtol(fields[fi++].c_str(), nullptr, 10); };
        auto Bool = [&]() -> bool     { auto v = fields[fi++]; return v == "1" || v == "true"; };
        auto Str  = [&](char* dst, size_t sz) {
            strncpy_s(dst, sz, fields[fi].c_str(), sz - 1);
            ++fi;
        };

        e.item_id      = U32();
        e.item_no      = I32();
        Str(e.item_code, sizeof(e.item_code));
        e.hash_0       = U32();
        e.hash_1       = U32();
        Str(e.text_key, sizeof(e.text_key));
        Str(e.package_id, sizeof(e.package_id));
        Str(e.package_sub_id, sizeof(e.package_sub_id));
        e.unk_8        = U32();
        e.shop_sort_id = I32();
        e.is_enabled   = Bool();
        e.unk_11       = U32();
        e.price        = I32();
        e.unk_13       = Bool();
        e.category_no  = I32();
        e.hash_2       = U32();
        e.unk_16       = Bool();
        e.unk_17       = U32();
        e.hash_3       = U32();
        e.unk_19       = U32();
        e.unk_20       = U32();
        e.unk_21       = U32();
        e.unk_22       = U32();
        e.hash_4       = U32();
        e.rarity       = I32();
        e.sort_group   = I32();

        bin.commonEntries.push_back(e);
    }

    return !bin.commonEntries.empty();
}

// ?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€
//  Public API
// ?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€

namespace FbsBinaryIO
{
    bool ImportBin(const std::string& path, ContentsBinData& out)
    {
        FILE* f = nullptr;
        if (fopen_s(&f, path.c_str(), "rb") != 0 || !f) return false;
        fseek(f, 0, SEEK_END);
        const size_t sz = static_cast<size_t>(ftell(f));
        fseek(f, 0, SEEK_SET);
        std::vector<uint8_t> data(sz);
        fread(data.data(), 1, sz, f);
        fclose(f);

        // Detect bin type from filename
        std::string name = path;
        const size_t sl = name.find_last_of("/\\");
        if (sl != std::string::npos) name = name.substr(sl + 1);
        // Strip .json suffix if present
        if (name.size() > 5 && name.substr(name.size() - 5) == ".json")
            name = name.substr(0, name.size() - 5);

        out.name = name;

        if (name == "customize_item_common_list.bin")
            return ReadCustomizeItemCommonBin(data, out);

        return false;  // unsupported type
    }

    bool ExportBin(const std::string& path, const ContentsBinData& bin)
    {
        std::vector<uint8_t> data;

        if (bin.type == BinType::CustomizeItemCommonList)
            data = BuildCustomizeItemCommonBin(bin);
        else
            return false;

        FILE* f = nullptr;
        if (fopen_s(&f, path.c_str(), "wb") != 0 || !f) return false;
        fwrite(data.data(), 1, data.size(), f);
        fclose(f);
        return true;
    }

    std::string OpenImportDialog()
    {
        wchar_t szFile[1024] = {};
        OPENFILENAMEW ofn    = {};
        ofn.lStructSize = sizeof(ofn);
        ofn.lpstrFile   = szFile;
        ofn.nMaxFile    = static_cast<DWORD>(sizeof(szFile) / sizeof(wchar_t));
        ofn.lpstrFilter = L"FBS Binary Files\0*.bin\0All Files\0*.*\0";
        ofn.Flags       = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
        if (!GetOpenFileNameW(&ofn)) return {};
        return WideToUtf8(szFile);
    }

    std::string OpenExportFolderDialog()
    {
        IFileDialog* pfd = nullptr;
        if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr,
                                    CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pfd))))
            return {};

        DWORD opts = 0;
        pfd->GetOptions(&opts);
        pfd->SetOptions(opts | FOS_PICKFOLDERS | FOS_PATHMUSTEXIST);

        if (FAILED(pfd->Show(nullptr))) { pfd->Release(); return {}; }

        IShellItem* psi = nullptr;
        if (FAILED(pfd->GetResult(&psi))) { pfd->Release(); return {}; }

        PWSTR pszPath = nullptr;
        psi->GetDisplayName(SIGDN_FILESYSPATH, &pszPath);
        std::wstring result(pszPath);
        CoTaskMemFree(pszPath);
        psi->Release();
        pfd->Release();
        return WideToUtf8(result.c_str());
    }

    bool ExportTsv(const std::string& path, const ContentsBinData& bin)
    {
        FILE* f = nullptr;
        if (fopen_s(&f, path.c_str(), "wt") != 0 || !f) return false;
        bool ok = false;
        if (bin.type == BinType::CustomizeItemCommonList)
            ok = WriteTsvCustomizeItemCommon(f, bin);
        fclose(f);
        return ok;
    }

    bool ImportTsv(const std::string& path, ContentsBinData& bin)
    {
        FILE* f = nullptr;
        if (fopen_s(&f, path.c_str(), "rt") != 0 || !f) return false;
        bool ok = false;
        if (bin.type == BinType::CustomizeItemCommonList)
            ok = ReadTsvCustomizeItemCommon(f, bin);
        fclose(f);
        return ok;
    }

    std::string OpenTsvSaveDialog(const std::string& defaultName)
    {
        std::wstring wDefault = Utf8ToWide(defaultName);
        wchar_t szFile[1024]  = {};
        if (wDefault.size() < sizeof(szFile) / sizeof(wchar_t))
            wcscpy_s(szFile, wDefault.c_str());

        OPENFILENAMEW ofn    = {};
        ofn.lStructSize = sizeof(ofn);
        ofn.lpstrFile   = szFile;
        ofn.nMaxFile    = static_cast<DWORD>(sizeof(szFile) / sizeof(wchar_t));
        ofn.lpstrFilter = L"Tab-Separated Values\0*.tsv\0All Files\0*.*\0";
        ofn.lpstrDefExt = L"tsv";
        ofn.Flags       = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
        if (!GetSaveFileNameW(&ofn)) return {};
        return WideToUtf8(szFile);
    }

    std::string OpenTsvOpenDialog()
    {
        wchar_t szFile[1024] = {};
        OPENFILENAMEW ofn    = {};
        ofn.lStructSize = sizeof(ofn);
        ofn.lpstrFile   = szFile;
        ofn.nMaxFile    = static_cast<DWORD>(sizeof(szFile) / sizeof(wchar_t));
        ofn.lpstrFilter = L"Tab-Separated Values\0*.tsv\0All Files\0*.*\0";
        ofn.Flags       = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
        if (!GetOpenFileNameW(&ofn)) return {};
        return WideToUtf8(szFile);
    }
}

#endif // _DEBUG
