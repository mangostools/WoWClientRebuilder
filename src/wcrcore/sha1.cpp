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
 * @file sha1.cpp
 * @brief FIPS 180-derived public-domain SHA-1 implementation.
 *
 * Core algorithm per US Secure Hash Algorithm 1 (FIPS PUB 180-1).
 * Adapted for C++17 / wcr::Bytes by the WoWClientRebuilder project.
 */

#include "sha1.h"
#include <cstdint>
#include <cstring>

// ---------------------------------------------------------------------------
// Internal implementation
// ---------------------------------------------------------------------------
namespace
{

static inline uint32_t rotl32(uint32_t x, uint32_t n)
{
    return (x << n) | (x >> (32u - n));
}

static inline uint32_t be32(const uint8_t* p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

static void process_block(uint32_t state[5], const uint8_t block[64])
{
    uint32_t w[80];
    for (int i = 0; i < 16; ++i)
    {
        w[i] = be32(block + i * 4);
    }
    for (int i = 16; i < 80; ++i)
    {
        w[i] = rotl32(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
    }

    uint32_t a = state[0];
    uint32_t b = state[1];
    uint32_t c = state[2];
    uint32_t d = state[3];
    uint32_t e = state[4];

    for (int i = 0; i < 80; ++i)
    {
        uint32_t f;
        uint32_t k;
        if (i < 20)
        {
            f = (b & c) | (~b & d);
            k = 0x5A827999u;
        }
        else if (i < 40)
        {
            f = b ^ c ^ d;
            k = 0x6ED9EBA1u;
        }
        else if (i < 60)
        {
            f = (b & c) | (b & d) | (c & d);
            k = 0x8F1BBCDCu;
        }
        else
        {
            f = b ^ c ^ d;
            k = 0xCA62C1D6u;
        }
        uint32_t tmp = rotl32(a, 5) + f + e + k + w[i];
        e = d;
        d = c;
        c = rotl32(b, 30);
        b = a;
        a = tmp;
    }

    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
    state[4] += e;
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// Public interface
// ---------------------------------------------------------------------------
namespace wcr
{

std::array<unsigned char, 20> sha1(const Bytes& data)
{
    // Initial hash state (FIPS 180-1 §5).
    uint32_t state[5] = {0x67452301u, 0xEFCDAB89u, 0x98BADCFEu,
                         0x10325476u, 0xC3D2E1F0u};

    const uint8_t* ptr = data.data();
    uint64_t len = data.size();
    uint64_t processed = 0;

    // Process complete 64-byte blocks.
    while (processed + 64u <= len)
    {
        process_block(state, ptr + processed);
        processed += 64u;
    }

    // Padding: copy remainder, append 0x80, zero-pad, append big-endian
    // bit-length in the final 8 bytes.
    uint8_t tail[128] = {};
    uint64_t remainder = len - processed;
    std::memcpy(tail, ptr + processed, (size_t)remainder);
    tail[remainder] = 0x80u;

    uint64_t bit_len = len * 8u;
    size_t count_offset = (remainder < 56u) ? 56u : 120u;
    for (int i = 0; i < 8; ++i)
    {
        tail[count_offset + (size_t)i] =
            (uint8_t)(bit_len >> (8 * (7 - i)));
    }

    process_block(state, tail);
    if (remainder >= 56u)
    {
        process_block(state, tail + 64u);
    }

    // Serialise digest as 20 raw big-endian bytes.
    std::array<unsigned char, 20> out{};
    for (int i = 0; i < 5; ++i)
    {
        out[(size_t)(i * 4 + 0)] = (unsigned char)(state[i] >> 24);
        out[(size_t)(i * 4 + 1)] = (unsigned char)(state[i] >> 16);
        out[(size_t)(i * 4 + 2)] = (unsigned char)(state[i] >> 8);
        out[(size_t)(i * 4 + 3)] = (unsigned char)(state[i]);
    }
    return out;
}

} // namespace wcr
