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
 * @file fetch.h
 * @brief Source-dispatching fetch engine: repair/zip/mpq-ptch artifact
 *        retrieval with per-artifact MD5 verification.
 */

#pragma once
#include "recipe.h"
#include "bytes.h"
#include <string>
#include <vector>

namespace wcr
{
/// Build a /repair store URL for a file addressed by its UPPERCASE MD5:
///   <base>/<md5[0]>/<md5[1]>/<md5>
std::string repair_url(const std::string& base, const std::string& md5);

/// Throw std::runtime_error if md5_hex(data) != expectedMd5. The name is
/// included in the message. Comparison is case-sensitive (UPPERCASE).
void verify_or_throw(const Bytes& data, const std::string& expectedMd5,
                     const std::string& name);

/// Delete the reconstruction scratch from outDir: each named source MPQ
/// (left in the root after MpqPtch/MpqExtract) plus the .wcr-journal. Missing
/// files are ignored. Called only after a fully successful reconstruct().
void remove_build_scratch(const std::string& outDir,
                          const std::vector<std::string>& sourceFiles);

/// Forward declaration: BitTorrent .tfil model (defined in bencode.h, M2.4).
struct Torrent;
/// Forward declaration: resume journal (defined in journal.h, M2.6).
struct Journal;

/// Optional knobs passed to the 3-arg reconstruct overload. All members are
/// non-owning / opt-in; a default-constructed instance restores plain
/// download-and-verify behaviour.
struct ReconstructOpts
{
        const Torrent* torrent = nullptr;     ///< M2.4: piece verification.
        Journal* journal = nullptr;           ///< M2.6: resume journal.
        std::vector<std::string> regionFallback; ///< M2.6: NA/EU failover.
};

/// Replace the first occurrence of substring `from` in url with `to`. If
/// `from` is absent, return url unchanged. Generic region-segment swap.
std::string swap_base(const std::string& url, const std::string& from,
                      const std::string& to);

/// Replace whichever of "/EU/" or "/NA/" appears after "wow-pod-retail" in
/// url with toSeg (e.g. "/NA/" or "/EU/"), preserving the build segment.
/// Returns url unchanged if no region code is found.
std::string swap_region(const std::string& url, const std::string& toSeg);

/// Region segments to try if the primary region fails: "EU" yields {"/NA/"},
/// "NA" yields {"/EU/"}, else empty.
std::vector<std::string> region_fallbacks(const std::string& region);

/// Return the CDN region path segment for the given region:
///   "NA" -> "/NA/"
///   anything else -> "/EU/" (safe default).
std::string region_segment(const std::string& region);

/// Apply the chosen region to a run recipe: swap each MPQ source URL's region
/// segment (build-agnostic), and rewrite the written WoW.mfil artifact for the
/// region (region-locked manifest name + URL segment, via base.regionManifests).
/// EU (the recipe default) and a region-agnostic recipe (empty regionManifests)
/// are effectively no-ops.
void apply_region_to_recipe(Recipe& run, const Recipe& base,
                            const std::string& region);

/// Reconstruct every artifact of recipe r into outDir, verifying each file by
/// size (when known) and MD5 (when set). Fails fast (throws) on any download,
/// extraction, or verification error; no partial output is left behind
/// silently. The 2-arg overload forwards a default-constructed opts.
void reconstruct(const Recipe& r, const std::string& outDir);
void reconstruct(const Recipe& r, const std::string& outDir,
                 const ReconstructOpts& opts);
} // namespace wcr
