/**
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2005-2026 MaNGOS <https://www.getmangos.eu>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * World of Warcraft, and all World of Warcraft or Warcraft art, images,
 * and lore are copyrighted by Blizzard Entertainment, Inc.
 */

/**
 * @file md5.cpp
 * @brief RFC 1321-derived public-domain MD5 implementation.
 *
 * Core algorithm by RSA Data Security, Inc. (public domain).
 * Adapted for C++17 / wcr::Bytes by the WoWClientRebuilder project.
 */

#include "md5.h"
#include <array>
#include <cstring>

// ---------------------------------------------------------------------------
// Internal implementation
// ---------------------------------------------------------------------------
namespace
{

// Per-round shift amounts (RFC 1321 §3.4)
static const uint32_t S[64] = {
    7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22,
    5, 9,  14, 20, 5, 9,  14, 20, 5, 9,  14, 20, 5, 9,  14, 20,
    4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23,
    6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21,
};

// Precomputed T[i] = floor(2^32 * |sin(i+1)|)  (RFC 1321 §3.4)
static const uint32_t K[64] = {
    0xd76aa478u, 0xe8c7b756u, 0x242070dbu, 0xc1bdceeeu, 0xf57c0fafu,
    0x4787c62au, 0xa8304613u, 0xfd469501u, 0x698098d8u, 0x8b44f7afu,
    0xffff5bb1u, 0x895cd7beu, 0x6b901122u, 0xfd987193u, 0xa679438eu,
    0x49b40821u, 0xf61e2562u, 0xc040b340u, 0x265e5a51u, 0xe9b6c7aau,
    0xd62f105du, 0x02441453u, 0xd8a1e681u, 0xe7d3fbc8u, 0x21e1cde6u,
    0xc33707d6u, 0xf4d50d87u, 0x455a14edu, 0xa9e3e905u, 0xfcefa3f8u,
    0x676f02d9u, 0x8d2a4c8au, 0xfffa3942u, 0x8771f681u, 0x6d9d6122u,
    0xfde5380cu, 0xa4beea44u, 0x4bdecfa9u, 0xf6bb4b60u, 0xbebfbc70u,
    0x289b7ec6u, 0xeaa127fau, 0xd4ef3085u, 0x04881d05u, 0xd9d4d039u,
    0xe6db99e5u, 0x1fa27cf8u, 0xc4ac5665u, 0xf4292244u, 0x432aff97u,
    0xab9423a7u, 0xfc93a039u, 0x655b59c3u, 0x8f0ccc92u, 0xffeff47du,
    0x85845dd1u, 0x6fa87e4fu, 0xfe2ce6e0u, 0xa3014314u, 0x4e0811a1u,
    0xf7537e82u, 0xbd3af235u, 0x2ad7d2bbu, 0xeb86d391u,
};

static inline uint32_t rotl32(uint32_t x, uint32_t n)
{
    return (x << n) | (x >> (32u - n));
}

static inline uint32_t le32(const uint8_t* p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static void process_block(uint32_t state[4], const uint8_t block[64])
{
    uint32_t M[16];
    for (int i = 0; i < 16; ++i)
    {
        M[i] = le32(block + i * 4);
    }

    uint32_t a = state[0];
    uint32_t b = state[1];
    uint32_t c = state[2];
    uint32_t d = state[3];

    for (int i = 0; i < 64; ++i)
    {
        uint32_t F;
        uint32_t g;
        if (i < 16)
        {
            F = (b & c) | (~b & d);
            g = (uint32_t)i;
        }
        else if (i < 32)
        {
            F = (d & b) | (~d & c);
            g = (5u * (uint32_t)i + 1u) % 16u;
        }
        else if (i < 48)
        {
            F = b ^ c ^ d;
            g = (3u * (uint32_t)i + 5u) % 16u;
        }
        else
        {
            F = c ^ (b | ~d);
            g = (7u * (uint32_t)i) % 16u;
        }
        uint32_t tmp = d;
        d = c;
        c = b;
        b = b + rotl32(a + F + K[i] + M[g], S[i]);
        a = tmp;
    }

    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// Public interface
// ---------------------------------------------------------------------------
namespace wcr
{

std::string md5_hex(const Bytes& data)
{
    // Initial hash state (RFC 1321 §3.3)
    uint32_t state[4] = {0x67452301u, 0xefcdab89u, 0x98badcfeu, 0x10325476u};

    const uint8_t* ptr = data.data();
    uint64_t len = data.size();
    uint64_t processed = 0;

    // Process complete 64-byte blocks
    while (processed + 64u <= len)
    {
        process_block(state, ptr + processed);
        processed += 64u;
    }

    // Padding: copy remainder, append 0x80, zero-pad, append bit-length
    uint8_t tail[128] = {};
    uint64_t remainder = len - processed;
    std::memcpy(tail, ptr + processed, (size_t)remainder);
    tail[remainder] = 0x80u;

    // Bit-count goes in bytes 56-63 of the last 64-byte block (little-endian)
    uint64_t bit_len = len * 8u;
    size_t count_offset = (remainder < 56u) ? 56u : 120u;
    for (int i = 0; i < 8; ++i)
    {
        tail[count_offset + (size_t)i] = (uint8_t)(bit_len >> (8 * i));
    }

    process_block(state, tail);
    if (remainder >= 56u)
    {
        process_block(state, tail + 64u);
    }

    // Serialise digest as UPPERCASE hex
    static const char HEX[] = "0123456789ABCDEF";
    std::string out;
    out.reserve(32);
    for (int i = 0; i < 4; ++i)
    {
        uint32_t w = state[i];
        for (int j = 0; j < 4; ++j)
        {
            uint8_t byte = (uint8_t)(w & 0xFFu);
            out += HEX[(byte >> 4) & 0xFu];
            out += HEX[byte & 0xFu];
            w >>= 8;
        }
    }
    return out;
}

} // namespace wcr
