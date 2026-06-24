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
 * @file zip.cpp
 * @brief In-memory PKZIP member extraction via miniz.
 */

#include "zip.h"
#include "miniz.h"
#include <stdexcept>

namespace wcr
{
Bytes zip_extract_member(const Bytes& zip, const std::string& member)
{
    mz_zip_archive ar;
    mz_zip_zero_struct(&ar);
    if (!mz_zip_reader_init_mem(&ar, zip.data(), zip.size(), 0))
    {
        throw std::runtime_error("zip: cannot open archive");
    }
    int idx = mz_zip_reader_locate_file(&ar, member.c_str(), nullptr, 0);
    if (idx < 0)
    {
        mz_zip_reader_end(&ar);
        throw std::runtime_error("zip: member not found: " + member);
    }
    size_t outSize = 0;
    void* p = mz_zip_reader_extract_to_heap(&ar, idx, &outSize, 0);
    if (!p)
    {
        mz_zip_reader_end(&ar);
        throw std::runtime_error("zip: extract failed: " + member);
    }
    Bytes out(static_cast<uint8_t*>(p), static_cast<uint8_t*>(p) + outSize);
    mz_free(p);
    mz_zip_reader_end(&ar);
    return out;
}
} // namespace wcr
