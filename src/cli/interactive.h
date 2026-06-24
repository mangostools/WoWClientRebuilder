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
 * @file interactive.h
 * @brief Interactive console front-end: numbered-menu prompts that fill a
 *        RunParams when the tool is launched without arguments.
 */

#pragma once
#include "args.h" // RunParams, Mode
#include <functional>
#include <iosfwd>
#include <string>
#include <vector>

namespace wcr
{
/// Returns the advertised locales for a version (production wraps
/// fetch_manifest; tests pass a stub). The advertised locale list is identical
/// across CDN regions, so region is not a parameter.
using FetchLocales =
    std::function<std::vector<std::string>(const std::string& version)>;

/// Print a numbered menu and read a choice. Returns the 0-based index of the
/// selected option, or -1 on end-of-input. Re-prompts on invalid input.
int prompt_menu(std::istream& in, std::ostream& out, const std::string& title,
                const std::vector<std::string>& options);

/// Ask a yes/no question. Returns true for y/yes, false for n/no or EOF.
bool prompt_yes_no(std::istream& in, std::ostream& out,
                   const std::string& question);

/// Prompt for a line of text, showing deflt in brackets. Empty input or EOF
/// returns deflt; otherwise the trimmed input.
std::string prompt_line(std::istream& in, std::ostream& out,
                        const std::string& prompt, const std::string& deflt);

/// Ask which generation mode to run.
Mode prompt_mode(std::istream& in, std::ostream& out);

/// Ask which client version to rebuild. Returns a version string ("4.3.4").
std::string prompt_version(std::istream& in, std::ostream& out);

/// Ask which CDN region to download from. Returns "EU" or "NA".
std::string prompt_region(std::istream& in, std::ostream& out);

/// The build number paired with a supported version ("4.3.4" -> "15595"), or
/// "" if the version is unknown. Mirrors find_recipe()'s supported set.
std::string build_for_version(const std::string& version);

/// Ask which locale(s) to include. Returns {chosen} for a single pick, {} for
/// "all" (the empty == all sentinel the engine expects), or {"enUS"} on EOF.
std::vector<std::string> prompt_locale(std::istream& in, std::ostream& out,
                                       const std::vector<std::string>& available);

/// Build the default output-folder name: WoW-<version>-<build>[-<localeTag>].
/// DataOnly omits the locale tag; "all" (empty locales) yields "-all".
std::string derive_outdir(const std::string& version, const std::string& build,
                          Mode mode, const std::vector<std::string>& locales);

/// Drive the full interactive menu and return a populated RunParams. The
/// locale list is obtained via fetchLocales(version) (skipped for Data only).
RunParams run_interactive(std::istream& in, std::ostream& out,
                          const FetchLocales& fetchLocales);

/// Resume decision: false (resume) when no journal exists; otherwise prompt and
/// return true ONLY if the user explicitly chooses to start fresh. EOF/empty/
/// "n" => false (safe default: keep the journal and resume).
bool should_clear_journal(std::istream& in, std::ostream& out,
                          bool journalExists, const std::string& outDir);
} // namespace wcr
