ROGUE MELEE AUTOMATIC UPDATES

If you already have RogueMelee.iso:

1. Keep RogueMelee.exe somewhere convenient.
2. Double-click RogueMelee.exe.
3. The first time only, select your existing RogueMelee.iso and your actual
   Slippi Dolphin.exe.
4. From then on, use RogueMelee.exe whenever you want to play.

RogueMelee.exe checks joshrcohen/rogueMelee GitHub Releases for the newest build.
When a newer build exists it downloads rogue.delta, version.txt, GRGE01.ini and
the newest launcher, verifies the generated executable, updates your existing ISO
transactionally, then launches Slippi.

You do not need the original Melee ISO again for normal updates, and you do not
need Git, Python, Ninja, or future update ZIPs.

If the internet is unavailable, RogueMelee.exe can launch a previously cached
installed build. Run "RogueMelee.exe --reset" to forget the saved ISO/emulator
paths and choose them again.

Update RogueMelee.exe and Play in Slippi.exe remain in the package as manual
fallback tools.

CRASH REPORTS:
RogueMelee.exe automatically writes the latest session diagnostic to:
%APPDATA%\RogueMelee\Crash Reports\Last session report.txt

If the emulator exits abnormally it also creates a timestamped
RogueMelee-crash-*.txt file and tells you where it was saved. Run
"RogueMelee.exe --crash-reports" to open the report folder. Send the latest
report when a playtest crashes, even if Dolphin later closes normally.
