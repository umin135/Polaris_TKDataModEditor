// .tkmod file I/O
// Format: uncompressed ZIP containing:
//   mod_info.json
//   fbsdata_mod/<bin_name>.json   (one per added bin)

#include "fbsdata/io/TkmodIO.h"
#include <windows.h>
#include <commdlg.h>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <cstdint>

#pragma comment(lib, "comdlg32.lib")

// ???????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????//  CRC-32 (required by ZIP format)
// ???????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????
static uint32_t s_crcTable[256];
static bool     s_crcInit = false;

static void InitCRC32()
{
    for (uint32_t i = 0; i < 256; ++i)
    {
        uint32_t c = i;
        for (int j = 0; j < 8; ++j)
            c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
        s_crcTable[i] = c;
    }
    s_crcInit = true;
}

static uint32_t CRC32(const uint8_t* data, size_t size)
{
    if (!s_crcInit) InitCRC32();
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < size; ++i)
        crc = s_crcTable[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    return crc ^ 0xFFFFFFFFu;
}

// ???????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????//  Uncompressed ZIP writer
// ???????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????
static void WriteU16(std::vector<uint8_t>& buf, uint16_t v)
{
    buf.push_back(v & 0xFF);
    buf.push_back((v >> 8) & 0xFF);
}

static void WriteU32(std::vector<uint8_t>& buf, uint32_t v)
{
    buf.push_back(v & 0xFF);
    buf.push_back((v >> 8)  & 0xFF);
    buf.push_back((v >> 16) & 0xFF);
    buf.push_back((v >> 24) & 0xFF);
}

static void WriteBytes(std::vector<uint8_t>& buf, const void* data, size_t len)
{
    const uint8_t* p = reinterpret_cast<const uint8_t*>(data);
    buf.insert(buf.end(), p, p + len);
}

struct ZipEntry
{
    std::string         name;
    std::vector<uint8_t> data;
    uint32_t            crc    = 0;
    uint32_t            offset = 0;  // byte offset of local header
};

static bool WriteZip(const std::string& path, std::vector<ZipEntry>& entries)
{
    std::vector<uint8_t> buf;
    buf.reserve(64 * 1024);

    // Local file entries
    for (auto& e : entries)
    {
        e.offset = static_cast<uint32_t>(buf.size());
        e.crc    = CRC32(e.data.data(), e.data.size());
        const uint32_t sz = static_cast<uint32_t>(e.data.size());

        WriteU32(buf, 0x04034b50u);                 // local file header sig
        WriteU16(buf, 20);                           // version needed
        WriteU16(buf, 0);                            // flags
        WriteU16(buf, 0);                            // compression: stored
        WriteU16(buf, 0);                            // mod time
        WriteU16(buf, 0x0021);                       // mod date (Jan 1, 1980)
        WriteU32(buf, e.crc);
        WriteU32(buf, sz);                           // compressed size
        WriteU32(buf, sz);                           // uncompressed size
        WriteU16(buf, static_cast<uint16_t>(e.name.size()));
        WriteU16(buf, 0);                            // extra field length
        WriteBytes(buf, e.name.data(), e.name.size());
        WriteBytes(buf, e.data.data(), e.data.size());
    }

    // Central directory
    const uint32_t cdOffset = static_cast<uint32_t>(buf.size());
    for (const auto& e : entries)
    {
        const uint32_t sz = static_cast<uint32_t>(e.data.size());
        WriteU32(buf, 0x02014b50u);                 // central dir sig
        WriteU16(buf, 20);                           // version made by
        WriteU16(buf, 20);                           // version needed
        WriteU16(buf, 0);                            // flags
        WriteU16(buf, 0);                            // compression
        WriteU16(buf, 0);                            // mod time
        WriteU16(buf, 0x0021);                       // mod date
        WriteU32(buf, e.crc);
        WriteU32(buf, sz);                           // compressed size
        WriteU32(buf, sz);                           // uncompressed size
        WriteU16(buf, static_cast<uint16_t>(e.name.size()));
        WriteU16(buf, 0);                            // extra field length
        WriteU16(buf, 0);                            // comment length
        WriteU16(buf, 0);                            // disk number start
        WriteU16(buf, 0);                            // internal attrs
        WriteU32(buf, 0);                            // external attrs
        WriteU32(buf, e.offset);
        WriteBytes(buf, e.name.data(), e.name.size());
    }
    const uint32_t cdSize = static_cast<uint32_t>(buf.size()) - cdOffset;

    // End of central directory
    WriteU32(buf, 0x06054b50u);
    WriteU16(buf, 0);
    WriteU16(buf, 0);
    WriteU16(buf, static_cast<uint16_t>(entries.size()));
    WriteU16(buf, static_cast<uint16_t>(entries.size()));
    WriteU32(buf, cdSize);
    WriteU32(buf, cdOffset);
    WriteU16(buf, 0);  // comment length

    FILE* f = nullptr;
    fopen_s(&f, path.c_str(), "wb");
    if (!f) return false;
    fwrite(buf.data(), 1, buf.size(), f);
    fclose(f);
    return true;
}

// ???????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????//  Uncompressed ZIP reader
// ???????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????
static uint16_t ReadU16(const uint8_t* p)
{
    return static_cast<uint16_t>(p[0]) | (static_cast<uint16_t>(p[1]) << 8);
}

static uint32_t ReadU32(const uint8_t* p)
{
    return static_cast<uint32_t>(p[0])
         | (static_cast<uint32_t>(p[1]) << 8)
         | (static_cast<uint32_t>(p[2]) << 16)
         | (static_cast<uint32_t>(p[3]) << 24);
}

struct ZipFileEntry
{
    std::string          name;
    std::vector<uint8_t> data;
};

static std::vector<ZipFileEntry> ReadZip(const std::string& path)
{
    FILE* f = nullptr;
    fopen_s(&f, path.c_str(), "rb");
    if (!f) return {};

    fseek(f, 0, SEEK_END);
    const size_t fileSize = static_cast<size_t>(ftell(f));
    fseek(f, 0, SEEK_SET);

    std::vector<uint8_t> buf(fileSize);
    fread(buf.data(), 1, fileSize, f);
    fclose(f);

    std::vector<ZipFileEntry> result;

    // Scan for local file headers (signature 0x04034b50)
    size_t pos = 0;
    while (pos + 30 <= fileSize)
    {
        if (ReadU32(buf.data() + pos) != 0x04034b50u)
        {
            ++pos;
            continue;
        }

        const uint16_t method   = ReadU16(buf.data() + pos + 8);
        const uint32_t compSize = ReadU32(buf.data() + pos + 18);
        const uint16_t nameLen  = ReadU16(buf.data() + pos + 26);
        const uint16_t extraLen = ReadU16(buf.data() + pos + 28);

        if (method != 0) { pos += 30; continue; }  // skip compressed entries

        const size_t dataStart = pos + 30 + nameLen + extraLen;
        if (dataStart + compSize > fileSize) break;

        ZipFileEntry e;
        e.name.assign(reinterpret_cast<const char*>(buf.data() + pos + 30), nameLen);
        e.data.assign(buf.data() + dataStart, buf.data() + dataStart + compSize);
        result.push_back(std::move(e));

        pos = dataStart + compSize;
    }

    return result;
}

// ???????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????//  JSON builder helpers
// ???????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????
static std::string JsonEsc(const char* s)
{
    std::string r;
    while (*s)
    {
        if      (*s == '"')  r += "\\\"";
        else if (*s == '\\') r += "\\\\";
        else                 r += *s;
        ++s;
    }
    return r;
}

static std::string BuildCustomizeItemCommonJson(const ContentsBinData& bin)
{
    std::string j = "{\n  \"version\": 1,\n  \"data\": {\n    \"entries\": [\n";
    for (size_t i = 0; i < bin.commonEntries.size(); ++i)
    {
        if (i > 0) j += ",\n";
        const auto& e = bin.commonEntries[i];
        j += "      {\n";
        j += "        \"id_0\": "  + std::to_string(e.item_id)                              + ",\n";
        j += "        \"id_1\": "  + std::to_string(e.item_no)                              + ",\n";
        j += "        \"id_2\": \"" + JsonEsc(e.item_code)                                  + "\",\n";
        j += "        \"id_3\": "  + std::to_string(e.hash_0)                               + ",\n";
        j += "        \"id_4\": "  + std::to_string(e.hash_1)                               + ",\n";
        j += "        \"id_5\": \"" + JsonEsc(e.text_key)                                   + "\",\n";
        j += "        \"id_6\": \"" + JsonEsc(e.package_id)                                 + "\",\n";
        j += "        \"id_7\": \"" + JsonEsc(e.package_sub_id)                             + "\",\n";
        j += "        \"id_8\": "  + std::to_string(e.unk_8)                                + ",\n";
        j += "        \"id_9\": "  + std::to_string(e.shop_sort_id)                         + ",\n";
        j += std::string("        \"id_10\": ") + (e.is_enabled ? "true" : "false")         + ",\n";
        j += "        \"id_11\": " + std::to_string(e.unk_11)                               + ",\n";
        j += "        \"id_12\": " + std::to_string(e.price)                                + ",\n";
        j += std::string("        \"id_13\": ") + (e.unk_13 ? "true" : "false")             + ",\n";
        j += "        \"id_14\": " + std::to_string(e.category_no)                          + ",\n";
        j += "        \"id_15\": " + std::to_string(e.hash_2)                               + ",\n";
        j += std::string("        \"id_16\": ") + (e.unk_16 ? "true" : "false")             + ",\n";
        j += "        \"id_17\": " + std::to_string(e.unk_17)                               + ",\n";
        j += "        \"id_18\": " + std::to_string(e.hash_3)                               + ",\n";
        j += "        \"id_19\": " + std::to_string(e.unk_19)                               + ",\n";
        j += "        \"id_20\": " + std::to_string(e.unk_20)                               + ",\n";
        j += "        \"id_21\": " + std::to_string(e.unk_21)                               + ",\n";
        j += "        \"id_22\": " + std::to_string(e.unk_22)                               + ",\n";
        j += "        \"id_23\": " + std::to_string(e.hash_4)                               + ",\n";
        j += "        \"id_24\": " + std::to_string(e.rarity)                               + ",\n";
        j += "        \"id_25\": " + std::to_string(e.sort_group)                           + "\n";
        j += "      }";
    }
    j += "\n    ]\n  }\n}\n";
    return j;
}

static std::string BuildModInfoJson()
{
    return "{\n"
           "  \"Author\": \"\",\n"
           "  \"Description\": \"\",\n"
           "  \"Version\": \"1.0.0\"\n"
           "}\n";
}

// ???????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????//  JSON parser helpers (minimal, for known structures)
// ???????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????
static const char* SkipWS(const char* p)
{
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') ++p;
    return p;
}

// Returns pointer just past the matching } (including it), or nullptr on error.
static const char* SkipObject(const char* p)
{
    if (*p != '{') return nullptr;
    int depth = 1;
    ++p;
    while (*p && depth > 0)
    {
        if      (*p == '{') ++depth;
        else if (*p == '}') --depth;
        else if (*p == '"')
        {
            ++p;
            while (*p && *p != '"')
            {
                if (*p == '\\') ++p;
                ++p;
            }
        }
        ++p;
    }
    return p;
}

// Find "key": inside [start, end) and position p after the ':' + whitespace.
// Returns nullptr if not found.
static const char* FindField(const char* start, const char* end, const char* key)
{
    char needle[256];
    snprintf(needle, sizeof(needle), "\"%s\"", key);
    const size_t nl = strlen(needle);

    const char* p = start;
    while (p && p < end)
    {
        p = (const char*)memchr(p, '"', end - p);
        if (!p) break;
        if (strncmp(p, needle, nl) == 0)
        {
            p += nl;
            p = SkipWS(p);
            if (*p == ':') { p = SkipWS(p + 1); return p; }
        }
        else ++p;
    }
    return nullptr;
}

static bool ParseString(const char* p, const char* end, char* out, size_t maxLen)
{
    if (!p || p >= end || *p != '"') return false;
    ++p;
    size_t i = 0;
    while (p < end && *p != '"' && i < maxLen - 1)
    {
        if (*p == '\\' && p + 1 < end)
        {
            ++p;
            switch (*p) {
            case '"': case '\\': case '/': out[i++] = *p; break;
            case 'n': out[i++] = '\n'; break;
            case 't': out[i++] = '\t'; break;
            default:  out[i++] = *p;  break;
            }
        }
        else out[i++] = *p;
        ++p;
    }
    out[i] = '\0';
    return true;
}

static bool ParseUInt32(const char* p, const char* end, uint32_t& out)
{
    if (!p || p >= end) return false;
    char* ep;
    out = static_cast<uint32_t>(strtoul(p, &ep, 10));
    return ep != p;
}

static bool ParseInt32(const char* p, const char* end, int32_t& out)
{
    if (!p || p >= end) return false;
    char* ep;
    out = static_cast<int32_t>(strtol(p, &ep, 10));
    return ep != p;
}

static bool ParseBool(const char* p, const char* end, bool& out)
{
    if (!p || p >= end) return false;
    if (strncmp(p, "true",  4) == 0) { out = true;  return true; }
    if (strncmp(p, "false", 5) == 0) { out = false; return true; }
    return false;
}

static bool ParseFloat(const char* p, const char* end, float& out)
{
    if (!p || p >= end) return false;
    char* ep;
    out = strtof(p, &ep);
    return ep != p;
}

static bool ParseCustomizeItemCommonEntry(const char* start, const char* end,
                                          CustomizeItemCommonEntry& e)
{
#define PU32(key, field) { auto* fp = FindField(start,end,key); ParseUInt32(fp,end,e.field); }
#define PI32(key, field) { auto* fp = FindField(start,end,key); ParseInt32(fp,end,e.field);  }
#define PBOOL(key,field) { auto* fp = FindField(start,end,key); ParseBool(fp,end,e.field);   }
#define PSTR(key, field) { auto* fp = FindField(start,end,key); ParseString(fp,end,e.field,sizeof(e.field)); }

    PU32("id_0",  item_id);       PI32("id_1",  item_no);       PSTR("id_2",  item_code);
    PU32("id_3",  hash_0);        PU32("id_4",  hash_1);        PSTR("id_5",  text_key);
    PSTR("id_6",  package_id);    PSTR("id_7",  package_sub_id);
    PU32("id_8",  unk_8);         PI32("id_9",  shop_sort_id);  PBOOL("id_10",is_enabled);
    PU32("id_11", unk_11);        PI32("id_12", price);         PBOOL("id_13",unk_13);
    PI32("id_14", category_no);   PU32("id_15", hash_2);        PBOOL("id_16",unk_16);
    PU32("id_17", unk_17);        PU32("id_18", hash_3);
    PU32("id_19", unk_19);        PU32("id_20", unk_20);
    PU32("id_21", unk_21);        PU32("id_22", unk_22);
    PU32("id_23", hash_4);        PI32("id_24", rarity);        PI32("id_25", sort_group);

#undef PU32
#undef PI32
#undef PBOOL
#undef PSTR
    return true;
}

static bool ParseCustomizeItemCommonJson(const std::string& json, ContentsBinData& bin)
{
    const char* p = json.c_str();

    // Navigate to "entries": [
    const char* entriesField = strstr(p, "\"entries\"");
    if (!entriesField) return false;
    const char* arr = strchr(entriesField, '[');
    if (!arr) return false;
    ++arr;

    while (true)
    {
        arr = SkipWS(arr);
        if (*arr == ']' || *arr == '\0') break;
        if (*arr == '{')
        {
            const char* objEnd = SkipObject(arr);
            if (!objEnd) break;

            CustomizeItemCommonEntry entry;
            ParseCustomizeItemCommonEntry(arr, objEnd, entry);
            bin.commonEntries.push_back(entry);
            arr = objEnd;
        }
        // Skip commas and whitespace between entries
        while (*arr == ',' || *arr == ' ' || *arr == '\n' || *arr == '\r' || *arr == '\t')
            ++arr;
    }
    return !bin.commonEntries.empty();
}

// -----------------------------------------------------------------------------
//  character_list JSON builder
// -----------------------------------------------------------------------------

