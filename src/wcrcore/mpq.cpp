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
 * @file mpq.cpp
 * @brief StormLib-backed MPQ archive reader implementation.
 */

#include "mpq.h"
#include "StormLib.h"
#include <stdexcept>
namespace
{
// RAII guard for a StormLib file handle opened with SFileOpenFileEx.
// Ensures SFileCloseFile is called even if an exception propagates.
struct MpqFileGuard
{
        HANDLE h{nullptr};
        ~MpqFileGuard()
        {
            if (h)
            {
                SFileCloseFile(h);
            }
        }
};

// RAII guard for a StormLib find handle from SFileFindFirstFile.
struct MpqFindGuard
{
        HANDLE h{nullptr};
        ~MpqFindGuard()
        {
            if (h)
            {
                SFileFindClose(h);
            }
        }
};
} // namespace

namespace wcr
{
MpqArchive::MpqArchive(const std::string& path)
{
    HANDLE h = nullptr;
    if (!SFileOpenArchive(path.c_str(), 0, MPQ_OPEN_READ_ONLY, &h))
    {
        throw std::runtime_error("SFileOpenArchive failed: " + path);
    }
    h_ = h;
}
MpqArchive::~MpqArchive()
{
    if (h_)
    {
        SFileCloseArchive((HANDLE)h_);
    }
}

bool MpqArchive::contains(const std::string& name) const
{
    return SFileHasFile((HANDLE)h_, name.c_str());
}
std::vector<std::string> MpqArchive::list() const
{
    std::vector<std::string> out;
    SFILE_FIND_DATA fd;
    memset(&fd, 0, sizeof(fd));
    HANDLE f = SFileFindFirstFile((HANDLE)h_, "*", &fd, nullptr);
    if (f)
    {
        MpqFindGuard guard{f};
        do
        {
            out.emplace_back(fd.cFileName);
        } while (SFileFindNextFile(f, &fd));
    }
    return out;
}
Bytes MpqArchive::extract(const std::string& name) const
{
    HANDLE hf = nullptr;
    if (!SFileOpenFileEx((HANDLE)h_, name.c_str(), SFILE_OPEN_FROM_MPQ, &hf))
    {
        throw std::runtime_error("file not found in MPQ: " + name);
    }
    MpqFileGuard guard{hf};
    DWORD high = 0, low = SFileGetFileSize(hf, &high);
    if (low == SFILE_INVALID_SIZE)
    {
        throw std::runtime_error("SFileGetFileSize failed: " + name);
    }
    Bytes out;
    out.reserve(low);
    uint8_t buf[65536];
    DWORD rd = 0;
    // StormLib read loop: SFileReadFile returns FALSE at EOF as well as on
    // real errors; the distinguishing invariant is that on EOF it still fills
    // 'rd' with the bytes of the final partial block before returning FALSE.
    // Appending 'rd' bytes on every iteration (including the last FALSE) means
    // we never drop the tail of the file.
    for (;;)
    {
        BOOL ok = SFileReadFile(hf, buf, sizeof(buf), &rd, nullptr);
        out.insert(
            out.end(), buf,
            buf + rd); // append the bytes actually read (incl. final partial)
        if (!ok)
        {
            break; // EOF or error: last partial already captured
        }
    }
    return out;
}
} // namespace wcr
