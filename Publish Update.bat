@echo off
setlocal EnableExtensions
cd /d "%~dp0"

echo RogueMelee publisher
echo ====================
echo.

where git >nul 2>nul
if errorlevel 1 (
    echo Git was not found on PATH.
    goto :fail
)

where gh >nul 2>nul
if errorlevel 1 (
    echo GitHub CLI was not found.
    echo Install it with:
    echo   winget install --id GitHub.cli
    echo Then run:
    echo   gh auth login
    goto :fail
)

gh auth status >nul 2>nul
if errorlevel 1 (
    echo GitHub CLI is not logged in.
    echo Run: gh auth login
    goto :fail
)

for /f "delims=" %%B in ('git branch --show-current') do set "BRANCH=%%B"
if /I not "%BRANCH%"=="main" (
    echo Publish Update.bat must be run from the main branch.
    echo Current branch: %BRANCH%
    goto :fail
)

set "DIRTY="
for /f "delims=" %%S in ('git status --porcelain') do set "DIRTY=1"
if defined DIRTY (
    echo The repository has uncommitted changes.
    echo Commit or stash them before publishing.
    git status --short
    goto :fail
)

echo [1/5] Pushing main...
git push origin main
if errorlevel 1 goto :fail

for /f "delims=" %%V in ('git rev-parse --short^=8 HEAD') do set "VERSION=%%V"
for /f "delims=" %%C in ('git rev-parse HEAD') do set "COMMIT=%%C"
set "TAG=build-%VERSION%"
set "ROGUEMELEE_VERSION=%VERSION%"

echo [2/5] Building RogueMelee %VERSION%...
where py >nul 2>nul
if %errorlevel%==0 (
    set "PY=py -3"
) else (
    set "PY=python"
)

%PY% tools\build.py
if errorlevel 1 goto :fail

echo [3/5] Building launcher and release assets...
%PY% tools\package\build_mod_zip.py
if errorlevel 1 goto :fail

if not exist "build\package\rogue.delta" goto :missing
if not exist "build\package\version.txt" goto :missing
if not exist "build\package\GRGE01.ini" goto :missing
if not exist "build\package\RogueMelee.exe" goto :missing

echo [4/5] Publishing GitHub Release %TAG%...
gh release view "%TAG%" >nul 2>nul
if errorlevel 1 (
    gh release create "%TAG%" ^
      "build\package\rogue.delta" ^
      "build\package\version.txt" ^
      "build\package\GRGE01.ini" ^
      "build\package\RogueMelee.exe" ^
      "dist\RogueMelee-Mod.zip" ^
      "dist\RogueMelee-Update.zip" ^
      --target "%COMMIT%" ^
      --title "RogueMelee %VERSION%" ^
      --notes "Rogue Bracket update: adds native-style route choice between fights, seeded encounter branches, act history, boss previews, and Tournament/All-Star-inspired presentation while retaining the full 26-fighter / 104-special system." ^
      --latest
    if errorlevel 1 goto :fail
) else (
    gh release upload "%TAG%" ^
      "build\package\rogue.delta" ^
      "build\package\version.txt" ^
      "build\package\GRGE01.ini" ^
      "build\package\RogueMelee.exe" ^
      "dist\RogueMelee-Mod.zip" ^
      "dist\RogueMelee-Update.zip" ^
      --clobber
    if errorlevel 1 goto :fail
    gh release edit "%TAG%" --latest
    if errorlevel 1 goto :fail
)

echo [5/5] Published.
echo.
echo Version: %VERSION%
echo Tag:     %TAG%
echo.
echo Friends using RogueMelee.exe will receive this build automatically
echo the next time they launch the game.
echo.
pause
exit /b 0

:missing
echo A required release asset was not generated.
goto :fail

:fail
echo.
echo Publish failed. Review the error above.
pause
exit /b 1