static std::string BuildCharacterListJson(const ContentsBinData& bin)
{
    std::string j = "{\n  \"version\": 1,\n  \"data\": {\n    \"entries\": [\n";
    for (size_t i = 0; i < bin.characterEntries.size(); ++i)
    {
        if (i > 0) j += ",\n";
        const auto& e = bin.characterEntries[i];
        char fBuf[32];
        snprintf(fBuf, sizeof(fBuf), "%g", e.camera_offset);

        j += "      {\n";
        j += "        \"id_0\": \""  + JsonEsc(e.character_code)                            + "\",\n";
        j += "        \"id_1\": "   + std::to_string(e.name_hash)                           + ",\n";
        j += std::string("        \"id_2\": ")  + (e.is_enabled    ? "true" : "false")      + ",\n";
        j += std::string("        \"id_3\": ")  + (e.is_selectable ? "true" : "false")      + ",\n";
        j += "        \"id_4\": \"" + JsonEsc(e.group)                                      + "\",\n";
        j += std::string("        \"id_5\": ")  + fBuf                                      + ",\n";
        j += std::string("        \"id_6\": ")  + (e.is_playable   ? "true" : "false")      + ",\n";
        j += "        \"id_7\": "   + std::to_string(e.sort_order)                          + ",\n";
        j += "        \"id_8\": \"" + JsonEsc(e.full_name_key)                              + "\",\n";
        j += "        \"id_9\": \"" + JsonEsc(e.short_name_jp_key)                          + "\",\n";
        j += "        \"id_10\": \"" + JsonEsc(e.short_name_key)                            + "\",\n";
        j += "        \"id_11\": \"" + JsonEsc(e.origin_key)                                + "\",\n";
        j += "        \"id_12\": \"" + JsonEsc(e.fighting_style_key)                        + "\",\n";
        j += "        \"id_13\": \"" + JsonEsc(e.height_key)                                + "\",\n";
        j += "        \"id_14\": \"" + JsonEsc(e.weight_key)                                + "\"\n";
        j += "      }";
    }
    j += "\n    ]\n  }\n}\n";
    return j;
}

// -----------------------------------------------------------------------------
//  customize_item_exclusive_list JSON builder
// -----------------------------------------------------------------------------

static std::string BuildExclusiveRuleArrayJson(
    const std::vector<CustomizeExclusiveRuleEntry>& entries, int indent)
{
    std::string pad(indent, ' ');
    std::string j = "[\n";
    for (size_t i = 0; i < entries.size(); ++i)
    {
        if (i > 0) j += ",\n";
        const auto& e = entries[i];
        j += pad + "      {\n";
        j += pad + "        \"id_0\": " + std::to_string(e.item_id)     + ",\n";
        j += pad + "        \"id_1\": " + std::to_string(e.hash)        + ",\n";
        j += pad + "        \"id_2\": " + std::to_string(e.link_type)   + ",\n";
        j += pad + "        \"id_3\": " + std::to_string(e.ref_item_id) + "\n";
        j += pad + "      }";
    }
    j += "\n" + pad + "    ]";
    return j;
}

static std::string BuildExclusivePairArrayJson(
    const std::vector<CustomizeExclusivePairEntry>& entries, int indent)
{
    std::string pad(indent, ' ');
    std::string j = "[\n";
    for (size_t i = 0; i < entries.size(); ++i)
    {
        if (i > 0) j += ",\n";
        const auto& e = entries[i];
        j += pad + "      {\n";
        j += pad + "        \"id_0\": " + std::to_string(e.item_id_a) + ",\n";
        j += pad + "        \"id_1\": " + std::to_string(e.item_id_b) + ",\n";
        j += pad + "        \"id_2\": " + std::to_string(e.flag)      + "\n";
        j += pad + "      }";
    }
    j += "\n" + pad + "    ]";
    return j;
}

static std::string BuildCustomizeItemExclusiveListJson(const ContentsBinData& bin)
{
    std::string j = "{\n  \"version\": 1,\n  \"data\": {\n";
    j += "    \"id_0\": " + BuildExclusiveRuleArrayJson(bin.exclusiveRuleEntries,       0) + ",\n";
    j += "    \"id_1\": " + BuildExclusivePairArrayJson(bin.exclusivePairEntries,       0) + ",\n";
    j += "    \"id_2\": " + BuildExclusiveRuleArrayJson(bin.exclusiveGroupRuleEntries,  0) + ",\n";
    j += "    \"id_3\": " + BuildExclusivePairArrayJson(bin.exclusiveGroupPairEntries,  0) + ",\n";
    j += "    \"id_4\": " + BuildExclusiveRuleArrayJson(bin.exclusiveSetRuleEntries,    0) + "\n";
    j += "  }\n}\n";
    return j;
}

// -----------------------------------------------------------------------------
//  character_list JSON parser
// -----------------------------------------------------------------------------

static bool ParseCharacterEntry(const char* start, const char* end, CharacterEntry& e)
{
#define PSTR(key, field)  { auto* fp = FindField(start,end,key); ParseString(fp,end,e.field,sizeof(e.field)); }
#define PU32(key, field)  { auto* fp = FindField(start,end,key); ParseUInt32(fp,end,e.field); }
#define PBOOL(key, field) { auto* fp = FindField(start,end,key); ParseBool(fp,end,e.field); }
#define PFLT(key, field)  { auto* fp = FindField(start,end,key); ParseFloat(fp,end,e.field); }

    PSTR("id_0",  character_code);
    PU32("id_1",  name_hash);
    PBOOL("id_2", is_enabled);
    PBOOL("id_3", is_selectable);
    PSTR("id_4",  group);
    PFLT("id_5",  camera_offset);
    PBOOL("id_6", is_playable);
    PU32("id_7",  sort_order);
    PSTR("id_8",  full_name_key);
    PSTR("id_9",  short_name_jp_key);
    PSTR("id_10", short_name_key);
    PSTR("id_11", origin_key);
    PSTR("id_12", fighting_style_key);
    PSTR("id_13", height_key);
    PSTR("id_14", weight_key);

#undef PSTR
#undef PU32
#undef PBOOL
#undef PFLT
    return true;
}

static bool ParseCharacterListJson(const std::string& json, ContentsBinData& bin)
{
    const char* p = json.c_str();
    const char* entriesField = strstr(p, "\"entries\"");
    if (!entriesField) return false;
    const char* arr = strchr(entriesField, '[');
    if (!arr) return false;
    ++arr;

    while (true)
    {
        arr = SkipWS(arr);
        if (*arr == ']' || *arr == '\0') break;
        if (*arr == '{')
        {
            const char* objEnd = SkipObject(arr);
            if (!objEnd) break;
            CharacterEntry entry;
            ParseCharacterEntry(arr, objEnd, entry);
            bin.characterEntries.push_back(entry);
            arr = objEnd;
        }
        while (*arr == ',' || *arr == ' ' || *arr == '\n' || *arr == '\r' || *arr == '\t')
            ++arr;
    }
    return !bin.characterEntries.empty();
}

// -----------------------------------------------------------------------------
//  customize_item_exclusive_list JSON parser
// -----------------------------------------------------------------------------

static bool ParseExclusiveRuleEntry(const char* start, const char* end,
                                    CustomizeExclusiveRuleEntry& e)
{
#define PU32(key, field) { auto* fp = FindField(start,end,key); ParseUInt32(fp,end,e.field); }
    PU32("id_0", item_id); PU32("id_1", hash); PU32("id_2", link_type); PU32("id_3", ref_item_id);
#undef PU32
    return true;
}

static bool ParseExclusivePairEntry(const char* start, const char* end,
                                    CustomizeExclusivePairEntry& e)
{
#define PU32(key, field) { auto* fp = FindField(start,end,key); ParseUInt32(fp,end,e.field); }
    PU32("id_0", item_id_a); PU32("id_1", item_id_b); PU32("id_2", flag);
#undef PU32
    return true;
}

template<typename TEntry, typename TParseFunc>
static void ParseExclusiveArray(const char* json, const char* arrayKey,
                                std::vector<TEntry>& out, TParseFunc parseFn)
{
    const char* field = strstr(json, arrayKey);
    if (!field) return;
    const char* arr = strchr(field, '[');
    if (!arr) return;
    ++arr;

    while (true)
    {
        arr = SkipWS(arr);
        if (*arr == ']' || *arr == '\0') break;
        if (*arr == '{')
        {
            const char* objEnd = SkipObject(arr);
            if (!objEnd) break;
            TEntry entry{};
            parseFn(arr, objEnd, entry);
            out.push_back(entry);
            arr = objEnd;
        }
        while (*arr == ',' || *arr == ' ' || *arr == '\n' || *arr == '\r' || *arr == '\t')
            ++arr;
    }
}

static bool ParseCustomizeItemExclusiveListJson(const std::string& json, ContentsBinData& bin)
{
    const char* p = json.c_str();
    ParseExclusiveArray(p, "\"id_0\"", bin.exclusiveRuleEntries,       ParseExclusiveRuleEntry);
    ParseExclusiveArray(p, "\"id_1\"", bin.exclusivePairEntries,       ParseExclusivePairEntry);
    ParseExclusiveArray(p, "\"id_2\"", bin.exclusiveGroupRuleEntries,  ParseExclusiveRuleEntry);
    ParseExclusiveArray(p, "\"id_3\"", bin.exclusiveGroupPairEntries,  ParseExclusivePairEntry);
    ParseExclusiveArray(p, "\"id_4\"", bin.exclusiveSetRuleEntries,    ParseExclusiveRuleEntry);
    return true;
}

// -----------------------------------------------------------------------------
//  area_list JSON builder / parser
// -----------------------------------------------------------------------------

static std::string BuildAreaListJson(const ContentsBinData& bin)
{
    std::string j = "{\n  \"version\": 1,\n  \"data\": {\n    \"entries\": [\n";
    for (size_t i = 0; i < bin.areaEntries.size(); ++i)
    {
        if (i > 0) j += ",\n";
        const auto& e = bin.areaEntries[i];
        j += "      {\n";
        j += "        \"id_0\": " + std::to_string(e.area_hash) + ",\n";
        j += "        \"id_1\": \"" + JsonEsc(e.area_code) + "\"\n";
        j += "      }";
    }
    j += "\n    ]\n  }\n}\n";
    return j;
}

static bool ParseAreaListJson(const std::string& json, ContentsBinData& bin)
{
    const char* p = json.c_str();
    const char* arr = strchr(strstr(p, "\"entries\"") ? strstr(p, "\"entries\"") : p, '[');
    if (!arr) return false;
    ++arr;
    while (true)
    {
        arr = SkipWS(arr);
        if (*arr == ']' || *arr == '\0') break;
        if (*arr == '{')
        {
            const char* objEnd = SkipObject(arr);
            if (!objEnd) break;
            AreaEntry e{};
            { auto* fp = FindField(arr, objEnd, "id_0"); ParseUInt32(fp, objEnd, e.area_hash); }
            { auto* fp = FindField(arr, objEnd, "id_1"); ParseString(fp, objEnd, e.area_code, sizeof(e.area_code)); }
            bin.areaEntries.push_back(e);
            arr = objEnd;
        }
        while (*arr == ',' || *arr == ' ' || *arr == '\n' || *arr == '\r' || *arr == '\t') ++arr;
    }
    return true;
}

// -----------------------------------------------------------------------------
//  battle_subtitle_info JSON builder / parser
// -----------------------------------------------------------------------------

static std::string BuildBattleSubtitleInfoJson(const ContentsBinData& bin)
{
    std::string j = "{\n  \"version\": 1,\n  \"data\": {\n    \"entries\": [\n";
    for (size_t i = 0; i < bin.battleSubtitleEntries.size(); ++i)
    {
        if (i > 0) j += ",\n";
        const auto& e = bin.battleSubtitleEntries[i];
        j += "      {\n";
        j += "        \"id_0\": " + std::to_string(e.subtitle_hash) + ",\n";
        j += "        \"id_1\": " + std::to_string(e.subtitle_type) + "\n";
        j += "      }";
    }
    j += "\n    ]\n  }\n}\n";
    return j;
}

static bool ParseBattleSubtitleInfoJson(const std::string& json, ContentsBinData& bin)
{
    const char* p = json.c_str();
    const char* ef = strstr(p, "\"entries\""); if (!ef) return false;
    const char* arr = strchr(ef, '['); if (!arr) return false;
    ++arr;
    while (true)
    {
        arr = SkipWS(arr);
        if (*arr == ']' || *arr == '\0') break;
        if (*arr == '{')
        {
            const char* objEnd = SkipObject(arr); if (!objEnd) break;
            BattleSubtitleInfoEntry e{};
            { auto* fp = FindField(arr, objEnd, "id_0"); ParseUInt32(fp, objEnd, e.subtitle_hash); }
            { auto* fp = FindField(arr, objEnd, "id_1"); ParseUInt32(fp, objEnd, e.subtitle_type); }
            bin.battleSubtitleEntries.push_back(e);
            arr = objEnd;
        }
        while (*arr == ',' || *arr == ' ' || *arr == '\n' || *arr == '\r' || *arr == '\t') ++arr;
    }
    return true;
}

// -----------------------------------------------------------------------------
//  fate_drama_player_start_list JSON builder / parser
// -----------------------------------------------------------------------------

static std::string BuildFateDramaPlayerStartListJson(const ContentsBinData& bin)
{
    std::string j = "{\n  \"version\": 1,\n  \"data\": {\n    \"entries\": [\n";
    for (size_t i = 0; i < bin.fateDramaPlayerStartEntries.size(); ++i)
    {
        if (i > 0) j += ",\n";
        const auto& e = bin.fateDramaPlayerStartEntries[i];
        j += "      {\n";
        j += "        \"id_0\": " + std::to_string(e.character1_hash) + ",\n";
        j += "        \"id_1\": " + std::to_string(e.character2_hash) + ",\n";
        j += "        \"id_2\": " + std::to_string(e.value_0) + ",\n";
        j += "        \"id_3\": " + std::to_string(e.hash_2) + ",\n";
        j += std::string("        \"id_4\": ") + (e.value_4 ? "true" : "false") + "\n";
        j += "      }";
    }
    j += "\n    ]\n  }\n}\n";
    return j;
}

static bool ParseFateDramaPlayerStartListJson(const std::string& json, ContentsBinData& bin)
{
    const char* p = json.c_str();
    const char* ef = strstr(p, "\"entries\""); if (!ef) return false;
    const char* arr = strchr(ef, '['); if (!arr) return false;
    ++arr;
    while (true)
    {
        arr = SkipWS(arr);
        if (*arr == ']' || *arr == '\0') break;
        if (*arr == '{')
        {
            const char* objEnd = SkipObject(arr); if (!objEnd) break;
            FateDramaPlayerStartEntry e{};
            { auto* fp = FindField(arr, objEnd, "id_0"); ParseUInt32(fp, objEnd, e.character1_hash); }
            { auto* fp = FindField(arr, objEnd, "id_1"); ParseUInt32(fp, objEnd, e.character2_hash); }
            { auto* fp = FindField(arr, objEnd, "id_2"); ParseUInt32(fp, objEnd, e.value_0); }
            { auto* fp = FindField(arr, objEnd, "id_3"); ParseUInt32(fp, objEnd, e.hash_2); }
            { auto* fp = FindField(arr, objEnd, "id_4"); ParseBool(fp, objEnd, e.value_4); }
            bin.fateDramaPlayerStartEntries.push_back(e);
            arr = objEnd;
        }
        while (*arr == ',' || *arr == ' ' || *arr == '\n' || *arr == '\r' || *arr == '\t') ++arr;
    }
    return true;
}

// -----------------------------------------------------------------------------
//  jukebox_list JSON builder / parser
// -----------------------------------------------------------------------------

static std::string BuildJukeboxListJson(const ContentsBinData& bin)
{
    std::string j = "{\n  \"version\": 1,\n  \"data\": {\n";
    j += "    \"id_0\": [\n";
    for (size_t i = 0; i < bin.jukeboxEntries.size(); ++i)
    {
        if (i > 0) j += ",\n";
        const auto& e = bin.jukeboxEntries[i];
        j += "      {\n";
        j += "        \"id_0\": " + std::to_string(e.bgm_hash) + ",\n";
        j += "        \"id_1\": " + std::to_string(e.series_hash) + ",\n";
        j += "        \"id_2\": " + std::to_string(e.unk_2) + ",\n";
        j += "        \"id_3\": \"" + JsonEsc(e.cue_name) + "\",\n";
        j += "        \"id_4\": \"" + JsonEsc(e.arrangement) + "\",\n";
        j += "        \"id_5\": \"" + JsonEsc(e.alt_cue_name_1) + "\",\n";
        j += "        \"id_6\": \"" + JsonEsc(e.alt_cue_name_2) + "\",\n";
        j += "        \"id_7\": \"" + JsonEsc(e.alt_cue_name_3) + "\",\n";
        j += "        \"id_8\": \"" + JsonEsc(e.display_text_key) + "\"\n";
        j += "      }";
    }
    j += "\n    ],\n";
    j += "    \"id_1\": " + std::to_string(bin.jukeboxListUnkValue1) + "\n";
    j += "  }\n}\n";
    return j;
}

