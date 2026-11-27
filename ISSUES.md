# SchoolCMS — Issue Tracking & Engineering Log

This document tracks all architectural, functional, and UI/UX issues identified, their root causes, implementation specifications, and resolution status.

---

## [ISSUE-001] Database Backup Snapshot Inventory & Point-and-Click Restore
- **Status:** `RESOLVED`
- **Module:** Institutional Configuration (`settings`)
- **Severity:** High
- **Files Modified:** `include/settings.h`, `src/modules/settings.c`, `src/ui/settings_view.c`, `tests/test_settings.c`
- **Commits:** `9946d28`, `82c74b0`, `8780fb2`, `13677c1`

### Problem Statement
1. The backup management section only had a trigger button to create a backup file, but did not display any list of existing database backups.
2. The user had no visibility into how many backup snapshots existed, their filenames, creation timestamps, or disk footprints.
3. Restoring an existing backup snapshot required manual shell commands and file replacement, posing a high risk of database corruption.
4. There was no capability to delete obsolete backup files from within the application interface.

### Root Cause
- The `settings` module lacked directory scanning and file metadata inspection capabilities for the `backups/` directory.
- The UI in `settings_view.c` lacked an inventory table and action rows for restore/delete operations.

### Resolution
1. Defined `BackupInfo` structure with filename, full path, formatted timestamp, and file size.
2. Implemented `settings_service_list_backups()` using standard POSIX/MinGW directory inspection (`<dirent.h>` and `<sys/stat.h>`), sorting snapshots newest first.
3. Implemented safe file deletion in `settings_service_delete_backup()` with directory traversal prevention.
4. Enhanced `settings_service_create_backup()` to auto-create missing parent directories.
5. Built a live inventory table in `settings_view.c` with count/size badges, point-and-click "Restore" (with confirmation modal), "Delete" (with confirmation modal), and auto-refresh.
6. Added automated unit tests in `tests/test_settings.c`.

---

## [ISSUE-002] Student Enrollment Class & Section Numeric Inputs Replaced with Dropdowns
- **Status:** `RESOLVED`
- **Module:** Student Management (`student`, `classes`)
- **Severity:** Medium
- **Files Modified:** `src/ui/student_view.c`
- **Commit:** `901180a`

### Problem Statement
In the Student Add and Edit modal dialogs, assigning a student to a class and section required users to manually type arbitrary integer IDs (`class_id` and `section_id` text entries). Users had no way of knowing which numeric ID corresponded to which class or section, leading to database foreign key errors and orphan records.

### Root Cause
- `show_add_edit_dialog` created standard `GtkEntry` widgets for `class_id` and `section_id` instead of querying the class and section services.

### Resolution
1. Replaced numeric text entries with interactive `GtkDropDown` widgets.
2. Dynamically loaded all active classes using `class_service_get_all()`.
3. Implemented dynamic dependent section loading: selecting a class triggers `update_sections_for_selected_class()`, which queries `section_service_get_by_class()` and repopulates the section dropdown.
4. Implemented pre-selection logic in edit mode so that opening an existing student's record automatically selects their current class and section.

---

## [ISSUE-003] Student Identity Card Generation System (Single & Bulk)
- **Status:** `RESOLVED`
- **Module:** Student Management (`student`, `ui/student_view`, `assets/styles/main.css`)
- **Severity:** High (New Core Feature)
- **Files Modified:** `include/student.h`, `src/modules/student.c`, `assets/styles/main.css`, `src/ui/student_view.c`, `tests/test_student.c`
- **Commits:** `8253b10`, `63a2046`, `226c468`, `901180a`, `fbeab0f`

### Problem Statement
School administrators had no way to generate, preview, or print student identity cards. School administration workflows require both printing individual cards (e.g. upon new enrollment or replacement) and bulk card generation (e.g. for an entire class or session).

### Root Cause
- Absence of student filtering query by class/section/status for card generation.
- Absence of ID card GTK widget layout and CSS styling rules.
- Absence of modal dialogs for single ID card preview and bulk batch generation.

### Resolution
1. Declared and implemented `student_service_get_for_id_cards(class_id, section_id, status, ...)` with flexible multi-parameter filtering.
2. Styled `.id-card-frame`, `.id-card-header`, `.id-card-ribbon`, and `.id-card-footer` in `assets/styles/main.css` for a professional, physical badge appearance.
3. Created `create_id_card_widget()` assembling institutional header, session, avatar photo frame, blood group badge, student details, emergency contact, barcode mockup, and principal signature block.
4. Created `show_id_card_dialog()` accessible from table rows and the student profile modal with a 1-click "Print ID Card" action.
5. Created `show_bulk_id_cards_dialog()` with class, section, and status filters, batch preview layout (`GtkFlowBox`), and "Print Batch" action.
6. Added unit test `test_student_id_cards_query()` in `tests/test_student.c`.

---

