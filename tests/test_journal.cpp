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
 * @file test_journal.cpp
 * @brief Resume journal + region failover unit tests.
 */

#include "doctest.h"
#include "journal.h"
#include "fetch.h"
#include "recipe.h"
#include <filesystem>
#include <fstream>

TEST_CASE("journal_round_trip")
{
    namespace fs = std::filesystem;
    fs::path dir = fs::temp_directory_path() / "wcr_journal_rt";
    fs::remove_all(dir);
    fs::create_directories(dir);

    wcr::Journal j = wcr::load_journal(dir.string());
    CHECK(j.done.empty());
    CHECK_FALSE(wcr::is_done(j, "Data/wow-update-15595.MPQ"));

    wcr::mark_done(j, "Data/wow-update-15595.MPQ");
    wcr::mark_done(j, "Data/enUS/locale-enUS.MPQ");
    CHECK(wcr::is_done(j, "Data/wow-update-15595.MPQ"));

    wcr::Journal reloaded = wcr::load_journal(dir.string());
    CHECK(reloaded.done.size() == 2);
    CHECK(wcr::is_done(reloaded, "Data/wow-update-15595.MPQ"));
    CHECK(wcr::is_done(reloaded, "Data/enUS/locale-enUS.MPQ"));
    CHECK_FALSE(wcr::is_done(reloaded, "Data/missing.MPQ"));

    fs::remove_all(dir);
}

TEST_CASE("region_swap")
{
    // swap_base is the generic single-substring replacement primitive.
    CHECK(wcr::swap_base("http://h/EU/x/Data/f", "/EU/x/", "/NA/y/") ==
          "http://h/NA/y/Data/f");
    CHECK(wcr::swap_base("http://h/Data/f", "/EU/x/", "/NA/y/") ==
          "http://h/Data/f");

    // swap_region replaces whichever region segment is present (build-agnostic).
    std::string eu =
        "http://dist/wow-pod-retail/EU/15050.direct/Data/foo.MPQ";
    CHECK(wcr::swap_region(eu, "/NA/") ==
          "http://dist/wow-pod-retail/NA/15050.direct/Data/foo.MPQ");
    std::string na =
        "http://dist/wow-pod-retail/NA/15050.direct/Data/foo.MPQ";
    CHECK(wcr::swap_region(na, "/EU/") ==
          "http://dist/wow-pod-retail/EU/15050.direct/Data/foo.MPQ");
    // No region token -> unchanged.
    CHECK(wcr::swap_region("http://dist/plain/Data/foo.MPQ",
                           "/NA/") ==
          "http://dist/plain/Data/foo.MPQ");

    // region_fallbacks returns the opposite region's short segment.
    std::vector<std::string> euFb = wcr::region_fallbacks("EU");
    REQUIRE(euFb.size() == 1);
    CHECK(euFb[0] == "/NA/");
    std::vector<std::string> naFb = wcr::region_fallbacks("NA");
    REQUIRE(naFb.size() == 1);
    CHECK(naFb[0] == "/EU/");
    CHECK(wcr::region_fallbacks("ZZ").empty());
}

