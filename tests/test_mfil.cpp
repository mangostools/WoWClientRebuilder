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
 * @file test_mfil.cpp
 * @brief Tests for WoW.mfil pointer parsing and partial manifest parsing.
 */

#include "doctest.h"
#include "mfil.h"
#include "bytes.h"
#include "md5.h"
#include <filesystem>
#include <string>

TEST_CASE("parse_pointer extracts location and manifest_partial")
{
    const std::string pointer =
        "version=2\r\n"
        "server=akamai\r\n"
        "\tlocation=http://dist.blizzard.com.edgesuite.net/"
        "wow-pod-retail/EU/15050.direct/\r\n"
        "manifest_partial=wow-15595-0C3502F50D17376754B9E9CB0109F4C5.mfil\r\n";

    wcr::Pointer p = wcr::parse_pointer(pointer);

    CHECK(p.dataBaseUrl ==
          "http://dist.blizzard.com.edgesuite.net/"
          "wow-pod-retail/EU/15050.direct/");
    CHECK(p.partialName ==
          "wow-15595-0C3502F50D17376754B9E9CB0109F4C5.mfil");
}

// Shared synthetic v2 partial manifest used across the parse_mfil tests.
// Mirrors the REAL v2 partial .mfil structure: all serverpath= declarations
// come first (advertising available locales), then file records each carry
// their own path= field (base or locale_<X>).
static std::string sample_mfil()
{
    return
        "version=2\r\n"
        "build=15595\r\n"
        // Serverpath declarations (upfront, as in the real manifest).
        "serverpath=base\r\n"
        "\tpath=\r\n"
        "serverpath=locale_frFR\r\n"
        "\tpath=\r\n"
        "serverpath=locale_enUS\r\n"
        "\tpath=\r\n"
        // File records: each carries its own path= field.
        "file=Data/art.MPQ\r\n"
        "name=Data/art.MPQ\r\n"
        "size=1048576\r\n"
        "fileversion=15595\r\n"
        "path=base\r\n"
        "file=Data/wow-update-base-15595.MPQ\r\n"
        "name=Data/wow-update-base-15595.MPQ\r\n"
        "size=2048\r\n"
        "fileversion=15595\r\n"
        "path=base\r\n"
        "file=Data/alternate.MPQ\r\n"
        "name=Data/alternate.MPQ\r\n"
        "size=4096\r\n"
        "path=locale_frFR\r\n"
        "file=Data/enUS/locale-enUS.MPQ\r\n"
        "name=Data/enUS/locale-enUS.MPQ\r\n"
        "size=8192\r\n"
        "fileversion=15595\r\n"
        "path=locale_enUS\r\n";
}

TEST_CASE("parse_mfil emits every record with name + size (incl wow-update)")
{
    wcr::Manifest m = wcr::parse_mfil(sample_mfil(),
                                      "http://example/EU/15050.direct/");

    CHECK(m.version == 2);
    CHECK(m.dataBaseUrl == "http://example/EU/15050.direct/");
    REQUIRE(m.files.size() == 4);

    // Records are emitted in source order.
    CHECK(m.files[0].relPath == "Data/art.MPQ");
    CHECK(m.files[0].size == 1048576);

    // wow-update-* must be KEPT (no whitelist).
    CHECK(m.files[1].relPath == "Data/wow-update-base-15595.MPQ");
    CHECK(m.files[1].size == 2048);

    CHECK(m.files[2].relPath == "Data/alternate.MPQ");
    CHECK(m.files[3].relPath == "Data/enUS/locale-enUS.MPQ");
    CHECK(m.files[3].size == 8192);
}

