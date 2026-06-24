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
 * @file assemble.h
 * @brief Pure helpers that turn a base recipe + expanded data artifacts into
 *        the final Recipe to reconstruct, and extract the embedded WoW.mfil
 *        pointer text. Kept free of network/CLI state for unit testing.
 */

#pragma once
#include "recipe.h"
#include "datarecipe.h"
#include <string>
#include <vector>

namespace wcr
{
/// Return the literal content of the Generated "WoW.mfil" artifact embedded in
/// recipe r. Throws std::runtime_error if no such artifact exists.
std::string pointer_text_from_recipe(const Recipe& r);

/// Build the Recipe to reconstruct. For Mode::FullClient the result is base
/// with data appended to its artifacts (binaries + data). For DataOnly and
/// LocaleOnly the result carries base's version/build/repairBase/zips/mpqs but
/// only the data artifacts.
Recipe assemble_recipe(const Recipe& base, const std::vector<Artifact>& data,
                       Mode mode);
} // namespace wcr
