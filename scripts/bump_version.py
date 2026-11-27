#!/usr/bin/env python3
"""
bump_version.py — SchoolCMS / APCOACHING Version Bump Script
============================================================
Updates the version string across all project files, updates CHANGELOG.md,
creates individual git commits (1 commit per file), creates an annotated
git tag, and optionally pushes to origin.

Usage:
    python scripts/bump_version.py [new_version] [--patch | --minor | --major]
                                   [--message "Release notes"] [--push] [--dry-run]

Examples:
    python scripts/bump_version.py                   # Interactive menu
    python scripts/bump_version.py --patch           # Auto-bump patch (1.1.3 -> 1.1.4)
    python scripts/bump_version.py --minor           # Auto-bump minor (1.1.3 -> 1.2.0)
    python scripts/bump_version.py 1.2.0             # Explicit version
    python scripts/bump_version.py --patch --push    # Bump patch and push to GitHub
    python scripts/bump_version.py 1.2.0 --dry-run   # Preview changes without modifying
"""

import sys
import os
import re
import subprocess
import argparse

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


# ---------------------------------------------------------------------------
# Dynamic App & Version Detection
# ---------------------------------------------------------------------------

def detect_app_info():
    """Read the current project name and version from CMakeLists.txt."""
    path = os.path.join(REPO_ROOT, "CMakeLists.txt")
    app_name = "APCOACHING"
    version = "1.1.3"

    if os.path.exists(path):
        with open(path, "r", encoding="utf-8") as f:
            content = f.read()
        m = re.search(r'project\(\s*([A-Za-z0-9_-]+)\s+VERSION\s+(\d+\.\d+\.\d+)', content)
        if m:
            app_name = m.group(1)
            version = m.group(2)

    return app_name, version


def validate_semver(version):
    """Ensure version matches MAJOR.MINOR.PATCH."""
    if not re.fullmatch(r'\d+\.\d+\.\d+', version):
        print(f"[ERROR] Version must be MAJOR.MINOR.PATCH format (e.g. 1.2.0). Got: '{version}'")
        sys.exit(1)


def calculate_bump(old_ver, bump_type):
    """Calculate the next semver based on bump type ('patch', 'minor', 'major')."""
    parts = [int(p) for p in old_ver.split(".")]
    if len(parts) != 3:
        return None
    major, minor, patch = parts
    if bump_type == "patch":
        patch += 1
    elif bump_type == "minor":
        minor += 1
        patch = 0
    elif bump_type == "major":
        major += 1
        minor = 0
        patch = 0
    return f"{major}.{minor}.{patch}"


# ---------------------------------------------------------------------------
# Shell & Git Execution
# ---------------------------------------------------------------------------

def run(cmd, cwd=REPO_ROOT, capture=True):
    """Run a shell command and return (returncode, stdout, stderr)."""
    result = subprocess.run(
        cmd, shell=True, cwd=cwd,
        capture_output=capture, text=True
    )
    return result.returncode, result.stdout.strip(), result.stderr.strip()


# ---------------------------------------------------------------------------
# File Replacement Definitions
# ---------------------------------------------------------------------------

