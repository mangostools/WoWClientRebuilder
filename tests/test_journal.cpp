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
                       const std::string& recipeId,
                       const std::string& torrentId)
{
    wcr::RunStamp s;
    s.region = region;
    s.manifest = manifest;
    s.recipeId = recipeId;
    s.torrentId = torrentId;
    return s;
}

/// Seed outDir with a stamped journal marking relPath done (what a real
/// completed run under `stamp` leaves behind mid-way).
void seed_done(const std::string& outDir, const wcr::RunStamp& stamp,
               const std::string& relPath)
{
    wcr::Journal j;
    j.path = outDir + "/.wcr-journal";
    j.stamp = stamp;
    wcr::write_stamp(j);
    wcr::mark_done(j, relPath);
}
} // namespace

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

    // Pre-place a SENTINEL at the done file's output and journal it as done
    // under a STAMPED journal (reconstruct only honours stamped ones).
    writeBytes(out / "Data" / "done.bin", "SENTINEL");
    seed_done(out.string(), stamp_of("EU", "m.mfil", wcr::recipe_id(r), ""),
              "Data/done.bin");

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

    // Mark the artifact as done in a STAMPED journal but do NOT create the
    // file on disk — the corrupt state that Fix B2 must handle.
    seed_done(out.string(), stamp_of("EU", "m.mfil", wcr::recipe_id(r), ""),
              "Data/x.bin");

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
    seed_done(out.string(), stamp_of("EU", "m.mfil", wcr::recipe_id(r), ""),
              "Data/x.bin");

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
    // Deliberately NOT pre-created: on a real first run the output directory
    // does not exist yet and write_stamp is the first thing to touch it, so it
    // must create the directory itself rather than fail the whole run.

    const wcr::RunStamp want = stamp_of("NA", "wow-18414-NA.mfil", "RID", "");
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
    const wcr::RunStamp na = stamp_of("NA", "wow-18414-NA.mfil", "RID", "");

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
        j.stamp = stamp_of("EU", "wow-18414-EU.mfil", "RID", "");
        wcr::write_stamp(j);
        CHECK_FALSE(wcr::journal_matches(wcr::load_journal(dir.string()), na));
    }
    SUBCASE("same region, different manifest (or version) does not")
    {
        wcr::Journal j;
        j.path = (dir / ".wcr-journal").string();
        j.stamp = stamp_of("NA", "wow-15595-NA.mfil", "RID", "");
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
        j.stamp = stamp_of("NA", "wow-18414-NA.mfil", "RID", "AAAA");
        wcr::write_stamp(j);
        CHECK_FALSE(wcr::journal_matches(
            wcr::load_journal(dir.string()),
            stamp_of("NA", "wow-18414-NA.mfil", "RID", "BBBB")));
        CHECK_FALSE(wcr::journal_matches(
            wcr::load_journal(dir.string()),
            stamp_of("NA", "wow-18414-NA.mfil", "RID", "")));
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
    SUBCASE("header truncated before the torrent line never matches")
    {
        // A crash mid-write_stamp can leave every line but the last. The
        // absent torrent key would otherwise read as "no torrent" and wrongly
        // match a no-torrent run, so a header missing ANY key is unstamped.
        put_bytes(dir / ".wcr-journal",
                  "#wcr region=NA\n"
                  "#wcr manifest=wow-18414-NA.mfil\n"
                  "#wcr recipe=RID\n");
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
    // Root-level source archives are resumable download targets too, so they
    // must be part of the sweep set (their basename lands in the outDir root).
    wcr::MpqSource m;
    m.key = "base";
    m.url = "http://dist/wow-pod-retail/EU/15890.direct/Data/base-Win.MPQ";
    r.mpqs.push_back(m);
    wcr::ZipSource z;
    z.key = "live64";
    z.url = "http://host/downloads/WoWLive-64-Win-15595.zip";
    r.zips.push_back(z);

    std::vector<std::string> got = wcr::artifact_part_paths(r, "out");
    REQUIRE(got.size() == 6);
    CHECK(got[0] == "out/Data/x.bin.part");
    CHECK(got[1] == "out/Data/x.bin.part.fb");
    CHECK(got[2] == "out/Updates/y.MPQ.part");
    CHECK(got[3] == "out/Updates/y.MPQ.part.fb");
    CHECK(got[4] == "out/base-Win.MPQ");
    CHECK(got[5] == "out/WoWLive-64-Win-15595.zip");
}

TEST_CASE("recipe_id_binds_the_full_download_identity")
{
    wcr::Recipe r;
    wcr::MpqSource m;
    m.key = "final";
    m.url = "http://dist/wow-pod-retail/EU/15890.direct/Updates/f.MPQ";
    m.size = 100;
    r.mpqs.push_back(m);
    wcr::Artifact a;
    a.outName = "Data/x.bin";
    a.source = wcr::Source::PlainUrl;
    a.url = "http://dist/wow-pod-retail/EU/15890.direct/Data/x.bin";
    a.size = 8;
    r.artifacts.push_back(a);

    const std::string base = wcr::recipe_id(r);
    CHECK(base == wcr::recipe_id(r)); // deterministic

    // Adding an artifact (a different mode / locale / cinematics selection)
    // changes the identity, so a stamp from one selection never resumes
    // another even at the same region and manifest.
    wcr::Recipe more = r;
    wcr::Artifact fr;
    fr.outName = "Data/frFR/locale-frFR.MPQ";
    fr.source = wcr::Source::PlainUrl;
    fr.url = "http://dist/wow-pod-retail/EU/15890.direct/Data/frFR/l.MPQ";
    fr.size = 9;
    more.artifacts.push_back(fr);
    CHECK(wcr::recipe_id(more) != base);

    // A size-only change (manifest revision) changes it too.
    wcr::Recipe resized = r;
    resized.artifacts[0].size = 9;
    CHECK(wcr::recipe_id(resized) != base);

    // And so does a URL-only change (region swap).
    wcr::Recipe moved = r;
    moved.artifacts[0].url =
        "http://dist/wow-pod-retail/NA/15890.direct/Data/x.bin";
    CHECK(wcr::recipe_id(moved) != base);
}

TEST_CASE("reconstruct_ignores_an_unstamped_journal")
{
    // The fail-closed check must hold at the core API, not only in the CLI: a
    // library caller handing reconstruct() an unstamped journal (pre-stamp
    // build, or lines appended without write_stamp) must not get skips from
    // it. The sentinel below is at the CORRECT size, so only the provenance
    // check -- not the size check -- can force the re-download.
    namespace fs = std::filesystem;
    fs::path root = fs::temp_directory_path() / "wcr_unstamped_ignored";
    fs::remove_all(root);
    fs::path src = root / "src";
    fs::path out = root / "out";
    fs::create_directories(src);
    fs::create_directories(out / "Data");
    put_bytes(src / "x.bin", "REALDATA");
    put_bytes(out / "Data" / "x.bin", "SENTINEL"); // same 8-byte length
    put_bytes(out / ".wcr-journal", "Data/x.bin\n"); // done, but NO stamp

    wcr::Recipe r;
    r.version = "4.3.4";
    r.build = "15595";
    wcr::Artifact a;
    a.outName = "Data/x.bin";
    a.source = wcr::Source::PlainUrl;
    a.url = std::string("file:///") + (src / "x.bin").generic_string();
    a.size = 8;
    r.artifacts.push_back(a);

    wcr::Journal journal = wcr::load_journal(out.string());
    CHECK_FALSE(journal.stamped);
    wcr::ReconstructOpts opts;
    opts.journal = &journal;
    REQUIRE_NOTHROW(reconstruct(r, out.string(), opts));

    // Not skipped: the unstamped done-line was ignored and the file was
    // re-downloaded.
    CHECK(get_bytes(out / "Data" / "x.bin") == "REALDATA");

    fs::remove_all(root);
}

TEST_CASE("reconstruct_failure_keeps_the_journal_for_resume")
{
    // An ordinary mid-run failure (network error) must leave the stamped
    // journal AND the partials in place: they are exactly what the next run
    // needs to prove the partials are its own and resume them. Only full
    // success removes the journal.
    namespace fs = std::filesystem;
    fs::path root = fs::temp_directory_path() / "wcr_fail_keeps_journal";
    fs::remove_all(root);
    fs::path out = root / "out";
    fs::create_directories(out / "Data");

    wcr::Recipe r;
    r.version = "5.4.8";
    r.build = "18414";
    wcr::Artifact a;
    a.outName = "Data/x.bin";
    a.source = wcr::Source::PlainUrl;
    a.url = std::string("file:///") +
        (root / "src" / "missing.bin").generic_string(); // does not exist
    a.size = 8;
    r.artifacts.push_back(a);

    // The REAL digest: "still matching" below then means "the next
    // invocation of this same recipe will trust and resume this journal".
    const wcr::RunStamp stamp =
        stamp_of("NA", "wow-18414-NA.mfil", wcr::recipe_id(r), "");
    wcr::Journal journal;
    journal.path = (out / ".wcr-journal").string();
    journal.stamp = stamp;
    wcr::write_stamp(journal);
    // A partial from an earlier artifact of this same run.
    put_bytes(out / "Data" / "earlier.MPQ.part", "HALF");

    wcr::ReconstructOpts opts;
    opts.journal = &journal;
    CHECK_THROWS_AS(reconstruct(r, out.string(), opts), std::runtime_error);

    // The journal survives the failure, still matching, and the partial from
    // the same run is untouched -- the next invocation resumes instead of
    // discarding.
    wcr::Journal after = wcr::load_journal(out.string());
    CHECK(wcr::journal_matches(after, stamp));
    CHECK(get_bytes(out / "Data" / "earlier.MPQ.part") == "HALF");

    fs::remove_all(root);
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

    const wcr::RunStamp naStamp =
        stamp_of("NA", "wow-15595-NA.mfil", wcr::recipe_id(r), "");

    SUBCASE("stale stamped journal from an EU run")
    {
        wcr::Journal eu;
        eu.path = (out / ".wcr-journal").string();
        eu.stamp = stamp_of("EU", "wow-15595-EU.mfil", "RID", "");
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

TEST_CASE("reconstruct_distrusts_a_foreign_stamped_journal")
{
    // Being stamped is not enough: the stamp must describe THIS recipe.
    // reconstruct() verifies the recipe digest itself, so a stamped journal
    // from different work handed in by a library caller neither authorises
    // skips nor gets its partials resumed. Both payloads are same-length so
    // only content assertions can tell right from wrong.
    namespace fs = std::filesystem;
    fs::path root = fs::temp_directory_path() / "wcr_foreign_journal";
    fs::remove_all(root);
    fs::path src = root / "src";
    fs::path out = root / "out";
    fs::create_directories(src);
    fs::create_directories(out / "Data");
    put_bytes(src / "x.bin", "REALDATA");
    put_bytes(src / "y.bin", "NANANANA");

    wcr::Recipe r;
    r.version = "4.3.4";
    r.build = "15595";
    wcr::Artifact a;
    a.outName = "Data/x.bin";
    a.source = wcr::Source::PlainUrl;
    a.url = std::string("file:///") + (src / "x.bin").generic_string();
    a.size = 8;
    r.artifacts.push_back(a);
    wcr::Artifact b;
    b.outName = "Data/y.bin";
    b.source = wcr::Source::PlainUrl;
    b.url = std::string("file:///") + (src / "y.bin").generic_string();
    b.size = 8;
    r.artifacts.push_back(b);

    // A journal from OTHER work: fully stamped, correct-size sentinel marked
    // done, and a stale partial waiting to be spliced.
    put_bytes(out / "Data" / "x.bin", "SENTINEL");
    seed_done(out.string(),
              stamp_of("EU", "m.mfil", "SOME-OTHER-RECIPE", ""),
              "Data/x.bin");
    put_bytes(out / "Data" / "y.bin.part", "EUEU");

    wcr::Journal journal = wcr::load_journal(out.string());
    CHECK(journal.stamped); // stamped -- but for someone else's recipe
    wcr::ReconstructOpts opts;
    opts.journal = &journal;
    REQUIRE_NOTHROW(reconstruct(r, out.string(), opts));

    // The skip was refused (sentinel replaced with the real bytes)...
    CHECK(get_bytes(out / "Data" / "x.bin") == "REALDATA");
    // ...and the stale partial was overwritten, not appended to: resuming it
    // would have produced the same-length splice "EUEUNANA".
    CHECK(get_bytes(out / "Data" / "y.bin") == "NANANANA");

    fs::remove_all(root);
}

TEST_CASE("reconstruct_never_writes_into_a_foreign_journal")
{
    // Writes are gated like reads: a run working under a foreign journal must
    // not append its completions to it. Otherwise, when THIS run fails midway
    // and the foreign journal survives, a later run matching the FOREIGN
    // stamp would trust those lines and skip a same-sized file produced by
    // the wrong recipe.
    namespace fs = std::filesystem;
    fs::path root = fs::temp_directory_path() / "wcr_foreign_no_write";
    fs::remove_all(root);
    fs::path src = root / "src";
    fs::path out = root / "out";
    fs::create_directories(src);
    fs::create_directories(out / "Data");
    put_bytes(src / "x.bin", "REALDATA");

    wcr::Recipe r;
    r.version = "4.3.4";
    r.build = "15595";
    wcr::Artifact ok;
    ok.outName = "Data/x.bin";
    ok.source = wcr::Source::PlainUrl;
    ok.url = std::string("file:///") + (src / "x.bin").generic_string();
    ok.size = 8;
    r.artifacts.push_back(ok);
    wcr::Artifact bad;
    bad.outName = "Data/z.bin";
    bad.source = wcr::Source::PlainUrl;
    bad.url = std::string("file:///") +
        (src / "absent.bin").generic_string(); // fails after x.bin completed
    bad.size = 4;
    r.artifacts.push_back(bad);

    // A foreign but fully stamped journal owns the output directory.
    wcr::Journal foreign;
    foreign.path = (out / ".wcr-journal").string();
    foreign.stamp = stamp_of("EU", "m.mfil", "SOME-OTHER-RECIPE", "");
    wcr::write_stamp(foreign);

    wcr::Journal journal = wcr::load_journal(out.string());
    wcr::ReconstructOpts opts;
    opts.journal = &journal;
    CHECK_THROWS_AS(reconstruct(r, out.string(), opts), std::runtime_error);

    // x.bin completed before the failure, but the surviving foreign journal
    // must not have gained its completion line.
    wcr::Journal after = wcr::load_journal(out.string());
    CHECK(after.stamped);
    CHECK(after.stamp.recipeId == "SOME-OTHER-RECIPE");
    CHECK_FALSE(wcr::is_done(after, "Data/x.bin"));

    fs::remove_all(root);
}
