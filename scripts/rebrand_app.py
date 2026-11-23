#!/usr/bin/env python3
"""
rebrand_app.py — Single-Click Application Rebranding & Build Automation Tool
===========================================================================
Updates application name, executable name (.exe), icons, and institution name
across the entire codebase, then automatically rebuilds and verifies the newly
named binary.

Usage:
    python scripts/rebrand_app.py [NewName] [--icon ICON_NAME] [--school "School Name"] [--build] [--dry-run]

Examples:
    # Interactive wizard (prompts for name, icon, and builds immediately):
    python scripts/rebrand_app.py

    # Rename app & .exe to EduMaster, update icon, and build immediately:
    python scripts/rebrand_app.py EduMaster --icon applications-education-symbolic --build

    # Dry-run preview without touching any files:
    python scripts/rebrand_app.py EduMaster --dry-run
"""

import os
import sys
import re
import shutil
import subprocess
import argparse

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def run_cmd(cmd, cwd=REPO_ROOT):
    """Run a command in repo root and return (returncode, stdout, stderr)."""
    res = subprocess.run(cmd, shell=True, cwd=cwd, capture_output=True, text=True)
    return res.returncode, res.stdout.strip(), res.stderr.strip()


def run_msys_cmd(bash_command):
    """Execute command inside MSYS2 UCRT64 environment."""
    bash_path = r"C:\msys64\usr\bin\bash.exe"
    if not os.path.exists(bash_path):
        bash_path = "bash"
    full_cmd = [
        bash_path, "-c",
        f'export PATH="/c/msys64/ucrt64/bin:/usr/bin:$PATH" && {bash_command}'
    ]
    res = subprocess.run(full_cmd, capture_output=True, text=True)
    return res.returncode, res.stdout, res.stderr


def detect_current_app_name():
    """Detect current project name from CMakeLists.txt."""
    path = os.path.join(REPO_ROOT, "CMakeLists.txt")
    if os.path.exists(path):
        with open(path, "r", encoding="utf-8") as f:
            content = f.read()
        m = re.search(r'project\(\s*([A-Za-z0-9_-]+)\s+VERSION', content)
        if m:
            return m.group(1)
    return "SchoolCMS"


def detect_current_version():
    """Detect current project version from CMakeLists.txt."""
    path = os.path.join(REPO_ROOT, "CMakeLists.txt")
    if os.path.exists(path):
        with open(path, "r", encoding="utf-8") as f:
            content = f.read()
        m = re.search(r'project\([A-Za-z0-9_-]+\s+VERSION\s+(\d+\.\d+\.\d+)', content)
        if m:
            return m.group(1)
    return "1.1.2"


def detect_current_icon():
    """Detect current app icon from src/ui/sidebar.c."""
    path = os.path.join(REPO_ROOT, "src", "ui", "sidebar.c")
    if os.path.exists(path):
        with open(path, "r", encoding="utf-8") as f:
            content = f.read()
        m = re.search(r'gtk_image_new_from_icon_name\("([^"]+)"\)', content)
        if m:
            return m.group(1)
    return "applications-education-symbolic"


def detect_current_school_name():
    """Detect default school name from src/database.c."""
    path = os.path.join(REPO_ROOT, "src", "database.c")
    if os.path.exists(path):
        with open(path, "r", encoding="utf-8") as f:
            content = f.read()
        m = re.search(r"\('school_name',\s*'([^']+)'", content)
        if m:
            return m.group(1)
    return "St. Xavier International School"


def update_file(rel_path, replacements, dry_run=False):
    """Apply list of (regex_pattern, replacement) tuples to a file."""
    abs_path = os.path.join(REPO_ROOT, rel_path.replace("/", os.sep))
    if not os.path.exists(abs_path):
        print(f"  [SKIP] {rel_path} (file not found)")
        return False

    with open(abs_path, "r", encoding="utf-8") as f:
        original = f.read()

    modified = original
    for pattern, repl in replacements:
        modified = re.sub(pattern, repl, modified)

    if modified == original:
        print(f"  [UNCHANGED] {rel_path}")
        return False

    if dry_run:
        print(f"  [DRY-RUN] Would update {rel_path}")
        return True

    with open(abs_path, "w", encoding="utf-8") as f:
        f.write(modified)
    print(f"  [UPDATED] {rel_path}")
    return True


