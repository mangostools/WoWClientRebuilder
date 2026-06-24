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
 * @file mpq.h
 * @brief RAII wrapper for reading files out of a StormLib MPQ archive.
 */

#pragma once
#include "bytes.h"
#include <string>
#include <vector>
namespace wcr
{
/// RAII wrapper around a StormLib MPQ archive opened for reading.
class MpqArchive
{
    public:
        /// Open the MPQ archive at path; throws std::runtime_error on failure.
        explicit MpqArchive(const std::string& path);
        /// Close the archive handle.
        ~MpqArchive();
        MpqArchive(const MpqArchive&) = delete;
        MpqArchive& operator=(const MpqArchive&) = delete;
        /// Return a list of all file names contained in the archive.
        std::vector<std::string> list() const;
        /// Return true if the archive contains a file with the given name.
        bool contains(const std::string& name) const;
        /// Extract and return the contents of the named file.
        Bytes extract(const std::string& name) const;

    private:
        void* h_ = nullptr; ///< Underlying StormLib HANDLE.
};
} // namespace wcr
