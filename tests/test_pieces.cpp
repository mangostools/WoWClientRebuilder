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
 * @file test_pieces.cpp
 * @brief Unit tests for wcr::verify_file_pieces() on a synthetic torrent.
 */

#include "doctest.h"
#include "pieces.h"
#include "bencode.h"
#include "sha1.h"
#include "bytes.h"
#include <array>
#include <filesystem>
#include <string>

// Build a one-file Torrent whose single piece exactly covers the fixture.
static wcr::Torrent make_torrent(const wcr::Bytes& fileBytes,
                                 long long pieceLen,
                                 const std::string& relPath)
{
    wcr::Torrent t;
    t.pieceLength = pieceLen;

    // Single piece: file bytes padded with zeros up to pieceLen.
    wcr::Bytes piece = fileBytes;
    piece.resize((size_t)pieceLen, 0);
    std::array<unsigned char, 20> h = wcr::sha1(piece);
    t.pieces.assign(reinterpret_cast<const char*>(h.data()), 20);

    wcr::TorrentFile f;
    f.path = relPath;
    f.length = (long long)fileBytes.size();
    f.alignment = false;
    t.files.push_back(f);
    return t;
}

TEST_CASE("verify_file_pieces passes a good file and rejects a flipped byte")
{
    namespace fs = std::filesystem;
    fs::path dir = fs::temp_directory_path() / "wcr_pieces_test";
    fs::create_directories(dir);
    fs::path file = dir / "frag.dat";

    // 10-byte fixture, 32-byte pieces -> one padded piece.
    wcr::Bytes good = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    wcr::write_file(file.string(), good);

    wcr::Torrent t = make_torrent(good, 32, "frag.dat");

    // Good file passes.
    CHECK_NOTHROW(
        wcr::verify_file_pieces(t, "frag.dat", file.string()));

    // Flip one byte on disk -> the piece hash no longer matches.
    wcr::Bytes bad = good;
    bad[3] ^= 0xFF;
    wcr::write_file(file.string(), bad);
    CHECK_THROWS_AS(
        wcr::verify_file_pieces(t, "frag.dat", file.string()),
        std::runtime_error);
}
