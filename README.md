# Rogue Melee

Offline roguelite mode built into the US 1.02 Melee decompilation. This is an
unfinished playtest for Dolphin and Slippi Dolphin, not an online mode.

Based on [doldecomp/melee](https://github.com/doldecomp/melee), at upstream
revision `11749c9ccbaf73bfc28a569650dfec5e18665a74`. Upstream documentation is
preserved in [.github/README.md](.github/README.md).

## Play

Use the portable `RogueMelee-Mod.zip` built by the packaging script. No game
image, extracted Nintendo assets, emulator, saves or compiler is committed.
The recipient needs an original US 1.02 Melee ISO/GCM and their own emulator.

1. Extract the ZIP and run **Apply Mod.exe** to create a new modded ISO.
2. Run **Play in Slippi.exe**, selecting the modded ISO and the actual
   **Slippi Dolphin.exe**. The launcher also accepts **Dolphin.exe**.
3. Choose **1-P Mode > Regular Match > Rogue Mode** in the game.

The portable launcher verifies the patched executable and places the separate
`GRGE01.ini` recognition file where Slippi needs it. It does not replace normal
Melee settings. See [playtest instructions](tools/package/ZIP-README.txt).
After a mod-code update, regenerate the ISO from the original game image.

## Development layout

Clone this repository as `melee` inside a workspace folder. Keep local emulator
files, game images and releases outside the source checkout:

```text
workspace/
  melee/                  this repository
  Dolphin-x64/            local Dolphin + DolphinTool, for extraction/testing
  .tools/bin/ninja.exe    optional local Ninja; otherwise install on PATH
  dist/                  generated shareable ZIP
  Launch Rogue Melee.cmd generated local launcher
```

The active mode is in `src/melee/rogue/`; engine hooks are in `src/melee/`.
Portable Windows packaging sources live in `tools/package/`.

## Build

Requirements: Windows, Python 3.10+, Ninja, the original US 1.02 image, and
the upstream compiler/binutils tooling downloaded by `configure.py`.
Use a standard Dolphin distribution with `DolphinTool.exe` for initial asset
extraction. Slippi is supported for play but is not the extraction tool.

From the `melee` checkout:

```powershell
python tools/build_rogue.py --image "C:\path\to\Melee-US-1.02.iso"
python tools/package/build_mod_zip.py
```

Subsequent builds omit `--image`. Add `--launch` to run the local build.
Packaging uses the Windows .NET Framework C# compiler. The source image is
read only; the build verifies the original DOL hash before accepting assets.
The upstream baseline must be an ancestor of the checkout; mod commits are
allowed. The current build intentionally uses non-matching code.

Generated assets and binaries stay in `build/` and `orig/`. Preserve
`build/dolphin-user` to retain local controls and saves. No proprietary game
data is required for the packaging unit tests or C# compilation checks.

## Validation

```powershell
python -m unittest discover -s tests -p "test_*.py" -v
```

CI runs those asset-free tests and compiles both portable Windows tools.
The PowerPC game build and emulator tests require a locally supplied game.
`rogue_state_test.c` and `rogue_hooks_test.c` are migration regression sources;
their host compilation currently requires the PC port's compatibility headers.
They are not part of the asset-free CI suite.

## Current scope and limits

Per-run upgrades, encounter progression, native character selection and stage
intros, post-fight rewards, and camp purchase zones are implemented. The native
camp glow advances to the next encounter. There are no permanent power upgrades.

Controller routing retains the port that selects Rogue Mode through character
selection, fights, rewards and camp. The port-2 bug is fixed in source and the
build, but physical GameCube adapter validation remains outstanding.

Borrowed-special adapters, full campaign compatibility and long-session testing
remain incomplete. Phillip is not enabled; bridge code is experimental and
disabled by default. Slippi boot has reached the memory-card screen in local
testing, but this is not a claim that all Slippi versions or complete runs pass.
