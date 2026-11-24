@echo off
setlocal
cd /d "%~dp0"
title Build Windows Installer Wizard — SchoolCMS

echo ==============================================================================
echo              Building Windows Installer Wizard (.exe Setup)
echo ==============================================================================
echo.

where python >nul 2>nul
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Python is not installed or not in PATH!
    pause
    exit /b 1
)

python scripts\build_installer.py
if %ERRORLEVEL% NEQ 0 (
    echo.
    echo [ERROR] Installer generation failed!
    pause
    exit /b %ERRORLEVEL%
)

echo.
echo Setup wizard is ready in the dist\ folder!
pause
