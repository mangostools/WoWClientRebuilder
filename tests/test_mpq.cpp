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
 * @file test_mpq.cpp
 * @brief Unit tests for MpqArchive: create a scratch MPQ, then list and
 *        extract a known file to verify round-trip correctness.
 */

#include "doctest.h"
#include "mpq.h"
#include "StormLib.h"
#include <string>
TEST_CASE("MpqArchive lists and extracts a created file")
{
    std::string path = "test_tmp.mpq";
    remove(path.c_str());
    HANDLE h = nullptr;
    REQUIRE(SFileCreateArchive(path.c_str(), MPQ_CREATE_ATTRIBUTES, 16, &h));
    const char* payload = "hello-mpq";
    HANDLE hf = nullptr;
    REQUIRE(SFileCreateFile(h, "dir\\hello.txt", 0, (DWORD)strlen(payload), 0,
                            MPQ_FILE_COMPRESS, &hf));
    REQUIRE(SFileWriteFile(hf, payload, (DWORD)strlen(payload),
                           MPQ_COMPRESSION_ZLIB));
    SFileFinishFile(hf);
    SFileCloseArchive(h);

    {
        wcr::MpqArchive a(path);
        CHECK(a.contains("dir\\hello.txt"));
        wcr::Bytes got = a.extract("dir\\hello.txt");
        CHECK(std::string(got.begin(), got.end()) == "hello-mpq");
    }
    // The archive must be closed (destructor calls SFileCloseArchive) before
    // remove(): Windows refuses to delete a file StormLib still holds open, so
    // an in-scope archive here would silently leak test_tmp.mpq into the tree.
    remove(path.c_str());
}
