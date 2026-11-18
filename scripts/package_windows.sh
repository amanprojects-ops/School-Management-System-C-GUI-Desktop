#!/bin/bash
# ==============================================================================
# Windows Release Packaging Shell Script
# Runs Python packaging utility from MSYS2 environment.
# ==============================================================================

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

echo "Starting Windows Release Packaging..."
cd "$PROJECT_ROOT"

python3 "$SCRIPT_DIR/package_windows.py"

echo "Packaging completed successfully."