TEST_CASE("parse_mfil assigns locale from each file's own path= field")
{
    wcr::Manifest m = wcr::parse_mfil(sample_mfil(),
                                      "http://example/EU/15050.direct/");

    // Files with path=base are locale-independent.
    CHECK(m.files[0].relPath == "Data/art.MPQ");
    CHECK(m.files[0].locale == "");
    CHECK(m.files[1].relPath == "Data/wow-update-base-15595.MPQ");
    CHECK(m.files[1].locale == "");

    // The locale MPQ carries path=locale_enUS -> tagged enUS.
    CHECK(m.files[3].relPath == "Data/enUS/locale-enUS.MPQ");
    CHECK(m.files[3].locale == "enUS");

    // Manifest.locales is populated from serverpath= declarations.
    bool hasEn = false;
    bool hasFr = false;
    for (const std::string& l : m.locales)
    {
        if (l == "enUS")
        {
            hasEn = true;
        }
        if (l == "frFR")
        {
            hasFr = true;
        }
    }
    CHECK(hasEn);
    CHECK(hasFr);
}

TEST_CASE("parse_mfil keys alternate.MPQ to its own path tag locale")
{
    wcr::Manifest m = wcr::parse_mfil(sample_mfil(),
                                      "http://example/EU/15050.direct/");

    // alternate.MPQ is the third record; its locale comes from its own
    // per-file path=locale_frFR, not from any serverpath section.
    REQUIRE(m.files.size() == 4);
    CHECK(m.files[2].relPath == "Data/alternate.MPQ");
    CHECK(m.files[2].locale == "frFR");
}

TEST_CASE("parse_mfil regression: per-file path= wins over last serverpath")
{
    // Reproduces the real v2 bug: serverpaths are declared upfront and enCN is
    // LAST, but the file carries path=locale_enUS.  The file must be tagged
    // "enUS", not "enCN" (which the old currentLocale tracking would produce).
    const std::string manifest =
        "version=2\r\n"
        // Serverpaths declared upfront — enUS is in the middle, enCN is last.
        "serverpath=base\r\n"
        "\tpath=\r\n"
        "serverpath=locale_enUS\r\n"
        "\tpath=\r\n"
        "serverpath=locale_deDE\r\n"
        "\tpath=\r\n"
        "serverpath=locale_enCN\r\n"
        "\tpath=\r\n"
        // File records: each carries its own path=.
        "file=Data/art.MPQ\r\n"
        "name=Data/art.MPQ\r\n"
        "size=3943419041\r\n"
        "path=base\r\n"
        "file=Data/enUS/locale-enUS.MPQ\r\n"
        "name=Data/enUS/locale-enUS.MPQ\r\n"
        "size=448138121\r\n"
        "path=locale_enUS\r\n"
        "file=Data/alternate.MPQ\r\n"
        "name=Data/alternate.MPQ\r\n"
        "size=4096\r\n"
        "path=locale_enUS\r\n";

    wcr::Manifest m = wcr::parse_mfil(manifest,
                                      "http://example/EU/15050.direct/");

    REQUIRE(m.files.size() == 3u);

    // Base file: path=base -> locale "".
    CHECK(m.files[0].relPath == "Data/art.MPQ");
    CHECK(m.files[0].locale == "");

    // enUS locale file: path=locale_enUS -> locale "enUS", NOT "enCN".
    CHECK(m.files[1].relPath == "Data/enUS/locale-enUS.MPQ");
    CHECK(m.files[1].locale == "enUS");

    // alternate.MPQ: path=locale_enUS -> tagged "enUS".
    CHECK(m.files[2].relPath == "Data/alternate.MPQ");
    CHECK(m.files[2].locale == "enUS");

    // Advertised locales collected from serverpath= declarations.
    bool hasEnUS = false;
    bool hasEnCN = false;
    for (const std::string& l : m.locales)
    {
        if (l == "enUS")
        {
            hasEnUS = true;
        }
        if (l == "enCN")
        {
            hasEnCN = true;
        }
    }
    CHECK(hasEnUS);
    CHECK(hasEnCN);
}

