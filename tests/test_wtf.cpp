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

#include "doctest.h"
#include "wtf.h"
#include "bytes.h"
#include <filesystem>
#include <string>

namespace
{
/// Read an entire file back as a std::string for line assertions.
std::string read_text(const std::string& p)
{
    wcr::Bytes b = wcr::read_file(p);
    return std::string(b.begin(), b.end());
}
} // namespace

TEST_CASE("write_runonce writes RunOnce.wtf and realmlist.wtf")
{
    std::filesystem::path tmp =
        std::filesystem::temp_directory_path() / "wcr_wtf_5_2";
    std::error_code ec;
    std::filesystem::remove_all(tmp, ec);

    wcr::write_runonce(tmp.string(), "127.0.0.1", "enUS", 1920, 1080);

    std::string runOnce = read_text((tmp / "WTF" / "RunOnce.wtf").string());
    CHECK(runOnce.find("SET realmlist \"127.0.0.1\"") != std::string::npos);
    CHECK(runOnce.find("SET gxWindow \"1\"") != std::string::npos);
    CHECK(runOnce.find("SET gxMaximize \"0\"") != std::string::npos);
    CHECK(runOnce.find("SET gxResolution \"1920x1080\"") != std::string::npos);

    std::string realm =
        read_text((tmp / "Data" / "enUS" / "realmlist.wtf").string());
    CHECK(realm.find("set realmlist \"127.0.0.1\"") != std::string::npos);

    std::filesystem::remove_all(tmp, ec);
}

TEST_CASE("write_runonce honors custom host, locale and resolution")
{
    std::filesystem::path tmp =
        std::filesystem::temp_directory_path() / "wcr_wtf_5_4";
    std::error_code ec;
    std::filesystem::remove_all(tmp, ec);

    wcr::write_runonce(tmp.string(), "logon.example.net", "deDE", 1280, 720);

    std::string runOnce = read_text((tmp / "WTF" / "RunOnce.wtf").string());
    CHECK(runOnce.find("SET realmlist \"logon.example.net\"") !=
          std::string::npos);
    CHECK(runOnce.find("SET gxResolution \"1280x720\"") != std::string::npos);

    std::string realm =
        read_text((tmp / "Data" / "deDE" / "realmlist.wtf").string());
    CHECK(realm.find("set realmlist \"logon.example.net\"") !=
          std::string::npos);

    std::filesystem::remove_all(tmp, ec);
}

// ---------------------------------------------------------------------------
// B2 security: write_runonce must reject poisoned locale strings
// ---------------------------------------------------------------------------

TEST_CASE("write_runonce throws on invalid locale (path-traversal guard)")
{
    // Belt-and-suspenders: even if a poisoned locale escapes manifest filtering,
    // write_runonce must throw rather than constructing a traversal path.
    std::filesystem::path tmp =
        std::filesystem::temp_directory_path() / "wcr_wtf_b2";
    std::error_code ec;
    std::filesystem::remove_all(tmp, ec);

    CHECK_THROWS_AS(
        wcr::write_runonce(tmp.string(), "127.0.0.1", "../../etc", 800, 600),
        std::runtime_error);

    // Valid locale must still work normally after.
    REQUIRE_NOTHROW(
        wcr::write_runonce(tmp.string(), "127.0.0.1", "enUS", 800, 600));

    std::string realm =
        read_text((tmp / "Data" / "enUS" / "realmlist.wtf").string());
    CHECK(realm.find("set realmlist \"127.0.0.1\"") != std::string::npos);

    std::filesystem::remove_all(tmp, ec);
}
