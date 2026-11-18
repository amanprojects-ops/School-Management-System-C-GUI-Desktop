cmake_minimum_required(VERSION 3.20)
project(SchoolCMS VERSION 1.0.0 LANGUAGES C)

# C11 standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Compiler warnings
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic -Wno-unused-parameter -Wno-overlength-strings)
endif()

# Find GTK4, SQLite3, and OpenSSL Crypto via pkg-config
find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK4 REQUIRED IMPORTED_TARGET gtk4)
pkg_check_modules(SQLITE3 REQUIRED IMPORTED_TARGET sqlite3)
pkg_check_modules(CRYPTO REQUIRED IMPORTED_TARGET libcrypto)

# Source files
set(SOURCES
    src/main.c
    src/app.c
    src/database.c
    src/auth/auth.c
    src/modules/dashboard.c
    src/modules/student.c
    src/modules/teacher.c
    src/modules/classes.c
    src/modules/attendance.c
    src/modules/fees.c
    src/modules/exams.c
    src/ui/login.c
    src/ui/window.c
    src/ui/sidebar.c
    src/ui/header.c
    src/ui/dashboard.c
    src/ui/student_view.c
    src/ui/teacher_view.c
    src/ui/classes_view.c
    src/ui/attendance_view.c
    src/ui/fees_view.c
    src/ui/exams_view.c
)

# Build executable
add_executable(${PROJECT_NAME} WIN32 ${SOURCES})

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/ui
    ${CMAKE_SOURCE_DIR}/include/auth
)

# Link GTK4, SQLite3, and Crypto
target_link_libraries(${PROJECT_NAME} PRIVATE
    PkgConfig::GTK4
    PkgConfig::SQLITE3
    PkgConfig::CRYPTO
)

# Copy assets to build directory
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/assets
        $<TARGET_FILE_DIR:${PROJECT_NAME}>/assets
    COMMENT "Copying assets to build directory"
)

# Database unit test executable
add_executable(test_database
    tests/test_database.c
    src/database.c
)
target_include_directories(test_database PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)
target_link_libraries(test_database PRIVATE
    PkgConfig::SQLITE3
)

# Auth unit test executable
add_executable(test_auth
    tests/test_auth.c
    src/auth/auth.c
    src/database.c
)
target_include_directories(test_auth PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/auth
)
target_link_libraries(test_auth PRIVATE
    PkgConfig::SQLITE3
    PkgConfig::CRYPTO
)

# Dashboard unit test executable
add_executable(test_dashboard
    tests/test_dashboard.c
    src/modules/dashboard.c
    src/database.c
)
target_include_directories(test_dashboard PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)
target_link_libraries(test_dashboard PRIVATE
    PkgConfig::SQLITE3
    PkgConfig::GTK4
)

# Student unit test executable
add_executable(test_student
    tests/test_student.c
    src/modules/student.c
    src/database.c
)
target_include_directories(test_student PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)
target_link_libraries(test_student PRIVATE
    PkgConfig::SQLITE3
)

# Teacher unit test executable
add_executable(test_teacher
    tests/test_teacher.c
    src/modules/teacher.c
    src/database.c
)
target_include_directories(test_teacher PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)
target_link_libraries(test_teacher PRIVATE
    PkgConfig::SQLITE3
)

# Classes unit test executable
add_executable(test_classes
    tests/test_classes.c
    src/modules/classes.c
    src/database.c
)
target_include_directories(test_classes PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)
target_link_libraries(test_classes PRIVATE
    PkgConfig::SQLITE3
)

# Attendance unit test executable
add_executable(test_attendance
    tests/test_attendance.c
    src/modules/attendance.c
    src/database.c
)
target_include_directories(test_attendance PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)
target_link_libraries(test_attendance PRIVATE
    PkgConfig::SQLITE3
)

# Fees unit test executable
add_executable(test_fees
    tests/test_fees.c
    src/modules/fees.c
    src/database.c
)
target_include_directories(test_fees PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)
target_link_libraries(test_fees PRIVATE
    PkgConfig::SQLITE3
)

# Exams unit test executable
add_executable(test_exams
    tests/test_exams.c
    src/modules/exams.c
    src/database.c
)
target_include_directories(test_exams PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)
target_link_libraries(test_exams PRIVATE
    PkgConfig::SQLITE3
)

# Install rules
install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION bin)
install(DIRECTORY assets/ DESTINATION bin/assets)
