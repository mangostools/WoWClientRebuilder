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
 * @file args.cpp
 * @brief Implementation of the flag-driven CLI argument parser.
 */

#include "args.h"
#include "locale_token.h"
#include <vector>

namespace
{

/// Map a mode keyword to its Mode enum; returns false for an unknown keyword.
bool map_mode(const std::string& word, wcr::Mode& out)
{
    if (word == "client")
    {
        out = wcr::Mode::FullClient;
        return true;
    }
    if (word == "data")
    {
        out = wcr::Mode::DataOnly;
        return true;
    }
    if (word == "locale")
    {
        out = wcr::Mode::LocaleOnly;
        return true;
    }
    return false;
}

/// Split a comma-separated value into its non-empty tokens, in order.
std::vector<std::string> split_csv(const std::string& csv)
{
    std::vector<std::string> out;
    std::string cur;
    for (char c : csv)
    {
        if (c == ',')
        {
            if (!cur.empty())
            {
                out.push_back(cur);
            }
            cur.clear();
        }
        else
        {
            cur.push_back(c);
        }
    }
    if (!cur.empty())
    {
        out.push_back(cur);
    }
    return out;
}
} // namespace

namespace wcr
{
bool parse_args(int argc, char** argv, RunParams& out, std::string& err)
{
    out.locales = {"enUS"};
    std::vector<std::string> positionals;
    for (int i = 1; i < argc; ++i)
    {
        std::string a = argv[i];
        if (a == "--no-cinematics")
        {
            out.cinematics = false;
        }
        else if (a == "--cinematics")
        {
            // Per-locale cinematics are included by default now, so this flag is
            // redundant; kept accepted so older invocations don't error out.
            out.cinematics = true;
        }
        else if (a == "--yes")
        {
            out.yes = true;
        }
        else if (a == "--locale" || a == "--tfil" || a == "--realmlist" ||
                 a == "--region")
        {
            if (i + 1 >= argc)
            {
                err = "flag " + a + " requires a value";
                return false;
            }
            std::string v = argv[++i];
            if (a == "--locale")
            {
                if (v == "all")
                {
                    out.locales = {};
                }
                else
                {
                    std::vector<std::string> tokens = split_csv(v);
                    for (const std::string& tok : tokens)
                    {
                        if (!is_valid_locale_token(tok))
                        {
                            err = "--locale token '" + tok +
                                  "' is not a valid 4-letter locale tag (e.g. enUS)";
                            return false;
                        }
                    }
                    out.locales = tokens;
                }
            }
            else if (a == "--tfil")
            {
                out.tfilPath = v;
            }
            else if (a == "--realmlist")
            {
                out.realmlist = v;
            }
            else
            {
                if (v != "EU" && v != "NA")
                {
                    err = "--region must be EU or NA (got '" + v + "')";
                    return false;
                }
                out.region = v;
            }
        }
        else if (a.size() >= 2 && a[0] == '-' && a[1] == '-')
        {
            err = "unknown flag '" + a + "'";
            return false;
        }
        else
        {
            positionals.push_back(a);
        }
    }
    if (positionals.size() != 3)
    {
        err = "expected: <mode> <version> <outDir>";
        return false;
    }
    if (!map_mode(positionals[0], out.mode))
    {
        err = "unknown mode '" + positionals[0] +
              "' (want client, data, or locale)";
        return false;
    }
    out.version = positionals[1];
    out.outDir = positionals[2];
    return true;
}
} // namespace wcr
