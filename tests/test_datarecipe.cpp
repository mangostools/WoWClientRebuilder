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
 * @file test_datarecipe.cpp
 * @brief Unit tests for wcr::expand_data, wcr::total_bytes, wcr::free_space.
 */

#include "doctest.h"
#include "datarecipe.h"
#include "mfil.h"
#include "recipe.h"
#include <sstream>
#include <string>
#include <vector>
#include <filesystem>

TEST_CASE("total_bytes sums sizes and ignores -1 sentinels")
{
    std::vector<wcr::Artifact> arts;

    wcr::Artifact a;
    a.outName = "Data/common.MPQ";
    a.size = 1000;
    arts.push_back(a);

    wcr::Artifact b;
    b.outName = "Data/expansion.MPQ";
    b.size = 250;
    arts.push_back(b);

    wcr::Artifact c;
    c.outName = "Data/unknown-size.file";
    c.size = -1;
    arts.push_back(c);

    CHECK(wcr::total_bytes(arts) == 1250);
    CHECK(wcr::total_bytes(std::vector<wcr::Artifact>{}) == 0);
}

static wcr::Manifest make_test_manifest()
{
    wcr::Manifest m;
    m.version = 2;
    m.dataBaseUrl =
        "http://dist.blizzard.com.edgesuite.net/wow-pod-retail/"
        "EU/15050.direct/";
    m.locales = {"enGB", "deDE"};

    wcr::MfilRecord base;
    base.relPath = "Data/common.MPQ";
    base.size = 1000;
    base.locale = "";
    m.files.push_back(base);

    wcr::MfilRecord en;
    en.relPath = "Data/enGB/locale-enGB.MPQ";
    en.size = 200;
    en.locale = "enGB";
    m.files.push_back(en);

    wcr::MfilRecord de;
    de.relPath = "Data/deDE/locale-deDE.MPQ";
    de.size = 300;
    de.locale = "deDE";
    m.files.push_back(de);

    wcr::MfilRecord cine;
    cine.relPath = "Data/Interface/Cinematics/WOW_Intro.avi";
    cine.size = 50;
    cine.locale = "";
    m.files.push_back(cine);

    return m;
}

TEST_CASE("expand_data DataOnly keeps only locale-independent records")
{
    wcr::Manifest m = make_test_manifest();
    std::vector<wcr::Artifact> arts =
        wcr::expand_data(m, {"enGB"}, wcr::Mode::DataOnly, true);

    // Base data file + base cinematic, no locale files.
    REQUIRE(arts.size() == 2u);

    const wcr::Artifact& mpq = arts[0];
    CHECK(mpq.outName == "Data/common.MPQ");
    CHECK(mpq.source == wcr::Source::PlainUrl);
    CHECK(mpq.url ==
          "http://dist.blizzard.com.edgesuite.net/wow-pod-retail/"
          "EU/15050.direct/Data/common.MPQ");
    CHECK(mpq.size == 1000);
    CHECK(mpq.locale == "");
    CHECK(mpq.optional == false);

    const wcr::Artifact& cine = arts[1];
    CHECK(cine.outName == "Data/Interface/Cinematics/WOW_Intro.avi");
    CHECK(cine.locale == "");
}

TEST_CASE("expand_data LocaleOnly keeps only selected-locale records")
{
    wcr::Manifest m = make_test_manifest();
    std::vector<wcr::Artifact> arts =
        wcr::expand_data(m, {"enGB"}, wcr::Mode::LocaleOnly, true);

    REQUIRE(arts.size() == 1u);
    const wcr::Artifact& en = arts[0];
    CHECK(en.outName == "Data/enGB/locale-enGB.MPQ");
    CHECK(en.locale == "enGB");
    CHECK(en.size == 200);
    CHECK(en.url ==
          "http://dist.blizzard.com.edgesuite.net/wow-pod-retail/"
          "EU/15050.direct/Data/enGB/locale-enGB.MPQ");
    CHECK(en.optional == false);
}

