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
#include "args.h"
#include <vector>
#include <string>

using namespace wcr;

/// Run parse_args over a literal argument list (argv[0] is the program name).
static bool run_parse(std::vector<const char*> args, RunParams& out,
                      std::string& err)
{
    return parse_args(static_cast<int>(args.size()),
                      const_cast<char**>(args.data()), out, err);
}

TEST_CASE("parse_args reads positionals and applies flag defaults")
{
    RunParams p;
    std::string err;
    bool ok = run_parse({"wowrebuild", "client", "4.3.4", "C:/out"}, p, err);
    REQUIRE(ok);
    CHECK(p.mode == Mode::FullClient);
    CHECK(p.version == "4.3.4");
    CHECK(p.outDir == "C:/out");
    REQUIRE(p.locales.size() == 1);
    CHECK(p.locales[0] == "enUS"); // no --locale == enUS default
    CHECK(p.tfilPath.empty());
    CHECK(p.realmlist == "127.0.0.1");
    CHECK(p.region == "EU");
    CHECK(p.cinematics == true); // included by default; --no-cinematics opts out
}

TEST_CASE("parse_args handles every flag and a csv --locale")
{
    RunParams p;
    std::string err;
    bool ok = run_parse(
        {"wowrebuild", "data", "4.3.4",
         "--locale", "enUS,deDE",
         "--tfil", "C:/in.tfil",
         "--realmlist", "logon.example.org",
         "--region", "NA",
         "--no-cinematics",
         "C:/out"},
        p, err);
    REQUIRE(ok);
    CHECK(p.mode == Mode::DataOnly);
    CHECK(p.version == "4.3.4");
    CHECK(p.outDir == "C:/out");
    REQUIRE(p.locales.size() == 2);
    CHECK(p.locales[0] == "enUS");
    CHECK(p.locales[1] == "deDE");
    CHECK(p.tfilPath == "C:/in.tfil");
    CHECK(p.realmlist == "logon.example.org");
    CHECK(p.region == "NA");
    CHECK(p.cinematics == false); // --no-cinematics opts out of the intros
}

TEST_CASE("parse_args still accepts the legacy --cinematics flag (no-op)")
{
    RunParams p;
    std::string err;
    bool ok = run_parse(
        {"wowrebuild", "client", "4.3.4", "--cinematics", "C:/out"}, p, err);
    REQUIRE(ok);
    // Cinematics are on by default; the legacy flag is accepted and harmless.
    CHECK(p.cinematics == true);
}

TEST_CASE("parse_args treats --locale all as the empty (all) sentinel")
{
    RunParams p;
    std::string err;
    bool ok = run_parse(
        {"wowrebuild", "locale", "4.3.4", "--locale", "all", "C:/out"},
        p, err);
    REQUIRE(ok);
    CHECK(p.mode == Mode::LocaleOnly);
    CHECK(p.locales.empty());
}

TEST_CASE("parse_args rejects bad input with false and a message")
{
    RunParams p;
    std::string err;

    SUBCASE("unknown mode")
    {
        err.clear();
        bool ok = run_parse({"wowrebuild", "bogus", "4.3.4", "C:/out"}, p, err);
        CHECK_FALSE(ok);
        CHECK_FALSE(err.empty());
    }
    SUBCASE("too few positionals")
    {
        err.clear();
        bool ok = run_parse({"wowrebuild", "client", "4.3.4"}, p, err);
        CHECK_FALSE(ok);
        CHECK_FALSE(err.empty());
    }
    SUBCASE("flag missing its value")
    {
        err.clear();
        bool ok = run_parse(
            {"wowrebuild", "client", "4.3.4", "C:/out", "--region"}, p, err);
        CHECK_FALSE(ok);
        CHECK_FALSE(err.empty());
    }
    SUBCASE("unknown flag")
    {
        err.clear();
        bool ok = run_parse(
            {"wowrebuild", "client", "4.3.4", "C:/out", "--nope"}, p, err);
        CHECK_FALSE(ok);
        CHECK_FALSE(err.empty());
    }
}

TEST_CASE("parse_args --yes sets the yes field and defaults to false")
{
    RunParams p;
    std::string err;

    SUBCASE("default: yes is false")
    {
        bool ok = run_parse({"wowrebuild", "client", "4.3.4", "C:/out"}, p, err);
        REQUIRE(ok);
        CHECK(p.yes == false);
    }

    SUBCASE("--yes sets yes to true")
    {
        bool ok = run_parse(
            {"wowrebuild", "client", "4.3.4", "--yes", "C:/out"}, p, err);
        REQUIRE(ok);
        CHECK(p.yes == true);
    }
}

TEST_CASE("parse_args rejects invalid --locale tokens (path injection guard)")
{
    RunParams p;
    std::string err;

    SUBCASE("--locale ../etc is rejected")
    {
        err.clear();
        bool ok = run_parse(
            {"wowrebuild", "client", "4.3.4", "--locale", "../etc", "C:/out"},
            p, err);
        CHECK_FALSE(ok);
        CHECK_FALSE(err.empty());
    }

    SUBCASE("--locale en/US is rejected (slash in token)")
    {
        err.clear();
        bool ok = run_parse(
            {"wowrebuild", "client", "4.3.4", "--locale", "en/US", "C:/out"},
            p, err);
        CHECK_FALSE(ok);
        CHECK_FALSE(err.empty());
    }

    SUBCASE("--locale enUS,deDE is accepted")
    {
        err.clear();
        bool ok = run_parse(
            {"wowrebuild", "client", "4.3.4", "--locale", "enUS,deDE", "C:/out"},
            p, err);
        REQUIRE(ok);
        REQUIRE(p.locales.size() == 2);
        CHECK(p.locales[0] == "enUS");
        CHECK(p.locales[1] == "deDE");
    }

    SUBCASE("--locale all is accepted as the all-locales sentinel")
    {
        err.clear();
        bool ok = run_parse(
            {"wowrebuild", "client", "4.3.4", "--locale", "all", "C:/out"},
            p, err);
        REQUIRE(ok);
        CHECK(p.locales.empty());
    }
}
