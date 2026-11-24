#!/usr/bin/env python3
"""
build_installer.py — Single-Click Windows Installer Wizard Generator
===================================================================
Packages the application distribution into a standalone Windows Setup Wizard
executable (.exe) using NSIS (Nullsoft Scriptable Install System).

Features of the generated installer:
- Modern UI 2 with Step-by-Step Wizard (Welcome, Directory, Progress, Finish)
- Desktop Shortcut with embedded application icon
- Start Menu Program Group with App and Uninstaller shortcuts
- Clean Windows Add/Remove Programs registry entry
- Ultra-high solid LZMA compression (~20 MB self-contained installer)
- Per-user installation without requiring Administrator UAC prompt

Usage:
    python scripts/build_installer.py [--skip-package] [--verbose]
"""

import os
import sys
import re
import shutil
import subprocess
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
DIST_DIR = REPO_ROOT / "dist"
INSTALLER_DIR = REPO_ROOT / "installer"
NSI_SCRIPT = INSTALLER_DIR / "installer.nsi"

MAKENSIS_PATHS = [
    Path(r"C:\msys64\ucrt64\bin\makensis.exe"),
    Path(r"C:\msys64\mingw64\bin\makensis.exe"),
    Path(r"C:\Program Files (x86)\NSIS\makensis.exe"),
    Path(r"C:\Program Files\NSIS\makensis.exe"),
    Path("makensis.exe"),
]


def find_makensis():
    """Find makensis executable path."""
    for p in MAKENSIS_PATHS:
        if p.exists():
            return str(p)
    which = shutil.which("makensis")
    if which:
        return which
    return None


def detect_app_info():
    """Read app name and version from CMakeLists.txt."""
    cmake_path = REPO_ROOT / "CMakeLists.txt"
    app_name = "APCOACHING"
    version = "1.1.3"

    if cmake_path.exists():
        with open(cmake_path, "r", encoding="utf-8") as f:
            content = f.read()
        m = re.search(r'project\(\s*([A-Za-z0-9_-]+)\s+VERSION\s+(\d+\.\d+\.\d+)', content)
        if m:
            app_name = m.group(1)
            version = m.group(2)

    return app_name, version


def main():
    print("=" * 66)
    print("       SchoolCMS Windows Installer Wizard Generator")
    print("=" * 66)

    app_name, version = detect_app_info()
    print(f"Target Application : {app_name}")
    print(f"Target Version     : v{version}")

    makensis = find_makensis()
    if not makensis:
        print("\n[ERROR] makensis.exe (NSIS) not found!")
        print("Please install NSIS via MSYS2 pacman:")
        print("    pacman -S mingw-w64-ucrt-x86_64-nsis")
        sys.exit(1)

    print(f"NSIS Compiler      : {makensis}")

    pkg_name = f"{app_name}-v{version}-windows-x64"
    pkg_dir = DIST_DIR / pkg_name

    # Check if distribution folder exists, otherwise run package_windows.py
    if not pkg_dir.exists() or not (pkg_dir / f"{app_name}.exe").exists():
        print(f"\n[INFO] Distribution directory {pkg_dir.name} not found or incomplete.")
        print("Running package_windows.py first...")
        pkg_script = REPO_ROOT / "scripts" / "package_windows.py"
        res = subprocess.run([sys.executable, str(pkg_script)])
        if res.returncode != 0:
            print("[ERROR] Packaging failed. Cannot build installer.")
            sys.exit(1)

    setup_exe = DIST_DIR / f"{app_name}-v{version}-Setup.exe"
    print(f"\nBuilding Installer Wizard: {setup_exe.name} ...")

    cmd = [
        makensis,
        f"/DAPP_NAME={app_name}",
        f"/DAPP_VERSION={version}",
        f"/DAPP_EXE={app_name}.exe",
        f"/DSOURCE_DIR={pkg_dir}",
        f"/DOUTPUT_SETUP={setup_exe}",
        str(NSI_SCRIPT)
    ]

    res = subprocess.run(cmd, cwd=str(INSTALLER_DIR), capture_output=True, text=True)

    if res.returncode != 0:
        print("\n[ERROR] makensis compilation failed:")
        print(res.stderr or res.stdout)
        sys.exit(1)

    if setup_exe.exists():
        size_mb = setup_exe.stat().st_size / (1024 * 1024)
        print("\n" + "=" * 66)
        print("SUCCESS! Windows Installer Wizard Generated:")
        print(f"  File : {setup_exe}")
        print(f"  Size : {size_mb:.2f} MB")
        print("=" * 66)
        print("\nFeatures:")
        print("  - Single self-contained Setup executable")
        print("  - Modern step-by-step installation wizard")
        print("  - Desktop shortcut with custom app icon")
        print("  - Start Menu shortcuts (App & Uninstaller)")
        print("  - Full uninstaller registered in Windows Settings > Installed Apps")
    else:
        print("[ERROR] Installer was not generated.")
        sys.exit(1)


if __name__ == "__main__":
    main()
