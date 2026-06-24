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
 * @file progress.cpp
 * @brief Implementation of human_bytes and render_progress.
 */

#include "progress.h"
#include <cstdio>
#include <string>

namespace wcr
{
std::string human_bytes(long long n)
{
    // -1 is the sentinel for "unknown size".
    if (n < 0)
    {
        return "?";
    }

    // 1024-based thresholds.
    const long long kKB = 1024LL;
    const long long kMB = 1024LL * kKB;
    const long long kGB = 1024LL * kMB;
    const long long kTB = 1024LL * kGB;

    char buf[64];
    if (n >= kTB)
    {
        std::snprintf(buf, sizeof(buf), "%.2f TB",
                      (double)n / (double)kTB);
    }
    else if (n >= kGB)
    {
        std::snprintf(buf, sizeof(buf), "%.2f GB",
                      (double)n / (double)kGB);
    }
    else if (n >= kMB)
    {
        std::snprintf(buf, sizeof(buf), "%.2f MB",
                      (double)n / (double)kMB);
    }
    else if (n >= kKB)
    {
        std::snprintf(buf, sizeof(buf), "%.2f KB",
                      (double)n / (double)kKB);
    }
    else
    {
        std::snprintf(buf, sizeof(buf), "%lld B", n);
    }
    return buf;
}

std::string render_progress(const std::string& label,
                            long long now,
                            long long total,
                            long long bytesPerSec)
{
    std::string out = "  " + label + "  ";
    if (total > 0)
    {
        long long pct = now * 100LL / total;
        char pctBuf[16];
        std::snprintf(pctBuf, sizeof(pctBuf), "%lld%%", pct);
        out += pctBuf;
        out += "  ";
        out += human_bytes(now);
        out += " / ";
        out += human_bytes(total);
    }
    else
    {
        out += human_bytes(now);
    }
    if (bytesPerSec >= 0)
    {
        out += "  ";
        out += human_bytes(bytesPerSec);
        out += "/s";
    }
    return out;
}
} // namespace wcr
