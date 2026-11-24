@echo off
setlocal
cd /d "%~dp0"

if "%~1"=="" (
    python scripts\bump_version.py
) else (
    python scripts\bump_version.py %*
)
if %ERRORLEVEL% NEQ 0 (
    echo.
    echo [ERROR] Version bump failed with code %ERRORLEVEL%
)
pause