TEST_CASE("expand_data LocaleOnly with two locales keeps both")
{
    wcr::Manifest m = make_test_manifest();
    std::vector<wcr::Artifact> arts =
        wcr::expand_data(m, {"enGB", "deDE"}, wcr::Mode::LocaleOnly, true);

    REQUIRE(arts.size() == 2u);
    CHECK(arts[0].locale == "enGB");
    CHECK(arts[1].locale == "deDE");
}

TEST_CASE("expand_data FullClient keeps base records plus selected locale")
{
    wcr::Manifest m = make_test_manifest();
    std::vector<wcr::Artifact> arts =
        wcr::expand_data(m, {"enGB"}, wcr::Mode::FullClient, true);

    // base MPQ + enGB locale + base cinematic == 3; deDE dropped.
    REQUIRE(arts.size() == 3u);

    bool sawBase = false;
    bool sawEn = false;
    bool sawCine = false;
    bool sawDe = false;
    for (const wcr::Artifact& a : arts)
    {
        if (a.outName == "Data/common.MPQ")
        {
            sawBase = true;
        }
        if (a.outName == "Data/enGB/locale-enGB.MPQ")
        {
            sawEn = true;
        }
        if (a.outName == "Data/Interface/Cinematics/WOW_Intro.avi")
        {
            sawCine = true;
        }
        if (a.outName == "Data/deDE/locale-deDE.MPQ")
        {
            sawDe = true;
        }
    }
    CHECK(sawBase);
    CHECK(sawEn);
    CHECK(sawCine);
    CHECK(sawDe == false);
}

TEST_CASE("expand_data: shared Data/Interface/ cinematics always in, marked optional")
{
    wcr::Manifest m = make_test_manifest();

    // The shared Data/Interface/Cinematics/ movie is locale-independent, so it
    // is included regardless of the --cinematics flag, and is always marked
    // optional (best-effort fetch).
    for (bool cinematics : {true, false})
    {
        std::vector<wcr::Artifact> arts =
            wcr::expand_data(m, {}, wcr::Mode::DataOnly, cinematics);
        bool sawCine = false;
        for (const wcr::Artifact& a : arts)
        {
            if (a.outName == "Data/Interface/Cinematics/WOW_Intro.avi")
            {
                sawCine = true;
                CHECK(a.optional == true);
            }
            else
            {
                // Non-cinematic base files are never optional.
                CHECK(a.optional == false);
            }
        }
        CHECK(sawCine);
        // base MPQ + shared cinematic, regardless of the flag.
        REQUIRE(arts.size() == 2u);
    }
}

TEST_CASE("parse_mfil: shared Data/Interface/ assets are forced locale-independent")
{
    const std::string mfil =
        "version=2\n"
        "serverpath=base\n\tpath=\n"
        "serverpath=locale_enUS\n\tpath=\n"
        "serverpath=locale_frFR\n\tpath=\n"
        // Shared cinematic: mis-tagged path=locale_frFR but in the non-locale
        // Data/Interface/ path, so it must come out as base ("").
        "file=Data/Interface/Cinematics/WoW3X_Intro_1280.avi\n"
        "\tname=Data/Interface/Cinematics/WoW3X_Intro_1280.avi\n\tsize=100\n\tpath=locale_frFR\n"
        // Per-locale cinematic under the locale dir: keeps its locale.
        "file=Data/enUS/Interface/Cinematics/WOW_Intro_800.avi\n"
        "\tname=Data/enUS/Interface/Cinematics/WOW_Intro_800.avi\n\tsize=200\n\tpath=locale_enUS\n";
    wcr::Manifest m = wcr::parse_mfil(mfil, "http://x/");
    auto loc = [&](const std::string& name) -> std::string {
        for (const auto& r : m.files) { if (r.relPath == name) { return r.locale; } }
        return "<missing>";
    };
    CHECK(loc("Data/Interface/Cinematics/WoW3X_Intro_1280.avi") == "");
    CHECK(loc("Data/enUS/Interface/Cinematics/WOW_Intro_800.avi") == "enUS");
}