TEST_CASE("fetch_manifest downloads the partial and parses it")
{
    namespace fs = std::filesystem;

    // Write the fixture manifest into a temp dir and form a file:// pointer
    // whose dataBaseUrl + partialName resolves to it.
    fs::path dir = fs::temp_directory_path() / "wcr_mfil_fetch";
    fs::create_directories(dir);
    const std::string partial = "wow-15595-test.mfil";
    fs::path manifestPath = dir / partial;

    const std::string body = sample_mfil();
    wcr::write_file(manifestPath.string(),
                    wcr::Bytes(body.begin(), body.end()));

    // file:// URL with forward slashes; ensure a trailing slash on the base.
    std::string base = "file:///" + dir.generic_string();
    if (!base.empty() && base.back() != '/')
    {
        base += '/';
    }

    wcr::Pointer p;
    p.dataBaseUrl = base;
    p.partialName = partial;

    // "wow-15595-test.mfil" carries no content-hash in its name, so pass
    // requireHash=false to keep the fixture working (synthetic test manifest).
    wcr::Manifest m = wcr::fetch_manifest(p, false);

    CHECK(m.version == 2);
    CHECK(m.dataBaseUrl == base);
    REQUIRE(m.files.size() == 4);
    CHECK(m.files[0].relPath == "Data/art.MPQ");
    CHECK(m.files[3].locale == "enUS");

    std::error_code ec;
    fs::remove_all(dir, ec);
}

TEST_CASE("fetch_manifest authenticates the body against the filename hash")
{
    // Security (B1): the partial-manifest filename is content-addressed
    // (wow-<build>-<MD5>.mfil) and comes from our PINNED recipe, so it is a
    // forge-proof trust anchor. fetch_manifest must verify md5(body) == that
    // hash and fail closed on any mismatch (MITM / tampered CDN edge).
    namespace fs = std::filesystem;

    fs::path dir = fs::temp_directory_path() / "wcr_mfil_hashcheck";
    fs::remove_all(dir);
    fs::create_directories(dir);

    const std::string body = sample_mfil();
    const std::string realMd5 =
        wcr::md5_hex(wcr::Bytes(body.begin(), body.end()));

    auto baseUrl = [&]()
    {
        std::string b = "file:///" + dir.generic_string();
        if (!b.empty() && b.back() != '/')
        {
            b += '/';
        }
        return b;
    };

    SUBCASE("matching hash parses normally")
    {
        // Name the fixture with the REAL md5 of its body -> check passes.
        const std::string partial = "wow-15595-" + realMd5 + ".mfil";
        wcr::write_file((dir / partial).string(),
                        wcr::Bytes(body.begin(), body.end()));

        wcr::Pointer p;
        p.dataBaseUrl = baseUrl();
        p.partialName = partial;

        wcr::Manifest m;
        REQUIRE_NOTHROW(m = wcr::fetch_manifest(p));
        CHECK(m.version == 2);
        REQUIRE(m.files.size() == 4);
        CHECK(m.files[3].locale == "enUS");
    }

    SUBCASE("mismatched hash throws (tampered body)")
    {
        // A valid-shaped but WRONG hash in the filename: the body's real md5
        // cannot match it, so the fetch must fail closed.
        const std::string wrongMd5 = "00000000000000000000000000000000";
        REQUIRE(wrongMd5 != realMd5);
        const std::string partial = "wow-15595-" + wrongMd5 + ".mfil";
        wcr::write_file((dir / partial).string(),
                        wcr::Bytes(body.begin(), body.end()));

        wcr::Pointer p;
        p.dataBaseUrl = baseUrl();
        p.partialName = partial;

        CHECK_THROWS_AS(wcr::fetch_manifest(p), std::runtime_error);
    }

    std::error_code ec;
    fs::remove_all(dir, ec);
}

// ---------------------------------------------------------------------------
// N5: fail-closed manifest auth for real recipes
// ---------------------------------------------------------------------------