def get_version_files(app_name):
    """Generate replacement definitions dynamically for the detected app name."""
    return [
        {
            "path": "include/app.h",
            "replacements": [
                (
                    r'#define APP_VERSION "\d+\.\d+\.\d+"',
                    r'#define APP_VERSION "{new}"'
                ),
            ],
        },
        {
            "path": "CMakeLists.txt",
            "replacements": [
                (
                    r'project\(\s*([A-Za-z0-9_-]+)\s+VERSION\s+\d+\.\d+\.\d+',
                    r'project(\1 VERSION {new}'
                ),
            ],
        },
        {
            "path": "src/ui/sidebar.c",
            "replacements": [
                (
                    r'gtk_label_new\("([A-Za-z0-9_-]+)\s+v\d+\.\d+\.\d+"\)',
                    r'gtk_label_new("\1 v{new}")'
                ),
            ],
        },
        {
            "path": "src/ui/window.c",
            "replacements": [
                (
                    r'gtk_label_new\("([A-Za-z0-9_-]+)\s+v\d+\.\d+\.\d+\s+•\s+Authenticated"\)',
                    r'gtk_label_new("\1 v{new}  •  Authenticated")'
                ),
            ],
        },
        {
            "path": "scripts/package_windows.py",
            "replacements": [
                (
                    r'PACKAGE_NAME = "([A-Za-z0-9_-]+)-v\d+\.\d+\.\d+-windows-x64"',
                    r'PACKAGE_NAME = "\1-v{new}-windows-x64"'
                ),
                (
                    r'Version \d+\.\d+\.\d+ \(x86_64\)',
                    r'Version {new} (x86_64)'
                ),
            ],
        },
        {
            "path": "installer/installer.nsi",
            "replacements": [
                (
                    r'!define APP_VERSION "\d+\.\d+\.\d+"',
                    r'!define APP_VERSION "{new}"'
                ),
            ],
        },
        {
            "path": "README.md",
            "replacements": [
                (
                    r'https://img\.shields\.io/badge/Release-v\d+\.\d+\.\d+-brightgreen\.svg',
                    r'https://img.shields.io/badge/Release-v{new}-brightgreen.svg'
                ),
                (
                    r'\|\s*v\d+\.\d+\.\d+\s*\|\s*Recent Institutional Audit Activity',
                    r'| v{new}            |  Recent Institutional Audit Activity'
                ),
                (
                    r'([A-Za-z0-9_-]+)-v\d+\.\d+\.\d+-Setup\.exe',
                    r'\1-v{new}-Setup.exe'
                ),
            ],
        },
        {
            "path": "docs/PACKAGING.md",
            "replacements": [
                (
                    r'([A-Za-z0-9_-]+)-v\d+\.\d+\.\d+-windows-x64',
                    r'\1-v{new}-windows-x64'
                ),
                (
                    r'([A-Za-z0-9_-]+)-v\d+\.\d+\.\d+-Setup\.exe',
                    r'\1-v{new}-Setup.exe'
                ),
            ],
        },
        {
            "path": "android/app/build.gradle",
            "replacements": [
                (
                    r'versionName "\d+\.\d+\.\d+"',
                    r'versionName "{new}"'
                ),
            ],
        },
    ]


def bump_file(rel_path, replacements, old_ver, new_ver, dry_run):
    """Apply version replacements to a single file."""
    abs_path = os.path.join(REPO_ROOT, rel_path.replace("/", os.sep))

    if not os.path.exists(abs_path):
        print(f"  [SKIP] {rel_path}  (file not found)")
        return False

    with open(abs_path, "r", encoding="utf-8") as f:
        original = f.read()

    content = original
    for pattern, template in replacements:
        replacement = template.replace("{new}", new_ver).replace("{old}", old_ver)
        new_content = re.sub(pattern, replacement, content)
        if new_content != content:
            content = new_content

    if content == original:
        print(f"  [NO-CHANGE] {rel_path}")
        return False

    if dry_run:
        orig_lines = original.splitlines()
        new_lines = content.splitlines()
        changes = [(i+1, o, n) for i, (o, n) in enumerate(zip(orig_lines, new_lines)) if o != n]
        print(f"  [DRY-RUN] {rel_path}  ({len(changes)} line(s) would change)")
        for lineno, old_line, new_line in changes[:4]:
            print(f"    L{lineno}: - {old_line.strip()}")
            print(f"    L{lineno}: + {new_line.strip()}")
        return True

    with open(abs_path, "w", encoding="utf-8") as f:
        f.write(content)

    print(f"  [UPDATED] {rel_path}")
    return True


