# HANDOFF — add WoW 5.4.7 (build 17898) support to WoWClientRebuilder

**Written:** 2026-07-15. **Status:** IMPLEMENTED on `feat/mop547-recipe` (2026-07-15).
Measurement outcomes: both exe MD5s pinned from the real MPQs (`Wow.exe`
`C726D7F5EDF940F988CC63495B6CF340`, `Wow-64.exe` `51AF423413B0A1E9B17A299B67D94B88` —
§5.2 answered: 17898 DOES ship a 64-bit client); 17898 ships `MovieProxy.exe` and the
launcher as COPY-transform creation PTCHes (apply_ptch extended, own commit); the
final's no-op patches MPQ-verify the inherited dbghelp/divxdecoder /repair pins.
mfil discovery: 17898's own partial-manifest name proved unrecoverable (manifest
reconstruction validated byte-exact on sibling builds 17658/17930 → HTTP 200, but
every 17898 candidate 404s); the recipe pins the surviving `wow-17930` manifest, whose
data view is byte-exact for a 17898 client (its newest data generation IS 17898's,
stamped the same minute as 17898's final). Full record in the local working plan
(`docs/superpowers/plans/2026-07-15-add-547-recipe.md`, git-ignored) and the recipe
comment. Owner-driven steps remaining: full regeneration ×2, boot test, and the §7.5
extractor payoff test.
**Audience:** an agent with no prior context on this work. Everything you need is here.

---

## 1. What you are being asked to do

Add a third built-in recipe — **WoW 5.4.7, build 17898** — alongside the existing
`recipe_cata434()` (4.3.4 / 15595) and `recipe_mop548()` (5.4.8 / 18414).

The tool is already **data-driven by design** (`struct Recipe` in `src/wcrcore/recipe.h`).
This is intended to be a *data addition plus its verification*, not new infrastructure. If
you find yourself changing the download/patch engine, stop and reconsider — that is a signal
you have misread the structure.

## 2. Why — the motivation (read this; it changes what "done" means)

5.4.7 is wanted as a **test fixture**, not as a client to play.

A separate campaign is recovering the MoP 5.4.8.18414 opcode table directly from the client
binary. The problem: **you cannot validate a 5.4.8 opcode extractor against 5.4.8** — the
client is the only authority, so you would be grading your own homework.

5.4.7 solves that. WowPacketParser carries an **independently sniffed 5.4.7 opcode table**
(`Enums/Version/V5_4_7_17898/Opcodes.cs`, **418 opcodes**) — a real answer key for a build we
can regenerate. Run the extractor against a regenerated 5.4.7 binary, diff against those 418.
If it reproduces them, the extractor is **proven**; then point it at 5.4.8 and trust the
output. If it doesn't, the bug surfaces on a build where the answer is known, instead of
shipping a silently-wrong table for the build that matters.

**Important nuance, so nobody "helpfully" misuses this:** 5.4.7 opcode *values* are
**useless** for 5.4.8. Measured: of 292 symbols common to WPP's 5.4.7.17898 and 5.4.8.18291
tables, **289 changed (99.0%)** — Blizzard fully rescrambles opcodes across a patch. The 3
survivors are indistinguishable from chance. So 5.4.7 is **worthless as a source of answers
and excellent as a check on the method that produces answers.** Same data, opposite role.

Secondary benefit: this closes the one documented respect in which another project's tool is
ahead of ours (Dramacydal's regen carries 5.4.1 / 5.4.2 / 5.4.7; ours does not).

## 3. VERIFIED FACTS (checked live on 2026-07-15 — re-check before trusting)

All four MoP builds are **live right now** on the pod the tool already uses. HEAD only:

```bash
CDN="http://dist.blizzard.com.edgesuite.net/wow-pod-retail/EU/15890.direct"
for B in 18414 17898 17658 17538; do
  curl -s -I -m 25 "$CDN/Updates/wow-0-$B-Win-final.MPQ" | head -1
done
curl -s -I -m 25 "$CDN/Data/base-Win.MPQ" | head -1
```

| Build | Version | `Updates/wow-0-<B>-Win-final.MPQ` | HTTP | Content-Length |
|---|---|---|---|---|
| 18414 | 5.4.8 | *(the existing recipe — CONTROL)* | **200** | **21729424** ✅ matches `recipe_mop548`'s declared size exactly |
| **17898** | **5.4.7** | **the target** | **200** | **28633456** |
| 17658 | 5.4.2 | (bonus, if wanted) | 200 | 21681906 |
| 17538 | 5.4.1 | (bonus, if wanted) | 200 | 12301243 |

- **`Data/base-Win.MPQ` → HTTP 200, Content-Length 31096584** — **exactly** the size
  `recipe_mop548()` already declares. **The base MPQ is SHARED across builds on this pod.**
  A 5.4.7 recipe should reuse the identical `base` MpqSource (same URL, same size); only the
  `final` MPQ differs per build. This is the single biggest labour-saver here.
- **`wow-0-17898-Win-final.MPQ` Last-Modified: `Fri, 14 Feb 2014 18:49:39 GMT`** — the real
  5.4.7 patch date. Corroborates that this is the genuine 5.4.7 payload and not a redirect,
  placeholder, or silently-aliased 5.4.8.
- The control (18414) returning both 200 **and** its exactly-expected byte count proves the
  CDN path and the tool's existing assumptions are still valid — so a 5.4.7 failure would be
  a 5.4.7 problem, not a rotted CDN.

## 4. The code you will touch

Repo: `E:\Mangos\Repos\Mangos\WoWClientRebuilder` (HEAD `21fc7d9` at time of writing).

- **`src/wcrcore/recipe.h`** — the data model. Read it first; it is short.
  - `struct Recipe { version, build, repairBase, zips, mpqs, artifacts, regionManifests }`
  - `struct Artifact { outName, md5, source, zipKey, baseMpqKey, basePath, patchMpqKey,
    patchPath, content, url, size, locale, optional }`
  - `enum class Source { RepairMd5, ZipMember, MpqPtch, MpqExtract, Generated, PlainUrl }`
  - Declares `recipe_cata434()`, `recipe_mop548()`, `find_recipe(version)`.
- **`src/wcrcore/recipe.cpp`** — the recipes themselves. `recipe_mop548()` starts at ~line 177
  and is **your template**. Copy its shape.
- `find_recipe()` (~line 280) dispatches on the version string: `"4.3.4"` → cata, `"5.4.8"` →
  mop548. **Add `"5.4.7"` → `recipe_mop547()`.**

`recipe_mop548()`'s existing shape, for reference:

```cpp
static const std::string cdn =
    "http://dist.blizzard.com.edgesuite.net/wow-pod-retail/EU/15890.direct/";
static const Recipe r = {
    "5.4.8",
    "18414",
    "http://dist.blizzard.com.edgesuite.net/repair/wow",
    {},                                                   // zips: none for MoP
    {
        {"base",  cdn + "Data/base-Win.MPQ",              31096584},
        {"final", cdn + "Updates/wow-0-18414-Win-final.MPQ", 21729424}
    },
    {
        mpqPtch("Wow.exe",    "24FD2CBB340D57C51B6F7A1C1D60E693", "base",
                "Wow.exe",    "final", "pc-game-hdfiles\\Wow.exe"),
        mpqPtch("Wow-64.exe", "96EF9239F97336F453562D350C33BCC7", "base",
                "Wow-64.exe", "final", "pc-game-hdfiles\\Wow-64.exe"),
        repair("BackgroundDownloader.exe", ...),
        ...
    },
    { /* regionManifests: EU/NA region-locked names */ }
};
```

For 5.4.7 the `mpqs` block becomes (base **unchanged**, final **swapped**):
```cpp
{"base",  cdn + "Data/base-Win.MPQ",                 31096584},   // VERIFIED identical
{"final", cdn + "Updates/wow-0-17898-Win-final.MPQ", 28633456},   // VERIFIED live
```

## 5. What is NOT known — you must determine these

Do **not** guess any of these. Every one is a value that will fail *silently* or *late* if wrong.

1. **The MD5s of 5.4.7's `Wow.exe` / `Wow-64.exe`.** The MoP recipe hardcodes
   `24FD2CBB…` / `96EF9239…` for 18414. **5.4.7's WILL differ and are unknown.** They must be
   captured from a real regenerated 5.4.7 client and pinned. This is the main unknown and the
   main work.
2. **Does 5.4.7 even ship a 64-bit client?** 5.4.8 has `Wow-64.exe`. 5.4.7 predates it by ~3
   months (Feb vs May 2014). **Verify — do not assume.** If absent, omit that artifact (or mark
   `optional = true`); do not fabricate an MD5 for a file that does not exist.
3. **`regionManifests`.** MoP's partial-manifest name is **region-locked**. 18414 uses EU
   `wow-18414-E68C6C84….mfil` / NA `wow-18414-447E3E61….mfil`. **17898's equivalents are
   unknown** and must be discovered, not pattern-matched — the hex is a content hash, not a
   derivable string. See `region_rewrite_mfil()` in `recipe.h` and the `manifest_partial=`
   / `.mfil` handling around `recipe.cpp:254-267`.
4. **Which root binaries 5.4.7 needs from the `/repair` store**, and whether they 404 there.
   The 5.4.8 recipe documents that some binaries are deliberately absent and left to the
   user's `Repair.exe`; 5.4.7's set may differ. `optional = true` is the 404-tolerant escape.
5. **Whether `pc-game-hdfiles\Wow.exe` is the right in-MPQ path for 17898.** Verify against the
   actual MPQ rather than inheriting 5.4.8's path.
6. **An oddity worth explaining before you trust the layout:** 5.4.7's final MPQ (28.6 MB) is
   **LARGER** than 5.4.8's (21.7 MB), despite being the earlier build. If `wow-0-<build>` were
   a cumulative 0→build patch, later should be bigger. It isn't. Either `0` is a
   channel/type marker rather than a "from" version, or these are not cumulative. **Understand
   this before pinning sizes** — a wrong mental model here will produce a recipe that appears
   to work and yields a subtly wrong client.

## 6. Gotchas and standing constraints (from prior work on this tool)

- **Zero redistribution.** The tool fetches from Blizzard's live CDN and the content-addressed
  `/repair` store. It never ships game bytes. **Do not break this** — it is the entire legal
  posture of the project.
- **The CDN is HTTP-only.** Not a bug; do not "fix" it to HTTPS without checking it still works.
- **The partial manifest is content-addressed — filename == MD5 of the body.** That property is
  the tool's **authenticity anchor** over a plaintext transport. Preserve it. Past work fixed a
  manifest-overwrite bug and a path-traversal issue here; don't regress either.
- **MoP needs StormLib v4-MPQ plus manual PTCH/BSD0 handling.** 5.4.7 is the same era, so the
  same machinery should apply.
- **Do not regress the 4.3.4 URL.** Dramacydal's regen (the reference implementation for the
  extra builds, and worth lifting its *reflection auto-discovery* from) has a **dead 4.3.4
  URL**. Ours works. If you lift code from it, do not import that regression.
- **Regenerable boundary is pre-CASC.** 4.3.4 and MoP 5.4.x are regenerable; **WotLK 3.3.5a and
  WoD 6.x+ are proven dead ends** (CASC/NGDP purge). 5.4.7 sits inside the window — that is the
  only reason this is possible. Don't spend time trying to extend past it.
- **Testing division of labour (important):** the repo owner smoke-tests multi-GB downloads on
  a 900 Mbps line. **Agent tests must stay tiny** — offline unit tests plus at most one small
  fetch/HEAD. Do **not** pull a full client in an automated test. Add offline unit tests for
  the recipe data (URLs well-formed, sizes present, `find_recipe("5.4.7")` resolves), and hand
  the real regeneration run to the owner.

## 7. Suggested acceptance criteria

1. `find_recipe("5.4.7")` returns a valid recipe; `"4.3.4"` and `"5.4.8"` still resolve
   unchanged (**regression check — these are the shipping paths**).
2. Offline unit tests cover the new recipe's data the way the existing recipes are covered.
3. A **real** 5.4.7 regeneration run (owner-driven) produces `Wow.exe` whose MD5 is stable and
   reproducible across two runs; that MD5 is then **pinned** in the recipe.
4. The regenerated 5.4.7 client **boots**, matching the bar the 4.3.4 and 5.4.8 recipes already
   clear.
5. **The real payoff test:** the regenerated 5.4.7 binary is fed to the opcode extractor and
   reproduces WPP's 418 `V5_4_7_17898` values. That is what this build is *for* — a recipe that
   regenerates a client but fails this test has not delivered the point of the work.

## 8. Provenance of this document

- CDN facts: live HEAD requests, 2026-07-15, commands in §3 — re-runnable in seconds.
- Code structure: read from `recipe.h` / `recipe.cpp` at `21fc7d9`.
- The 99% rescramble figure: measured by diffing WPP's `V5_4_7_17898` and `V5_4_8_18291`
  tables. Recorded in the opcode campaign at
  `E:\Mangos\WIP\Four\FourOpCodeWork\claude\facts\FACTS_opcode_build_stability.md` (commit
  `348eac4`).
- **Everything in §5 is explicitly UNKNOWN.** It is listed so you check it, not so you assume
  it. A standing lesson from the sibling campaign: three separate "facts" in its notes turned
  out to be a transcription typo, a mislabelled reference, and an unverified claim — none
  catchable by code review, all caught by measurement. Verify; don't inherit.
