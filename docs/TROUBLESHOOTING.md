# Troubleshooting & Frequently Asked Questions — SchoolCMS

This guide documents common issues encountered during setup, compilation, runtime execution, and data export, along with verified solutions.

---

## 1. Missing DLL Errors on Standalone Execution

- **Symptom:** Running the application on a separate Windows computer displays:
  `The code execution cannot proceed because libgtk-4-1.dll was not found.`
- **Cause:** Windows does not ship GTK4 or SQLite libraries natively.
- **Solution:**
  - Always use the generated installer (`dist/APCOACHING-v1.1.3-Setup.exe`), OR
  - If using the portable ZIP package, run the app using `Run_APCOACHING.bat`. Do not move `APCOACHING.exe` out of its folder without its accompanying 65 DLLs.

---

## 2. Broken or Missing Vector Icons

- **Symptom:** The user interface shows blank square boxes instead of icons in the sidebar, header, or buttons.
- **Cause:** GTK4 symbolic icons are SVG files rendered via `librsvg-2-2.dll` and GDK-Pixbuf SVG loaders. If `librsvg` or `loaders.cache` is missing or unindexed, icon rendering fails.
- **Solution:**
  - Re-run `python scripts/package_windows.py`.
  - The script automatically copies `librsvg-2-2.dll`, generates a clean `loaders.cache`, and bundles the full Adwaita symbolic theme with a pre-compiled `icon-theme.cache`.

---

## 3. GLib GSettings Schema Error on Startup

- **Symptom:** The application crashes immediately on launch with:
  `GLib-GIO-ERROR **: Settings schema 'org.gtk.gtk4.Settings.FileChooser' is not installed.`
- **Cause:** GTK4 file chooser and dialogs depend on compiled GSettings XML schemas.
- **Solution:** Ensure `share/glib-2.0/schemas/gschemas.compiled` is present in the application folder. The launcher script `Run_APCOACHING.bat` and NSIS installer automatically set the `GSETTINGS_SCHEMA_DIR` environment variable to point to this directory.

---

## 4. Database Busy / Locked (`SQLITE_BUSY`)

- **Symptom:** Database transactions fail with `database is locked`.
- **Cause:** Another instance of `APCOACHING.exe` is already running and holding a write transaction lock, or a background process hung.
- **Solution:**
  - Close any duplicate instances of the application.
  - Terminate hung background processes in PowerShell:
    ```powershell
    Stop-Process -Name APCOACHING -Force
    ```
  - SchoolCMS uses SQLite `WAL` mode (`PRAGMA journal_mode=WAL;`), which allows multiple concurrent readers and handles locks cleanly under normal operation.

---

## 5. CSV Exports Showing Garbled Characters in Microsoft Excel

- **Symptom:** Exported student lists or fee reports display strange characters (e.g. `â‚¹` instead of `₹`) in Microsoft Excel.
- **Cause:** Excel defaults to ANSI/ASCII encoding unless a UTF-8 Byte Order Mark (`BOM`) is detected.
- **Solution:** SchoolCMS automatically prefixes every exported CSV file with the standard UTF-8 BOM (`\xEF\xBB\xBF`). When opened in Excel, characters such as currency symbols (`₹`), quotes, and special characters render cleanly without needing import wizard adjustments.

---

## 6. Window Resizing & Screen Boundary Issues

- **Symptom:** Window appears too large on 720p or 1366x768 screens, or cannot be resized smaller.
- **Solution (Fixed in v1.1.3):**
  - SchoolCMS v1.1.3 automatically detects primary monitor geometry and sets default window dimensions to 88% width and 82% height, leaving safe margins for the Windows taskbar and titlebar.
  - Minimum window width was lowered to 640px and height to 450px, allowing seamless half-screen Windows Snap (`Win + Left/Right`).
