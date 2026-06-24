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
 * @file test_bencode.cpp
 * @brief Unit tests for wcr::parse_torrent() on a synthetic .tfil.
 */

#include "doctest.h"
#include "bencode.h"
#include "sha1.h"
#include "bytes.h"
#include <array>
#include <string>

// Helper: turn a std::string into a wcr::Bytes buffer (raw bytes).
static wcr::Bytes str_bytes(const std::string& s)
{
    return wcr::Bytes(s.begin(), s.end());
}

TEST_CASE("parse_torrent reads piece length, files, direct download, info hash")
{
    // Synthetic torrent. Two real files + one "alignment" pad entry.
    // pieces = exactly two 20-byte SHA-1 blocks (we use 40 'A's as a stand-in
    // for two concatenated raw digests; their VALUES are not checked here).
    std::string pieces(40, 'A');

    // The bencoded "info" dictionary VALUE (keys sorted: files, length-less,
    // name, piece length, pieces). We construct it by hand.
    std::string filesList =
        "ld6:lengthi100e4:pathl5:a.txteed"          // file a.txt, len 100
        "6:lengthi50e4:pathl5:b.bineed"             // file b.bin, len 50
        "6:lengthi8e4:pathl9:alignmenteee";         // alignment pad, len 8
    // info = d 5:files <filesList> 12:piece length i262144e 6:pieces <pieces> e
    std::string info =
        "d5:files" + filesList +
        "12:piece lengthi262144e" +
        "6:pieces" + std::to_string(pieces.size()) + ":" + pieces +
        "e";

    // Top level: d 15:direct download <url> 4:info <info> e
    std::string url = "http://example/dd.torrent";
    std::string top =
        "d15:direct download" + std::to_string(url.size()) + ":" + url +
        "4:info" + info +
        "e";

    wcr::Torrent t = wcr::parse_torrent(str_bytes(top));

    CHECK(t.pieceLength == 262144);
    CHECK(t.pieces == pieces);
    CHECK(t.directDownload == url);
    REQUIRE(t.files.size() == 3);
    CHECK(t.files[0].path == "a.txt");
    CHECK(t.files[0].length == 100);
    CHECK(t.files[0].alignment == false);
    CHECK(t.files[1].path == "b.bin");
    CHECK(t.files[1].length == 50);
    CHECK(t.files[2].path == "alignment");
    CHECK(t.files[2].alignment == true);

    // infoHash must equal sha1 of the exact raw "info" substring.
    std::array<unsigned char, 20> expected = wcr::sha1(str_bytes(info));
    CHECK(t.infoHash == expected);
}

TEST_CASE("parse_torrent throws on 25-digit integer overflow in bencode integer")
{
    // A bencoded integer whose decimal representation is 25 nines overflows
    // long long (max ~9.2e18, 19 digits). The parser must throw before UB.
    // Wrap it in a minimal valid torrent so parse_torrent exercises read_int.
    std::string pieces(20, 'A');
    std::string bigInt = std::string(25, '9'); // 25 nines
    std::string info =
        "d5:filesl"
        "d6:lengthi" + bigInt + "e4:pathl5:a.txtee"
        "e"
        "12:piece lengthi262144e"
        "6:pieces" + std::to_string(pieces.size()) + ":" + pieces +
        "e";
    std::string top = "d4:info" + info + "e";
    CHECK_THROWS_AS(wcr::parse_torrent(str_bytes(top)), std::runtime_error);
}

TEST_CASE("parse_torrent throws on 25-digit string length overflow in bencode string")
{
    // A bencoded string whose length prefix is 25 nines overflows long long.
    // Construct a torrent whose "pieces" key has a 25-digit length prefix.
    // The parser must throw on the overflow before the pointer-bounds check.
    std::string bigLen = std::string(25, '9'); // 25 nines
    // Minimal info dict: piece length + pieces with overflowing length
    std::string info =
        "d5:filesl"
        "d6:lengthi100e4:pathl5:a.txtee"
        "e"
        "12:piece lengthi262144e"
        "6:pieces" + bigLen + ":AAAA"  // length prefix overflows
        "e";
    std::string top = "d4:info" + info + "e";
    CHECK_THROWS_AS(wcr::parse_torrent(str_bytes(top)), std::runtime_error);
}
