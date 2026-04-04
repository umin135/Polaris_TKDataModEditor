// TkdataExtractor.cpp
// Selective file extraction from tkdata.bin (Tekken 8 VFS archive).
// Mirrors the Python reference in _references/tkdata/tkdata.py.
#include "TkdataExtractor.h"
#include "TkdataNames.h"
#include <cstring>
#include <windows.h>
#include <bcrypt.h>
#pragma comment(lib, "Bcrypt.lib")

// Forward-declare the zstd decoder functions we use.
// This avoids pulling in zstd.h (which depends on zstd_errors.h).
// The symbols are provided by third-party/zstd/zstddeclib-in.c.
extern "C" {
    size_t        ZSTD_decompress(void* dst, size_t dstCap,
                                  const void* src, size_t srcSize);
    unsigned long long ZSTD_getFrameContentSize(const void* src, size_t srcSize);
    unsigned      ZSTD_isError(size_t code);
    const char*   ZSTD_getErrorName(size_t code);
}
static const unsigned long long kZstdContentSizeError   = (0ULL - 2);
static const unsigned long long kZstdContentSizeUnknown = (0ULL - 1);

// ------------------------------------------------------------------
//  Crypto constants (from tkdata.py)
// ------------------------------------------------------------------

static const uint32_t kXorKeys[16] = {
    0xADFB76C8u, 0x7DEF1F1Cu, 0xA84B71E0u, 0xD88448A2u,
    0x964F5B9Eu, 0x7EE2BC2Cu, 0xA27D5221u, 0xBB9FE67Du,
    0xAD15269Fu, 0xEC1A9785u, 0x9BAE2F45u, 0xA4296896u,
    0x275AA004u, 0x37E22F31u, 0x3803D4A7u, 0x9B81329Fu,
};

