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
 * @file mfil.cpp
 * @brief Implementation of WoW.mfil pointer + partial manifest parsing.
 */

#include "mfil.h"
#include "download.h"
#include "bytes.h"
#include "md5.h"
#include "locale_token.h"
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <stdexcept>
#include <vector>

namespace
{
// Strip leading and trailing ASCII whitespace (space, tab, CR, LF) from s.
static std::string trim(const std::string& s)
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

// Split text into lines on '\n'; each returned line has any trailing '\r'
// removed so CRLF and LF manifests parse identically.
static std::vector<std::string> split_lines(const std::string& text)
{
    std::vector<std::string> out;
    std::string::size_type start = 0;
    while (start <= text.size())
    {
        std::string::size_type nl = text.find('\n', start);
        std::string line = (nl == std::string::npos)
                               ? text.substr(start)
                               : text.substr(start, nl - start);
        if (!line.empty() && line.back() == '\r')
        {
            line.pop_back();
        }
        out.push_back(line);
        if (nl == std::string::npos)
        {
            break;
        }
        start = nl + 1;
    }
    return out;
}

// If line is "key=value" (split on the FIRST '='), set key/value (both
// trimmed) and return true; otherwise return false.
static bool parse_kv(const std::string& line, std::string& key,
                     std::string& value)
{
    std::string::size_type eq = line.find('=');
    if (eq == std::string::npos)
    {
        return false;
    }
    key = trim(line.substr(0, eq));
    value = trim(line.substr(eq + 1));
    return true;
}

// Extract the locale tag from a "locale_<L>" value; "" if the prefix is
// absent (e.g. "locale_enUS" -> "enUS", "data" -> "").
static std::string locale_of(const std::string& value)
{
    const std::string prefix = "locale_";
    if (value.compare(0, prefix.size(), prefix) == 0)
    {
        return value.substr(prefix.size());
    }
    return std::string();
}

// Return the last path segment of relPath (basename), splitting on '/' or
// '\\' (e.g. "Data/alternate.MPQ" -> "alternate.MPQ").
static std::string base_name(const std::string& relPath)
{
    std::string::size_type p = relPath.find_last_of("/\\");
    return (p == std::string::npos) ? relPath : relPath.substr(p + 1);
}

// True when c is an ASCII hexadecimal digit (0-9, a-f, A-F).
static bool is_hex(char c)
{
    return std::isxdigit(static_cast<unsigned char>(c)) != 0;
}

// Extract the 32-hex-character MD5 embedded in a content-addressed partial
// manifest name. Blizzard names the partial manifest wow-<build>-<MD5>.mfil,
// so the hash is the 32 hex chars immediately preceding the ".mfil" suffix,
// delimited by a leading '-'. Returns the hash on success, or "" when the name
// carries no parseable hash (e.g. a synthetic test fixture) -- callers treat
// the absent-hash case as "no anchor to check against". The input may be a URL
// or a bare name; only the basename's tail is inspected.
static std::string hash_from_manifest_name(const std::string& name)
{
    std::string base = base_name(name);
    const std::string ext = ".mfil";
    if (base.size() < ext.size() + 33)
    {
        return std::string();
    }
    if (base.compare(base.size() - ext.size(), ext.size(), ext) != 0)
    {
        return std::string();
    }
    // The 32 chars ending just before ".mfil" must all be hex, and the char
    // immediately before them must be the '-' delimiter (wow-<build>-<MD5>).
    std::string::size_type hashEnd = base.size() - ext.size();
    std::string::size_type hashStart = hashEnd - 32;
    if (hashStart == 0 || base[hashStart - 1] != '-')
    {
        return std::string();
    }
    for (std::string::size_type i = hashStart; i < hashEnd; ++i)
    {
        if (!is_hex(base[i]))
        {
            return std::string();
        }
    }
    return base.substr(hashStart, 32);
}

// Case-insensitive equality of two ASCII strings of equal length.
static bool iequals_ascii(const std::string& a, const std::string& b)
{
    if (a.size() != b.size())
    {
        return false;
    }
    for (std::string::size_type i = 0; i < a.size(); ++i)
    {
        if (std::tolower(static_cast<unsigned char>(a[i])) !=
            std::tolower(static_cast<unsigned char>(b[i])))
        {
            return false;
        }
    }
    return true;
}

// Return the locale implied by a file's directory segment, validated against
// the advertised locale set, or "" if none applies. Locale-specific Data files
// live under "Data/<locale>/..."; Blizzard tags the per-build
// wow-update-<locale>-<build>.MPQ patches as path=base even though they are
// locale content, so the directory is the authoritative locale signal. Only
// advertised locales are recognised, so non-locale subdirs such as
// "Data/Interface/..." correctly remain base.
static std::string locale_from_path(const std::string& relPath,
                                    const std::vector<std::string>& known)
{
    std::string::size_type p1 = relPath.find('/');
    if (p1 == std::string::npos)
    {
        return std::string();
    }
    std::string::size_type p2 = relPath.find('/', p1 + 1);
    if (p2 == std::string::npos)
    {
        return std::string();
    }
    std::string seg = relPath.substr(p1 + 1, p2 - (p1 + 1));
    for (const std::string& l : known)
    {
        if (l == seg)
        {
            return seg;
        }
    }
    return std::string();
}

// True when relPath is a shared, locale-independent asset directly under
// "Data/Interface/" (e.g. the shared Data/Interface/Cinematics/ intro movies).
// Blizzard tags the western copy of those with a single locale (path=locale_frFR)
// even though they belong in every client; this forces them to base. Per-locale
// assets live under "Data/<locale>/Interface/..." and do NOT match this prefix.
static bool is_shared_interface_path(const std::string& relPath)
{
    const std::string prefix = "Data/Interface/";
    return relPath.compare(0, prefix.size(), prefix) == 0;
}

// v3 partial manifests (e.g. MoP 5.4.8) mark each file with category tag= lines
// instead of serverpath=locale_. A tag value not in this fixed category set is
// a locale code. Mirrors the C# ManifestFile's LocaleDetectLineVersion3
// IgnoredTags, and is used to build the advertised locale list for v3.
static bool is_non_locale_tag(const std::string& tag)
{
    static const char* const kNonLocale[] = {
        "base", "OSX", "Win", "ALT", "EXP1", "EXP2", "EXP3", "EXP4"};
    for (const char* t : kNonLocale)
    {
        if (tag == t)
        {
            return true;
        }
    }
    return false;
}
} // namespace

