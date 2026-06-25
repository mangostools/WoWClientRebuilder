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
#include <ostream>
#include <string>
#include <vector>

namespace wcr
{
/// Print the MaNGOS banner used on the interactive path.
void print_banner(std::ostream& out);

/// Clear the console and re-show the banner so each interactive question
/// appears on a clean screen.
void clear_screen_and_print_banner(std::ostream& out);

/// Returns the advertised locales for a version (production wraps
/// fetch_manifest; tests pass a stub). The advertised locale list is identical
/// across CDN regions, so region is not a parameter.
using FetchLocales =
    std::function<std::vector<std::string>(const std::string& version)>;

/// Injected screen-reset action (clear the console + re-show the banner) run
/// before each interactive question. Empty by default so the unit-tested core
/// stays free of real-console side effects; production passes
/// clear_screen_and_print_banner.
using ScreenReset = std::function<void(std::ostream& out)>;

/// Outcome of a navigable prompt: the user made a choice, asked to go Back to
/// the previous question, or asked to Exit. Back is only offered when the
/// caller passes allowBack (i.e. not on the first question).
enum class Nav
{
    Select, ///< The out-parameter holds the chosen value.
    Back,   ///< Return to the previous question.
    Exit    ///< Quit the interactive flow (caller confirms + aborts).
};

/// Print a numbered menu with [B] Back (when allowBack) and [X] Exit, and read
/// a choice. On Nav::Select, index is the 0-based option. EOF maps to Exit.
/// Re-prompts on invalid input.
Nav prompt_menu(std::istream& in, std::ostream& out, const std::string& title,
                const std::vector<std::string>& options, bool allowBack,
                int& index);

/// Ask a yes/no question. Returns true for y/yes, false for n/no or EOF.
bool prompt_yes_no(std::istream& in, std::ostream& out,
                   const std::string& question);

/// Prompt for the output folder, showing deflt in brackets and the B/X hints.
/// Empty input selects deflt; "b" is Back (when allowBack), "x" is Exit, EOF is
/// Exit; any other input is taken as the path (original case preserved).
Nav prompt_outdir(std::istream& in, std::ostream& out, const std::string& deflt,
                  bool allowBack, std::string& out_path);

/// Ask which generation mode to run. On Nav::Select, out holds the Mode.
Nav prompt_mode(std::istream& in, std::ostream& out, bool allowBack,
                Mode& out_mode);

/// Ask which client version to rebuild. On Nav::Select, out holds "4.3.4" etc.
Nav prompt_version(std::istream& in, std::ostream& out, bool allowBack,
                   std::string& out_version);

/// Ask which CDN region to download from. On Nav::Select, out is "EU" or "NA".
Nav prompt_region(std::istream& in, std::ostream& out, bool allowBack,
                  std::string& out_region);

/// The build number paired with a supported version ("4.3.4" -> "15595"), or
/// "" if the version is unknown. Mirrors find_recipe()'s supported set.
std::string build_for_version(const std::string& version);

/// Ask which locale(s) to include. On Nav::Select, out is {chosen} for a single
/// pick or {} for "all" (the empty == all sentinel). An empty advertised list
/// selects {} without prompting.
Nav prompt_locale(std::istream& in, std::ostream& out,
                  const std::vector<std::string>& available, bool allowBack,
                  std::vector<std::string>& out_locales);

/// Build the default output-folder name: WoW-<version>-<build>[-<localeTag>].
/// DataOnly omits the locale tag; "all" (empty locales) yields "-all".
std::string derive_outdir(const std::string& version, const std::string& build,
                          Mode mode, const std::vector<std::string>& locales);

/// Drive the full interactive menu and return a populated RunParams. The user
/// can step Back to the previous question or Exit (with a confirmation) at any
/// of the five setup prompts; an Exit sets RunParams.cancelled so the caller
/// aborts before downloading. The locale list is obtained via
/// fetchLocales(version) (skipped, both directions, for Data only). resetScreen,
/// when set, runs before each question so it appears on a freshly-bannered
/// screen.
RunParams run_interactive(std::istream& in, std::ostream& out,
                          const FetchLocales& fetchLocales,
                          const ScreenReset& resetScreen = {});

/// Resume decision: false (resume) when no journal exists; otherwise prompt and
/// return true ONLY if the user explicitly chooses to start fresh. EOF/empty/
/// "n" => false (safe default: keep the journal and resume). resetScreen, when
/// set, refreshes the banner before the prompt.
bool should_clear_journal(std::istream& in, std::ostream& out,
                          bool journalExists, const std::string& outDir,
                          const ScreenReset& resetScreen = {});
} // namespace wcr