// 1024-char key pool (KEY_POOL_STR from tkdata.py)
static const char kKeyPool[] =
    "U%ZPP0S2y4WoIofdsjM@2a%LJr@#*Ir!u8?FTSD2T%YcYsYl3I0W8=tSiI9.EOTQJ4?"
    "09hmYC3L5FL0uzRHN2?JF9IX9WI7t?UAQ4FujsC3XH+RkqIGJ%k19$OJS&.AKh$@LFT1"
    "C83nXrjJ0MMcZVWF0Ln!SfhwzSKoWDjQj7c!PXVEYOKD@+BUZGhY5@#RgW+Gw0sKw63Q"
    "BjAp$$QOEFB%$V5?jcS@Q+hfg!=DrtmmALG0G@UXXEaVr17bnLrDnovHG.%PfIyB!8W9n"
    "&#xAywcKo6te3DAYi$KSCCz8bXQiN331x77uw5IJVq.BUfckF4sEBp%XV+e6rFg*V.ZS40"
    "#supW8T3ZB4e892MpxUMPMxr8d4LCltyVlJHBWKjK2W4SlynsF1CK5CxAkwLd6?ta.W6c"
    "W4IrQ1f@x5tUvpfPxOOyao6kEjfaPKGsBYB=Y6qQwQW3l3oGtV5zEH=&EgBL8h7Zz5sm3"
    "L&HsrFLS8CKoh3lR.XBAWXnB%S*O5&q?H9zjOqqr#JqwX3TU%2hYHTJ&2dY*fWIrPdc!P"
    "$hPlzFZUQPkgy6+Pg5C8f#bYYXua9gDn484!XO=5BuHI2FX$RR8%&rX%c@6u8EFBYXLhi"
    "0BNW!w7d+Z=iOabHqZZXSQ6Db*sncGALQFLgm%lvHfKhMP5.78Gm0Dd?nFE9zsTBIF=f6"
    "VBtD=*1PWxmnthUSjGQ&tf@9l$CAJR87UM4I$2oa@8ncr48dDMyMuM7KZX4@B.Ny6dObQe"
    "SdJ2=8jkV61GvIBOHAbbUiWfDCu52&MRs=@Bz#HHUMoQ9WFm5TxKWdoHlDEOW$!4$z==V"
    "G9PkfxPIT4.y*737zPZ9V4?JG.rl7kHK$k44$hdf.L*QX.=wp&JbpULUm#GI2vJv*lPJ$"
    "2B7M9Gg.+.O6tgUZvU4HMV2&7gvcBHDA.FG9jzC@s5*6XW8mUHB6j$*tz$OVPYDjW@07a"
    "pt&poihk.I69&*d%y=m%@XP1ERk+bTP0v5iHd?xQ9C%1O2t=Tj7.xpgABlCyee1i1hPVz"
    "egGXSQD1Fp1lQd*IxwpA%HtBmuG6c$#43S?HK*Q*!Tj0kAU&RTbDoRfW=!MwGiHM2KMMN"
    "NBK=31f#83gt%NXAzix3D!6EQ5O3NSZKvQYvSKuvUSu1gGAATF1MqWG7USVjG9NAjz1&?K"
    "W0dTMaCJfD!Luym?WWLN3nf9#UcDUvL02m7L2$zJA=dbNQ2N5kVybBg7JE3ByS3AaZO=u"
    "@ty2g*dPrYW7UN&pqA1g*rQ+7%PM3XTsLcH=B+BFRIDPTsLj!+=68T+%vIMaRzFoiC4rJ"
    "A#uevTsalLrpO.WfY2SG%%qbKihqRXg2=IJwXKZ+xoYUydgLLh9eRt+Df0&5&C3T7i9sj"
    ".Z2TEUCxaNl2T8d@=Muw$S955+iHs=KCt?LZNFy*#%UU8gOHl*I9lQUXH4#cVYy9JY@gi"
    "lIQ?v.TdkUBN9gHob*OVicLITzNqQpGL6wvupDkVqTlAORJTgL5RMKqQYpSD8LiYUp#aW"
    "KgFCBs1hCYTzFwa$*cllNQW.M2GSSI+x#qa#JcK7Cwh.v78hI1&T3c#DR&VDazL.XL2KG"
    "USSBKmcc31R%JCa9736gp=QV5#I7%BGEdJkOs7QhKxt0n3sS4#sAl1BGNNXPx&9PoPx8.G"
    "dv=eDotAfJmKuSUAirWB0XUe4FG9PYTgR??$t2IKh4+LjNCZ8TkXGqh=7rN4B86TZ3gjJ"
    "6QY#sT?FIc?Z*IstFG43YZqw%BwC&Ml2p7LB@JH!YW!zvNJYKjgaNm1!%sf$ZQbms4Akc"
    "Ojs2Bh";

// ------------------------------------------------------------------
//  Helpers
// ------------------------------------------------------------------

static void GetAesKey(uint8_t seed, uint8_t outKey[32])
{
    // Python: off = (44983 * seed) % (len(s) - 32)
    static const size_t kPoolLen = sizeof(kKeyPool) - 1; // exclude null terminator
    size_t off = (static_cast<size_t>(44983) * seed) % (kPoolLen - 32);
    memcpy(outKey, kKeyPool + off, 32);
}

static void XorDecryptFooter(uint8_t buf[128])
{
    for (int i = 0; i < 16; ++i) {
        buf[i * 4 + 0] ^= static_cast<uint8_t>((kXorKeys[i] >>  0) & 0xFF);
        buf[i * 4 + 1] ^= static_cast<uint8_t>((kXorKeys[i] >>  8) & 0xFF);
        buf[i * 4 + 2] ^= static_cast<uint8_t>((kXorKeys[i] >> 16) & 0xFF);
        buf[i * 4 + 3] ^= static_cast<uint8_t>((kXorKeys[i] >> 24) & 0xFF);
    }
}

