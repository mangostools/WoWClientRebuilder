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
#include <cstdlib>
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
void print_banner(std::ostream& out)
{
    out << "\n"
        << "  __  __      _  _  ___  ___  ___\n"
        << " |  \\/  |__ _| \\| |/ __|/ _ \\/ __|     WoW Client Rebuilder\n"
        << " | |\\/| / _` | .` | (_ | (_) \\__ \\     byte-identical 4.3.4 / 5.4.8 clients\n"
        << " |_|  |_\\__,_|_|\\_|\\___|\\___/|___/      from Blizzard's live CDN\n"
        << "\n"
        << " For help and support please visit:\n"
        << " Website: https://getmangos.eu\n"
        << "    Repo: https://github.com/mangostools/WoWClientRebuilder\n"
        << "\n";
}

void clear_screen_and_print_banner(std::ostream& out)
{
    std::system("cls");
    print_banner(out);
}

Nav prompt_menu(std::istream& in, std::ostream& out, const std::string& title,
                const std::vector<std::string>& options, bool allowBack,
                int& index)
{
    while (true)
    {
        out << title << "\n";
        for (std::size_t i = 0; i < options.size(); ++i)
        {
            out << "  [" << (i + 1) << "] " << options[i] << "\n";
        }
        if (allowBack)
        {
            out << "  [B] Back\n";
        }
        out << "  [X] Exit\n";
        out << "Select [1-" << options.size()
            << (allowBack ? ", B, X" : ", X") << "]: ";
        std::string line;
        if (!std::getline(in, line))
        {
            return Nav::Exit;
        }
        std::string s = to_lower(trim(line));
        if (s == "x")
        {
            return Nav::Exit;
        }
        if (allowBack && s == "b")
        {
            return Nav::Back;
        }
        int n = 0;
        if (parse_int(s, n) && n >= 1 &&
            n <= static_cast<int>(options.size()))
        {
            index = n - 1;
            return Nav::Select;
        }
        out << "Please enter a number between 1 and " << options.size()
            << (allowBack ? ", B, or X.\n" : ", or X.\n");
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

Nav prompt_outdir(std::istream& in, std::ostream& out, const std::string& deflt,
                  bool allowBack, std::string& out_path)
{
    out << "Output folder [" << deflt << "] ("
        << (allowBack ? "B=back, " : "") << "X=exit): ";
    std::string line;
    if (!std::getline(in, line))
    {
        return Nav::Exit;
    }
    std::string s = trim(line);
    if (s.empty())
    {
        out_path = deflt;
        return Nav::Select;
    }
    std::string low = to_lower(s);
    if (low == "x")
    {
        return Nav::Exit;
    }
    if (allowBack && low == "b")
    {
        return Nav::Back;
    }
    out_path = s; // a real path keeps its original case
    return Nav::Select;
}

Nav prompt_mode(std::istream& in, std::ostream& out, bool allowBack,
                Mode& out_mode)
{
    int idx = 0;
    Nav nav = prompt_menu(in, out, "Generation mode:",
                          {"Full client (entire client + Data/ + your locale's "
                           "cinematics)",
                           "Data only", "Locale(s) only"},
                          allowBack, idx);
    if (nav != Nav::Select)
    {
        return nav;
    }
    if (idx == 1)
    {
        out_mode = Mode::DataOnly;
    }
    else if (idx == 2)
    {
        out_mode = Mode::LocaleOnly;
    }
    else
    {
        out_mode = Mode::FullClient;
    }
    return Nav::Select;
}

Nav prompt_version(std::istream& in, std::ostream& out, bool allowBack,
                   std::string& out_version)
{
    std::vector<std::string> opts;
    for (const VersionInfo& v : kVersions)
    {
        opts.push_back(std::string(v.version) + " (" + v.build + ")");
    }
    int idx = 0;
    Nav nav = prompt_menu(in, out, "Client version:", opts, allowBack, idx);
    if (nav != Nav::Select)
    {
        return nav;
    }
    out_version = kVersions[idx].version;
    return Nav::Select;
}

Nav prompt_region(std::istream& in, std::ostream& out, bool allowBack,
                  std::string& out_region)
{
    int idx = 0;
    Nav nav = prompt_menu(in, out,
                          "CDN region (the other stays automatic failover):",
                          {"EU", "NA"}, allowBack, idx);
    if (nav != Nav::Select)
    {
        return nav;
    }
    out_region = (idx == 1) ? "NA" : "EU";
    return Nav::Select;
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

Nav prompt_locale(std::istream& in, std::ostream& out,
                  const std::vector<std::string>& available, bool allowBack,
                  std::vector<std::string>& out_locales)
{
    // No advertised locales (degenerate manifest): nothing to pick, so select
    // the all-locales sentinel rather than loop forever on a "1-0" range.
    if (available.empty())
    {
        out_locales = {};
        return Nav::Select;
    }
    while (true)
    {
        out << "Locale:\n";
        for (std::size_t i = 0; i < available.size(); ++i)
        {
            out << "  [" << (i + 1) << "] " << available[i] << "\n";
        }
        out << "  [A] All locales\n";
        if (allowBack)
        {
            out << "  [B] Back\n";
        }
        out << "  [X] Exit\n";
        out << "Select [1-" << available.size() << ", A"
            << (allowBack ? ", B, X" : ", X") << "]: ";
        std::string line;
        if (!std::getline(in, line))
        {
            return Nav::Exit;
        }
        std::string s = to_lower(trim(line));
        if (s == "x")
        {
            return Nav::Exit;
        }
        if (allowBack && s == "b")
        {
            return Nav::Back;
        }
        if (s == "a" || s == "all")
        {
            out_locales = {};
            return Nav::Select;
        }
        int n = 0;
        if (parse_int(s, n) && n >= 1 &&
            n <= static_cast<int>(available.size()))
        {
            out_locales = {available[static_cast<std::size_t>(n) - 1]};
            return Nav::Select;
        }
        out << "Please enter a number 1-" << available.size() << ", A"
            << (allowBack ? ", B, or X.\n" : ", or X.\n");
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

// Confirm an Exit request (the [X] option) before abandoning setup.
static bool confirm_exit(std::istream& in, std::ostream& out)
{
    return prompt_yes_no(in, out, "Exit without rebuilding?");
}

RunParams run_interactive(std::istream& in, std::ostream& out,
                          const FetchLocales& fetchLocales)
{
    RunParams p;
    // Ordered setup questions. Locale is skipped in BOTH directions for
    // DataOnly, so Region's Back returns to Version.
    enum Step
    {
        StepMode = 0,
        StepVersion,
        StepLocale,
        StepRegion,
        StepOutput,
        StepDone
    };
    auto nextStep = [](int step, Mode mode) -> int
    {
        int s = step + 1;
        if (s == StepLocale && mode == Mode::DataOnly)
        {
            s = StepRegion;
        }
        return s;
    };
    auto prevStep = [](int step, Mode mode) -> int
    {
        int s = step - 1;
        if (s == StepLocale && mode == Mode::DataOnly)
        {
            s = StepVersion;
        }
        return s;
    };

    int step = StepMode;
    while (step != StepDone)
    {
        clear_screen_and_print_banner(out);
        const bool allowBack = (step != StepMode); // nowhere to go back from #1
        Nav nav = Nav::Select;
        switch (step)
        {
            case StepMode:
            {
                nav = prompt_mode(in, out, allowBack, p.mode);
                if (nav == Nav::Select && p.mode == Mode::DataOnly)
                {
                    p.locales = {}; // DataOnly carries no locale selection
                }
                break;
            }
            case StepVersion:
            {
                nav = prompt_version(in, out, allowBack, p.version);
                break;
            }
            case StepLocale:
            {
                std::vector<std::string> avail = fetchLocales(p.version);
                nav = prompt_locale(in, out, avail, allowBack, p.locales);
                break;
            }
            case StepRegion:
            {
                nav = prompt_region(in, out, allowBack, p.region);
                break;
            }
            case StepOutput:
            {
                std::string build = build_for_version(p.version);
                std::string deflt =
                    derive_outdir(p.version, build, p.mode, p.locales);
                nav = prompt_outdir(in, out, deflt, allowBack, p.outDir);
                break;
            }
            default:
            {
                break;
            }
        }

        if (nav == Nav::Exit)
        {
            // A typed 'x' on a live stream gets a confirmation. A closed/
            // exhausted stream (EOF) cannot answer one, so cancel
            // unconditionally rather than re-prompting into an infinite loop.
            if (!in.good() || confirm_exit(in, out))
            {
                p.cancelled = true;
                return p;
            }
            continue; // declined: re-show the same question
        }
        if (nav == Nav::Back)
        {
            step = prevStep(step, p.mode);
            continue;
        }
        step = nextStep(step, p.mode);
    }
    return p;
}

bool should_clear_journal(std::istream& in, std::ostream& out,
                          bool journalExists, const std::string& outDir)
{
    if (!journalExists)
    {
        return false;
    }
    clear_screen_and_print_banner(out);
    return prompt_yes_no(
        in, out,
        "A previous download for '" + outDir +
            "' was found. Start fresh (re-download everything) instead of "
            "resuming?");
}
} // namespace wcr
