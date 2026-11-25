# Standalone Release Packaging & Installer Wizard Guide — SchoolCMS

Because Windows does not include GTK4, SQLite, or GDK-Pixbuf image loaders natively, SchoolCMS provides two zero-dependency deployment formats:
1. **Portable Zero-Dependency Release Bundle (.zip):** Pre-packaged folder with all 65 DLLs and a launcher script.
2. **Windows Setup Installer Wizard (.exe):** Self-contained, step-by-step installer created with NSIS Modern UI 2.

---

## 1. Portable Release Packaging

The automated packaging pipeline gathers the compiled application, scans imported PE headers, and bundles all transitive runtime DLLs, compiled icon themes, and schemas.

### How to Package:
From the project root:

**Using Python (Direct):**
```bash
python scripts/package_windows.py
```

**Using Batch:**
```cmd
scripts\package_windows.bat
```

### Generated Artifacts in `dist/`:

1. **Portable Directory:** `dist/APCOACHING-v1.1.6-windows-x64/`
   ```
   dist/APCOACHING-v1.1.6-windows-x64/
   ├── APCOACHING.exe                # Main compiled C11 application binary
   ├── Run_APCOACHING.bat            # Production launcher script with environment setup
   ├── README.txt                    # Standalone end-user instructions
   ├── 65 Runtime DLLs               # libgtk-4-1, librsvg-2-2, libsqlite3, libcrypto, etc.
   ├── assets/
   │   └── styles/main.css           # Modern SaaS stylesheet
   ├── lib/
   │   └── gdk-pixbuf-2.0/2.10.0/
   │       ├── loaders/              # pixbufloader_svg.dll and image decoders
   │       └── loaders.cache         # Registered image loader cache
   └── share/
       ├── glib-2.0/schemas/         # gschemas.compiled settings definitions
       └── icons/
           ├── Adwaita/              # Full vector Adwaita symbolic SVGs & cache
           └── hicolor/              # Fallback theme definitions
   ```

2. **Compressed Release Archive:** `dist/APCOACHING-v1.1.6-windows-x64.zip` (~28.8 MB)
   - Self-contained portable ZIP archive ready for distribution.
   - Users simply extract the ZIP on any Windows 10/11 x64 PC and double-click `Run_APCOACHING.bat`.

---

## 2. Windows Installer Wizard (Setup .exe)

To provide end users with a standard Windows installation experience (Desktop shortcut, Start Menu folder, and clean uninstaller in Windows Settings), an NSIS Modern UI 2 installer wizard is provided.

### Single-Click Generation:
Double-click `make_installer.bat` in the repository root, or run:
```bash
python scripts/build_installer.py
```

### Installer Output:
- **Target File:** `dist/APCOACHING-v1.1.6-Setup.exe` (~19.5 MB)
- **Compression:** Solid LZMA (compresses 73 MB of binaries down to under 20 MB)

### Key Features of the Installer:
- **Modern UI 2 Wizard:** Professional Welcome screen, Destination Folder selection, Extraction progress bar, and "Launch now" finish checkbox.
- **Per-User Installation:** Installs by default to `%LOCALAPPDATA%\Programs\<AppName>` — works without requiring Administrator (UAC) elevation, perfect for locked school computers.
- **Shortcuts:** Automatically creates Desktop and Start Menu shortcuts with the application's embedded multi-resolution icon.
- **Clean Uninstaller:** Registers in Windows *Settings > Installed apps* / *Control Panel* with version, publisher, icon, and estimated size. The uninstaller removes all files cleanly.
