# MaNGOS is a full featured server for World of Warcraft, supporting
# the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
#
# Copyright (C) 2005-2026 MaNGOS <https://www.getmangos.eu>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
#
# World of Warcraft, and all World of Warcraft or Warcraft art, images,
# and lore are copyrighted by Blizzard Entertainment, Inc.

cmake_minimum_required(VERSION 3.21)

# Ask vcpkg to copy runtime DLL dependencies (libcurl, zlib, etc.) to the
# install prefix alongside the exe when cmake --install is run.  Must be set
# before project() so the vcpkg toolchain file picks it up.
set(X_VCPKG_APPLOCAL_DEPS_INSTALL ON)

project(WoWClientRebuilder CXX C)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(dep/StormLib)                 # builds 'storm' (StormLib.lib)
find_package(CURL REQUIRED)                     # system/vcpkg libcurl

add_library(wcrcore
  src/wcrcore/ptch.cpp src/wcrcore/mpq.cpp src/wcrcore/download.cpp
  src/wcrcore/md5.cpp src/wcrcore/zip.cpp src/wcrcore/recipe.cpp
  src/wcrcore/fetch.cpp src/wcrcore/mfil.cpp
  src/wcrcore/datarecipe.cpp
  src/wcrcore/sha1.cpp
  src/wcrcore/bencode.cpp
  src/wcrcore/pieces.cpp
  src/wcrcore/wtf.cpp
  src/wcrcore/journal.cpp
  src/wcrcore/progress.cpp
  src/wcrcore/locale_token.cpp
  src/cli/args.cpp src/cli/assemble.cpp src/cli/interactive.cpp
  dep/miniz/miniz.c)
target_include_directories(wcrcore PUBLIC src/wcrcore src/cli
  PRIVATE dep/StormLib/src dep/miniz ${CURL_INCLUDE_DIRS})
target_link_libraries(wcrcore PUBLIC storm CURL::libcurl)

add_executable(wowrebuild src/cli/main.cpp)
target_link_libraries(wowrebuild PRIVATE wcrcore)

