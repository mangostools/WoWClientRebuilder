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

#include "doctest.h"
#include "interactive.h"
#include <sstream>
#include <string>
#include <vector>

TEST_CASE("prompt_menu returns the 0-based index of a valid choice")
{
    std::istringstream in("2\n");
    std::ostringstream out;
    int idx = wcr::prompt_menu(in, out, "Pick:", {"a", "b", "c"});
    CHECK(idx == 1);
    CHECK(out.str().find("[2] b") != std::string::npos);
}

TEST_CASE("prompt_menu re-prompts on non-numeric then out-of-range, then accepts")
{
    std::istringstream in("x\n9\n3\n");
    std::ostringstream out;
    int idx = wcr::prompt_menu(in, out, "Pick:", {"a", "b", "c"});
    CHECK(idx == 2);
    // Both the non-numeric ("x") and out-of-range ("9") inputs must re-prompt:
    // count the error line EXACTLY twice (matches the test-case name).
    std::string s = out.str();
    std::size_t n = 0;
    std::string::size_type pos = 0;
    while ((pos = s.find("between 1 and 3", pos)) != std::string::npos)
    {
        ++n;
        pos += 1;
    }
    CHECK(n == 2);
}

TEST_CASE("prompt_menu returns -1 on end-of-input")
{
    std::istringstream in("");
    std::ostringstream out;
    CHECK(wcr::prompt_menu(in, out, "Pick:", {"a", "b"}) == -1);
}

TEST_CASE("prompt_yes_no accepts y/yes/n/no and re-prompts otherwise")
{
    {
        std::istringstream in("y\n");
        std::ostringstream out;
        CHECK(wcr::prompt_yes_no(in, out, "ok?") == true);
    }
    {
        std::istringstream in("No\n");
        std::ostringstream out;
        CHECK(wcr::prompt_yes_no(in, out, "ok?") == false);
    }
    {
        std::istringstream in("maybe\nyes\n");
        std::ostringstream out;
        CHECK(wcr::prompt_yes_no(in, out, "ok?") == true);
        CHECK(out.str().find("'y' or 'n'") != std::string::npos);
    }
    {
        std::istringstream in(""); // EOF -> false
        std::ostringstream out;
        CHECK(wcr::prompt_yes_no(in, out, "ok?") == false);
    }
}

TEST_CASE("prompt_line returns default on empty/EOF and the trimmed value otherwise")
{
    {
        std::istringstream in("\n");
        std::ostringstream out;
        CHECK(wcr::prompt_line(in, out, "Folder", "def") == "def");
    }
    {
        std::istringstream in("  C:/x  \n");
        std::ostringstream out;
        CHECK(wcr::prompt_line(in, out, "Folder", "def") == "C:/x");
    }
    {
        std::istringstream in(""); // EOF -> default
        std::ostringstream out;
        CHECK(wcr::prompt_line(in, out, "Folder", "def") == "def");
    }
}

TEST_CASE("prompt_mode maps menu choices to Mode")
{
    {
        std::istringstream in("1\n");
        std::ostringstream out;
        CHECK(wcr::prompt_mode(in, out) == wcr::Mode::FullClient);
    }
    {
        std::istringstream in("2\n");
        std::ostringstream out;
        CHECK(wcr::prompt_mode(in, out) == wcr::Mode::DataOnly);
    }
    {
        std::istringstream in("3\n");
        std::ostringstream out;
        CHECK(wcr::prompt_mode(in, out) == wcr::Mode::LocaleOnly);
    }
    {
        std::istringstream in(""); // EOF -> default FullClient
        std::ostringstream out;
        CHECK(wcr::prompt_mode(in, out) == wcr::Mode::FullClient);
    }
}

TEST_CASE("prompt_version returns the chosen version string")
{
    std::istringstream in("2\n");
    std::ostringstream out;
    CHECK(wcr::prompt_version(in, out) == "5.4.8");
    CHECK(out.str().find("4.3.4 (15595)") != std::string::npos);
    CHECK(out.str().find("5.4.8 (18414)") != std::string::npos);
}

TEST_CASE("prompt_region returns EU or NA and defaults to EU")
{
    {
        std::istringstream in("2\n");
        std::ostringstream out;
        CHECK(wcr::prompt_region(in, out) == "NA");
    }
    {
        std::istringstream in("1\n");
        std::ostringstream out;
        CHECK(wcr::prompt_region(in, out) == "EU");
    }
    {
        std::istringstream in(""); // EOF -> EU
        std::ostringstream out;
        CHECK(wcr::prompt_region(in, out) == "EU");
    }
}

TEST_CASE("build_for_version maps known versions and rejects unknown")
{
    CHECK(wcr::build_for_version("4.3.4") == "15595");
    CHECK(wcr::build_for_version("5.4.8") == "18414");
    CHECK(wcr::build_for_version("9.9.9") == "");
}

