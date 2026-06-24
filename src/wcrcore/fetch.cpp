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
 * @file fetch.cpp
 * @brief Source-dispatching fetch engine implementation.
 */

#include "fetch.h"
#include "download.h"
#include "zip.h"
#include "mpq.h"
#include "ptch.h"
#include "md5.h"
#include "journal.h"
#include "pieces.h"
#include <cstdio>
#include <filesystem>
#include <map>
#include <memory>
#include <stdexcept>
#include <vector>

namespace wcr
{
std::string repair_url(const std::string& base, const std::string& md5)
{
    if (md5.size() < 2)
    {
        throw std::runtime_error("repair_url: md5 too short: " + md5);
    }
    return base + "/" + md5.substr(0, 1) + "/" + md5.substr(1, 1) + "/" + md5;
}

std::string swap_base(const std::string& url, const std::string& from,
                      const std::string& to)
{
    std::string::size_type pos = url.find(from);
    if (pos == std::string::npos)
    {
        return url;
    }
    return url.substr(0, pos) + to + url.substr(pos + from.size());
}

std::string swap_region(const std::string& url, const std::string& toSeg)
{
    static const std::string kEu = "/EU/";
    static const std::string kNa = "/NA/";
    const std::string anchor = "wow-pod-retail";
    std::string::size_type a = url.find(anchor);
    if (a == std::string::npos)
    {
        return url;
    }
    std::string::size_type eu = url.find(kEu, a);
    std::string::size_type na = url.find(kNa, a);
    if (eu != std::string::npos)
    {
        return url.substr(0, eu) + toSeg + url.substr(eu + kEu.size());
    }
    if (na != std::string::npos)
    {
        return url.substr(0, na) + toSeg + url.substr(na + kNa.size());
    }
    return url;
}

std::vector<std::string> region_fallbacks(const std::string& region)
{
    if (region == "EU")
    {
        return {"/NA/"};
    }
    if (region == "NA")
    {
        return {"/EU/"};
    }
    return {};
}

std::string region_segment(const std::string& region)
{
    return (region == "NA") ? "/NA/" : "/EU/";
}

void apply_region_to_recipe(Recipe& run, const Recipe& base,
                            const std::string& region)
{
    for (MpqSource& m : run.mpqs)
    {
        m.url = swap_region(m.url, region_segment(region));
    }
    for (Artifact& a : run.artifacts)
    {
        if (a.outName == "WoW.mfil")
        {
            a.content =
                region_rewrite_mfil(a.content, base.regionManifests, region);
        }
    }
}

void verify_or_throw(const Bytes& data, const std::string& expectedMd5,
                     const std::string& name)
{
    std::string got = md5_hex(data);
    if (got != expectedMd5)
    {
        throw std::runtime_error("MD5 mismatch for " + name + ": expected " +
                                 expectedMd5 + " got " + got);
    }
}

void remove_build_scratch(const std::string& outDir,
                          const std::vector<std::string>& sourceFiles)
{
    std::error_code ec;
    for (const std::string& f : sourceFiles)
    {
        std::filesystem::remove(outDir + "/" + f, ec);
    }
    std::filesystem::remove(outDir + "/.wcr-journal", ec);
}

namespace
{
/// Return the final path segment (file name) of a URL.
std::string url_basename(const std::string& url)
{
    return url.substr(url.find_last_of('/') + 1);
}
} // namespace

void reconstruct(const Recipe& r, const std::string& outDir)
{
    reconstruct(r, outDir, ReconstructOpts{});
}

void reconstruct(const Recipe& r, const std::string& outDir,
                 const ReconstructOpts& opts)
{
    std::filesystem::create_directories(outDir);

    // These are declared before the try so the catch block and the post-try
    // success reporting can access them.
    std::map<std::string, Bytes> zipCache;
    std::vector<std::string> zipSourceFiles;
    std::vector<std::string> mpqSourceFiles;
    std::map<std::string, std::unique_ptr<MpqArchive>> mpqCache;
    int reportSizeChecked = 0;
    long long reportTotalBytes = 0;

    try
    {
    // Download each zip source once and keep its bytes in memory.
    for (const ZipSource& z : r.zips)
    {
        std::string dest = outDir + "/" + url_basename(z.url);
        printf("Downloading %s...\n", url_basename(z.url).c_str());
        DownloadOpts zopts;
        zopts.progress_label = url_basename(z.url);
        download_file(z.url, dest, zopts);
        // Track the zip file BEFORE read_file: if read_file throws (e.g. a
        // disk error after a successful download), the exception path below
        // needs zipSourceFiles to be populated so it can remove the stranded
        // zip file.  On the success path the inline remove(dest) below cleans
        // it up, and the catch cleanup is a harmless no-op.
        zipSourceFiles.push_back(url_basename(z.url));
        zipCache[z.key] = read_file(dest);
        std::filesystem::remove(dest);
    }

    // Download each MPQ source once and open it for reading.
    for (const MpqSource& m : r.mpqs)
    {
        std::string dest = outDir + "/" + url_basename(m.url);
        DownloadOpts mopts;
        mopts.expected_size = m.size;
        mopts.progress_label = url_basename(m.url);
        printf("Downloading %s...\n", url_basename(m.url).c_str());
        download_file(m.url, dest, mopts);
        // Track BEFORE opening: if MpqArchive construction throws (e.g. corrupt
        // MPQ that downloaded at the right size but is invalid), the file is
        // still on disk and must be cleaned up by the exception path below.
        mpqSourceFiles.push_back(url_basename(m.url));
        mpqCache[m.key] = std::make_unique<MpqArchive>(dest);
    }

    // Produce, verify, and write every artifact.
    for (const Artifact& a : r.artifacts)
    {
        std::string dst = outDir + "/" + a.outName;
        std::filesystem::create_directories(
            std::filesystem::path(dst).parent_path());
        std::string part;
        Bytes data;
        bool skipped = false;
        bool markPending = false;
        switch (a.source)
        {
            case Source::RepairMd5:
            {
                part = dst + ".part";
                DownloadOpts dopts;
                dopts.resume = false;
                dopts.progress_label = a.outName;
                download_file(repair_url(r.repairBase, a.md5), part, dopts);
                data = read_file(part);
                break;
            }
            case Source::ZipMember:
            {
                auto it = zipCache.find(a.zipKey);
                if (it == zipCache.end())
                {
                    throw std::runtime_error("unknown zipKey: " + a.zipKey);
                }
                data = zip_extract_member(it->second, a.outName);
                break;
            }
            case Source::MpqPtch:
            {
                auto base = mpqCache.find(a.baseMpqKey);
                auto patch = mpqCache.find(a.patchMpqKey);
                if (base == mpqCache.end() || patch == mpqCache.end())
                {
                    throw std::runtime_error("unknown mpqKey for " + a.outName);
                }
                Bytes b = base->second->extract(a.basePath);
                Bytes p = patch->second->extract(a.patchPath);
                data = apply_ptch(b, p);
                break;
            }
            case Source::MpqExtract:
            {
                auto src = mpqCache.find(a.baseMpqKey);
                if (src == mpqCache.end())
                {
                    throw std::runtime_error("unknown mpqKey for " + a.outName);
                }
                data = src->second->extract(a.basePath);
                if (!a.patchMpqKey.empty())
                {
                    auto patch = mpqCache.find(a.patchMpqKey);
                    if (patch == mpqCache.end())
                    {
                        throw std::runtime_error("unknown patchMpqKey for " +
                                                 a.outName);
                    }
                    data = apply_ptch(data, patch->second->extract(a.patchPath));
                }
                break;
            }
            case Source::PlainUrl:
            {
                part = dst + ".part";
                std::filesystem::create_directories(
                    std::filesystem::path(dst).parent_path());
                DownloadOpts popts;
                popts.resume = true;
                popts.expected_size = a.size;
                popts.progress_label = a.outName;
                if (opts.journal && is_done(*opts.journal, a.outName))
                {
                    // Only skip if the destination file is actually on disk.
                    // When a size is known, also confirm the file size matches
                    // so a truncated output is re-downloaded, not silently
                    // accepted as complete.
                    std::error_code fec;
                    bool dstOk = std::filesystem::exists(dst, fec);
                    if (dstOk && a.size >= 0)
                    {
                        auto fsz = std::filesystem::file_size(dst, fec);
                        if (fec || static_cast<long long>(fsz) != a.size)
                        {
                            dstOk = false;
                        }
                    }
                    if (dstOk)
                    {
                        skipped = true;
                        break;
                    }
                    // File missing or wrong size — fall through and re-download.
                }
                try
                {
                    download_file(a.url, part, popts);
                }
                catch (const std::exception&)
                {
                    bool recovered = false;
                    for (const std::string& fb : opts.regionFallback)
                    {
                        try
                        {
                            download_file(swap_region(a.url, fb), part, popts);
                            recovered = true;
                            break;
                        }
                        catch (const std::exception&)
                        {
                            // try the next fallback region
                        }
                    }
                    if (!recovered)
                    {
                        if (a.optional)
                        {
                            std::error_code rec;
                            std::filesystem::remove(part, rec);
                            skipped = true;
                            break;
                        }
                        throw;
                    }
                }
                // Only load bytes into RAM when an MD5 must be verified.
                // Data files carry no MD5, so multi-GB files stay on disk and
                // are size-checked via file_size + (optionally) streamed for
                // piece verification.
                if (!a.md5.empty())
                {
                    data = read_file(part);
                }
                if (opts.torrent != nullptr)
                {
                    printf("  Verifying %s (piece SHA-1)...\n",
                           a.outName.c_str());
                    try
                    {
                        verify_file_pieces(*opts.torrent, a.outName, part);
                    }
                    catch (...)
                    {
                        std::error_code rec;
                        std::filesystem::remove(part, rec);
                        throw;
                    }
                }
                if (opts.journal && !skipped)
                {
                    markPending = true;
                }
                break;
            }
            case Source::Generated:
            {
                data = Bytes(a.content.begin(), a.content.end());
                break;
            }
        }
        if (skipped)
        {
            printf("  %-28s SKIPPED (optional, source missing)\n",
                   a.outName.c_str());
            continue;
        }
        // Size comes from disk for download-to-disk sources (RepairMd5,
        // PlainUrl) so multi-GB Data files are never loaded into RAM; in-memory
        // sources (ZipMember/MpqPtch/Generated) use the buffer length.
        long long actualSize = part.empty()
            ? static_cast<long long>(data.size())
            : static_cast<long long>(std::filesystem::file_size(part));
        // Verification: for PlainUrl, discard the .part file on mismatch so
        // a re-run re-downloads from scratch (corrupt bytes, not just network
        // failure which must remain resumable).
        try
        {
            if (!a.md5.empty())
            {
                verify_or_throw(data, a.md5, a.outName);
            }
            if (a.size >= 0 && actualSize != a.size)
            {
                throw std::runtime_error(
                    "size mismatch for " + a.outName + ": expected " +
                    std::to_string(a.size) + " got " +
                    std::to_string(actualSize));
            }
        }
        catch (...)
        {
            // Discard any .part file on verification failure so that a re-run
            // downloads from scratch rather than resuming corrupt bytes.
            // Covers both PlainUrl and RepairMd5 (both download to `part`).
            if (!part.empty())
            {
                std::error_code rec;
                std::filesystem::remove(part, rec);
            }
            throw;
        }
        if (a.size >= 0)
        {
            ++reportSizeChecked;
        }
        if (a.source == Source::RepairMd5 || a.source == Source::PlainUrl)
        {
            std::filesystem::rename(part, dst);
        }
        else
        {
            write_file(dst, data);
        }
        printf("  %-28s %lld bytes  [%s]\n", a.outName.c_str(), actualSize,
               a.md5.c_str());
        reportTotalBytes += actualSize;
        if (markPending && opts.journal)
        {
            mark_done(*opts.journal, a.outName);
        }
    }
    } // end try
    catch (...)
    {
        // On any failure: close open MPQ handles first (StormLib keeps file
        // handles open; Windows blocks deletion of open files), then remove
        // whatever scratch files were downloaded before the failure.
        // zipSourceFiles covers the case where read_file throws after the zip
        // was successfully downloaded but before the inline remove(); the
        // cleanup is a harmless no-op if the file is already gone.
        mpqCache.clear();
        remove_build_scratch(outDir, zipSourceFiles);
        remove_build_scratch(outDir, mpqSourceFiles);
        throw;
    }

    // Success: strip the build scratch so the output is a pristine client
    // (the source MPQs are already correctly present in Data/ and Updates/).
    // This is only reached after the artifact loop completes, i.e. on FULL
    // success — any failed/interrupted artifact throws earlier and never gets
    // here, so the journal has nothing left to resume and is safe to remove.
    // Close the MPQ handles FIRST: while an MpqArchive is open, StormLib keeps
    // the source .MPQ open and Windows refuses to delete an open file (the
    // remove fails silently via error_code), stranding the scratch in the root.
    mpqCache.clear();
    remove_build_scratch(outDir, mpqSourceFiles);

    int reportPieceVerified = (opts.torrent != nullptr) ? reportSizeChecked : 0;
    printf("\n=== Reconstruction complete ===\n");
    printf("  artifacts written : %zu\n", r.artifacts.size());
    printf("  total bytes       : %lld\n", reportTotalBytes);
    printf("  size-checked      : %d\n", reportSizeChecked);
    printf("  piece-verified    : %d\n", reportPieceVerified);
    printf("\nFinal safety state:\n");
    printf("  CDN reconstruction complete\n");
    printf("  Blizzard files distributed by this tool: no\n");
}
} // namespace wcr
