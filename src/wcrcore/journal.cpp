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
#include <stdexcept>
#include <vector>

namespace wcr
{
namespace
{
/// Header key prefix for the stamp lines. '#' can never begin a manifest
/// relPath, so stamped and unstamped lines are unambiguous.
const char* kStampPrefix = "#wcr ";
const std::string::size_type kStampLen = 5; // strlen(kStampPrefix)

/// Bit per stamp key, so load_journal can demand a COMPLETE header. A header
/// truncated by a crash mid-write (e.g. region+manifest present, torrent line
/// missing) must not count as stamped: the absent torrent key would read as
/// "no torrent" and wrongly match a no-torrent run.
enum StampKey
{
    kSawRegion = 1,
    kSawManifest = 2,
    kSawRecipe = 4,
    kSawTorrent = 8,
    kSawAll = kSawRegion | kSawManifest | kSawRecipe | kSawTorrent
};

/// Parse "#wcr <key>=<value>" into the stamp. Unknown keys are ignored so a
/// journal written by a newer build degrades to "no usable stamp" rather than
/// being misread.
void apply_stamp_line(const std::string& line, RunStamp& s, int& sawKeys)
{
    const std::string body = line.substr(kStampLen);
    std::string::size_type eq = body.find('=');
    if (eq == std::string::npos)
    {
        return;
    }
    const std::string key = body.substr(0, eq);
    const std::string val = body.substr(eq + 1);
    if (key == "region")
    {
        s.region = val;
        sawKeys |= kSawRegion;
    }
    else if (key == "manifest")
    {
        s.manifest = val;
        sawKeys |= kSawManifest;
    }
    else if (key == "recipe")
    {
        s.recipeId = val;
        sawKeys |= kSawRecipe;
    }
    else if (key == "torrent")
    {
        s.torrentId = val;
        sawKeys |= kSawTorrent;
    }
}
} // namespace

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
    int sawKeys = 0;
    while (std::getline(in, line))
    {
        if (!line.empty() && line.back() == '\r')
        {
            line.pop_back();
        }
        if (line.empty())
        {
            continue;
        }
        if (line.compare(0, kStampLen, kStampPrefix) == 0)
        {
            apply_stamp_line(line, j.stamp, sawKeys);
            continue;
        }
        j.done.insert(line);
    }
    // Stamped ONLY with the complete header: a partial one (crash mid-write)
    // must never be trusted, or its missing keys would read as defaults and
    // could match a run they do not describe.
    j.stamped = (sawKeys == kSawAll);
    return j;
}

bool journal_matches(const Journal& j, const RunStamp& want)
{
    // A journal with no stamp, or an incomplete one, cannot be shown to
    // describe this run: refuse it rather than guess. (load_journal only sets
    // `stamped` when the full header was present.)
    if (!j.stamped || j.stamp.region.empty() || j.stamp.manifest.empty() ||
        j.stamp.recipeId.empty())
    {
        return false;
    }
    return j.stamp == want;
}

void write_stamp(Journal& j)
{
    // A first run into a brand-new output directory stamps before anything
    // else exists there, so create the directory here rather than relying on
    // reconstruct() to have done it.
    std::error_code dec;
    std::filesystem::create_directories(
        std::filesystem::path(j.path).parent_path(), dec);
    std::ofstream out(j.path, std::ios::binary | std::ios::trunc);
    if (!out)
    {
        throw std::runtime_error("cannot write resume journal " + j.path);
    }
    out << kStampPrefix << "region=" << j.stamp.region << "\n"
        << kStampPrefix << "manifest=" << j.stamp.manifest << "\n"
        << kStampPrefix << "recipe=" << j.stamp.recipeId << "\n"
        << kStampPrefix << "torrent=" << j.stamp.torrentId << "\n";
    out.flush();
    if (!out)
    {
        throw std::runtime_error("cannot write resume journal " + j.path);
    }
    j.stamped = true;
}

void discard_stale_run(const std::string& outDir,
                       const std::vector<std::string>& partPaths)
{
    // Only ever the paths this run itself would resume. A recursive sweep by
    // ".part" suffix would also delete unrelated files in a reused output dir.
    std::vector<std::string> failed;
    std::error_code ec;
    std::filesystem::remove(outDir + "/.wcr-journal", ec);
    if (ec)
    {
        failed.push_back(outDir + "/.wcr-journal");
    }
    for (const std::string& p : partPaths)
    {
        std::error_code rm;
        std::filesystem::remove(p, rm);
        if (rm)
        {
            failed.push_back(p);
        }
    }
    if (!failed.empty())
    {
        // Carrying on would resume exactly the bytes this call exists to
        // destroy, so fail loudly instead.
        throw std::runtime_error(
            "cannot discard stale download state (" +
            std::to_string(failed.size()) + " file(s), first: " + failed[0] +
            "); remove it by hand or choose a different output directory");
    }
}

bool is_done(const Journal& j, const std::string& relPath)
{
    return j.done.find(relPath) != j.done.end();
}

void mark_done(Journal& j, const std::string& relPath)
{
    if (j.done.insert(relPath).second)
    {
        // The stamp header is written up front by write_stamp(), not lazily
        // here: a journal must identify its run from the moment the first byte
        // is fetched, not from the moment the first artifact completes.
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
