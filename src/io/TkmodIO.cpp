// .tkmod file I/O
// Format: uncompressed ZIP containing:
//   mod_info.json
//   fbsdata_mod/<bin_name>.json   (one per added bin)

#include "TkmodIO.h"
#include <windows.h>
#include <commdlg.h>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <cstdint>

#pragma comment(lib, "comdlg32.lib")

// ?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?//  CRC-32 (required by ZIP format)
// ?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?
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

// ?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?//  Uncompressed ZIP writer
// ?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?
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

// ?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?//  Uncompressed ZIP reader
// ?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?
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

// ?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?//  JSON builder helpers
// ?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?
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

// ?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?//  JSON parser helpers (minimal, for known structures)
// ?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?
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

// ─────────────────────────────────────────────────────────────────────────────
//  character_list JSON builder
// ─────────────────────────────────────────────────────────────────────────────

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

// ─────────────────────────────────────────────────────────────────────────────
//  customize_item_exclusive_list JSON builder
// ─────────────────────────────────────────────────────────────────────────────

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

// ─────────────────────────────────────────────────────────────────────────────
//  character_list JSON parser
// ─────────────────────────────────────────────────────────────────────────────

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

// ─────────────────────────────────────────────────────────────────────────────
//  customize_item_exclusive_list JSON parser
// ─────────────────────────────────────────────────────────────────────────────

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

// ?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?//  Win32 file dialogs + wide/narrow conversion
// ?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?
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

// ?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?//  Public API
// ?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?
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
