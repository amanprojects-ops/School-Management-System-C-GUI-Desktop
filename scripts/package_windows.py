#!/usr/bin/env python3
"""
SchoolCMS Windows Release Packaging Script.
Collects SchoolCMS.exe, all transitive runtime DLL dependencies from MSYS2 UCRT64,
GTK4 GSettings schemas, GDK-pixbuf loaders, assets, and builds a portable distribution bundle and zip.
"""

import os
import sys
import shutil
import subprocess
import zipfile
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parent.parent
BUILD_DIR = PROJECT_ROOT / "build"
DIST_DIR = PROJECT_ROOT / "dist"
APP_NAME = "APCOACHING"
EXE_NAME = "APCOACHING.exe"
PACKAGE_NAME = "APCOACHING-v1.1.6-windows-x64"
TARGET_DIR = DIST_DIR / PACKAGE_NAME
ZIP_FILE = DIST_DIR / f"{PACKAGE_NAME}.zip"

UCRT64_BIN = Path("C:/msys64/ucrt64/bin")
UCRT64_SHARE = Path("C:/msys64/ucrt64/share")
UCRT64_LIB = Path("C:/msys64/ucrt64/lib")

SYSTEM_DLLS = {
    "kernel32.dll", "user32.dll", "gdi32.dll", "shell32.dll", "ole32.dll",
    "oleaut32.dll", "advapi32.dll", "ws2_32.dll", "msvcrt.dll", "ntdll.dll",
    "comctl32.dll", "comdlg32.dll", "shlwapi.dll", "imm32.dll", "winmm.dll",
    "crypt32.dll", "secur32.dll", "iphlpapi.dll", "uxtheme.dll", "dwmapi.dll",
    "setupapi.dll", "wldap32.dll", "normaliz.dll", "version.dll", "bcrypt.dll",
    "dwrite.dll", "d3d11.dll", "dxgi.dll", "opengl32.dll", "mscms.dll"
}

def get_imported_dlls(pe_path):
    """Query imported DLL names from a PE binary using objdump."""
    objdump_cmd = str(UCRT64_BIN / "objdump.exe") if (UCRT64_BIN / "objdump.exe").exists() else "objdump"
    try:
        res = subprocess.run(
            [objdump_cmd, "-p", str(pe_path)],
            capture_output=True,
            text=True,
            check=True
        )
    except Exception as e:
        return []

    dlls = []
    for line in res.stdout.splitlines():
        line = line.strip()
        if line.startswith("DLL Name:"):
            dll_name = line.split(":", 1)[1].strip()
            dlls.append(dll_name)
    return dlls

