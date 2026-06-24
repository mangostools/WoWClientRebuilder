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
 * @file test_progress.cpp
 * @brief Unit tests for wcr::human_bytes and wcr::render_progress.
 *
 * NOTE: The libcurl progress callback (the CURLOPT_XFERINFOFUNCTION hook in
 * download.cpp) is intentionally not unit-tested here because it requires a
 * live transfer; it is covered by the user's manual smoke test of a large
 * download.
 */

#include "doctest.h"
#include "progress.h"
#include <string>

// ---------------------------------------------------------------------------
// human_bytes boundary cases
// ---------------------------------------------------------------------------

TEST_CASE("human_bytes: zero bytes")
{
    CHECK(wcr::human_bytes(0LL) == "0 B");
}

TEST_CASE("human_bytes: sub-KB values stay in bytes")
{
    CHECK(wcr::human_bytes(942LL) == "942 B");
    CHECK(wcr::human_bytes(1023LL) == "1023 B");
    CHECK(wcr::human_bytes(1LL) == "1 B");
}

TEST_CASE("human_bytes: KB boundary")
{
    // 1024 bytes == 1.00 KB
    CHECK(wcr::human_bytes(1024LL) == "1.00 KB");
    // 1536 bytes == 1.50 KB
    CHECK(wcr::human_bytes(1536LL) == "1.50 KB");
}

TEST_CASE("human_bytes: MB boundary")
{
    // 1 MB exactly == 1.00 MB
    CHECK(wcr::human_bytes(1024LL * 1024LL) == "1.00 MB");
}

TEST_CASE("human_bytes: GB - the 3.67 GB reference case")
{
    // 3943419041 bytes -> 3.67 GB (design spec example)
    CHECK(wcr::human_bytes(3943419041LL) == "3.67 GB");
}

TEST_CASE("human_bytes: unknown sentinel -1 returns question mark")
{
    CHECK(wcr::human_bytes(-1LL) == "?");
}

// ---------------------------------------------------------------------------
// render_progress
// ---------------------------------------------------------------------------

TEST_CASE("render_progress: total > 0 includes label, pct, and both sizes")
{
    // 512 of 1024 bytes = 50%
    std::string line = wcr::render_progress("foo.mpq", 512LL, 1024LL, -1LL);

    // Label present
    CHECK(line.find("foo.mpq") != std::string::npos);
    // Percentage present
    CHECK(line.find("50%") != std::string::npos);
    // Human sizes present
    CHECK(line.find("512 B") != std::string::npos);
    CHECK(line.find("1.00 KB") != std::string::npos);
    // Separator between now/total
    CHECK(line.find("/") != std::string::npos);
}

TEST_CASE("render_progress: total <= 0 shows label and bytes, no pct or slash")
{
    // Unknown total (-1 sentinel)
    std::string line = wcr::render_progress("bar.mpq", 2048LL, -1LL, -1LL);

    CHECK(line.find("bar.mpq") != std::string::npos);
    CHECK(line.find("2.00 KB") != std::string::npos);
    // No percentage when total unknown
    CHECK(line.find("%") == std::string::npos);
    // No "now / total" separator
    CHECK(line.find("/") == std::string::npos);
}

TEST_CASE("render_progress: zero total also no pct")
{
    std::string line = wcr::render_progress("test", 100LL, 0LL, -1LL);
    CHECK(line.find("test") != std::string::npos);
    CHECK(line.find("%") == std::string::npos);
}

// ---------------------------------------------------------------------------
// render_progress speed argument (4-arg overload)
// ---------------------------------------------------------------------------

TEST_CASE("render_progress: positive speed appends rate with /s suffix")
{
    // 13107200 bytes/s == 12.50 MB/s
    std::string line = wcr::render_progress(
        "foo.mpq", 512LL, 1024LL, 13107200LL);

    // Must still contain the label and progress
    CHECK(line.find("foo.mpq") != std::string::npos);
    CHECK(line.find("50%") != std::string::npos);
    // Speed suffix must appear
    CHECK(line.find("/s") != std::string::npos);
    // The human_bytes of 13107200 is "12.50 MB", so "MB/s" must be in there
    CHECK(line.find("MB/s") != std::string::npos);
}

TEST_CASE("render_progress: negative speed omits rate entirely")
{
    std::string line = wcr::render_progress("bar.mpq", 512LL, 1024LL, -1LL);

    CHECK(line.find("bar.mpq") != std::string::npos);
    // No speed suffix when bytesPerSec < 0
    CHECK(line.find("/s") == std::string::npos);
}

TEST_CASE("render_progress: zero speed shows 0 B/s")
{
    std::string line = wcr::render_progress("baz.mpq", 512LL, 1024LL, 0LL);

    CHECK(line.find("/s") != std::string::npos);
    CHECK(line.find("0 B/s") != std::string::npos);
}
