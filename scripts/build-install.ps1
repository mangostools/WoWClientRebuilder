# MaNGOS is a full featured server for World of Warcraft, supporting
# the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
#
# Copyright (C) 2005-2026 MaNGOS <https://www.getmangos.eu>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
#
# World of Warcraft, and all World of Warcraft or Warcraft art, images,
# and lore are copyrighted by Blizzard Entertainment, Inc.

<#
.SYNOPSIS
    Configure, build (Release), and install WoWClientRebuilder.

.DESCRIPTION
    Mirrors the server-core clone -> build -> install flow.
    Produces:
      WoWClientRebuilder/           <- source (this repo)
      WoWClientRebuilder_build/     <- cmake build tree
      WoWClientRebuilder_install/   <- runnable install (wowrebuild.exe + DLLs)

.PARAMETER VcpkgToolchain
    Path to vcpkg.cmake toolchain file.
    Defaults to $env:VCPKG_ROOT\scripts\buildsystems\vcpkg.cmake when VCPKG_ROOT
    is set, otherwise auto-discovers the vcpkg bundled with the local Visual
    Studio install (via vswhere).
#>
param(
    [string]$VcpkgToolchain = ""
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

# ---------------------------------------------------------------------------
# Resolve toolchain path
# ---------------------------------------------------------------------------
if ($VcpkgToolchain -eq "") {
    if ($env:VCPKG_ROOT) {
        $VcpkgToolchain = Join-Path $env:VCPKG_ROOT "scripts\buildsystems\vcpkg.cmake"
    } else {
        # No VCPKG_ROOT: auto-discover the vcpkg bundled with the local Visual
        # Studio install via vswhere (any edition / drive) rather than a fixed path.
        $vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
        if (Test-Path $vswhere) {
            $vsRoot = & $vswhere -latest -property installationPath 2>$null
            if ($vsRoot) {
                $VcpkgToolchain = Join-Path $vsRoot "VC\vcpkg\scripts\buildsystems\vcpkg.cmake"
            }
        }
    }
}

if (-not (Test-Path $VcpkgToolchain)) {
    Write-Error "vcpkg toolchain not found at: $VcpkgToolchain`nSet VCPKG_ROOT or pass -VcpkgToolchain."
}

# ---------------------------------------------------------------------------
# Resolve source, build, and install paths (sibling layout)
# ---------------------------------------------------------------------------
$SourceDir  = Split-Path -Parent $PSScriptRoot
$BuildDir   = Join-Path (Split-Path -Parent $SourceDir) "WoWClientRebuilder_build"
$InstallDir = Join-Path (Split-Path -Parent $SourceDir) "WoWClientRebuilder_install"

Write-Host ""
Write-Host "=== WoWClientRebuilder build-install ==="
Write-Host "  Source:   $SourceDir"
Write-Host "  Build:    $BuildDir"
Write-Host "  Install:  $InstallDir"
Write-Host "  Toolchain: $VcpkgToolchain"
Write-Host ""

# ---------------------------------------------------------------------------
# Configure
# ---------------------------------------------------------------------------
Write-Host "--- Configure ---"
& "cmake.exe" `
    -S $SourceDir `
    -B $BuildDir `
    -A x64 `
    "-DCMAKE_TOOLCHAIN_FILE=$VcpkgToolchain" `
    "-DX_VCPKG_APPLOCAL_DEPS_INSTALL=ON"
if ($LASTEXITCODE -ne 0) { Write-Error "CMake configure failed (exit $LASTEXITCODE)." }

# ---------------------------------------------------------------------------
# Build (Release)
# ---------------------------------------------------------------------------
Write-Host ""
Write-Host "--- Build (Release) ---"
& "cmake.exe" --build $BuildDir --config Release
if ($LASTEXITCODE -ne 0) { Write-Error "CMake build failed (exit $LASTEXITCODE)." }

# ---------------------------------------------------------------------------
# Install
# ---------------------------------------------------------------------------
Write-Host ""
Write-Host "--- Install ---"
& "cmake.exe" --install $BuildDir --config Release --prefix $InstallDir
if ($LASTEXITCODE -ne 0) { Write-Error "CMake install failed (exit $LASTEXITCODE)." }

Write-Host ""
Write-Host "=== Install complete ==="
Write-Host "  Runnable files in: $InstallDir"
Write-Host "  Run: $InstallDir\wowrebuild.exe <mode> <version> [options] <outDir>  (or double-click for the menu)"