def main():
    print(f"=== Packaging {PACKAGE_NAME} ===")
    exe_path = BUILD_DIR / EXE_NAME
    if not exe_path.exists():
        print(f"[ERROR] Executable not found at {exe_path}. Build the project first.")
        sys.exit(1)

    if TARGET_DIR.exists():
        def remove_readonly(func, path, exc_info):
            import stat
            os.chmod(path, stat.S_IWRITE)
            func(path)
        shutil.rmtree(TARGET_DIR, onexc=remove_readonly)
    TARGET_DIR.mkdir(parents=True, exist_ok=True)

    # 1. Copy executable
    print(f"Copying main executable: {exe_path.name}")
    shutil.copy2(exe_path, TARGET_DIR / EXE_NAME)

    # 2. Recursively find and copy DLL dependencies
    print("Resolving and collecting DLL dependencies...")
    visited_dlls = set()
    queue = [TARGET_DIR / EXE_NAME]

    # Explicitly include SVG renderer and GDK Pixbuf loaders in dependency resolution
    rsvg_dll = UCRT64_BIN / "librsvg-2-2.dll"
    if rsvg_dll.exists():
        dst_rsvg = TARGET_DIR / "librsvg-2-2.dll"
        shutil.copy2(rsvg_dll, dst_rsvg)
        queue.append(dst_rsvg)
        visited_dlls.add("librsvg-2-2.dll")

    loaders_dir = UCRT64_LIB / "gdk-pixbuf-2.0" / "2.10.0" / "loaders"
    if loaders_dir.exists():
        for loader in loaders_dir.glob("*.dll"):
            queue.append(loader)

    copied_count = len(visited_dlls)

    while queue:
        current_file = queue.pop(0)
        imported = get_imported_dlls(current_file)
        for dll_name in imported:
            dll_lower = dll_name.lower()
            if dll_lower.startswith("api-ms-win-") or dll_lower.startswith("ext-ms-win-") or dll_lower in SYSTEM_DLLS:
                continue

            if dll_lower not in visited_dlls:
                visited_dlls.add(dll_lower)
                src_dll = UCRT64_BIN / dll_name
                if src_dll.exists():
                    dst_dll = TARGET_DIR / dll_name
                    shutil.copy2(src_dll, dst_dll)
                    copied_count += 1
                    queue.append(dst_dll)
                else:
                    # Case-insensitive search in ucrt64/bin
                    found = False
                    for candidate in UCRT64_BIN.glob("*.dll"):
                        if candidate.name.lower() == dll_lower:
                            dst_dll = TARGET_DIR / candidate.name
                            shutil.copy2(candidate, dst_dll)
                            copied_count += 1
                            queue.append(dst_dll)
                            found = True
                            break
                    if not found:
                        print(f"  [Notice] Non-system DLL '{dll_name}' not in UCRT64 bin (assumed system or OS library)")

    print(f"Copied {copied_count} runtime DLLs.")

    # 3. Copy Assets
    assets_src = PROJECT_ROOT / "assets"
    if assets_src.exists():
        print("Copying assets directory...")
        shutil.copytree(assets_src, TARGET_DIR / "assets")

    # 4. Copy GLib Schemas
    schema_src = UCRT64_SHARE / "glib-2.0" / "schemas"
    if schema_src.exists():
        print("Copying GLib schemas...")
        schema_dst = TARGET_DIR / "share" / "glib-2.0" / "schemas"
        schema_dst.mkdir(parents=True, exist_ok=True)
        compiled = schema_src / "gschemas.compiled"
        if compiled.exists():
            shutil.copy2(compiled, schema_dst / "gschemas.compiled")
        for xml in schema_src.glob("*.gschema.xml"):
            shutil.copy2(xml, schema_dst / xml.name)

    # 5. Copy GDK Pixbuf loaders if available
    pixbuf_src = UCRT64_LIB / "gdk-pixbuf-2.0" / "2.10.0"
    if pixbuf_src.exists():
        print("Copying GDK Pixbuf loaders...")
        pixbuf_dst = TARGET_DIR / "lib" / "gdk-pixbuf-2.0" / "2.10.0"
        pixbuf_dst.mkdir(parents=True, exist_ok=True)
        loaders_src = pixbuf_src / "loaders"
        if loaders_src.exists():
            shutil.copytree(loaders_src, pixbuf_dst / "loaders", dirs_exist_ok=True)
        loaders_cache = pixbuf_src / "loaders.cache"
        if loaders_cache.exists():
            shutil.copy2(loaders_cache, pixbuf_dst / "loaders.cache")

    # 5.5. Copy Icon Themes
    icons_dst = TARGET_DIR / "share" / "icons"
    icons_dst.mkdir(parents=True, exist_ok=True)

    adwaita_src = UCRT64_SHARE / "icons" / "Adwaita"
    if adwaita_src.exists():
        print("Copying Adwaita icon theme...")
        adw_dst = icons_dst / "Adwaita"
        adw_dst.mkdir(parents=True, exist_ok=True)
        if (adwaita_src / "symbolic").exists():
            shutil.copytree(adwaita_src / "symbolic", adw_dst / "symbolic", dirs_exist_ok=True)
        if (adwaita_src / "index.theme").exists():
            shutil.copy2(adwaita_src / "index.theme", adw_dst / "index.theme")
        if (adwaita_src / "icon-theme.cache").exists():
            shutil.copy2(adwaita_src / "icon-theme.cache", adw_dst / "icon-theme.cache")

    hicolor_src = UCRT64_SHARE / "icons" / "hicolor"
    if hicolor_src.exists():
        print("Copying hicolor icon theme...")
        shutil.copytree(hicolor_src, icons_dst / "hicolor", dirs_exist_ok=True)

    # 6. Create Launchers & Docs
    print("Creating launch scripts and distribution documentation...")
    bat_content = f"""@echo off
cd /d "%~dp0"
set "XDG_DATA_DIRS=%~dp0share;%XDG_DATA_DIRS%"
set "GSETTINGS_SCHEMA_DIR=%~dp0share\\glib-2.0\\schemas"
set "GDK_PIXBUF_MODULE_FILE=%~dp0lib\\gdk-pixbuf-2.0\\2.10.0\\loaders.cache"
start "" "%~dp0{EXE_NAME}"
"""
    with open(TARGET_DIR / f"Run_{APP_NAME}.bat", "w", encoding="utf-8") as f:
        f.write(bat_content)

    readme_content = f"""# {APP_NAME} — Standalone Portable Windows Release
Version 1.1.6 (x86_64)

## Running the Application
Double-click `Run_{APP_NAME}.bat` or `{EXE_NAME}` to launch the application.

Default Administrator Credentials:
- Username: admin
- Password: Password123!

## Features Included:
- Student Lifecycle & Class Rostering
- Teacher & Faculty Directory
- Daily & Monthly Attendance Register
- Institutional Fee Collection & Accounting
- Examinations, Grading & Merit Rankings
- Weekly Timetable Grid & Conflict Prevention
- Multi-Category Reports & RFC 4180 CSV Export
- Institutional Settings, Theme Toggle & SQLite Disaster Recovery
"""
    with open(TARGET_DIR / "README.txt", "w", encoding="utf-8") as f:
        f.write(readme_content)

    # 7. Create Zip Archive
    print(f"Creating zip release archive: {ZIP_FILE.name}...")
    with zipfile.ZipFile(ZIP_FILE, "w", zipfile.ZIP_DEFLATED) as zipf:
        for root, dirs, files in os.walk(TARGET_DIR):
            for file in files:
                abs_file = Path(root) / file
                rel_file = abs_file.relative_to(DIST_DIR)
                zipf.write(abs_file, rel_file)

    zip_size_mb = ZIP_FILE.stat().st_size / (1024 * 1024)
    print(f"Packaging complete! Release archive: {ZIP_FILE} ({zip_size_mb:.2f} MB)")

if __name__ == "__main__":
    main()