static bool ParseJukeboxListJson(const std::string& json, ContentsBinData& bin)
{
    const char* p = json.c_str();
    { auto* fp = FindField(p, p + json.size(), "id_1"); ParseUInt32(fp, p + json.size(), bin.jukeboxListUnkValue1); }
    ParseExclusiveArray(p, "\"id_0\"", bin.jukeboxEntries,
        [](const char* s, const char* e, JukeboxEntry& entry) -> bool {
            { auto* fp = FindField(s, e, "id_0"); ParseUInt32(fp, e, entry.bgm_hash); }
            { auto* fp = FindField(s, e, "id_1"); ParseUInt32(fp, e, entry.series_hash); }
            { auto* fp = FindField(s, e, "id_2"); ParseUInt32(fp, e, entry.unk_2); }
            { auto* fp = FindField(s, e, "id_3"); ParseString(fp, e, entry.cue_name, sizeof(entry.cue_name)); }
            { auto* fp = FindField(s, e, "id_4"); ParseString(fp, e, entry.arrangement, sizeof(entry.arrangement)); }
            { auto* fp = FindField(s, e, "id_5"); ParseString(fp, e, entry.alt_cue_name_1, sizeof(entry.alt_cue_name_1)); }
            { auto* fp = FindField(s, e, "id_6"); ParseString(fp, e, entry.alt_cue_name_2, sizeof(entry.alt_cue_name_2)); }
            { auto* fp = FindField(s, e, "id_7"); ParseString(fp, e, entry.alt_cue_name_3, sizeof(entry.alt_cue_name_3)); }
            { auto* fp = FindField(s, e, "id_8"); ParseString(fp, e, entry.display_text_key, sizeof(entry.display_text_key)); }
            return true;
        });
    return true;
}

// -----------------------------------------------------------------------------
//  series_list JSON builder / parser
// -----------------------------------------------------------------------------

static std::string BuildSeriesListJson(const ContentsBinData& bin)
{
    std::string j = "{\n  \"version\": 1,\n  \"data\": {\n    \"entries\": [\n";
    for (size_t i = 0; i < bin.seriesEntries.size(); ++i)
    {
        if (i > 0) j += ",\n";
        const auto& e = bin.seriesEntries[i];
        j += "      {\n";
        j += "        \"id_0\": " + std::to_string(e.series_hash) + ",\n";
        j += "        \"id_1\": \"" + JsonEsc(e.jacket_text_key) + "\",\n";
        j += "        \"id_2\": \"" + JsonEsc(e.jacket_icon_key) + "\",\n";
        j += "        \"id_3\": \"" + JsonEsc(e.logo_text_key) + "\",\n";
        j += "        \"id_4\": \"" + JsonEsc(e.logo_icon_key) + "\"\n";
        j += "      }";
    }
    j += "\n    ]\n  }\n}\n";
    return j;
}

static bool ParseSeriesListJson(const std::string& json, ContentsBinData& bin)
{
    const char* p = json.c_str();
    const char* ef = strstr(p, "\"entries\""); if (!ef) return false;
    const char* arr = strchr(ef, '['); if (!arr) return false;
    ++arr;
    while (true)
    {
        arr = SkipWS(arr);
        if (*arr == ']' || *arr == '\0') break;
        if (*arr == '{')
        {
            const char* objEnd = SkipObject(arr); if (!objEnd) break;
            SeriesEntry e{};
            { auto* fp = FindField(arr, objEnd, "id_0"); ParseUInt32(fp, objEnd, e.series_hash); }
            { auto* fp = FindField(arr, objEnd, "id_1"); ParseString(fp, objEnd, e.jacket_text_key, sizeof(e.jacket_text_key)); }
            { auto* fp = FindField(arr, objEnd, "id_2"); ParseString(fp, objEnd, e.jacket_icon_key, sizeof(e.jacket_icon_key)); }
            { auto* fp = FindField(arr, objEnd, "id_3"); ParseString(fp, objEnd, e.logo_text_key, sizeof(e.logo_text_key)); }
            { auto* fp = FindField(arr, objEnd, "id_4"); ParseString(fp, objEnd, e.logo_icon_key, sizeof(e.logo_icon_key)); }
            bin.seriesEntries.push_back(e);
            arr = objEnd;
        }
        while (*arr == ',' || *arr == ' ' || *arr == '\n' || *arr == '\r' || *arr == '\t') ++arr;
    }
    return true;
}

// -----------------------------------------------------------------------------
//  tam_mission_list JSON builder / parser
// -----------------------------------------------------------------------------

static std::string BuildTamMissionListJson(const ContentsBinData& bin)
{
    std::string j = "{\n  \"version\": 1,\n  \"data\": {\n    \"entries\": [\n";
    for (size_t i = 0; i < bin.tamMissionEntries.size(); ++i)
    {
        if (i > 0) j += ",\n";
        const auto& e = bin.tamMissionEntries[i];
        j += "      {\n";
        j += "        \"id_0\": "  + std::to_string(e.mission_id) + ",\n";
        j += "        \"id_1\": "  + std::to_string(e.value_1)    + ",\n";
        j += "        \"id_2\": "  + std::to_string(e.value_2)    + ",\n";
        j += "        \"id_3\": \"" + JsonEsc(e.location)         + "\",\n";
        j += "        \"id_4\": "  + std::to_string(e.hash_0)     + ",\n";
        j += "        \"id_5\": "  + std::to_string(e.hash_1)     + ",\n";
        j += "        \"id_6\": "  + std::to_string(e.hash_2)     + ",\n";
        j += "        \"id_7\": "  + std::to_string(e.hash_3)     + ",\n";
        j += "        \"id_8\": "  + std::to_string(e.hash_4)     + ",\n";
        j += "        \"id_9\": "  + std::to_string(e.value_9)    + ",\n";
        j += "        \"id_10\": " + std::to_string(e.value_10)   + ",\n";
        j += "        \"id_11\": " + std::to_string(e.value_11)   + "\n";
        j += "      }";
    }
    j += "\n    ]\n  }\n}\n";
    return j;
}

static bool ParseTamMissionListJson(const std::string& json, ContentsBinData& bin)
{
    const char* p = json.c_str();
    const char* ef = strstr(p, "\"entries\""); if (!ef) return false;
    const char* arr = strchr(ef, '['); if (!arr) return false;
    ++arr;
    while (true)
    {
        arr = SkipWS(arr);
        if (*arr == ']' || *arr == '\0') break;
        if (*arr == '{')
        {
            const char* objEnd = SkipObject(arr); if (!objEnd) break;
            TamMissionEntry e{};
            { auto* fp = FindField(arr, objEnd, "id_0");  ParseUInt32(fp, objEnd, e.mission_id); }
            { auto* fp = FindField(arr, objEnd, "id_1");  ParseUInt32(fp, objEnd, e.value_1); }
            { auto* fp = FindField(arr, objEnd, "id_2");  ParseUInt32(fp, objEnd, e.value_2); }
            { auto* fp = FindField(arr, objEnd, "id_3");  ParseString(fp, objEnd, e.location, sizeof(e.location)); }
            { auto* fp = FindField(arr, objEnd, "id_4");  ParseUInt32(fp, objEnd, e.hash_0); }
            { auto* fp = FindField(arr, objEnd, "id_5");  ParseUInt32(fp, objEnd, e.hash_1); }
            { auto* fp = FindField(arr, objEnd, "id_6");  ParseUInt32(fp, objEnd, e.hash_2); }
            { auto* fp = FindField(arr, objEnd, "id_7");  ParseUInt32(fp, objEnd, e.hash_3); }
            { auto* fp = FindField(arr, objEnd, "id_8");  ParseUInt32(fp, objEnd, e.hash_4); }
            { auto* fp = FindField(arr, objEnd, "id_9");  ParseUInt32(fp, objEnd, e.value_9); }
            { auto* fp = FindField(arr, objEnd, "id_10"); ParseUInt32(fp, objEnd, e.value_10); }
            { auto* fp = FindField(arr, objEnd, "id_11"); ParseUInt32(fp, objEnd, e.value_11); }
            bin.tamMissionEntries.push_back(e);
            arr = objEnd;
        }
        while (*arr == ',' || *arr == ' ' || *arr == '\n' || *arr == '\r' || *arr == '\t') ++arr;
    }
    return true;
}

// -----------------------------------------------------------------------------
//  drama_player_start_list JSON builder / parser
// -----------------------------------------------------------------------------

static std::string BuildDramaPlayerStartListJson(const ContentsBinData& bin)
{
    std::string j = "{\n  \"version\": 1,\n  \"data\": {\n    \"entries\": [\n";
    for (size_t i = 0; i < bin.dramaPlayerStartEntries.size(); ++i)
    {
        if (i > 0) j += ",\n";
        const auto& e = bin.dramaPlayerStartEntries[i];
        char fBuf[32];
        j += "      {\n";
        j += "        \"id_0\": "  + std::to_string(e.character_hash) + ",\n";
        j += "        \"id_1\": "  + std::to_string(e.hash_1) + ",\n";
        j += "        \"id_2\": "  + std::to_string(e.index) + ",\n";
        j += "        \"id_3\": "  + std::to_string(e.scene_hash) + ",\n";
        j += "        \"id_4\": "  + std::to_string(e.config_hash) + ",\n";
#define FOUT(id, field) snprintf(fBuf,sizeof(fBuf),"%g",e.field); j += "        \"id_" #id "\": " + std::string(fBuf) + ",\n";
#define FOUT_LAST(id, field) snprintf(fBuf,sizeof(fBuf),"%g",e.field); j += "        \"id_" #id "\": " + std::string(fBuf) + "\n";
#define UOUT(id, field) j += "        \"id_" #id "\": " + std::to_string(e.field) + ",\n";
        FOUT(5,  unk_float_5)  FOUT(6,  pos_x)       FOUT(7,  pos_y)
        UOUT(8,  state_hash)   UOUT(9,  unk_9)
        FOUT(10, scale)        UOUT(11, ref_hash)
        FOUT(12, unk_float_12) FOUT(13, unk_float_13) FOUT(14, unk_float_14) FOUT(15, unk_float_15)
        UOUT(16, unk_16)       UOUT(17, unk_17)
        FOUT(18, unk_float_18) FOUT(19, rate)
        UOUT(20, blk1_marker)
        FOUT(21, blk1_scale)   FOUT(22, blk1_field_22) FOUT(23, blk1_field_23) FOUT(24, blk1_field_24)
        FOUT(25, blk1_field_25) FOUT(26, blk1_field_26) FOUT(27, blk1_field_27) FOUT(28, blk1_field_28)
        FOUT(29, blk1_angle)
        UOUT(30, blk1_hash_a)  UOUT(31, blk1_hash_b)
        UOUT(32, blk2_marker)
        FOUT(33, blk2_scale)   FOUT(34, blk2_field_34) FOUT(35, blk2_field_35) FOUT(36, blk2_field_36)
        FOUT(37, blk2_field_37) FOUT(38, blk2_field_38) FOUT(39, blk2_field_39) FOUT(40, blk2_field_40)
        FOUT(41, blk2_angle)
        UOUT(42, blk2_hash_a)  UOUT(43, blk2_hash_b)
        UOUT(44, blk3_marker)
        FOUT(45, blk3_scale)   FOUT(46, blk3_field_46) FOUT(47, blk3_field_47) FOUT(48, blk3_field_48)
        FOUT(49, blk3_field_49) FOUT(50, blk3_field_50) FOUT(51, blk3_field_51) FOUT(52, blk3_field_52)
        FOUT(53, blk3_angle)
        UOUT(54, blk3_hash_a)  UOUT(55, blk3_hash_b)
        UOUT(56, end_marker)
        FOUT(57, unk_float_57)
        FOUT(58, extra_range)  FOUT(59, extra_param_a) FOUT(60, extra_param_b) FOUT(61, extra_param_c)
        FOUT_LAST(62, extra_param_d)
#undef FOUT
#undef FOUT_LAST
#undef UOUT
        j += "      }";
    }
    j += "\n    ]\n  }\n}\n";
    return j;
}

static bool ParseDramaPlayerStartListJson(const std::string& json, ContentsBinData& bin)
{
    const char* p = json.c_str();
    const char* ef = strstr(p, "\"entries\""); if (!ef) return false;
    const char* arr = strchr(ef, '['); if (!arr) return false;
    ++arr;
    while (true)
    {
        arr = SkipWS(arr);
        if (*arr == ']' || *arr == '\0') break;
        if (*arr == '{')
        {
            const char* objEnd = SkipObject(arr); if (!objEnd) break;
            DramaPlayerStartEntry e{};
#define PU32(id, field) { auto* fp = FindField(arr, objEnd, id); ParseUInt32(fp, objEnd, e.field); }
#define PFLT(id, field) { auto* fp = FindField(arr, objEnd, id); ParseFloat(fp, objEnd, e.field); }
            PU32("id_0",  character_hash)  PU32("id_1",  hash_1)       PU32("id_2",  index)
            PU32("id_3",  scene_hash)      PU32("id_4",  config_hash)
            PFLT("id_5",  unk_float_5)     PFLT("id_6",  pos_x)         PFLT("id_7",  pos_y)
            PU32("id_8",  state_hash)      PU32("id_9",  unk_9)
            PFLT("id_10", scale)           PU32("id_11", ref_hash)
            PFLT("id_12", unk_float_12)    PFLT("id_13", unk_float_13)  PFLT("id_14", unk_float_14)  PFLT("id_15", unk_float_15)
            PU32("id_16", unk_16)          PU32("id_17", unk_17)
            PFLT("id_18", unk_float_18)    PFLT("id_19", rate)
            PU32("id_20", blk1_marker)
            PFLT("id_21", blk1_scale)      PFLT("id_22", blk1_field_22) PFLT("id_23", blk1_field_23) PFLT("id_24", blk1_field_24)
            PFLT("id_25", blk1_field_25)   PFLT("id_26", blk1_field_26) PFLT("id_27", blk1_field_27) PFLT("id_28", blk1_field_28)
            PFLT("id_29", blk1_angle)
            PU32("id_30", blk1_hash_a)     PU32("id_31", blk1_hash_b)
            PU32("id_32", blk2_marker)
            PFLT("id_33", blk2_scale)      PFLT("id_34", blk2_field_34) PFLT("id_35", blk2_field_35) PFLT("id_36", blk2_field_36)
            PFLT("id_37", blk2_field_37)   PFLT("id_38", blk2_field_38) PFLT("id_39", blk2_field_39) PFLT("id_40", blk2_field_40)
            PFLT("id_41", blk2_angle)
            PU32("id_42", blk2_hash_a)     PU32("id_43", blk2_hash_b)
            PU32("id_44", blk3_marker)
            PFLT("id_45", blk3_scale)      PFLT("id_46", blk3_field_46) PFLT("id_47", blk3_field_47) PFLT("id_48", blk3_field_48)
            PFLT("id_49", blk3_field_49)   PFLT("id_50", blk3_field_50) PFLT("id_51", blk3_field_51) PFLT("id_52", blk3_field_52)
            PFLT("id_53", blk3_angle)
            PU32("id_54", blk3_hash_a)     PU32("id_55", blk3_hash_b)
            PU32("id_56", end_marker)
            PFLT("id_57", unk_float_57)
            PFLT("id_58", extra_range)     PFLT("id_59", extra_param_a) PFLT("id_60", extra_param_b)
            PFLT("id_61", extra_param_c)   PFLT("id_62", extra_param_d)
#undef PU32
#undef PFLT
            bin.dramaPlayerStartEntries.push_back(e);
            arr = objEnd;
        }
        while (*arr == ',' || *arr == ' ' || *arr == '\n' || *arr == '\r' || *arr == '\t') ++arr;
    }
    return true;
}

// -----------------------------------------------------------------------------
//  stage_list JSON builder / parser
// -----------------------------------------------------------------------------

