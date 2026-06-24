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
 * @file pieces.h
 * @brief Torrent piece-level integrity check for a single reconstructed file.
 */

#pragma once
#include "bencode.h"
#include <string>

namespace wcr
{
/// Verify the on-disk file at localPath against the torrent's piece hashes.
///
/// The file's byte offset within the torrent stream is the sum of all
/// preceding TorrentFile.length values (real and alignment). The file is
/// piece-aligned at its start; every 256 KB (pieceLength) piece that this
/// file's bytes touch is hashed with sha1() and compared to the
/// corresponding 20-byte block in Torrent::pieces. The file's final partial
/// piece is reconstructed as the file's tail bytes followed by zero padding
/// up to pieceLength before hashing. Alignment files are assumed zero-filled
/// (a flagged live-unknown).
///
/// Throws std::runtime_error naming \p torrentRelPath and the first failing
/// piece index on any mismatch, or if the file cannot be matched.
void verify_file_pieces(const Torrent& t, const std::string& torrentRelPath,
                        const std::string& localPath);
} // namespace wcr
