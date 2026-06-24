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
 * @file mfil.h
 * @brief WoW manifest (.mfil) parsing and pointer resolution: turns a
 *        WoW.mfil pointer plus a partial manifest into a list of data files.
 */

#pragma once
#include <string>
#include <vector>
namespace wcr
{
/// One file entry parsed from a partial .mfil manifest.
struct MfilRecord
{
        std::string relPath; ///< Path relative to the data base URL (name=).
        long long size = -1; ///< Expected byte size (size=), -1 if absent.
        std::string locale;  ///< Locale tag ("" = base/locale-independent).
};

/// A fully parsed partial manifest: every file the client needs.
struct Manifest
{
        int version = 0;                  ///< Manifest format version (v2).
        std::string dataBaseUrl;          ///< Base URL data paths hang off.
        std::vector<std::string> locales; ///< Distinct locale tags seen.
        std::vector<MfilRecord> files;    ///< Every file record (no whitelist).
};

/// The two pieces of a WoW.mfil pointer: where data lives and the partial name.
struct Pointer
{
        std::string dataBaseUrl; ///< location= value (data base URL).
        std::string partialName; ///< manifest_partial= value.
};

/// Parse a WoW.mfil pointer text into its data base URL and partial name.
Pointer parse_pointer(const std::string& pointerText);

/// Parse a v2 partial manifest body into a Manifest; \p dataBaseUrl is the
/// base URL the records' relative paths hang off of.
Manifest parse_mfil(const std::string& text, const std::string& dataBaseUrl);

/// Download the partial manifest named by \p p and parse it into a Manifest.
/// When \p requireHash is true (the default) and the manifest name carries no
/// parseable 32-hex content hash, the call throws rather than silently skipping
/// the integrity check.  Pass \p requireHash = false only for synthetic test
/// fixtures whose names intentionally carry no hash.
Manifest fetch_manifest(const Pointer& p, bool requireHash = true);
} // namespace wcr
