#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  KamuiHash.h
//  Tekken 8 "Kamui" string hash — extracted from game disassembly.
//  Source: AliK3112/tekken-8-movesets-scripts / cpp/hash.h
//
//  Public API:
//    int64_t KamuiHash::Compute(const char* str)
//    int64_t KamuiHash::Compute(const std::string& str)
// ─────────────────────────────────────────────────────────────────────────────
#include <cstdint>
#include <cstdlib>   // _byteswap_ulong (MSVC / Windows)
#include <string>

namespace KamuiHash {
namespace detail {

// Use kh_ prefix to avoid collisions with Windows SDK macros (ROL4, etc.)

static inline uint32_t kh_rol4(uint32_t value, int count)
{
    count %= 32;
    if (count == 0) return value;
    if (count < 0) count += 32;
    return (value << count) | (value >> (32 - count));
}

static inline uint32_t kh_ror4(uint32_t value, int count)
{
    return kh_rol4(value, -count);
}

// Read 3 bytes as: byte[0] | (byte[1] | (uint16[+2] << 8)) << 8
// Equivalent to: b[0] | b[1]<<8 | (*(uint16*)(b+2))<<16
static inline uint32_t kh_rd3(const uint8_t* p)
{
    uint16_t w;
    memcpy(&w, p + 2, 2);
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)w << 16);
}

static inline uint32_t kh_finalize(uint32_t x)
{
    uint32_t tmp = 5u * (x - 0x52250ECu);
    uint32_t a   = 0x85EBCA6Bu * (tmp ^ (tmp >> 16));
    uint32_t b   = 0xC2B2AE35u * (a ^ (a >> 13));
    return b ^ (b >> 16);
}

static int64_t Hash12To24(const uint8_t* a1, uint32_t length)
{
    const uint8_t* v2 = a1 + length;
    const uint8_t* v3 = a1 + ((uint64_t)length >> 1);

    // Unrolled from the original deeply nested ROL expression (6 levels deep)
    uint32_t h0 = length ^ (0x1B873593u * kh_rol4(0xCC9E2D51u * kh_rd3(a1 + (length >> 1) - 4), 15));
    uint32_t h1 = 5u * (kh_rol4(h0, 13) - 0x52250ECu);

    uint32_t h2 = (0x1B873593u * kh_rol4(0xCC9E2D51u * kh_rd3(a1 + 4), 15)) ^ h1;
    uint32_t h3 = 5u * kh_rol4(h2, 13) - 0x19AB949Cu;

    uint32_t h4 = (0x1B873593u * kh_rol4(0xCC9E2D51u * kh_rd3(v2 - 8), 15)) ^ h3;
    uint32_t h5 = 5u * (kh_rol4(h4, 13) - 0x52250ECu);

    uint32_t h6 = (0x1B873593u * kh_rol4(0xCC9E2D51u * kh_rd3(v3), 15)) ^ h5;
    uint32_t h7 = 5u * kh_rol4(h6, 13) - 0x19AB949Cu;

    uint32_t h8 = (0x1B873593u * kh_rol4(0xCC9E2D51u * kh_rd3(a1), 15)) ^ h7;
    uint32_t h9 = 5u * (kh_rol4(h8, 13) - 0x52250ECu);

    uint32_t h10 = (0x1B873593u * kh_rol4(0xCC9E2D51u * kh_rd3(v2 - 4), 15)) ^ h9;
    int v4 = (int)kh_rol4(h10, 13);

    uint32_t tmp = 5u * ((uint32_t)v4 - 0x52250ECu);
    uint32_t fa  = 0x85EBCA6Bu * (tmp ^ (tmp >> 16));
    uint32_t fb  = 0xC2B2AE35u * (fa ^ (fa >> 13));
    return (int64_t)(uint32_t)(fb ^ (fb >> 16));
}

