#!/usr/bin/env python3
"""
bump_version.py — SchoolCMS Version Bump Script
================================================
Updates the version string in ALL project files, creates an annotated git tag,
and optionally pushes to origin.

Usage:
    python scripts/bump_version.py <new_version> [--message "Release notes"] [--push] [--dry-run]

Examples:
    python scripts/bump_version.py 1.2.0
    python scripts/bump_version.py 1.2.0 --message "ID Card printing improvements and bug fixes"
    python scripts/bump_version.py 1.2.0 --push
    python scripts/bump_version.py 1.2.0 --dry-run

Files updated:
    - CMakeLists.txt          (project version)
    - src/ui/sidebar.c        (sidebar footer label)
    - src/ui/window.c         (status bar label)
    - scripts/package_windows.py (PACKAGE_NAME and version comments)
    - scripts/installer.nsi   (APP_VERSION and folder path)
    - README.md               (badge + history table row)
"""

import sys
import os
import re
import subprocess
import argparse
from datetime import datetime

# ---------------------------------------------------------------------------
# File definitions: each entry has the file path and one or more replacements.
# Each replacement is a (regex_pattern, new_string_template) pair.
# In new_string_template, {new} = new version, {old} = old version.
# ---------------------------------------------------------------------------

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

VERSION_FILES = [
    {
        "path": "CMakeLists.txt",
        "replacements": [
            (
                r'project\(SchoolCMS VERSION \d+\.\d+\.\d+ LANGUAGES C\)',
                "project(SchoolCMS VERSION {new} LANGUAGES C)"
            ),
        ],
    },
    {
        "path": "src/ui/sidebar.c",
        "replacements": [
            (
                r'"SchoolCMS v\d+\.\d+\.\d+"',
                '"SchoolCMS v{new}"'
            ),
        ],
    },
    {
        "path": "src/ui/window.c",
        "replacements": [
            (
                r'"SchoolCMS v\d+\.\d+\.\d+  •  Authenticated"',
                '"SchoolCMS v{new}  \u2022  Authenticated"'
            ),
        ],
    },
    {
        "path": "scripts/package_windows.py",
        "replacements": [
            (
                r'PACKAGE_NAME = "SchoolCMS-v\d+\.\d+\.\d+-windows-x64"',
                'PACKAGE_NAME = "SchoolCMS-v{new}-windows-x64"'
            ),
            (
                r'SchoolCMS-v\d+\.\d+\.\d+-windows-x64\b(?!\")',
                "SchoolCMS-v{new}-windows-x64"
            ),
            (
                r'Version \d+\.\d+\.\d+ \(x86_64\)',
                "Version {new} (x86_64)"
            ),
        ],
    },
    {
        "path": "scripts/installer.nsi",
        "replacements": [
            (
                r'!define APP_VERSION "\d+\.\d+\.\d+"',
                '!define APP_VERSION "{new}"'
            ),
            (
                r'SchoolCMS-v\d+\.\d+\.\d+-windows-x64',
                "SchoolCMS-v{new}-windows-x64"
            ),
        ],
    },
    {
        "path": "README.md",
        "replacements": [
            (
                r'https://img\.shields\.io/badge/Release-v\d+\.\d+\.\d+-brightgreen\.svg',
                "https://img.shields.io/badge/Release-v{new}-brightgreen.svg"
            ),
        ],
    },
]


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def run(cmd, cwd=REPO_ROOT, capture=True):
    """Run a shell command and return (returncode, stdout, stderr)."""
    result = subprocess.run(
        cmd, shell=True, cwd=cwd,
        capture_output=capture, text=True
    )
    return result.returncode, result.stdout.strip(), result.stderr.strip()


def detect_current_version():
    """Read the current version from CMakeLists.txt."""
    path = os.path.join(REPO_ROOT, "CMakeLists.txt")
    with open(path, "r", encoding="utf-8") as f:
        content = f.read()
    m = re.search(r'project\(SchoolCMS VERSION (\d+\.\d+\.\d+)', content)
    if m:
        return m.group(1)
    return "unknown"


def validate_semver(version):
    """Ensure version matches MAJOR.MINOR.PATCH."""
    if not re.fullmatch(r'\d+\.\d+\.\d+', version):
        print(f"[ERROR] Version must be MAJOR.MINOR.PATCH format (e.g. 1.2.0). Got: {version}")
        sys.exit(1)