TEST_CASE("expand_data: shared cinematics always in, per-locale gated by --cinematics")
{
    const std::string mfil =
        "version=2\n"
        "serverpath=base\n\tpath=\n"
        "serverpath=locale_enUS\n\tpath=\n"
        "serverpath=locale_deDE\n\tpath=\n"
        "serverpath=locale_frFR\n\tpath=\n"
        "file=Data/Interface/Cinematics/WoW3X_Intro_1280.avi\n"
        "\tname=Data/Interface/Cinematics/WoW3X_Intro_1280.avi\n\tsize=100\n\tpath=locale_frFR\n"
        "file=Data/enUS/Interface/Cinematics/WOW_Intro_800.avi\n"
        "\tname=Data/enUS/Interface/Cinematics/WOW_Intro_800.avi\n\tsize=200\n\tpath=locale_enUS\n"
        "file=Data/deDE/Interface/Cinematics/WOW_Intro_800.avi\n"
        "\tname=Data/deDE/Interface/Cinematics/WOW_Intro_800.avi\n\tsize=300\n\tpath=locale_deDE\n";
    wcr::Manifest m = wcr::parse_mfil(mfil, "http://x/");
    auto has = [&](const std::vector<wcr::Artifact>& arts, const std::string& name) {
        for (const auto& a : arts) { if (a.outName == name) { return true; } }
        return false;
    };
    // Default (cinematics OFF): shared movie in, all per-locale movies out.
    std::vector<wcr::Artifact> def =
        wcr::expand_data(m, {"enUS"}, wcr::Mode::FullClient, false);
    CHECK(has(def, "Data/Interface/Cinematics/WoW3X_Intro_1280.avi"));
    CHECK_FALSE(has(def, "Data/enUS/Interface/Cinematics/WOW_Intro_800.avi"));
    CHECK_FALSE(has(def, "Data/deDE/Interface/Cinematics/WOW_Intro_800.avi"));
    // --cinematics ON: the selected locale's per-locale cinematics also come in.
    std::vector<wcr::Artifact> cin =
        wcr::expand_data(m, {"enUS"}, wcr::Mode::FullClient, true);
    CHECK(has(cin, "Data/Interface/Cinematics/WoW3X_Intro_1280.avi"));
    CHECK(has(cin, "Data/enUS/Interface/Cinematics/WOW_Intro_800.avi"));
    CHECK_FALSE(has(cin, "Data/deDE/Interface/Cinematics/WOW_Intro_800.avi"));
}

TEST_CASE("free_space reports available bytes and supports pre-flight check")
{
    long long avail = wcr::free_space(".");
    CHECK(avail > 0);

    std::vector<wcr::Artifact> arts;
    wcr::Artifact a;
    a.outName = "Data/tiny.MPQ";
    a.size = 16;
    arts.push_back(a);

    // A 16-byte plan must fit on any real test host.
    CHECK(avail >= wcr::total_bytes(arts));
}

TEST_CASE("free_space returns ancestor volume space for non-existent dir")
{
    // A deeply nested path that does not exist yet.  free_space must walk up
    // to the first existing ancestor (e.g. the temp directory) and return its
    // available space (> 0), not -1.
    std::filesystem::path nonexistent =
        std::filesystem::temp_directory_path() /
        "wcr_test_nonexistent_XJ7Q2" / "does" / "not" / "exist" / "yet";

    // Confirm the path really does not exist.
    REQUIRE_FALSE(std::filesystem::exists(nonexistent));

    long long avail = wcr::free_space(nonexistent.string());
    CHECK(avail > 0);
}