static std::string BuildStageListJson(const ContentsBinData& bin)
{
    std::string j = "{\n  \"version\": 1,\n  \"data\": {\n    \"entries\": [\n";
    for (size_t i = 0; i < bin.stageEntries.size(); ++i)
    {
        if (i > 0) j += ",\n";
        const auto& e = bin.stageEntries[i];
        char fBuf[32];
        snprintf(fBuf, sizeof(fBuf), "%g", e.camera_offset);
        j += "      {\n";
        j += "        \"id_0\": \""  + JsonEsc(e.stage_code) + "\",\n";
        j += "        \"id_1\": "   + std::to_string(e.stage_hash) + ",\n";
        j += std::string("        \"id_2\": ")  + (e.is_selectable      ? "true" : "false") + ",\n";
        j += std::string("        \"id_3\": ")  + fBuf                                       + ",\n";
        j += "        \"id_4\": "   + std::to_string(e.parent_stage_index) + ",\n";
        j += "        \"id_5\": "   + std::to_string(e.variant_hash) + ",\n";
        j += std::string("        \"id_6\": ")  + (e.has_weather         ? "true" : "false") + ",\n";
        j += std::string("        \"id_7\": ")  + (e.is_active           ? "true" : "false") + ",\n";
        j += std::string("        \"id_8\": ")  + (e.flag_interlocked    ? "true" : "false") + ",\n";
        j += std::string("        \"id_9\": ")  + (e.flag_ocean          ? "true" : "false") + ",\n";
        j += std::string("        \"id_10\": ") + (e.flag_10             ? "true" : "false") + ",\n";
        j += std::string("        \"id_11\": ") + (e.flag_infinite       ? "true" : "false") + ",\n";
        j += std::string("        \"id_12\": ") + (e.flag_battle         ? "true" : "false") + ",\n";
        j += std::string("        \"id_13\": ") + (e.flag_13             ? "true" : "false") + ",\n";
        j += std::string("        \"id_14\": ") + (e.flag_balcony        ? "true" : "false") + ",\n";
        j += std::string("        \"id_15\": ") + (e.flag_15             ? "true" : "false") + ",\n";
        j += std::string("        \"id_16\": ") + (e.reserved_16         ? "true" : "false") + ",\n";
        j += std::string("        \"id_17\": ") + (e.is_online_enabled   ? "true" : "false") + ",\n";
        j += std::string("        \"id_18\": ") + (e.is_ranked_enabled   ? "true" : "false") + ",\n";
        j += std::string("        \"id_19\": ") + (e.reserved_19         ? "true" : "false") + ",\n";
        j += std::string("        \"id_20\": ") + (e.reserved_20         ? "true" : "false") + ",\n";
        j += "        \"id_21\": " + std::to_string(e.arena_width)  + ",\n";
        j += "        \"id_22\": " + std::to_string(e.arena_depth)  + ",\n";
        j += "        \"id_23\": " + std::to_string(e.reserved_23)  + ",\n";
        j += "        \"id_24\": " + std::to_string(e.arena_param)  + ",\n";
        j += "        \"id_25\": " + std::to_string(e.extra_width)  + ",\n";
        j += "        \"id_26\": \"" + JsonEsc(e.extra_group) + "\",\n";
        j += "        \"id_27\": " + std::to_string(e.extra_depth)  + ",\n";
        j += "        \"id_28\": \"" + JsonEsc(e.group_id) + "\",\n";
        j += "        \"id_29\": \"" + JsonEsc(e.stage_name_key) + "\",\n";
        j += "        \"id_30\": \"" + JsonEsc(e.level_name) + "\",\n";
        j += "        \"id_31\": \"" + JsonEsc(e.sound_bank) + "\",\n";
        j += "        \"id_32\": " + std::to_string(e.wall_distance_a) + ",\n";
        j += "        \"id_33\": " + std::to_string(e.wall_distance_b) + ",\n";
        j += "        \"id_34\": " + std::to_string(e.stage_mode)      + ",\n";
        j += "        \"id_35\": " + std::to_string(e.reserved_35)     + ",\n";
        j += std::string("        \"id_36\": ") + (e.is_default_variant ? "true" : "false") + "\n";
        j += "      }";
    }
    j += "\n    ]\n  }\n}\n";
    return j;
}

static bool ParseStageListJson(const std::string& json, ContentsBinData& bin)
{
    const char* p = json.c_str();
    const char* ef = strstr(p, "\"entries\""); if (!ef) return false;
    const char* arr = strchr(ef, '['); if (!arr) return false;
    ++arr;
    while (true)
    {
        arr = SkipWS(arr);
        if (*arr == ']' || *arr == '\0') break;
        if (*arr == '{')
        {
            const char* objEnd = SkipObject(arr); if (!objEnd) break;
            StageEntry e{};
#define PSTR(id, field)  { auto* fp = FindField(arr, objEnd, id); ParseString(fp, objEnd, e.field, sizeof(e.field)); }
#define PU32(id, field)  { auto* fp = FindField(arr, objEnd, id); ParseUInt32(fp, objEnd, e.field); }
#define PBOOL(id, field) { auto* fp = FindField(arr, objEnd, id); ParseBool(fp, objEnd, e.field); }
#define PFLT(id, field)  { auto* fp = FindField(arr, objEnd, id); ParseFloat(fp, objEnd, e.field); }
            PSTR("id_0",  stage_code)          PU32("id_1",  stage_hash)
            PBOOL("id_2", is_selectable)        PFLT("id_3",  camera_offset)
            PU32("id_4",  parent_stage_index)   PU32("id_5",  variant_hash)
            PBOOL("id_6", has_weather)          PBOOL("id_7", is_active)
            PBOOL("id_8", flag_interlocked)     PBOOL("id_9", flag_ocean)
            PBOOL("id_10",flag_10)              PBOOL("id_11",flag_infinite)
            PBOOL("id_12",flag_battle)          PBOOL("id_13",flag_13)
            PBOOL("id_14",flag_balcony)         PBOOL("id_15",flag_15)
            PBOOL("id_16",reserved_16)          PBOOL("id_17",is_online_enabled)
            PBOOL("id_18",is_ranked_enabled)    PBOOL("id_19",reserved_19)
            PBOOL("id_20",reserved_20)
            PU32("id_21", arena_width)          PU32("id_22", arena_depth)
            PU32("id_23", reserved_23)          PU32("id_24", arena_param)
            PU32("id_25", extra_width)          PSTR("id_26", extra_group)
            PU32("id_27", extra_depth)          PSTR("id_28", group_id)
            PSTR("id_29", stage_name_key)       PSTR("id_30", level_name)
            PSTR("id_31", sound_bank)
            PU32("id_32", wall_distance_a)      PU32("id_33", wall_distance_b)
            PU32("id_34", stage_mode)           PU32("id_35", reserved_35)
            PBOOL("id_36",is_default_variant)
#undef PSTR
#undef PU32
#undef PBOOL
#undef PFLT
            bin.stageEntries.push_back(e);
            arr = objEnd;
        }
        while (*arr == ',' || *arr == ' ' || *arr == '\n' || *arr == '\r' || *arr == '\t') ++arr;
    }
    return true;
}

// -----------------------------------------------------------------------------
//  ball_property_list JSON builder / parser
// -----------------------------------------------------------------------------

static std::string BuildBallPropertyListJson(const ContentsBinData& bin)
{
    std::string j = "{\n  \"version\": 1,\n  \"data\": {\n";
    j += "    \"id_0\": [\n";
    for (size_t i = 0; i < bin.ballPropertyEntries.size(); ++i)
    {
        if (i > 0) j += ",\n";
        const auto& e = bin.ballPropertyEntries[i];
        char fBuf[32];
        j += "      {\n";
        j += "        \"id_0\": "   + std::to_string(e.ball_hash) + ",\n";
        j += "        \"id_1\": \"" + JsonEsc(e.ball_code) + "\",\n";
        j += "        \"id_2\": \"" + JsonEsc(e.effect_name) + "\",\n";
        j += "        \"id_3\": "   + std::to_string(e.hash_3) + ",\n";
        j += "        \"id_4\": "   + std::to_string(e.hash_4) + ",\n";
        j += "        \"id_5\": "   + std::to_string(e.unk_5) + ",\n";
        j += "        \"id_6\": "   + std::to_string(e.unk_6) + ",\n";
        j += "        \"id_7\": "   + std::to_string(e.hash_7) + ",\n";
        j += "        \"id_8\": "   + std::to_string(e.item_no) + ",\n";
        j += "        \"id_9\": "   + std::to_string(e.rarity) + ",\n";
#define FOUT(id, field) snprintf(fBuf,sizeof(fBuf),"%g",e.field); j += "        \"id_" #id "\": " + std::string(fBuf) + ",\n";
#define FOUT_LAST(id, field) snprintf(fBuf,sizeof(fBuf),"%g",e.field); j += "        \"id_" #id "\": " + std::string(fBuf) + "\n";
        FOUT(10,value_10) FOUT(11,value_11) FOUT(12,value_12) FOUT(13,value_13)
        FOUT(14,value_14) FOUT(15,value_15) FOUT(16,value_16) FOUT(17,value_17)
        FOUT_LAST(18,value_18)
#undef FOUT
#undef FOUT_LAST
        j += "      }";
    }
    j += "\n    ],\n";
    j += "    \"id_1\": " + std::to_string(bin.ballPropertyListUnkValue1) + "\n";
    j += "  }\n}\n";
    return j;
}

static bool ParseBallPropertyListJson(const std::string& json, ContentsBinData& bin)
{
    const char* p = json.c_str();
    const size_t jsz = json.size();
    { auto* fp = FindField(p, p + jsz, "id_1"); ParseUInt32(fp, p + jsz, bin.ballPropertyListUnkValue1); }
    ParseExclusiveArray(p, "\"id_0\"", bin.ballPropertyEntries,
        [](const char* s, const char* e, BallPropertyEntry& entry) -> bool {
            { auto* fp = FindField(s, e, "id_0"); ParseUInt32(fp, e, entry.ball_hash); }
            { auto* fp = FindField(s, e, "id_1"); ParseString(fp, e, entry.ball_code, sizeof(entry.ball_code)); }
            { auto* fp = FindField(s, e, "id_2"); ParseString(fp, e, entry.effect_name, sizeof(entry.effect_name)); }
            { auto* fp = FindField(s, e, "id_3"); ParseUInt32(fp, e, entry.hash_3); }
            { auto* fp = FindField(s, e, "id_4"); ParseUInt32(fp, e, entry.hash_4); }
            { auto* fp = FindField(s, e, "id_5"); ParseUInt32(fp, e, entry.unk_5); }
            { auto* fp = FindField(s, e, "id_6"); ParseUInt32(fp, e, entry.unk_6); }
            { auto* fp = FindField(s, e, "id_7"); ParseUInt32(fp, e, entry.hash_7); }
            { auto* fp = FindField(s, e, "id_8"); ParseUInt32(fp, e, entry.item_no); }
            { auto* fp = FindField(s, e, "id_9"); ParseUInt32(fp, e, entry.rarity); }
            { auto* fp = FindField(s, e, "id_10"); ParseFloat(fp, e, entry.value_10); }
            { auto* fp = FindField(s, e, "id_11"); ParseFloat(fp, e, entry.value_11); }
            { auto* fp = FindField(s, e, "id_12"); ParseFloat(fp, e, entry.value_12); }
            { auto* fp = FindField(s, e, "id_13"); ParseFloat(fp, e, entry.value_13); }
            { auto* fp = FindField(s, e, "id_14"); ParseFloat(fp, e, entry.value_14); }
            { auto* fp = FindField(s, e, "id_15"); ParseFloat(fp, e, entry.value_15); }
            { auto* fp = FindField(s, e, "id_16"); ParseFloat(fp, e, entry.value_16); }
            { auto* fp = FindField(s, e, "id_17"); ParseFloat(fp, e, entry.value_17); }
            { auto* fp = FindField(s, e, "id_18"); ParseFloat(fp, e, entry.value_18); }
            return true;
        });
    return true;
}

// -----------------------------------------------------------------------------
//  body_cylinder_data_list JSON builder / parser
// -----------------------------------------------------------------------------

static std::string BuildBodyCylinderDataListJson(const ContentsBinData& bin)
{
    std::string j = "{\n  \"version\": 1,\n  \"data\": {\n";
    char fBuf[32];
    snprintf(fBuf, sizeof(fBuf), "%g", bin.bodyCylinderGlobalScale);
    j += "    \"id_0\": " + std::string(fBuf) + ",\n";
    j += "    \"id_1\": [\n";
    for (size_t i = 0; i < bin.bodyCylinderDataEntries.size(); ++i)
    {
        if (i > 0) j += ",\n";
        const auto& e = bin.bodyCylinderDataEntries[i];
        j += "      {\n";
        j += "        \"id_0\": " + std::to_string(e.character_hash) + ",\n";
#define FOUT(id, field)  snprintf(fBuf,sizeof(fBuf),"%g",e.field); j += "        \"id_" #id "\": " + std::string(fBuf) + ",\n";
#define UOUT(id, field)  j += "        \"id_" #id "\": " + std::to_string(e.field) + ",\n";
#define UOUT_LAST(id, field) j += "        \"id_" #id "\": " + std::to_string(e.field) + "\n";
        FOUT(1,cyl0_radius)   FOUT(2,cyl0_height)   FOUT(3,cyl0_offset_y)
        UOUT(4,cyl0_unk_hash) UOUT(5,unk_5)         UOUT(6,unk_6)         UOUT(7,unk_7)
        FOUT(8,cyl1_radius)   FOUT(9,cyl1_height)   FOUT(10,cyl1_offset_y)
        UOUT(11,cyl1_unk_hash) UOUT(12,unk_12)      UOUT(13,unk_13)       UOUT(14,unk_14)
        FOUT(15,cyl2_radius)  FOUT(16,cyl2_height)  FOUT(17,cyl2_offset_y)
        UOUT_LAST(18,cyl2_unk_hash)
#undef FOUT
#undef UOUT
#undef UOUT_LAST
        j += "      }";
    }
    j += "\n    ]\n  }\n}\n";
    return j;
}

static bool ParseBodyCylinderDataListJson(const std::string& json, ContentsBinData& bin)
{
    const char* p = json.c_str();
    const size_t jsz = json.size();
    { auto* fp = FindField(p, p + jsz, "id_0"); ParseFloat(fp, p + jsz, bin.bodyCylinderGlobalScale); }
    ParseExclusiveArray(p, "\"id_1\"", bin.bodyCylinderDataEntries,
        [](const char* s, const char* e, BodyCylinderDataEntry& entry) -> bool {
            { auto* fp = FindField(s, e, "id_0");  ParseUInt32(fp, e, entry.character_hash); }
            { auto* fp = FindField(s, e, "id_1");  ParseFloat(fp,  e, entry.cyl0_radius); }
            { auto* fp = FindField(s, e, "id_2");  ParseFloat(fp,  e, entry.cyl0_height); }
            { auto* fp = FindField(s, e, "id_3");  ParseFloat(fp,  e, entry.cyl0_offset_y); }
            { auto* fp = FindField(s, e, "id_4");  ParseUInt32(fp, e, entry.cyl0_unk_hash); }
            { auto* fp = FindField(s, e, "id_5");  ParseUInt32(fp, e, entry.unk_5); }
            { auto* fp = FindField(s, e, "id_6");  ParseUInt32(fp, e, entry.unk_6); }
            { auto* fp = FindField(s, e, "id_7");  ParseUInt32(fp, e, entry.unk_7); }
            { auto* fp = FindField(s, e, "id_8");  ParseFloat(fp,  e, entry.cyl1_radius); }
            { auto* fp = FindField(s, e, "id_9");  ParseFloat(fp,  e, entry.cyl1_height); }
            { auto* fp = FindField(s, e, "id_10"); ParseFloat(fp,  e, entry.cyl1_offset_y); }
            { auto* fp = FindField(s, e, "id_11"); ParseUInt32(fp, e, entry.cyl1_unk_hash); }
            { auto* fp = FindField(s, e, "id_12"); ParseUInt32(fp, e, entry.unk_12); }
            { auto* fp = FindField(s, e, "id_13"); ParseUInt32(fp, e, entry.unk_13); }
            { auto* fp = FindField(s, e, "id_14"); ParseUInt32(fp, e, entry.unk_14); }
            { auto* fp = FindField(s, e, "id_15"); ParseFloat(fp,  e, entry.cyl2_radius); }
            { auto* fp = FindField(s, e, "id_16"); ParseFloat(fp,  e, entry.cyl2_height); }
            { auto* fp = FindField(s, e, "id_17"); ParseFloat(fp,  e, entry.cyl2_offset_y); }
            { auto* fp = FindField(s, e, "id_18"); ParseUInt32(fp, e, entry.cyl2_unk_hash); }
            return true;
        });
    return true;
}

// -----------------------------------------------------------------------------
//  customize_item_unique_list JSON builder / parser
// -----------------------------------------------------------------------------

