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
 * @file main.cpp
 * @brief CLI entry point (flag-driven MVP): parse args, fetch the data
 *        manifest from the recipe's embedded WoW.mfil pointer, expand the
 *        requested locales, assemble the final recipe, and reconstruct.
 */

#include "args.h"
#include "assemble.h"
#include "interactive.h"
#include "recipe.h"
#include "fetch.h"
#include "journal.h"
#include "bytes.h"
#include "wtf.h"
#include "bencode.h"
#include "mfil.h"
#include "datarecipe.h"
#include <curl/curl.h>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
using namespace wcr;

/// Print usage for the flag-driven MVP.
static void print_usage()
{
    printf("usage: wowrebuild <mode> <version> [options] <outDir>\n");
    printf("  mode:        client | data | locale\n");
    printf("  version:     4.3.4 | 5.4.8\n");
    printf("  --locale L   csv list (e.g. enUS,deDE) or 'all'"
           " (default enUS)\n");
    printf("  --tfil path  optional .tfil torrent for piece verification"
           " (loads each file into RAM)\n");
    printf("  --realmlist H host written into the WTF files (default 127.0.0.1)\n");
    printf("  --region R   EU | NA (default EU)\n");
    printf("  --cinematics include cinematics movie files\n");
    printf("  --yes        skip pre-flight confirmation prompt\n");
}

/// Print the MaNGOS banner (the logo mangosd/realmd show at startup), framed for
/// WoWClientRebuilder. Shown once at interactive startup, and again after the
/// pre-flight confirmation so the download begins on a clean screen.
static void print_banner()
{
    printf(
        "\n"
        "  __  __      _  _  ___  ___  ___\n"
        " |  \\/  |__ _| \\| |/ __|/ _ \\/ __|     WoW Client Rebuilder\n"
        " | |\\/| / _` | .` | (_ | (_) \\__ \\     byte-identical 4.3.4 / 5.4.8 clients\n"
        " |_|  |_\\__,_|_|\\_|\\___|\\___/|___/      from Blizzard's live CDN\n"
        "\n"
        " For help and support please visit:\n"
        " Website: https://getmangos.eu\n"
        "    Repo: https://github.com/mangostools/WoWClientRebuilder\n"
        "\n");
}

/// Build the final recipe to reconstruct from parsed run parameters. Fetches
/// the manifest unless the mode is binaries-only (client with no data is still
/// data-bearing here, so every mode fetches the manifest).
static Recipe build_run_recipe(const RunParams& params)
{
    const Recipe* base = find_recipe(params.version);
    if (base == nullptr)
    {
        throw std::runtime_error("unknown version '" + params.version + "'");
    }
    Pointer ptr = parse_pointer(pointer_text_from_recipe(*base));
    Manifest manifest = fetch_manifest(ptr);
    manifest.dataBaseUrl =
        wcr::swap_region(manifest.dataBaseUrl,
                         wcr::region_segment(params.region));
    std::vector<std::string> locales = params.locales;
    if (locales.empty())
    {
        locales = manifest.locales;
    }
    std::vector<Artifact> data =
        expand_data(manifest, locales, params.mode, params.cinematics);
    Recipe run = assemble_recipe(*base, data, params.mode);
    wcr::apply_region_to_recipe(run, *base, params.region);
    return run;
}

/// On the interactive (double-click) path, keep the console open so the user
/// can read the result before the window closes.
static void pause_if_interactive(bool interactive)
{
    if (interactive)
    {
        printf("\nPress Enter to close...");
        std::string dummy;
        std::getline(std::cin, dummy);
    }
}

int main(int argc, char** argv)
{
    if (CURLcode cc = curl_global_init(CURL_GLOBAL_DEFAULT); cc != CURLE_OK)
    {
        printf("ERROR: curl_global_init failed (%d)\n", static_cast<int>(cc));
        return 1;
    }

    const bool interactive = (argc == 1);
    RunParams params;
    if (interactive)
    {
        print_banner();
        // Production locale source: fetch the partial manifest and read its
        // advertised locales. The list is region-independent.
        wcr::FetchLocales fetchLocales =
            [](const std::string& version) -> std::vector<std::string>
        {
            const Recipe* base = find_recipe(version);
            if (base == nullptr)
            {
                throw std::runtime_error("unknown version '" + version + "'");
            }
            Pointer ptr = parse_pointer(pointer_text_from_recipe(*base));
            Manifest m = fetch_manifest(ptr);
            return m.locales;
        };
        try
        {
            params = wcr::run_interactive(std::cin, std::cout, fetchLocales);
        }
        catch (const std::exception& e)
        {
            printf("ERROR: %s\n", e.what());
            curl_global_cleanup();
            pause_if_interactive(interactive);
            return 1;
        }
        // Resume: a previous journal auto-resumes (already-downloaded files are
        // skipped) UNLESS the user explicitly asks to start fresh. The
        // destructive clear requires an explicit yes, so EOF/Enter keep it.
        if (wcr::should_clear_journal(std::cin, std::cout,
                                      wcr::journal_exists(params.outDir),
                                      params.outDir))
        {
            wcr::clear_journal(params.outDir);
        }
    }
    else
    {
        std::string err;
        if (!parse_args(argc, argv, params, err))
        {
            printf("ERROR: %s\n", err.c_str());
            print_usage();
            curl_global_cleanup();
            return 2;
        }
    }

    int rc = 0;
    try
    {
        Recipe run = build_run_recipe(params);
        printf("Reconstructing WoW %s (build %s): %zu artifacts into %s\n",
               run.version.c_str(), run.build.c_str(),
               run.artifacts.size(), params.outDir.c_str());
        wcr::Torrent torrent;
        bool haveTorrent = false;
        if (!params.tfilPath.empty())
        {
            torrent = wcr::parse_torrent(wcr::read_file(params.tfilPath));
            haveTorrent = true;
        }
        wcr::ReconstructOpts opts;
        if (haveTorrent)
        {
            opts.torrent = &torrent;
        }
        wcr::Journal journal = wcr::load_journal(params.outDir);
        opts.journal = &journal;
        opts.regionFallback = wcr::region_fallbacks(params.region);
        long long total = wcr::total_bytes(run.artifacts);
        long long avail = wcr::free_space(params.outDir);
        if (!wcr::confirm_preflight(total, avail, params.yes,
                                    std::cin, std::cout))
        {
            rc = 2;
        }
        else
        {
            // On the interactive path, clear the menu/pre-flight chatter and
            // re-show the banner so the download starts on a clean screen.
            if (interactive)
            {
                std::system("cls");
                print_banner();
            }
            reconstruct(run, params.outDir, opts);
            if (params.mode == Mode::FullClient)
            {
                std::string cfgLocale =
                    params.locales.empty() ? "enUS" : params.locales.front();
                write_runonce(params.outDir, params.realmlist, cfgLocale,
                              1920, 1080);
                // Report only the realmlist that was actually written. The
                // default is the 127.0.0.1 localhost safeguard; a custom host
                // is reported plainly (no alarmist "private server" wording).
                if (params.realmlist == "127.0.0.1")
                {
                    printf("  Localhost safety realmlist: 127.0.0.1\n");
                }
                else
                {
                    printf("  Realmlist: %s (set via --realmlist)\n",
                           params.realmlist.c_str());
                }
            }
            printf("Done.\n");
        }
    }
    catch (const std::exception& e)
    {
        printf("ERROR: %s\n", e.what());
        rc = 1;
    }
    curl_global_cleanup();
    pause_if_interactive(interactive);
    return rc;
}