TEST_CASE("journal_skip_done_file")
{
    namespace fs = std::filesystem;
    fs::path root = fs::temp_directory_path() / "wcr_journal_skip";
    fs::remove_all(root);
    fs::path src = root / "src";
    fs::path out = root / "out";
    fs::create_directories(src);
    fs::create_directories(out / "Data");

    // Two source fixtures with known bytes.
    auto writeBytes = [](const fs::path& p, const std::string& s)
    {
        std::ofstream f(p, std::ios::binary);
        f.write(s.data(), (std::streamsize)s.size());
    };
    writeBytes(src / "done.bin", "REALDONE");
    writeBytes(src / "fresh.bin", "REALFRESH");

    auto fileUrl = [](const fs::path& p)
    {
        return std::string("file:///") + p.generic_string();
    };

    wcr::Recipe r;
    r.version = "4.3.4";
    r.build = "15595";

    wcr::Artifact doneA;
    doneA.outName = "Data/done.bin";
    doneA.source = wcr::Source::PlainUrl;
    doneA.url = fileUrl(src / "done.bin");
    doneA.size = 8; // "REALDONE"
    r.artifacts.push_back(doneA);

    wcr::Artifact freshA;
    freshA.outName = "Data/fresh.bin";
    freshA.source = wcr::Source::PlainUrl;
    freshA.url = fileUrl(src / "fresh.bin");
    freshA.size = 9; // "REALFRESH"
    r.artifacts.push_back(freshA);

    // Pre-place a SENTINEL at the done file's output and journal it as done.
    writeBytes(out / "Data" / "done.bin", "SENTINEL");
    wcr::Journal seed = wcr::load_journal(out.string());
    wcr::mark_done(seed, "Data/done.bin");

    wcr::ReconstructOpts opts;
    wcr::Journal journal = wcr::load_journal(out.string());
    opts.journal = &journal;

    reconstruct(r, out.string(), opts);

    // done.bin was journalled -> skipped -> sentinel untouched.
    {
        std::ifstream f(out / "Data" / "done.bin", std::ios::binary);
        std::string got((std::istreambuf_iterator<char>(f)),
                        std::istreambuf_iterator<char>());
        CHECK(got == "SENTINEL");
    }
    // fresh.bin was NOT journalled -> downloaded -> real bytes written.
    {
        std::ifstream f(out / "Data" / "fresh.bin", std::ios::binary);
        std::string got((std::istreambuf_iterator<char>(f)),
                        std::istreambuf_iterator<char>());
        CHECK(got == "REALFRESH");
    }
    // A fully-successful reconstruct cleans up its scratch: the resume journal
    // is removed (nothing left to resume) so the output folder is pristine.
    CHECK_FALSE(wcr::journal_exists(out.string()));

    fs::remove_all(root);
}

TEST_CASE("journal_resume_redownloads_missing_file")
{
    // Fix B2: a journal entry for a file that no longer exists on disk must NOT
    // be honoured as a skip — reconstruct must re-download so the file is
    // present in the output after the run.
    namespace fs = std::filesystem;
    fs::path root = fs::temp_directory_path() / "wcr_journal_redownload";
    fs::remove_all(root);
    fs::path src = root / "src";
    fs::path out = root / "out";
    fs::create_directories(src);
    fs::create_directories(out / "Data");

    // Write the real source content under a file:// URL.
    {
        std::ofstream f(src / "x.bin", std::ios::binary);
        const char* bytes = "REALBYTES";
        f.write(bytes, 9);
    }
    std::string url = std::string("file:///") +
        (src / "x.bin").generic_string();

    wcr::Recipe r;
    r.version = "4.3.4";
    r.build = "15595";
    wcr::Artifact a;
    a.outName = "Data/x.bin";
    a.source = wcr::Source::PlainUrl;
    a.url = url;
    a.size = 9; // "REALBYTES"
    r.artifacts.push_back(a);

    // Mark the artifact as done in the journal but do NOT create the file on
    // disk — this simulates the corrupt state that Fix B2 must handle.
    wcr::Journal seed = wcr::load_journal(out.string());
    wcr::mark_done(seed, "Data/x.bin");

    wcr::ReconstructOpts opts;
    wcr::Journal journal = wcr::load_journal(out.string());
    opts.journal = &journal;

    // Reconstruct must succeed and the file must end up on disk with the
    // correct content (re-downloaded, not blindly skipped).
    REQUIRE_NOTHROW(reconstruct(r, out.string(), opts));
    {
        std::ifstream f(out / "Data" / "x.bin", std::ios::binary);
        std::string got((std::istreambuf_iterator<char>(f)),
                        std::istreambuf_iterator<char>());
        CHECK(got == "REALBYTES");
    }

    fs::remove_all(root);
}