static std::string BuildCustomizeItemUniqueListJson(const ContentsBinData& bin)
{
    std::string j = "{\n  \"version\": 1,\n  \"data\": {\n";
    j += "    \"id_0\": [\n";
    for (size_t i = 0; i < bin.customizeItemUniqueEntries.size(); ++i)
    {
        if (i > 0) j += ",\n";
        const auto& e = bin.customizeItemUniqueEntries[i];
        j += "      {\n";
        j += "        \"id_0\": "   + std::to_string(e.char_item_id)  + ",\n";
        j += "        \"id_1\": \"" + JsonEsc(e.asset_name)           + "\",\n";
        j += "        \"id_2\": "   + std::to_string(e.character_hash)+ ",\n";
        j += "        \"id_3\": "   + std::to_string(e.hash_1)        + ",\n";
        j += "        \"id_4\": \"" + JsonEsc(e.text_key)             + "\",\n";
        j += "        \"id_5\": \"" + JsonEsc(e.extra_text_key_1)     + "\",\n";
        j += "        \"id_6\": \"" + JsonEsc(e.extra_text_key_2)     + "\",\n";
        j += std::string("        \"id_7\": ")  + (e.flag_7  ? "true" : "false") + ",\n";
        j += "        \"id_8\": "   + std::to_string(e.unk_8)         + ",\n";
        j += std::string("        \"id_9\": ")  + (e.flag_9  ? "true" : "false") + ",\n";
        j += "        \"id_10\": "  + std::to_string(e.unk_10)        + ",\n";
        j += "        \"id_11\": "  + std::to_string(e.price)         + ",\n";
        j += "        \"id_12\": "  + std::to_string(e.unk_12)        + ",\n";
        j += "        \"id_13\": "  + std::to_string(e.unk_13)        + ",\n";
        j += "        \"id_14\": "  + std::to_string(e.hash_2)        + ",\n";
        j += std::string("        \"id_15\": ") + (e.flag_15 ? "true" : "false") + ",\n";
        j += "        \"id_16\": "  + std::to_string(e.unk_16)        + ",\n";
        j += "        \"id_17\": "  + std::to_string(e.hash_3)        + ",\n";
        j += "        \"id_18\": "  + std::to_string(e.unk_18)        + ",\n";
        j += "        \"id_19\": "  + std::to_string(e.unk_19)        + ",\n";
        j += "        \"id_20\": "  + std::to_string(e.unk_20)        + ",\n";
        j += "        \"id_21\": "  + std::to_string(e.unk_21)        + "\n";
        j += "      }";
    }
    j += "\n    ],\n";
    j += "    \"id_1\": [\n";
    for (size_t i = 0; i < bin.customizeItemUniqueBodyEntries.size(); ++i)
    {
        if (i > 0) j += ",\n";
        const auto& e = bin.customizeItemUniqueBodyEntries[i];
        j += "      {\n";
        j += "        \"id_0\": \"" + JsonEsc(e.asset_name) + "\",\n";
        j += "        \"id_1\": "   + std::to_string(e.char_item_id) + "\n";
        j += "      }";
    }
    j += "\n    ]\n  }\n}\n";
    return j;
}

static bool ParseCustomizeItemUniqueListJson(const std::string& json, ContentsBinData& bin)
{
    const char* p = json.c_str();
    ParseExclusiveArray(p, "\"id_0\"", bin.customizeItemUniqueEntries,
        [](const char* s, const char* e, CustomizeItemUniqueEntry& entry) -> bool {
            { auto* fp = FindField(s, e, "id_0");  ParseUInt32(fp, e, entry.char_item_id); }
            { auto* fp = FindField(s, e, "id_1");  ParseString(fp, e, entry.asset_name, sizeof(entry.asset_name)); }
            { auto* fp = FindField(s, e, "id_2");  ParseUInt32(fp, e, entry.character_hash); }
            { auto* fp = FindField(s, e, "id_3");  ParseUInt32(fp, e, entry.hash_1); }
            { auto* fp = FindField(s, e, "id_4");  ParseString(fp, e, entry.text_key, sizeof(entry.text_key)); }
            { auto* fp = FindField(s, e, "id_5");  ParseString(fp, e, entry.extra_text_key_1, sizeof(entry.extra_text_key_1)); }
            { auto* fp = FindField(s, e, "id_6");  ParseString(fp, e, entry.extra_text_key_2, sizeof(entry.extra_text_key_2)); }
            { auto* fp = FindField(s, e, "id_7");  ParseBool(fp,   e, entry.flag_7); }
            { auto* fp = FindField(s, e, "id_8");  ParseUInt32(fp, e, entry.unk_8); }
            { auto* fp = FindField(s, e, "id_9");  ParseBool(fp,   e, entry.flag_9); }
            { auto* fp = FindField(s, e, "id_10"); ParseUInt32(fp, e, entry.unk_10); }
            { auto* fp = FindField(s, e, "id_11"); ParseUInt32(fp, e, entry.price); }
            { auto* fp = FindField(s, e, "id_12"); ParseUInt32(fp, e, entry.unk_12); }
            { auto* fp = FindField(s, e, "id_13"); ParseUInt32(fp, e, entry.unk_13); }
            { auto* fp = FindField(s, e, "id_14"); ParseUInt32(fp, e, entry.hash_2); }
            { auto* fp = FindField(s, e, "id_15"); ParseBool(fp,   e, entry.flag_15); }
            { auto* fp = FindField(s, e, "id_16"); ParseUInt32(fp, e, entry.unk_16); }
            { auto* fp = FindField(s, e, "id_17"); ParseUInt32(fp, e, entry.hash_3); }
            { auto* fp = FindField(s, e, "id_18"); ParseUInt32(fp, e, entry.unk_18); }
            { auto* fp = FindField(s, e, "id_19"); ParseUInt32(fp, e, entry.unk_19); }
            { auto* fp = FindField(s, e, "id_20"); ParseUInt32(fp, e, entry.unk_20); }
            { auto* fp = FindField(s, e, "id_21"); ParseUInt32(fp, e, entry.unk_21); }
            return true;
        });
    ParseExclusiveArray(p, "\"id_1\"", bin.customizeItemUniqueBodyEntries,
        [](const char* s, const char* e, CustomizeItemUniqueBodyEntry& entry) -> bool {
            { auto* fp = FindField(s, e, "id_0"); ParseString(fp, e, entry.asset_name, sizeof(entry.asset_name)); }
            { auto* fp = FindField(s, e, "id_1"); ParseUInt32(fp, e, entry.char_item_id); }
            return true;
        });
    return true;
}

// -----------------------------------------------------------------------------
//  character_select_list JSON builder / parser
// -----------------------------------------------------------------------------

static std::string BuildCharacterSelectListJson(const ContentsBinData& bin)
{
    std::string j = "{\n  \"version\": 1,\n  \"data\": {\n";
    j += "    \"id_0\": [\n";
    for (size_t i = 0; i < bin.characterSelectHashEntries.size(); ++i)
    {
        if (i > 0) j += ",\n";
        j += "      { \"id_0\": " + std::to_string(bin.characterSelectHashEntries[i].character_hash) + " }";
    }
    j += "\n    ],\n";
    j += "    \"id_1\": [\n";
    for (size_t i = 0; i < bin.characterSelectParamEntries.size(); ++i)
    {
        if (i > 0) j += ",\n";
        const auto& e = bin.characterSelectParamEntries[i];
        j += "      {\n";
        j += "        \"id_0\": " + std::to_string(e.game_version) + ",\n";
        j += "        \"id_1\": " + std::to_string(e.value_1) + "\n";
        j += "      }";
    }
    j += "\n    ]\n  }\n}\n";
    return j;
}

static bool ParseCharacterSelectListJson(const std::string& json, ContentsBinData& bin)
{
    const char* p = json.c_str();
    ParseExclusiveArray(p, "\"id_0\"", bin.characterSelectHashEntries,
        [](const char* s, const char* e, CharacterSelectHashEntry& entry) -> bool {
            { auto* fp = FindField(s, e, "id_0"); ParseUInt32(fp, e, entry.character_hash); }
            return true;
        });
    ParseExclusiveArray(p, "\"id_1\"", bin.characterSelectParamEntries,
        [](const char* s, const char* e, CharacterSelectParamEntry& entry) -> bool {
            { auto* fp = FindField(s, e, "id_0"); ParseUInt32(fp, e, entry.game_version); }
            { auto* fp = FindField(s, e, "id_1"); ParseUInt32(fp, e, entry.value_1); }
            return true;
        });
    return true;
}

// -----------------------------------------------------------------------------
//  customize_item_prohibit_drama_list JSON builder / parser
// -----------------------------------------------------------------------------

static std::string BuildProhibitDramaGroupJson(
    const std::vector<CustomizeItemProhibitDramaEntry>& entries)
{
    std::string j = "[\n";
    for (size_t i = 0; i < entries.size(); ++i)
    {
        if (i > 0) j += ",\n";
        const auto& e = entries[i];
        j += "      {\n";
        j += "        \"id_0\": " + std::to_string(e.value_0) + ",\n";
        j += "        \"id_1\": " + std::to_string(e.value_1) + "\n";
        j += "      }";
    }
    j += "\n    ]";
    return j;
}

static std::string BuildCustomizeItemProhibitDramaListJson(const ContentsBinData& bin)
{
    std::string j = "{\n  \"version\": 1,\n  \"data\": {\n";
    j += "    \"id_0\": " + BuildProhibitDramaGroupJson(bin.prohibitDramaGroup0) + ",\n";
    j += "    \"id_1\": " + BuildProhibitDramaGroupJson(bin.prohibitDramaGroup1) + ",\n";
    j += "    \"id_2\": [";
    for (size_t i = 0; i < bin.prohibitDramaCategoryValues.size(); ++i)
    {
        if (i > 0) j += ", ";
        j += std::to_string(bin.prohibitDramaCategoryValues[i]);
    }
    j += "]\n  }\n}\n";
    return j;
}

static bool ParseCustomizeItemProhibitDramaListJson(const std::string& json, ContentsBinData& bin)
{
    const char* p = json.c_str();
    ParseExclusiveArray(p, "\"id_0\"", bin.prohibitDramaGroup0,
        [](const char* s, const char* e, CustomizeItemProhibitDramaEntry& entry) -> bool {
            { auto* fp = FindField(s, e, "id_0"); ParseInt32(fp, e, entry.value_0); }
            { auto* fp = FindField(s, e, "id_1"); ParseInt32(fp, e, entry.value_1); }
            return true;
        });
    ParseExclusiveArray(p, "\"id_1\"", bin.prohibitDramaGroup1,
        [](const char* s, const char* e, CustomizeItemProhibitDramaEntry& entry) -> bool {
            { auto* fp = FindField(s, e, "id_0"); ParseInt32(fp, e, entry.value_0); }
            { auto* fp = FindField(s, e, "id_1"); ParseInt32(fp, e, entry.value_1); }
            return true;
        });
    // Parse scalar uint32 array id_2
    {
        const char* field = strstr(p, "\"id_2\"");
        if (field)
        {
            const char* arr = strchr(field, '[');
            if (arr)
            {
                ++arr;
                while (true)
                {
                    arr = SkipWS(arr);
                    if (*arr == ']' || *arr == '\0') break;
                    uint32_t v = 0;
                    if (ParseUInt32(arr, arr + 64, v))
                        bin.prohibitDramaCategoryValues.push_back(v);
                    while (*arr && *arr != ',' && *arr != ']') ++arr;
                    if (*arr == ',') ++arr;
                }
            }
        }
    }
    return true;
}

// -----------------------------------------------------------------------------
//  battle_motion_list JSON builder / parser
// -----------------------------------------------------------------------------

static std::string BuildBattleMotionArrayJson(const std::vector<BattleMotionEntry>& entries)
{
    std::string j = "[\n";
    for (size_t i = 0; i < entries.size(); ++i)
    {
        if (i > 0) j += ",\n";
        const auto& e = entries[i];
        j += "      {\n";
        j += "        \"id_0\": " + std::to_string(e.motion_id) + ",\n";
        j += "        \"id_1\": " + std::to_string(e.value_1)   + ",\n";
        j += "        \"id_2\": " + std::to_string(e.value_2)   + "\n";
        j += "      }";
    }
    j += "\n    ]";
    return j;
}

static std::string BuildBattleMotionListJson(const ContentsBinData& bin)
{
    char fBuf[32];
    std::string j = "{\n  \"version\": 1,\n  \"data\": {\n";
    j += "    \"id_0\": "  + BuildBattleMotionArrayJson(bin.battleMotionEntries) + ",\n";
#define FOUT(id, field) snprintf(fBuf,sizeof(fBuf),"%g",bin.field); j += "    \"id_" #id "\": " + std::string(fBuf) + ",\n";
    FOUT(1,battleMotionValue1) FOUT(2,battleMotionValue2) FOUT(3,battleMotionValue3)
    FOUT(4,battleMotionValue4) FOUT(5,battleMotionValue5) FOUT(6,battleMotionValue6)
    FOUT(7,battleMotionValue7) FOUT(8,battleMotionValue8) FOUT(9,battleMotionValue9)
    FOUT(10,battleMotionValue10)
#undef FOUT
    j += "    \"id_11\": " + std::to_string(bin.battleMotionValue11) + ",\n";
    j += "    \"id_12\": " + BuildBattleMotionArrayJson(bin.battleMotionEntriesAlt) + "\n";
    j += "  }\n}\n";
    return j;
}

static void ParseBattleMotionArray(const char* json, const char* arrayKey,
                                   std::vector<BattleMotionEntry>& out)
{
    ParseExclusiveArray(json, arrayKey, out,
        [](const char* s, const char* e, BattleMotionEntry& entry) -> bool {
            uint32_t tmp = 0;
            { auto* fp = FindField(s, e, "id_0"); ParseUInt32(fp, e, tmp); entry.motion_id = static_cast<uint8_t>(tmp); }
            { auto* fp = FindField(s, e, "id_1"); ParseUInt32(fp, e, entry.value_1); }
            { auto* fp = FindField(s, e, "id_2"); ParseUInt32(fp, e, entry.value_2); }
            return true;
        });
}

static bool ParseBattleMotionListJson(const std::string& json, ContentsBinData& bin)
{
    const char* p = json.c_str();
    const size_t jsz = json.size();
    ParseBattleMotionArray(p, "\"id_0\"",  bin.battleMotionEntries);
    { auto* fp = FindField(p, p+jsz, "id_1");  ParseFloat(fp, p+jsz, bin.battleMotionValue1); }
    { auto* fp = FindField(p, p+jsz, "id_2");  ParseFloat(fp, p+jsz, bin.battleMotionValue2); }
    { auto* fp = FindField(p, p+jsz, "id_3");  ParseFloat(fp, p+jsz, bin.battleMotionValue3); }
    { auto* fp = FindField(p, p+jsz, "id_4");  ParseFloat(fp, p+jsz, bin.battleMotionValue4); }
    { auto* fp = FindField(p, p+jsz, "id_5");  ParseFloat(fp, p+jsz, bin.battleMotionValue5); }
    { auto* fp = FindField(p, p+jsz, "id_6");  ParseFloat(fp, p+jsz, bin.battleMotionValue6); }
    { auto* fp = FindField(p, p+jsz, "id_7");  ParseFloat(fp, p+jsz, bin.battleMotionValue7); }
    { auto* fp = FindField(p, p+jsz, "id_8");  ParseFloat(fp, p+jsz, bin.battleMotionValue8); }
    { auto* fp = FindField(p, p+jsz, "id_9");  ParseFloat(fp, p+jsz, bin.battleMotionValue9); }
    { auto* fp = FindField(p, p+jsz, "id_10"); ParseFloat(fp, p+jsz, bin.battleMotionValue10); }
    { auto* fp = FindField(p, p+jsz, "id_11"); ParseUInt32(fp, p+jsz, bin.battleMotionValue11); }
    ParseBattleMotionArray(p, "\"id_12\"", bin.battleMotionEntriesAlt);
    return true;
}

// -----------------------------------------------------------------------------
//  arcade_cpu_list JSON builder / parser
// -----------------------------------------------------------------------------

