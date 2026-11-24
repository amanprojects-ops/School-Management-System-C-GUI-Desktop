# Automated Testing & Verification Guide — SchoolCMS

SchoolCMS includes 13 comprehensive unit and integration test executables verifying every backend service, SQLite database migration, grade computation, timetable collision check, and CSV export routine.

---

## 1. Running All Tests via Test Runner Script

### In MSYS2 UCRT64 or Git Bash:
```bash
cd C:/apps/SchoolCMS
bash run_tests.sh
```

### In Windows PowerShell:
```powershell
& "C:\msys64\usr\bin\bash.exe" -c 'export PATH="/c/msys64/ucrt64/bin:/usr/bin:$PATH" && cd /c/apps/SchoolCMS && ./run_tests.sh'
```

---

## 2. Test Suite Execution Summary (100% Pass Rate)

| Test Executable | Target Module | Scope of Verification | Status |
| :--- | :--- | :--- | :--- |
| `test_database.exe` | Database Core | 19 tables, indexes, WAL journal mode, seed data, online backup handle | **PASS** |
| `test_auth.exe` | Authentication | PBKDF2 hashing, salt generation, 5-attempt lockout, sessions | **PASS** |
| `test_dashboard.exe` | Dashboard | Real-time SQL metric aggregations, attendance rates, fee sums | **PASS** |
| `test_student.exe` | Students | Student CRUD, `ADM-YYYY-XXXX` generator, filtering, pagination | **PASS** |
| `test_teacher.exe` | Faculty & Staff | `TCH-` & `STF-` code generation, department filters, staff CRUD | **PASS** |
| `test_classes.exe` | Classes & Sections | Class creation, section limits, capacity overflow prevention | **PASS** |
| `test_attendance.exe` | Attendance | Daily roster marking, bulk transactions, monthly percentages | **PASS** |
| `test_fees.exe` | Fee Management | Fee structures, duplicate payment guard, receipts, dues ledger | **PASS** |
| `test_exams.exe` | Examinations | Exam schedules, marks entry, automatic grading (A+..F), ranks | **PASS** |
| `test_timetable.exe` | Timetables | Schedule slots, teacher collision prevention, room conflict check | **PASS** |
| `test_reports.exe` | Reporting | 6 report generators, RFC 4180 CSV escaping, UTF-8 BOM headers | **PASS** |
| `test_settings.exe` | System Settings | School profile mutations, online snapshot backup & restore | **PASS** |
| `test_system_integration.exe` | End-to-End | 8 multi-module workflows connecting students, fees, exams, timetable | **PASS** |

---

## 3. Running Individual Test Executables

You can run any individual test target directly from the `build/` directory for detailed debugging:

```bash
cd C:/apps/SchoolCMS/build

# Run database tests
./test_database.exe

# Run authentication and PBKDF2 tests
./test_auth.exe

# Run end-to-end integration tests
./test_system_integration.exe
```

Every test executable returns exit code `0` on success and non-zero on assertion failure, printing diagnostic failure messages to stderr.
