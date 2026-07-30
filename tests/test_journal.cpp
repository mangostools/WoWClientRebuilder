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
 * @brief Resume journal + region-bound resume state unit tests.
 */

#include "doctest.h"
#include "journal.h"
#include "fetch.h"
#include "recipe.h"
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

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




namespace
{
/// Write bytes to p, creating parents.
void put_bytes(const std::filesystem::path& p, const std::string& s)
{
    std::filesystem::create_directories(p.parent_path());
    std::ofstream f(p, std::ios::binary);
    f.write(s.data(), static_cast<std::streamsize>(s.size()));
}

/// Read a whole file as a string ("" if absent).
std::string get_bytes(const std::filesystem::path& p)
{
    std::ifstream f(p, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(f)),
                       std::istreambuf_iterator<char>());
}

wcr::RunStamp stamp_of(const std::string& region, const std::string& manifest,
                       const std::string& torrentId)
{
    wcr::RunStamp s;
    s.region = region;
    s.manifest = manifest;
    s.torrentId = torrentId;
    return s;
}
} // namespace




TEST_CASE("no_cross_region_failover_on_a_missing_primary")
{
    // Cross-region failover was removed: a fallback copy cannot be validated
    // here, so an unreachable primary must fail rather than silently substitute
    // the other region's bytes.
    namespace fs = std::filesystem;
    fs::path root = fs::temp_directory_path() / "wcr_no_failover";
    fs::remove_all(root);
    fs::path pod = root / "wow-pod-retail";
    fs::path out = root / "out";
    fs::create_directories(out / "Data");
    // The NA copy exists and the EU copy does not; nothing may reach for NA.
    put_bytes(pod / "NA" / "15050.direct" / "Data" / "x.bin", "NAONLY");

    std::string euUrl = std::string("file:///") +
        (pod / "EU" / "15050.direct" / "Data" / "x.bin").generic_string();

    wcr::Recipe r;
    r.version = "4.3.4";
    r.build = "15595";
    wcr::Artifact a;
    a.outName = "Data/x.bin";
    a.source = wcr::Source::PlainUrl;
    a.url = euUrl;
    a.size = 6;
    r.artifacts.push_back(a);

    CHECK_THROWS_AS(reconstruct(r, out.string(), wcr::ReconstructOpts{}),
                    std::runtime_error);
    CHECK_FALSE(fs::exists(out / "Data" / "x.bin"));

    fs::remove_all(root);
}


TEST_CASE("write_stamp_then_matches_and_round_trips")
{
    namespace fs = std::filesystem;
    fs::path dir = fs::temp_directory_path() / "wcr_stamp_rt";
    fs::remove_all(dir);
    fs::create_directories(dir);

    const wcr::RunStamp want = stamp_of("NA", "wow-18414-NA.mfil", "");
    wcr::Journal j;
    j.path = (dir / ".wcr-journal").string();
    j.stamp = want;
    wcr::write_stamp(j);
    // The stamp is on disk BEFORE any artifact completes: an interruption right
    // now still leaves a journal identifying the run that owns the partials.
    wcr::Journal empty = wcr::load_journal(dir.string());
    CHECK(empty.stamped);
    CHECK(wcr::journal_matches(empty, want));
    CHECK(empty.done.empty());

    wcr::mark_done(j, "Data/a.MPQ");
    wcr::mark_done(j, "Data/b.MPQ");
    wcr::Journal again = wcr::load_journal(dir.string());
    CHECK(wcr::journal_matches(again, want));
    CHECK(again.done.size() == 2);
    CHECK(wcr::is_done(again, "Data/a.MPQ"));

    fs::remove_all(dir);
}

