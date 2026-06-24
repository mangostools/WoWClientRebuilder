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
 * @file download.cpp
 * @brief libcurl-based file downloader with resume, size-validation, and
 *        optional live progress display.
 */

#include "download.h"
#include "progress.h"
#include <chrono>
#include <curl/curl.h>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <sys/stat.h>

namespace
{
// libcurl write callback: forward incoming data directly to the FILE* passed
// as userdata.  Returns the number of bytes consumed; mismatch signals error.
size_t write_cb(char* p, size_t s, size_t n, void* fp)
{
    return fwrite(p, 1, s * n, (FILE*)fp);
}

// Returns the on-disk size of path, or -1 if the file does not exist.
// Uses 64-bit stat on Windows so files larger than 2 GB are handled correctly.
static long long file_size(const std::string& path)
{
#ifdef _WIN32
    struct _stat64 st;
    return _stat64(path.c_str(), &st) == 0 ? (long long)st.st_size : -1;
#else
    struct stat st;
    return stat(path.c_str(), &st) == 0 ? (long long)st.st_size : -1;
#endif
}

/// State block threaded through the libcurl XFERINFO callback.
struct ProgressState
{
    /// Short label shown on the progress line (filename or description).
    std::string label;
    /// Monotonic clock snapshot of the last screen redraw.
    std::chrono::steady_clock::time_point lastDraw;
    /// Byte count at the time of the last screen redraw (for speed calc).
    long long lastBytes = 0;
    /// Most-recently computed speed in bytes/sec; -1 = not yet known.
    long long lastSpeed = -1;
    /// Length (chars) of the last line printed, used to erase it after done.
    int lastLen = 0;
};

/// Minimum interval between console redraws to avoid flooding stdout.
static const int kRedrawMs = 150;

/// libcurl XFERINFO callback.  Signature required by libcurl:
///   int(void* userdata, curl_off_t dltotal, curl_off_t dlnow,
///       curl_off_t ultotal, curl_off_t ulnow)
/// Return 0 to continue, non-zero to abort.
int xferinfo_cb(void* userdata,
                curl_off_t dltotal,
                curl_off_t dlnow,
                curl_off_t /* ultotal */,
                curl_off_t /* ulnow */)
{
    ProgressState* ps = static_cast<ProgressState*>(userdata);
    auto now = std::chrono::steady_clock::now();
    auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                         now - ps->lastDraw)
                         .count();
    if (elapsedMs < kRedrawMs)
    {
        return 0;
    }

    // Compute instantaneous download speed over the interval since last draw.
    // Guard against divide-by-zero with a small epsilon (1 ms minimum).
    long long bytesNow = static_cast<long long>(dlnow);
    double elapsedSec  = static_cast<double>(elapsedMs) / 1000.0;
    long long speed    = ps->lastSpeed; // reuse previous if interval too small
    if (elapsedSec >= 0.001)
    {
        long long delta = bytesNow - ps->lastBytes;
        speed = static_cast<long long>(
            static_cast<double>(delta) / elapsedSec);
        if (speed < 0)
        {
            speed = 0; // clamp: delta can be negative on resume resets
        }
        ps->lastSpeed = speed;
    }

    ps->lastDraw  = now;
    ps->lastBytes = bytesNow;

    std::string line = wcr::render_progress(
        ps->label,
        bytesNow,
        static_cast<long long>(dltotal),
        speed);
    // Overwrite the current line.  Pad to clear any trailing chars from a
    // longer previous render.
    int lineLen = static_cast<int>(line.size());
    std::printf("\r%s", line.c_str());
    for (int i = lineLen; i < ps->lastLen; ++i)
    {
        std::putchar(' ');
    }
    std::fflush(stdout);
    ps->lastLen = lineLen;
    return 0;
}
} // namespace

namespace wcr
{
void download_file(const std::string& url, const std::string& dest,
                   const DownloadOpts& opts)
{
    // Resume logic: if the destination already has bytes and resume is enabled,
    // open in append mode and ask the server for a byte-range starting where we
    // left off (HTTP Range: bytes=<start>-).  Otherwise start fresh ("wb").
    long long start = 0;
    const char* mode = "wb";
    if (opts.resume)
    {
        long long have = file_size(dest);
        if (have > 0)
        {
            start = have;
            mode = "ab";
        }
    }
    FILE* fp = fopen(dest.c_str(), mode);
    if (!fp)
    {
        throw std::runtime_error("cannot open dest: " + dest);
    }
    CURL* c = curl_easy_init();
    if (!c)
    {
        fclose(fp);
        throw std::runtime_error("curl_easy_init failed");
    }
    curl_easy_setopt(c, CURLOPT_URL, url.c_str());
    curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(c, CURLOPT_WRITEDATA, fp);
    curl_easy_setopt(c, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(c, CURLOPT_CONNECTTIMEOUT, (long)opts.connect_timeout_s);
    // FAILONERROR makes curl treat HTTP 4xx/5xx as a transfer error rather than
    // silently writing the error body to dest.
    curl_easy_setopt(c, CURLOPT_FAILONERROR, 1L);
    if (start > 0)
    {
        // CURLOPT_RESUME_FROM_LARGE is the 64-bit variant; it emits the
        // "Range: bytes=<start>-" header so the server skips already-received
        // content.
        curl_easy_setopt(c, CURLOPT_RESUME_FROM_LARGE, (curl_off_t)start);
    }

    // Wire up the progress display when the caller provides a label.
    ProgressState ps;
    bool showProgress = !opts.progress_label.empty();
    if (showProgress)
    {
        ps.label    = opts.progress_label;
        ps.lastDraw = std::chrono::steady_clock::now() -
                      std::chrono::milliseconds(kRedrawMs + 1);
        // Enable the progress meter (libcurl suppresses it by default).
        curl_easy_setopt(c, CURLOPT_NOPROGRESS, 0L);
        curl_easy_setopt(c, CURLOPT_XFERINFOFUNCTION, xferinfo_cb);
        curl_easy_setopt(c, CURLOPT_XFERINFODATA, &ps);
    }

    CURLcode rc = curl_easy_perform(c);
    curl_easy_cleanup(c);
    fclose(fp);

    // Clear the progress line so the caller's summary printf lands cleanly.
    if (showProgress && ps.lastLen > 0)
    {
        std::printf("\r");
        for (int i = 0; i < ps.lastLen; ++i)
        {
            std::putchar(' ');
        }
        std::printf("\r");
        std::fflush(stdout);
    }

    if (rc != CURLE_OK)
    {
        throw std::runtime_error(std::string("download failed: ") +
                                 curl_easy_strerror(rc));
    }
    // Post-download size validation: compare on-disk file size against the
    // caller-supplied expected value.  A mismatch means we got a truncated or
    // corrupt response; throw so the caller can retry or abort cleanly.
    if (opts.expected_size >= 0 && file_size(dest) != opts.expected_size)
    {
        throw std::runtime_error("size mismatch for " + dest);
    }
}
} // namespace wcr
