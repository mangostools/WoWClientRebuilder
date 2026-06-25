# WoWClientRebuilder

WoWClientRebuilder regenerates a **complete, runnable World of Warcraft client** —
the program binaries, the full `Data/` MPQ set, and your chosen locale(s) — entirely
from Blizzard's own still-live CDN. **No Blizzard data is redistributed:** the tool
ships only code, downloads everything at runtime from official Blizzard CDN endpoints,
verifies files by MD5, and discards the temporary patch archives after use. Output is
byte-identical to a real install.

Supported clients: **Cataclysm 4.3.4 (15595)** and **Mists of Pandaria 5.4.8 (18414)** —
the pre-CASC "pod-retail" builds Blizzard still serves.

## Features

- **Whole client or partial** — regenerate the full client (binaries + `Data/` + locales),
  `data` only, or `locale` only.
- **Every program binary, byte-exact** — including the auxiliary ones Blizzard removed
  from its `/repair` store (`MovieProxy`, `BlizzardError`, `SystemSurvey`, `WowError`,
  `WowError-64`, `Battle.net-64`, and the `Utils\` in-game browser), recovered directly
  out of the already-downloaded MPQs — no redistribution.
- **All MoP locales** (15 at last check, read live from the manifest) via an interactive
  picker, or `--locale enUS,deDE` / `all`.
- **CDN region** selection (EU / NA) with automatic cross-region failover.
- **Two ways to run** — an interactive menu when launched with no arguments
  (double-click), or a scriptable flag interface.
- **Verified end to end** — the partial download manifest is first authenticated against
  the content-hash pinned in the recipe (so a tampered or MITM'd manifest fails closed and
  can't redirect or substitute downloads), every produced file is then MD5-verified, and
  the build scratch is cleaned up — leaving a clean client folder.
- **Localhost safety guard** — in full-client (`client`) mode the rebuilt client's
  realmlist defaults to `127.0.0.1` (see below).

## Requirements

| Requirement | Version / Notes |
|---|---|
| CMake | >= 3.21 (for `CMakePresets.json`) |
| Compiler | MSVC with C++17 — Visual Studio 2026 (the preset's generator); VS 2022 also works via a manual configure (see below) |
| vcpkg | bootstrapped, with `VCPKG_ROOT` set. Supplies the **only** external dependency, `libcurl` (declared in `vcpkg.json`, manifest mode). StormLib, miniz, and doctest are vendored in `dep/`. |
| Internet | vcpkg fetches curl on the first configure; the tool fetches from `dist.blizzard.com.edgesuite.net` and `eu.media.battle.net.edgesuite.net` at runtime |

## Directory layout

The project follows the same sibling-dir convention as the MaNGOS server core:

```
WoWClientRebuilder/          <- source (this repo)
WoWClientRebuilder_build/    <- cmake build tree  (created by configure step)
WoWClientRebuilder_install/  <- runnable install  (wowrebuild.exe + DLLs)
```

## Build & Install

### (a) VS2026 "Open Folder"

Open the `WoWClientRebuilder` folder in Visual Studio 2026. VS auto-detects
`CMakePresets.json` and applies its bundled vcpkg toolchain. Select the
`windows-vcpkg / Release` configuration and choose **Build > Install
WoWClientRebuilder** to produce `WoWClientRebuilder_install`. (This is the path of
least resistance — VS supplies vcpkg, so no `VCPKG_ROOT` is needed.)

### (b) CLI with CMake presets

```powershell
# From a shell with cmake.exe on PATH and VCPKG_ROOT set:
cmake --preset windows-vcpkg
cmake --build --preset windows-vcpkg
cmake --install ..\WoWClientRebuilder_build --config Release
```

### (c) Build-install script

```powershell
# From the repo root:
.\scripts\build-install.ps1

# Or with an explicit vcpkg toolchain path:
.\scripts\build-install.ps1 -VcpkgToolchain "C:\vcpkg\scripts\buildsystems\vcpkg.cmake"
```

The script configures → builds → installs in one step, printing the final install
path on completion.

> If `VCPKG_ROOT` is not set and you pass no `-VcpkgToolchain`, the script tries to
> auto-discover the vcpkg bundled with your local Visual Studio install (via `vswhere`);
> if it can't find one it exits with an error. Set `VCPKG_ROOT` or pass
> `-VcpkgToolchain` explicitly to be sure.

### Troubleshooting: "CMake cannot find CURL"

`libcurl` comes from vcpkg, so this error means the configure step never received the
vcpkg toolchain. It's almost always one (or more) of:

1. **`VCPKG_ROOT` is not set.** The preset wires the toolchain via
   `$env{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake`; with the variable unset that
   path is broken. Set it and make sure vcpkg is bootstrapped:
   ```powershell
   $env:VCPKG_ROOT = "C:\vcpkg"   # or your VS install's "...\VC\vcpkg"
   ```
2. **A stale build dir.** Once a configure fails, the bad toolchain path is pinned in
   `CMakeCache.txt` and re-running cmake reuses it — so **delete the build dir and
   reconfigure** (CMake will not switch toolchains in place):
   ```powershell
   Remove-Item -Recurse -Force ..\WoWClientRebuilder_build
   ```
3. **Not using VS 2026.** The `windows-vcpkg` preset's generator is `Visual Studio 18
   2026`; on a different VS, skip the preset and configure manually:
   ```powershell
   cmake -S . -B ..\WoWClientRebuilder_build -G "Visual Studio 17 2022" -A x64 `
     -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT\scripts\buildsystems\vcpkg.cmake"
   cmake --build ..\WoWClientRebuilder_build --config Release
   ```

The most common cause is `VCPKG_ROOT` unset **combined with** a leftover build dir from
the first failed attempt — wipe the build dir after setting the variable.

## Usage

### Interactive (double-click, or no arguments)

Launched with no arguments, `wowrebuild.exe` drives a numbered console menu:
mode → version → locale (skipped for `data`) → CDN region → output folder →
resume-or-fresh-start (if a prior download exists) → pre-flight confirmation. This is
what runs when you double-click the exe.

At each of the five setup questions you can enter **`B`** to step back to the previous
question or **`X`** to exit (with a confirmation). Exiting stops cleanly before anything
is downloaded.

### Scripted (flags)

```
wowrebuild <mode> <version> [options] <outDir>
  mode:         client | data | locale
  version:      4.3.4 | 5.4.8
  --locale L    csv list (e.g. enUS,deDE) or 'all'   (default enUS)
  --region R    EU | NA                               (default EU)
  --no-cinematics skip the per-locale cinematic movies (included by default)
  --realmlist H host written into the WTF files       (default 127.0.0.1)
  --yes         skip the pre-flight confirmation prompt
  --tfil path   optional .tfil torrent for piece verification
```

Example — a full enUS 5.4.8 client, no prompts:

```powershell
WoWClientRebuilder_install\wowrebuild.exe client 5.4.8 --locale enUS --yes C:\wow548\Rebuild
```

How the program binaries are sourced per version:

| Version | Client | Binary source |
|---|---|---|
| `4.3.4` | Cataclysm (15595) | 32-bit from the content-addressed `/repair` store; 64-bit from `WoWLive-64-Win-15595.zip` [^launcher] |
| `5.4.8` | Mists of Pandaria (18414) | `Wow.exe`/`Wow-64.exe` PTCH-applied from `base-Win.MPQ` + `wow-0-18414-Win-final.MPQ`; the aux binaries extracted from those MPQs |

The `Data/` layer and locales are downloaded from the build's partial manifest in both
cases, and every produced file is MD5-verified.

[^launcher]: The 4.3.4 set includes `Launcher.exe` to mirror a pristine retail folder
    byte-for-byte. It is the legacy Blizzard self-updater: **never run it** — it streams
    the client toward current retail and corrupts the pinned 4.3.4 install. The file is
    inert unless launched.

## Running tests

```powershell
ctest --test-dir WoWClientRebuilder_build -C Release
```

The doctest suite (100+ cases across 21 `tests/test_*.cpp` files) runs fully offline.
Two network/fixture-gated acceptance groups sit alongside it:

- **acceptance** (5.4.8): three fixture-gated cases driven from local MPQ fixtures in
  `WoWClientRebuilder_build/fixtures/` — the `Wow.exe`/`Wow-64.exe` byte-exact pair (the
  named `acceptance` ctest target), the 9 recovered aux binaries (MpqExtract artifacts),
  and an MPQ-handle cleanup regression (the latter two run under the `wcrtests` omnibus
  target). All skip cleanly when the fixtures are absent, so CI / fresh checkouts stay
  green.
- **acceptance_cata434** (4.3.4): an opt-in live test, **registered only when you configure
  with `-DWCR_LIVE_TESTS=ON`**. It downloads ~46 MB from Blizzard's CDN and verifies the
  binaries byte-exact. Registering it auto-sets `WCR_LIVE=1` so it actually runs (instead
  of reporting a green "0 assertions"), so a default offline `ctest` never sees it.

```powershell
# Register + run the live 4.3.4 acceptance:
cmake --preset windows-vcpkg -DWCR_LIVE_TESTS=ON
cmake --build --preset windows-vcpkg
ctest --test-dir WoWClientRebuilder_build -C Release -R acceptance_cata434
```

## Localhost safety guard

When regenerating a full client (`client` mode), the tool generates a `WTF/RunOnce.wtf`
and `Data/<locale>/realmlist.wtf` that set the client realmlist to `127.0.0.1` as a
safety default. (`data` and `locale` modes produce no WTF files.)

1. **Localhost-only default**: The generated `RunOnce.wtf` sets the client realmlist to
   `127.0.0.1`. The rebuilt client will only attempt to connect to a server running on
   your own machine unless you change this file.
2. **No external server configured or recommended**: This project does not include,
   recommend, or configure any public or private-server address. No third-party server
   credentials or URLs are distributed with this tool.
3. **User responsibility**: Users who modify the generated WTF configuration to point at
   an external server are solely responsible for their own use of the rebuilt client and
   must ensure compliance with any applicable terms of service.

## License

WoWClientRebuilder's own source is released under the **GNU General Public License v2**
(see the header block in each source file). World of Warcraft and all related assets
remain copyright Blizzard Entertainment, Inc.; this tool redistributes none of them.
