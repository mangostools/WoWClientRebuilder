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
 * @file test_recipe.cpp
 * @brief Unit tests for wcr::recipe_cata434(), wcr::recipe_mop548(),
 *        and wcr::find_recipe().
 */

#include "doctest.h"
#include "md5.h"
#include "recipe.h"
#include <string>

TEST_CASE("find_recipe selects by version and rejects unknown")
{
    const wcr::Recipe* cata = wcr::find_recipe("4.3.4");
    REQUIRE(cata != nullptr);
    CHECK(cata->version == "4.3.4");
    CHECK(cata->build == "15595");

    const wcr::Recipe* mop = wcr::find_recipe("5.4.8");
    REQUIRE(mop != nullptr);
    CHECK(mop->version == "5.4.8");
    CHECK(mop->build == "18414");
    REQUIRE(mop->mpqs.size() == 2u);
    // Built out from the binaries-only stub to a full recipe: 2 MpqPtch exes +
    // 11 /repair binaries + 1 Generated WoW.mfil Data pointer + 9 MpqExtract
    // aux binaries = 23 artifacts.
    REQUIRE(mop->artifacts.size() == 23u);
    CHECK(mop->repairBase ==
          "http://dist.blizzard.com.edgesuite.net/repair/wow");
    CHECK(mop->artifacts[0].source == wcr::Source::MpqPtch);
    CHECK(mop->artifacts[1].source == wcr::Source::MpqPtch);
    int mopRepairCount = 0;
    bool sawMopWow = false;
    bool sawMopWow64 = false;
    bool sawMopMfil = false;
    for (const wcr::Artifact& a : mop->artifacts)
    {
        if (a.source == wcr::Source::RepairMd5)
        {
            ++mopRepairCount;
        }
        if (a.outName == "Wow.exe")
        {
            sawMopWow = true;
            CHECK(a.md5 == "24FD2CBB340D57C51B6F7A1C1D60E693");
        }
        if (a.outName == "Wow-64.exe")
        {
            sawMopWow64 = true;
            CHECK(a.md5 == "96EF9239F97336F453562D350C33BCC7");
        }
        if (a.outName == "WoW.mfil")
        {
            sawMopMfil = true;
            CHECK(a.source == wcr::Source::Generated);
            CHECK(a.content.find(
                      "wow-18414-E68C6C849BBD16D2A8A153AFC865062F.mfil") !=
                  std::string::npos);
            CHECK(a.content.find("15890.direct") != std::string::npos);
        }
    }
    CHECK(mopRepairCount == 11);
    CHECK(sawMopWow);
    CHECK(sawMopWow64);
    CHECK(sawMopMfil);

    CHECK(wcr::find_recipe("9.9.9") == nullptr);
}