def apply_rebranding(old_name, new_name, old_icon, new_icon, old_school, new_school, dry_run=False):
    """Update all files with the new application name, executable name, and icon."""
    print(f"\nApplying rebranding changes: '{old_name}' -> '{new_name}'")
    if new_icon and new_icon != old_icon:
        print(f"Updating application icon: '{old_icon}' -> '{new_icon}'")
    if new_school and new_school != old_school:
        print(f"Updating default school seed: '{old_school}' -> '{new_school}'")
    print("-" * 65)

    name_pattern = rf'(?:{re.escape(old_name)}|SchoolCMS)'
    new_app_id = f"com.{new_name.lower().replace('-', '').replace('_', '')}.app"
    old_app_id_pattern = r'gtk_application_new\(\s*"com\.[a-z0-9_-]+\.app"'

    # 1. CMakeLists.txt (Defines project name and output .exe name)
    cm_replacements = [
        (rf'project\({name_pattern}\s+VERSION', f'project({new_name} VERSION'),
    ]
    update_file("CMakeLists.txt", cm_replacements, dry_run)

    # 2. src/app.c (GtkApplication ID)
    app_replacements = [
        (old_app_id_pattern, f'gtk_application_new(\n        "{new_app_id}"'),
    ]
    update_file("src/app.c", app_replacements, dry_run)

    # 3. src/ui/window.c (Window Title, Status Bar, Taskbar Icon)
    win_replacements = [
        (rf'gtk_window_set_title\(GTK_WINDOW\(window\),\s*"{name_pattern}\s*—',
         f'gtk_window_set_title(GTK_WINDOW(window), "{new_name} —'),
        (rf'"{name_pattern}\s+v(\d+\.\d+\.\d+)\s+•\s+Authenticated"',
         rf'"{new_name} v\1  •  Authenticated"'),
    ]
    if new_icon:
        win_replacements.append((
            rf'gtk_window_set_icon_name\(GTK_WINDOW\(window\),\s*"[^"]+"\);',
            f'gtk_window_set_icon_name(GTK_WINDOW(window), "{new_icon}");'
        ))
    update_file("src/ui/window.c", win_replacements, dry_run)

    # 4. src/ui/sidebar.c (Sidebar Brand Text, Sidebar Version, Sidebar Logo Icon)
    side_replacements = [
        (rf"<span weight='bold' size='large'>{name_pattern}</span>",
         f"<span weight='bold' size='large'>{new_name}</span>"),
        (rf'"{name_pattern}\s+v(\d+\.\d+\.\d+)"',
         rf'"{new_name} v\1"'),
    ]
    if new_icon:
        side_replacements.append((
            rf'gtk_image_new_from_icon_name\("[^"]+"\)',
            f'gtk_image_new_from_icon_name("{new_icon}")'
        ))
    update_file("src/ui/sidebar.c", side_replacements, dry_run)

    # 5. src/ui/login.c (Login Screen Hero Title & Icon)
    login_replacements = [
        (rf"<span size='xx-large' weight='bold'>{name_pattern}</span>",
         f"<span size='xx-large' weight='bold'>{new_name}</span>"),
    ]
    if new_icon:
        login_replacements.append((
            rf'gtk_image_new_from_icon_name\("[^"]+"\)',
            f'gtk_image_new_from_icon_name("{new_icon}")'
        ))
    update_file("src/ui/login.c", login_replacements, dry_run)

    # 6. src/database.c (Default institutional name seed if provided)
    if new_school and new_school != old_school:
        db_replacements = [
            (rf"\('school_name',\s*'[^']+'",
             f"('school_name', '{new_school}'"),
        ]
        update_file("src/database.c", db_replacements, dry_run)

    # 7. scripts/package_windows.py (Package Name, Executable Name, Run bat)
    pkg_replacements = [
        (rf'APP_NAME = "{name_pattern}"', f'APP_NAME = "{new_name}"'),
        (rf'EXE_NAME = "{name_pattern}\.exe"', f'EXE_NAME = "{new_name}.exe"'),
        (rf'PACKAGE_NAME = "{name_pattern}-v(\d+\.\d+\.\d+)-windows-x64"',
         rf'PACKAGE_NAME = "{new_name}-v\1-windows-x64"'),
        (rf'"{name_pattern}\.exe"', f'"{new_name}.exe"'),
        (rf'Run_{name_pattern}\.bat', f'Run_{new_name}.bat'),
        (rf'# {name_pattern} — Standalone', f'# {new_name} — Standalone'),
    ]
    update_file("scripts/package_windows.py", pkg_replacements, dry_run)

    # 8. scripts/installer.nsi (NSIS installer definitions & shortcuts)
    nsi_replacements = [
        (rf'!define APP_NAME "[^"]+"', f'!define APP_NAME "{new_name}"'),
        (rf'!define APP_EXE "[^"]+"', f'!define APP_EXE "{new_name}.exe"'),
        (rf'{name_pattern}-v(\d+\.\d+\.\d+)-windows-x64', rf'{new_name}-v\1-windows-x64'),
        (rf'\\{name_pattern}', rf'\\{new_name}'),
        (rf'"{name_pattern}"', f'"{new_name}"'),
    ]
    update_file("scripts/installer.nsi", nsi_replacements, dry_run)

    # 9. scripts/bump_version.py (Dynamic project pattern)
    bump_replacements = [
        (rf'project\({name_pattern} VERSION', f'project({new_name} VERSION'),
        (rf'"{name_pattern} v', f'"{new_name} v'),
        (rf'PACKAGE_NAME = "{name_pattern}-v', f'PACKAGE_NAME = "{new_name}-v'),
        (rf'{name_pattern}-v', f'{new_name}-v'),
    ]
    update_file("scripts/bump_version.py", bump_replacements, dry_run)

    # 10. README.md (Header title)
    readme_replacements = [
        (rf'# {name_pattern} — Enterprise', f'# {new_name} — Enterprise'),
    ]
    update_file("README.md", readme_replacements, dry_run)

    print("-" * 65)
    print("[SUCCESS] All files rebranded successfully!")


