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
 * @file journal.cpp
 * @brief Resume journal implementation.
 */

#include "journal.h"
#include <filesystem>
#include <fstream>

namespace wcr
{
Journal load_journal(const std::string& outDir)
{
    Journal j;
    j.path = outDir + "/.wcr-journal";
    std::ifstream in(j.path, std::ios::binary);
    if (!in)
    {
        return j;
    }
    std::string line;
    while (std::getline(in, line))
    {
        if (!line.empty() && line.back() == '\r')
        {
            line.pop_back();
        }
        if (!line.empty())
        {
            j.done.insert(line);
        }
    }
    return j;
}

bool is_done(const Journal& j, const std::string& relPath)
{
    return j.done.find(relPath) != j.done.end();
}

void mark_done(Journal& j, const std::string& relPath)
{
    if (j.done.insert(relPath).second)
    {
        std::ofstream out(j.path, std::ios::binary | std::ios::app);
        out << relPath << "\n";
    }
}

bool journal_exists(const std::string& outDir)
{
    // Non-throwing overload (mirrors clear_journal): a filesystem error on a
    // user-typed output path must not escape — the interactive resume-offer
    // call site runs outside main()'s try/catch.
    std::error_code ec;
    return std::filesystem::exists(outDir + "/.wcr-journal", ec);
}

void clear_journal(const std::string& outDir)
{
    std::error_code ec;
    std::filesystem::remove(outDir + "/.wcr-journal", ec);
}
} // namespace wcr
