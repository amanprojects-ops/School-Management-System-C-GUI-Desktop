# Building & Setup Guide — SchoolCMS

This guide covers prerequisites, MSYS2 UCRT64 toolchain setup, compiling the source code with CMake and Ninja, and running the application in development mode.

---

## 1. Prerequisites & Environment Setup (MSYS2 UCRT64)

SchoolCMS is developed in pure **C11** and built natively on Windows using the **MSYS2 UCRT64** toolchain.

### Step 1: Install MSYS2
1. Download the MSYS2 installer from [https://www.msys2.org/](https://www.msys2.org/).
2. Run the installer and install to `C:\msys64` (default).
3. Once installation completes, open the **MSYS2 UCRT64** terminal:
   - Shortcut: `MSYS2 UCRT64` from Start Menu, OR
   - Run `C:\msys64\ucrt64.exe`.

### Step 2: Install Compiler, GTK4, SQLite, and Tooling
Execute the following commands inside the MSYS2 UCRT64 terminal:

```bash
# Update package databases
pacman -Syu --noconfirm

# Install C11 compiler, CMake, Ninja, and build tools
pacman -S --needed --noconfirm \
    mingw-w64-ucrt-x86_64-gcc \
    mingw-w64-ucrt-x86_64-cmake \
    mingw-w64-ucrt-x86_64-ninja \
    mingw-w64-ucrt-x86_64-pkg-config \
    mingw-w64-ucrt-x86_64-gdb \
    git

# Install GTK4, SQLite3, OpenSSL, SVG renderer, and icon themes
pacman -S --needed --noconfirm \
    mingw-w64-ucrt-x86_64-gtk4 \
    mingw-w64-ucrt-x86_64-sqlite3 \
    mingw-w64-ucrt-x86_64-openssl \
    mingw-w64-ucrt-x86_64-librsvg \
    mingw-w64-ucrt-x86_64-gdk-pixbuf2 \
    mingw-w64-ucrt-x86_64-adwaita-icon-theme \
    mingw-w64-ucrt-x86_64-hicolor-icon-theme

# Install Python & NSIS (required for release packaging and installer wizard)
pacman -S --needed --noconfirm \
    mingw-w64-ucrt-x86_64-python \
    mingw-w64-ucrt-x86_64-nsis
```

### Step 3: Configure Windows System PATH (Optional for CMD / PowerShell)
To run build commands directly from Windows PowerShell or Command Prompt, add the following directories to your Windows `PATH`:
- `C:\msys64\ucrt64\bin`
- `C:\msys64\usr\bin`

Verify setup by opening PowerShell and running:
```powershell
gcc --version
cmake --version
ninja --version
pkg-config --modversion gtk4
```

---

## 2. Step-by-Step Compilation Guide

### Option A: Building with CMake and Ninja (Recommended — Fastest)

1. Open your terminal (MSYS2 UCRT64 or PowerShell with UCRT64 in PATH):
   ```bash
   cd C:/apps/SchoolCMS
   ```

2. Create a clean build directory:
   ```bash
   mkdir -p build
   cd build
   ```

3. Run CMake configuration targeting the Ninja generator:
   ```bash
   cmake -G Ninja -DCMAKE_BUILD_TYPE=Release ..
   ```

4. Compile the application and all test suites:
   ```bash
   ninja
   ```

**Compilation Output:**
- `build/APCOACHING.exe` (main application binary with embedded Windows icon)
- `build/assets/` (application styles, images, and visual assets)
- 13 test executables (`test_database.exe`, `test_auth.exe`, etc.)

### Option B: Building with CMake and MinGW Makefiles

If you do not have Ninja installed:
```bash
cd C:/apps/SchoolCMS
mkdir -p build && cd build
cmake -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release ..
cmake --build . -j4
```

---

## 3. Running the Application

### Launching in Development Mode
From the `build/` directory in MSYS2 or Git Bash:
```bash
./APCOACHING.exe
```

Or from the project root in PowerShell:
```powershell
.\build\APCOACHING.exe
```

### Default Administrator Credentials
On first launch, SQLite automatically runs all schema migrations, creates the 19 tables, and seeds default administrative credentials:

| Field | Value |
| :--- | :--- |
| **Username** | `admin` |
| **Password** | `Password123!` |
| **Role** | `ADMINISTRATOR` |

> [!NOTE]
> All passwords are cryptographically hashed using **PBKDF2-HMAC-SHA256** with 10,000 iterations and a unique salt. The database file `schoolcms.db` is stored locally in the application working directory with Write-Ahead Logging (`WAL`) mode enabled.