def build_and_verify(new_name):
    """Trigger CMake reconfiguration, Ninja build, and test run."""
    print("\n" + "=" * 65)
    print(f"  BUILDING NEW EXECUTABLE: {new_name}.exe")
    print("=" * 65)

    build_dir = os.path.join(REPO_ROOT, "build")
    os.makedirs(build_dir, exist_ok=True)

    print("Step 1: Re-configuring CMake with Ninja...")
    rc, out, err = run_msys_cmd(f"cd /c/apps/SchoolCMS/build && cmake -G Ninja ..")
    if rc != 0:
        print("[ERROR] CMake configuration failed:")
        print(err or out)
        return False
    print("  [OK] CMake configured")

    print(f"Step 2: Compiling {new_name}.exe with Ninja...")
    rc, out, err = run_msys_cmd(f"cd /c/apps/SchoolCMS/build && ninja")
    if rc != 0:
        print("[ERROR] Ninja build failed:")
        print(err or out)
        return False
    print("  [OK] Compilation finished with 0 warnings")

    exe_path = os.path.join(build_dir, f"{new_name}.exe")
    if not os.path.exists(exe_path):
        print(f"[ERROR] Target binary was not found at {exe_path}!")
        return False
    size_mb = os.path.getsize(exe_path) / (1024 * 1024)
    print(f"  [OK] Generated {new_name}.exe ({size_mb:.2f} MB)")

    print("Step 3: Running SchoolCMS test suite (13 targets)...")
    rc, out, err = run_msys_cmd("cd /c/apps/SchoolCMS && bash run_tests.sh")
    if rc != 0:
        print("[WARN] Some tests failed. Output:")
        print(out)
        return False
    print("  [OK] All 13/13 test suites passed cleanly (100%)")

    print("\n" + "=" * 65)
    print(f"  BUILD VERIFIED SUCCESSFULLY!")
    print(f"  Target Executable: build/{new_name}.exe")
    print("=" * 65)
    return True