TEST_CASE("confirm_preflight: insufficient disk space returns false")
{
    std::istringstream in("");
    std::ostringstream out;
    // totalBytes > availBytes => should fail without touching in.
    bool result = wcr::confirm_preflight(1000LL, 500LL, false, in, out);
    CHECK_FALSE(result);
    std::string outStr = out.str();
    bool hasError = outStr.find("ERROR") != std::string::npos;
    CHECK(hasError);
    // Output must contain a human-readable size for the total (1000 B).
    bool hasTotalHuman = outStr.find("1000 B") != std::string::npos;
    CHECK(hasTotalHuman);
    // Output must contain a human-readable size for available space (500 B).
    bool hasAvailHuman = outStr.find("500 B") != std::string::npos;
    CHECK(hasAvailHuman);
}

TEST_CASE("confirm_preflight: assumeYes returns true without reading stream")
{
    std::istringstream in("");   // empty: nothing to read
    std::ostringstream out;
    // ample space
    bool result = wcr::confirm_preflight(100LL, 9999LL, true, in, out);
    CHECK(result);
    // The stream was never read: it must not be in a hard-fail state.
    bool streamOk = in.good() || in.eof();
    CHECK(streamOk);
    // Output must contain human-readable sizes (100 B to download).
    std::string outStr = out.str();
    bool hasTotalHuman = outStr.find("100 B") != std::string::npos;
    CHECK(hasTotalHuman);
}

TEST_CASE("expand_data skips size-0 directory placeholder records")
{
    wcr::Manifest m;
    m.version = 2;
    m.dataBaseUrl =
        "http://dist.blizzard.com.edgesuite.net/wow-pod-retail/"
        "EU/15050.direct/";
    m.locales = {"enUS"};

    // Directory placeholder: size == 0, no locale.
    wcr::MfilRecord dirRoot;
    dirRoot.relPath = "Data";
    dirRoot.size = 0;
    dirRoot.locale = "";
    m.files.push_back(dirRoot);

    // Directory placeholder: size == 0, locale-tagged.
    wcr::MfilRecord dirLocale;
    dirLocale.relPath = "Data/enUS";
    dirLocale.size = 0;
    dirLocale.locale = "enUS";
    m.files.push_back(dirLocale);

    // Real base file.
    wcr::MfilRecord realBase;
    realBase.relPath = "Data/art.MPQ";
    realBase.size = (long long)3943419041LL;
    realBase.locale = "";
    m.files.push_back(realBase);

    // Real locale file.
    wcr::MfilRecord realLocale;
    realLocale.relPath = "Data/enUS/locale-enUS.MPQ";
    realLocale.size = 150000000LL;
    realLocale.locale = "enUS";
    m.files.push_back(realLocale);

    std::vector<wcr::Artifact> arts =
        wcr::expand_data(m, {"enUS"}, wcr::Mode::FullClient, false);

    // Only the two real files; directory placeholders must be absent.
    REQUIRE(arts.size() == 2u);
    for (const wcr::Artifact& a : arts)
    {
        CHECK(a.size > 0);
        CHECK(a.outName != "Data");
        CHECK(a.outName != "Data/enUS");
    }
    bool sawBase   = false;
    bool sawLocale = false;
    for (const wcr::Artifact& a : arts)
    {
        if (a.outName == "Data/art.MPQ")
        {
            sawBase = true;
            CHECK(a.size == (long long)3943419041LL);
        }
        if (a.outName == "Data/enUS/locale-enUS.MPQ")
        {
            sawLocale = true;
            CHECK(a.size == 150000000LL);
        }
    }
    CHECK(sawBase);
    CHECK(sawLocale);
}

TEST_CASE("confirm_preflight: interactive replies")
{
    SUBCASE("'y' accepted")
    {
        std::istringstream in("y\n");
        std::ostringstream out;
        CHECK(wcr::confirm_preflight(100LL, 9999LL, false, in, out));
    }

    SUBCASE("'yes' accepted")
    {
        std::istringstream in("yes\n");
        std::ostringstream out;
        CHECK(wcr::confirm_preflight(100LL, 9999LL, false, in, out));
    }

    SUBCASE("'n' rejected")
    {
        std::istringstream in("n\n");
        std::ostringstream out;
        CHECK_FALSE(wcr::confirm_preflight(100LL, 9999LL, false, in, out));
    }

    SUBCASE("empty reply rejected")
    {
        std::istringstream in("\n");
        std::ostringstream out;
        CHECK_FALSE(wcr::confirm_preflight(100LL, 9999LL, false, in, out));
    }
}

