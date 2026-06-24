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
 * @file test_md5.cpp
 * @brief Unit tests for wcr::md5_hex() against RFC 1321 standard test vectors.
 */

#include "doctest.h"
#include "md5.h"
#include "bytes.h"
#include <cstdint>
#include <string>

// Helper: convert a string literal to a wcr::Bytes buffer.
static wcr::Bytes to_bytes(const char* s)
{
    wcr::Bytes b;
    for (; *s; ++s)
    {
        b.push_back(static_cast<uint8_t>(*s));
    }
    return b;
}

TEST_CASE("md5_hex RFC 1321 standard test vectors (UPPERCASE)")
{
    // RFC 1321 Appendix A.5 — all expected digests in UPPERCASE.

    // MD5 ("") = D41D8CD98F00B204E9800998ECF8427E
    CHECK(wcr::md5_hex({}) == "D41D8CD98F00B204E9800998ECF8427E");

    // MD5 ("abc") = 900150983CD24FB0D6963F7D28E17F72
    CHECK(wcr::md5_hex(to_bytes("abc")) == "900150983CD24FB0D6963F7D28E17F72");

    // MD5 ("message digest") = F96B697D7CB7938D525A2F31AAF161D0
    CHECK(wcr::md5_hex(to_bytes("message digest")) ==
          "F96B697D7CB7938D525A2F31AAF161D0");
}
