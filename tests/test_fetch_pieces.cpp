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
 * @file test_fetch_pieces.cpp
 * @brief Integration test: reconstruct() runs torrent piece verification
 *        on a PlainUrl Data file when opts.torrent is set.
 */

#include "doctest.h"
#include "fetch.h"
#include "recipe.h"
#include "bencode.h"
#include "sha1.h"
#include "bytes.h"
#include <array>
#include <filesystem>
#include <string>

namespace
{
// Build a file:// URL for an absolute path (forward slashes).
std::string file_url(const std::filesystem::path& p)
{
    std::string s = p.generic_string();
    return "file:///" + s;
}

// One-file recipe with a single PlainUrl artifact, no MD5 (md5 empty),
// size set so the shared size check passes.
wcr::Recipe one_plainurl(const std::string& url, const std::string& outName,
                         long long size)
{
    wcr::Recipe r;
    r.version = "test";
    r.build = "0";
    wcr::Artifact a;
    a.outName = outName;
    a.source = wcr::Source::PlainUrl;
    a.url = url;
    a.size = size;
    r.artifacts.push_back(a);
    return r;
}

// Single-piece torrent over the given bytes (zero-padded to pieceLen).
wcr::Torrent one_piece_torrent(const wcr::Bytes& fileBytes, long long pieceLen,
                               const std::string& relPath, bool corrupt)
{
    wcr::Torrent t;
    t.pieceLength = pieceLen;
    wcr::Bytes piece = fileBytes;
    piece.resize((size_t)pieceLen, 0);
    std::array<unsigned char, 20> h = wcr::sha1(piece);
    if (corrupt)
    {
        h[0] ^= 0xFF;
    }
    t.pieces.assign(reinterpret_cast<const char*>(h.data()), 20);
    wcr::TorrentFile f;
    f.path = relPath;
    f.length = (long long)fileBytes.size();
    f.alignment = false;
    t.files.push_back(f);
    return t;
}
} // namespace

TEST_CASE("reconstruct piece-verifies a PlainUrl Data file via opts.torrent")
{
    namespace fs = std::filesystem;
    fs::path root = fs::temp_directory_path() / "wcr_fetch_pieces";
    fs::remove_all(root);
    fs::path srcDir = root / "src";
    fs::create_directories(srcDir);

    // The served file content.
    wcr::Bytes content = {10, 20, 30, 40, 50};
    fs::path srcFile = srcDir / "Data" / "frag.dat";
    fs::create_directories(srcFile.parent_path());
    wcr::write_file(srcFile.string(), content);

    std::string url = file_url(srcFile);
    wcr::Recipe r =
        one_plainurl(url, "Data/frag.dat", (long long)content.size());

    // Good torrent -> reconstruct succeeds.
    {
        fs::path out = root / "out_good";
        wcr::Torrent good =
            one_piece_torrent(content, 32, "Data/frag.dat", false);
        wcr::ReconstructOpts opts;
        opts.torrent = &good;
        CHECK_NOTHROW(wcr::reconstruct(r, out.string(), opts));
    }

    // Corrupt torrent hash -> reconstruct throws from piece verification.
    {
        fs::path out = root / "out_bad";
        wcr::Torrent bad =
            one_piece_torrent(content, 32, "Data/frag.dat", true);
        wcr::ReconstructOpts opts;
        opts.torrent = &bad;
        CHECK_THROWS_AS(wcr::reconstruct(r, out.string(), opts),
                        std::runtime_error);
    }
}