TEST_CASE("parse_mfil derives locale from Data/<locale>/ dir when path=base")
{
    const std::string mfil =
        "version=2\n"
        "serverpath=base\n\tpath=\n"
        "serverpath=locale_enUS\n\tpath=\n"
        "serverpath=locale_deDE\n\tpath=\n"
        "file=Data/art.MPQ\n\tname=Data/art.MPQ\n\tsize=100\n\tpath=base\n"
        "file=Data/enUS/locale-enUS.MPQ\n\tname=Data/enUS/locale-enUS.MPQ\n\tsize=200\n\tpath=locale_enUS\n"
        "file=Data/wow-update-base-15211.MPQ\n\tname=Data/wow-update-base-15211.MPQ\n\tsize=300\n\tpath=base\n"
        "file=Data/enUS/wow-update-enUS-15211.MPQ\n\tname=Data/enUS/wow-update-enUS-15211.MPQ\n\tsize=400\n\tpath=base\n"
        "file=Data/deDE/wow-update-deDE-15211.MPQ\n\tname=Data/deDE/wow-update-deDE-15211.MPQ\n\tsize=500\n\tpath=base\n"
        "file=Data/Interface/foo.MPQ\n\tname=Data/Interface/foo.MPQ\n\tsize=600\n\tpath=base\n";
    wcr::Manifest m = wcr::parse_mfil(mfil, "http://x/");
    auto loc = [&](const std::string& name) -> std::string {
        for (const auto& r : m.files) { if (r.relPath == name) { return r.locale; } }
        return "<missing>";
    };
    CHECK(loc("Data/art.MPQ") == "");
    CHECK(loc("Data/enUS/locale-enUS.MPQ") == "enUS");
    CHECK(loc("Data/wow-update-base-15211.MPQ") == "");
    CHECK(loc("Data/enUS/wow-update-enUS-15211.MPQ") == "enUS");
    CHECK(loc("Data/deDE/wow-update-deDE-15211.MPQ") == "deDE");
    CHECK(loc("Data/Interface/foo.MPQ") == "");
}

TEST_CASE("expand_data: --locale enUS drops other locales' wow-update patches")
{
    const std::string mfil =
        "version=2\n"
        "serverpath=base\n\tpath=\n"
        "serverpath=locale_enUS\n\tpath=\n"
        "serverpath=locale_deDE\n\tpath=\n"
        "file=Data/wow-update-base-15211.MPQ\n\tname=Data/wow-update-base-15211.MPQ\n\tsize=300\n\tpath=base\n"
        "file=Data/enUS/wow-update-enUS-15211.MPQ\n\tname=Data/enUS/wow-update-enUS-15211.MPQ\n\tsize=400\n\tpath=base\n"
        "file=Data/deDE/wow-update-deDE-15211.MPQ\n\tname=Data/deDE/wow-update-deDE-15211.MPQ\n\tsize=500\n\tpath=base\n";
    wcr::Manifest m = wcr::parse_mfil(mfil, "http://x/");
    auto arts = wcr::expand_data(m, {"enUS"}, wcr::Mode::FullClient, false);
    auto has = [&](const std::string& name) {
        for (const auto& a : arts) { if (a.outName == name) { return true; } }
        return false;
    };
    CHECK(has("Data/wow-update-base-15211.MPQ"));
    CHECK(has("Data/enUS/wow-update-enUS-15211.MPQ"));
    CHECK_FALSE(has("Data/deDE/wow-update-deDE-15211.MPQ"));
}

