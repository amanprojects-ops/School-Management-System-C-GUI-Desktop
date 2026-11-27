cmake_minimum_required(VERSION 3.18.1)
project(schoolcms_core LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

get_filename_component(SCHOOLCMS_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../../.." ABSOLUTE)

# Bundled SQLite 3.47.0 amalgamation with JSON1 + WAL thread-safety enabled
add_library(sqlite3_static STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/sqlite3/sqlite3.c
)
target_include_directories(sqlite3_static PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/sqlite3
)
target_compile_definitions(sqlite3_static PRIVATE
    SQLITE_THREADSAFE=1
    SQLITE_ENABLE_JSON1=1
    SQLITE_ENABLE_FTS5=1
    SQLITE_DEFAULT_WAL_SYNCHRONOUS=1
)

# Shared SchoolCMS C11 core + offline-first delta sync engine + JNI bridge
add_library(schoolcms_core SHARED
    ${CMAKE_CURRENT_SOURCE_DIR}/schoolcms_jni.c
    ${SCHOOLCMS_ROOT}/src/database.c
    ${SCHOOLCMS_ROOT}/src/auth/auth.c
    ${SCHOOLCMS_ROOT}/src/sync/sync_core.c
    ${SCHOOLCMS_ROOT}/src/modules/dashboard.c
    ${SCHOOLCMS_ROOT}/src/modules/student.c
    ${SCHOOLCMS_ROOT}/src/modules/teacher.c
    ${SCHOOLCMS_ROOT}/src/modules/classes.c
    ${SCHOOLCMS_ROOT}/src/modules/attendance.c
    ${SCHOOLCMS_ROOT}/src/modules/fees.c
    ${SCHOOLCMS_ROOT}/src/modules/exams.c
    ${SCHOOLCMS_ROOT}/src/modules/timetable.c
    ${SCHOOLCMS_ROOT}/src/modules/reports.c
    ${SCHOOLCMS_ROOT}/src/modules/settings.c
)

target_include_directories(schoolcms_core PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/sqlite3
    ${SCHOOLCMS_ROOT}/include
    ${SCHOOLCMS_ROOT}/include/auth
    ${SCHOOLCMS_ROOT}/include/sync
)

target_compile_options(schoolcms_core PRIVATE
    -std=c11
    -Wall
    -Wextra
    -Wno-unused-parameter
    -Wno-overlength-strings
)

find_library(log-lib log)

target_link_libraries(schoolcms_core
    sqlite3_static
    ${log-lib}
)
