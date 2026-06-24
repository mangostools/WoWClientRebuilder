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
 * @file locale_token.h
 * @brief Shared locale-token validation used by CLI argument parsing,
 *        manifest parsing, and first-run config writing.
 */

#pragma once
#include <string>

namespace wcr
{
/// True when \p token is a valid 4-letter ASCII locale tag (e.g. "enUS").
///
/// Accepts only tokens consisting of exactly 4 ASCII letters (A-Z or a-z).
/// This rejects path-traversal sequences ("../../etc"), separators, and
/// anything else that could be injected as a filesystem path component.
/// The sentinel value "all" (used by the CLI --locale flag) is NOT accepted
/// by this function; callers must handle it before calling is_valid_locale_token.
bool is_valid_locale_token(const std::string& token);
} // namespace wcr
