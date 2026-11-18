# SchoolCMS — School Management System

A modern, professional, GUI-based School Management System desktop application built with **pure C**, **GTK4**, and **SQLite3**.

## Technology Stack

| Component      | Technology       |
|----------------|------------------|
| Language        | C (C11)          |
| GUI Framework   | GTK4             |
| Database        | SQLite3          |
| Build System    | CMake 3.20+      |
| Styling         | GTK CSS          |
| Platform        | Windows 10/11    |

## Building

### Prerequisites (MSYS2 / MinGW)

```bash
pacman -S mingw-w64-ucrt-x86_64-gtk4 mingw-w64-ucrt-x86_64-sqlite3 \
          mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-gcc \
          mingw-w64-ucrt-x86_64-pkg-config
```

### Compile & Run

```bash
cd SchoolCMS
mkdir build && cd build
cmake -G "MinGW Makefiles" ..
cmake --build .
./SchoolCMS.exe
```

## Development Phases

- [x] Phase 1: Project Setup and GTK4 Application Shell
- [x] Phase 2: Database and Migration System
- [x] Phase 3: Login and Authentication
- [x] Phase 4: Main Dashboard UI (live data)
- [x] Phase 5: Student Management CRUD
- [x] Phase 6: Teacher & Staff Management
- [ ] Phase 7: Class & Section Management
- [ ] Phase 8: Attendance System
- [ ] Phase 9: Fee Management
- [ ] Phase 10: Examination & Results
- [ ] Phase 11: Timetable
- [ ] Phase 12: Reports and Export
- [ ] Phase 13: Settings, Backup & Restore
- [ ] Phase 14: Testing and Bug Fixing
- [ ] Phase 15: Windows EXE Packaging and Installer

## License

Proprietary — All rights reserved.