// AES-256-ECB decryption using Windows BCrypt (no external DLL).
// data is decrypted in-place. data.size() must be a multiple of 16.
// Returns false on failure; data remains unchanged.
static bool AesEcbDecrypt(const uint8_t key[32], std::vector<uint8_t>& data)
{
    if (data.empty() || data.size() % 16 != 0)
        return false;

    BCRYPT_ALG_HANDLE hAlg = nullptr;
    if (!BCRYPT_SUCCESS(BCryptOpenAlgorithmProvider(
            &hAlg, BCRYPT_AES_ALGORITHM, nullptr, 0)))
        return false;

    BCryptSetProperty(hAlg, BCRYPT_CHAINING_MODE,
        reinterpret_cast<PUCHAR>(const_cast<wchar_t*>(BCRYPT_CHAIN_MODE_ECB)),
        sizeof(BCRYPT_CHAIN_MODE_ECB), 0);

    DWORD keyObjSize = 0, result = 0;
    BCryptGetProperty(hAlg, BCRYPT_OBJECT_LENGTH,
        reinterpret_cast<PUCHAR>(&keyObjSize), sizeof(DWORD), &result, 0);

    std::vector<uint8_t> keyObj(keyObjSize);
    BCRYPT_KEY_HANDLE hKey = nullptr;
    if (!BCRYPT_SUCCESS(BCryptGenerateSymmetricKey(
            hAlg, &hKey, keyObj.data(), keyObjSize,
            const_cast<PUCHAR>(key), 32, 0))) {
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return false;
    }

    // Decrypt without padding flag -- raw ECB, caller manages padding
    ULONG outSize = 0;
    NTSTATUS st = BCryptDecrypt(hKey,
        data.data(), static_cast<ULONG>(data.size()),
        nullptr, nullptr, 0,
        data.data(), static_cast<ULONG>(data.size()),
        &outSize, 0);

    BCryptDestroyKey(hKey);
    BCryptCloseAlgorithmProvider(hAlg, 0);
    return BCRYPT_SUCCESS(st);
}

static constexpr uint8_t kZstdMagic[4] = { 0x28, 0xB5, 0x2F, 0xFD };

static bool LooksLikeZstd(const uint8_t* buf, size_t len)
{
    return len >= 4 && memcmp(buf, kZstdMagic, 4) == 0;
}

// Decompress 'compLen' bytes from src into a new vector.
// Uses ZSTD_getFrameContentSize to pre-allocate output.
static bool ZstdDecompress(const uint8_t* src, size_t compLen,
                            std::vector<uint8_t>& out)
{
    unsigned long long decompSize =
        ZSTD_getFrameContentSize(src, compLen);
    if (decompSize == kZstdContentSizeError)
        return false;

    if (decompSize == kZstdContentSizeUnknown) {
        // Streaming fallback for frames without content size
        // Allocate a generous output buffer
        decompSize = compLen * 8;
    }

    out.resize(static_cast<size_t>(decompSize));
    size_t actual = ZSTD_decompress(out.data(), out.size(), src, compLen);
    if (ZSTD_isError(actual))
        return false;
    out.resize(actual);
    return true;
}

// Pad data to a multiple of 16 (AES block size) if needed.
static void PadToBlockSize(std::vector<uint8_t>& data)
{
    size_t rem = data.size() % 16;
    if (rem != 0)
        data.resize(data.size() + (16 - rem), 0);
}

// Strip PKCS7 padding after AES-ECB decryption (mirrors Python's unpad()).
// If the trailing bytes don't look like valid PKCS7, leaves data unchanged.
static void StripPkcs7Padding(std::vector<uint8_t>& data)
{
    if (data.empty()) return;
    uint8_t pad = data.back();
    if (pad < 1 || pad > 16 || static_cast<size_t>(pad) > data.size()) return;
    for (size_t i = data.size() - pad; i < data.size(); ++i) {
        if (data[i] != pad) return; // not valid PKCS7, leave as-is
    }
    data.resize(data.size() - pad);
}

// ------------------------------------------------------------------
//  Read helpers (little-endian)
// ------------------------------------------------------------------

template<typename T>
static T ReadLE(const uint8_t* p) {
    T v = 0;
    memcpy(&v, p, sizeof(T));
    return v;
}

// ------------------------------------------------------------------
//  TkdataExtractor::Open
// ------------------------------------------------------------------

static std::string ResolveTkdataPath(const std::string& tkdataPath)
{
    // tkdataPath is already the full resolved path passed by the caller.
    return tkdataPath;
}

