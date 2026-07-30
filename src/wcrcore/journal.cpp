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
namespace
{
/// Header key prefix for the stamp lines. '#' can never begin a manifest
/// relPath, so stamped and unstamped lines are unambiguous.
const char* kStampPrefix = "#wcr ";
const std::string::size_type kStampLen = 5; // strlen(kStampPrefix)

/// Parse "#wcr <key>=<value>" into the stamp. Unknown keys are ignored so a
/// journal written by a newer build degrades to "no usable stamp" rather than
/// being misread.
void apply_stamp_line(const std::string& line, RunStamp& s, bool& sawAny)
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
        sawAny = true;
    }
    else if (key == "manifest")
    {
        s.manifest = val;
        sawAny = true;
    }
    else if (key == "pieces")
    {
        s.pieces = (val == "1");
        sawAny = true;
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
    bool sawStamp = false;
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
            apply_stamp_line(line, j.stamp, sawStamp);
            continue;
        }
        j.done.insert(line);
    }
    j.stamped = sawStamp;
    return j;
}

Journal load_journal(const std::string& outDir, const RunStamp& want)
{
    Journal j = load_journal(outDir);
    // An unstamped journal predates stamping (or was hand-made): its provenance
    // cannot be established, so it is discarded rather than trusted.
    if (j.done.empty() && !j.stamped)
    {
        j.stamp = want;
        return j;
    }
    if (j.stamped && j.stamp == want)
    {
        return j;
    }
    discard_stale_run(outDir);
    Journal fresh;
    fresh.path = j.path;
    fresh.stamp = want;
    return fresh;
}

void discard_stale_run(const std::string& outDir)
{
    std::error_code ec;
    std::filesystem::remove(outDir + "/.wcr-journal", ec);
    // Sweep partials so the next run cannot resume into bytes fetched under a
    // different region/manifest. Iterate non-throwing: the output dir is a
    // user-typed path and may be unreadable in places.
    std::filesystem::recursive_directory_iterator it(
        outDir, std::filesystem::directory_options::skip_permission_denied, ec);
    if (ec)
    {
        return;
    }
    std::filesystem::recursive_directory_iterator end;
    for (; it != end; it.increment(ec))
    {
        if (ec)
        {
            return;
        }
        const std::string p = it->path().string();
        const bool isPart = p.size() >= 5 && p.compare(p.size() - 5, 5,
                                                       ".part") == 0;
        const bool isFb = p.size() >= 8 && p.compare(p.size() - 8, 8,
                                                     ".part.fb") == 0;
        if (isPart || isFb)
        {
            std::error_code rm;
            std::filesystem::remove(it->path(), rm);
        }
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
        // Write the stamp header the first time the file is created, so every
        // journal on disk identifies the run that owns it.
        std::error_code ec;
        const bool fresh = !std::filesystem::exists(j.path, ec);
        std::ofstream out(j.path, std::ios::binary | std::ios::app);
        if (fresh && !j.stamp.region.empty())
        {
            out << kStampPrefix << "region=" << j.stamp.region << "\n"
                << kStampPrefix << "manifest=" << j.stamp.manifest << "\n"
                << kStampPrefix << "pieces=" << (j.stamp.pieces ? "1" : "0")
                << "\n";
            j.stamped = true;
        }
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