def bump_file(rel_path, old_ver, new_ver, dry_run):
    """Apply all version replacements to a single file."""
    abs_path = os.path.join(REPO_ROOT, rel_path.replace("/", os.sep))

    if not os.path.exists(abs_path):
        print(f"  [SKIP] {rel_path}  (file not found)")
        return False

    with open(abs_path, "r", encoding="utf-8") as f:
        original = f.read()

    content = original
    for file_def in VERSION_FILES:
        if file_def["path"] == rel_path:
            for pattern, template in file_def["replacements"]:
                replacement = template.replace("{new}", new_ver).replace("{old}", old_ver)
                new_content = re.sub(pattern, replacement, content)
                if new_content != content:
                    content = new_content

    if content == original:
        print(f"  [NO-CHANGE] {rel_path}")
        return False

    if dry_run:
        # Show a brief diff
        orig_lines = original.splitlines()
        new_lines = content.splitlines()
        changes = [(i+1, o, n) for i, (o, n) in enumerate(zip(orig_lines, new_lines)) if o != n]
        print(f"  [DRY-RUN] {rel_path}  ({len(changes)} line(s) would change)")
        for lineno, old_line, new_line in changes[:5]:
            print(f"    L{lineno}: - {old_line.strip()}")
            print(f"    L{lineno}: + {new_line.strip()}")
        return True

    with open(abs_path, "w", encoding="utf-8") as f:
        f.write(content)

    print(f"  [UPDATED] {rel_path}")
    return True


def append_readme_history(new_ver, message, dry_run):
    """Append a new row to the Release Tags & Version History table in README.md."""
    abs_path = os.path.join(REPO_ROOT, "README.md")
    with open(abs_path, "r", encoding="utf-8") as f:
        content = f.read()

    # Get current HEAD commit hash (short)
    _, commit_hash, _ = run("git rev-parse --short HEAD")

    new_row = f"| `v{new_ver}` | `{commit_hash}` | {message} |"

    # Find the last table row before the closing --- and insert after it
    # Pattern: last line matching the tag table format before "---"
    last_row_pattern = re.compile(
        r'(\| `v\d+\.\d+\.\d+` \| `[0-9a-f]+` \| .+ \|\n)',
        re.MULTILINE
    )
    matches = list(last_row_pattern.finditer(content))
    if not matches:
        print("  [WARN] Could not locate version history table in README.md")
        return

    last_match = matches[-1]
    # Don't duplicate if already in there
    if f"v{new_ver}" in content:
        print(f"  [SKIP] README history table already has v{new_ver}")
        return

    new_content = (
        content[:last_match.end()]
        + new_row + "\n"
        + content[last_match.end():]
    )

    if dry_run:
        print(f"  [DRY-RUN] README.md  (would append history row: {new_row})")
        return

    with open(abs_path, "w", encoding="utf-8") as f:
        f.write(new_content)
    print(f"  [UPDATED] README.md  (history row appended)")


def git_commit_file(rel_path, new_ver):
    """Stage and commit a single file (1 commit per file rule)."""
    file_path = rel_path.replace("/", os.sep)
    rc, out, err = run(f'git add "{file_path}"')
    if rc != 0:
        print(f"  [GIT ERROR] git add {file_path}: {err}")
        return False

    # Check if there's actually something staged
    rc2, diff_out, _ = run(f'git diff --cached --name-only "{file_path}"')
    if not diff_out.strip():
        print(f"  [NO-STAGE] {rel_path}  (nothing to commit)")
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
    full_msg = message if message else f"SchoolCMS {tag} release"
    rc, out, err = run(f'git tag -a "{tag}" -m "{full_msg}"')
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
    print("  [PUSH] Commits pushed")

    print("Pushing tags to origin...")
    rc2, out2, err2 = run("git push origin --tags")
    if rc2 != 0 and "error" in err2.lower():
        print(f"  [GIT ERROR] push tags: {err2}")
        return False
    print("  [PUSH] Tags pushed")
    return True


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

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


def prompt_user_for_version(old_ver):
    """Interactive CLI menu when no version argument is passed."""
    patch_v = calculate_bump(old_ver, "patch")
    minor_v = calculate_bump(old_ver, "minor")
    major_v = calculate_bump(old_ver, "major")

    print("\n" + "=" * 50)
    print(f"  Current SchoolCMS Version: v{old_ver}")
    print("=" * 50)
    print("Select version upgrade option:")
    print(f"  1) Patch: v{patch_v}  (Bug fixes, minor tweaks)")
    print(f"  2) Minor: v{minor_v}  (New features, UI improvements)")
    print(f"  3) Major: v{major_v}  (Breaking changes, major release)")
    print("  4) Custom version string")
    print("=" * 50)

    try:
        choice = input("\nEnter choice [1/2/3/4] or custom version (default 1): ").strip()
    except (EOFError, KeyboardInterrupt):
        print("\nAborted.")
        sys.exit(0)

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
            print("\nAborted.")
            sys.exit(0)
        return custom.lstrip("v")
    else:
        # User might have typed the version directly (e.g. "1.2.0")
        return choice.lstrip("v")


