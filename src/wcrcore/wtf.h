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
 * @file wtf.h
 * @brief Writes first-run client config: WTF/RunOnce.wtf and the
 *        per-locale Data/<locale>/realmlist.wtf.
 */

#pragma once
#include <string>

namespace wcr
{
/// Write the first-run WoW config files into \p outDir.
///
/// Creates outDir + "/WTF/RunOnce.wtf" (SET realmlist / gxWindow /
/// gxMaximize / gxResolution) and outDir + "/Data/" + \p locale +
/// "/realmlist.wtf" (set realmlist "<host>").  Parent directories are
/// created as needed.
///
/// \param outDir    Root of the reconstructed client.
/// \param realmlist Realm host the client should connect to.
/// \param locale    Locale tag (e.g. "enUS") for the Data subdir.
/// \param width     gxResolution width in pixels.
/// \param height    gxResolution height in pixels.
void write_runonce(const std::string& outDir, const std::string& realmlist,
                   const std::string& locale, int width, int height);
} // namespace wcr