def update_changelog(new_ver, message, dry_run):
    """Add new release section and tag row to docs/CHANGELOG.md."""
    abs_path = os.path.join(REPO_ROOT, "docs", "CHANGELOG.md")
    if not os.path.exists(abs_path):
        print("  [WARN] docs/CHANGELOG.md not found, skipping changelog update")
        return False

    with open(abs_path, "r", encoding="utf-8") as f:
        content = f.read()

    if f"v{new_ver}" in content:
        print(f"  [SKIP] docs/CHANGELOG.md already contains v{new_ver}")
        return False

    # Get current commit hash (short)
    _, commit_hash, _ = run("git rev-parse --short HEAD")
    if not commit_hash or len(commit_hash) < 4:
        commit_hash = "HEAD"

    # 1. Add new milestone header under "## Version Milestones"
    milestone_header = "## Version Milestones\n"
    new_milestone = (
        f"\n### v{new_ver} (Current Release)\n"
        f"- {message}\n"
    )
    if milestone_header in content:
        content = content.replace("(Current Release)", "").replace(milestone_header, milestone_header + new_milestone)

    # 2. Add new tag row to the table under "## Annotated Git Tags History"
    new_tag_row = f"| `v{new_ver}` | `{commit_hash}` | {message} |\n"
    table_marker = re.search(r'(\| `v\d+\.\d+\.\d+` \| `[0-9a-f]+` \| .+ \|\n)', content)
    if table_marker:
        # Find the last table row
        last_match = list(re.finditer(r'(\| `v\d+\.\d+\.\d+` \| `[0-9a-f]+` \| .+ \|\n)', content))[-1]
        content = content[:last_match.end()] + new_tag_row + content[last_match.end():]

    if dry_run:
        print(f"  [DRY-RUN] docs/CHANGELOG.md  (would add v{new_ver} section & history tag row)")
        return True

    with open(abs_path, "w", encoding="utf-8") as f:
        f.write(content)

    print(f"  [UPDATED] docs/CHANGELOG.md  (v{new_ver} milestone added)")
    return True


def git_commit_file(rel_path, new_ver):
    """Stage and commit a single file individually (1 commit per file rule)."""
    file_path = rel_path.replace("/", os.sep)
    rc, out, err = run(f'git add "{file_path}"')
    if rc != 0:
        print(f"  [GIT ERROR] git add {file_path}: {err}")
        return False

    rc2, diff_out, _ = run(f'git diff --cached --name-only "{file_path}"')
    if not diff_out.strip():
        print(f"  [NO-STAGE] {rel_path}  (no staged changes)")
        return False

    msg = f"chore: bump version to {new_ver} in {os.path.basename(rel_path)}"
    rc3, _, err3 = run(f'git commit -m "{msg}"')
    if rc3 != 0:
        print(f"  [GIT ERROR] commit failed: {err3}")
        return False

    _, short_hash, _ = run("git rev-parse --short HEAD")
    print(f"  [COMMIT] {short_hash}  {rel_path}")
    return True


def git_tag(new_ver, message):
    """Create an annotated git tag."""
    tag = f"v{new_ver}"
    rc, out, err = run(f'git tag -a "{tag}" -m "{message}"')
    if rc != 0:
        print(f"  [GIT ERROR] tag failed: {err}")
        return False
    print(f"  [TAG] Created annotated tag: {tag}")
    return True


def git_push():
    """Push commits and tags to origin."""
    print("\nPushing commits to origin main...")
    rc1, out1, err1 = run("git push origin main")
    if rc1 != 0 and "error" in err1.lower():
        print(f"  [GIT ERROR] push commits: {err1}")
        return False
    print("  [PUSH] Commits pushed to origin main")

    print("Pushing tags to origin...")
    rc2, out2, err2 = run("git push origin --tags")
    if rc2 != 0 and "error" in err2.lower():
        print(f"  [GIT ERROR] push tags: {err2}")
        return False
    print("  [PUSH] Tags pushed to origin")
    return True


def prompt_user_for_version(old_ver, app_name):
    """Interactive CLI menu when no version argument is passed."""
    patch_v = calculate_bump(old_ver, "patch")
    minor_v = calculate_bump(old_ver, "minor")
    major_v = calculate_bump(old_ver, "major")

    print("\n" + "=" * 54)
    print(f"   {app_name} Version Bump Wizard")
    print(f"   Current Version: v{old_ver}")
    print("=" * 54)
    print("Select version upgrade option:")
    print(f"  1) Patch : v{patch_v}  (Bug fixes, minor tweaks) [DEFAULT]")
    print(f"  2) Minor : v{minor_v}  (New features, UI improvements)")
    print(f"  3) Major : v{major_v}  (Major architectural upgrade)")
    print(f"  4) Custom version string")
    print("=" * 54)

    try:
        choice = input("\nEnter choice [1/2/3/4] or custom version (default 1): ").strip()
    except (EOFError, KeyboardInterrupt):
        print(f"\nDefaulting to patch bump: v{patch_v}")
        return patch_v

    if not choice or choice == "1":
        return patch_v
    elif choice == "2":
        return minor_v
    elif choice == "3":
        return major_v
    elif choice == "4":
        try:
            custom = input("Enter custom version (e.g. 1.2.0): ").strip()
        except (EOFError, KeyboardInterrupt):
            return patch_v
        return custom.lstrip("v")
    else:
        return choice.lstrip("v")


