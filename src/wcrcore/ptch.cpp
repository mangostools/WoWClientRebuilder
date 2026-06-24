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
 * @file ptch.cpp
 * @brief Implementation of Blizzard PTCH/BSD0 patch application.
 */

#include "ptch.h"
#include <cstring>
#include <stdexcept>
namespace
{
// Little-endian word helpers: read without alignment requirements.
inline uint32_t rd32(const uint8_t* p)
{
    return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24;
}
inline uint64_t rd64(const uint8_t* p)
{
    uint64_t v = 0;
    for (int i = 0; i < 8; i++)
    {
        v |= (uint64_t)p[i] << (8 * i);
    }
    return v;
}

// Blizzard RLE decompressor (mirrors StormLib Decompress_RLE):
//   high bit set  => copy (b & 0x7F)+1 literal bytes from input
//   high bit clear => skip b+1 bytes in output (leave them as the zero-fill
//                     already done by std::memset)
// The first 4 bytes of 'in' are the decompressed length header; skip them.
static void decompress_rle(uint8_t* out, uint32_t outLen, const uint8_t* in,
                           uint32_t inLen)
{
    const uint8_t* inEnd = in + inLen;
    uint8_t* outEnd = out + outLen;
    in += sizeof(uint32_t); // cut the leading DWORD (decompressed-size hint)
    std::memset(out, 0, outLen);
    while (in < inEnd && out < outEnd)
    {
        uint8_t b = *in++;
        if (b & 0x80) // copy (b & 0x7F) + 1 literal bytes
        {
            int rep = (b & 0x7F) + 1;
            for (int i = 0; i < rep; ++i)
            {
                if (out >= outEnd || in >= inEnd)
                {
                    break;
                }
                *out++ = *in++;
            }
        }
        else // skip (b + 1) zero bytes, clamped to the output buffer
        {
            uint32_t skip = (uint32_t)b + 1;
            if (skip > (uint32_t)(outEnd - out))
            {
                skip = (uint32_t)(outEnd - out);
            }
            out += skip;
        }
    }
}
} // namespace