TEST_CASE("journal_matches_is_fail_closed")
{
    namespace fs = std::filesystem;
    fs::path dir = fs::temp_directory_path() / "wcr_stamp_match";
    fs::remove_all(dir);
    fs::create_directories(dir);
    const wcr::RunStamp na = stamp_of("NA", "wow-18414-NA.mfil", "");

    SUBCASE("matching stamp resumes")
    {
        wcr::Journal j;
        j.path = (dir / ".wcr-journal").string();
        j.stamp = na;
        wcr::write_stamp(j);
        CHECK(wcr::journal_matches(wcr::load_journal(dir.string()), na));
    }
    SUBCASE("region change does not")
    {
        wcr::Journal j;
        j.path = (dir / ".wcr-journal").string();
        j.stamp = stamp_of("EU", "wow-18414-EU.mfil", "");
        wcr::write_stamp(j);
        CHECK_FALSE(wcr::journal_matches(wcr::load_journal(dir.string()), na));
    }
    SUBCASE("same region, different manifest (or version) does not")
    {
        wcr::Journal j;
        j.path = (dir / ".wcr-journal").string();
        j.stamp = stamp_of("NA", "wow-15595-NA.mfil", "");
        wcr::write_stamp(j);
        CHECK_FALSE(wcr::journal_matches(wcr::load_journal(dir.string()), na));
    }
    SUBCASE("different torrent identity does not")
    {
        // torrentId names WHICH .tfil verified the pieces. Entries completed
        // under torrent A must not be skipped by a run using torrent B, and a
        // no-torrent journal must not satisfy a --tfil run.
        wcr::Journal j;
        j.path = (dir / ".wcr-journal").string();
        j.stamp = stamp_of("NA", "wow-18414-NA.mfil", "AAAA");
        wcr::write_stamp(j);
        CHECK_FALSE(wcr::journal_matches(
            wcr::load_journal(dir.string()),
            stamp_of("NA", "wow-18414-NA.mfil", "BBBB")));
        CHECK_FALSE(wcr::journal_matches(
            wcr::load_journal(dir.string()),
            stamp_of("NA", "wow-18414-NA.mfil", "")));
    }
    SUBCASE("unstamped journal (pre-stamp build) never matches")
    {
        put_bytes(dir / ".wcr-journal", "Data/a.MPQ\n");
        CHECK_FALSE(wcr::journal_matches(wcr::load_journal(dir.string()), na));
    }
    SUBCASE("partial stamp header never matches")
    {
        put_bytes(dir / ".wcr-journal", "#wcr region=NA\nData/a.MPQ\n");
        CHECK_FALSE(wcr::journal_matches(wcr::load_journal(dir.string()), na));
    }
    SUBCASE("missing journal never matches")
    {
        CHECK_FALSE(wcr::journal_matches(wcr::load_journal(dir.string()), na));
    }

    fs::remove_all(dir);
}

TEST_CASE("discard_stale_run_removes_exactly_the_named_paths")
{
    namespace fs = std::filesystem;
    fs::path dir = fs::temp_directory_path() / "wcr_discard";
    fs::remove_all(dir);
    fs::create_directories(dir);
    put_bytes(dir / ".wcr-journal", "#wcr region=EU\nData/a.MPQ\n");
    put_bytes(dir / "Data" / "a.MPQ.part", "EUPART");
    put_bytes(dir / "Data" / "b.MPQ.part.fb", "SCRATCH");
    // NOT in the recipe: a user file that merely ends in .part, and finished
    // output. Neither may be touched.
    put_bytes(dir / "notes.part", "USERDATA");
    put_bytes(dir / "Data" / "keep.MPQ", "FINISHED");

    std::vector<std::string> parts = {
        (dir / "Data" / "a.MPQ.part").string(),
        (dir / "Data" / "a.MPQ.part.fb").string(), // absent: must be a no-op
        (dir / "Data" / "b.MPQ.part").string(),    // absent: must be a no-op
        (dir / "Data" / "b.MPQ.part.fb").string(),
    };
    CHECK_NOTHROW(wcr::discard_stale_run(dir.string(), parts));

    CHECK_FALSE(fs::exists(dir / ".wcr-journal"));
    CHECK_FALSE(fs::exists(dir / "Data" / "a.MPQ.part"));
    CHECK_FALSE(fs::exists(dir / "Data" / "b.MPQ.part.fb"));
    CHECK(get_bytes(dir / "notes.part") == "USERDATA");
    CHECK(get_bytes(dir / "Data" / "keep.MPQ") == "FINISHED");

    fs::remove_all(dir);
}

