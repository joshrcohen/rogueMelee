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

After the first setup, the quickest release flow is to double-click
`Make Update Package.bat`. It rebuilds the mod, creates both ZIPs, and opens
`dist/`. The equivalent commands are `python tools/build.py` followed by
`python tools/package/build_mod_zip.py`.

Packaging creates two shareable artifacts:

- `dist/RogueMelee-Mod.zip` â€” full package for a first install.
- `dist/RogueMelee-Update.zip` â€” small update package for friends who already
  have `RogueMelee.iso`.

No game image, emulator, compiler, or save is committed. The Windows .NET
Framework compiler builds the portable patch, updater, and launch tools.
Set `ROGUEMELEE_VERSION` before packaging if you want a friendly version name;
otherwise the package uses the current short Git commit.

The upstream baseline is `11749c9ccbaf73bfc28a569650dfec5e18665a74`.
It is downloaded into ignored `.cache/`; extracted assets and build output are
also ignored. `python tools/build.py --prepare-only` verifies the upstream patch
without a game image. Preserve `build/dolphin-user` if using local saved settings.

## Play

For a first install, extract `RogueMelee-Mod.zip`, run **Apply Mod.exe** on your
original ISO, then run **Play in Slippi.exe** with the patched ISO and your
emulator. The launcher handles Slippi's separate mod-recognition file.

For updates, send `RogueMelee-Update.zip`. Your friend extracts it, runs
**Update RogueMelee.exe**, selects their existing `RogueMelee.iso`, and keeps
using that same ISO. The updater rebuilds the latest mod executable from the
clean US 1.02 executable already preserved inside the Rogue Melee image, verifies
it, appends it, and only then switches the disc header to the new executable.
It does not need the original ISO again and does not touch Dolphin/Slippi saves.

Newly-created Rogue Melee ISOs record the clean executable location for fast
future updates. Older Rogue Melee ISOs are supported by a verified fallback scan;
if a very old image cannot be identified, recreate it once with the latest
**Apply Mod.exe** and future updates are direct.

Choose **1-P Mode > Regular Match > Rogue Mode**. Camp choices use floor zones
and A; the native glowing exit starts the next fight.

## Tests and status

```powershell
python -m unittest discover -s tests -p "test_*.py" -v
```

CI checks packaging tests and compiles the portable patch, update, and launch
tools. The two C regression sources require the PC port's host compatibility
headers and are not run by CI.

This is an unfinished offline playtest. Physical adapter validation, full-run
compatibility and borrowed-special compatibility remain outstanding. Phillip AI
and online play are not enabled. Controller routing follows the port that selects
Rogue Mode. See the ZIP instructions for current playtest limitations.
