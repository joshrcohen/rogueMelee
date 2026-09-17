@echo off
setlocal EnableExtensions
cd /d "%~dp0"

where py >nul 2>nul
if %errorlevel%==0 (
    set "PY=py -3"
) else (
    set "PY=python"
)

%PY% tools\qa\ability_matrix.py %*
exit /b %errorlevel%
