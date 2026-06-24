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
 * @file interactive.cpp
 * @brief Implementation of the interactive console front-end.
 */

#include "interactive.h"
#include <cctype>
#include <istream>
#include <ostream>
#include <string>
#include <vector>

namespace
{
std::string trim(const std::string& s)
{
    const char* ws = " \t\r\n";
    std::string::size_type b = s.find_first_not_of(ws);
    if (b == std::string::npos)
    {
        return std::string();
    }
    std::string::size_type e = s.find_last_not_of(ws);
    return s.substr(b, e - b + 1);
}

std::string to_lower(const std::string& s)
{
    std::string r;
    r.reserve(s.size());
    for (char c : s)
    {
        r.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    return r;
}

bool parse_int(const std::string& s, int& out)
{
    if (s.empty())
    {
        return false;
    }
    for (char c : s)
    {
        if (!std::isdigit(static_cast<unsigned char>(c)))
        {
            return false;
        }
    }
    try
    {
        out = std::stoi(s);
    }
    catch (...)
    {
        return false;
    }
    return true;
}

struct VersionInfo
{
    const char* version;
    const char* build;
};
const VersionInfo kVersions[] = {
    {"4.3.4", "15595"},
    {"5.4.8", "18414"},
};
} // namespace

namespace wcr
{
int prompt_menu(std::istream& in, std::ostream& out, const std::string& title,
                const std::vector<std::string>& options)
{
    while (true)
    {
        out << title << "\n";
        for (std::size_t i = 0; i < options.size(); ++i)
        {
            out << "  [" << (i + 1) << "] " << options[i] << "\n";
        }
        out << "Select [1-" << options.size() << "]: ";
        std::string line;
        if (!std::getline(in, line))
        {
            return -1;
        }
        int n = 0;
        if (parse_int(trim(line), n) && n >= 1 &&
            n <= static_cast<int>(options.size()))
        {
            return n - 1;
        }
        out << "Please enter a number between 1 and " << options.size()
            << ".\n";
    }
}

bool prompt_yes_no(std::istream& in, std::ostream& out,
                   const std::string& question)
{
    while (true)
    {
        out << question << " [y/n]: ";
        std::string line;
        if (!std::getline(in, line))
        {
            return false;
        }
        std::string s = to_lower(trim(line));
        if (s == "y" || s == "yes")
        {
            return true;
        }
        if (s == "n" || s == "no")
        {
            return false;
        }
        out << "Please enter 'y' or 'n'.\n";
    }
}

std::string prompt_line(std::istream& in, std::ostream& out,
                        const std::string& prompt, const std::string& deflt)
{
    out << prompt << " [" << deflt << "]: ";
    std::string line;
    if (!std::getline(in, line))
    {
        return deflt;
    }
    std::string s = trim(line);
    return s.empty() ? deflt : s;
}

Mode prompt_mode(std::istream& in, std::ostream& out)
{
    int idx = prompt_menu(in, out, "Generation mode:",
                          {"Full client (entire client + Data/, incl. shared "
                           "cinematics)",
                           "Data only", "Locale(s) only"});
    if (idx == 1)
    {
        return Mode::DataOnly;
    }
    if (idx == 2)
    {
        return Mode::LocaleOnly;
    }
    return Mode::FullClient; // idx 0, or -1 on EOF
}

std::string prompt_version(std::istream& in, std::ostream& out)
{
    std::vector<std::string> opts;
    for (const VersionInfo& v : kVersions)
    {
        opts.push_back(std::string(v.version) + " (" + v.build + ")");
    }
    int idx = prompt_menu(in, out, "Client version:", opts);
    if (idx < 0)
    {
        idx = 0;
    }
    return kVersions[idx].version;
}

std::string prompt_region(std::istream& in, std::ostream& out)
{
    int idx = prompt_menu(in, out,
                          "CDN region (the other stays automatic failover):",
                          {"EU", "NA"});
    return idx == 1 ? "NA" : "EU";
}

std::string build_for_version(const std::string& version)
{
    for (const VersionInfo& v : kVersions)
    {
        if (version == v.version)
        {
            return v.build;
        }
    }
    return std::string();
}

std::vector<std::string> prompt_locale(std::istream& in, std::ostream& out,
                                       const std::vector<std::string>& available)
{
    // No advertised locales (degenerate manifest): nothing to pick, so proceed
    // with the all-locales sentinel rather than loop forever on a "1-0" range.
    if (available.empty())
    {
        return {};
    }
    while (true)
    {
        out << "Locale:\n";
        for (std::size_t i = 0; i < available.size(); ++i)
        {
            out << "  [" << (i + 1) << "] " << available[i] << "\n";
        }
        out << "  [A] All locales\n";
        out << "Select [1-" << available.size() << " or A]: ";
        std::string line;
        if (!std::getline(in, line))
        {
            return {"enUS"};
        }
        std::string s = to_lower(trim(line));
        if (s == "a" || s == "all")
        {
            return {};
        }
        int n = 0;
        if (parse_int(s, n) && n >= 1 &&
            n <= static_cast<int>(available.size()))
        {
            return {available[static_cast<std::size_t>(n) - 1]};
        }
        out << "Please enter a number 1-" << available.size() << " or A.\n";
    }
}

std::string derive_outdir(const std::string& version, const std::string& build,
                          Mode mode, const std::vector<std::string>& locales)
{
    std::string name = "WoW-" + version + "-" + build;
    if (mode == Mode::DataOnly)
    {
        return name;
    }
    std::string tag;
    if (locales.empty())
    {
        tag = "all";
    }
    else
    {
        for (std::size_t i = 0; i < locales.size(); ++i)
        {
            if (i != 0)
            {
                tag += "-";
            }
            tag += locales[i];
        }
    }
    return name + "-" + tag;
}

RunParams run_interactive(std::istream& in, std::ostream& out,
                          const FetchLocales& fetchLocales)
{
    RunParams p;
    p.mode = prompt_mode(in, out);
    p.version = prompt_version(in, out);
    if (p.mode == Mode::DataOnly)
    {
        p.locales = {};
    }
    else
    {
        std::vector<std::string> avail = fetchLocales(p.version);
        p.locales = prompt_locale(in, out, avail);
    }
    p.region = prompt_region(in, out);
    std::string build = build_for_version(p.version);
    std::string deflt = derive_outdir(p.version, build, p.mode, p.locales);
    p.outDir = prompt_line(in, out, "Output folder", deflt);
    return p;
}

bool should_clear_journal(std::istream& in, std::ostream& out,
                          bool journalExists, const std::string& outDir)
{
    if (!journalExists)
    {
        return false;
    }
    return prompt_yes_no(
        in, out,
        "A previous download for '" + outDir +
            "' was found. Start fresh (re-download everything) instead of "
            "resuming?");
}
} // namespace wcr