enable_testing()
add_executable(wcrtests tests/test_smoke.cpp tests/test_ptch.cpp tests/test_mpq.cpp tests/test_download.cpp tests/test_md5.cpp tests/test_acceptance.cpp tests/test_zip.cpp tests/test_recipe.cpp tests/test_fetch.cpp tests/test_mfil.cpp tests/test_datarecipe.cpp tests/test_args.cpp tests/test_assemble.cpp tests/test_sha1.cpp tests/test_bencode.cpp tests/test_pieces.cpp tests/test_fetch_pieces.cpp tests/test_wtf.cpp tests/test_journal.cpp tests/test_progress.cpp tests/test_interactive.cpp)
target_include_directories(wcrtests PRIVATE dep/doctest src/wcrcore dep/StormLib/src dep/miniz)
target_link_libraries(wcrtests PRIVATE wcrcore)
add_test(NAME wcrtests COMMAND wcrtests)
add_test(NAME ptch COMMAND wcrtests -tc="apply_ptch reconstructs a known target (uncompressed BSD0/BSDIFF40)")
add_test(NAME mpq COMMAND wcrtests -tc="MpqArchive lists and extracts a created file")
add_test(NAME download COMMAND wcrtests -tc="download_file copies a file:// source and validates size")
add_test(NAME md5 COMMAND wcrtests -tc="md5_hex RFC 1321 standard test vectors (UPPERCASE)")
add_test(NAME zip COMMAND wcrtests -tc="zip_extract_member returns a stored member's bytes")
add_test(NAME recipe COMMAND wcrtests -tc="find_recipe selects by version and rejects unknown")
add_test(NAME fetch COMMAND wcrtests -tc="repair_url maps an MD5 to <base>/<c0>/<c1>/<MD5>")
add_test(NAME acceptance
  COMMAND wcrtests -tc="byte-exact 5.4.8 Wow.exe from real fixtures"
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
# The live-CDN byte-exact acceptance test needs network access and WCR_LIVE=1.
# Gate it behind an explicit option so a normal offline `ctest` run does not
# report a green PASS for a test that executed zero assertions.
option(WCR_LIVE_TESTS "Register live-CDN acceptance tests (need network + WCR_LIVE=1)" OFF)
if(WCR_LIVE_TESTS)
  add_test(NAME acceptance_cata434
    COMMAND wcrtests -tc="byte-exact 4.3.4 full binary set from live CDN"
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
  set_tests_properties(acceptance_cata434 PROPERTIES ENVIRONMENT "WCR_LIVE=1")
endif()
add_test(NAME acceptance_mop_aux
  COMMAND wcrtests -tc="byte-exact 5.4.8 aux binaries from real fixtures"
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
add_test(NAME reconstruct_cleanup
  COMMAND wcrtests -tc="reconstruct removes the source MPQ from the root on success"
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
add_test(NAME datarecipe_osx_filter COMMAND wcrtests -tc="expand_data drops OSX-only archives*")
add_test(NAME args_yes COMMAND wcrtests -tc="parse_args --yes sets the yes field and defaults to false")
add_test(NAME artifact_fields COMMAND wcrtests -tc="Artifact has size/locale/optional fields with defaults")
add_test(NAME reconstruct_opts COMMAND wcrtests -tc="ReconstructOpts default-constructs with null torrent/journal")
add_test(NAME plainurl_subdir COMMAND wcrtests -tc="PlainUrl artifact with a subdir outName lands in the subdir")
add_test(NAME size_verify COMMAND wcrtests -tc="size match passes and size mismatch throws")
add_test(NAME md5_empty_size_only COMMAND wcrtests -tc="empty md5 with set size verifies by size only")
add_test(NAME optional_skip COMMAND wcrtests -tc="optional artifact with a missing source is skipped*")
add_test(NAME mfil_pointer COMMAND wcrtests -tc="parse_pointer extracts location and manifest_partial")
add_test(NAME mfil_records COMMAND wcrtests -tc="parse_mfil emits every record with name + size (incl wow-update)")
add_test(NAME mfil_locale COMMAND wcrtests -tc="parse_mfil assigns locale from each file's own path= field")
add_test(NAME mfil_alternate COMMAND wcrtests -tc="parse_mfil keys alternate.MPQ to its own path tag locale")
add_test(NAME mfil_fetch COMMAND wcrtests -tc="fetch_manifest downloads the partial and parses it")
add_test(NAME mfil_hashcheck COMMAND wcrtests -tc="fetch_manifest authenticates the body against the filename hash")
add_test(NAME datarecipe_total_bytes COMMAND wcrtests -tc="total_bytes sums sizes and ignores -1 sentinels")
add_test(NAME datarecipe_dataonly COMMAND wcrtests -tc="expand_data DataOnly keeps only locale-independent records")
add_test(NAME datarecipe_localeonly COMMAND wcrtests -tc="expand_data LocaleOnly keeps only selected-locale records")
add_test(NAME datarecipe_localeonly_two COMMAND wcrtests -tc="expand_data LocaleOnly with two locales keeps both")
add_test(NAME datarecipe_fullclient COMMAND wcrtests -tc="expand_data FullClient keeps base records plus selected locale")
add_test(NAME datarecipe_cinematics_shared COMMAND wcrtests -tc="expand_data: shared Data/Interface/ cinematics always in*")
add_test(NAME datarecipe_cinematics_perlocale COMMAND wcrtests -tc="expand_data: shared cinematics always in*")
add_test(NAME datarecipe_free_space COMMAND wcrtests -tc="free_space reports available bytes and supports pre-flight check")
add_test(NAME args COMMAND wcrtests -tc="parse_args reads positionals and applies flag defaults")
add_test(NAME args_flags COMMAND wcrtests -tc="parse_args handles every flag and a csv --locale")
add_test(NAME args_all COMMAND wcrtests -tc="parse_args treats --locale all as the empty (all) sentinel")
add_test(NAME args_bad COMMAND wcrtests -tc="parse_args rejects bad input with false and a message")
add_test(NAME assemble_ptr COMMAND wcrtests -tc="pointer_text_from_recipe yields parseable WoW.mfil pointer")
add_test(NAME assemble_full COMMAND wcrtests -tc="assemble_recipe FullClient appends data after binaries")
add_test(NAME assemble_data COMMAND wcrtests -tc="assemble_recipe DataOnly carries only the data artifacts")
add_test(NAME sha1 COMMAND wcrtests -tc="sha1 known vectors (empty and abc)")
add_test(NAME bencode COMMAND wcrtests -tc="parse_torrent reads piece length*")
add_test(NAME pieces COMMAND wcrtests -tc="verify_file_pieces passes a good file and rejects a flipped byte")
add_test(NAME fetch_pieces COMMAND wcrtests -tc="reconstruct piece-verifies a PlainUrl Data file via opts.torrent")
add_test(NAME wtf COMMAND wcrtests -tc="write_runonce writes RunOnce.wtf and realmlist.wtf")
add_test(NAME wtf_custom COMMAND wcrtests -tc="write_runonce honors custom host*")
add_test(NAME journal COMMAND wcrtests -tc="journal_round_trip")
add_test(NAME region_swap COMMAND wcrtests -tc="region_swap")
add_test(NAME journal_skip COMMAND wcrtests -tc="journal_skip_done_file")
add_test(NAME journal_resume_missing COMMAND wcrtests -tc="journal_resume_redownloads_missing_file")
add_test(NAME journal_resume_wrong_size COMMAND wcrtests -tc="journal_resume_redownloads_wrong_size_file")
add_test(NAME region_failover COMMAND wcrtests -tc="region_failover_eu_to_na")
add_test(NAME progress_zero_bytes COMMAND wcrtests -tc="human_bytes: zero bytes")
add_test(NAME progress_sub_kb COMMAND wcrtests -tc="human_bytes: sub-KB values stay in bytes")
add_test(NAME progress_kb COMMAND wcrtests -tc="human_bytes: KB boundary")
add_test(NAME progress_mb COMMAND wcrtests -tc="human_bytes: MB boundary")
add_test(NAME progress_gb COMMAND wcrtests -tc="human_bytes: GB - the 3.67 GB reference case")
add_test(NAME progress_sentinel COMMAND wcrtests -tc="human_bytes: unknown sentinel -1 returns question mark")
add_test(NAME progress_render_known_total COMMAND wcrtests -tc="render_progress: total > 0 includes label*")
add_test(NAME progress_render_unknown_total COMMAND wcrtests -tc="render_progress: total <= 0 shows label*")
add_test(NAME progress_render_zero_total COMMAND wcrtests -tc="render_progress: zero total also no pct")
add_test(NAME progress_render_speed_positive COMMAND wcrtests -tc="render_progress: positive speed appends rate with /s suffix")
add_test(NAME progress_render_speed_negative COMMAND wcrtests -tc="render_progress: negative speed omits rate entirely")
add_test(NAME progress_render_speed_zero COMMAND wcrtests -tc="render_progress: zero speed shows 0 B/s")

# ---------------------------------------------------------------------------
# Install rules — produces a self-contained WoWClientRebuilder_install layout
# ---------------------------------------------------------------------------
# Install the wowrebuild executable at the install-prefix root.
# X_VCPKG_APPLOCAL_DEPS_INSTALL (set above, before project()) causes the vcpkg
# toolchain to hook install(TARGETS ...) and invoke applocal.ps1, which copies
# libcurl.dll, zlib1.dll, and any other vcpkg-installed runtime DLLs next to
# the exe — so the install dir is self-contained with no extra PATH setup.
install(TARGETS wowrebuild RUNTIME DESTINATION .)
