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
 * @file wtf.cpp
 * @brief Implementation of write_runonce (first-run client config files).
 */

#include "wtf.h"
#include "bytes.h"
#include "locale_token.h"
#include <filesystem>
#include <stdexcept>
#include <string>

namespace wcr
{
/// Convert a std::string to a Bytes buffer for write_file.
static Bytes to_bytes(const std::string& s)
{
    return Bytes(s.begin(), s.end());
}

void write_runonce(const std::string& outDir, const std::string& realmlist,
                   const std::string& locale, int width, int height)
{
    std::filesystem::path root(outDir);

    std::filesystem::path wtfDir = root / "WTF";
    std::filesystem::create_directories(wtfDir);
    std::string runOnce;
    runOnce += "SET realmlist \"" + realmlist + "\"\n";
    runOnce += "SET gxWindow \"1\"\n";
    runOnce += "SET gxMaximize \"0\"\n";
    runOnce += "SET gxResolution \"" + std::to_string(width) + "x" +
               std::to_string(height) + "\"\n";
    write_file((wtfDir / "RunOnce.wtf").string(), to_bytes(runOnce));

    // Security (B2): belt-and-suspenders guard; reject any locale string that
    // is not a valid 4-letter ASCII tag before using it as a path segment.
    // This catches a poisoned locale that somehow escaped the manifest filter.
    if (!is_valid_locale_token(locale))
    {
        throw std::runtime_error("invalid locale for realmlist path: " + locale);
    }
    std::filesystem::path dataDir = root / "Data" / locale;
    std::filesystem::create_directories(dataDir);
    std::string realm = "set realmlist \"" + realmlist + "\"\n";
    write_file((dataDir / "realmlist.wtf").string(), to_bytes(realm));
}
} // namespace wcr