static std::string BuildArcadeCpuListJson(const ContentsBinData& bin)
{
    std::string j = "{\n  \"version\": 1,\n  \"data\": {\n";
    j += "    \"id_0\": {\n";
    j += "      \"id_0\": " + std::to_string(bin.arcadeCpuSettings.unk_0) + ",\n";
    j += "      \"id_1\": " + std::to_string(bin.arcadeCpuSettings.unk_1) + ",\n";
    j += "      \"id_2\": " + std::to_string(bin.arcadeCpuSettings.unk_2) + "\n";
    j += "    },\n";
    j += "    \"id_1\": [\n";
    for (size_t i = 0; i < bin.arcadeCpuCharacterEntries.size(); ++i)
    {
        if (i > 0) j += ",\n";
        const auto& e = bin.arcadeCpuCharacterEntries[i];
        char fBuf[32];
        j += "      {\n";
        j += "        \"id_0\": " + std::to_string(e.character_hash) + ",\n";
        j += "        \"id_1\": " + std::to_string(e.ai_level) + ",\n";
        snprintf(fBuf,sizeof(fBuf),"%g",e.float_1); j += "        \"id_2\": " + std::string(fBuf) + ",\n";
        j += "        \"id_3\": " + std::to_string(e.uint_2) + ",\n";
        snprintf(fBuf,sizeof(fBuf),"%g",e.float_2); j += "        \"id_4\": " + std::string(fBuf) + ",\n";
        j += "        \"id_5\": " + std::to_string(e.uint_3) + ",\n";
        snprintf(fBuf,sizeof(fBuf),"%g",e.float_3); j += "        \"id_6\": " + std::string(fBuf) + "\n";
        j += "      }";
    }
    j += "\n    ],\n";
    j += "    \"id_2\": [\n";
    for (size_t i = 0; i < bin.arcadeCpuHashGroupA.size(); ++i)
    {
        if (i > 0) j += ",\n";
        j += "      { \"id_0\": " + std::to_string(bin.arcadeCpuHashGroupA[i].value_hash) + " }";
    }
    j += "\n    ],\n";
    j += "    \"id_3\": [\n";
    for (size_t i = 0; i < bin.arcadeCpuHashGroupB.size(); ++i)
    {
        if (i > 0) j += ",\n";
        j += "      { \"id_0\": " + std::to_string(bin.arcadeCpuHashGroupB[i].value_hash) + " }";
    }
    j += "\n    ],\n";
    j += "    \"id_4\": [\n";
    for (size_t i = 0; i < bin.arcadeCpuRuleEntries.size(); ++i)
    {
        if (i > 0) j += ",\n";
        const auto& e = bin.arcadeCpuRuleEntries[i];
        j += "      {\n";
        j += "        \"id_0\": " + std::to_string(e.flag_0)  + ",\n";
        j += "        \"id_1\": " + std::to_string(e.flag_1)  + ",\n";
        j += "        \"id_2\": " + std::to_string(e.value_2) + ",\n";
        j += "        \"id_3\": " + std::to_string(e.value_3) + "\n";
        j += "      }";
    }
    j += "\n    ]\n  }\n}\n";
    return j;
}

static bool ParseArcadeCpuListJson(const std::string& json, ContentsBinData& bin)
{
    const char* p = json.c_str();
    const size_t jsz = json.size();
    // Parse settings singleton
    {
        const char* sf = FindField(p, p + jsz, "id_0");
        if (sf && *sf == '{')
        {
            const char* se = SkipObject(sf);
            if (se)
            {
                { auto* fp = FindField(sf, se, "id_0"); ParseUInt32(fp, se, bin.arcadeCpuSettings.unk_0); }
                { auto* fp = FindField(sf, se, "id_1"); ParseUInt32(fp, se, bin.arcadeCpuSettings.unk_1); }
                { auto* fp = FindField(sf, se, "id_2"); ParseUInt32(fp, se, bin.arcadeCpuSettings.unk_2); }
            }
        }
    }
    ParseExclusiveArray(p, "\"id_1\"", bin.arcadeCpuCharacterEntries,
        [](const char* s, const char* e, ArcadeCpuCharacterEntry& entry) -> bool {
            { auto* fp = FindField(s, e, "id_0"); ParseUInt32(fp, e, entry.character_hash); }
            { auto* fp = FindField(s, e, "id_1"); ParseUInt32(fp, e, entry.ai_level); }
            { auto* fp = FindField(s, e, "id_2"); ParseFloat(fp,  e, entry.float_1); }
            { auto* fp = FindField(s, e, "id_3"); ParseUInt32(fp, e, entry.uint_2); }
            { auto* fp = FindField(s, e, "id_4"); ParseFloat(fp,  e, entry.float_2); }
            { auto* fp = FindField(s, e, "id_5"); ParseUInt32(fp, e, entry.uint_3); }
            { auto* fp = FindField(s, e, "id_6"); ParseFloat(fp,  e, entry.float_3); }
            return true;
        });
    ParseExclusiveArray(p, "\"id_2\"", bin.arcadeCpuHashGroupA,
        [](const char* s, const char* e, ArcadeCpuHashEntry& entry) -> bool {
            { auto* fp = FindField(s, e, "id_0"); ParseUInt32(fp, e, entry.value_hash); }
            return true;
        });
    ParseExclusiveArray(p, "\"id_3\"", bin.arcadeCpuHashGroupB,
        [](const char* s, const char* e, ArcadeCpuHashEntry& entry) -> bool {
            { auto* fp = FindField(s, e, "id_0"); ParseUInt32(fp, e, entry.value_hash); }
            return true;
        });
    ParseExclusiveArray(p, "\"id_4\"", bin.arcadeCpuRuleEntries,
        [](const char* s, const char* e, ArcadeCpuRuleEntry& entry) -> bool {
            uint32_t tmp = 0;
            { auto* fp = FindField(s, e, "id_0"); ParseUInt32(fp, e, tmp); entry.flag_0 = static_cast<uint8_t>(tmp); }
            { auto* fp = FindField(s, e, "id_1"); ParseUInt32(fp, e, tmp); entry.flag_1 = static_cast<uint8_t>(tmp); }
            { auto* fp = FindField(s, e, "id_2"); ParseUInt32(fp, e, entry.value_2); }
            { auto* fp = FindField(s, e, "id_3"); ParseUInt32(fp, e, entry.value_3); }
            return true;
        });
    return true;
}

// -----------------------------------------------------------------------------
//  ball_recommend_list JSON builder / parser
// -----------------------------------------------------------------------------

static std::string BuildBallRecommendGroupJson(const std::vector<BallRecommendEntry>& entries)
{
    std::string j = "[\n";
    for (size_t i = 0; i < entries.size(); ++i)
    {
        if (i > 0) j += ",\n";
        const auto& e = entries[i];
        j += "      {\n";
        j += "        \"id_0\": "   + std::to_string(e.character_hash) + ",\n";
        j += "        \"id_1\": \"" + JsonEsc(e.move_name_key) + "\",\n";
        j += "        \"id_2\": \"" + JsonEsc(e.command_text_key) + "\",\n";
        j += "        \"id_3\": "   + std::to_string(e.unk_3) + ",\n";
        j += "        \"id_4\": "   + std::to_string(e.unk_4) + "\n";
        j += "      }";
    }
    j += "\n    ]";
    return j;
}

static std::string BuildBallRecommendListJson(const ContentsBinData& bin)
{
    std::string j = "{\n  \"version\": 1,\n  \"data\": {\n";
    j += "    \"id_0\": " + BuildBallRecommendGroupJson(bin.ballRecommendGroup0) + ",\n";
    j += "    \"id_1\": " + BuildBallRecommendGroupJson(bin.ballRecommendGroup1) + ",\n";
    j += "    \"id_2\": " + BuildBallRecommendGroupJson(bin.ballRecommendGroup2) + ",\n";
    j += "    \"id_3\": [";
    for (size_t i = 0; i < bin.ballRecommendUnkValues.size(); ++i)
    {
        if (i > 0) j += ", ";
        j += std::to_string(bin.ballRecommendUnkValues[i]);
    }
    j += "]\n  }\n}\n";
    return j;
}

static void ParseBallRecommendGroup(const char* json, const char* arrayKey,
                                    std::vector<BallRecommendEntry>& out)
{
    ParseExclusiveArray(json, arrayKey, out,
        [](const char* s, const char* e, BallRecommendEntry& entry) -> bool {
            { auto* fp = FindField(s, e, "id_0"); ParseUInt32(fp, e, entry.character_hash); }
            { auto* fp = FindField(s, e, "id_1"); ParseString(fp, e, entry.move_name_key, sizeof(entry.move_name_key)); }
            { auto* fp = FindField(s, e, "id_2"); ParseString(fp, e, entry.command_text_key, sizeof(entry.command_text_key)); }
            { auto* fp = FindField(s, e, "id_3"); ParseUInt32(fp, e, entry.unk_3); }
            { auto* fp = FindField(s, e, "id_4"); ParseUInt32(fp, e, entry.unk_4); }
            return true;
        });
}

static bool ParseBallRecommendListJson(const std::string& json, ContentsBinData& bin)
{
    const char* p = json.c_str();
    ParseBallRecommendGroup(p, "\"id_0\"", bin.ballRecommendGroup0);
    ParseBallRecommendGroup(p, "\"id_1\"", bin.ballRecommendGroup1);
    ParseBallRecommendGroup(p, "\"id_2\"", bin.ballRecommendGroup2);
    // scalar uint32 array id_3
    {
        const char* field = strstr(p, "\"id_3\"");
        if (field)
        {
            const char* arr = strchr(field, '[');
            if (arr) { ++arr;
                while (true) {
                    arr = SkipWS(arr);
                    if (*arr == ']' || *arr == '\0') break;
                    uint32_t v = 0;
                    if (ParseUInt32(arr, arr + 64, v)) bin.ballRecommendUnkValues.push_back(v);
                    while (*arr && *arr != ',' && *arr != ']') ++arr;
                    if (*arr == ',') ++arr;
                }
            }
        }
    }
    return true;
}

// -----------------------------------------------------------------------------
//  ball_setting_list JSON builder / parser
// -----------------------------------------------------------------------------

static std::string BuildBallSettingListJson(const ContentsBinData& bin)
{
    const auto& d = bin.ballSettingData;
    char fBuf[32];
    std::string j = "{\n  \"version\": 1,\n  \"data\": {\n";
#define FOUT(id, field) snprintf(fBuf,sizeof(fBuf),"%g",d.field); j += "    \"id_" #id "\": " + std::string(fBuf) + ",\n";
#define UOUT(id, field) j += "    \"id_" #id "\": " + std::to_string(d.field) + ",\n";
#define UOUT_LAST(id, field) j += "    \"id_" #id "\": " + std::to_string(d.field) + "\n";
    FOUT(0,value_0) FOUT(1,value_1) FOUT(2,value_2) FOUT(3,value_3) FOUT(4,value_4)
    FOUT(5,value_5) FOUT(6,value_6) FOUT(7,value_7) FOUT(8,value_8)
    UOUT(9,value_9) UOUT(10,value_10)
    FOUT(11,value_11) FOUT(12,value_12) FOUT(13,value_13) FOUT(14,value_14)
    UOUT(15,value_15)
    FOUT(16,value_16)
    UOUT(17,value_17)
    FOUT(18,value_18) FOUT(19,value_19) FOUT(20,value_20) FOUT(21,value_21)
    FOUT(22,value_22) FOUT(23,value_23)
    UOUT(24,value_24) UOUT(25,value_25) UOUT(26,value_26) UOUT(27,value_27)
    UOUT(28,value_28) UOUT(29,value_29)
    FOUT(30,value_30)
    UOUT(31,value_31)
    FOUT(32,value_32)
    UOUT(33,value_33) UOUT(34,value_34) UOUT(35,value_35) UOUT(36,value_36)
    UOUT(37,value_37) UOUT(38,value_38) UOUT(39,value_39) UOUT(40,value_40)
    FOUT(41,value_41) FOUT(42,value_42) FOUT(43,value_43)
    UOUT(44,value_44) UOUT(45,value_45)
    FOUT(46,value_46)
    UOUT(47,value_47) UOUT(48,value_48) UOUT(49,value_49) UOUT(50,value_50)
    UOUT(51,value_51) UOUT(52,value_52)
    FOUT(53,value_53) FOUT(54,value_54)
    UOUT(55,value_55)
    FOUT(56,value_56) FOUT(57,value_57)
    UOUT(58,value_58) UOUT(59,value_59) UOUT(60,value_60)
    FOUT(61,value_61) FOUT(62,value_62)
    UOUT(63,value_63) UOUT(64,value_64) UOUT(65,value_65) UOUT(66,value_66)
    UOUT(67,value_67) UOUT(68,value_68) UOUT(69,value_69) UOUT(70,value_70)
    UOUT_LAST(71,value_71)
#undef FOUT
#undef UOUT
#undef UOUT_LAST
    j += "  }\n}\n";
    return j;
}

static bool ParseBallSettingListJson(const std::string& json, ContentsBinData& bin)
{
    const char* p = json.c_str();
    const size_t jsz = json.size();
    auto& d = bin.ballSettingData;
#define PFLT(id, field) { auto* fp = FindField(p, p+jsz, id); ParseFloat(fp, p+jsz, d.field); }
#define PU32(id, field) { auto* fp = FindField(p, p+jsz, id); ParseUInt32(fp, p+jsz, d.field); }
    PFLT("id_0",value_0) PFLT("id_1",value_1) PFLT("id_2",value_2) PFLT("id_3",value_3)
    PFLT("id_4",value_4) PFLT("id_5",value_5) PFLT("id_6",value_6) PFLT("id_7",value_7)
    PFLT("id_8",value_8)
    PU32("id_9",value_9) PU32("id_10",value_10)
    PFLT("id_11",value_11) PFLT("id_12",value_12) PFLT("id_13",value_13) PFLT("id_14",value_14)
    PU32("id_15",value_15)
    PFLT("id_16",value_16)
    PU32("id_17",value_17)
    PFLT("id_18",value_18) PFLT("id_19",value_19) PFLT("id_20",value_20) PFLT("id_21",value_21)
    PFLT("id_22",value_22) PFLT("id_23",value_23)
    PU32("id_24",value_24) PU32("id_25",value_25) PU32("id_26",value_26) PU32("id_27",value_27)
    PU32("id_28",value_28) PU32("id_29",value_29)
    PFLT("id_30",value_30)
    PU32("id_31",value_31)
    PFLT("id_32",value_32)
    PU32("id_33",value_33) PU32("id_34",value_34) PU32("id_35",value_35) PU32("id_36",value_36)
    PU32("id_37",value_37) PU32("id_38",value_38) PU32("id_39",value_39) PU32("id_40",value_40)
    PFLT("id_41",value_41) PFLT("id_42",value_42) PFLT("id_43",value_43)
    PU32("id_44",value_44) PU32("id_45",value_45)
    PFLT("id_46",value_46)
    PU32("id_47",value_47) PU32("id_48",value_48) PU32("id_49",value_49) PU32("id_50",value_50)
    PU32("id_51",value_51) PU32("id_52",value_52)
    PFLT("id_53",value_53) PFLT("id_54",value_54)
    PU32("id_55",value_55)
    PFLT("id_56",value_56) PFLT("id_57",value_57)
    PU32("id_58",value_58) PU32("id_59",value_59) PU32("id_60",value_60)
    PFLT("id_61",value_61) PFLT("id_62",value_62)
    PU32("id_63",value_63) PU32("id_64",value_64) PU32("id_65",value_65) PU32("id_66",value_66)
    PU32("id_67",value_67) PU32("id_68",value_68) PU32("id_69",value_69) PU32("id_70",value_70)
    PU32("id_71",value_71)
#undef PFLT
#undef PU32
    return true;
}

// -----------------------------------------------------------------------------
//  battle_common_list JSON builder / parser
// -----------------------------------------------------------------------------

static std::string BuildBattleCommonSingleValueArrayJson(
    const std::vector<BattleCommonSingleValueEntry>& entries)
{
    std::string j = "[\n";
    for (size_t i = 0; i < entries.size(); ++i)
    {
        if (i > 0) j += ",\n";
        j += "      { \"id_0\": " + std::to_string(entries[i].value) + " }";
    }
    j += "\n    ]";
    return j;
}

static std::string BuildBattleCommonCharacterScaleArrayJson(
    const std::vector<BattleCommonCharacterScaleEntry>& entries)
{
    std::string j = "[\n";
    for (size_t i = 0; i < entries.size(); ++i)
    {
        if (i > 0) j += ",\n";
        const auto& e = entries[i];
        char fBuf[32];
        j += "      {\n";
        j += "        \"id_0\": " + std::to_string(e.hash_0) + ",\n";
#define FOUT(id, field) snprintf(fBuf,sizeof(fBuf),"%g",e.field); j += "        \"id_" #id "\": " + std::string(fBuf) + ",\n";
#define FOUT_LAST(id, field) snprintf(fBuf,sizeof(fBuf),"%g",e.field); j += "        \"id_" #id "\": " + std::string(fBuf) + "\n";
        FOUT(1,value_1) FOUT(2,value_2) FOUT(3,value_3) FOUT(4,value_4)
        FOUT(5,value_5) FOUT(6,value_6) FOUT_LAST(7,value_7)
#undef FOUT
#undef FOUT_LAST
        j += "      }";
    }
    j += "\n    ]";
    return j;
}