TEST_CASE("journal_resume_redownloads_wrong_size_file")
{
    // Fix B2 (size branch): a journal entry for a file that IS present on disk
    // but has the wrong size must NOT be honoured as a skip — reconstruct must
    // re-download so the file has the correct content after the run.
    namespace fs = std::filesystem;
    fs::path root = fs::temp_directory_path() / "wcr_journal_wrongsize";
    fs::remove_all(root);
    fs::path src = root / "src";
    fs::path out = root / "out";
    fs::create_directories(src);
    fs::create_directories(out / "Data");

    // Write the real source content (9 bytes) under a file:// URL.
    {
        std::ofstream f(src / "x.bin", std::ios::binary);
        const char* bytes = "REALBYTES";
        f.write(bytes, 9);
    }
    std::string url = std::string("file:///") +
        (src / "x.bin").generic_string();

    wcr::Recipe r;
    r.version = "4.3.4";
    r.build = "15595";
    wcr::Artifact a;
    a.outName = "Data/x.bin";
    a.source = wcr::Source::PlainUrl;
    a.url = url;
    a.size = 9; // "REALBYTES"
    r.artifacts.push_back(a);

    // Pre-place the destination with the WRONG size (5 bytes) and mark it done
    // in the journal — this simulates the corrupt state that Fix B2 must handle.
    {
        std::ofstream f(out / "Data" / "x.bin", std::ios::binary);
        const char* stub = "WRONG";
        f.write(stub, 5);
    }
    wcr::Journal seed = wcr::load_journal(out.string());
    wcr::mark_done(seed, "Data/x.bin");

    wcr::ReconstructOpts opts;
    wcr::Journal journal = wcr::load_journal(out.string());
    opts.journal = &journal;

    // Reconstruct must succeed and the file must end up with the correct content
    // (re-downloaded, not blindly skipped despite the stale journal entry).
    REQUIRE_NOTHROW(reconstruct(r, out.string(), opts));
    {
        std::ifstream f(out / "Data" / "x.bin", std::ios::binary);
        std::string got((std::istreambuf_iterator<char>(f)),
                        std::istreambuf_iterator<char>());
        CHECK(got == "REALBYTES");
    }

    fs::remove_all(root);
}

TEST_CASE("journal_exists and clear_journal manage the .wcr-journal file")
{
    namespace fs = std::filesystem;
    fs::path dir = fs::temp_directory_path() / "wcr_journal_exists";
    fs::remove_all(dir);
    fs::create_directories(dir);

    CHECK(wcr::journal_exists(dir.string()) == false);

    wcr::Journal j = wcr::load_journal(dir.string());
    wcr::mark_done(j, "Data/x.MPQ"); // creates + writes the journal file
    CHECK(wcr::journal_exists(dir.string()) == true);

    wcr::clear_journal(dir.string());
    CHECK(wcr::journal_exists(dir.string()) == false);

    wcr::clear_journal(dir.string()); // idempotent no-op
    CHECK(wcr::journal_exists(dir.string()) == false);

    fs::remove_all(dir);
}

TEST_CASE("region_failover_eu_to_na")
{
    namespace fs = std::filesystem;
    fs::path root = fs::temp_directory_path() / "wcr_region_failover";
    fs::remove_all(root);
    // Embed "wow-pod-retail" in the path so swap_region can find its anchor.
    fs::path pod = root / "wow-pod-retail";
    fs::path na = pod / "NA" / "15050.direct" / "Data";
    fs::path out = root / "out";
    fs::create_directories(na);
    fs::create_directories(out / "Data");
    // Note: the EU directory is deliberately NOT created.

    {
        std::ofstream f(na / "x.bin", std::ios::binary);
        const char* bytes = "NAONLY";
        f.write(bytes, 6);
    }

    // Build a URL that points at the (missing) EU tree but whose region
    // segment swap_region can rewrite to the existing NA tree.
    std::string euUrl = std::string("file:///") +
        (pod / "EU" / "15050.direct" / "Data" / "x.bin").generic_string();
    CHECK(euUrl.find("wow-pod-retail") != std::string::npos);
    CHECK(euUrl.find("/EU/15050.direct/") != std::string::npos);

    wcr::Recipe r;
    r.version = "4.3.4";
    r.build = "15595";
    wcr::Artifact a;
    a.outName = "Data/x.bin";
    a.source = wcr::Source::PlainUrl;
    a.url = euUrl;
    a.size = 6; // "NAONLY"
    r.artifacts.push_back(a);

    wcr::ReconstructOpts opts;
    opts.regionFallback = {"/NA/"};

    // Primary EU path is missing; failover to NA must succeed.
    REQUIRE_NOTHROW(reconstruct(r, out.string(), opts));

    {
        std::ifstream f(out / "Data" / "x.bin", std::ios::binary);
        std::string got((std::istreambuf_iterator<char>(f)),
                        std::istreambuf_iterator<char>());
        CHECK(got == "NAONLY");
    }

    fs::remove_all(root);
}