bool TkdataExtractor::Open(const std::string& path, std::string& errorMsg)
{
    Close();

    fopen_s(&m_file, path.c_str(), "rb");
    if (!m_file) {
        errorMsg = "tkdata.bin not found: " + path;
        return false;
    }

    // Get file size
    _fseeki64(m_file, 0, SEEK_END);
    m_fileSize = static_cast<uint64_t>(_ftelli64(m_file));

    if (m_fileSize < 128 + 16) {
        errorMsg = "tkdata.bin too small.";
        Close();
        return false;
    }

    // Verify header magic
    _fseeki64(m_file, 0, SEEK_SET);
    char magic[16] = {};
    fread(magic, 1, 16, m_file);
    if (memcmp(magic, "__TEKKEN8FILES__", 16) != 0) {
        errorMsg = "tkdata.bin: bad header magic.";
        Close();
        return false;
    }

    return ParseToc(errorMsg);
}

// ------------------------------------------------------------------
//  TkdataExtractor::ParseToc
// ------------------------------------------------------------------

bool TkdataExtractor::ParseToc(std::string& errorMsg)
{
    // 1. Read and XOR-decrypt the 128-byte footer
    uint8_t footer[128] = {};
    _fseeki64(m_file, static_cast<int64_t>(m_fileSize) - 128, SEEK_SET);
    if (fread(footer, 1, 128, m_file) != 128) {
        errorMsg = "tkdata.bin: failed to read footer.";
        return false;
    }
    XorDecryptFooter(footer);

    // 2. Read TOC metadata from footer
    uint8_t  aesI   = footer[0x11];
    int32_t  tocOff = ReadLE<int32_t>(footer + 0x18);
    int32_t  tocSz  = ReadLE<int32_t>(footer + 0x20);
    int32_t  tocUs  = ReadLE<int32_t>(footer + 0x28); // uncompressed TOC size

    if (tocOff <= 0 || tocSz <= 0 || tocUs <= 0) {
        errorMsg = "tkdata.bin: invalid TOC metadata in footer.";
        return false;
    }

    // 3. Read TOC (encrypted + compressed)
    std::vector<uint8_t> toc(static_cast<size_t>(tocSz));
    _fseeki64(m_file, static_cast<int64_t>(tocOff), SEEK_SET);
    if (fread(toc.data(), 1, toc.size(), m_file) != toc.size()) {
        errorMsg = "tkdata.bin: failed to read TOC.";
        return false;
    }

    // 4. AES-decrypt TOC if aes_i != 0
    if (aesI != 0) {
        uint8_t key[32];
        GetAesKey(aesI, key);
        PadToBlockSize(toc); // should already be aligned, but guard anyway
        if (!AesEcbDecrypt(key, toc)) {
            errorMsg = "tkdata.bin: TOC AES decryption failed.";
            return false;
        }
        StripPkcs7Padding(toc); // remove PKCS7 padding (mirrors Python unpad())
    }

    // 5. Zstd-decompress TOC
    if (!LooksLikeZstd(toc.data(), toc.size())) {
        errorMsg = "tkdata.bin: TOC is not a zstd stream.";
        return false;
    }

    // Use ZSTD_getFrameContentSize to determine output buffer size.
    // tocUs from the footer is used as a fallback if the frame has no content size.
    unsigned long long frameSize = ZSTD_getFrameContentSize(toc.data(), toc.size());
    size_t decompCapacity;
    if (frameSize == kZstdContentSizeError) {
        errorMsg = "tkdata.bin: TOC zstd frame is corrupt.";
        return false;
    } else if (frameSize == kZstdContentSizeUnknown) {
        // No content size in frame header -- use tocUs as declared capacity
        decompCapacity = static_cast<size_t>(tocUs);
    } else {
        decompCapacity = static_cast<size_t>(frameSize);
    }

    std::vector<uint8_t> tocDecomp(decompCapacity);
    size_t actual = ZSTD_decompress(
        tocDecomp.data(), tocDecomp.size(),
        toc.data(), toc.size());
    if (ZSTD_isError(actual)) {
        char buf[256];
        snprintf(buf, sizeof(buf),
                 "TOC zstd: %s (aesI=%u tocSz=%d tocUs=%d frameSize=%llu actual=%zu)",
                 ZSTD_getErrorName(actual), aesI, tocSz, tocUs, frameSize, actual);
        errorMsg = buf;
        return false;
    }
    tocDecomp.resize(actual);

    // 6. Parse TOC entries
    // Header: [0x00-0x07] reversed "BNBinLst" sig (not verified here)
    //         [0x08-0x0B] count (int32)
    //         [0x10-0x13] start offset (int32)
    if (tocDecomp.size() < 0x14) {
        errorMsg = "tkdata.bin: TOC too short.";
        return false;
    }

    int32_t count = ReadLE<int32_t>(tocDecomp.data() + 0x08);
    int32_t start = ReadLE<int32_t>(tocDecomp.data() + 0x10);

    if (count <= 0 || start < 0 ||
        static_cast<size_t>(start) + static_cast<size_t>(count) * 64 > tocDecomp.size()) {
        errorMsg = "tkdata.bin: TOC entry range out of bounds.";
        return false;
    }

    m_entries.reserve(static_cast<size_t>(count));

    for (int32_t i = 0; i < count; ++i) {
        const uint8_t* e = tocDecomp.data() + start + i * 64;
        TocEntry entry;
        entry.flag       = e[8];
        entry.key        = e[9];
        entry.fileOffset = ReadLE<uint64_t>(e + 24);
        entry.fileSize   = ReadLE<uint64_t>(e + 32);
        entry.size2      = ReadLE<uint64_t>(e + 40);
        uint64_t hash    = ReadLE<uint64_t>(e + 16);
        m_entries[hash]  = entry;
    }

    return true;
}