# ---------------------------------------------------------------------------
# Main Routine
# ---------------------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser(
        description="Bump SchoolCMS / APCOACHING version across all project files."
    )
    parser.add_argument(
        "new_version", nargs="?", default=None,
        help="Target version string, e.g. 1.2.0 (optional)"
    )
    parser.add_argument(
        "--patch", action="store_true",
        help="Automatically bump patch version (e.g. 1.1.3 -> 1.1.4)"
    )
    parser.add_argument(
        "--minor", action="store_true",
        help="Automatically bump minor version (e.g. 1.1.3 -> 1.2.0)"
    )
    parser.add_argument(
        "--major", action="store_true",
        help="Automatically bump major version (e.g. 1.1.3 -> 2.0.0)"
    )
    parser.add_argument(
        "--message", "-m", default="",
        help="Release description for git tag and changelog."
    )
    parser.add_argument(
        "--push", action="store_true",
        help="Push commits and tag to origin after updating."
    )
    parser.add_argument(
        "--dry-run", action="store_true",
        help="Preview changes without modifying files or creating git commits."
    )
    args = parser.parse_args()

    app_name, old_ver = detect_app_info()

    # Determine target new version
    if args.new_version:
        new_ver = args.new_version.lstrip("v")
    elif args.patch:
        new_ver = calculate_bump(old_ver, "patch")
    elif args.minor:
        new_ver = calculate_bump(old_ver, "minor")
    elif args.major:
        new_ver = calculate_bump(old_ver, "major")
    else:
        new_ver = prompt_user_for_version(old_ver, app_name)

    validate_semver(new_ver)

    print()
    print("=" * 60)
    print(f"  {app_name} Version Bump")
    print(f"  Current : v{old_ver}")
    print(f"  Target  : v{new_ver}")
    if args.dry_run:
        print("  MODE    : DRY RUN (no files will be written)")
    print("=" * 60)
    print()

    if old_ver == new_ver:
        print(f"[ERROR] Target version ({new_ver}) is identical to current version. Aborting.")
        sys.exit(1)

    # ── Step 1: Update source files ─────────────────────────────────
    print("Step 1: Updating version strings in project files...")
    version_files = get_version_files(app_name)
    changed_files = []

    for file_def in version_files:
        changed = bump_file(
            file_def["path"],
            file_def["replacements"],
            old_ver,
            new_ver,
            args.dry_run
        )
        if changed:
            changed_files.append(file_def["path"])

    print()

    # ── Step 2: Update docs/CHANGELOG.md ────────────────────────────
    print("Step 2: Updating docs/CHANGELOG.md...")
    release_notes = args.message if args.message else f"{app_name} v{new_ver} release"
    cl_changed = update_changelog(new_ver, release_notes, args.dry_run)
    if cl_changed and "docs/CHANGELOG.md" not in changed_files:
        changed_files.append("docs/CHANGELOG.md")

    print()

    if args.dry_run:
        print("=" * 60)
        print("  DRY RUN complete — no files, commits, or tags were modified.")
        print("=" * 60)
        return

    if not changed_files:
        print("[WARN] No files were changed. Version may already be up to date.")
        sys.exit(0)

    # ── Step 3: Commit each file individually (1 commit per file) ───
    print("Step 3: Committing changed files individually (1 commit per file)...")
    for rel_path in changed_files:
        git_commit_file(rel_path, new_ver)

    print()

    # ── Step 4: Create annotated git tag ─────────────────────────────
    print("Step 4: Creating annotated git tag...")
    git_tag(new_ver, release_notes)
    print()

    # ── Step 5: Push (optional) ───────────────────────────────────────
    if args.push:
        print("Step 5: Pushing commits and tags to origin...")
        git_push()
    else:
        print("Step 5: Push skipped (use --push to push automatically)")
        print(f"\n  To push manually run:\n    git push origin main\n    git push origin v{new_ver}")

    print()
    print("=" * 60)
    print(f"  SUCCESS! Version bumped to v{new_ver} successfully.")
    print("=" * 60)


if __name__ == "__main__":
    main()
