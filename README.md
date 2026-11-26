# APCOACHING — Enterprise Desktop School Management System

[![Language](https://img.shields.io/badge/Language-C11-00599C.svg?logo=c)](https://en.wikipedia.org/wiki/C11_(C_standard_revision))
[![GUI Framework](https://img.shields.io/badge/GUI-GTK4-4A90E2.svg?logo=gnome)](https://www.gtk.org/)
[![Database](https://img.shields.io/badge/Database-SQLite3-003B57.svg?logo=sqlite)](https://www.sqlite.org/)
[![Platform](https://img.shields.io/badge/Platform-Windows%2010%20%7C%2011-0078D6.svg?logo=windows)](https://microsoft.com/windows)
[![Build](https://img.shields.io/badge/Build-CMake%20%7C%20Ninja-064F8C.svg?logo=cmake)](https://cmake.org/)
[![Test Suite](https://img.shields.io/badge/Tests-13%2F13%20Passed%20(100%25)-2ea44f.svg)](run_tests.sh)
[![Release](https://img.shields.io/badge/Release-v1.2.0-brightgreen.svg)](dist/)

A modern, high-performance, offline-first institutional **School Management System** desktop application developed in pure **C (C11)**, **GTK4**, and **SQLite3** for Windows 10 and 11. Designed following enterprise SaaS visual standards, featuring real-time analytics, automated student and faculty rostering, conflict-free timetable scheduling, comprehensive fee accounting, examination grading, RFC 4180 CSV reporting, and online SQLite disaster recovery.

---

## Documentation Library

To keep the project clean, comprehensive, and easy to navigate, detailed documentation is organized into modular guides:

| Guide | Description |
| :--- | :--- |
| 📖 **[BUILDING.md](docs/BUILDING.md)** | Prerequisites, MSYS2 UCRT64 toolchain setup, CMake & Ninja compilation guide |
| 🧪 **[TESTING.md](docs/TESTING.md)** | Automated test runner, 13 test targets overview (100% pass rate), and test debugging |
| 📦 **[PACKAGING.md](docs/PACKAGING.md)** | Portable zero-dependency release bundle (`.zip`) & Windows Setup Wizard (`.exe`) |
| 🏗️ **[ARCHITECTURE.md](docs/ARCHITECTURE.md)** | Full project directory tree, C11 layered architecture, and 19-table database schema with ER diagram |
| 🎨 **[REBRANDING.md](docs/REBRANDING.md)** | Single-click rebranding via `rebrand.bat`, changing app name, Windows icon, and custom PNG logo |
| 🔧 **[TROUBLESHOOTING.md](docs/TROUBLESHOOTING.md)** | Frequently asked questions, DLL errors, icon rendering fixes, and Excel CSV encoding |
| 📜 **[CHANGELOG.md](docs/CHANGELOG.md)** | Version release notes and complete history of all 34 milestone Git tags |

---

## Quick Start Guide

### 1. Build the Application
Ensure you have the [MSYS2 UCRT64 toolchain installed](docs/BUILDING.md), then run:
```bash
cd C:/apps/SchoolCMS
mkdir -p build && cd build
cmake -G Ninja -DCMAKE_BUILD_TYPE=Release ..
ninja
```

### 2. Run All Tests
```bash
cd C:/apps/SchoolCMS
bash run_tests.sh
```

### 3. Launch the Application
```powershell
.\build\APCOACHING.exe
```

### 4. Create Windows Setup Installer Wizard (.exe)
Double-click `make_installer.bat` or run:
```bash
python scripts/build_installer.py
```
Outputs standalone installer: `dist/APCOACHING-v1.2.0-Setup.exe` (~19.5 MB).

---

## Default Administrator Credentials

On first launch, SQLite automatically runs all schema migrations and creates default admin credentials:

| Field | Value |
| :--- | :--- |
| **Username** | `admin` |
| **Password** | `Password123!` |
| **Role** | `ADMINISTRATOR` |

> [!NOTE]
> All passwords use **PBKDF2-HMAC-SHA256** with 10,000 iterations and a unique salt. The database file `schoolcms.db` is stored locally with Write-Ahead Logging (`WAL`) mode enabled.

---

## Core Capabilities & Features

```
+-----------------------------------------------------------------------------------------+
|                                    SchoolCMS Desktop Shell                              |
+-------------------+---------------------------------------------------------------------+
| [Logo] APCOACHING |  [ Search... ]           [ Notifications (0) ]   [ Admin (ADMINISTRATOR) ] |
+-------------------+---------------------------------------------------------------------+
| [o] Dashboard     |  +----------------+ +----------------+ +----------------+ +-----------+ |
| [o] Students      |  | Total Students | | Total Teachers | | Total Staff    | | Classes   | |
| [o] Teachers      |  | 248            | | 18             | | 12             | | 8         | |
| [o] Classes       |  +----------------+ +----------------+ +----------------+ +-----------+ |
| [o] Attendance    |                                                                         |
| [o] Fees          |  +---------------------------+  +-------------------------------------+ |
| [o] Exams         |  | Weekly Attendance (Cairo) |  | 6-Month Fee Revenue Trend (Cairo)   | |
| [o] Timetable     |  | [|||||||] Mon-Sat Avg 92% |  | [---/---] Monthly INR Inflow        | |
| [o] Reports       |  +---------------------------+  +-------------------------------------+ |
| [o] Settings      |                                                                         |
|                   |  Quick Actions: [+ Student]  [Collect Fee]  [Mark Attendance]  [Report] |
| v1.2.0            |  Recent Institutional Audit Activity (Live SQLite WAL Stream)           |
+-------------------+---------------------------------------------------------------------+
```

- **Modern SaaS UI:** Responsive layout with auto-screen adaptation, dark/light theme, and Cairo 2D charts.
- **Student Lifecycle:** Auto admission codes (`ADM-YYYY-XXXX`), class rostering, profile cards, and bulk ID card printing.
- **Faculty & Staff:** Teacher and non-teaching staff directories with automated payroll records and search.
- **Academic Structure:** 12 classes with dynamic section mappings and real-time capacity enforcement.
- **Attendance Registers:** Daily classroom marking with one-click toggles and monthly aggregated registers.
- **Institutional Fees:** Fee structures, receipt numbering (`RCP-YYYY-XXXX`), multi-mode payments (Cash, UPI, Cheque), and dues ledger.
- **Examinations & Grading:** Standard automated grading scale (A+ to F), marks entry sheets, and printable report cards.
- **Timetable Scheduling:** Weekly period matrix with real-time double-booking prevention for teachers and classrooms.
- **Reports & RFC 4180 CSV:** 6 standard institutional reports with UTF-8 BOM encoding for native Microsoft Excel compatibility.
- **Security & Backups:** PBKDF2 authentication, 5-attempt brute-force lockout, and live hot SQLite online snapshots with one-click restore.

---

## Technology Stack

| Layer | Component | Specification |
| :--- | :--- | :--- |
| **Language** | Core Logic | Pure C (ISO/IEC 9899:2011 - C11) — strictly **no C++** |
| **GUI Framework** | Window & Widgets | GTK4 (`libgtk-4-1.dll`, v4.14+) |
| **Vector Engine** | Charts & 2D Graphics | Cairo (`libcairo-2.dll`) |
| **Font & Text** | Text Layout | Pango (`libpango-1.0-0.dll`) |
| **Database** | Storage Engine | SQLite 3 (`libsqlite3-0.dll`) with WAL mode |
| **Cryptography** | Hashing & Randomness | OpenSSL libcrypto (`libcrypto-4-x64.dll`, PBKDF2-HMAC-SHA256) |
| **Build System** | Metabuild & Compiler | CMake 3.20+ with Ninja & MinGW-w64 GCC (UCRT64) |
| **Packaging** | Bundling & Installer | Python PE Dependency Scanner + NSIS Modern UI 2 |
| **Target OS** | Deployment Platform | Windows 10 / Windows 11 (64-bit x86_64) |

---

## License

Proprietary — All rights reserved. Designed and developed for institutional desktop deployment.