TEST_CASE("parse_mfil v3: locales come from tag= (excludes base/OSX/Win/ALT/EXP)")
{
    // MoP 5.4.8 form: version=3, no serverpath=locale_, per-file tag= lines.
    const std::string mfil =
        "version=3\n"
        "serverpath=base\n\tpath=\n"
        "file=Data/base-Win.MPQ\n\tname=Data/base-Win.MPQ\n\tsize=100\n\tpath=base\n\ttag=Win\n"
        "file=Data/base-OSX.MPQ\n\tname=Data/base-OSX.MPQ\n\tsize=110\n\tpath=base\n\ttag=OSX\n"
        "file=Data/alternate.MPQ\n\tname=Data/alternate.MPQ\n\tsize=120\n\tpath=base\n\ttag=ALT\n"
        "file=Data/expansion1.MPQ\n\tname=Data/expansion1.MPQ\n\tsize=300\n\tpath=base\n\ttag=EXP1\n\ttag=base\n"
        "file=Data/enUS/locale-enUS.MPQ\n\tname=Data/enUS/locale-enUS.MPQ\n\tsize=400\n\tpath=base\n\ttag=enUS\n"
        "file=Data/deDE/expansion2-speech-deDE.MPQ\n\tname=Data/deDE/expansion2-speech-deDE.MPQ\n\tsize=500\n\tpath=base\n\ttag=deDE\n\ttag=EXP2\n";
    wcr::Manifest m = wcr::parse_mfil(mfil, "http://x/");
    auto hasLoc = [&](const std::string& s) {
        for (const auto& l : m.locales) { if (l == s) { return true; } }
        return false;
    };
    // Only real locale tags are advertised; category tags are excluded.
    REQUIRE(m.locales.size() == 2u);
    CHECK(hasLoc("enUS"));
    CHECK(hasLoc("deDE"));
    CHECK_FALSE(hasLoc("base"));
    CHECK_FALSE(hasLoc("Win"));
    CHECK_FALSE(hasLoc("OSX"));
    CHECK_FALSE(hasLoc("ALT"));
    CHECK_FALSE(hasLoc("EXP1"));
    CHECK_FALSE(hasLoc("EXP2"));
    // Per-file locale still comes from the Data/<locale>/ directory.
    auto loc = [&](const std::string& name) -> std::string {
        for (const auto& r : m.files) { if (r.relPath == name) { return r.locale; } }
        return "<missing>";
    };
    CHECK(loc("Data/base-Win.MPQ") == "");
    CHECK(loc("Data/enUS/locale-enUS.MPQ") == "enUS");
    CHECK(loc("Data/deDE/expansion2-speech-deDE.MPQ") == "deDE");
}

TEST_CASE("expand_data v3: --locale enUS keeps base + enUS, drops other locales")
{
    const std::string mfil =
        "version=3\n"
        "file=Data/base-Win.MPQ\n\tname=Data/base-Win.MPQ\n\tsize=100\n\tpath=base\n\ttag=Win\n"
        "file=Data/enUS/locale-enUS.MPQ\n\tname=Data/enUS/locale-enUS.MPQ\n\tsize=400\n\tpath=base\n\ttag=enUS\n"
        "file=Data/deDE/locale-deDE.MPQ\n\tname=Data/deDE/locale-deDE.MPQ\n\tsize=500\n\tpath=base\n\ttag=deDE\n";
    wcr::Manifest m = wcr::parse_mfil(mfil, "http://x/");
    auto arts = wcr::expand_data(m, {"enUS"}, wcr::Mode::FullClient, false);
    auto has = [&](const std::string& name) {
        for (const auto& a : arts) { if (a.outName == name) { return true; } }
        return false;
    };
    CHECK(has("Data/base-Win.MPQ"));
    CHECK(has("Data/enUS/locale-enUS.MPQ"));
    CHECK_FALSE(has("Data/deDE/locale-deDE.MPQ"));
}

