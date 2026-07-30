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
#include <vector>

namespace wcr
{
/// Identity of the run that produced a journal. A resume is only sound when
/// the new run would fetch the SAME bytes from the SAME place under the SAME
/// checks. Every input that changes any of those belongs here.
///
/// Region matters because same-named archives genuinely differ between CDN
/// regions (MoP's Updates/wow-0-18414-Win-final.MPQ is 21729424 in EU and
/// 21729944 in NA). Resuming an interrupted EU download under --region NA would
/// append NA bytes at the EU partial's offset and land on exactly the NA
/// expected size, so the size check -- the only check a Data artifact has --
/// would accept a file spliced from two regions.
///
/// `torrentId` identifies WHICH torrent verified the pieces, not merely that
/// one was supplied: a journal entry means it "passed ALL integrity checks",
/// so two different --tfil files must not stamp alike, or entries written
/// under one set of piece hashes would let a run with different hashes skip
/// those files. Empty means no piece verification was in force.
struct RunStamp
{
    std::string region;    ///< Selected CDN region ("EU"/"NA").
    std::string manifest;  ///< Partial-manifest name the sizes came from.
    std::string torrentId; ///< Identity of the .tfil, or "" for none.

    bool operator==(const RunStamp& o) const
    {
        return region == o.region && manifest == o.manifest &&
               torrentId == o.torrentId;
    }
};

/// On-disk resume ledger. Each line of the journal file is one relPath that
/// has passed ALL integrity checks. The file lives at outDir/.wcr-journal.
struct Journal
{
    std::string path;           ///< Full path to the .wcr-journal file.
    std::set<std::string> done; ///< Set of relPaths already completed.
    RunStamp stamp;             ///< Identity of the run that owns these lines.
    bool stamped = false;       ///< Whether the file carried a stamp at all.
};

/// Load the journal at outDir/.wcr-journal. A missing file yields an empty,
/// ready-to-use Journal whose path still points at where it will be written.
Journal load_journal(const std::string& outDir);

/// True only when j was written by a run matching `want` and may therefore be
/// resumed. Pure predicate, no side effects: deciding is separate from
/// discarding so the caller can confirm with the user before destroying
/// anything. Fail-closed -- an unstamped journal (written before stamps
/// existed, or hand-made) has unestablishable provenance and never matches,
/// and neither does a journal missing any stamp field.
bool journal_matches(const Journal& j, const RunStamp& want);

/// Write j's stamp to a fresh journal file, replacing any existing one. Call
/// once the run is committed, BEFORE the first download: otherwise an
/// interruption during the very first artifact leaves partials on disk with
/// no journal to identify which run produced them, and the next run --
/// seeing no journal -- would treat them as its own and resume them.
void write_stamp(Journal& j);

/// Remove the journal and the given partial-download paths, so nothing from
/// a previous run can be resumed into or mistaken for this one. `partPaths`
/// must be exactly the paths THIS run would resume (see artifact_part_paths):
/// a blind sweep by suffix would also delete unrelated files that happen to
/// end in .part. Throws if anything could not be removed -- proceeding after
/// a failed cleanup would resume the very bytes this call exists to destroy.
void discard_stale_run(const std::string& outDir,
                       const std::vector<std::string>& partPaths);

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
