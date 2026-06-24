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
 * @file test_sha1.cpp
 * @brief Unit tests for wcr::sha1() against known SHA-1 vectors.
 */

#include "doctest.h"
#include "sha1.h"
#include "bytes.h"
#include <array>
#include <cstdint>
#include <string>

// Helper: convert a 20-byte digest to a lowercase hex string.
static std::string to_hex(const std::array<unsigned char, 20>& d)
{
    static const char HEX[] = "0123456789abcdef";
    std::string out;
    out.reserve(40);
    for (unsigned char b : d)
    {
        out += HEX[(b >> 4) & 0xF];
        out += HEX[b & 0xF];
    }
    return out;
}

// Helper: convert a C string to a wcr::Bytes buffer (no terminator).
static wcr::Bytes to_bytes(const char* s)
{
    wcr::Bytes b;
    for (; *s; ++s)
    {
        b.push_back(static_cast<uint8_t>(*s));
    }
    return b;
}

TEST_CASE("sha1 known vectors (empty and abc)")
{
    // SHA1("") = da39a3ee5e6b4b0d3255bfef95601890afd80709
    CHECK(to_hex(wcr::sha1({})) ==
          "da39a3ee5e6b4b0d3255bfef95601890afd80709");

    // SHA1("abc") = a9993e364706816aba3e25717850c26c9cd0d89d
    CHECK(to_hex(wcr::sha1(to_bytes("abc"))) ==
          "a9993e364706816aba3e25717850c26c9cd0d89d");
}