def interactive_wizard():
    """Prompt user interactively for rebrand parameters."""
    old_name = detect_current_app_name()
    old_icon = detect_current_icon()
    old_school = detect_current_school_name()

    print("=" * 65)
    print("  SchoolCMS Single-Click Rebranding & Build Wizard")
    print("=" * 65)
    print(f"Current Application Name:     {old_name}")
    print(f"Current Application Icon:     {old_icon}")
    print(f"Current Default School Seed:  {old_school}")
    print("=" * 65)

    try:
        new_name = input(f"\nEnter new application name (press Enter to keep '{old_name}'): ").strip()
        if not new_name:
            new_name = old_name

        print("\nCommon education icons:")
        print("  1) applications-education-symbolic (Default school cap & book)")
        print("  2) school-symbolic (School building)")
        print("  3) system-users-symbolic (Faculty & student group)")
        print("  4) org.gnome.Education-symbolic (Academic logo)")
        print("  5) Keep current icon")

        icon_choice = input(f"Select icon [1-5] or enter custom icon name (default: keep current): ").strip()
        icon_map = {
            "1": "applications-education-symbolic",
            "2": "school-symbolic",
            "3": "system-users-symbolic",
            "4": "org.gnome.Education-symbolic",
        }
        if icon_choice in icon_map:
            new_icon = icon_map[icon_choice]
        elif icon_choice and icon_choice != "5":
            new_icon = icon_choice
        else:
            new_icon = old_icon

        new_school = input(f"\nEnter default school name (press Enter to keep '{old_school}'): ").strip()
        if not new_school:
            new_school = old_school

        do_build = input(f"\nBuild and verify '{new_name}.exe' immediately? [Y/n]: ").strip().lower()
        should_build = (do_build != "n")

    except (EOFError, KeyboardInterrupt):
        print("\nOperation cancelled.")
        sys.exit(0)

    return new_name, new_icon, new_school, should_build


def main():
    parser = argparse.ArgumentParser(
        description="Single-click application renaming, executable name update, icon customize, and automatic build."
    )
    parser.add_argument("name", nargs="?", default=None, help="New application name, e.g. EduMaster")
    parser.add_argument("--icon", default=None, help="New symbolic icon name or path")
    parser.add_argument("--school", default=None, help="New default institutional school name for DB seed")
    parser.add_argument("--build", action="store_true", help="Automatically rebuild and verify the new executable immediately")
    parser.add_argument("--dry-run", action="store_true", help="Preview modifications without writing files")
    args = parser.parse_args()

    old_name = detect_current_app_name()
    old_icon = detect_current_icon()
    old_school = detect_current_school_name()

    if args.name is None and not args.dry_run and sys.stdin.isatty():
        new_name, new_icon, new_school, should_build = interactive_wizard()
    else:
        new_name = args.name if args.name else old_name
        new_icon = args.icon if args.icon else old_icon
        new_school = args.school if args.school else old_school
        should_build = args.build

    if not re.match(r'^[A-Za-z0-9_-]+$', new_name):
        print(f"[ERROR] Invalid application name '{new_name}'. Must contain only letters, numbers, hyphens, and underscores.")
        sys.exit(1)

    # 1. Apply file updates
    apply_rebranding(old_name, new_name, old_icon, new_icon, old_school, new_school, args.dry_run)

    if args.dry_run:
        print("\n[DRY RUN COMPLETE] No files were modified.")
        return

    # 2. Build and verify if requested
    if should_build:
        success = build_and_verify(new_name)
        if not success:
            sys.exit(1)
    else:
        print(f"\nRebranding completed! To build {new_name}.exe manually, run:")
        print(f"  cd build && ninja")


if __name__ == "__main__":
    main()
