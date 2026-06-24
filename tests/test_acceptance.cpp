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
 * @file test_acceptance.cpp
 * @brief Gated local-acceptance test: reconstructs 5.4.8 binaries from real
 *        Blizzard MPQ fixtures and verifies byte-exact size, PE magic, and
 *        MD5 digest.  Skips cleanly when fixtures are absent (CI / fresh
 *        checkout).
 */

#include "doctest.h"
#include "mpq.h"
#include "ptch.h"
#include "bytes.h"
#include "md5.h"
#include "recipe.h"
#include "fetch.h"
#include <cstdlib>
#include <filesystem>
#include <sys/stat.h>

static bool exists(const char* p)
{
    struct stat s;
    return stat(p, &s) == 0;
}

static const bool fixtures_present =
    exists("fixtures/base-Win.MPQ") &&
    exists("fixtures/wow-0-18414-Win-final.MPQ");

TEST_CASE("byte-exact 5.4.8 Wow.exe from real fixtures" *
          doctest::skip(!fixtures_present))
{
    wcr::MpqArchive base("fixtures/base-Win.MPQ");
    wcr::MpqArchive fin("fixtures/wow-0-18414-Win-final.MPQ");

    wcr::Bytes wow = wcr::apply_ptch(base.extract("Wow.exe"),
                                     fin.extract("pc-game-hdfiles\\Wow.exe"));
    CHECK(wow.size() == 13154864u);
    CHECK(wow[0] == 'M');
    CHECK(wow[1] == 'Z');
    CHECK(wcr::md5_hex(wow) == "24FD2CBB340D57C51B6F7A1C1D60E693");

    wcr::Bytes wow64 = wcr::apply_ptch(
        base.extract("Wow-64.exe"), fin.extract("pc-game-hdfiles\\Wow-64.exe"));
    CHECK(wow64.size() == 20915760u);
    CHECK(wow64[0] == 'M');
    CHECK(wow64[1] == 'Z');
    CHECK(wcr::md5_hex(wow64) == "96EF9239F97336F453562D350C33BCC7");
}

// Reproduce every MpqExtract artifact from the real fixture MPQs exactly the
// way reconstruct() does, and assert the recipe md5 is byte-exact. Proves the
// 9 recovered binaries are official + unmodified.
TEST_CASE("byte-exact 5.4.8 aux binaries from real fixtures" *
          doctest::skip(!fixtures_present))
{
    wcr::MpqArchive base("fixtures/base-Win.MPQ");
    wcr::MpqArchive fin("fixtures/wow-0-18414-Win-final.MPQ");
    const wcr::Recipe& r = wcr::recipe_mop548();
    int checked = 0;
    for (const wcr::Artifact& a : r.artifacts)
    {
        if (a.source != wcr::Source::MpqExtract) { continue; }
        wcr::MpqArchive& src = (a.baseMpqKey == "final") ? fin : base;
        wcr::Bytes data = src.extract(a.basePath);
        if (!a.patchMpqKey.empty())
        {
            data = wcr::apply_ptch(data, fin.extract(a.patchPath));
        }
        CHECK(wcr::md5_hex(data) == a.md5);
        ++checked;
    }
    CHECK(checked == 9);
}

// Opt-in live test: set the WCR_LIVE environment variable to run it. It
// downloads the full 4.3.4 binary set (~46 MB) from Blizzard's live CDN into
// a local directory and asserts every file is byte-exact. Skips by default so
// CI and offline runs stay green.
TEST_CASE("byte-exact 4.3.4 full binary set from live CDN" *
          doctest::skip(std::getenv("WCR_LIVE") == nullptr))
{
    const wcr::Recipe& r = wcr::recipe_cata434();
    const std::string out = "live_4.3.4_out";
    wcr::reconstruct(r, out);
    for (const wcr::Artifact& a : r.artifacts)
    {
        wcr::Bytes f = wcr::read_file(out + "/" + a.outName);
        CHECK(wcr::md5_hex(f) == a.md5);
    }
    // Verifies the scratch zip was cleaned up: fetch.cpp downloads the zip to
    // outDir then removes it after extraction — this asserts the cleanup ran.
    CHECK(!std::filesystem::exists(out + "/WoWLive-64-Win-15595.zip"));
}

// Regression: reconstruct() must close the source-MPQ handles before the
// build-scratch cleanup, or Windows leaves the (still-open) .MPQ in the root.
TEST_CASE("reconstruct removes the source MPQ from the root on success" *
          doctest::skip(!fixtures_present))
{
    namespace fs = std::filesystem;
    fs::path out = fs::temp_directory_path() / "wcr_recon_cleanup";
    fs::remove_all(out);
    auto fileUrl = [](const fs::path& p)
    {
        return std::string("file:///") + p.generic_string();
    };
    wcr::Recipe r;
    r.version = "5.4.8";
    r.build = "18414";
    wcr::MpqSource base;
    base.key = "base";
    base.url = fileUrl(fs::absolute("fixtures/base-Win.MPQ"));
    r.mpqs.push_back(base);
    wcr::Artifact a;
    a.outName = "WowError.exe";
    a.md5 = "EFEF12D480F64B850A6B1C8D5850A484";
    a.source = wcr::Source::MpqExtract;
    a.baseMpqKey = "base";
    a.basePath = "WowError.exe";
    r.artifacts.push_back(a);
    wcr::reconstruct(r, out.string(), wcr::ReconstructOpts{});
    CHECK(fs::exists(out / "WowError.exe"));       // artifact was extracted
    CHECK_FALSE(fs::exists(out / "base-Win.MPQ")); // source scratch cleaned up
    fs::remove_all(out);
}
