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
 * @file bytes.h
 * @brief Raw byte buffer type and convenience file-I/O helpers.
 */

#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include <fstream>
#include <stdexcept>
namespace wcr
{
/// Raw byte buffer type used throughout the library.
using Bytes = std::vector<uint8_t>;

/// Read an entire file from disk and return its contents as a Bytes buffer.
inline Bytes read_file(const std::string& p)
{
    std::ifstream f(p, std::ios::binary);
    if (!f)
    {
        throw std::runtime_error("cannot open " + p);
    }
    return Bytes((std::istreambuf_iterator<char>(f)),
                 std::istreambuf_iterator<char>());
}

/// Write a Bytes buffer to disk, overwriting any existing file at path p.
/// Throws std::runtime_error if the write or close fails (e.g. disk full).
inline void write_file(const std::string& p, const Bytes& b)
{
    std::ofstream f(p, std::ios::binary);
    if (!f)
    {
        throw std::runtime_error("cannot write " + p);
    }
    f.write(reinterpret_cast<const char*>(b.data()), (std::streamsize)b.size());
    if (!f)
    {
        throw std::runtime_error("write_file: write failed: " + p);
    }
    f.close();
    if (!f)
    {
        throw std::runtime_error("write_file: close failed (disk full?): " + p);
    }
}
} // namespace wcr
