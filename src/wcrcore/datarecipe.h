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
 * @file datarecipe.h
 * @brief Expand a parsed Data manifest into PlainUrl Artifacts and provide
 *        pre-flight size accounting (total bytes, free disk space).
 */

#pragma once
#include "mfil.h"
#include "recipe.h"
#include <iosfwd>
#include <string>
#include <vector>
namespace wcr
{
/// Which slice of the manifest to expand into download Artifacts.
enum class Mode
{
    FullClient, ///< Locale-independent records plus selected locale records.
    DataOnly,   ///< Locale-independent records only (locale == "").
    LocaleOnly  ///< Records whose locale is in the selected set only.
};

/// Map a manifest into PlainUrl Artifacts filtered by mode and locales.
/// Each surviving MfilRecord becomes an Artifact with outName == relPath,
/// url == m.dataBaseUrl + relPath, the record size and locale, and optional
/// set true when relPath contains "/Interface/Cinematics/".  When cinematics
/// is false, cinematics records are dropped entirely.
std::vector<Artifact> expand_data(const Manifest& m,
                                  const std::vector<std::string>& locales,
                                  Mode mode, bool cinematics);

/// Sum of every artifact's expected size; sizes of -1 contribute nothing.
long long total_bytes(const std::vector<Artifact>& a);

/// Bytes available on the filesystem containing dir (std::filesystem::space).
long long free_space(const std::string& dir);

/// Interactive pre-flight guard.
/// Prints a one-line summary to out.  If availBytes >= 0 and availBytes <
/// totalBytes, prints an error line and returns false.  If assumeYes is true,
/// returns true without reading in.  Otherwise prompts and returns true only
/// when the trimmed, lower-cased reply is "y" or "yes".
bool confirm_preflight(long long totalBytes, long long availBytes,
                       bool assumeYes, std::istream& in, std::ostream& out);
} // namespace wcr