## [ISSUE-004] Application Window Auto-Adjust & Custom Responsive Resizing
- **Status:** `RESOLVED`
- **Module:** UI Window Shell (`window`, `login`, and all UI view modules)
- **Severity:** High
- **Files Modified:**
  - `include/ui/window.h`
  - `src/ui/window.c`
  - `src/ui/login.c`
  - `src/ui/dashboard.c`
  - `src/ui/student_view.c`
  - `src/ui/teacher_view.c`
  - `src/ui/classes_view.c`
  - `src/ui/attendance_view.c`
  - `src/ui/fees_view.c`
  - `src/ui/exams_view.c`
  - `src/ui/timetable_view.c`
  - `src/ui/settings_view.c`

### Problem Statement
1. **Auto-Adjust Failure**: On launch, the application window opened at a fixed 1366x768 size without adapting to the user's monitor resolution. On smaller displays (1280x720, or 1366x768 with Windows DPI scaling), the window overflowed beyond the screen edges. On larger monitors (1080p, 1440p), it opened as an unmaximized floating box.
2. **Custom Resize Lock**: Users could not freely resize the window to custom dimensions (e.g. 960x600, half-screen split snapping). The window would lock or refuse to shrink.
3. **Login View Viewport Clipping**: On resizing, the login background gradient did not expand to fill the entire window viewport.

### Root Cause
1. In `include/ui/window.h`, `WINDOW_MIN_WIDTH` was set to a rigid `1100` and `WINDOW_MIN_HEIGHT` to `700`, which blocked Windows Snap Assist (960px width on 1080p) and smaller resolutions.
2. `src/ui/window.c` did not maximize on startup and did not adjust to screen display geometry.
3. In all main UI views (`student_view.c`, `teacher_view.c`, `classes_view.c`, `attendance_view.c`, `fees_view.c`, `exams_view.c`, `timetable_view.c`, `settings_view.c`, `dashboard.c`), the main `GtkScrolledWindow` had `GTK_POLICY_NEVER` for horizontal scrolling. In GTK4, `GTK_POLICY_NEVER` enforces the child's minimum width directly onto the parent window, completely preventing the user from resizing the window smaller than the widest table.
4. In `login.c`, the root container was set to `GTK_ALIGN_CENTER` instead of filling the viewport (`GTK_ALIGN_FILL`).

### Resolution
1. Lowered `WINDOW_MIN_WIDTH` to `800` and `WINDOW_MIN_HEIGHT` to `500` in `include/ui/window.h`. Set default dimensions to `1280x720`.
2. In `src/ui/window.c`, explicitly enabled `gtk_window_set_resizable(window, TRUE)` and added `gtk_window_maximize(window)` to provide auto-adjusted full-screen workspace across any monitor resolution.
3. In `src/ui/login.c`, set root container to `GTK_ALIGN_FILL` so the gradient fills the entire window, with the login card centered.
4. In all UI views, updated `GtkScrolledWindow` policy from `GTK_POLICY_NEVER` to `GTK_POLICY_AUTOMATIC` on the horizontal axis. This allows smooth custom window resizing to any dimension while presenting smooth horizontal scrolling when needed.

---

## [ISSUE-010] Companion Android Application (.apk) & Serverless Multi-Device Sync (GitHub Issue #10)
- **Status:** `RESOLVED`
- **Module:** Android NDK Core, Jetpack Compose UI, WorkManager Sync, Offline PDF Printing
- **Severity:** High (New Cross-Platform Companion App)
- **GitHub Issue:** `#10`
- **Files Created / Modified:**
  - `include/dashboard.h`, `src/auth/auth.c`, `src/sync/sync_core.c` (Android NDK API compatibility & built-in PBKDF2-HMAC-SHA256 parity)
  - `android/app/src/main/cpp/CMakeLists.txt`, `android/app/src/main/cpp/schoolcms_jni.c`
  - `android/app/src/main/java/com/apcoaching/schoolcms/**` (NativeBridge, CpanelSyncClient, SyncManager, SyncWorker, PdfPrintHelper, 10 Jetpack Compose screens)
  - `.github/workflows/release.yml`, `scripts/bump_version.py`

### Resolution
1. Ported the shared C11 persistence, PBKDF2 auth/RBAC, 10 operational modules, and Schema V3 delta sync engine into `libschoolcms_core.so` compiled across `arm64-v8a`, `armeabi-v7a`, and `x86_64` via Android NDK + CMake + SQLite `3.47.0`.
2. Built responsive Material 3 Jetpack Compose UI mirroring Dashboard, Students (`ADM-YYYY-M0001`), Faculty (`TCH`/`STF`), Classes/Sections/Subjects, Bulk Attendance, Fee Collection & Vector Receipt Print/Share PDF (`PrintManager`), Exams/Marksheets PDF, Conflict-Checked Timetable, Analytics Reports PDF, and RBAC + Cloud Sync Settings.
3. Integrated active in-app debounced push (`~900ms`) + 20s periodic pull and background Android `WorkManager` delta synchronization against `cpanel_backend/`.
4. Configured release APK signing, GitHub Release workflow automation, and PR merge into `master`.
