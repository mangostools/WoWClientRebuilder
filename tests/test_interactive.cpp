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

TEST_CASE("prompt_menu returns Select + the 0-based index of a valid choice")
{
    std::istringstream in("2\n");
    std::ostringstream out;
    int idx = -1;
    CHECK(wcr::prompt_menu(in, out, "Pick:", {"a", "b", "c"}, false, idx) ==
          wcr::Nav::Select);
    CHECK(idx == 1);
    CHECK(out.str().find("[2] b") != std::string::npos);
    CHECK(out.str().find("[X] Exit") != std::string::npos);
}

TEST_CASE("prompt_menu re-prompts on non-numeric then out-of-range, then accepts")
{
    // "x" is now the Exit token, so use a different non-numeric ("zz").
    std::istringstream in("zz\n9\n3\n");
    std::ostringstream out;
    int idx = -1;
    CHECK(wcr::prompt_menu(in, out, "Pick:", {"a", "b", "c"}, false, idx) ==
          wcr::Nav::Select);
    CHECK(idx == 2);
    // Both the non-numeric ("zz") and out-of-range ("9") inputs must re-prompt:
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

TEST_CASE("prompt_menu maps x and EOF to Exit")
{
    {
        std::istringstream in("x\n");
        std::ostringstream out;
        int idx = -1;
        CHECK(wcr::prompt_menu(in, out, "Pick:", {"a", "b"}, false, idx) ==
              wcr::Nav::Exit);
    }
    {
        std::istringstream in(""); // EOF -> Exit
        std::ostringstream out;
        int idx = -1;
        CHECK(wcr::prompt_menu(in, out, "Pick:", {"a", "b"}, false, idx) ==
              wcr::Nav::Exit);
    }
}

TEST_CASE("prompt_menu offers Back only when allowBack")
{
    {
        // allowBack=true: 'b' -> Back, and [B] Back is shown.
        std::istringstream in("b\n");
        std::ostringstream out;
        int idx = -1;
        CHECK(wcr::prompt_menu(in, out, "Pick:", {"a", "b"}, true, idx) ==
              wcr::Nav::Back);
        CHECK(out.str().find("[B] Back") != std::string::npos);
    }
    {
        // allowBack=false: 'b' is invalid input (re-prompt), and no [B] is shown.
        std::istringstream in("b\n1\n");
        std::ostringstream out;
        int idx = -1;
        CHECK(wcr::prompt_menu(in, out, "Pick:", {"a", "b"}, false, idx) ==
              wcr::Nav::Select);
        CHECK(idx == 0);
        CHECK(out.str().find("[B] Back") == std::string::npos);
    }
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

TEST_CASE("prompt_outdir: empty selects default, path kept, b/x navigate")
{
    {
        std::istringstream in("\n");
        std::ostringstream out;
        std::string path;
        CHECK(wcr::prompt_outdir(in, out, "def", true, path) ==
              wcr::Nav::Select);
        CHECK(path == "def");
        CHECK(out.str().find("Output folder [def]") != std::string::npos);
    }
    {
        std::istringstream in("  C:/x  \n");
        std::ostringstream out;
        std::string path;
        CHECK(wcr::prompt_outdir(in, out, "def", true, path) ==
              wcr::Nav::Select);
        CHECK(path == "C:/x");
    }
    {
        std::istringstream in("b\n"); // Back (allowBack true)
        std::ostringstream out;
        std::string path;
        CHECK(wcr::prompt_outdir(in, out, "def", true, path) == wcr::Nav::Back);
    }
    {
        std::istringstream in("x\n"); // Exit
        std::ostringstream out;
        std::string path;
        CHECK(wcr::prompt_outdir(in, out, "def", true, path) == wcr::Nav::Exit);
    }
    {
        std::istringstream in(""); // EOF -> Exit
        std::ostringstream out;
        std::string path;
        CHECK(wcr::prompt_outdir(in, out, "def", false, path) ==
              wcr::Nav::Exit);
    }
    {
        // allowBack=false: 'b' is taken as a literal path, not Back.
        std::istringstream in("b\n");
        std::ostringstream out;
        std::string path;
        CHECK(wcr::prompt_outdir(in, out, "def", false, path) ==
              wcr::Nav::Select);
        CHECK(path == "b");
    }
}

TEST_CASE("prompt_mode maps menu choices to Mode; EOF is Exit")
{
    {
        std::istringstream in("1\n");
        std::ostringstream out;
        wcr::Mode m{};
        CHECK(wcr::prompt_mode(in, out, false, m) == wcr::Nav::Select);
        CHECK(m == wcr::Mode::FullClient);
    }
    {
        std::istringstream in("2\n");
        std::ostringstream out;
        wcr::Mode m{};
        CHECK(wcr::prompt_mode(in, out, false, m) == wcr::Nav::Select);
        CHECK(m == wcr::Mode::DataOnly);
    }
    {
        std::istringstream in("3\n");
        std::ostringstream out;
        wcr::Mode m{};
        CHECK(wcr::prompt_mode(in, out, false, m) == wcr::Nav::Select);
        CHECK(m == wcr::Mode::LocaleOnly);
    }
    {
        std::istringstream in(""); // EOF -> Exit
        std::ostringstream out;
        wcr::Mode m{};
        CHECK(wcr::prompt_mode(in, out, false, m) == wcr::Nav::Exit);
    }
}

TEST_CASE("prompt_version returns the chosen version string")
{
    std::istringstream in("2\n");
    std::ostringstream out;
    std::string v;
    CHECK(wcr::prompt_version(in, out, true, v) == wcr::Nav::Select);
    CHECK(v == "5.4.8");
    CHECK(out.str().find("4.3.4 (15595)") != std::string::npos);
    CHECK(out.str().find("5.4.8 (18414)") != std::string::npos);
}

TEST_CASE("prompt_region returns EU or NA, and b is Back")
{
    {
        std::istringstream in("2\n");
        std::ostringstream out;
        std::string r;
        CHECK(wcr::prompt_region(in, out, true, r) == wcr::Nav::Select);
        CHECK(r == "NA");
    }
    {
        std::istringstream in("1\n");
        std::ostringstream out;
        std::string r;
        CHECK(wcr::prompt_region(in, out, true, r) == wcr::Nav::Select);
        CHECK(r == "EU");
    }
    {
        std::istringstream in("b\n");
        std::ostringstream out;
        std::string r;
        CHECK(wcr::prompt_region(in, out, true, r) == wcr::Nav::Back);
    }
}

TEST_CASE("build_for_version maps known versions and rejects unknown")
{
    CHECK(wcr::build_for_version("4.3.4") == "15595");
    CHECK(wcr::build_for_version("5.4.8") == "18414");
    CHECK(wcr::build_for_version("9.9.9") == "");
}

TEST_CASE("prompt_locale: numbered pick, all-sentinel, back/exit, EOF")
{
    std::vector<std::string> avail = {"enUS", "deDE", "frFR"};
    {
        std::istringstream in("2\n");
        std::ostringstream out;
        std::vector<std::string> r;
        CHECK(wcr::prompt_locale(in, out, avail, true, r) == wcr::Nav::Select);
        REQUIRE(r.size() == 1u);
        CHECK(r[0] == "deDE");
        CHECK(out.str().find("[A] All locales") != std::string::npos);
    }
    {
        std::istringstream in("A\n");
        std::ostringstream out;
        std::vector<std::string> r{"stale"};
        CHECK(wcr::prompt_locale(in, out, avail, true, r) == wcr::Nav::Select);
        CHECK(r.empty()); // all sentinel
    }
    {
        std::istringstream in("all\n");
        std::ostringstream out;
        std::vector<std::string> r{"stale"};
        CHECK(wcr::prompt_locale(in, out, avail, true, r) == wcr::Nav::Select);
        CHECK(r.empty());
    }
    {
        std::istringstream in("zz\n1\n"); // re-prompt then accept
        std::ostringstream out;
        std::vector<std::string> r;
        CHECK(wcr::prompt_locale(in, out, avail, true, r) == wcr::Nav::Select);
        REQUIRE(r.size() == 1u);
        CHECK(r[0] == "enUS");
    }
    {
        std::istringstream in("b\n");
        std::ostringstream out;
        std::vector<std::string> r;
        CHECK(wcr::prompt_locale(in, out, avail, true, r) == wcr::Nav::Back);
    }
    {
        std::istringstream in("x\n");
        std::ostringstream out;
        std::vector<std::string> r;
        CHECK(wcr::prompt_locale(in, out, avail, true, r) == wcr::Nav::Exit);
    }
    {
        std::istringstream in(""); // EOF -> Exit
        std::ostringstream out;
        std::vector<std::string> r;
        CHECK(wcr::prompt_locale(in, out, avail, true, r) == wcr::Nav::Exit);
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
    // output folder = <empty line> (accept default).
    std::istringstream in("1\n1\n1\n1\n\n");
    std::ostringstream out;
    wcr::FetchLocales stub = [](const std::string&)
    { return std::vector<std::string>{"enUS", "deDE"}; };
    wcr::RunParams p = wcr::run_interactive(in, out, stub);
    CHECK(p.cancelled == false);
    CHECK(p.mode == wcr::Mode::FullClient);
    CHECK(p.version == "4.3.4");
    REQUIRE(p.locales.size() == 1u);
    CHECK(p.locales[0] == "enUS");
    CHECK(p.region == "EU");
    CHECK(p.outDir == "WoW-4.3.4-15595-enUS");
    CHECK(p.cinematics == true); // interactive always includes the cinematics
    CHECK(p.realmlist == "127.0.0.1");
    // Proves the output-folder prompt actually ran (empty-input -> default).
    CHECK(out.str().find("Output folder [WoW-4.3.4-15595-enUS]") !=
          std::string::npos);
}

TEST_CASE("run_interactive: resetScreen fires once before each question shown")
{
    // full, 4.3.4, enUS, EU, default folder => 5 questions => 5 resets. Proves
    // the per-question screen reset is invoked without any real-console side
    // effect (the callback just counts).
    std::istringstream in("1\n1\n1\n1\n\n");
    std::ostringstream out;
    int resets = 0;
    wcr::FetchLocales stub = [](const std::string&)
    { return std::vector<std::string>{"enUS", "deDE"}; };
    wcr::run_interactive(in, out, stub, [&](std::ostream&) { ++resets; });
    CHECK(resets == 5); // Mode, Version, Locale, Region, Output
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
    CHECK(p.cancelled == false);
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
    CHECK(p.cancelled == false);
    CHECK(p.mode == wcr::Mode::LocaleOnly);
    CHECK(p.locales.empty()); // "all" sentinel
    CHECK(p.outDir == "C:/mine");
}

TEST_CASE("run_interactive: Back from Region returns to Locale and re-picks")
{
    // full, 4.3.4, locale=enUS, then 'b' at Region -> back to Locale, pick
    // deDE, region=EU, default folder.
    std::istringstream in("1\n1\n1\nb\n2\n1\n\n");
    std::ostringstream out;
    wcr::FetchLocales stub = [](const std::string&)
    { return std::vector<std::string>{"enUS", "deDE"}; };
    wcr::RunParams p = wcr::run_interactive(in, out, stub);
    CHECK(p.cancelled == false);
    REQUIRE(p.locales.size() == 1u);
    CHECK(p.locales[0] == "deDE"); // the changed answer took effect
    CHECK(p.region == "EU");
    CHECK(p.outDir == "WoW-4.3.4-15595-deDE");
}

TEST_CASE("run_interactive: Back from Version returns to Mode and a mode switch resets locale")
{
    // full, 'b' at Version -> back to Mode, switch to data-only, 4.3.4, NA.
    // Switching to DataOnly on the re-pick must clear any locale state and the
    // locale step must stay skipped.
    std::istringstream in("1\nb\n2\n1\n1\n\n");
    std::ostringstream out;
    bool fetched = false;
    wcr::FetchLocales stub = [&](const std::string&)
    { fetched = true; return std::vector<std::string>{"enUS", "deDE"}; };
    wcr::RunParams p = wcr::run_interactive(in, out, stub);
    CHECK(p.cancelled == false);
    CHECK(p.mode == wcr::Mode::DataOnly);
    CHECK(p.version == "4.3.4");
    CHECK(p.region == "EU");
    CHECK(p.locales.empty());           // reset on the switch to DataOnly
    CHECK(fetched == false);            // DataOnly never reaches the locale step
    CHECK(p.outDir == "WoW-4.3.4-15595");
}

TEST_CASE("run_interactive: Exit then confirm-yes cancels")
{
    // full, 4.3.4, then 'x' at Locale -> confirm 'y' -> cancelled.
    std::istringstream in("1\n1\nx\ny\n");
    std::ostringstream out;
    wcr::FetchLocales stub = [](const std::string&)
    { return std::vector<std::string>{"enUS", "deDE"}; };
    wcr::RunParams p = wcr::run_interactive(in, out, stub);
    CHECK(p.cancelled == true);
}

TEST_CASE("run_interactive: Exit then confirm-no re-shows the same question")
{
    // full, 4.3.4, 'x' at Locale -> confirm 'n' -> Locale again -> enUS, EU.
    std::istringstream in("1\n1\nx\nn\n1\n1\n\n");
    std::ostringstream out;
    wcr::FetchLocales stub = [](const std::string&)
    { return std::vector<std::string>{"enUS", "deDE"}; };
    wcr::RunParams p = wcr::run_interactive(in, out, stub);
    CHECK(p.cancelled == false);
    REQUIRE(p.locales.size() == 1u);
    CHECK(p.locales[0] == "enUS");
    CHECK(p.region == "EU");
}

TEST_CASE("run_interactive: EOF mid-flow cancels (no confirm, no infinite loop)")
{
    // full, 4.3.4, then the stream ends before the locale answer. EOF cannot
    // answer an exit confirmation, so the flow must cancel and return, NOT loop.
    std::istringstream in("1\n1\n");
    std::ostringstream out;
    wcr::FetchLocales stub = [](const std::string&)
    { return std::vector<std::string>{"enUS", "deDE"}; };
    wcr::RunParams p = wcr::run_interactive(in, out, stub);
    CHECK(p.cancelled == true);
}

TEST_CASE("run_interactive: DataOnly Back from Region skips Locale to Version")
{
    // data, 4.3.4, 'b' at Region -> back to Version (Locale skipped), pick
    // 5.4.8, region=EU, default folder.
    std::istringstream in("2\n1\nb\n2\n1\n\n");
    std::ostringstream out;
    bool fetched = false;
    wcr::FetchLocales stub = [&](const std::string&)
    { fetched = true; return std::vector<std::string>{"enUS"}; };
    wcr::RunParams p = wcr::run_interactive(in, out, stub);
    CHECK(p.cancelled == false);
    CHECK(p.mode == wcr::Mode::DataOnly);
    CHECK(p.version == "5.4.8"); // re-picked after stepping back to Version
    CHECK(p.region == "EU");
    CHECK(p.outDir == "WoW-5.4.8-18414");
    CHECK(fetched == false); // DataOnly never fetches locales
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
    std::vector<std::string> r{"junk"};
    CHECK(wcr::prompt_locale(in, out, {}, true, r) == wcr::Nav::Select);
    CHECK(r.empty());         // all-locales sentinel
    CHECK(out.str().empty()); // short-circuits without prompting/looping
}
