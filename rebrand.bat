@echo off
setlocal
cd /d "%~dp0"

title SchoolCMS Application Rebranding & Build Tool

if "%~1"=="" (
    python scripts\rebrand_app.py
) else (
    python scripts\rebrand_app.py %*
)

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo [ERROR] Process exited with error code %ERRORLEVEL%
    pause
)