// ------------------------------------------------------------------
//  TkdataExtractor::Close
// ------------------------------------------------------------------

void TkdataExtractor::Close()
{
    if (m_file) {
        fclose(m_file);
        m_file = nullptr;
    }
    m_entries.clear();
    m_fileSize = 0;
}

// ------------------------------------------------------------------
//  TkdataExtractor::ExtractFile
// ------------------------------------------------------------------

std::vector<uint8_t> TkdataExtractor::ExtractFile(const std::string& vfsPath) const
{
    if (!m_file)
        return {};

    // Resolve hash from embedded name table
    uint64_t hash = TkdataNameLookup(vfsPath.c_str());
    if (hash == 0)
        return {}; // VFS path not in our table -> game updated or unknown file

    auto it = m_entries.find(hash);
    if (it == m_entries.end())
        return {}; // hash not in tkdata.bin TOC -> graceful skip

    const TocEntry& entry = it->second;

    if (entry.fileSize == 0)
        return {};

    // Read encrypted data
    // File data offset: 0x10 + entry.fileOffset (per tkdata.py)
    uint64_t fileOff = 0x10 + entry.fileOffset;
    if (fileOff + entry.fileSize > m_fileSize)
        return {};

    std::vector<uint8_t> data(static_cast<size_t>(entry.fileSize));
    _fseeki64(m_file, static_cast<int64_t>(fileOff), SEEK_SET);
    if (fread(data.data(), 1, data.size(), m_file) != data.size())
        return {};

    // AES-decrypt
    uint8_t key[32];
    GetAesKey(entry.key, key);
    PadToBlockSize(data); // guard against misalignment
    if (!AesEcbDecrypt(key, data))
        return {};
    StripPkcs7Padding(data); // remove PKCS7 padding (mirrors Python unpad())

    // Zstd-decompress if the decrypted payload starts with zstd magic
    if (LooksLikeZstd(data.data(), data.size())) {
        // When flag == 0: only the first entry.size2 bytes are the zstd stream
        // When flag != 0: the entire decrypted buffer is the zstd stream
        size_t compLen = (entry.flag == 0)
            ? (entry.size2 < static_cast<uint64_t>(data.size())
                   ? static_cast<size_t>(entry.size2)
                   : data.size())
            : data.size();

        std::vector<uint8_t> decompressed;
        if (!ZstdDecompress(data.data(), compLen, decompressed))
            return {};
        return decompressed;
    }

    // Not compressed -- strip AES padding if possible
    // Trim trailing zero bytes that are AES block padding
    if (entry.size2 > 0 && entry.size2 < data.size())
        data.resize(static_cast<size_t>(entry.size2));

    return data;
}