TEST_CASE("4.3.4 recipe is the complete verified 16-file binary set")
{
    const wcr::Recipe& r = wcr::recipe_cata434();
    // 13 /repair binaries + 3 live64 zip members + 1 Generated WoW.mfil = 17.
    // ijl15.dll + unicows.dll were recovered from the base-Win.MPQ binary
    // sweep -- legacy libs Blizzard kept in /repair (same md5s MoP uses).
    CHECK(r.artifacts.size() == 17u);
    CHECK(r.repairBase ==
          "http://dist.blizzard.com.edgesuite.net/repair/wow");
    REQUIRE(r.zips.size() == 1u);
    CHECK(r.zips[0].key == "live64");

    int repair = 0;
    int zipmem = 0;
    int generated = 0;
    for (const wcr::Artifact& a : r.artifacts)
    {
        CHECK(a.md5.size() == 32u);
        if (a.source == wcr::Source::RepairMd5)
        {
            ++repair;
        }
        if (a.source == wcr::Source::ZipMember)
        {
            ++zipmem;
            CHECK(a.zipKey == "live64");
        }
        if (a.source == wcr::Source::Generated)
        {
            ++generated;
        }
    }
    CHECK(repair == 13);
    CHECK(zipmem == 3);
    CHECK(generated == 1);

    // Spot-check the two anchor binaries.
    bool sawWow = false;
    bool sawWow64 = false;
    bool sawMfil = false;
    bool sawLauncher = false;
    bool sawIjl15 = false;
    bool sawUnicows = false;
    for (const wcr::Artifact& a : r.artifacts)
    {
        if (a.outName == "Wow.exe")
        {
            sawWow = true;
            CHECK(a.source == wcr::Source::RepairMd5);
            CHECK(a.md5 == "DE5A2E274F2D3F2B89A2E6EC9CD8FD2A");
        }
        if (a.outName == "Wow-64.exe")
        {
            sawWow64 = true;
            CHECK(a.source == wcr::Source::ZipMember);
            CHECK(a.md5 == "5ACD2205377352083D2D98B89F48B602");
        }
        if (a.outName == "WoW.mfil")
        {
            sawMfil = true;
            CHECK(a.source == wcr::Source::Generated);
            CHECK(a.md5 == "F8E7D7BA6CDE053B1A9F85BD36980A72");
            // Lock the embedded literal to the exact bytes.
            wcr::Bytes contentBytes(a.content.begin(), a.content.end());
            CHECK(wcr::md5_hex(contentBytes) ==
                  "F8E7D7BA6CDE053B1A9F85BD36980A72");
        }
        if (a.outName == "Launcher.exe")
        {
            sawLauncher = true;
            CHECK(a.source == wcr::Source::RepairMd5);
            CHECK(a.md5 == "C7C7121E1DD819088403F514FEBD06BA");
        }
        if (a.outName == "ijl15.dll")
        {
            sawIjl15 = true;
            CHECK(a.source == wcr::Source::RepairMd5);
            CHECK(a.md5 == "1AA06C81A0621E277E755B965B5E4B5F");
        }
        if (a.outName == "unicows.dll")
        {
            sawUnicows = true;
            CHECK(a.source == wcr::Source::RepairMd5);
            CHECK(a.md5 == "E1102CEDF0C818984C2ACA2A666D4C5F");
        }
    }
    CHECK(sawWow);
    CHECK(sawWow64);
    CHECK(sawMfil);
    CHECK(sawLauncher);
    CHECK(sawIjl15);
    CHECK(sawUnicows);
}

TEST_CASE("recipe_mop548 recovers the 9 aux binaries via MpqExtract")
{
    const wcr::Recipe& r = wcr::recipe_mop548();
    // 2 MpqPtch exes + 11 /repair + 1 WoW.mfil + 9 MpqExtract = 23.
    REQUIRE(r.artifacts.size() == 23u);
    int extract = 0;
    bool sawSystemSurvey = false, sawUtilsLibcef = false, sawMovieProxy = false;
    for (const wcr::Artifact& a : r.artifacts)
    {
        if (a.source != wcr::Source::MpqExtract) { continue; }
        ++extract;
        CHECK(a.md5.size() == 32u);
        if (a.outName == "SystemSurvey.exe")
        {
            sawSystemSurvey = true;
            CHECK(a.baseMpqKey == "final");
            CHECK(a.basePath == "pc-game-hdfiles\\SystemSurvey.exe");
            CHECK(a.patchMpqKey.empty());
            CHECK(a.md5 == "5E9E75B78AF45FE376022B4D61767109");
        }
        if (a.outName == "Utils\\libcef.dll")
        {
            sawUtilsLibcef = true;
            CHECK(a.md5 == "EC729CC8FEADAB8C4C2108D83E8BFE27");
        }
        if (a.outName == "MovieProxy.exe")
        {
            sawMovieProxy = true;
            CHECK(a.baseMpqKey == "base");
            CHECK(a.patchMpqKey == "final"); // the only patched one
            CHECK(a.patchPath == "pc-game-hdfiles\\MovieProxy.exe");
            CHECK(a.md5 == "988F5E3A0FEB790862F69DE423F0E67A");
        }
    }
    CHECK(extract == 9);
    CHECK(sawSystemSurvey);
    CHECK(sawUtilsLibcef);
    CHECK(sawMovieProxy);
}

