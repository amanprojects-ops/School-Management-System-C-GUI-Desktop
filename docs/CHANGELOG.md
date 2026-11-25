# Release Notes & Version History — SchoolCMS

This document tracks all version releases, milestone tags, and feature additions across the development history of SchoolCMS.

---

## Version Milestones

### v1.1.5 (Current Release)
- APCOACHING v1.1.5 release

### v1.1.4 
- APCOACHING v1.1.4 release

### v1.1.3 
- **Windows Setup Installer Wizard:** Built-in Modern UI 2 NSIS setup wizard (`make_installer.bat` and `scripts/build_installer.py`) producing a single 19.5 MB standalone `Setup.exe`.
- **Dynamic Display Monitor Auto-Adjust:** Integrated GDK4 monitor geometry detection; auto-calculates optimal baseline window dimensions (88% width, 82% height) with safety margins for the Windows taskbar and titlebar.
- **Half-Screen Snap & Custom Resizing:** Reduced `WINDOW_MIN_WIDTH` to 640px and `WINDOW_MIN_HEIGHT` to 450px; enabled non-homogeneous stacks (`root_stack` & `content_stack`), ellipsizing labels, and scrollable containers across all views.
- **PE Resource Embedding:** Linked `src/app.rc` with Windows PE resource compiler (`windres`) to embed high-res application icon directly into the `.exe`.

### v1.1.2
- **Settings View Stability:** Resolved `GtkScrolledWindow` collapsing to 0 height by setting `vexpand`, `hexpand`, and minimum content height.
- **Automatic Navigation Refresh:** Switching to the Settings tab automatically reloads profile data and scans database backup files.

### v1.1.1
- **Educational Icon Branding:** Upgraded default symbolic icon to `applications-education-symbolic`.
- **Operating System Taskbar Integration:** `gtk_window_set_icon_name()` sets the application icon for the Windows taskbar, title bar, and Alt+Tab switcher.

### v1.1.0
- **Point-and-Click Backup Inventory:** Live scanner for `backups/` directory with one-click restore and deletion modals.
- **Cascading Dropdown Selectors:** Replaced manual integer ID entry with dynamic class and section dropdown pickers (`GtkDropDown`).
- **Student ID Card Generation:** Single student ID card view and bulk classroom print grid with photo avatar and institutional header.
- **Responsive Viewport Adaptability:** Bidirectional auto-scrolling across modules and centered login viewport.

### v1.0.0 (Production Release)
- **Zero-Dependency Release Packager:** Portable release bundler copying `APCOACHING.exe`, all 65 DLLs, Adwaita icon theme, schemas, and assets into `dist/`.
- **Modern Enterprise SaaS UI:** Cairo vector charts, dark/light theme toggle, elevation cards, and hover states.
- **10 Core Academic Modules:** Dashboard, Students, Teachers, Classes, Attendance, Fees, Exams, Timetable, Reports, and Settings.
- **13 Test Suites:** 100% unit and integration test pass rate.

---

## Annotated Git Tags History

This repository maintains 34 annotated Git tags tracking each milestone from initial shell to production release:

| Tag Name | Target Commit | Milestone Description |
| :--- | :--- | :--- |
| `phase-01-app-shell` | `a8b1e29` | Phase 1: GTK4 shell, main window, sidebar, and dashboard widgets |
| `v0.1.0` | `a8b1e29` | SchoolCMS v0.1.0 - Application Shell & Navigation |
| `phase-02-database` | `6c15a45` | Phase 2: Database migration system (19 tables, indexes, WAL mode) |
| `v0.2.0` | `6c15a45` | SchoolCMS v0.2.0 - SQLite3 Migrations & Schema |
| `phase-03-auth-login` | `071b5a2` | Phase 3: Login view, PBKDF2 hashing, lockout protection |
| `v0.3.0` | `071b5a2` | SchoolCMS v0.3.0 - Authentication & Session Lockout |
| `phase-04-dashboard-charts`| `143bd7b` | Phase 4: Main dashboard with live SQL aggregations & Cairo charts |
| `v0.4.0` | `143bd7b` | SchoolCMS v0.4.0 - Live Dashboard & Cairo Charting |
| `phase-05-students-crud` | `eca730a` | Phase 5: Student CRUD, `ADM-` code generator, pagination |
| `v0.5.0` | `eca730a` | SchoolCMS v0.5.0 - Student Lifecycle Management |
| `phase-06-teachers-staff` | `2de2b12` | Phase 6: Faculty directory, `TCH-`/`STF-` code generation |
| `v0.6.0` | `2de2b12` | SchoolCMS v0.6.0 - Faculty & Staff Management |
| `phase-07-classes-sections`| `c9abcef` | Phase 7: Class & section management with capacity limits |
| `v0.7.0` | `c9abcef` | SchoolCMS v0.7.0 - Academic Structure & Rostering |
| `phase-08-attendance-system`| `b654cfe` | Phase 8: Daily marking, bulk transactions, monthly registers |
| `v0.8.0` | `b654cfe` | SchoolCMS v0.8.0 - Attendance Marking & Registers |
| `phase-09-fee-management` | `fa1d9f4` | Phase 9: Fee structures, payments, receipts, dues ledger |
| `v0.9.0` | `fa1d9f4` | SchoolCMS v0.9.0 - Institutional Fee Accounting |
| `phase-10-exams-grading` | `fc89b7e` | Phase 10: Exam schedules, marks entry, auto-grading, merit ranks |
| `v0.10.0` | `fc89b7e` | SchoolCMS v0.10.0 - Examinations & Results Processing |
| `phase-11-timetable-grid` | `d71550e` | Phase 11: Weekly timetable grid & collision detection engine |
| `v0.11.0` | `d71550e` | SchoolCMS v0.11.0 - Timetable Scheduling & Conflict Prevention |
| `phase-12-reports-csv-export`| `46bb86a`| Phase 12: 6 institutional reports & RFC 4180 CSV export |
| `v0.12.0` | `46bb86a` | SchoolCMS v0.12.0 - Reporting & Data Export Engine |
| `phase-13-settings-backup` | `a438ba8` | Phase 13: School profile, theme switch, online SQLite backup |
| `v0.13.0` | `a438ba8` | SchoolCMS v0.13.0 - Settings, Theme & Disaster Recovery |
| `phase-14-verification-tests`| `82d8297`| Phase 14: 13 test suites, integration tests, 100% pass rate |
| `v0.14.0` | `82d8297` | SchoolCMS v0.14.0 - Comprehensive Verification & 100% Test Coverage |
| `phase-15-windows-packaging`| `1ccb083` | Phase 15: Windows standalone EXE packaging & NSIS installer |
| `v1.0.0-rc1` | `1ccb083` | SchoolCMS v1.0.0-rc1 - Release Candidate with Portable Packaging |
| `v1.0.0-ui-overhaul` | `63842d0` | Modern enterprise SaaS UI/UX overhaul & styling |
| `v1.0.0` | `d63ee38` | Production release with full vector icon fix & 65 runtime DLLs |
| `v1.0.1` | `b808c96` | Comprehensive installation, compilation & packaging documentation |
| `v1.1.0` | `5936e61` | Database Backup Inventory, Dynamic Dropdowns, Student ID Cards & Responsive Auto-Resize |
| `v1.1.1` | `3ad678b` | Application education icon branding, settings form placeholder text, OS taskbar icon |
| `v1.1.2` | `e81c5ad` | Settings view scrolled window expansion & bidirectional auto-sizing, navigation reload fix |
| `v1.1.3` | `6d79ce0` | Windows PE icon resource embedding, dynamic monitor auto-adjust, and Setup installer wizard |
| `v1.1.4` | `b709ecf` | APCOACHING v1.1.4 release |
| `v1.1.5` | `3dc5204` | APCOACHING v1.1.5 release |
