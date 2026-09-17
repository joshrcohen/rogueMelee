@echo off
cd /d "%~dp0"
if not exist "melee\build\game\sys\main.dol" (
  echo Build first: python melee\tools\build_rogue.py --image "path-to-US-1.02.iso"
  pause
  exit /b 1
)
start "" "Dolphin-x64\Dolphin.exe" -u "%~dp0melee\build\dolphin-user" -e "%~dp0melee\build\game\sys\main.dol"
