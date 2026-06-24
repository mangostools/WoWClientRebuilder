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
 * @file test_ptch.cpp
 * @brief Unit tests for apply_ptch(): round-trip reconstruction and rejection
 *        of malformed PTCH input.
 */

#include "doctest.h"
#include "ptch.h"
#include "bytes.h"
#include <cstring>
using wcr::Bytes;
static void put32(Bytes& b, uint32_t v)
{
    for (int i = 0; i < 4; i++)
    {
        b.push_back((v >> (8 * i)) & 0xFF);
    }
}
static void put64(Bytes& b, uint64_t v)
{
    for (int i = 0; i < 8; i++)
    {
        b.push_back((v >> (8 * i)) & 0xFF);
    }
}

TEST_CASE("apply_ptch reconstructs a known target (uncompressed BSD0/BSDIFF40)")
{
    Bytes base = {0, 0, 0, 0}; // sizeBefore = 4
    // BSDIFF40 blob: header(32) + ctrl(12) + data(4) + extra(1) = 49 bytes
    Bytes bsdiff;
    for (char c : std::string("BSDIFF40"))
    {
        bsdiff.push_back((uint8_t)c);
    }
    put64(bsdiff, 12);
    put64(bsdiff, 4);
    put64(bsdiff, 5); // ctrlSize, dataSize, newSize
    put32(bsdiff, 4);
    put32(bsdiff, 1);
    put32(bsdiff, 0); // ctrl triple: add4, extra1, seek0
    for (uint8_t v : {1, 2, 3, 4})
    {
        bsdiff.push_back(v); // data block (added to base)
    }
    bsdiff.push_back(5); // extra block
    REQUIRE(bsdiff.size() == 49);

    Bytes p;
    for (char c : std::string("PTCH"))
    {
        p.push_back((uint8_t)c);
    }
    put32(p, 68 + (uint32_t)bsdiff.size()); // sizeOfPatchData = 117
    put32(p, 4);                            // sizeBeforePatch
    put32(p, 5);                            // sizeAfterPatch
    for (char c : std::string("MD5_"))
    {
        p.push_back((uint8_t)c);
    }
    put32(p, 40); // md5 block size
    for (int i = 0; i < 32; i++)
    {
        p.push_back(0); // md5_before + md5_after (unverified)
    }
    for (char c : std::string("XFRM"))
    {
        p.push_back((uint8_t)c);
    }
    put32(p, 12 + (uint32_t)bsdiff.size()); // xfrmBlockSize = 61
    for (char c : std::string("BSD0"))
    {
        p.push_back((uint8_t)c);
    }
    p.insert(p.end(), bsdiff.begin(),
             bsdiff.end()); // uncompressed body (cbCompressed==cbDecompressed)
    REQUIRE(p.size() == 117);

    Bytes out = wcr::apply_ptch(base, p);
    CHECK(out == Bytes({1, 2, 3, 4, 5}));
}

TEST_CASE("apply_ptch rejects malformed input")
{
    Bytes base = {0, 0, 0, 0};

    // Case 1: buffer too short (well under 68-byte minimum)
    {
        Bytes p = {1, 2, 3};
        CHECK_THROWS_AS(wcr::apply_ptch(base, p), std::runtime_error);
    }

    // Case 2: 80-byte buffer but wrong magic (not "PTCH")
    {
        Bytes p(80, 0);
        CHECK_THROWS_AS(wcr::apply_ptch(base, p), std::runtime_error);
    }

    // Case 3: valid PTCH/MD5_/XFRM/BSD0 magics but sizeOfPatchData < 68
    // (triggers sizeOfPatchData guard)
    {
        Bytes p;
        for (char c : std::string("PTCH"))
        {
            p.push_back((uint8_t)c);
        }
        put32(p, 60); // sizeOfPatchData = 60 < HDR(0x44=68) -> throws
        put32(p, 4);  // sizeBeforePatch
        put32(p, 4);  // sizeAfterPatch
        for (char c : std::string("MD5_"))
        {
            p.push_back((uint8_t)c);
        }
        put32(p, 40); // md5 block size
        for (int i = 0; i < 32; i++)
        {
            p.push_back(0); // md5 placeholder
        }
        for (char c : std::string("XFRM"))
        {
            p.push_back((uint8_t)c);
        }
        put32(p, 12); // xfrmBlockSize = 12 (valid)
        for (char c : std::string("BSD0"))
        {
            p.push_back((uint8_t)c);
        }
        // p.size() == 68 now; no body bytes needed (guard fires first)
        CHECK_THROWS_AS(wcr::apply_ptch(base, p), std::runtime_error);
    }

    // Case 4: valid magics but xfrmBlockSize < 12 (triggers xfrmBlockSize
    // guard)
    {
        Bytes p;
        for (char c : std::string("PTCH"))
        {
            p.push_back((uint8_t)c);
        }
        put32(p, 80); // sizeOfPatchData = 80 >= 68 -> OK
        put32(p, 4);  // sizeBeforePatch
        put32(p, 4);  // sizeAfterPatch
        for (char c : std::string("MD5_"))
        {
            p.push_back((uint8_t)c);
        }
        put32(p, 40); // md5 block size
        for (int i = 0; i < 32; i++)
        {
            p.push_back(0); // md5 placeholder
        }
        for (char c : std::string("XFRM"))
        {
            p.push_back((uint8_t)c);
        }
        put32(p, 8); // xfrmBlockSize = 8 < 12 -> throws
        for (char c : std::string("BSD0"))
        {
            p.push_back((uint8_t)c);
        }
        CHECK_THROWS_AS(wcr::apply_ptch(base, p), std::runtime_error);
    }
}