TEST_CASE("prompt_locale: numbered pick, all-sentinel, and EOF default")
{
    std::vector<std::string> avail = {"enUS", "deDE", "frFR"};
    {
        std::istringstream in("2\n");
        std::ostringstream out;
        std::vector<std::string> r = wcr::prompt_locale(in, out, avail);
        REQUIRE(r.size() == 1u);
        CHECK(r[0] == "deDE");
        CHECK(out.str().find("[A] All locales") != std::string::npos);
    }
    {
        std::istringstream in("A\n");
        std::ostringstream out;
        CHECK(wcr::prompt_locale(in, out, avail).empty()); // all sentinel
    }
    {
        std::istringstream in("all\n");
        std::ostringstream out;
        CHECK(wcr::prompt_locale(in, out, avail).empty());
    }
    {
        std::istringstream in("zz\n1\n"); // re-prompt then accept
        std::ostringstream out;
        std::vector<std::string> r = wcr::prompt_locale(in, out, avail);
        REQUIRE(r.size() == 1u);
        CHECK(r[0] == "enUS");
    }
    {
        std::istringstream in(""); // EOF -> enUS
        std::ostringstream out;
        std::vector<std::string> r = wcr::prompt_locale(in, out, avail);
        REQUIRE(r.size() == 1u);
        CHECK(r[0] == "enUS");
    }
}

TEST_CASE("derive_outdir builds the default folder name per mode and locale")
{
    using wcr::Mode;
    CHECK(wcr::derive_outdir("4.3.4", "15595", Mode::FullClient, {"enUS"}) ==
          "WoW-4.3.4-15595-enUS");
    CHECK(wcr::derive_outdir("4.3.4", "15595", Mode::FullClient, {}) ==
          "WoW-4.3.4-15595-all");
    CHECK(wcr::derive_outdir("4.3.4", "15595", Mode::LocaleOnly,
                             {"enUS", "deDE"}) == "WoW-4.3.4-15595-enUS-deDE");
    CHECK(wcr::derive_outdir("4.3.4", "15595", Mode::DataOnly, {}) ==
          "WoW-4.3.4-15595");
}

TEST_CASE("run_interactive: full client / single locale / EU")
{
    // mode=1 (full), version=1 (4.3.4), locale=1 (enUS), region=1 (EU),
    // output folder = <empty line> (accept default, exercising prompt_line's
    // empty-input branch, NOT the EOF short-circuit).
    std::istringstream in("1\n1\n1\n1\n\n");
    std::ostringstream out;
    wcr::FetchLocales stub = [](const std::string&)
    { return std::vector<std::string>{"enUS", "deDE"}; };
    wcr::RunParams p = wcr::run_interactive(in, out, stub);
    CHECK(p.mode == wcr::Mode::FullClient);
    CHECK(p.version == "4.3.4");
    REQUIRE(p.locales.size() == 1u);
    CHECK(p.locales[0] == "enUS");
    CHECK(p.region == "EU");
    CHECK(p.outDir == "WoW-4.3.4-15595-enUS");
    CHECK(p.cinematics == true); // interactive always includes the cinematics
    CHECK(p.realmlist == "127.0.0.1");
    // Proves the output-folder prompt actually ran (empty-input -> default),
    // rather than short-circuiting on EOF.
    CHECK(out.str().find("Output folder [WoW-4.3.4-15595-enUS]:") !=
          std::string::npos);
}

TEST_CASE("run_interactive: data only skips the locale prompt")
{
    // mode=2 (data only), version=1 (4.3.4), region=2 (NA), folder=<empty>.
    std::istringstream in("2\n1\n2\n\n");
    std::ostringstream out;
    bool fetched = false;
    wcr::FetchLocales stub = [&](const std::string&)
    { fetched = true; return std::vector<std::string>{"enUS"}; };
    wcr::RunParams p = wcr::run_interactive(in, out, stub);
    CHECK(p.mode == wcr::Mode::DataOnly);
    CHECK(fetched == false); // locale step skipped, so no fetch
    CHECK(p.region == "NA");
    CHECK(p.outDir == "WoW-4.3.4-15595");
    CHECK(p.locales.empty());
}

TEST_CASE("run_interactive: locale-only / all / custom output path")
{
    // mode=3 (locale only), version=1, locale=A (all), region=1,
    // folder="C:/mine".
    std::istringstream in("3\n1\nA\n1\nC:/mine\n");
    std::ostringstream out;
    wcr::FetchLocales stub = [](const std::string&)
    { return std::vector<std::string>{"enUS", "deDE"}; };
    wcr::RunParams p = wcr::run_interactive(in, out, stub);
    CHECK(p.mode == wcr::Mode::LocaleOnly);
    CHECK(p.locales.empty()); // "all" sentinel
    CHECK(p.outDir == "C:/mine");
}

TEST_CASE("should_clear_journal: only an explicit yes clears; EOF/no resume")
{
    {
        std::istringstream in("y\n"); // explicit start-fresh -> clear
        std::ostringstream out;
        CHECK(wcr::should_clear_journal(in, out, true, "out") == true);
    }
    {
        std::istringstream in("n\n"); // decline -> resume (keep journal)
        std::ostringstream out;
        CHECK(wcr::should_clear_journal(in, out, true, "out") == false);
    }
    {
        std::istringstream in(""); // EOF -> resume, NEVER clears (safe default)
        std::ostringstream out;
        CHECK(wcr::should_clear_journal(in, out, true, "out") == false);
    }
    {
        std::istringstream in("y\n"); // no journal -> no prompt, no clear
        std::ostringstream out;
        CHECK(wcr::should_clear_journal(in, out, false, "out") == false);
        CHECK(out.str().empty());
    }
}

TEST_CASE("prompt_locale: empty advertised list -> all-locales sentinel, no prompt")
{
    std::istringstream in("1\n"); // would be invalid against an empty list
    std::ostringstream out;
    std::vector<std::string> r = wcr::prompt_locale(in, out, {});
    CHECK(r.empty());         // all-locales sentinel
    CHECK(out.str().empty()); // short-circuits without prompting/looping
}