TEST_CASE("expand_data drops OSX-only archives (Win-only tool)")
{
    const std::string mfil =
        "version=3\n"
        "file=Data/base-Win.MPQ\n\tname=Data/base-Win.MPQ\n\tsize=100\n\tpath=base\n\ttag=Win\n"
        "file=Data/base-OSX.MPQ\n\tname=Data/base-OSX.MPQ\n\tsize=110\n\tpath=base\n\ttag=OSX\n"
        "file=Updates/wow-0-18414-OSX-final.MPQ\n\tname=Updates/wow-0-18414-OSX-final.MPQ\n\tsize=120\n\tpath=base\n\ttag=OSX\n";
    wcr::Manifest m = wcr::parse_mfil(mfil, "http://x/");
    auto arts = wcr::expand_data(m, {}, wcr::Mode::FullClient, false);
    auto has = [&](const std::string& n) {
        for (const auto& a : arts) { if (a.outName == n) { return true; } }
        return false;
    };
    CHECK(has("Data/base-Win.MPQ"));
    CHECK_FALSE(has("Data/base-OSX.MPQ"));
    CHECK_FALSE(has("Updates/wow-0-18414-OSX-final.MPQ"));
}

TEST_CASE("expand_data drops alternate.MPQ for non-Chinese locales, keeps for zhCN")
{
    const std::string mfil =
        "version=3\n"
        "file=Data/alternate.MPQ\n\tname=Data/alternate.MPQ\n\tsize=150\n\tpath=base\n\ttag=ALT\n"
        "file=Data/world.MPQ\n\tname=Data/world.MPQ\n\tsize=200\n\tpath=base\n\ttag=base\n";
    wcr::Manifest m = wcr::parse_mfil(mfil, "http://x/");
    auto altKept = [&](const std::vector<std::string>& locs) {
        auto arts = wcr::expand_data(m, locs, wcr::Mode::FullClient, false);
        for (const auto& a : arts) { if (a.outName == "Data/alternate.MPQ") { return true; } }
        return false;
    };
    CHECK_FALSE(altKept({"enUS"}));
    CHECK(altKept({"zhCN"}));
    CHECK(altKept({"zhTW"}));
}

TEST_CASE("expand_data drops manifest entries with path-traversal or absolute relPaths")
{
    // A malicious or malformed manifest may contain entries that would escape
    // the output directory. expand_data must silently skip such records.
    // Legitimate Data/... entries must still pass through.
    wcr::Manifest m;
    m.version = 2;
    m.dataBaseUrl = "http://x/";
    m.locales = {};

    auto rec = [](const std::string& path) {
        wcr::MfilRecord r;
        r.relPath = path;
        r.size = 100;
        r.locale = "";
        return r;
    };

    // Dangerous paths that must be dropped.
    m.files.push_back(rec("../evil.exe"));
    m.files.push_back(rec("../../etc/passwd"));
    m.files.push_back(rec("Data/../../evil.exe"));
    m.files.push_back(rec("C:/Windows/x.dll"));
    m.files.push_back(rec("/etc/x"));
    // Windows drive-relative path: has a root-name ("C:") but no
    // root-directory and no ".." component, so it slips past the earlier
    // checks. is_safe_relpath must reject it via has_root_name() (Fix 2 / T3).
    m.files.push_back(rec("C:foo"));

    // Safe path that must be kept.
    m.files.push_back(rec("Data/common.MPQ"));

    auto arts = wcr::expand_data(m, {}, wcr::Mode::DataOnly, false);

    // Only the safe Data/common.MPQ record should survive.
    REQUIRE(arts.size() == 1u);
    CHECK(arts[0].outName == "Data/common.MPQ");
}