TEST_CASE("Artifact has size/locale/optional fields with defaults")
{
    wcr::Artifact a;
    CHECK(a.size == -1);
    CHECK(a.locale.empty());
    CHECK(a.optional == false);

    a.size = 4096;
    a.locale = "enUS";
    a.optional = true;
    CHECK(a.size == 4096);
    CHECK(a.locale == "enUS");
    CHECK(a.optional == true);
}

TEST_CASE("recipe_mop548 carries the region->manifest-name map")
{
    const wcr::Recipe& r = wcr::recipe_mop548();
    auto find = [&](const std::string& region) -> std::string {
        for (const auto& pr : r.regionManifests) { if (pr.first == region) { return pr.second; } }
        return "";
    };
    CHECK(find("EU") == "E68C6C849BBD16D2A8A153AFC865062F");
    CHECK(find("NA") == "447E3E618F731CCBF4F7D2C4E56C5644");
    // 4.3.4's manifest is region-agnostic -> no map.
    CHECK(wcr::recipe_cata434().regionManifests.empty());
}

TEST_CASE("region_rewrite_mfil only replaces the manifest_partial= occurrence, not earlier ones")
{
    // N4: region_rewrite_mfil uses out.find(other) which finds the FIRST
    // occurrence of the hash in the content.  If the hash also appeared in the
    // location= URL the wrong occurrence would be rewritten.  The fix anchors
    // the search to start from the "manifest_partial=" position.
    //
    // Craft a WoW.mfil content where the "EU" hash (other) appears TWICE:
    // once in the location URL and once as the manifest_partial= value.
    const std::string euHash   = "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA1";
    const std::string naHash   = "BBBBBBBBBBBBBBBBBBBBBBBBBBBBBB2B";
    // NB: both hashes are 32 chars — the function treats them as opaque strings
    // (it does no length or hex check itself), so any 32-char token is fine.

    // Content: EU hash injected into the URL, and then in manifest_partial=.
    const std::string content =
        "version=2\r\n"
        "server=akamai\r\n"
        "\tlocation=http://example.com/" + euHash + "/EU/15890.direct/\r\n"
        "manifest_partial=wow-18414-" + euHash + ".mfil\r\n";

    std::vector<std::pair<std::string, std::string>> regionManifests = {
        {"EU", euHash},
        {"NA", naHash}
    };

    std::string na = wcr::region_rewrite_mfil(content, regionManifests, "NA");

    // The manifest_partial= value must be rewritten to the NA hash.
    CHECK(na.find("manifest_partial=wow-18414-" + naHash + ".mfil") !=
          std::string::npos);

    // The hash in the location URL must be left UNTOUCHED (it is not the
    // manifest name; only the manifest_partial= occurrence should change).
    CHECK(na.find("http://example.com/" + euHash + "/") != std::string::npos);

    // The EU hash must not appear in manifest_partial= after rewrite.
    CHECK(na.find("manifest_partial=wow-18414-" + euHash + ".mfil") ==
          std::string::npos);
}

TEST_CASE("region_rewrite_mfil swaps EU->NA segment + manifest name")
{
    const wcr::Recipe& r = wcr::recipe_mop548();
    std::string eu;
    for (const auto& a : r.artifacts)
    {
        if (a.outName == "WoW.mfil") { eu = a.content; }
    }
    REQUIRE(!eu.empty());
    std::string na = wcr::region_rewrite_mfil(eu, r.regionManifests, "NA");
    CHECK(na.find("/NA/15890.direct/") != std::string::npos);
    CHECK(na.find("447E3E618F731CCBF4F7D2C4E56C5644") != std::string::npos);
    CHECK(na.find("E68C6C849BBD16D2A8A153AFC865062F") == std::string::npos);
    // EU returns the content unchanged.
    CHECK(wcr::region_rewrite_mfil(eu, r.regionManifests, "EU") == eu);
}
