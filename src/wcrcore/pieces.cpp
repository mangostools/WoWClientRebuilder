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
 * @file pieces.cpp
 * @brief Torrent piece-level integrity check implementation.
 */

#include "pieces.h"
#include "sha1.h"
#include "bytes.h"
#include <array>
#include <cstring>
#include <stdexcept>

namespace wcr
{

void verify_file_pieces(const Torrent& t, const std::string& torrentRelPath,
                        const std::string& localPath)
{
    if (t.pieceLength <= 0)
    {
        throw std::runtime_error(
            "verify_file_pieces: invalid pieceLength for " + torrentRelPath);
    }

    // Locate the file in the torrent and accumulate the preceding lengths.
    long long startOffset = -1;
    long long fileLen = 0;
    long long acc = 0;
    for (const TorrentFile& f : t.files)
    {
        if (f.path == torrentRelPath)
        {
            startOffset = acc;
            fileLen = f.length;
            break;
        }
        acc += f.length;
    }
    if (startOffset < 0)
    {
        throw std::runtime_error(
            "verify_file_pieces: file not in torrent: " + torrentRelPath);
    }

    // The file is piece-aligned at its start (a flagged live-unknown for
    // alignment pads, but every real Data file begins on a piece boundary).
    if (startOffset % t.pieceLength != 0)
    {
        throw std::runtime_error(
            "verify_file_pieces: file not piece-aligned: " + torrentRelPath);
    }

    Bytes contents = read_file(localPath);
    if ((long long)contents.size() != fileLen)
    {
        throw std::runtime_error(
            "verify_file_pieces: on-disk size mismatch for " + torrentRelPath);
    }

    long long firstPiece = startOffset / t.pieceLength;
    long long pieceLen = t.pieceLength;
    long long total = (long long)contents.size();
    // Number of pieces this file begins/covers (ceil division).
    long long pieceCount = (total + pieceLen - 1) / pieceLen;
    if (total == 0)
    {
        pieceCount = 0;
    }

    size_t haveDigests = t.pieces.size() / 20;
    for (long long i = 0; i < pieceCount; ++i)
    {
        long long globalIndex = firstPiece + i;
        if ((size_t)globalIndex >= haveDigests)
        {
            throw std::runtime_error(
                "verify_file_pieces: piece index out of range for " +
                torrentRelPath);
        }

        // Build the piece buffer: the file's slice, zero-padded to pieceLen
        // for the final partial piece.
        Bytes piece;
        piece.resize((size_t)pieceLen, 0);
        long long off = i * pieceLen;
        long long n = total - off;
        if (n > pieceLen)
        {
            n = pieceLen;
        }
        std::memcpy(piece.data(), contents.data() + (size_t)off, (size_t)n);

        std::array<unsigned char, 20> got = sha1(piece);
        const unsigned char* want =
            reinterpret_cast<const unsigned char*>(
                t.pieces.data() + (size_t)globalIndex * 20);
        if (std::memcmp(got.data(), want, 20) != 0)
        {
            throw std::runtime_error(
                "verify_file_pieces: piece " + std::to_string(globalIndex) +
                " mismatch in " + torrentRelPath);
        }
    }
}

} // namespace wcr
