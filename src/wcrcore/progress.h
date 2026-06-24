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
 * @file progress.h
 * @brief Pure, testable helpers for human-readable byte sizes and a
 *        single-line download-progress renderer.
 */

#pragma once
#include <string>

namespace wcr
{
/// Convert a byte count to a human-readable 1024-based string.
/// Units: B / KB / MB / GB / TB, two decimal places for KB and above.
/// @param n  Byte count; the sentinel value -1 returns "?" (unknown size).
/// @returns  E.g. "0 B", "1.50 KB", "3.67 GB", "?".
std::string human_bytes(long long n);

/// Build a single-line download-progress string for console output.
/// @param label       Filename or short description of the download.
/// @param now         Bytes received so far.
/// @param total       Total expected bytes; <= 0 means unknown (no percentage).
/// @param bytesPerSec Current download speed in bytes/sec; < 0 means unknown
///                    (speed is omitted from the output).
/// @returns  E.g. "  foo.mpq  42%  1.50 KB / 3.55 KB  12.50 MB/s"
///           or   "  foo.mpq  1.50 KB"  when total is unknown and speed < 0.
std::string render_progress(const std::string& label,
                            long long now,
                            long long total,
                            long long bytesPerSec);
} // namespace wcr