TEST_CASE("apply_ptch rejects PTCH where cbCompressed exceeds cbDecompressed (heap overflow guard)")
{
    // Craft a PTCH whose XFRM block claims cbCompressed > cbDecompressed.
    //   cbDecompressed = sizeOfPatchData - HDR(0x44)
    //   cbCompressed   = xfrmBlockSize  - SIZE_OF_XFRM_HEADER(12)
    //
    // To isolate THIS guard (not incidentally hit a later check), make
    // cbDecompressed large enough to pass the "< 32" BSDIFF40 size check
    // but still smaller than cbCompressed:
    //   sizeOfPatchData = 0x44 + 40 = 108  => cbDecompressed = 40
    //   xfrmBlockSize   = 12 + 80  = 92    => cbCompressed   = 80
    // => cbCompressed(80) > cbDecompressed(40).
    // The patch must supply 80 bytes of body so the truncation guard passes.
    //
    // Without the fix, dec is allocated with size 40 and memcpy writes 80
    // bytes into it — a 40-byte heap overflow. With the fix it throws before
    // any memcpy.
    Bytes base = {0x00, 0x01, 0x02, 0x03};

    Bytes p;
    // "PTCH"
    for (char c : std::string("PTCH")) { p.push_back((uint8_t)c); }
    put32(p, 0x44 + 40);  // sizeOfPatchData = 108 => cbDecompressed = 40
    put32(p, 4);           // sizeBeforePatch
    put32(p, 4);           // sizeAfterPatch
    // "MD5_"
    for (char c : std::string("MD5_")) { p.push_back((uint8_t)c); }
    put32(p, 40);          // md5BlockSize
    for (int i = 0; i < 32; i++) { p.push_back(0); }
    // "XFRM"
    for (char c : std::string("XFRM")) { p.push_back((uint8_t)c); }
    put32(p, 12 + 80);    // xfrmBlockSize = 92 => cbCompressed = 80
    // "BSD0"
    for (char c : std::string("BSD0")) { p.push_back((uint8_t)c); }
    // 80 payload bytes so the truncation guard (patch.size() < HDR+cbCompressed)
    // passes; content doesn't matter since we must throw before reaching them.
    for (int i = 0; i < 80; i++) { p.push_back((uint8_t)i); }

    // Must throw std::runtime_error with the specific bounds-guard message
    // BEFORE performing the heap-overflowing memcpy. Without the guard the
    // code proceeds to memcpy (overflow), then fails with a different message
    // ("PTCH no BSDIFF40 magic" or similar), so matching the exact message
    // confirms the guard fires first.
    bool threwCorrectly = false;
    try
    {
        wcr::apply_ptch(base, p);
    }
    catch (const std::runtime_error& e)
    {
        threwCorrectly = std::string(e.what()) == "PTCH cbCompressed exceeds cbDecompressed";
    }
    CHECK(threwCorrectly);
}

TEST_CASE(
    "apply_ptch rejects BSDIFF40 add larger than newSize (OOB write guard)")
{
    // Build a minimal valid uncompressed PTCH whose single ctrl triple has
    // add=0xFFFFFFFF (hugely larger than newSize=2). The diff-segment bounds
    // check must fire before any heap write.
    Bytes base = {0xAA, 0xBB};

    // BSDIFF40 blob: header(32) + ctrl(12) + data(0) + extra(0)
    // newSize = 2, add = 0xFFFFFFFF -> add > newSize - no  =>  throws
    Bytes bsdiff;
    for (char c : std::string("BSDIFF40"))
    {
        bsdiff.push_back((uint8_t)c);
    }
    put64(bsdiff, 12); // ctrlSize = 12 (one triple)
    put64(bsdiff, 0); // dataSize = 0  (no data bytes — guard fires before read)
    put64(bsdiff, 2); // newSize  = 2
    put32(bsdiff, 0xFFFFFFFFu); // add  = huge
    put32(bsdiff, 0);           // mov  = 0
    put32(bsdiff, 0);           // seek = 0

    // Wrap in a PTCH envelope (uncompressed: cbCompressed == cbDecompressed)
    Bytes p;
    for (char c : std::string("PTCH"))
    {
        p.push_back((uint8_t)c);
    }
    uint32_t bsdSize = (uint32_t)bsdiff.size();
    put32(p, 0x44 + bsdSize); // sizeOfPatchData
    put32(p, 2);              // sizeBeforePatch
    put32(p, 2);              // sizeAfterPatch
    for (char c : std::string("MD5_"))
    {
        p.push_back((uint8_t)c);
    }
    put32(p, 40); // md5BlockSize
    for (int i = 0; i < 32; i++)
    {
        p.push_back(0); // md5 placeholder
    }
    for (char c : std::string("XFRM"))
    {
        p.push_back((uint8_t)c);
    }
    put32(p, 12 + bsdSize); // xfrmBlockSize
    for (char c : std::string("BSD0"))
    {
        p.push_back((uint8_t)c);
    }
    p.insert(p.end(), bsdiff.begin(), bsdiff.end());

    CHECK_THROWS_AS(wcr::apply_ptch(base, p), std::runtime_error);
}