def main():
    parser = argparse.ArgumentParser(
        description="Bump SchoolCMS version across all project files."
    )
    parser.add_argument(
        "new_version", nargs="?", default=None,
        help="New version string, e.g. 1.2.0 (optional if --patch/--minor/--major is used)"
    )
    parser.add_argument(
        "--patch", action="store_true",
        help="Automatically bump patch version (e.g. 1.1.0 -> 1.1.1)"
    )
    parser.add_argument(
        "--minor", action="store_true",
        help="Automatically bump minor version (e.g. 1.1.0 -> 1.2.0)"
    )
    parser.add_argument(
        "--major", action="store_true",
        help="Automatically bump major version (e.g. 1.1.0 -> 2.0.0)"
    )
    parser.add_argument(
        "--message", "-m", default="",
        help="Release description for git tag annotation and README history table."
    )
    parser.add_argument(
        "--push", action="store_true",
        help="Push commits and tag to origin after updating."
    )
    parser.add_argument(
        "--dry-run", action="store_true",
        help="Show what would change without writing any files or creating commits/tags."
    )
    args = parser.parse_args()

    old_ver = detect_current_version()

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
        # Check if stdin is interactive
        if sys.stdin.isatty():
            new_ver = prompt_user_for_version(old_ver)
        else:
            print(f"[ERROR] Version argument missing. Current version is v{old_ver}")
            print("\nUsage examples:")
            print("  python scripts/bump_version.py 1.2.0")
            print("  python scripts/bump_version.py --patch")
            print("  python scripts/bump_version.py --minor")
            print("  python scripts/bump_version.py --major")
            print("  python scripts/bump_version.py 1.2.0 --dry-run")
            sys.exit(1)

    validate_semver(new_ver)

    print("=" * 60)
    print(f"  SchoolCMS Version Bump")
    print(f"  {old_ver}  -->  {new_ver}")
    if args.dry_run:
        print("  MODE: DRY RUN (no files will be written)")
    print("=" * 60)
    print()

    if old_ver == new_ver:
        print(f"[ERROR] New version ({new_ver}) is the same as the current version. Aborting.")
        sys.exit(1)

    # ── Step 1: Update all version files ────────────────────────────
    print("Step 1: Updating version strings in source files...")
    changed_files = []
    for file_def in VERSION_FILES:
        changed = bump_file(file_def["path"], old_ver, new_ver, args.dry_run)
        if changed:
            changed_files.append(file_def["path"])

    print()

    # ── Step 2: Append README version history row ────────────────────
    print("Step 2: Appending version history row to README.md...")
    history_message = args.message if args.message else f"SchoolCMS v{new_ver} release"
    append_readme_history(new_ver, history_message, args.dry_run)
    if "README.md" not in changed_files:
        changed_files.append("README.md")
    print()

    if args.dry_run:
        print("=" * 60)
        print("  DRY RUN complete — no files, commits, or tags were modified.")
        print("=" * 60)
        return

    if not changed_files:
        print("[WARN] No files were changed. Is the version already up to date?")
        sys.exit(0)

    # ── Step 3: Commit each file (1 commit per file) ──────────────────
    print("Step 3: Committing each file individually...")
    for rel_path in changed_files:
        git_commit_file(rel_path, new_ver)
    print()

    # ── Step 4: Create annotated git tag ─────────────────────────────
    print("Step 4: Creating annotated git tag...")
    tag_message = args.message if args.message else f"SchoolCMS v{new_ver} release"
    git_tag(new_ver, tag_message)
    print()

    # ── Step 5: Push (optional) ───────────────────────────────────────
    if args.push:
        print("Step 5: Pushing to origin...")
        git_push()
    else:
        print("Step 5: (Skipped — run with --push to push commits and tag)")
        print(f"\n  To push manually:\n    git push origin main\n    git push origin v{new_ver}")

    print()
    print("=" * 60)
    print(f"  Version bumped to v{new_ver} successfully!")
    print("=" * 60)


if __name__ == "__main__":
    main()