static int64_t Hash(const uint8_t* a1, uint64_t length)
{
    if (length > 24)
    {
        uint32_t len32 = (uint32_t)length;

        int v14 = (int)(5u * (kh_rol4(
            (0x1B873593u * kh_rol4(0xCC9E2D51u * kh_rd3(a1 + length - 16), 15)) ^
            (5u * (kh_rol4(len32 ^ (0x1B873593u * kh_rol4(0xCC9E2D51u * kh_rd3(a1 + length - 4), 15)), 13) -
            0x52250ECu)), 13) - 0x52250ECu));

        int v15 = (int)(5u * (kh_rol4(
            (0x1B873593u * kh_rol4(0xCC9E2D51u * kh_rd3(a1 + length - 12), 15)) ^
            (5u * (kh_rol4((0xCC9E2D51u * len32) ^ (461845907u * kh_rol4(0xCC9E2D51u * kh_rd3(a1 + length - 8), 15)), 13) -
            0x52250ECu)), 13) - 0x52250ECu));

        int v16 = (int)(5u * (kh_rol4(
            0xCC9E2D51u * len32 + 461845907u * kh_rol4(0xCC9E2D51u * kh_rd3(a1 + length - 20), 15), 13) -
            0x52250ECu));

        uint32_t counter = (uint32_t)((length - 1) / 20);
        const uint8_t* v18 = a1 + 6;
        int lastV23 = 0;
        uint32_t lastV24 = 0;

        do {
            int v19 = (int)(0x1B873593u * kh_rol4(0xCC9E2D51u * kh_rd3(v18 - 6), 15));
            int v20 = (int)kh_rd3(v18 - 2);
            int v21 = (int)kh_rol4((uint32_t)(v14 ^ v19), 14);
            int v22 = (int)kh_rd3(v18 + 10);
            int v23 = (int)((uint32_t)v19 - 0x3361D2AFu * kh_rol4((uint32_t)v20 + (uint32_t)v16, 13));
            lastV23  = v23;
            v14      = v23;
            uint32_t v24 = _byteswap_ulong(5u * ((uint32_t)v22 + kh_rol4(
                (5u * ((uint32_t)v21 - 0x52250ECu)) ^
                ((uint32_t)v20 + 0x1B873593u * kh_rol4(0xCC9E2D51u * kh_rd3(v18 + 6), 15)), 13) - 0x52250ECu));
            lastV24  = v24;
            int v25  = (int)kh_rol4((uint32_t)v15 + 0x1B873593u * kh_rol4(0xCC9E2D51u * kh_rd3(v18 + 2), 15), 14);
            v18     += 20;
            v15      = (int)v24;
            v16      = (int)(5u * _byteswap_ulong((uint32_t)v22 ^ (5u * ((uint32_t)v25 - 0x52250ECu))));
            --counter;
        } while (counter);

        return (int64_t)(uint32_t)(0xCC9E2D51u * kh_rol4(5u * kh_rol4(0xCC9E2D51u *
            (kh_rol4(0xCC9E2D51u * kh_ror4((uint32_t)v16, 11), 15) + kh_rol4(5u *
            (kh_rol4((uint32_t)lastV23 - 0x3361D2AFu * kh_rol4(0xCC9E2D51u * kh_ror4(lastV24, 11), 15), 13) -
            0x52250ECu), 15)), 13) - 0x19AB949Cu, 15));
    }
    else if (length > 12)
    {
        return Hash12To24(a1, (uint32_t)length);
    }
    else if (length > 4)
    {
        const uint8_t* v11 = a1 + (uint32_t)length;
        uint32_t len32 = (uint32_t)length;

        uint32_t word4 = (uint32_t)a1[0]
                       | ((uint32_t)a1[1] << 8)
                       | ((uint32_t)a1[2] << 16)
                       | ((uint32_t)a1[3] << 24);

        int v12 = (int)kh_rol4(
            (5u * (kh_rol4(
                (5u * (kh_rol4(
                    (5u * len32) ^ (0x1B873593u * kh_rol4(0xCC9E2D51u * (len32 + word4), 15)), 13) -
                0x52250ECu)) ^ (0x1B873593u * kh_rol4(0xCC9E2D51u *
                (5u * len32 + kh_rd3(v11 - 4)), 15)), 13) -
                0x52250ECu)) ^ (0x1B873593u * kh_rol4(0x318F97D9u - 0x3361D2AFu *
                kh_rd3(a1 + ((len32 >> 1) & 4)), 15)), 13);

        return (int64_t)kh_finalize((uint32_t)v12);
    }
    else
    {
        int v4 = 0;
        int v5 = 9;
        const uint8_t* v3 = a1;
        uint32_t rem = (uint32_t)length;

        if (rem) {
            do {
                int v6 = (int)(0x3361D2AFu * (uint32_t)v4);
                int v7 = (int)(int8_t)*v3++;
                v4 = v7 - v6;
                v5 ^= v4;
                --rem;
            } while (rem);
        }

        int v8 = (int)kh_rol4(
            (5u * (kh_rol4((0x1B873593u * kh_rol4(0xCC9E2D51u * (uint32_t)(int)length, 15)) ^ (uint32_t)v5, 13) -
            0x52250ECu)) ^ (0x1B873593u * kh_rol4(0xCC9E2D51u * (uint32_t)v4, 15)), 13);

        return (int64_t)kh_finalize((uint32_t)v8);
    }
}

} // namespace detail

// ── Public API ───────────────────────────────────────────────────────────────

inline int64_t Compute(const char* str, uint64_t length)
{
    return detail::Hash(reinterpret_cast<const uint8_t*>(str), length);
}

inline int64_t Compute(const char* str)
{
    uint64_t len = 0;
    while (str[len]) ++len;
    return detail::Hash(reinterpret_cast<const uint8_t*>(str), len);
}

inline int64_t Compute(const std::string& str)
{
    return detail::Hash(reinterpret_cast<const uint8_t*>(str.c_str()), str.size());
}

} // namespace KamuiHash
