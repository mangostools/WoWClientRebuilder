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
 * @file assemble.cpp
 * @brief Implementation of pointer extraction and Recipe assembly.
 */

#include "assemble.h"
#include <cctype>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>

namespace wcr
{
namespace
{
/// Fold an output file name to its NTFS identity for collision detection.
/// NTFS folds case and strips trailing dots and spaces from each path segment,
/// so "Wow.exe", "wow.exe", "Wow.exe " and "Wow.exe." all denote one file.
/// Normalises by lowercasing, replacing '\\' with '/', and stripping trailing
/// '.' and ' ' from every '/'-separated segment.
std::string ntfs_normalize(const std::string& name)
{
    std::string lowered;
    lowered.reserve(name.size());
    for (char c : name)
    {
        if (c == '\\')
        {
            c = '/';
        }
        lowered.push_back(
            static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }

    std::string result;
    result.reserve(lowered.size());
    std::stringstream ss(lowered);
    std::string segment;
    bool first = true;
    while (std::getline(ss, segment, '/'))
    {
        // Strip trailing dots and spaces from the segment (NTFS behaviour).
        std::string::size_type end = segment.find_last_not_of(". ");
        if (end == std::string::npos)
        {
            segment.clear();
        }
        else
        {
            segment.erase(end + 1);
        }
        // Skip empty segments: they arise from consecutive '/' separators
        // (e.g. "Data//x" splits into ["Data", "", "x"]).  NTFS collapses
        // repeated separators, so the empty segment contributes nothing to
        // the on-disk path.  Only the leading segment of an absolute path
        // (before the first '/') is allowed to be empty; relative paths
        // handled here never start with '/'.
        if (segment.empty() && !first)
        {
            continue;
        }
        if (!first)
        {
            result.push_back('/');
        }
        result += segment;
        first = false;
    }
    return result;
}
} // namespace

std::string pointer_text_from_recipe(const Recipe& r)
{
    for (const Artifact& a : r.artifacts)
    {
        if (a.source == Source::Generated && a.outName == "WoW.mfil")
        {
            return a.content;
        }
    }
    throw std::runtime_error(
        "recipe has no Generated WoW.mfil pointer artifact");
}

Recipe assemble_recipe(const Recipe& base, const std::vector<Artifact>& data,
                       Mode mode)
{
    Recipe out;
    out.version = base.version;
    out.build = base.build;
    out.repairBase = base.repairBase;
    out.zips = base.zips;
    out.mpqs = base.mpqs;
    // Build the set of NTFS-normalized base outNames. A data artifact that
    // collides with one of these denotes the same on-disk file as a base
    // (binary) artifact and must be rejected — defense-in-depth behind the
    // Data/Updates allowlist in expand_data. The set is built unconditionally
    // so the guard also covers DataOnly/LocaleOnly tampering.
    std::set<std::string> baseNames;
    for (const Artifact& a : base.artifacts)
    {
        baseNames.insert(ntfs_normalize(a.outName));
    }

    if (mode == Mode::FullClient)
    {
        out.artifacts = base.artifacts;
    }
    for (const Artifact& a : data)
    {
        if (baseNames.find(ntfs_normalize(a.outName)) != baseNames.end())
        {
            // Fail closed: a collision means a tampered or corrupt manifest.
            throw std::runtime_error(
                "manifest record collides with a base artifact: " + a.outName);
        }
        out.artifacts.push_back(a);
    }
    return out;
}
} // namespace wcr