static std::string BuildBattleCommonPairArrayJson(
    const std::vector<BattleCommonPairEntry>& entries)
{
    std::string j = "[\n";
    for (size_t i = 0; i < entries.size(); ++i)
    {
        if (i > 0) j += ",\n";
        const auto& e = entries[i];
        j += "      {\n";
        j += "        \"id_0\": " + std::to_string(e.value_0) + ",\n";
        j += "        \"id_1\": " + std::to_string(e.value_1) + ",\n";
        j += "        \"id_2\": " + std::to_string(e.value_2) + "\n";
        j += "      }";
    }
    j += "\n    ]";
    return j;
}

static std::string BuildBattleCommonMiscArrayJson(
    const std::vector<BattleCommonMiscEntry>& entries)
{
    std::string j = "[\n";
    for (size_t i = 0; i < entries.size(); ++i)
    {
        if (i > 0) j += ",\n";
        const auto& e = entries[i];
        char fBuf[32];
        j += "      {\n";
        snprintf(fBuf,sizeof(fBuf),"%g",e.value_0); j += "        \"id_0\": " + std::string(fBuf) + ",\n";
        snprintf(fBuf,sizeof(fBuf),"%g",e.value_1); j += "        \"id_1\": " + std::string(fBuf) + ",\n";
        snprintf(fBuf,sizeof(fBuf),"%g",e.value_2); j += "        \"id_2\": " + std::string(fBuf) + "\n";
        j += "      }";
    }
    j += "\n    ]";
    return j;
}

static std::string BuildBattleCommonListJson(const ContentsBinData& bin)
{
    char fBuf[32];
    std::string j = "{\n  \"version\": 1,\n  \"data\": {\n";
    j += "    \"id_0\": "  + BuildBattleCommonSingleValueArrayJson(bin.battleCommonSingleValueEntries) + ",\n";
    j += "    \"id_1\": "  + BuildBattleCommonCharacterScaleArrayJson(bin.battleCommonCharacterScaleEntries) + ",\n";
    j += "    \"id_2\": "  + BuildBattleCommonPairArrayJson(bin.battleCommonPairEntries) + ",\n";
    j += "    \"id_3\": "  + std::to_string(bin.battleCommonValue3) + ",\n";
    j += "    \"id_4\": "  + std::to_string(bin.battleCommonValue4) + ",\n";
    j += "    \"id_5\": "  + BuildBattleCommonMiscArrayJson(bin.battleCommonMiscEntries) + ",\n";
    snprintf(fBuf,sizeof(fBuf),"%g",bin.battleCommonValue6);  j += "    \"id_6\": "  + std::string(fBuf) + ",\n";
    snprintf(fBuf,sizeof(fBuf),"%g",bin.battleCommonValue7);  j += "    \"id_7\": "  + std::string(fBuf) + ",\n";
    snprintf(fBuf,sizeof(fBuf),"%g",bin.battleCommonValue8);  j += "    \"id_8\": "  + std::string(fBuf) + ",\n";
    j += "    \"id_9\": "  + std::to_string(bin.battleCommonValue9) + ",\n";
    snprintf(fBuf,sizeof(fBuf),"%g",bin.battleCommonValue10); j += "    \"id_10\": " + std::string(fBuf) + ",\n";
    j += "    \"id_11\": " + std::to_string(bin.battleCommonValue11) + ",\n";
    j += "    \"id_12\": " + std::to_string(bin.battleCommonValue12) + ",\n";
    j += "    \"id_13\": " + std::to_string(bin.battleCommonValue13) + "\n";
    j += "  }\n}\n";
    return j;
}

static bool ParseBattleCommonListJson(const std::string& json, ContentsBinData& bin)
{
    const char* p = json.c_str();
    const size_t jsz = json.size();
    ParseExclusiveArray(p, "\"id_0\"", bin.battleCommonSingleValueEntries,
        [](const char* s, const char* e, BattleCommonSingleValueEntry& entry) -> bool {
            { auto* fp = FindField(s, e, "id_0"); ParseUInt32(fp, e, entry.value); }
            return true;
        });
    ParseExclusiveArray(p, "\"id_1\"", bin.battleCommonCharacterScaleEntries,
        [](const char* s, const char* e, BattleCommonCharacterScaleEntry& entry) -> bool {
            { auto* fp = FindField(s, e, "id_0"); ParseUInt32(fp, e, entry.hash_0); }
            { auto* fp = FindField(s, e, "id_1"); ParseFloat(fp,  e, entry.value_1); }
            { auto* fp = FindField(s, e, "id_2"); ParseFloat(fp,  e, entry.value_2); }
            { auto* fp = FindField(s, e, "id_3"); ParseFloat(fp,  e, entry.value_3); }
            { auto* fp = FindField(s, e, "id_4"); ParseFloat(fp,  e, entry.value_4); }
            { auto* fp = FindField(s, e, "id_5"); ParseFloat(fp,  e, entry.value_5); }
            { auto* fp = FindField(s, e, "id_6"); ParseFloat(fp,  e, entry.value_6); }
            { auto* fp = FindField(s, e, "id_7"); ParseFloat(fp,  e, entry.value_7); }
            return true;
        });
    ParseExclusiveArray(p, "\"id_2\"", bin.battleCommonPairEntries,
        [](const char* s, const char* e, BattleCommonPairEntry& entry) -> bool {
            { auto* fp = FindField(s, e, "id_0"); ParseUInt32(fp, e, entry.value_0); }
            { auto* fp = FindField(s, e, "id_1"); ParseUInt32(fp, e, entry.value_1); }
            { auto* fp = FindField(s, e, "id_2"); ParseUInt32(fp, e, entry.value_2); }
            return true;
        });
    { auto* fp = FindField(p, p+jsz, "id_3");  ParseUInt32(fp, p+jsz, bin.battleCommonValue3); }
    { auto* fp = FindField(p, p+jsz, "id_4");  ParseUInt32(fp, p+jsz, bin.battleCommonValue4); }
    ParseExclusiveArray(p, "\"id_5\"", bin.battleCommonMiscEntries,
        [](const char* s, const char* e, BattleCommonMiscEntry& entry) -> bool {
            { auto* fp = FindField(s, e, "id_0"); ParseFloat(fp, e, entry.value_0); }
            { auto* fp = FindField(s, e, "id_1"); ParseFloat(fp, e, entry.value_1); }
            { auto* fp = FindField(s, e, "id_2"); ParseFloat(fp, e, entry.value_2); }
            return true;
        });
    { auto* fp = FindField(p, p+jsz, "id_6");  ParseFloat(fp,  p+jsz, bin.battleCommonValue6); }
    { auto* fp = FindField(p, p+jsz, "id_7");  ParseFloat(fp,  p+jsz, bin.battleCommonValue7); }
    { auto* fp = FindField(p, p+jsz, "id_8");  ParseFloat(fp,  p+jsz, bin.battleCommonValue8); }
    { auto* fp = FindField(p, p+jsz, "id_9");  ParseUInt32(fp, p+jsz, bin.battleCommonValue9); }
    { auto* fp = FindField(p, p+jsz, "id_10"); ParseFloat(fp,  p+jsz, bin.battleCommonValue10); }
    { auto* fp = FindField(p, p+jsz, "id_11"); ParseUInt32(fp, p+jsz, bin.battleCommonValue11); }
    { auto* fp = FindField(p, p+jsz, "id_12"); ParseUInt32(fp, p+jsz, bin.battleCommonValue12); }
    { auto* fp = FindField(p, p+jsz, "id_13"); ParseUInt32(fp, p+jsz, bin.battleCommonValue13); }
    return true;
}

// -----------------------------------------------------------------------------
//  battle_cpu_list JSON builder / parser
// -----------------------------------------------------------------------------

static std::string BuildBattleCpuRankEntryJson(const BattleCpuRankEntry& e, const std::string& pad)
{
    std::string j = pad + "{\n";
    for (int fi = 0; fi < 47; ++fi)
        j += pad + "  \"id_" + std::to_string(fi) + "\": " + std::to_string(e.values[fi]) + ",\n";
    j += pad + "  \"id_47\": \"" + JsonEsc(e.rank_label) + "\"\n";
    j += pad + "}";
    return j;
}

static std::string BuildBattleCpuListJson(const ContentsBinData& bin)
{
    std::string j = "{\n  \"version\": 1,\n  \"data\": {\n";
    j += "    \"id_0\": [\n";
    for (size_t i = 0; i < bin.battleCpuRankEntries.size(); ++i)
    {
        if (i > 0) j += ",\n";
        j += BuildBattleCpuRankEntryJson(bin.battleCpuRankEntries[i], "      ");
    }
    j += "\n    ],\n";
    j += "    \"id_1\": [\n";
    for (size_t i = 0; i < bin.battleCpuStepEntries.size(); ++i)
    {
        if (i > 0) j += ",\n";
        const auto& e = bin.battleCpuStepEntries[i];
        j += "      {\n";
        j += "        \"id_0\": " + std::to_string(e.value_0) + ",\n";
        j += "        \"id_1\": " + std::to_string(e.value_1) + ",\n";
        j += "        \"id_2\": " + std::to_string(e.value_2) + ",\n";
        j += "        \"id_3\": " + std::to_string(e.value_3) + "\n";
        j += "      }";
    }
    j += "\n    ],\n";
    j += "    \"id_2\": [";
    for (size_t i = 0; i < bin.battleCpuParamValues.size(); ++i)
    {
        if (i > 0) j += ", ";
        j += std::to_string(bin.battleCpuParamValues[i]);
    }
    j += "],\n";
    j += "    \"id_3\": " + BuildBattleCpuRankEntryJson(bin.battleCpuRankExEntry, "    ") + "\n";
    j += "  }\n}\n";
    return j;
}

static void ParseBattleCpuRankEntryFromObj(const char* start, const char* end,
                                           BattleCpuRankEntry& e)
{
    for (int fi = 0; fi < 47; ++fi)
    {
        char key[16];
        snprintf(key, sizeof(key), "id_%d", fi);
        auto* fp = FindField(start, end, key);
        ParseUInt32(fp, end, e.values[fi]);
    }
    { auto* fp = FindField(start, end, "id_47"); ParseString(fp, end, e.rank_label, sizeof(e.rank_label)); }
}

static bool ParseBattleCpuListJson(const std::string& json, ContentsBinData& bin)
{
    const char* p = json.c_str();
    const size_t jsz = json.size();
    ParseExclusiveArray(p, "\"id_0\"", bin.battleCpuRankEntries,
        [](const char* s, const char* e, BattleCpuRankEntry& entry) -> bool {
            ParseBattleCpuRankEntryFromObj(s, e, entry);
            return true;
        });
    ParseExclusiveArray(p, "\"id_1\"", bin.battleCpuStepEntries,
        [](const char* s, const char* e, BattleCpuStepEntry& entry) -> bool {
            { auto* fp = FindField(s, e, "id_0"); ParseUInt32(fp, e, entry.value_0); }
            { auto* fp = FindField(s, e, "id_1"); ParseUInt32(fp, e, entry.value_1); }
            { auto* fp = FindField(s, e, "id_2"); ParseUInt32(fp, e, entry.value_2); }
            { auto* fp = FindField(s, e, "id_3"); ParseUInt32(fp, e, entry.value_3); }
            return true;
        });
    // scalar int32 array id_2
    {
        const char* field = strstr(p, "\"id_2\"");
        if (field)
        {
            const char* arr = strchr(field, '[');
            if (arr) { ++arr;
                while (true) {
                    arr = SkipWS(arr);
                    if (*arr == ']' || *arr == '\0') break;
                    int32_t v = 0;
                    ParseInt32(arr, arr + 32, v);
                    bin.battleCpuParamValues.push_back(v);
                    while (*arr && *arr != ',' && *arr != ']') ++arr;
                    if (*arr == ',') ++arr;
                }
            }
        }
    }
    // singleton rank_ex_entry id_3
    {
        const char* sf = FindField(p, p + jsz, "id_3");
        if (sf && *sf == '{')
        {
            const char* se = SkipObject(sf);
            if (se) ParseBattleCpuRankEntryFromObj(sf, se, bin.battleCpuRankExEntry);
        }
    }
    return true;
}

// -----------------------------------------------------------------------------
//  rank_list JSON builder / parser
// -----------------------------------------------------------------------------

static std::string BuildRankListJson(const ContentsBinData& bin)
{
    std::string j = "{\n  \"version\": 1,\n  \"data\": {\n    \"id_0\": [\n";
    for (size_t gi = 0; gi < bin.rankGroups.size(); ++gi)
    {
        if (gi > 0) j += ",\n";
        const auto& g = bin.rankGroups[gi];
        j += "      { \"id_0\": " + std::to_string(g.group_id) + ", \"id_1\": [\n";
        for (size_t ii = 0; ii < g.entries.size(); ++ii)
        {
            if (ii > 0) j += ",\n";
            const auto& item = g.entries[ii];
            j += "        {\n";
            j += "          \"id_0\": " + std::to_string(item.hash) + ",\n";
            j += "          \"id_1\": \"" + JsonEsc(item.text_key) + "\",\n";
            j += "          \"id_2\": \"" + JsonEsc(item.name) + "\",\n";
            j += "          \"id_3\": " + std::to_string(item.rank) + "\n";
            j += "        }";
        }
        j += "\n      ]}";
    }
    j += "\n    ]\n  }\n}\n";
    return j;
}

static bool ParseRankListJson(const std::string& json, ContentsBinData& bin)
{
    const char* p = json.c_str();
    const char* outerArr = strstr(p, "\"id_0\"");
    if (!outerArr) return false;
    const char* arr = strchr(outerArr, '[');
    if (!arr) return false;
    ++arr;

    while (true)
    {
        arr = SkipWS(arr);
        if (*arr == ']' || *arr == '\0') break;
        if (*arr == '{')
        {
            const char* groupEnd = SkipObject(arr);
            if (!groupEnd) break;
            RankGroup g;
            { auto* fp = FindField(arr, groupEnd, "id_0"); ParseUInt32(fp, groupEnd, g.group_id); }
            // Parse inner array id_1
            const char* id1f = FindField(arr, groupEnd, "id_1");
            if (id1f && *id1f == '[')
            {
                const char* innerArr = id1f + 1;
                while (true)
                {
                    innerArr = SkipWS(innerArr);
                    if (*innerArr == ']' || *innerArr == '\0') break;
                    if (*innerArr == '{')
                    {
                        const char* itemEnd = SkipObject(innerArr);
                        if (!itemEnd) break;
                        RankItem item{};
                        { auto* fp = FindField(innerArr, itemEnd, "id_0"); ParseUInt32(fp, itemEnd, item.hash); }
                        { auto* fp = FindField(innerArr, itemEnd, "id_1"); ParseString(fp, itemEnd, item.text_key, sizeof(item.text_key)); }
                        { auto* fp = FindField(innerArr, itemEnd, "id_2"); ParseString(fp, itemEnd, item.name, sizeof(item.name)); }
                        { auto* fp = FindField(innerArr, itemEnd, "id_3"); ParseUInt32(fp, itemEnd, item.rank); }
                        g.entries.push_back(item);
                        innerArr = itemEnd;
                    }
                    while (*innerArr == ',' || *innerArr == ' ' || *innerArr == '\n' || *innerArr == '\r' || *innerArr == '\t') ++innerArr;
                }
            }
            bin.rankGroups.push_back(std::move(g));
            arr = groupEnd;
        }
        while (*arr == ',' || *arr == ' ' || *arr == '\n' || *arr == '\r' || *arr == '\t') ++arr;
    }
    return true;
}

// -----------------------------------------------------------------------------
//  assist_input_list JSON builder / parser
// -----------------------------------------------------------------------------