TEST_CASE("discard_stale_run_throws_when_a_partial_survives")
{
    // Proceeding after a failed cleanup would resume the very bytes the
    // discard exists to destroy, so it must fail loudly, not press on.
    namespace fs = std::filesystem;
    fs::path dir = fs::temp_directory_path() / "wcr_discard_fail";
    fs::remove_all(dir);
    // A directory at the .part path makes remove() fail (non-empty dir).
    fs::create_directories(dir / "Data" / "a.MPQ.part" / "pin");
    put_bytes(dir / "Data" / "a.MPQ.part" / "pin" / "x", "PIN");

    std::vector<std::string> parts = {(dir / "Data" / "a.MPQ.part").string()};
    CHECK_THROWS_AS(wcr::discard_stale_run(dir.string(), parts),
                    std::runtime_error);

    fs::remove_all(dir);
}

TEST_CASE("artifact_part_paths_mirrors_reconstruct_naming")
{
    wcr::Recipe r;
    wcr::Artifact a;
    a.outName = "Data/x.bin";
    r.artifacts.push_back(a);
    wcr::Artifact b;
    b.outName = "Updates/y.MPQ";
    r.artifacts.push_back(b);

    std::vector<std::string> got = wcr::artifact_part_paths(r, "out");
    REQUIRE(got.size() == 4);
    CHECK(got[0] == "out/Data/x.bin.part");
    CHECK(got[1] == "out/Data/x.bin.part.fb");
    CHECK(got[2] == "out/Updates/y.MPQ.part");
    CHECK(got[3] == "out/Updates/y.MPQ.part.fb");
}

TEST_CASE("cross_run_region_change_cannot_splice_a_partial")
{
    // End-to-end form of the cross-run splice through reconstruct(), with
    // same-length regional content so only the content assertion can tell a
    // spliced result from a clean one. Covers BOTH prior-run shapes:
    // - a stamped journal from an interrupted run, and
    // - partials with NO journal at all (interrupted before the first artifact
    //   completed under a pre-stamp build, or after clear_journal) -- the case
    //   a lazily-written stamp cannot see, closed by discarding whenever the
    //   journal does not affirmatively match.
    namespace fs = std::filesystem;
    fs::path root = fs::temp_directory_path() / "wcr_crossrun_splice";
    fs::remove_all(root);
    fs::path pod = root / "wow-pod-retail";
    fs::path out = root / "out";
    fs::create_directories(out / "Data");
    put_bytes(pod / "EU" / "15050.direct" / "Data" / "x.bin", "EUEUEUEU");
    put_bytes(pod / "NA" / "15050.direct" / "Data" / "x.bin", "NANANANA");

    wcr::Recipe r;
    r.version = "4.3.4";
    r.build = "15595";
    wcr::Artifact a;
    a.outName = "Data/x.bin";
    a.source = wcr::Source::PlainUrl;
    a.url = std::string("file:///") +
        (pod / "NA" / "15050.direct" / "Data" / "x.bin").generic_string();
    a.size = 8;
    r.artifacts.push_back(a);

    const wcr::RunStamp naStamp = stamp_of("NA", "wow-15595-NA.mfil", "");

    SUBCASE("stale stamped journal from an EU run")
    {
        wcr::Journal eu;
        eu.path = (out / ".wcr-journal").string();
        eu.stamp = stamp_of("EU", "wow-15595-EU.mfil", "");
        wcr::write_stamp(eu);
        wcr::mark_done(eu, "Data/other.bin");
        put_bytes(out / "Data" / "x.bin.part", "EUEU");
    }
    SUBCASE("orphan partial with no journal at all")
    {
        put_bytes(out / "Data" / "x.bin.part", "EUEU");
    }

    // The CLI flow: read, decide, and -- since nothing affirmatively matches --
    // discard this recipe's partials and stamp the new run before fetching.
    wcr::Journal j = wcr::load_journal(out.string());
    CHECK_FALSE(wcr::journal_matches(j, naStamp));
    wcr::discard_stale_run(out.string(),
                           wcr::artifact_part_paths(r, out.string()));
    j = wcr::Journal{};
    j.path = (out / ".wcr-journal").string();
    j.stamp = naStamp;
    wcr::write_stamp(j);
    CHECK_FALSE(fs::exists(out / "Data" / "x.bin.part"));

    wcr::ReconstructOpts opts;
    opts.journal = &j;
    REQUIRE_NOTHROW(reconstruct(r, out.string(), opts));

    // Whole NA content. "EUEUNANA" is the splice this guards against: it is
    // also 8 bytes, so only the content assertion can catch it.
    CHECK(get_bytes(out / "Data" / "x.bin") == "NANANANA");

    fs::remove_all(root);
}