TEST_CASE("expand_data: first path component must be Data or Updates")
{
    // Even when is_safe_relpath passes (no traversal/absolute/drive), a record
    // whose first path component is not Data or Updates must be dropped: those
    // are the only legitimate top-level subtrees in Blizzard's partial
    // manifest. This blocks a record from colliding with a root binary
    // (Wow.exe, Battle.net.dll, Utils\...). (Fix 1.)
    wcr::Manifest m;
    m.version = 3;
    m.dataBaseUrl = "http://x/";
    m.locales = {};

    auto rec = [](const std::string& path) {
        wcr::MfilRecord r;
        r.relPath = path;
        r.size = 100;
        r.locale = "";
        return r;
    };

    // Root binary masquerading as a manifest record: must be DROPPED.
    m.files.push_back(rec("Wow.exe"));
    // A first component that merely starts with "Data" but is not exactly
    // "Data" must be DROPPED (so "DataXYZ/x" cannot smuggle past a substring
    // check).
    m.files.push_back(rec("DataXYZ/x"));

    // Legitimate top-level subtrees: must be KEPT.
    m.files.push_back(rec("Data/common.MPQ"));
    // MoP 5.4.8 legitimately ships an Updates/ MPQ: a Data-only check would
    // wrongly drop it.
    m.files.push_back(rec("Updates/wow-0-18414-Win-final.MPQ"));

    auto arts = wcr::expand_data(m, {}, wcr::Mode::DataOnly, false);

    auto has = [&](const std::string& name) {
        for (const auto& a : arts) { if (a.outName == name) { return true; } }
        return false;
    };
    CHECK_FALSE(has("Wow.exe"));
    CHECK_FALSE(has("DataXYZ/x"));
    CHECK(has("Data/common.MPQ"));
    CHECK(has("Updates/wow-0-18414-Win-final.MPQ"));
    REQUIRE(arts.size() == 2u);
}

TEST_CASE("expand_data: dot path component is rejected as unsafe")
{
    // N1: is_safe_relpath must reject a "." component (e.g. "Data/./Wow.exe").
    // A single "." is a no-op on the filesystem but has no place in a
    // legitimate Blizzard manifest path; any such record is suspicious and
    // must be silently dropped.
    wcr::Manifest m;
    m.version = 2;
    m.dataBaseUrl = "http://x/";
    m.locales = {};

    auto rec = [](const std::string& path, long long sz = 100) {
        wcr::MfilRecord r;
        r.relPath = path;
        r.size = sz;
        r.locale = "";
        return r;
    };

    // A "." component is not a legitimate manifest path component.
    m.files.push_back(rec("Data/./Wow.exe"));

    // A clean path must still pass through unchanged.
    m.files.push_back(rec("Data/common.MPQ"));

    auto arts = wcr::expand_data(m, {}, wcr::Mode::DataOnly, false);

    // Only Data/common.MPQ survives; Data/./Wow.exe must be dropped.
    REQUIRE(arts.size() == 1u);
    CHECK(arts[0].outName == "Data/common.MPQ");
}

TEST_CASE("expand_data: Data/Updates allowlist handles backslash first component")
{
    // Manifest relPaths can use backslashes. The first-component allowlist must
    // operate on path components, not a raw substring, so "Data\\x" (backslash
    // separator) is recognised as the Data subtree and kept, while a
    // backslash-bearing root binary form is dropped. (Fix 1.)
    wcr::Manifest m;
    m.version = 3;
    m.dataBaseUrl = "http://x/";
    m.locales = {};

    auto rec = [](const std::string& path) {
        wcr::MfilRecord r;
        r.relPath = path;
        r.size = 100;
        r.locale = "";
        return r;
    };

    m.files.push_back(rec("Data\\world.MPQ"));
    m.files.push_back(rec("Updates\\patch.MPQ"));
    // Root binary in backslash form (e.g. "Utils\foo") must be dropped: its
    // first component is "Utils", not Data/Updates.
    m.files.push_back(rec("Utils\\WowBrowserProxy.exe"));

    auto arts = wcr::expand_data(m, {}, wcr::Mode::DataOnly, false);

    auto has = [&](const std::string& name) {
        for (const auto& a : arts) { if (a.outName == name) { return true; } }
        return false;
    };
    CHECK(has("Data\\world.MPQ"));
    CHECK(has("Updates\\patch.MPQ"));
    CHECK_FALSE(has("Utils\\WowBrowserProxy.exe"));
}