static std::string BuildAssistInputListJson(const ContentsBinData& bin)
{
    std::string j = "{\n  \"version\": 1,\n  \"data\": {\n    \"entries\": [\n";
    for (size_t i = 0; i < bin.assistInputEntries.size(); ++i)
    {
        if (i > 0) j += ",\n";
        const auto& e = bin.assistInputEntries[i];
        j += "      {\n";
        j += "        \"id_0\": " + std::to_string(e.character_hash) + ",\n";
        for (int fi = 0; fi < 58; ++fi)
        {
            j += "        \"id_" + std::to_string(fi + 1) + "\": " + std::to_string(e.values[fi]);
            j += (fi < 57) ? ",\n" : "\n";
        }
        j += "      }";
    }
    j += "\n    ]\n  }\n}\n";
    return j;
}

static bool ParseAssistInputListJson(const std::string& json, ContentsBinData& bin)
{
    const char* p = json.c_str();
    const char* ef = strstr(p, "\"entries\""); if (!ef) return false;
    const char* arr = strchr(ef, '['); if (!arr) return false;
    ++arr;
    while (true)
    {
        arr = SkipWS(arr);
        if (*arr == ']' || *arr == '\0') break;
        if (*arr == '{')
        {
            const char* objEnd = SkipObject(arr); if (!objEnd) break;
            AssistInputEntry e{};
            { auto* fp = FindField(arr, objEnd, "id_0"); ParseUInt32(fp, objEnd, e.character_hash); }
            for (int fi = 0; fi < 58; ++fi)
            {
                char key[16];
                snprintf(key, sizeof(key), "id_%d", fi + 1);
                auto* fp = FindField(arr, objEnd, key);
                ParseInt32(fp, objEnd, e.values[fi]);
            }
            bin.assistInputEntries.push_back(e);
            arr = objEnd;
        }
        while (*arr == ',' || *arr == ' ' || *arr == '\n' || *arr == '\r' || *arr == '\t') ++arr;
    }
    return true;
}

// ???????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????//  Win32 file dialogs + wide/narrow conversion
// ???????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????
static std::string WideToUtf8(const wchar_t* w)
{
    if (!w || !*w) return {};
    const int len = WideCharToMultiByte(CP_UTF8, 0, w, -1, nullptr, 0, nullptr, nullptr);
    std::string s(len - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w, -1, &s[0], len, nullptr, nullptr);
    return s;
}

static std::wstring GetExeDir()
{
    wchar_t buf[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, buf, MAX_PATH);
    std::wstring path(buf);
    const size_t pos = path.rfind(L'\\');
    return (pos != std::wstring::npos) ? path.substr(0, pos) : path;
}

static std::string OpenSaveDialog()
{
    const std::wstring initDir = GetExeDir();
    wchar_t szFile[1024] = {};
    OPENFILENAMEW ofn    = {};
    ofn.lStructSize    = sizeof(ofn);
    ofn.lpstrFile      = szFile;
    ofn.nMaxFile       = (DWORD)(sizeof(szFile) / sizeof(wchar_t));
    ofn.lpstrFilter    = L"TkMod Files\0*.tkmod\0All Files\0*.*\0";
    ofn.lpstrDefExt    = L"tkmod";
    ofn.lpstrInitialDir = initDir.c_str();
    ofn.Flags          = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
    if (!GetSaveFileNameW(&ofn)) return {};
    return WideToUtf8(szFile);
}

static std::string OpenLoadDialog()
{
    wchar_t szFile[1024] = {};
    OPENFILENAMEW ofn    = {};
    ofn.lStructSize  = sizeof(ofn);
    ofn.lpstrFile    = szFile;
    ofn.nMaxFile     = (DWORD)(sizeof(szFile) / sizeof(wchar_t));
    ofn.lpstrFilter  = L"TkMod Files\0*.tkmod\0All Files\0*.*\0";
    ofn.Flags        = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    if (!GetOpenFileNameW(&ofn)) return {};
    return WideToUtf8(szFile);
}

// ???????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????//  Public API
// ???????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????
namespace TkmodIO
{
    bool SaveDialog(const ModData& data)
    {
        const std::string path = OpenSaveDialog();
        if (path.empty()) return false;

        std::vector<ZipEntry> entries;

        // mod_info.json
        {
            ZipEntry e;
            e.name = "mod_info.json";
            const std::string json = BuildModInfoJson();
            e.data.assign(json.begin(), json.end());
            entries.push_back(std::move(e));
        }

        // fbsdata_mod/<bin>.json for each content bin
        for (const auto& bin : data.contents)
        {
            std::string json;
            if (bin.type == BinType::CustomizeItemCommonList)
                json = BuildCustomizeItemCommonJson(bin);
            else if (bin.type == BinType::CharacterList)
                json = BuildCharacterListJson(bin);
            else if (bin.type == BinType::CustomizeItemExclusiveList)
                json = BuildCustomizeItemExclusiveListJson(bin);
            else if (bin.type == BinType::AreaList)
                json = BuildAreaListJson(bin);
            else if (bin.type == BinType::BattleSubtitleInfoList)
                json = BuildBattleSubtitleInfoJson(bin);
            else if (bin.type == BinType::FateDramaPlayerStartList)
                json = BuildFateDramaPlayerStartListJson(bin);
            else if (bin.type == BinType::JukeboxList)
                json = BuildJukeboxListJson(bin);
            else if (bin.type == BinType::SeriesList)
                json = BuildSeriesListJson(bin);
            else if (bin.type == BinType::TamMissionList)
                json = BuildTamMissionListJson(bin);
            else if (bin.type == BinType::DramaPlayerStartList)
                json = BuildDramaPlayerStartListJson(bin);
            else if (bin.type == BinType::StageList)
                json = BuildStageListJson(bin);
            else if (bin.type == BinType::BallPropertyList)
                json = BuildBallPropertyListJson(bin);
            else if (bin.type == BinType::BodyCylinderDataList)
                json = BuildBodyCylinderDataListJson(bin);
            else if (bin.type == BinType::CustomizeItemUniqueList)
                json = BuildCustomizeItemUniqueListJson(bin);
            else if (bin.type == BinType::CharacterSelectList)
                json = BuildCharacterSelectListJson(bin);
            else if (bin.type == BinType::CustomizeItemProhibitDramaList)
                json = BuildCustomizeItemProhibitDramaListJson(bin);
            else if (bin.type == BinType::BattleMotionList)
                json = BuildBattleMotionListJson(bin);
            else if (bin.type == BinType::ArcadeCpuList)
                json = BuildArcadeCpuListJson(bin);
            else if (bin.type == BinType::BallRecommendList)
                json = BuildBallRecommendListJson(bin);
            else if (bin.type == BinType::BallSettingList)
                json = BuildBallSettingListJson(bin);
            else if (bin.type == BinType::BattleCommonList)
                json = BuildBattleCommonListJson(bin);
            else if (bin.type == BinType::BattleCpuList)
                json = BuildBattleCpuListJson(bin);
            else if (bin.type == BinType::RankList)
                json = BuildRankListJson(bin);
            else if (bin.type == BinType::AssistInputList)
                json = BuildAssistInputListJson(bin);
            else
                continue;

            ZipEntry e;
            e.name = "fbsdata_mod/" + bin.name + ".json";
            e.data.assign(json.begin(), json.end());
            entries.push_back(std::move(e));
        }

        return WriteZip(path, entries);
    }

    bool LoadFromPath(const std::string& path, ModData& data)
    {
        const auto zipFiles = ReadZip(path);
        if (zipFiles.empty()) return false;

        ModData loaded;

        for (const auto& zf : zipFiles)
        {
            if (zf.name == "mod_info.json") continue;  // future: parse author/desc/ver

            // Expect "fbsdata_mod/<binname>.json"
            const std::string prefix = "fbsdata_mod/";
            if (zf.name.rfind(prefix, 0) != 0) continue;

            const std::string binJsonName = zf.name.substr(prefix.size());
            // Strip trailing ".json"
            if (binJsonName.size() < 6) continue;
            const std::string binName = binJsonName.substr(0, binJsonName.size() - 5);

            const std::string jsonStr(zf.data.begin(), zf.data.end());

            if (binName == "customize_item_common_list.bin")
            {
                ContentsBinData bin;
                bin.type = BinType::CustomizeItemCommonList;
                bin.name = binName;
                if (ParseCustomizeItemCommonJson(jsonStr, bin))
                {
                    loaded.selectedIndex = static_cast<int>(loaded.contents.size());
                    loaded.contents.push_back(std::move(bin));
                }
            }
            else if (binName == "character_list.bin")
            {
                ContentsBinData bin;
                bin.type = BinType::CharacterList;
                bin.name = binName;
                if (ParseCharacterListJson(jsonStr, bin))
                {
                    loaded.selectedIndex = static_cast<int>(loaded.contents.size());
                    loaded.contents.push_back(std::move(bin));
                }
            }
            else if (binName == "customize_item_exclusive_list.bin")
            {
                ContentsBinData bin;
                bin.type = BinType::CustomizeItemExclusiveList;
                bin.name = binName;
                ParseCustomizeItemExclusiveListJson(jsonStr, bin);
                loaded.selectedIndex = static_cast<int>(loaded.contents.size());
                loaded.contents.push_back(std::move(bin));
            }
            else if (binName == "area_list.bin")
            {
                ContentsBinData bin;
                bin.type = BinType::AreaList;
                bin.name = binName;
                ParseAreaListJson(jsonStr, bin);
                loaded.selectedIndex = static_cast<int>(loaded.contents.size());
                loaded.contents.push_back(std::move(bin));
            }
            else if (binName == "battle_subtitle_info.bin")
            {
                ContentsBinData bin;
                bin.type = BinType::BattleSubtitleInfoList;
                bin.name = binName;
                ParseBattleSubtitleInfoJson(jsonStr, bin);
                loaded.selectedIndex = static_cast<int>(loaded.contents.size());
                loaded.contents.push_back(std::move(bin));
            }
            else if (binName == "fate_drama_player_start_list.bin")
            {
                ContentsBinData bin;
                bin.type = BinType::FateDramaPlayerStartList;
                bin.name = binName;
                ParseFateDramaPlayerStartListJson(jsonStr, bin);
                loaded.selectedIndex = static_cast<int>(loaded.contents.size());
                loaded.contents.push_back(std::move(bin));
            }
            else if (binName == "jukebox_list.bin")
            {
                ContentsBinData bin;
                bin.type = BinType::JukeboxList;
                bin.name = binName;
                ParseJukeboxListJson(jsonStr, bin);
                loaded.selectedIndex = static_cast<int>(loaded.contents.size());
                loaded.contents.push_back(std::move(bin));
            }
            else if (binName == "series_list.bin")
            {
                ContentsBinData bin;
                bin.type = BinType::SeriesList;
                bin.name = binName;
                ParseSeriesListJson(jsonStr, bin);
                loaded.selectedIndex = static_cast<int>(loaded.contents.size());
                loaded.contents.push_back(std::move(bin));
            }
            else if (binName == "tam_mission_list.bin")
            {
                ContentsBinData bin;
                bin.type = BinType::TamMissionList;
                bin.name = binName;
                ParseTamMissionListJson(jsonStr, bin);
                loaded.selectedIndex = static_cast<int>(loaded.contents.size());
                loaded.contents.push_back(std::move(bin));
            }
            else if (binName == "drama_player_start_list.bin")
            {
                ContentsBinData bin;
                bin.type = BinType::DramaPlayerStartList;
                bin.name = binName;
                ParseDramaPlayerStartListJson(jsonStr, bin);
                loaded.selectedIndex = static_cast<int>(loaded.contents.size());
                loaded.contents.push_back(std::move(bin));
            }
            else if (binName == "stage_list.bin")
            {
                ContentsBinData bin;
                bin.type = BinType::StageList;
                bin.name = binName;
                ParseStageListJson(jsonStr, bin);
                loaded.selectedIndex = static_cast<int>(loaded.contents.size());
                loaded.contents.push_back(std::move(bin));
            }
            else if (binName == "ball_property_list.bin")
            {
                ContentsBinData bin;
                bin.type = BinType::BallPropertyList;
                bin.name = binName;
                ParseBallPropertyListJson(jsonStr, bin);
                loaded.selectedIndex = static_cast<int>(loaded.contents.size());
                loaded.contents.push_back(std::move(bin));
            }
            else if (binName == "body_cylinder_data_list.bin")
            {
                ContentsBinData bin;
                bin.type = BinType::BodyCylinderDataList;
                bin.name = binName;
                ParseBodyCylinderDataListJson(jsonStr, bin);
                loaded.selectedIndex = static_cast<int>(loaded.contents.size());
                loaded.contents.push_back(std::move(bin));
            }
            else if (binName == "customize_item_unique_list.bin")
            {
                ContentsBinData bin;
                bin.type = BinType::CustomizeItemUniqueList;
                bin.name = binName;
                ParseCustomizeItemUniqueListJson(jsonStr, bin);
                loaded.selectedIndex = static_cast<int>(loaded.contents.size());
                loaded.contents.push_back(std::move(bin));
            }
            else if (binName == "character_select_list.bin")
            {
                ContentsBinData bin;
                bin.type = BinType::CharacterSelectList;
                bin.name = binName;
                ParseCharacterSelectListJson(jsonStr, bin);
                loaded.selectedIndex = static_cast<int>(loaded.contents.size());
                loaded.contents.push_back(std::move(bin));
            }
            else if (binName == "customize_item_prohibit_drama_list.bin")
            {
                ContentsBinData bin;
                bin.type = BinType::CustomizeItemProhibitDramaList;
                bin.name = binName;
                ParseCustomizeItemProhibitDramaListJson(jsonStr, bin);
                loaded.selectedIndex = static_cast<int>(loaded.contents.size());
                loaded.contents.push_back(std::move(bin));
            }
            else if (binName == "battle_motion_list.bin")
            {
                ContentsBinData bin;
                bin.type = BinType::BattleMotionList;
                bin.name = binName;
                ParseBattleMotionListJson(jsonStr, bin);
                loaded.selectedIndex = static_cast<int>(loaded.contents.size());
                loaded.contents.push_back(std::move(bin));
            }
            else if (binName == "arcade_cpu_list.bin")
            {
                ContentsBinData bin;
                bin.type = BinType::ArcadeCpuList;
                bin.name = binName;
                ParseArcadeCpuListJson(jsonStr, bin);
                loaded.selectedIndex = static_cast<int>(loaded.contents.size());
                loaded.contents.push_back(std::move(bin));
            }
            else if (binName == "ball_recommend_list.bin")
            {
                ContentsBinData bin;
                bin.type = BinType::BallRecommendList;
                bin.name = binName;
                ParseBallRecommendListJson(jsonStr, bin);
                loaded.selectedIndex = static_cast<int>(loaded.contents.size());
                loaded.contents.push_back(std::move(bin));
            }
            else if (binName == "ball_setting_list.bin")
            {
                ContentsBinData bin;
                bin.type = BinType::BallSettingList;
                bin.name = binName;
                ParseBallSettingListJson(jsonStr, bin);
                loaded.selectedIndex = static_cast<int>(loaded.contents.size());
                loaded.contents.push_back(std::move(bin));
            }
            else if (binName == "battle_common_list.bin")
            {
                ContentsBinData bin;
                bin.type = BinType::BattleCommonList;
                bin.name = binName;
                ParseBattleCommonListJson(jsonStr, bin);
                loaded.selectedIndex = static_cast<int>(loaded.contents.size());
                loaded.contents.push_back(std::move(bin));
            }
            else if (binName == "battle_cpu_list.bin")
            {
                ContentsBinData bin;
                bin.type = BinType::BattleCpuList;
                bin.name = binName;
                ParseBattleCpuListJson(jsonStr, bin);
                loaded.selectedIndex = static_cast<int>(loaded.contents.size());
                loaded.contents.push_back(std::move(bin));
            }
            else if (binName == "rank_list.bin")
            {
                ContentsBinData bin;
                bin.type = BinType::RankList;
                bin.name = binName;
                ParseRankListJson(jsonStr, bin);
                loaded.selectedIndex = static_cast<int>(loaded.contents.size());
                loaded.contents.push_back(std::move(bin));
            }
            else if (binName == "assist_input_list.bin")
            {
                ContentsBinData bin;
                bin.type = BinType::AssistInputList;
                bin.name = binName;
                ParseAssistInputListJson(jsonStr, bin);
                loaded.selectedIndex = static_cast<int>(loaded.contents.size());
                loaded.contents.push_back(std::move(bin));
            }
            // Additional bin types will be handled here as they are implemented
        }

        if (loaded.contents.empty()) return false;

        data = std::move(loaded);
        return true;
    }

    bool LoadDialog(ModData& data)
    {
        const std::string path = OpenLoadDialog();
        if (path.empty()) return false;
        return LoadFromPath(path, data);
    }
}
