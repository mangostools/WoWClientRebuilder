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
 * @file test_zip.cpp
 * @brief Unit tests for wcr::zip_extract_member().
 */

#include "doctest.h"
#include "zip.h"
#include "bytes.h"
#include "miniz.h"
#include <cstring>
#include <string>

/// Build a one-member zip in memory using miniz, for use as a test fixture.
static wcr::Bytes make_zip(const char* name, const char* content)
{
    mz_zip_archive ar;
    mz_zip_zero_struct(&ar);
    REQUIRE(mz_zip_writer_init_heap(&ar, 0, 0));
    REQUIRE(mz_zip_writer_add_mem(&ar, name, content, std::strlen(content),
                                  MZ_BEST_COMPRESSION));
    void* buf = nullptr;
    size_t sz = 0;
    REQUIRE(mz_zip_writer_finalize_heap_archive(&ar, &buf, &sz));
    wcr::Bytes z(static_cast<uint8_t*>(buf), static_cast<uint8_t*>(buf) + sz);
    mz_free(buf);
    mz_zip_writer_end(&ar);
    return z;
}

TEST_CASE("zip_extract_member returns a stored member's bytes")
{
    wcr::Bytes zip = make_zip("hello.txt", "WoW 4.3.4");
    wcr::Bytes out = wcr::zip_extract_member(zip, "hello.txt");
    std::string s(out.begin(), out.end());
    CHECK(s == "WoW 4.3.4");
}

TEST_CASE("zip_extract_member throws when the member is absent")
{
    wcr::Bytes zip = make_zip("hello.txt", "data");
    CHECK_THROWS_AS(wcr::zip_extract_member(zip, "missing.exe"),
                    std::runtime_error);
}
