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
 * @file recipe.cpp
 * @brief Built-in version recipes for WoW 4.3.4 (Cataclysm) and 5.4.8
 *        (Mists of Pandaria), plus find_recipe() dispatch.
 */

#include "recipe.h"
#include <utility>
namespace wcr
{
namespace
{
/// Build a RepairMd5 artifact.
Artifact repair(std::string name, std::string md5)
{
    Artifact a;
    a.outName = std::move(name);
    a.md5 = std::move(md5);
    a.source = Source::RepairMd5;
    return a;
}
/// Build a ZipMember artifact (member name equals outName).
Artifact zipMember(std::string name, std::string md5, std::string zipKey)
{
    Artifact a;
    a.outName = std::move(name);
    a.md5 = std::move(md5);
    a.source = Source::ZipMember;
    a.zipKey = std::move(zipKey);
    return a;
}
/// Build an MpqPtch artifact.
Artifact mpqPtch(std::string name, std::string md5, std::string baseKey,
                 std::string basePath, std::string patchKey,
                 std::string patchPath)
{
    Artifact a;
    a.outName = std::move(name);
    a.md5 = std::move(md5);
    a.source = Source::MpqPtch;
    a.baseMpqKey = std::move(baseKey);
    a.basePath = std::move(basePath);
    a.patchMpqKey = std::move(patchKey);
    a.patchPath = std::move(patchPath);
    return a;
}
/// Build an MpqExtract artifact: plain extraction of mpqPath from mpqKey.
Artifact mpqExtract(std::string name, std::string md5, std::string mpqKey,
                    std::string mpqPath)
{
    Artifact a;
    a.outName = std::move(name);
    a.md5 = std::move(md5);
    a.source = Source::MpqExtract;
    a.baseMpqKey = std::move(mpqKey);
    a.basePath = std::move(mpqPath);
    return a;
}
/// Build an MpqExtract artifact that extracts from base then applies a PTCH
/// from the patch MPQ (for files Blizzard ships as a patch, e.g. MovieProxy).
Artifact mpqExtractPatched(std::string name, std::string md5,
                           std::string baseKey, std::string basePath,
                           std::string patchKey, std::string patchPath)
{
    Artifact a = mpqExtract(std::move(name), std::move(md5), std::move(baseKey),
                            std::move(basePath));
    a.patchMpqKey = std::move(patchKey);
    a.patchPath = std::move(patchPath);
    return a;
}
/// Build a Generated artifact (literal content embedded in the recipe).
Artifact generated(std::string name, std::string md5, std::string content)
{
    Artifact a;
    a.outName = std::move(name);
    a.md5 = std::move(md5);
    a.source = Source::Generated;
    a.content = std::move(content);
    return a;
}
/// Build a PlainUrl artifact (download from a direct URL, verify by MD5).
Artifact plainUrl(std::string name, std::string md5, std::string url)
{
    Artifact a;
    a.outName = std::move(name);
    a.md5 = std::move(md5);
    a.source = Source::PlainUrl;
    a.url = std::move(url);
    return a;
}
} // namespace

const Recipe& recipe_cata434()
{
    static const Recipe r = {
        "4.3.4",
        "15595",
        "http://dist.blizzard.com.edgesuite.net/repair/wow",
        {
            {"live64",
             "http://eu.media.battle.net.edgesuite.net/downloads/"
             "wow-installers/live/WoWLive-64-Win-15595.zip"}
        },
        {},
        {
            repair("Wow.exe", "DE5A2E274F2D3F2B89A2E6EC9CD8FD2A"),
            repair("WowError.exe", "78766BBBFC6F9E5DA5D930CB11F0A1E1"),
            repair("BackgroundDownloader.exe",
                   "E1FC69A72E4E23A96DBD535B372974A8"),
            repair("Battle.net.dll", "24433A51A32335A39D2AF8CB55C467D3"),
            repair("Blizzard Updater.exe",
                   "82EF43D5F8D1B1C87C3505ECD241FFF6"),
            repair("dbghelp.dll", "4003E34416EBD25E4C115D49DC15E1A7"),
            repair("divxdecoder.dll", "57E72CAE12091DAFA29A8E4DB8B4F1D1"),
            // Legacy support DLLs recovered from the base-Win.MPQ binary sweep:
            // ijl15 = Intel JPEG decoder, unicows = the Win9x Unicode shim.
            // Both predate the 15595 patch (static, never rebuilt) and Blizzard
            // keeps them in /repair with the same md5s MoP's recipe uses.
            repair("ijl15.dll", "1AA06C81A0621E277E755B965B5E4B5F"),
            repair("unicows.dll", "E1102CEDF0C818984C2ACA2A666D4C5F"),
            repair("Microsoft.VC80.CRT.manifest",
                   "D34B3DA03C59F38A510EAA8CCC151EC7"),
            repair("msvcr80.dll", "1169436EE42F860C7DB37A4692B38F0E"),
            repair("Repair.exe", "E198F00FE056B24ED58B36E1C6A048F4"),
            // Launcher.exe is the legacy Blizzard self-updater. If run, it
            // streams the client toward current retail and would corrupt this
            // pinned 4.3.4 install (the 4.3.4 autopatcher's "never run
            // launcher.exe" warning). It is included only to mirror a pristine
            // retail folder byte-for-byte -- the file is inert unless launched.
            repair("Launcher.exe", "C7C7121E1DD819088403F514FEBD06BA"),
            zipMember("Wow-64.exe", "5ACD2205377352083D2D98B89F48B602",
                      "live64"),
            zipMember("Battle.net-64.dll",
                      "5CA22973EDF3D10F9C69297A1EB28058", "live64"),
            zipMember("MovieProxy.exe", "37EC741FCBDEEBD01F90D9877D872EA1",
                      "live64"),
            // md5 matches the retail 4.3.4 WoW.mfil byte-for-byte (a test
            // asserts it). If Blizzard rotates the CDN pointer file, BOTH
            // this literal and its test (tests/test_recipe.cpp) must be
            // updated in lockstep -- it will not auto-detect staleness.
            generated("WoW.mfil", "F8E7D7BA6CDE053B1A9F85BD36980A72",
                "version=2\r\n"
                "server=akamai\r\n"
                "\tlocation=http://dist.blizzard.com.edgesuite.net/"
                "wow-pod-retail/EU/15050.direct/\r\n"
                "manifest_partial=wow-15595-"
                "0C3502F50D17376754B9E9CB0109F4C5.mfil\r\n")
        }
    };
    return r;
}

const Recipe& recipe_mop548()
{
    static const std::string cdn =
        "http://dist.blizzard.com.edgesuite.net/wow-pod-retail/EU/"
        "15890.direct/";
    static const Recipe r = {
        "5.4.8",
        "18414",
        "http://dist.blizzard.com.edgesuite.net/repair/wow",
        {},
        {
            {"base", cdn + "Data/base-Win.MPQ", 31096584},
            {"final", cdn + "Updates/wow-0-18414-Win-final.MPQ", 21729424}
        },
        {
            mpqPtch("Wow.exe", "24FD2CBB340D57C51B6F7A1C1D60E693", "base",
                    "Wow.exe", "final", "pc-game-hdfiles\\Wow.exe"),
            mpqPtch("Wow-64.exe", "96EF9239F97336F453562D350C33BCC7", "base",
                    "Wow-64.exe", "final", "pc-game-hdfiles\\Wow-64.exe"),
            // Root binaries fetched from Blizzard's content-addressed /repair
            // store (zero redistribution). MD5s captured from a real 5.4.8
            // client; the boot-critical DLLs Battle.net.dll + msvcr80.dll turn
            // out byte-identical to 4.3.4's and ARE in /repair (MPQ-verified).
            // Binaries that 404 in /repair are NOT boot-critical and are left
            // for the user's included Repair.exe to pull from Blizzard:
            // Battle.net-64.dll, WowError(.64).exe, MovieProxy.exe, the Utils\
            // browser stack, BlizzardError, Scan(.64), SystemSurvey, and
            // "World of Warcraft Launcher".
            repair("BackgroundDownloader.exe",
                   "E1FC69A72E4E23A96DBD535B372974A8"),
            repair("Battle.net.dll", "24433A51A32335A39D2AF8CB55C467D3"),
            repair("Blizzard Updater.exe",
                   "82EF43D5F8D1B1C87C3505ECD241FFF6"),
            repair("dbghelp.dll", "4003E34416EBD25E4C115D49DC15E1A7"),
            repair("divxdecoder.dll", "57E72CAE12091DAFA29A8E4DB8B4F1D1"),
            repair("ijl15.dll", "1AA06C81A0621E277E755B965B5E4B5F"),
            repair("Launcher.exe", "C7C7121E1DD819088403F514FEBD06BA"),
            repair("Microsoft.VC80.CRT.manifest",
                   "D34B3DA03C59F38A510EAA8CCC151EC7"),
            repair("msvcr80.dll", "1169436EE42F860C7DB37A4692B38F0E"),
            repair("Repair.exe", "859C93F0F2CCE243AB5CC94BB2E54D35"),
            repair("unicows.dll", "E1102CEDF0C818984C2ACA2A666D4C5F"),
            // Auxiliary binaries Blizzard removed from /repair (and whose own
            // Repair.exe backend is now dead), recovered byte-exact from the
            // already-downloaded base/final MPQs -- no new downloads. md5s
            // verified against a real installed client. 8 are plain extracts;
            // only MovieProxy.exe ships as a PTCH (base + final patch). The
            // Utils\ browser stack is written under Utils\ to match a real
            // client. NOT recovered: Scan(.64).dll (Warden, fetched online at
            // runtime, in no install MPQ) and the self-updating launcher
            // (deliberately excluded -- it would patch the pinned build).
            mpqExtract("Battle.net-64.dll",
                       "5CA22973EDF3D10F9C69297A1EB28058", "base",
                       "Battle.net-64.dll"),
            mpqExtract("WowError.exe", "EFEF12D480F64B850A6B1C8D5850A484",
                       "base", "WowError.exe"),
            mpqExtract("WowError-64.exe", "A6CCC044CD8756F57330B54ED2C443E6",
                       "base", "WowError-64.exe"),
            mpqExtractPatched("MovieProxy.exe",
                              "988F5E3A0FEB790862F69DE423F0E67A", "base",
                              "MovieProxy.exe", "final",
                              "pc-game-hdfiles\\MovieProxy.exe"),
            mpqExtract("BlizzardError.exe",
                       "98CB5B27549A3C9DD5CBC4F58F5A5BDB", "final",
                       "pc-game-hdfiles\\BlizzardError.exe"),
            mpqExtract("SystemSurvey.exe",
                       "5E9E75B78AF45FE376022B4D61767109", "final",
                       "pc-game-hdfiles\\SystemSurvey.exe"),
            mpqExtract("Utils\\WowBrowserProxy.exe",
                       "34D23F2255A984429829F50CC92F50DB", "final",
                       "pc-game-hdfiles\\Utils\\WowBrowserProxy.exe"),
            mpqExtract("Utils\\libcef.dll",
                       "EC729CC8FEADAB8C4C2108D83E8BFE27", "final",
                       "pc-game-hdfiles\\Utils\\libcef.dll"),
            mpqExtract("Utils\\icudt.dll",
                       "5434E18B933E03F274D8DA59FDA4C676", "final",
                       "pc-game-hdfiles\\Utils\\icudt.dll"),
            // Data-layer pointer (mirrors recipe_cata434's WoW.mfil). The 18414
            // partial-manifest name is REGION-SPECIFIC (both probed live): EU
            // serves wow-18414-E68C6C84...mfil (the MangosWoWRegeneration
            // WoW54818414 value -- NOT stale), NA serves wow-18414-447E3E61...
            // (the gist / a real NA client). Using EU to match the EU base/final
            // MPQ source above. Authored single-akamai pointer (a real client's
            // WoW.mfil also lists a dead llnw.blizzard.com fallback we drop);
            // md5 empty since this is authored, not a byte-matched retail file.
            generated("WoW.mfil", "",
                "version=2\r\n"
                "server=akamai\r\n"
                "\tlocation=http://dist.blizzard.com.edgesuite.net/"
                "wow-pod-retail/EU/15890.direct/\r\n"
                "manifest_partial=wow-18414-"
                "E68C6C849BBD16D2A8A153AFC865062F.mfil\r\n")
        },
        // regionManifests: MoP's partial-manifest name is region-locked
        // (EU/E68C live, NA/447E live; cross-region 404s).
        {{"EU", "E68C6C849BBD16D2A8A153AFC865062F"},
         {"NA", "447E3E618F731CCBF4F7D2C4E56C5644"}}
    };
    return r;
}

const Recipe* find_recipe(const std::string& version)
{
    if (version == "4.3.4")
    {
        return &recipe_cata434();
    }
    if (version == "5.4.8")
    {
        return &recipe_mop548();
    }
    return nullptr;
}

std::string region_rewrite_mfil(
    const std::string& content,
    const std::vector<std::pair<std::string, std::string>>& regionManifests,
    const std::string& region)
{
    if (regionManifests.empty())
    {
        return content;
    }
    std::string target;
    std::string other;
    for (const auto& pr : regionManifests)
    {
        if (pr.first == region)
        {
            target = pr.second;
        }
        else
        {
            other = pr.second;
        }
    }
    if (target.empty() || other.empty())
    {
        return content;
    }
    std::string out = content;
    // Swap the manifest name (other -> target).
    // Anchor the search to start from the "manifest_partial=" key so that an
    // occurrence of the hash earlier in the content (e.g. in the location= URL)
    // is not inadvertently replaced.  "manifest_partial=" always precedes the
    // hash in a well-formed WoW.mfil pointer, so the anchored search finds the
    // correct occurrence while leaving any earlier one untouched.
    static const std::string kAnchor = "manifest_partial=";
    std::string::size_type anchorPos = out.find(kAnchor);
    std::string::size_type n = (anchorPos != std::string::npos)
        ? out.find(other, anchorPos)
        : out.find(other);
    if (n != std::string::npos)
    {
        out = out.substr(0, n) + target + out.substr(n + other.size());
    }
    // Swap the region URL segment to match.
    const std::string fromSeg = (region == "NA") ? "/EU/" : "/NA/";
    const std::string toSeg = (region == "NA") ? "/NA/" : "/EU/";
    std::string::size_type s = out.find(fromSeg);
    if (s != std::string::npos)
    {
        out = out.substr(0, s) + toSeg + out.substr(s + fromSeg.size());
    }
    return out;
}
} // namespace wcr
