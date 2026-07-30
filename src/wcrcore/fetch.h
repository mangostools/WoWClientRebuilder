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

/// Delete named source-archive scratch (zips / MPQs left in the outDir root)
/// from outDir. Missing files are ignored. Deliberately does NOT touch the
/// .wcr-journal: this also runs on the failure path, where the journal and the
/// .part files must survive so the next run can resume them. The journal is
/// removed separately on full success.
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
        /// M2.6: resume journal. Honoured (skips + .part resumption) only when
        /// stamped AND its stamp's recipeId equals recipe_id of the recipe
        /// being reconstructed -- reconstruct() verifies that itself, so a
        /// journal from different work can neither skip nor splice. The
        /// orchestrator remains responsible for the full journal_matches
        /// check (region/manifest/torrent) and for discard_stale_run.
        Journal* journal = nullptr;
        // NOTE: there was a regionFallback member here (NA/EU failover). It was
        // removed: a fallback copy cannot be validated at the fetch layer (Data
        // artifacts carry no MD5, and the other region's authentic size lives
        // only in that region's manifest), so it could accept an unverified
        // substitute for a genuinely region-specific archive. See the comment
        // on the PlainUrl download in reconstruct().
};

/// Replace the first occurrence of substring `from` in url with `to`. If
/// `from` is absent, return url unchanged. Generic region-segment swap.
std::string swap_base(const std::string& url, const std::string& from,
                      const std::string& to);

/// Replace whichever of "/EU/" or "/NA/" appears after "wow-pod-retail" in
/// url with toSeg (e.g. "/NA/" or "/EU/"), preserving the build segment.
/// Returns url unchanged if no region code is found.
std::string swap_region(const std::string& url, const std::string& toSeg);

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

/// Every resumable download target this recipe could leave in outDir: the
/// "<dst>.part" scratch reconstruct() resumes for each artifact, the
/// "<dst>.part.fb" left by the removed cross-region failover in an earlier
/// release, and the root-level source archives (zip / MPQ basenames), which
/// download_file also resumes in place. Used by discard_stale_run() so cleanup
/// touches only files this tool created, never unrelated ones that happen to
/// end in .part.
std::vector<std::string> artifact_part_paths(const Recipe& r,
                                             const std::string& outDir);

/// Digest of a recipe's download identity: an md5 over every MPQ/zip source
/// (url, size) and every artifact (outName, size, url, md5). Two runs with the
/// same recipe_id fetch the same bytes to the same names, so a resume between
/// them is sound; any change of version, region, mode, locale selection or
/// cinematics changes the digest. Stored in RunStamp::recipeId.
std::string recipe_id(const Recipe& r);

/// Reconstruct every artifact of recipe r into outDir, verifying each file by
/// size (when known) and MD5 (when set). Fails fast (throws) on any download,
/// extraction, or verification error; no partial output is left behind
/// silently. The 2-arg overload forwards a default-constructed opts.
void reconstruct(const Recipe& r, const std::string& outDir);
void reconstruct(const Recipe& r, const std::string& outDir,
                 const ReconstructOpts& opts);
} // namespace wcr
