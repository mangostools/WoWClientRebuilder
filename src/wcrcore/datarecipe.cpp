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
 * @file datarecipe.cpp
 * @brief Implementation of manifest expansion and pre-flight accounting.
 */

#include "datarecipe.h"
#include "progress.h"
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <istream>
#include <ostream>
#include <string>

namespace wcr
{
namespace
{
/// True when a manifest-relative path is safe to use as an output file path.
/// Returns false for absolute paths, root-relative paths (e.g. "/etc/x" on
/// Windows, which is not is_absolute() but still escapes outDir), paths with
/// a drive/root component, and any path containing a ".." component.
bool is_safe_relpath(const std::string& relPath)
{
    std::filesystem::path p(relPath);
    // Reject fully absolute paths (e.g. "C:/Windows/x.dll" on Windows,
    // "/home/x" on POSIX).
    if (p.is_absolute())
    {
        return false;
    }
    // Reject root-relative paths: on Windows "/etc/x" is not is_absolute()
    // but has a root_directory (the '\' prefix), so it still escapes outDir.
    if (p.has_root_directory())
    {
        return false;
    }
    // Reject Windows drive-relative paths: "C:foo" has a root_name ("C:") but
    // no root_directory and no ".." component, so it slips past the checks
    // above yet still resolves relative to the drive's current directory
    // (outside outDir). No-op on POSIX, where paths never carry a root_name.
    if (p.has_root_name())
    {
        return false;
    }
    // Reject any path with a ".." or "." component.  A single "." is a no-op
    // on the filesystem but has no place in a legitimate Blizzard manifest
    // path, so any such record is treated as suspicious and rejected.
    for (const std::filesystem::path& component : p)
    {
        if (component == ".." || component == ".")
        {
            return false;
        }
    }
    return true;
}

/// True when a manifest-relative path's FIRST component is exactly "Data" or
/// "Updates" — the only legitimate top-level subtrees in Blizzard's partial
/// manifest (MoP 5.4.8 ships Updates/wow-0-18414-Win-final.MPQ, so Updates is
/// not optional). This blocks a tampered record from colliding with a root
/// binary that lives at the client root or under Utils\ (Wow.exe,
/// Battle.net.dll, Utils\...) — none of which sit under Data/ or Updates/.
/// Comparison is done by path-component iteration (not a raw substring) so that
/// backslash separators ("Data\\x") and look-alike prefixes ("DataXYZ/x") are
/// handled correctly. Manifest relPaths may use backslashes.
bool has_allowed_top_level(const std::string& relPath)
{
    std::filesystem::path p(relPath);
    std::filesystem::path::iterator it = p.begin();
    if (it == p.end())
    {
        return false;
    }
    const std::filesystem::path& first = *it;
    return first == "Data" || first == "Updates";
}

/// True when a manifest-relative path is a cinematics asset.
bool is_cinematics_path(const std::string& relPath)
{
    return relPath.find("/Interface/Cinematics/") != std::string::npos;
}

/// True when a locale tag is present in the selected-locales set.
bool locale_selected(const std::vector<std::string>& selected,
                     const std::string& locale)
{
    return std::find(selected.begin(), selected.end(), locale) !=
           selected.end();
}

/// True for the macOS-only Data archives. The tool only builds Windows
/// clients, so these are always skipped (mirrors the C# IsAcceptedFile).
bool is_osx_only(const std::string& relPath)
{
    return relPath.find("-OSX") != std::string::npos;
}

/// True when any selected locale is a Chinese locale (alternate.MPQ is the
/// Chinese-alternate art archive; non-Chinese clients do not need it).
bool chinese_selected(const std::vector<std::string>& selected)
{
    return locale_selected(selected, "zhCN") ||
           locale_selected(selected, "zhTW");
}
} // namespace

std::vector<Artifact> expand_data(const Manifest& m,
                                  const std::vector<std::string>& locales,
                                  Mode mode, bool cinematics)
{
    std::vector<Artifact> out;
    for (const MfilRecord& rec : m.files)
    {
        const bool isBase = rec.locale.empty();
        bool keep = false;
        switch (mode)
        {
            case Mode::DataOnly:
            {
                keep = isBase;
                break;
            }
            case Mode::LocaleOnly:
            {
                keep = !isBase && locale_selected(locales, rec.locale);
                break;
            }
            case Mode::FullClient:
            {
                keep = isBase ||
                       locale_selected(locales, rec.locale);
                break;
            }
        }
        if (!keep)
        {
            continue;
        }

        // Skip size-0 records: these are directory placeholders in the
        // partial .mfil listing (e.g. "Data" / "Data/enUS" with size 0).
        // Real downloadable files always carry a positive byte count.
        if (rec.size <= 0)
        {
            continue;
        }

        // The tool produces Windows clients only: skip macOS-only archives
        // (base-OSX.MPQ, wow-0-*-OSX-final.MPQ).
        if (is_osx_only(rec.relPath))
        {
            continue;
        }
        // alternate.MPQ is Chinese-alternate art; keep it only when a Chinese
        // locale is selected.
        if (rec.relPath == "Data/alternate.MPQ" && !chinese_selected(locales))
        {
            continue;
        }

        // The shared Data/Interface/Cinematics/ movies are locale-independent
        // (isBase) and ship with every complete client, so they are always kept.
        // The per-locale Data/<locale>/Interface/Cinematics/ intros are included
        // by default (a complete client has them); --no-cinematics (cinematics ==
        // false) is the opt-out that drops them to shrink the download.
        const bool isCine = is_cinematics_path(rec.relPath);
        if (isCine && !isBase && !cinematics)
        {
            continue;
        }

        // Reject any record whose path would escape the output directory.
        if (!is_safe_relpath(rec.relPath))
        {
            continue;
        }

        // Restrict records to the only legitimate top-level subtrees (Data/ or
        // Updates/). A record outside them (e.g. a root binary name like
        // "Wow.exe") would otherwise overwrite a base artifact in the same
        // directory even though it cleared is_safe_relpath.
        if (!has_allowed_top_level(rec.relPath))
        {
            continue;
        }

        Artifact a;
        a.outName = rec.relPath;
        a.url = m.dataBaseUrl + rec.relPath;
        a.size = rec.size;
        a.locale = rec.locale;
        a.optional = isCine;
        a.source = Source::PlainUrl;
        out.push_back(a);
    }
    return out;
}

long long total_bytes(const std::vector<Artifact>& a)
{
    long long sum = 0;
    for (const Artifact& art : a)
    {
        if (art.size > 0)
        {
            sum += art.size;
        }
    }
    return sum;
}

long long free_space(const std::string& dir)
{
    // Walk up the directory tree to find the nearest existing ancestor so that
    // a not-yet-created output directory still yields the parent volume's
    // available space rather than -1.
    std::filesystem::path p(dir);
    while (!p.empty())
    {
        std::error_code ec;
        std::filesystem::space_info info = std::filesystem::space(p, ec);
        if (!ec)
        {
            return (long long)info.available;
        }
        std::filesystem::path parent = p.parent_path();
        if (parent == p)
        {
            break;
        }
        p = parent;
    }
    return -1;
}

bool confirm_preflight(long long totalBytes, long long availBytes,
                       bool assumeYes, std::istream& in, std::ostream& out)
{
    out << "Pre-flight: " << human_bytes(totalBytes)
        << " to download; " << human_bytes(availBytes)
        << " free at target.\n";
    if (availBytes >= 0 && availBytes < totalBytes)
    {
        out << "ERROR: insufficient disk space\n";
        return false;
    }
    if (assumeYes)
    {
        return true;
    }
    out << "Proceed with download? [y/N]: ";
    std::string line;
    if (!std::getline(in, line))
    {
        return false;
    }
    // Trim leading/trailing whitespace and lowercase.
    std::string s;
    for (char c : line)
    {
        if (!std::isspace(static_cast<unsigned char>(c)))
        {
            s.push_back(
                static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
        }
    }
    return s == "y" || s == "yes";
}
} // namespace wcr
