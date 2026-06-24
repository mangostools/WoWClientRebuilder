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
 * @file recipe.h
 * @brief Data-driven version recipes: structs and built-in recipe accessors
 *        for WoW 4.3.4 (Cataclysm) and 5.4.8 (Mists of Pandaria).
 */

#pragma once
#include <string>
#include <utility>
#include <vector>
namespace wcr
{
/// How an artifact's bytes are obtained.
enum class Source
{
    RepairMd5, ///< GET from the /repair store, addressed by MD5.
    ZipMember, ///< Extract a member from a downloaded zip.
    MpqPtch, ///< Extract a base file + PTCH blob from MPQs and apply.
    MpqExtract, ///< Extract a file from an MPQ; optionally apply a final PTCH.
    Generated, ///< Write embedded literal content verbatim.
    PlainUrl   ///< Download from a direct URL; verify by MD5.
};

/// One output file and how to obtain it.
struct Artifact
{
        std::string outName;             ///< File name written into outDir.
        std::string md5;                 ///< Expected UPPERCASE MD5.
        Source source = Source::RepairMd5; ///< How to obtain the bytes.
        std::string zipKey;              ///< ZipMember: source zip key.
        std::string baseMpqKey;          ///< MpqPtch: MPQ holding the base.
        std::string basePath;            ///< MpqPtch: base path inside the MPQ.
        std::string patchMpqKey;         ///< MpqPtch: MPQ holding the patch.
        std::string patchPath;           ///< MpqPtch: patch path inside the MPQ.
        std::string content;             ///< Generated: literal bytes to write.
        std::string url;                 ///< PlainUrl: direct download URL.
        long long size = -1;             ///< Expected byte size (-1 = skip).
        std::string locale;              ///< Locale tag ("" = base/independent).
        bool optional = false;           ///< 404-tolerant: skip if missing.
};

/// A zip archive downloaded once and shared by its ZipMember artifacts.
struct ZipSource
{
        std::string key; ///< Referenced by Artifact::zipKey.
        std::string url; ///< Download URL.
};

/// An MPQ archive downloaded once and shared by its MpqPtch artifacts.
struct MpqSource
{
        std::string key;  ///< Referenced by Artifact base/patch MPQ keys.
        std::string url;  ///< Download URL.
        long long size = -1; ///< Expected byte size (-1 to skip the check).
};

/// A full set of artifacts for one client version, plus their sources.
struct Recipe
{
        std::string version;             ///< e.g. "4.3.4".
        std::string build;               ///< e.g. "15595".
        std::string repairBase;          ///< Base URL of the /repair store.
        std::vector<ZipSource> zips;     ///< Zip sources for ZipMember.
        std::vector<MpqSource> mpqs;     ///< MPQ sources for MpqPtch.
        std::vector<Artifact> artifacts; ///< Files to produce.
        /// Region -> partial-manifest name, for versions whose manifest name
        /// is region-locked (MoP). Empty when the name is region-agnostic.
        std::vector<std::pair<std::string, std::string>> regionManifests;
};

/// Rewrite a generated WoW.mfil's content for a target region: swap the
/// /EU/ or /NA/ URL segment and substitute the region-locked manifest name
/// from the map. Returns content unchanged when the region already matches or
/// the map is empty.
std::string region_rewrite_mfil(
    const std::string& content,
    const std::vector<std::pair<std::string, std::string>>& regionManifests,
    const std::string& region);

/// Built-in recipe for WoW 4.3.4a (build 15595).
const Recipe& recipe_cata434();
/// Built-in recipe for WoW 5.4.8 (build 18414).
const Recipe& recipe_mop548();
/// Find a built-in recipe by version string; nullptr if unknown.
const Recipe* find_recipe(const std::string& version);
} // namespace wcr
