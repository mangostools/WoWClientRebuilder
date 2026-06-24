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
 * @file bencode.h
 * @brief Minimal bencode decoder for the .tfil torrent: extracts piece
 *        metadata, the file list, the direct-download URL, and the
 *        SHA-1 info-hash of the raw bencoded "info" dictionary.
 */

#pragma once
#include "bytes.h"
#include <array>
#include <string>
#include <vector>

namespace wcr
{
/// One file entry from the torrent's "info"->"files" list.
struct TorrentFile
{
        std::string path;     ///< Path components joined with "/".
        long long length;     ///< Declared byte length of this file.
        bool alignment;       ///< True iff path == "alignment" (a pad entry).
};

/// Decoded view of a WoW .tfil torrent needed for piece verification.
struct Torrent
{
        long long pieceLength = 0;       ///< Bytes per piece (e.g. 262144).
        std::string pieces;              ///< Concatenated 20-byte SHA-1s.
        std::vector<TorrentFile> files;  ///< Ordered file list (incl. pads).
        std::string directDownload;      ///< Top-level "direct download" URL.
        std::array<unsigned char, 20> infoHash{}; ///< SHA-1 of raw "info" dict.
};

/// Parse a bencoded .tfil buffer into a Torrent. Throws std::runtime_error
/// on malformed input. infoHash = sha1() of the raw bencoded bytes of the
/// top-level "info" value. A file is alignment iff its joined path equals
/// "alignment".
Torrent parse_torrent(const Bytes& tfil);
} // namespace wcr
