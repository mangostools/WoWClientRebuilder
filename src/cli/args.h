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
 * @file args.h
 * @brief CLI argument parsing for the flag-driven MVP: positional mode +
 *        version + outDir, with --locale/--tfil/--realmlist/--region/
 *        --no-cinematics flags resolved into a RunParams.
 */

#pragma once
#include "datarecipe.h"
#include <string>
#include <vector>

namespace wcr
{
/// Parsed command-line run parameters for one rebuild invocation.
struct RunParams
{
        Mode mode = Mode::FullClient;     ///< client/data/locale selection.
        std::string version;              ///< Recipe version, e.g. "4.3.4".
        std::vector<std::string> locales; ///< Requested locales; empty == all.
        std::string tfilPath;             ///< Optional path to a .tfil torrent.
        std::string realmlist = "127.0.0.1"; ///< realmlist host for WTF files.
        std::string region = "EU";        ///< CDN region segment (EU or NA).
        std::string outDir;               ///< Destination directory.
        bool cinematics = true;           ///< Include per-locale cinematics (on
                                          ///< by default; --no-cinematics skips).
        bool yes = false;                 ///< Skip interactive pre-flight prompt.
};

/// Parse argv into out. Returns true on success; on failure returns false and
/// sets err to a human-readable reason. Positional: mode {client,data,locale},
/// version, outDir. Flags: --locale (csv or "all"), --tfil, --realmlist,
/// --region, --no-cinematics, --yes. The "all" sentinel leaves out.locales
/// empty.
bool parse_args(int argc, char** argv, RunParams& out, std::string& err);
} // namespace wcr