TEST_CASE("fetch_manifest with hash-less name throws when requireHash=true")
{
    // N5: A hash-less manifest name (e.g. "wow-15595-test.mfil") must cause
    // fetch_manifest to throw when requireHash=true (the default), so that a
    // future real recipe with a malformed name fails loudly rather than silently
    // skipping the content-hash check.
    namespace fs = std::filesystem;

    fs::path dir = fs::temp_directory_path() / "wcr_mfil_requirehash";
    fs::remove_all(dir);
    fs::create_directories(dir);

    const std::string body = sample_mfil();
    // A hash-less name: no 32-hex suffix before ".mfil".
    const std::string partial = "wow-15595-nohash.mfil";
    wcr::write_file((dir / partial).string(),
                    wcr::Bytes(body.begin(), body.end()));

    std::string base = "file:///" + dir.generic_string();
    if (!base.empty() && base.back() != '/') { base += '/'; }

    wcr::Pointer p;
    p.dataBaseUrl = base;
    p.partialName = partial;

    // Default (requireHash=true): must throw because the name carries no hash.
    CHECK_THROWS_AS(wcr::fetch_manifest(p), std::runtime_error);

    // requireHash=false: must parse normally (synthetic fixture use case).
    wcr::Manifest m;
    REQUIRE_NOTHROW(m = wcr::fetch_manifest(p, false));
    CHECK(m.version == 2);
    REQUIRE(m.files.size() == 4);

    std::error_code ec;
    fs::remove_all(dir, ec);
}

// ---------------------------------------------------------------------------
// B2 security: manifest-advertised locales must be validated (path-traversal)
// ---------------------------------------------------------------------------

TEST_CASE("parse_mfil rejects poisoned v2 serverpath locale (path-traversal)")
{
    // A v2 manifest advertising a serverpath=locale_../../etc traversal token.
    // The poisoned locale must NOT appear in m.locales; clean locales must be kept.
    const std::string manifest =
        "version=2\r\n"
        "build=15595\r\n"
        "serverpath=locale_enUS\r\n"
        "\tpath=\r\n"
        "serverpath=locale_../../etc\r\n"
        "\tpath=\r\n"
        "serverpath=locale_deDE\r\n"
        "\tpath=\r\n"
        "file=Data/enUS/locale-enUS.MPQ\r\n"
        "name=Data/enUS/locale-enUS.MPQ\r\n"
        "size=8192\r\n"
        "path=locale_enUS\r\n";

    wcr::Manifest m = wcr::parse_mfil(manifest, "http://example/");

    bool hasEnUS = false;
    bool hasDeDE = false;
    bool hasPoisoned = false;
    for (const std::string& l : m.locales)
    {
        if (l == "enUS")
        {
            hasEnUS = true;
        }
        if (l == "deDE")
        {
            hasDeDE = true;
        }
        if (l == "../../etc")
        {
            hasPoisoned = true;
        }
    }
    CHECK(hasEnUS);
    CHECK(hasDeDE);
    CHECK(!hasPoisoned);
}

TEST_CASE("parse_mfil rejects poisoned v3 tag locale (path-traversal)")
{
    // A v3 manifest advertising a tag=../../etc traversal token.
    // The poisoned locale must NOT appear in m.locales; clean locales must be kept.
    const std::string manifest =
        "version=3\r\n"
        "build=18414\r\n"
        "file=Data/enUS/locale-enUS.MPQ\r\n"
        "name=Data/enUS/locale-enUS.MPQ\r\n"
        "size=8192\r\n"
        "tag=enUS\r\n"
        "file=Data/deDE/locale-deDE.MPQ\r\n"
        "name=Data/deDE/locale-deDE.MPQ\r\n"
        "size=4096\r\n"
        "tag=../../etc\r\n"
        "file=Data/frFR/locale-frFR.MPQ\r\n"
        "name=Data/frFR/locale-frFR.MPQ\r\n"
        "size=2048\r\n"
        "tag=frFR\r\n";

    wcr::Manifest m = wcr::parse_mfil(manifest, "http://example/");

    bool hasEnUS = false;
    bool hasFrFR = false;
    bool hasPoisoned = false;
    for (const std::string& l : m.locales)
    {
        if (l == "enUS")
        {
            hasEnUS = true;
        }
        if (l == "frFR")
        {
            hasFrFR = true;
        }
        if (l == "../../etc")
        {
            hasPoisoned = true;
        }
    }
    CHECK(hasEnUS);
    CHECK(hasFrFR);
    CHECK(!hasPoisoned);
}
