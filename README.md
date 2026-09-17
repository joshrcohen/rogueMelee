# Rogue Melee

Offline roguelite mod for Melee US 1.02, played through Dolphin or Slippi Dolphin.
This repository contains only the mod. The build fetches the pinned upstream
[decompilation](https://github.com/doldecomp/melee) automatically.

## Contents

- `mod/` — Rogue mode source.
- `patches/engine.patch` — changes to native Melee and its build configuration.
- `tools/` — build and portable packaging/update tools.
- `tests/` — regression tests.
- `Make Update Package.bat` — local build/package helper.
- `Publish Update.bat` — one-click GitHub Release publisher for friend updates.

## Build

Requires Windows, Python 3.10+, Git, Ninja, and your own US 1.02 game image.
For initial extraction, supply a standard Dolphin distribution containing
`DolphinTool.exe`. Slippi can be used to play the finished mod.

```powershell
python tools/build.py --image "C:\path\Melee.iso" --dolphin "C:\path\Dolphin.exe"
python tools/package/build_mod_zip.py
```

After the first setup, `Make Update Package.bat` rebuilds the mod and creates
local/test packages in `dist/`.

Packaging creates:

- `dist/RogueMelee-Mod.zip` — first-install package.
- `dist/RogueMelee-Update.zip` — manual fallback update package.
- `build/package/RogueMelee.exe` — self-updating friend launcher.
- `build/package/rogue.delta`, `version.txt`, and `GRGE01.ini` — release assets.

No game image, emulator, compiler, save, DOL, or delta is committed. The Windows
.NET Framework compiler builds the portable patch/update/launch tools. By
default package versions use the current short Git commit.

The upstream baseline is `11749c9ccbaf73bfc28a569650dfec5e18665a74`.
It is downloaded into ignored `.cache/`; extracted assets and build output are
also ignored. `python tools/build.py --prepare-only` verifies the upstream patch
without a game image.

## Friend install and automatic updates

For a first install, a friend extracts `RogueMelee-Mod.zip`, runs
**Apply Mod.exe** on their own clean US 1.02 image, and saves `RogueMelee.iso`.
They then run **RogueMelee.exe** and select:

1. that `RogueMelee.iso`;
2. their actual `Slippi Dolphin.exe` or `Dolphin.exe`.

After that, they only run **RogueMelee.exe**.

The launcher checks the repository's latest GitHub Release. If a newer build is
available, it downloads the tagged release's `rogue.delta`, `GRGE01.ini`, and
launcher, rebuilds the newest mod executable from the clean US 1.02 executable
preserved inside the Rogue Melee ISO, verifies it, appends it transactionally,
updates the disc metadata, refreshes the Slippi recognition file, and launches
the emulator.

The launcher stores selected paths and cached release assets under
`%APPDATA%\RogueMelee`. Run:

```text
RogueMelee.exe --reset
```

to select different ISO/emulator paths.

If GitHub is temporarily unavailable, a cached installed build can still launch.
The old **Update RogueMelee.exe** and **Play in Slippi.exe** are kept as manual
fallback tools.

## Publishing updates

The automatic launcher expects GitHub Releases tagged as:

```text
build-<version>
```

with these assets:

```text
rogue.delta
version.txt
GRGE01.ini
RogueMelee.exe
```

`Publish Update.bat` automates this. It requires the GitHub CLI (`gh`) to be
installed and authenticated. One-time setup:

```powershell
winget install --id GitHub.cli
gh auth login
```

Then, after committing changes on `main`, double-click:

```text
Publish Update.bat
```

The publisher:

1. refuses to publish a dirty working tree or non-`main` branch;
2. pushes `main`;
3. builds Rogue Melee;
4. generates `rogue.delta` and the Windows launcher;
5. uses the current 8-character commit SHA as the version;
6. creates/updates `build-<sha>` on GitHub Releases;
7. marks that release as Latest.

Friends receive it automatically the next time they run `RogueMelee.exe`.

## Play

Choose **1-P Mode > Regular Match > Rogue Mode**. Camp choices use floor zones
and A; the native glowing exit starts the next fight.

## Tests and status

```powershell
python -m unittest discover -s tests -p "test_*.py" -v
```

CI checks packaging tests and compiles the portable patch, manual updater,
manual launcher, and automatic `RogueMelee.exe` launcher. The C regression
sources require the PC port's host compatibility headers and are not run by CI.

This is an unfinished offline playtest. Physical adapter validation, full-run
compatibility and borrowed-special compatibility remain outstanding. Phillip AI
and online play are not enabled. Controller routing follows the port that
selects Rogue Mode.

## Borrowed-special compatibility QA

The ability registry contains all four special slots for all 26 playable
characters (104 source specials). Cross-character behavior is validated with a
host-driven Dolphin matrix rather than assuming that a loaded animation is safe.

Build and run the full recipient × source × slot × ground/air matrix:

```powershell
Run Ability Matrix.bat --iso "C:\path\Melee.iso" --dolphin "C:\path\Dolphin.exe" --fresh
```

That is 5,408 automated cases. Progress is written after every case, so the run
can be stopped and resumed by running the same command again without `--fresh`.
For a quick harness check first:

```powershell
Run Ability Matrix.bat --iso "C:\path\Melee.iso" --dolphin "C:\path\Dolphin.exe" --recipient Mario --source Falco --slot neutral --limit 2
```

The runner watches an in-game heartbeat. A process exit is recorded as a crash;
a stopped heartbeat is recorded as a hang. It restarts Dolphin and continues.
Results are written under `qa-results/` as CSV, JSON compatibility data, a
Markdown summary, and per-crash diagnostics. Use `--rerun-failures` after fixes
to repeat only cases that did not pass.

The QA build is local only (`ROGUE_QA=3`) and is never published by
`Publish Update.bat`; a normal build reconfigures without the QA flag.
