@echo off
setlocal
cd /d "%~dp0"

where py >nul 2>nul
if %errorlevel%==0 (
    set "PY=py -3"
) else (
    set "PY=python"
)

echo [1/2] Building Rogue Melee...
%PY% tools\build.py
if errorlevel 1 goto :fail

echo.
echo [2/2] Building shareable packages...
%PY% tools\package\build_mod_zip.py
if errorlevel 1 goto :fail

echo.
echo Done.
echo Send dist\RogueMelee-Update.zip to friends who already have RogueMelee.iso.
echo Use dist\RogueMelee-Mod.zip for a fresh install.
start "" explorer.exe "%CD%\dist"
exit /b 0

:fail
echo.
echo Build/package failed. Review the error above.
pause
exit /b 1