namespace wcr
{
Pointer parse_pointer(const std::string& pointerText)
{
    Pointer p;
    for (const std::string& line : split_lines(pointerText))
    {
        std::string key;
        std::string value;
        if (!parse_kv(line, key, value))
        {
            continue;
        }
        if (key == "location")
        {
            p.dataBaseUrl = value;
        }
        else if (key == "manifest_partial")
        {
            p.partialName = value;
        }
    }
    return p;
}

Manifest parse_mfil(const std::string& text, const std::string& dataBaseUrl)
{
    Manifest m;
    m.dataBaseUrl = dataBaseUrl;

    bool inBlock = false;
    MfilRecord cur;
    std::string curPath;

    // Emit the current block's record (only when it carries a name=).
    // Each file's locale is derived from its OWN per-file path= field via
    // locale_of(): path=base (or absent) -> "" (locale-independent);
    // path=locale_<X> -> "<X>".  The serverpath= declarations are only used
    // to populate Manifest.locales (the advertised locale list).
    auto flush = [&]()
    {
        if (inBlock && !cur.relPath.empty())
        {
            // Shared assets under Data/Interface/ are locale-independent even
            // though Blizzard tags them with a single locale (path=locale_frFR
            // on the shared Data/Interface/Cinematics/ movies); force them to
            // base so every client gets them. Otherwise derive the locale from
            // path=, falling back to the Data/<locale>/ directory (the
            // wow-update-<locale>-<build>.MPQ patches are mis-tagged path=base).
            if (is_shared_interface_path(cur.relPath))
            {
                cur.locale.clear();
            }
            else
            {
                cur.locale = locale_of(curPath);
                if (cur.locale.empty())
                {
                    cur.locale = locale_from_path(cur.relPath, m.locales);
                }
            }
            m.files.push_back(cur);
        }
        cur = MfilRecord{};
        curPath.clear();
        inBlock = false;
    };

    for (const std::string& line : split_lines(text))
    {
        std::string key;
        std::string value;
        if (!parse_kv(line, key, value))
        {
            continue;
        }
        if (key == "version")
        {
            m.version = std::atoi(value.c_str());
        }
        else if (key == "file")
        {
            flush();
            inBlock = true;
        }
        else if (key == "name")
        {
            cur.relPath = value;
        }
        else if (key == "size")
        {
            cur.size = std::atoll(value.c_str());
        }
        else if (key == "serverpath")
        {
            // Collect advertised locales from serverpath= declarations.
            // Security (B2): validate each locale token before accepting it;
            // a poisoned value such as "../../etc" is silently dropped to
            // prevent it from being offered as an interactive choice or used
            // as a filesystem path component.
            std::string loc = locale_of(value);
            if (!loc.empty() && is_valid_locale_token(loc))
            {
                bool seen = false;
                for (const std::string& l : m.locales)
                {
                    if (l == loc)
                    {
                        seen = true;
                    }
                }
                if (!seen)
                {
                    m.locales.push_back(loc);
                }
            }
        }
        else if (key == "path")
        {
            curPath = value;
        }
        else if (key == "tag")
        {
            // v3 manifests advertise locales via per-file tag= lines (there is
            // no serverpath=locale_). A tag that is not a fixed category tag is
            // a locale code; collect the distinct set. Per-file locale still
            // comes from the Data/<locale>/ directory (locale_from_path) -- the
            // same way the C# tool's IsAcceptedFile filters by directory.
            // Security (B2): validate each candidate locale token; a poisoned
            // value such as "../../etc" is silently dropped.
            if (!is_non_locale_tag(value) && is_valid_locale_token(value))
            {
                bool seen = false;
                for (const std::string& l : m.locales)
                {
                    if (l == value)
                    {
                        seen = true;
                    }
                }
                if (!seen)
                {
                    m.locales.push_back(value);
                }
            }
        }
    }
    flush();
    return m;
}

Manifest fetch_manifest(const Pointer& p, bool requireHash)
{
    // Download the partial manifest to a temp file, read it, and parse it.
    // The data base URL is preserved so the returned records' paths resolve
    // against it later.
    std::filesystem::path tmp =
        std::filesystem::temp_directory_path() / p.partialName;

    DownloadOpts opts;
    opts.resume = false;
    download_file(p.dataBaseUrl + p.partialName, tmp.string(), opts);

    Bytes raw = read_file(tmp.string());
    std::error_code ec;
    std::filesystem::remove(tmp, ec);

    // Security (B1): authenticate the manifest against its PINNED, content-
    // addressed name. The partial manifest is fetched over plain HTTP and would
    // otherwise be trusted wholesale, letting a MITM / compromised CDN edge
    // inject records that overwrite already-MD5-verified root binaries or smuggle
    // malicious locale strings. Blizzard names the manifest wow-<build>-<MD5>.mfil
    // where <MD5> is the md5 of the exact body, and that name comes from our
    // recipe (not the network), so the hash is a forge-proof trust anchor. Verify
    // md5(body) == that hash and fail closed on mismatch.
    //
    // Fail-closed for real callers (requireHash=true, the default): if the name
    // carries no parseable hash, throw immediately.  Real recipes always embed a
    // 32-hex content hash in the manifest name; a hash-less name indicates a
    // malformed or synthetic recipe that should not silently skip auth.
    // Pass requireHash=false only for synthetic test fixtures.
    std::string expected = hash_from_manifest_name(p.partialName);
    if (expected.empty())
    {
        if (requireHash)
        {
            throw std::runtime_error(
                "manifest name carries no verifiable content-hash: " +
                p.partialName);
        }
        // requireHash=false: synthetic fixture — no anchor, skip the check.
    }
    else
    {
        std::string actual = md5_hex(raw);
        if (!iequals_ascii(actual, expected))
        {
            throw std::runtime_error("manifest hash mismatch for " +
                                     p.partialName + ": expected " + expected +
                                     " got " + actual);
        }
    }

    std::string text(raw.begin(), raw.end());
    return parse_mfil(text, p.dataBaseUrl);
}
} // namespace wcr
