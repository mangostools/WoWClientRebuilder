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
#include "assemble.h"
#include "fetch.h"
#include "mfil.h"
#include "recipe.h"
#include "datarecipe.h"
#include <stdexcept>
#include <string>
#include <vector>

using namespace wcr;

/// Two synthetic PlainUrl data artifacts standing in for expand_data output.
static std::vector<Artifact> fake_data()
{
    Artifact a;
    a.source = Source::PlainUrl;
    a.outName = "Data/enUS/locale-enUS.MPQ";
    a.url = "http://x/Data/enUS/locale-enUS.MPQ";
    a.size = 10;
    a.locale = "enUS";
    Artifact b;
    b.source = Source::PlainUrl;
    b.outName = "Data/common.MPQ";
    b.url = "http://x/Data/common.MPQ";
    b.size = 20;
    return {a, b};
}

TEST_CASE("pointer_text_from_recipe yields parseable WoW.mfil pointer")
{
    const Recipe& r = recipe_cata434();
    std::string text = pointer_text_from_recipe(r);
    Pointer p = parse_pointer(text);
    CHECK(p.dataBaseUrl ==
          "http://dist.blizzard.com.edgesuite.net/"
          "wow-pod-retail/EU/15050.direct/");
    CHECK(p.partialName ==
          "wow-15595-0C3502F50D17376754B9E9CB0109F4C5.mfil");
}

TEST_CASE("assemble_recipe FullClient appends data after binaries")
{
    const Recipe& base = recipe_cata434();
    size_t baseCount = base.artifacts.size();
    std::vector<Artifact> data = fake_data();
    Recipe out = assemble_recipe(base, data, Mode::FullClient);
    CHECK(out.version == base.version);
    CHECK(out.artifacts.size() == baseCount + data.size());
    // data is appended at the tail, after the binary artifacts.
    CHECK(out.artifacts[baseCount].outName == "Data/enUS/locale-enUS.MPQ");
}

TEST_CASE("assemble_recipe DataOnly carries only the data artifacts")
{
    const Recipe& base = recipe_cata434();
    std::vector<Artifact> data = fake_data();
    Recipe out = assemble_recipe(base, data, Mode::DataOnly);
    CHECK(out.artifacts.size() == data.size());
    CHECK(out.artifacts[0].outName == "Data/enUS/locale-enUS.MPQ");
    // base identity fields are still carried for downstream consumers.
    CHECK(out.build == base.build);
    CHECK(out.repairBase == base.repairBase);
}

/// A minimal base recipe with a single root-binary artifact, plus one
/// data artifact whose outName the caller supplies. Used to drive the
/// outName-collision guard (Fix 3).
static Recipe base_with_root_binary()
{
    Recipe base;
    base.version = "4.3.4";
    base.build = "15595";
    Artifact wow;
    wow.source = Source::RepairMd5;
    wow.outName = "Wow.exe";
    base.artifacts.push_back(wow);
    return base;
}

TEST_CASE("assemble_recipe treats doubled separators as the same path (ntfs_normalize collapse)")
{
    // N2: ntfs_normalize must collapse consecutive '/' into a single '/'.
    // NTFS collapses repeated separators, so "Data//x" and "Data/x" denote the
    // same file on disk. assemble_recipe must detect this as a collision and
    // throw rather than writing two artifacts to the same on-disk destination.
    Recipe base;
    base.version = "4.3.4";
    base.build = "15595";

    // A base artifact at the canonical path.
    Artifact baseArt;
    baseArt.source = Source::RepairMd5;
    baseArt.outName = "Data/common.MPQ";
    base.artifacts.push_back(baseArt);

    // A data artifact with a doubled separator: after normalization this is
    // identical to "data/common.mpq" and must collide with the base artifact.
    Artifact data;
    data.source = Source::PlainUrl;
    data.outName = "Data//common.MPQ";

    CHECK_THROWS_AS(assemble_recipe(base, {data}, Mode::FullClient),
                    std::runtime_error);
}

TEST_CASE("assemble_recipe rejects a data artifact colliding with a base binary")
{
    // Defense-in-depth behind the Data/Updates allowlist: a tampered manifest
    // record whose outName NTFS-normalizes to the same file as a base binary
    // must fail closed (throw), never silently overwrite the binary. (Fix 3.)

    SUBCASE("exact lowercase collision")
    {
        Recipe base = base_with_root_binary();
        Artifact d;
        d.source = Source::PlainUrl;
        d.outName = "wow.exe"; // NTFS folds case -> collides with Wow.exe
        CHECK_THROWS_AS(assemble_recipe(base, {d}, Mode::FullClient),
                        std::runtime_error);
    }

    SUBCASE("trailing-space collision")
    {
        Recipe base = base_with_root_binary();
        Artifact d;
        d.source = Source::PlainUrl;
        d.outName = "Wow.exe "; // NTFS strips trailing space -> collides
        CHECK_THROWS_AS(assemble_recipe(base, {d}, Mode::FullClient),
                        std::runtime_error);
    }

    SUBCASE("trailing-dot collision")
    {
        Recipe base = base_with_root_binary();
        Artifact d;
        d.source = Source::PlainUrl;
        d.outName = "Wow.exe."; // NTFS strips trailing dot -> collides
        CHECK_THROWS_AS(assemble_recipe(base, {d}, Mode::FullClient),
                        std::runtime_error);
    }

    SUBCASE("normal data artifact does not collide")
    {
        Recipe base = base_with_root_binary();
        Artifact d;
        d.source = Source::PlainUrl;
        d.outName = "Data/common.MPQ";
        CHECK_NOTHROW(assemble_recipe(base, {d}, Mode::FullClient));
    }
}

TEST_CASE("pointer_text_for_region: fetch and write name the same region")
{
    // The manifest's records carry region-specific sizes (MoP's
    // Updates/wow-0-18414-Win-final.MPQ is 21729424 in EU, 21729944 in NA), so
    // the manifest fetched here must be the same region whose URLs the run
    // downloads from. Fetching EU's manifest for an NA run paired an EU size
    // with an NA URL and failed on the last of 394 records, after ~21.7 GB.
    // The written WoW.mfil is the client's own copy of that pointer: if the two
    // ever disagree, one of them is wrong.
    const Recipe& base = recipe_mop548();
    const char* regions[] = {"EU", "NA"};
    const char* names[] = {"E68C6C849BBD16D2A8A153AFC865062F",
                           "447E3E618F731CCBF4F7D2C4E56C5644"};
    for (int i = 0; i < 2; ++i)
    {
        const std::string region = regions[i];
        const std::string fetched = pointer_text_for_region(base, region);
        Pointer ptr = parse_pointer(fetched);
        CHECK(ptr.dataBaseUrl.find("/" + region + "/") != std::string::npos);
        CHECK(ptr.partialName == std::string("wow-18414-") + names[i] +
                                     ".mfil");

        Recipe run = base;
        apply_region_to_recipe(run, base, region);
        std::string written;
        for (const Artifact& a : run.artifacts)
        {
            if (a.outName == "WoW.mfil")
            {
                written = a.content;
            }
        }
        CHECK(written == fetched);
    }
}

TEST_CASE("pointer_text_for_region: no region-locked name leaves the pointer")
{
    // 4.3.4 has no regionManifests, so its pointer is unchanged for either
    // region and main.cpp's dataBaseUrl swap remains the only region switch.
    const Recipe& cata = recipe_cata434();
    const std::string plain = pointer_text_from_recipe(cata);
    CHECK(pointer_text_for_region(cata, "EU") == plain);
    CHECK(pointer_text_for_region(cata, "NA") == plain);
}
