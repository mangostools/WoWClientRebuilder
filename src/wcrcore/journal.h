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
 * @file journal.h
 * @brief Resume journal: records artifacts that have passed integrity so a
 *        re-run can skip already-completed work.
 */

#pragma once
#include <set>
#include <string>

namespace wcr
{
/// On-disk resume ledger. Each line of the journal file is one relPath that
/// has passed ALL integrity checks. The file lives at outDir/.wcr-journal.
struct Journal
{
    std::string path;           ///< Full path to the .wcr-journal file.
    std::set<std::string> done; ///< Set of relPaths already completed.
};

/// Load the journal at outDir/.wcr-journal. A missing file yields an empty,
/// ready-to-use Journal whose path still points at where it will be written.
Journal load_journal(const std::string& outDir);

/// Return true if relPath is already recorded as done in journal j.
bool is_done(const Journal& j, const std::string& relPath);

/// Insert relPath into the journal's done set and append it as a new line to
/// the on-disk journal file. Call ONLY after relPath passes integrity.
void mark_done(Journal& j, const std::string& relPath);

/// True if a resume journal already exists at outDir/.wcr-journal.
bool journal_exists(const std::string& outDir);

/// Remove the resume journal at outDir/.wcr-journal (no-op if absent), e.g.
/// when the user declines to resume a previous download.
void clear_journal(const std::string& outDir);
} // namespace wcr
