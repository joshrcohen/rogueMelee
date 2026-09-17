ROGUE MELEE UPDATE

For friends who already have a RogueMelee.iso:

1. Extract every file from RogueMelee-Update.zip into one folder.
2. Double-click "Update RogueMelee.exe".
3. Select your existing RogueMelee.iso.
4. When it says the update succeeded, keep using that same ISO.

The updater does not need your original Melee ISO. RogueMelee keeps the clean US 1.02
main.dol inside the image, and the updater rebuilds only the latest mod executable.
It appends and verifies the new executable before switching the ISO to it.

You can jump from an older Rogue Melee build straight to the newest update package.
Very old RogueMelee ISOs made before updater metadata existed are supported by a
fallback scan. If that scan cannot identify the clean executable, recreate the ISO
once using the newest Apply Mod.exe; updates after that are direct.

Do not rename or separate rogue.delta from Update RogueMelee.exe.
The updater does not touch Dolphin/Slippi saves or settings.

After updating, use the "Play in Slippi.exe" included with the SAME update ZIP.
That launcher verifies the ISO against the new patch and installs/verifies GRGE01.ini.

If the updater is interrupted before the final switch, it restores the previous ISO
header and length so the old build remains selected.
