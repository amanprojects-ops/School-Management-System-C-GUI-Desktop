#!/bin/bash
# ==============================================================================
# SchoolCMS Comprehensive Test Runner Script
# Runs all 13 unit and integration test executables with error reporting.
# ==============================================================================

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/build"

cd "$BUILD_DIR"

echo "================================================================"
echo "          Running SchoolCMS Complete Test Suite (13 Targets)    "
echo "================================================================"

TESTS=(
    "test_database.exe"
    "test_auth.exe"
    "test_dashboard.exe"
    "test_student.exe"
    "test_teacher.exe"
    "test_classes.exe"
    "test_attendance.exe"
    "test_fees.exe"
    "test_exams.exe"
    "test_timetable.exe"
    "test_reports.exe"
    "test_settings.exe"
    "test_system_integration.exe"
)

TOTAL=${#TESTS[@]}
PASSED=0
FAILED=0

for t in "${TESTS[@]}"; do
    if [ -f "$t" ]; then
        echo -n "Running $t ... "
        if ./"$t" > /dev/null 2>&1; then
            echo "[PASS]"
            PASSED=$((PASSED + 1))
        else
            echo "[FAILED]"
            FAILED=$((FAILED + 1))
        fi
    else
        echo "Binary not found: $t [FAILED]"
        FAILED=$((FAILED + 1))
    fi
done

echo "================================================================"
echo "Test Execution Summary: $PASSED / $TOTAL Passed (Failed: $FAILED)"
echo "================================================================"

if [ $FAILED -eq 0 ]; then
    echo ">>> ALL 13 TEST SUITES PASSED CLEANLY (100%) <<<"
    exit 0
else
    echo ">>> ONE OR MORE TESTS FAILED <<<"
    exit 1
fi
