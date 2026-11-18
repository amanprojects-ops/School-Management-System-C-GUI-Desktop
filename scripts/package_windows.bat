@echo off
REM ==============================================================================
REM Windows Release Packaging Batch Script
REM ==============================================================================

cd /d "%~dp0\.."
echo Packaging SchoolCMS Windows Release...
C:\msys64\usr\bin\bash.exe -lc "export PATH=/ucrt64/bin:/usr/bin:$PATH && cd /c/apps/SchoolCMS && bash scripts/package_windows.sh"
if %ERRORLEVEL% EQU 0 (
    echo [SUCCESS] Windows bundle created in dist\
) else (
    echo [ERROR] Packaging failed with error code %ERRORLEVEL%
)
pause
