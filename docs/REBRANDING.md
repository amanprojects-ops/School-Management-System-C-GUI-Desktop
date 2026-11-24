# Application Customization & Rebranding Guide — SchoolCMS

This guide explains how to rebrand SchoolCMS for any school, college, or coaching institute — including changing the software name, executable name (`.exe`), Windows desktop icon, in-app logo, and institution details.

---

## 1. Single-Click Rebranding (Recommended)

SchoolCMS includes an automated rebranding tool that updates all C11 source files, CMake targets, resource files, and scripts in one click, and automatically recompiles the binary with Ninja.

### Using Batch (Interactive):
Double-click `rebrand.bat` in the repository root:
```cmd
rebrand.bat
```
The wizard will ask you for:
1. **New App & Executable Name** (e.g., `EduMaster`, `MyCoaching`)
2. **Icon Name or Path** (e.g., `applications-education-symbolic`, or path to your custom `.ico` / `.png` logo)
3. **School / Institution Name** (e.g., `Apex International School`)

### Using Python CLI:
```bash
# Rename app & .exe to EduMaster, update institution name, and build immediately:
python scripts/rebrand_app.py EduMaster --school "Apex International Academy" --build

# Use a custom PNG image for the logo:
python scripts/rebrand_app.py EduMaster --icon "C:/path/to/my_logo.png" --build
```

---

## 2. Changing the Institution Name (Runtime & Database)

The school name that appears on identity cards, fee receipts, and marksheets is stored dynamically in SQLite.

### Method A: Via GUI Settings View (No Code Required)
1. Launch the app and log in as Administrator.
2. Click **Settings** in the left sidebar.
3. In the **Institutional Profile** card, edit:
   - **School Name** (e.g., `Delhi Public Coaching Center`)
   - **Tagline**, **Principal Name**, **Address**, **Phone**, **Email**, **Website**
   - **Currency Symbol** (`₹`, `$`, `INR`, etc.)
4. Click **Save Changes**. All receipts, ID cards, and exports will update immediately.

### Method B: Via SQLite Query
```sql
UPDATE settings SET value = 'Delhi Public Coaching Center' WHERE key = 'school_name';
UPDATE settings SET value = 'Dr. Sunita Sharma' WHERE key = 'principal_name';
```

---

## 3. Changing Application Icons & Logos

### Windows Executable (.exe) Icon
The icon shown on the `.exe` file in Windows Explorer, on the desktop shortcut, and in the taskbar is embedded directly into the PE binary:
1. Place a multi-resolution `.ico` file (16x16, 32x32, 48x48, 256x256) at `assets/icons/app.ico`.
2. The icon is linked via `src/app.rc`:
   ```rc
   1 ICON "../assets/icons/app.ico"
   ```
3. When compiled with `ninja`, CMake compiles `src/app.rc` with `windres` and embeds the icon into the `.exe`.

### In-App Sidebar & Login Screen Logo (PNG Image)
1. Save your institutional logo image as `assets/images/app_logo.png` (transparent PNG recommended).
2. The sidebar (`src/ui/sidebar.c`) and login screen (`src/ui/login.c`) automatically load this image at launch.

---

## 4. Manual Code Customization Reference

If you prefer to edit files manually rather than using `rebrand.bat`:

| Target Component | Source File | Current Code | Where to Edit |
| :--- | :--- | :--- | :--- |
| **Main Window Title** | `src/ui/window.c` | `gtk_window_set_title(..., "APCOACHING — ...")` | Edit title string |
| **Status Bar Brand** | `src/ui/window.c` | `gtk_label_new("APCOACHING v1.1.3 • ...")` | Edit brand text |
| **Sidebar Brand Header** | `src/ui/sidebar.c` | `<span weight='bold'>APCOACHING</span>` | Edit brand text |
| **Sidebar Version Footer** | `src/ui/sidebar.c` | `gtk_label_new("APCOACHING v1.1.3")` | Edit version text |
| **Login Screen Title** | `src/ui/login.c` | `<span size='xx-large'>APCOACHING</span>` | Edit hero title |
| **CMake Project Target** | `CMakeLists.txt` | `project(APCOACHING VERSION ...)` | Change project name |
| **Release Packager** | `scripts/package_windows.py` | `APP_NAME = "APCOACHING"` | Update target name |
| **Installer Wizard** | `installer/installer.nsi` | `!define APP_NAME "APCOACHING"` | Update setup name |

After manual edits, rebuild and package:
```bash
cd build && ninja
python scripts/package_windows.py
python scripts/build_installer.py
```