namespace wcr
{
// PTCH header layout (all offsets little-endian):
//   0x00  "PTCH"  magic
//   0x04  uint32  sizeOfPatchData  (total patch payload size in bytes)
//   0x08  uint32  sizeBeforePatch  (expected source file size, informational)
//   0x0C  uint32  sizeAfterPatch   (expected output size; validated at the end)
//   0x10  "MD5_"  block magic
//   0x14  uint32  md5BlockSize     (always 40: 16 bytes source MD5 + 16 bytes
//                                   target MD5 + 8 bytes padding)
//   0x18  byte[32] md5 hashes      (not verified here)
//   0x38  "XFRM"  block magic
//   0x3C  uint32  xfrmBlockSize    (size of the transform block incl.
//   "BSD0"+data) 0x40  "BSD0"  transform type   (BSD0 = BSDIFF40 data, possibly
//   RLE-compressed) 0x44  byte[]  transform payload
Bytes apply_ptch(const Bytes& base, const Bytes& patch)
{
    const uint8_t* p = patch.data();

    // --- 1. Validate the PTCH header ----------------------------------------
    if (patch.size() < 68 || std::memcmp(p, "PTCH", 4))
    {
        throw std::runtime_error("not a PTCH");
    }
    uint32_t sizeOfPatchData = rd32(p + 0x04), sizeAfter = rd32(p + 0x0C);
    if (std::memcmp(p + 0x10, "MD5_", 4))
    {
        throw std::runtime_error("no MD5_");
    }
    if (std::memcmp(p + 0x38, "XFRM", 4))
    {
        throw std::runtime_error("no XFRM");
    }
    uint32_t xfrmBlockSize = rd32(p + 0x3C);
    if (std::memcmp(p + 0x40, "BSD0", 4))
    {
        throw std::runtime_error("patch type != BSD0");
    }
    // HDR = 0x44: the byte offset where the transform payload begins.
    // SIZE_OF_XFRM_HEADER = 12: "XFRM"(4) + xfrmBlockSize field(4) + "BSD0"(4).
    const uint32_t HDR = 0x44, SIZE_OF_XFRM_HEADER = 12;
    if (sizeOfPatchData < HDR)
    {
        throw std::runtime_error("PTCH sizeOfPatchData too small");
    }
    if (xfrmBlockSize < SIZE_OF_XFRM_HEADER)
    {
        throw std::runtime_error("PTCH xfrmBlockSize too small");
    }
    uint32_t cbCompressed = xfrmBlockSize - SIZE_OF_XFRM_HEADER;
    uint32_t cbDecompressed = sizeOfPatchData - HDR;
    if (patch.size() < (size_t)HDR + cbCompressed)
    {
        throw std::runtime_error("PTCH patch data truncated");
    }

    // --- 2. RLE-decompress the BSD0 payload (if compressed) -----------------
    // When cbCompressed < cbDecompressed the payload is RLE-compressed;
    // otherwise it is stored verbatim (uncompressed pass-through).
    Bytes dec(cbDecompressed);
    if (cbCompressed > cbDecompressed)
    {
        throw std::runtime_error("PTCH cbCompressed exceeds cbDecompressed");
    }
    if (cbCompressed < cbDecompressed)
    {
        decompress_rle(dec.data(), cbDecompressed, p + HDR, cbCompressed);
    }
    else
    {
        std::memcpy(dec.data(), p + HDR, cbCompressed);
    }

    // --- 3. Apply the BSDIFF40 patch ----------------------------------------
    // The decompressed payload is a BSDIFF40 blob: 32-byte header then the
    // control, diff(data) and extra blocks. Validate every offset/length
    // against the decompressed buffer before using it -- this data is
    // attacker-influenced (downloaded), and the apply loop writes into nw.
    if (cbDecompressed < 32)
    {
        throw std::runtime_error("PTCH BSDIFF40 block too small");
    }
    if (std::memcmp(dec.data(), "BSDIFF40", 8) != 0)
    {
        throw std::runtime_error("PTCH no BSDIFF40 magic");
    }
    uint64_t ctrlSize = rd64(dec.data() + 8);
    uint64_t dataSize = rd64(dec.data() + 16);
    uint64_t newSize = rd64(dec.data() + 24);
    if (newSize != sizeAfter)
    {
        throw std::runtime_error("PTCH BSDIFF40 newSize != sizeAfterPatch");
    }
    // ctrl + data blocks must fit within the decompressed buffer
    if (ctrlSize > cbDecompressed - 32 ||
        dataSize > cbDecompressed - 32 - ctrlSize)
    {
        throw std::runtime_error("PTCH BSDIFF40 block sizes out of range");
    }
    const uint8_t* ctrl = dec.data() + 32;
    const uint8_t* ctrlEnd = ctrl + (size_t)ctrlSize;
    const uint8_t* dataPtr = ctrlEnd;
    const uint8_t* dataEnd = dataPtr + (size_t)dataSize;
    const uint8_t* extraPtr = dataEnd;
    const uint8_t* extraEnd = dec.data() + cbDecompressed;

    Bytes nw((size_t)newSize);
    uint64_t no = 0;
    uint64_t oo = 0;
    uint64_t oldSize = base.size();
    while (no < newSize)
    {
        if (ctrl + 12 > ctrlEnd)
        {
            throw std::runtime_error("PTCH control block exhausted");
        }
        uint32_t add = rd32(ctrl);
        uint32_t mov = rd32(ctrl + 4);
        uint32_t seek = rd32(ctrl + 8);
        ctrl += 12;

        // diff segment: copy `add` bytes then add the matching base bytes
        if (add > newSize - no || (uint64_t)(dataEnd - dataPtr) < add)
        {
            throw std::runtime_error("PTCH diff segment out of range");
        }
        std::memcpy(nw.data() + no, dataPtr, add);
        dataPtr += add;
        for (uint32_t i = 0; i < add; ++i)
        {
            if (oo < oldSize)
            {
                nw[(size_t)no] = (uint8_t)(nw[(size_t)no] + base[(size_t)oo]);
            }
            ++no;
            ++oo;
        }

        // extra segment: copy `mov` verbatim bytes
        if (mov > newSize - no || (uint64_t)(extraEnd - extraPtr) < mov)
        {
            throw std::runtime_error("PTCH extra segment out of range");
        }
        std::memcpy(nw.data() + no, extraPtr, mov);
        extraPtr += mov;
        no += mov;

        // Signed seek in the old file. BSDIFF stores the offset as
        // sign-magnitude (high bit set => negative magnitude). Apply it to
        // the 64-bit cursor as a real signed value: the original 32-bit code
        // relied on uint32 wraparound, which silently breaks once `oo` is a
        // 64-bit counter (the cursor jumps ~4 GB instead of moving back).
        int64_t seekOff;
        if (seek & 0x80000000u)
        {
            seekOff = -(int64_t)(seek & 0x7FFFFFFFu);
        }
        else
        {
            seekOff = (int64_t)seek;
        }
        oo = (uint64_t)((int64_t)oo + seekOff);
    }
    return nw;
}
} // namespace wcr
