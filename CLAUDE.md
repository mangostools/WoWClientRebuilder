# CLAUDE.md

Context for AI assistants — the Claude GitHub App (`@claude`) and contributors using Claude — working in
this repo. The machine-checkable style source of truth is [`.clang-format`](.clang-format); user-facing
behaviour is documented in [`README.md`](README.md).

## Project

**WoWClientRebuilder** — a C++17 command-line tool that regenerates a **complete, byte-identical World of
Warcraft client** from Blizzard's own still-live "pod-retail" CDN. It ships **only code**: at runtime it
downloads the program binaries, the `Data/` MPQ set, and the chosen locale(s), MD5-verifies every file, and
discards the temporary patch archives — leaving an install byte-identical to a real one.

**Cardinal invariants — do not break these:**

- **Zero Blizzard redistribution.** Never commit Blizzard binaries, MPQ contents, fixtures, or downloaded
  client data into the repo. The tool's whole reason to exist is that it distributes none of it. Test
  fixtures made from real client files stay local and are git-ignored; the fixture-gated acceptance tests
  skip cleanly when they are absent.
- **Supported builds are the pre-CASC pod-retail boundary only: Cataclysm 4.3.4 (15595) and MoP 5.4.8
  (18414).** WoD 6.x+/Legion/BfA (CASC + purged NGDP build-configs) and WotLK 3.3.5a (dead streaming CDN)
  are **not** regenerable — do not add recipes or code paths for them.
- **The CDN is HTTP-only.** The pod-retail hosts fail HTTPS certificate verification; this is expected. Do
  **not** "harden" the fetch by switching to `https://`.
- **The partial manifest is the trust anchor.** Its filename is the MD5 of its body, so `fetch_manifest`
  authenticates the download against the content hash pinned in the recipe before anything else is fetched.
  Keep that check fail-closed — a tampered/MITM'd manifest must not be able to redirect or substitute files.
- **Localhost safety.** Full-client (`client`) mode writes a realmlist of `127.0.0.1`. The tool never bundles,
  recommends, or defaults to any external server address.

## Build & test

CMake ≥ 3.21 + a vcpkg manifest. The **only** external dependency is `libcurl` (declared in
[`vcpkg.json`](vcpkg.json), pinned baseline); **StormLib, miniz, and doctest are vendored in `dep/`**. The
build/install trees are **siblings** of the source dir (see the `windows-vcpkg` preset).

```powershell
# VS 2026 + vcpkg (VCPKG_ROOT set). For VS 2022, configure manually (see README "Troubleshooting").
cmake --preset windows-vcpkg
cmake --build --preset windows-vcpkg
cmake --install ..\WoWClientRebuilder_build --config Release   # -> ..\WoWClientRebuilder_install
ctest --test-dir ..\WoWClientRebuilder_build -C Release        # offline doctest suite
```

- **CI must stay green.** AppVeyor builds the `windows-vcpkg` preset (Release) and runs `ctest`; the offline
  unit suite must pass on a checkout with **no** Blizzard data present.
- The **live 4.3.4 acceptance** test hits the real CDN and is registered only with `-DWCR_LIVE_TESTS=ON`
  (auto-sets `WCR_LIVE=1`); the **5.4.8 byte-exact** tests are fixture-gated and skip without local MPQs. A
  default `ctest` runs neither — never make the default suite depend on network or Blizzard fixtures.

## Code style

Source of truth: [`.clang-format`](.clang-format) (`InsertBraces: true` is enforced — run clang-format).
Non-default rules:

- **4-space indent, never tabs**; **80-column** lines (a little over is tolerable, don't reflow whole files).
- **Allman braces**, and **brace single-statement blocks** — even one-line `if`/`for`/`while`. Do not de-brace
  existing ones.
- **One space before `(` in control statements, none inside**: `if (x)`, not `if( x )`. Left-aligned pointers
  (`T* p`).
- Doxygen: `///` above a declaration, `///<` trailing, `/** ... */` multi-line. Every file carries the MaNGOS
  GPL-v2 header block.

## Repository etiquette

- This is a maintainer-owned repo (`mangostools/WoWClientRebuilder`). Keep commits **small and
  single-purpose** with clear messages; keep the `ctest` suite green in the same commit as the change.
- **The maintainer controls pushes, tags, and the AppVeyor deploy** — don't assume a local commit is
  published. Don't rewrite published history.
- Git-ignored and never committed: build/install trees, `vcpkg_installed/`, `live_*_out/`, any downloaded
  client data, and locally-built test fixtures.

## Architecture note

The pipeline is linear and each stage is a small, separately-tested unit (`src/wcrcore/`, driven by
`src/cli/`):

**recipe** (embedded `WoW.mfil` pointer per version) → **`fetch_manifest`** (downloads + content-hash
authenticates the partial manifest) → **`expand_data`** (filters records by mode + locale; per-locale
cinematics on by default, `--no-cinematics` opts out) → **`assemble_recipe`** → **`reconstruct`** (per
artifact: download, MD5/size-verify, then apply by source kind, then clean the scratch). Artifact source
kinds: `RepairMd5` (content-addressed `/repair/<c0>/<c1>/<MD5>`, **UPPERCASE**), `ZipMember`, `MpqPtch`
(PTCH/BSD0 patch apply), `MpqExtract` (recover aux binaries from the already-downloaded MPQs — no
redistribution), `Generated` (WTF/mfil), `PlainUrl`. Two front-ends share this core: an **interactive menu**
(no args) and a **scriptable flag CLI** ([`src/cli/args.cpp`](src/cli/args.cpp)).

## Review focus (for `@claude`)

Prioritise: **(1)** the **cardinal invariants** above — especially the zero-redistribution rule and the
manifest/`/repair` **trust boundary** (content-hash manifest auth fail-closed, per-file MD5 verification, no
HTTPS assumption, no path-traversal out of the output dir); **(2)** the **regenerable-version boundary** (no
CASC/WoD+/Wrath creep); **(3)** coding-standard conformance above; **(4)** build/CI impact (the offline
`ctest` stays green with no Blizzard data, fixtures never committed, the AppVeyor artifact stays portable).
Keep feedback concrete and minimal-diff; flag correctness/safety and standard issues, not style preferences
`.clang-format` doesn't cover.
