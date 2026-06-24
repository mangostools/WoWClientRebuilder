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
 * @file bencode.cpp
 * @brief Minimal bencode decoder + WoW .tfil torrent parser.
 */

#include "bencode.h"
#include "sha1.h"
#include <climits>
#include <stdexcept>

// ---------------------------------------------------------------------------
// Internal bencode reader
// ---------------------------------------------------------------------------
namespace
{

/// A forward cursor over the raw bencoded bytes.
struct Reader
{
        const char* p;   ///< Current position.
        const char* end; ///< One past the last byte.

        explicit Reader(const wcr::Bytes& b)
            : p(reinterpret_cast<const char*>(b.data())),
              end(reinterpret_cast<const char*>(b.data()) + b.size())
        {
        }

        char peek() const
        {
            if (p >= end)
            {
                throw std::runtime_error("bencode: unexpected end of input");
            }
            return *p;
        }

        char get()
        {
            char c = peek();
            ++p;
            return c;
        }

        void expect(char c)
        {
            if (get() != c)
            {
                throw std::runtime_error(
                    std::string("bencode: expected '") + c + "'");
            }
        }
};

/// Read a bencoded integer: i<digits>e. Cursor must be at 'i'.
long long read_int(Reader& r)
{
    r.expect('i');
    bool neg = false;
    if (r.peek() == '-')
    {
        neg = true;
        r.get();
    }
    long long v = 0;
    bool any = false;
    while (r.p < r.end && r.peek() != 'e')
    {
        char c = r.get();
        if (c < '0' || c > '9')
        {
            throw std::runtime_error("bencode: bad integer digit");
        }
        if (v > (LLONG_MAX - (c - '0')) / 10)
        {
            throw std::runtime_error("bencode: integer overflow");
        }
        v = v * 10 + (c - '0');
        any = true;
    }
    if (!any)
    {
        throw std::runtime_error("bencode: empty integer");
    }
    r.expect('e');
    return neg ? -v : v;
}

/// Read a bencoded byte string: <len>:<bytes>. Cursor at a digit.
std::string read_string(Reader& r)
{
    long long len = 0;
    bool any = false;
    while (r.p < r.end && r.peek() != ':')
    {
        char c = r.get();
        if (c < '0' || c > '9')
        {
            throw std::runtime_error("bencode: bad string length");
        }
        if (len > (LLONG_MAX - (c - '0')) / 10)
        {
            throw std::runtime_error("bencode: integer overflow");
        }
        len = len * 10 + (c - '0');
        any = true;
    }
    if (!any)
    {
        throw std::runtime_error("bencode: missing string length");
    }
    r.expect(':');
    if (r.p + len > r.end)
    {
        throw std::runtime_error("bencode: string longer than input");
    }
    std::string s(r.p, r.p + (size_t)len);
    r.p += len;
    return s;
}

/// Skip any one bencoded value (used to advance past values we ignore).
void skip_value(Reader& r)
{
    char c = r.peek();
    if (c == 'i')
    {
        read_int(r);
    }
    else if (c == 'l')
    {
        r.get();
        while (r.peek() != 'e')
        {
            skip_value(r);
        }
        r.get();
    }
    else if (c == 'd')
    {
        r.get();
        while (r.peek() != 'e')
        {
            read_string(r); // key
            skip_value(r);  // value
        }
        r.get();
    }
    else
    {
        read_string(r);
    }
}

/// Parse one "files" entry: d 6:length i..e 4:path l <strings> e e.
wcr::TorrentFile read_file_entry(Reader& r)
{
    r.expect('d');
    wcr::TorrentFile f;
    f.length = 0;
    std::string joined;
    bool first = true;
    while (r.peek() != 'e')
    {
        std::string key = read_string(r);
        if (key == "length")
        {
            f.length = read_int(r);
        }
        else if (key == "path")
        {
            r.expect('l');
            while (r.peek() != 'e')
            {
                std::string seg = read_string(r);
                if (!first)
                {
                    joined += "/";
                }
                joined += seg;
                first = false;
            }
            r.get(); // consume list 'e'
        }
        else
        {
            skip_value(r);
        }
    }
    r.get(); // consume dict 'e'
    f.path = joined;
    f.alignment = (joined == "alignment");
    return f;
}

/// Parse the "info" dictionary, filling pieceLength/pieces/files.
void read_info(Reader& r, wcr::Torrent& t)
{
    r.expect('d');
    while (r.peek() != 'e')
    {
        std::string key = read_string(r);
        if (key == "piece length")
        {
            t.pieceLength = read_int(r);
        }
        else if (key == "pieces")
        {
            t.pieces = read_string(r);
        }
        else if (key == "files")
        {
            r.expect('l');
            while (r.peek() != 'e')
            {
                t.files.push_back(read_file_entry(r));
            }
            r.get(); // consume list 'e'
        }
        else
        {
            skip_value(r);
        }
    }
    r.get(); // consume dict 'e'
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// Public interface
// ---------------------------------------------------------------------------
namespace wcr
{

Torrent parse_torrent(const Bytes& tfil)
{
    Torrent t;
    Reader r(tfil);
    r.expect('d');
    bool sawInfo = false;
    while (r.peek() != 'e')
    {
        std::string key = read_string(r);
        if (key == "info")
        {
            const char* infoStart = r.p;
            read_info(r, t);
            const char* infoEnd = r.p;
            Bytes raw(reinterpret_cast<const uint8_t*>(infoStart),
                      reinterpret_cast<const uint8_t*>(infoEnd));
            t.infoHash = sha1(raw);
            sawInfo = true;
        }
        else if (key == "direct download")
        {
            t.directDownload = read_string(r);
        }
        else
        {
            skip_value(r);
        }
    }
    r.get(); // consume top dict 'e'
    if (!sawInfo)
    {
        throw std::runtime_error("parse_torrent: no \"info\" dictionary");
    }
    return t;
}

} // namespace wcr
