# Rogue Melee

Offline roguelite mod for Melee US 1.02, played through Dolphin or Slippi Dolphin.
This repository contains only the mod. The build fetches the pinned upstream
[decompilation](https://github.com/doldecomp/melee) automatically.

## Contents

- `mod/` — Rogue mode source.
- `patches/engine.patch` — changes to native Melee and its build configuration.
- `tools/` — build and portable ZIP packaging tools.
- `tests/` — regression tests.

## Build

Requires Windows, Python 3.10+, Git, Ninja, and your own US 1.02 game image.
For initial extraction, supply a standard Dolphin distribution containing
`DolphinTool.exe`. Slippi can be used to play the finished mod.

```powershell
python tools/build.py --image "C:\path\Melee.iso" --dolphin "C:\path\Dolphin.exe"
python tools/package/build_mod_zip.py
```

Subsequent builds: `python tools/build.py`. The shareable artifact is
`dist/RogueMelee-Mod.zip`. No game image, emulator, compiler, or save is committed.
The Windows .NET Framework compiler builds the portable patch and launch tools.

The upstream baseline is `11749c9ccbaf73bfc28a569650dfec5e18665a74`.
It is downloaded into ignored `.cache/`; extracted assets and build output are
also ignored. `python tools/build.py --prepare-only` verifies the upstream patch
without a game image. Preserve `build/dolphin-user` if using local saved settings.

## Play

Extract the ZIP, run **Apply Mod.exe** on your original ISO, then run
**Play in Slippi.exe** with the patched ISO and your emulator. The launcher
handles Slippi's separate mod-recognition file. Full instructions are in the ZIP.
Choose **1-P Mode > Regular Match > Rogue Mode**. Camp choices use floor zones
and A; the native glowing exit starts the next fight.

## Tests and status

```powershell
python -m unittest discover -s tests -p "test_*.py" -v
```

CI checks packaging tests and compiles the portable tools. The two C regression
sources require the PC port's host compatibility headers and are not run by CI.

This is an unfinished offline playtest. Physical adapter validation, full-run
compatibility and borrowed-special compatibility remain outstanding. Phillip AI
and online play are not enabled. Controller routing follows the port that selects
Rogue Mode. See the ZIP instructions for current playtest limitations.
