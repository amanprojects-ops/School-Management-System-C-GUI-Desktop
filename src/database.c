/**
 * @file database.c
 * @brief SQLite3 Database and migration implementation for SchoolCMS.
 */

#include "database.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Global database handle */
static sqlite3 *s_db = NULL;

/* ── Migration DDL (Version 1) ────────────────────────────────────── */

static const char *SCHEMA_V1 =
    "-- Enable foreign keys\n"
    "PRAGMA foreign_keys = ON;\n"

    "-- 1. Roles table\n"
    "CREATE TABLE IF NOT EXISTS roles (\n"
    "    id INTEGER PRIMARY KEY AUTOINCREMENT,\n"
    "    name TEXT UNIQUE NOT NULL,\n"
    "    description TEXT\n"
    ");\n"

    "-- 2. Users table\n"
    "CREATE TABLE IF NOT EXISTS users (\n"
    "    id INTEGER PRIMARY KEY AUTOINCREMENT,\n"
    "    username TEXT UNIQUE NOT NULL,\n"
    "    password_hash TEXT NOT NULL,\n"
    "    salt TEXT NOT NULL,\n"
    "    role_id INTEGER NOT NULL,\n"
    "    full_name TEXT NOT NULL,\n"
    "    email TEXT UNIQUE,\n"
    "    phone TEXT,\n"
    "    status TEXT NOT NULL DEFAULT 'ACTIVE',\n"
    "    failed_attempts INTEGER DEFAULT 0,\n"
    "    locked_until DATETIME,\n"
    "    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,\n"
    "    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,\n"
    "    FOREIGN KEY (role_id) REFERENCES roles(id)\n"
    ");\n"

    "-- 3. Academic Years table\n"
    "CREATE TABLE IF NOT EXISTS academic_years (\n"
    "    id INTEGER PRIMARY KEY AUTOINCREMENT,\n"
    "    name TEXT UNIQUE NOT NULL,\n"
    "    start_date DATE NOT NULL,\n"
    "    end_date DATE NOT NULL,\n"
    "    is_current INTEGER NOT NULL DEFAULT 0\n"
    ");\n"

    "-- 4. Classes table\n"
    "CREATE TABLE IF NOT EXISTS classes (\n"
    "    id INTEGER PRIMARY KEY AUTOINCREMENT,\n"
    "    name TEXT NOT NULL,\n"
    "    numeric_value INTEGER NOT NULL,\n"
    "    capacity INTEGER NOT NULL DEFAULT 40,\n"
    "    academic_year_id INTEGER,\n"
    "    FOREIGN KEY (academic_year_id) REFERENCES academic_years(id)\n"
    ");\n"

    "-- 5. Sections table\n"
    "CREATE TABLE IF NOT EXISTS sections (\n"
    "    id INTEGER PRIMARY KEY AUTOINCREMENT,\n"
    "    class_id INTEGER NOT NULL,\n"
    "    name TEXT NOT NULL,\n"
    "    class_teacher_id INTEGER,\n"
    "    FOREIGN KEY (class_id) REFERENCES classes(id) ON DELETE CASCADE,\n"
    "    UNIQUE(class_id, name)\n"
    ");\n"

    "-- 6. Teachers table\n"
    "CREATE TABLE IF NOT EXISTS teachers (\n"
    "    id INTEGER PRIMARY KEY AUTOINCREMENT,\n"
    "    teacher_code TEXT UNIQUE NOT NULL,\n"
    "    user_id INTEGER UNIQUE,\n"
    "    full_name TEXT NOT NULL,\n"
    "    phone TEXT NOT NULL,\n"
    "    email TEXT UNIQUE,\n"
    "    address TEXT,\n"
    "    qualification TEXT,\n"
    "    subject TEXT,\n"
    "    joining_date DATE NOT NULL,\n"
    "    salary REAL NOT NULL DEFAULT 0.0,\n"
    "    status TEXT NOT NULL DEFAULT 'ACTIVE',\n"
    "    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,\n"
    "    FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE SET NULL\n"
    ");\n"

    "-- 7. Staff table\n"
    "CREATE TABLE IF NOT EXISTS staff (\n"
    "    id INTEGER PRIMARY KEY AUTOINCREMENT,\n"
    "    staff_code TEXT UNIQUE NOT NULL,\n"
    "    user_id INTEGER UNIQUE,\n"
    "    full_name TEXT NOT NULL,\n"
    "    role_title TEXT NOT NULL,\n"
    "    phone TEXT NOT NULL,\n"
    "    email TEXT UNIQUE,\n"
    "    address TEXT,\n"
    "    joining_date DATE NOT NULL,\n"
    "    salary REAL NOT NULL DEFAULT 0.0,\n"
    "    status TEXT NOT NULL DEFAULT 'ACTIVE',\n"
    "    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,\n"
    "    FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE SET NULL\n"
    ");\n"

    "-- 8. Students table\n"
    "CREATE TABLE IF NOT EXISTS students (\n"
    "    id INTEGER PRIMARY KEY AUTOINCREMENT,\n"
    "    admission_no TEXT UNIQUE NOT NULL,\n"
    "    full_name TEXT NOT NULL,\n"
    "    father_name TEXT,\n"
    "    mother_name TEXT,\n"
    "    dob DATE,\n"
    "    gender TEXT NOT NULL,\n"
    "    category TEXT DEFAULT 'GENERAL',\n"
    "    phone TEXT,\n"
    "    email TEXT,\n"
    "    address TEXT,\n"
    "    admission_date DATE NOT NULL,\n"
    "    class_id INTEGER NOT NULL,\n"
    "    section_id INTEGER NOT NULL,\n"
    "    roll_number INTEGER,\n"
    "    previous_school TEXT,\n"
    "    emergency_contact TEXT,\n"
    "    blood_group TEXT,\n"
    "    photo_path TEXT,\n"
    "    status TEXT NOT NULL DEFAULT 'ACTIVE',\n"
    "    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,\n"
    "    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,\n"
    "    FOREIGN KEY (class_id) REFERENCES classes(id),\n"
    "    FOREIGN KEY (section_id) REFERENCES sections(id)\n"
    ");\n"

    "-- 9. Subjects table\n"
    "CREATE TABLE IF NOT EXISTS subjects (\n"
    "    id INTEGER PRIMARY KEY AUTOINCREMENT,\n"
    "    code TEXT NOT NULL,\n"
    "    name TEXT NOT NULL,\n"
    "    class_id INTEGER NOT NULL,\n"
    "    teacher_id INTEGER,\n"
    "    FOREIGN KEY (class_id) REFERENCES classes(id) ON DELETE CASCADE,\n"
    "    FOREIGN KEY (teacher_id) REFERENCES teachers(id) ON DELETE SET NULL,\n"
    "    UNIQUE(code, class_id)\n"
    ");\n"

    "-- 10. Attendance table\n"
    "CREATE TABLE IF NOT EXISTS attendance (\n"
    "    id INTEGER PRIMARY KEY AUTOINCREMENT,\n"
    "    student_id INTEGER NOT NULL,\n"
    "    class_id INTEGER NOT NULL,\n"
    "    section_id INTEGER NOT NULL,\n"
    "    date DATE NOT NULL,\n"
    "    status TEXT NOT NULL CHECK(status IN ('PRESENT', 'ABSENT', 'LATE', 'HALF_DAY')),\n"
    "    remarks TEXT,\n"
    "    recorded_by INTEGER,\n"
    "    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,\n"
    "    FOREIGN KEY (student_id) REFERENCES students(id) ON DELETE CASCADE,\n"
    "    FOREIGN KEY (class_id) REFERENCES classes(id),\n"
    "    FOREIGN KEY (section_id) REFERENCES sections(id),\n"
    "    FOREIGN KEY (recorded_by) REFERENCES users(id),\n"
    "    UNIQUE(student_id, date)\n"
    ");\n"

    "-- 11. Fee Structures table\n"
    "CREATE TABLE IF NOT EXISTS fee_structures (\n"
    "    id INTEGER PRIMARY KEY AUTOINCREMENT,\n"
    "    name TEXT NOT NULL,\n"
    "    class_id INTEGER NOT NULL,\n"
    "    academic_year_id INTEGER NOT NULL,\n"
    "    fee_type TEXT NOT NULL,\n"
    "    amount REAL NOT NULL,\n"
    "    due_date DATE,\n"
    "    FOREIGN KEY (class_id) REFERENCES classes(id),\n"
    "    FOREIGN KEY (academic_year_id) REFERENCES academic_years(id)\n"
    ");\n"

    "-- 12. Fee Payments table\n"
    "CREATE TABLE IF NOT EXISTS fee_payments (\n"
    "    id INTEGER PRIMARY KEY AUTOINCREMENT,\n"
    "    receipt_no TEXT UNIQUE NOT NULL,\n"
    "    student_id INTEGER NOT NULL,\n"
    "    fee_structure_id INTEGER NOT NULL,\n"
    "    amount_paid REAL NOT NULL,\n"
    "    discount REAL DEFAULT 0.0,\n"
    "    late_fine REAL DEFAULT 0.0,\n"
    "    total_amount REAL NOT NULL,\n"
    "    payment_method TEXT NOT NULL CHECK(payment_method IN ('CASH', 'UPI', 'BANK_TRANSFER', 'CHEQUE')),\n"
    "    transaction_ref TEXT,\n"
    "    payment_date DATE NOT NULL,\n"
    "    remarks TEXT,\n"
    "    collected_by INTEGER,\n"
    "    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,\n"
    "    FOREIGN KEY (student_id) REFERENCES students(id),\n"
    "    FOREIGN KEY (fee_structure_id) REFERENCES fee_structures(id),\n"
    "    FOREIGN KEY (collected_by) REFERENCES users(id)\n"
    ");\n"

    "-- 13. Examinations table\n"
    "CREATE TABLE IF NOT EXISTS examinations (\n"
    "    id INTEGER PRIMARY KEY AUTOINCREMENT,\n"
    "    name TEXT NOT NULL,\n"
    "    academic_year_id INTEGER NOT NULL,\n"
    "    start_date DATE NOT NULL,\n"
    "    end_date DATE NOT NULL,\n"
    "    grading_system TEXT DEFAULT 'PERCENTAGE',\n"
    "    status TEXT NOT NULL DEFAULT 'PLANNED',\n"
    "    FOREIGN KEY (academic_year_id) REFERENCES academic_years(id)\n"
    ");\n"

    "-- 14. Exam Subjects table\n"
    "CREATE TABLE IF NOT EXISTS exam_subjects (\n"
    "    id INTEGER PRIMARY KEY AUTOINCREMENT,\n"
    "    exam_id INTEGER NOT NULL,\n"
    "    subject_id INTEGER NOT NULL,\n"
    "    exam_date DATE NOT NULL,\n"
    "    max_marks REAL NOT NULL DEFAULT 100.0,\n"
    "    passing_marks REAL NOT NULL DEFAULT 35.0,\n"
    "    FOREIGN KEY (exam_id) REFERENCES examinations(id) ON DELETE CASCADE,\n"
    "    FOREIGN KEY (subject_id) REFERENCES subjects(id) ON DELETE CASCADE,\n"
    "    UNIQUE(exam_id, subject_id)\n"
    ");\n"

    "-- 15. Exam Results table\n"
    "CREATE TABLE IF NOT EXISTS exam_results (\n"
    "    id INTEGER PRIMARY KEY AUTOINCREMENT,\n"
    "    exam_subject_id INTEGER NOT NULL,\n"
    "    student_id INTEGER NOT NULL,\n"
    "    marks_obtained REAL NOT NULL,\n"
    "    grade TEXT,\n"
    "    remarks TEXT,\n"
    "    FOREIGN KEY (exam_subject_id) REFERENCES exam_subjects(id) ON DELETE CASCADE,\n"
    "    FOREIGN KEY (student_id) REFERENCES students(id) ON DELETE CASCADE,\n"
    "    UNIQUE(exam_subject_id, student_id)\n"
    ");\n"

    "-- 16. Timetables table\n"
    "CREATE TABLE IF NOT EXISTS timetables (\n"
    "    id INTEGER PRIMARY KEY AUTOINCREMENT,\n"
    "    class_id INTEGER NOT NULL,\n"
    "    section_id INTEGER NOT NULL,\n"
    "    day_of_week TEXT NOT NULL CHECK(day_of_week IN ('MONDAY', 'TUESDAY', 'WEDNESDAY', 'THURSDAY', 'FRIDAY', 'SATURDAY')),\n"
    "    period_number INTEGER NOT NULL,\n"
    "    start_time TIME NOT NULL,\n"
    "    end_time TIME NOT NULL,\n"
    "    subject_id INTEGER NOT NULL,\n"
    "    teacher_id INTEGER NOT NULL,\n"
    "    room_number TEXT,\n"
    "    FOREIGN KEY (class_id) REFERENCES classes(id) ON DELETE CASCADE,\n"
    "    FOREIGN KEY (section_id) REFERENCES sections(id) ON DELETE CASCADE,\n"
    "    FOREIGN KEY (subject_id) REFERENCES subjects(id) ON DELETE CASCADE,\n"
    "    FOREIGN KEY (teacher_id) REFERENCES teachers(id) ON DELETE CASCADE,\n"
    "    UNIQUE(class_id, section_id, day_of_week, period_number)\n"
    ");\n"

    "-- 17. Notifications table\n"
    "CREATE TABLE IF NOT EXISTS notifications (\n"
    "    id INTEGER PRIMARY KEY AUTOINCREMENT,\n"
    "    user_id INTEGER,\n"
    "    title TEXT NOT NULL,\n"
    "    message TEXT NOT NULL,\n"
    "    is_read INTEGER NOT NULL DEFAULT 0,\n"
    "    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,\n"
    "    FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE\n"
    ");\n"

    "-- 18. Activity Logs table\n"
    "CREATE TABLE IF NOT EXISTS activity_logs (\n"
    "    id INTEGER PRIMARY KEY AUTOINCREMENT,\n"
    "    user_id INTEGER,\n"
    "    action TEXT NOT NULL,\n"
    "    module TEXT NOT NULL,\n"
    "    details TEXT,\n"
    "    ip_address TEXT DEFAULT '127.0.0.1',\n"
    "    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,\n"
    "    FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE SET NULL\n"
    ");\n"

    "-- 19. Settings table\n"
    "CREATE TABLE IF NOT EXISTS settings (\n"
    "    key TEXT PRIMARY KEY NOT NULL,\n"
    "    value TEXT NOT NULL,\n"
    "    description TEXT,\n"
    "    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP\n"
    ");\n"

    "-- 20. Role Permissions table\n"
    "CREATE TABLE IF NOT EXISTS role_permissions (\n"
    "    id INTEGER PRIMARY KEY AUTOINCREMENT,\n"
    "    role_id INTEGER NOT NULL,\n"
    "    module_name TEXT NOT NULL,\n"
    "    can_view INTEGER NOT NULL DEFAULT 1,\n"
    "    can_edit INTEGER NOT NULL DEFAULT 0,\n"
    "    FOREIGN KEY (role_id) REFERENCES roles(id) ON DELETE CASCADE,\n"
    "    UNIQUE(role_id, module_name)\n"
    ");\n"

    "-- Indexes\n"
    "CREATE INDEX IF NOT EXISTS idx_users_username ON users(username);\n"
    "CREATE INDEX IF NOT EXISTS idx_students_admission_no ON students(admission_no);\n"
    "CREATE INDEX IF NOT EXISTS idx_students_class_section ON students(class_id, section_id);\n"
    "CREATE INDEX IF NOT EXISTS idx_attendance_date_class ON attendance(date, class_id, section_id);\n"
    "CREATE INDEX IF NOT EXISTS idx_attendance_student_date ON attendance(student_id, date);\n"
    "CREATE INDEX IF NOT EXISTS idx_fee_payments_receipt ON fee_payments(receipt_no);\n"
    "CREATE INDEX IF NOT EXISTS idx_fee_payments_student ON fee_payments(student_id);\n"
    "CREATE INDEX IF NOT EXISTS idx_timetable_teacher_slot ON timetables(teacher_id, day_of_week, period_number);\n"
    "CREATE INDEX IF NOT EXISTS idx_timetable_class_slot ON timetables(class_id, section_id, day_of_week, period_number);\n"
    "CREATE INDEX IF NOT EXISTS idx_exam_results_student ON exam_results(exam_subject_id, student_id);\n"
    "CREATE INDEX IF NOT EXISTS idx_activity_logs_created ON activity_logs(created_at);\n"
    "CREATE INDEX IF NOT EXISTS idx_role_permissions_role ON role_permissions(role_id);\n";

/* ── Seed Data ────────────────────────────────────────────────────── */

static const char *SEED_DATA =
    "-- Initial roles\n"
    "INSERT OR IGNORE INTO roles (id, name, description) VALUES\n"
    "    (1, 'ADMIN', 'Full administrative access'),\n"
    "    (2, 'TEACHER', 'Academic, attendance and exam result entry'),\n"
    "    (3, 'ACCOUNTANT', 'Fee collection and financial report management'),\n"
    "    (4, 'STAFF', 'General staff read-only and inquiry access');\n"

    "-- Default academic year\n"
    "INSERT OR IGNORE INTO academic_years (id, name, start_date, end_date, is_current) VALUES\n"
    "    (1, '2026-2027', '2026-04-01', '2027-03-31', 1);\n"

    "-- Default Administrator user (username: admin, initial password hash placeholder - phase 3 updates with salt)\n"
    "INSERT OR IGNORE INTO users (id, username, password_hash, salt, role_id, full_name, email, status) VALUES\n"
    "    (1, 'admin', '8c6976e5b5410415bde908bd4dee15dfb167a9c873fc4bb8a81f6f2ab448a918', 'c2Nob29sY21zX3NhbHQ=', 1, 'School Administrator', 'admin@schoolcms.local', 'ACTIVE');\n"

    "-- Default School Settings\n"
    "INSERT OR IGNORE INTO settings (key, value, description) VALUES\n"
    "    ('school_name', 'St. Xavier International School', 'Official school institution name'),\n"
    "    ('school_address', '124 Academic Enclave, Sector 14, City', 'Physical mailing address'),\n"
    "    ('school_phone', '+91 98765 43210', 'Primary contact phone number'),\n"
    "    ('school_email', 'info@stxavier.edu', 'Institutional contact email'),\n"
    "    ('school_website', 'https://stxavier.edu', 'Official website URL'),\n"
    "    ('principal_name', 'Dr. Alok Verma', 'Name of the current school principal'),\n"
    "    ('currency_symbol', '₹', 'Currency symbol used for accounting and receipts'),\n"
    "    ('theme_mode', 'light', 'Default UI theme mode: light or dark'),\n"
    "    ('smtp_host', 'smtp.gmail.com', 'Outgoing SMTP server hostname'),\n"
    "    ('smtp_port', '587', 'SMTP server port (587 for TLS, 465 for SSL)'),\n"
    "    ('smtp_username', 'school.admin@example.com', 'SMTP authentication username'),\n"
    "    ('smtp_password', '', 'SMTP authentication password or app token'),\n"
    "    ('smtp_encryption', 'STARTTLS', 'SMTP encryption protocol: STARTTLS, SSL, or NONE'),\n"
    "    ('smtp_sender_name', 'School Administration', 'Outgoing email sender display name'),\n"
    "    ('smtp_sender_email', 'noreply@schoolcms.edu', 'Outgoing email from address');\n"

    "-- Initial role permissions\n"
    "INSERT OR IGNORE INTO role_permissions (role_id, module_name, can_view, can_edit) VALUES\n"
    "    (1, 'STUDENTS', 1, 1), (1, 'TEACHERS', 1, 1), (1, 'CLASSES', 1, 1),\n"
    "    (1, 'ATTENDANCE', 1, 1), (1, 'FEES', 1, 1), (1, 'EXAMS', 1, 1),\n"
    "    (1, 'TIMETABLE', 1, 1), (1, 'REPORTS', 1, 1), (1, 'SETTINGS', 1, 1),\n"
    "    (2, 'STUDENTS', 1, 0), (2, 'TEACHERS', 1, 0), (2, 'CLASSES', 1, 0),\n"
    "    (2, 'ATTENDANCE', 1, 1), (2, 'FEES', 0, 0), (2, 'EXAMS', 1, 1),\n"
    "    (2, 'TIMETABLE', 1, 0), (2, 'REPORTS', 1, 0), (2, 'SETTINGS', 0, 0),\n"
    "    (3, 'STUDENTS', 1, 0), (3, 'TEACHERS', 0, 0), (3, 'CLASSES', 1, 0),\n"
    "    (3, 'ATTENDANCE', 1, 0), (3, 'FEES', 1, 1), (3, 'EXAMS', 0, 0),\n"
    "    (3, 'TIMETABLE', 0, 0), (3, 'REPORTS', 1, 1), (3, 'SETTINGS', 0, 0),\n"
    "    (4, 'STUDENTS', 1, 0), (4, 'TEACHERS', 1, 0), (4, 'CLASSES', 1, 0),\n"
    "    (4, 'ATTENDANCE', 1, 0), (4, 'FEES', 0, 0), (4, 'EXAMS', 0, 0),\n"
    "    (4, 'TIMETABLE', 1, 0), (4, 'REPORTS', 0, 0), (4, 'SETTINGS', 0, 0);\n";

/* ── Implementation ──────────────────────────────────────────────── */

DbResult db_init(const char *db_path)
{
    const char *path = (db_path != NULL && strlen(db_path) > 0) ? db_path : DB_DEFAULT_FILE;

    if (s_db != NULL) {
        return DB_SUCCESS; /* Already initialized */
    }

    int rc = sqlite3_open_v2(path, &s_db,
                             SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX,
                             NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[DB ERROR] Failed to open SQLite database '%s': %s\n",
                path, sqlite3_errmsg(s_db));
        if (s_db) {
            sqlite3_close_v2(s_db);
            s_db = NULL;
        }
        return DB_ERROR_OPEN;
    }

    /* Configure WAL mode and foreign keys */
    char *errmsg = NULL;
    rc = sqlite3_exec(s_db, "PRAGMA journal_mode = WAL; PRAGMA foreign_keys = ON; PRAGMA synchronous = NORMAL;",
                      NULL, NULL, &errmsg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[DB WARNING] Could not set WAL or foreign_keys: %s\n", errmsg ? errmsg : "unknown");
        sqlite3_free(errmsg);
    }

    /* Execute migrations */
    DbResult mig_res = db_run_migrations();
    if (mig_res != DB_SUCCESS) {
        fprintf(stderr, "[DB ERROR] Migration failed during initialization.\n");
        return mig_res;
    }

    db_log_activity(1, "SYSTEM_START", "DATABASE", "Database initialized and verified successfully.");
    return DB_SUCCESS;
}

void db_close(void)
{
    if (s_db != NULL) {
        sqlite3_close_v2(s_db);
        s_db = NULL;
    }
}

sqlite3 *db_get_handle(void)
{
    return s_db;
}

int db_get_version(void)
{
    if (s_db == NULL) return -1;

    sqlite3_stmt *stmt = NULL;
    int version = -1;
    int rc = sqlite3_prepare_v2(s_db, "PRAGMA user_version;", -1, &stmt, NULL);
    if (rc == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            version = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }
    return version;
}

static DbResult set_db_version(int version)
{
    char sql[64];
    snprintf(sql, sizeof(sql), "PRAGMA user_version = %d;", version);
    char *errmsg = NULL;
    int rc = sqlite3_exec(s_db, sql, NULL, NULL, &errmsg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[DB ERROR] Failed to set user_version: %s\n", errmsg ? errmsg : "unknown");
        sqlite3_free(errmsg);
        return DB_ERROR_EXEC;
    }
    return DB_SUCCESS;
}

DbResult db_run_migrations(void)
{
    if (s_db == NULL) return DB_ERROR_OPEN;

    int current_version = db_get_version();
    if (current_version < 0) {
        return DB_ERROR_EXEC;
    }

    if (current_version < 1) {
        /* Run Migration 1 */
        DbResult tx_res = db_begin_transaction();
        if (tx_res != DB_SUCCESS) return tx_res;

        char *errmsg = NULL;
        int rc = sqlite3_exec(s_db, SCHEMA_V1, NULL, NULL, &errmsg);
        if (rc != SQLITE_OK) {
            fprintf(stderr, "[DB ERROR] Migration V1 schema failed: %s\n", errmsg ? errmsg : "unknown");
            sqlite3_free(errmsg);
            db_rollback();
            return DB_ERROR_MIGRATION;
        }

        /* Insert seed data */
        rc = sqlite3_exec(s_db, SEED_DATA, NULL, NULL, &errmsg);
        if (rc != SQLITE_OK) {
            fprintf(stderr, "[DB ERROR] Migration V1 seed data failed: %s\n", errmsg ? errmsg : "unknown");
            sqlite3_free(errmsg);
            db_rollback();
            return DB_ERROR_MIGRATION;
        }

        if (set_db_version(2) != DB_SUCCESS) {
            db_rollback();
            return DB_ERROR_MIGRATION;
        }

        db_commit();
    } else if (current_version < 2) {
        /* Run Migration 2 for existing v1 databases */
        DbResult tx_res = db_begin_transaction();
        if (tx_res != DB_SUCCESS) return tx_res;

        static const char *MIGRATION_V2 =
            "CREATE TABLE IF NOT EXISTS role_permissions (\n"
            "    id INTEGER PRIMARY KEY AUTOINCREMENT,\n"
            "    role_id INTEGER NOT NULL,\n"
            "    module_name TEXT NOT NULL,\n"
            "    can_view INTEGER NOT NULL DEFAULT 1,\n"
            "    can_edit INTEGER NOT NULL DEFAULT 0,\n"
            "    FOREIGN KEY (role_id) REFERENCES roles(id) ON DELETE CASCADE,\n"
            "    UNIQUE(role_id, module_name)\n"
            ");\n"
            "CREATE INDEX IF NOT EXISTS idx_role_permissions_role ON role_permissions(role_id);\n"
            "INSERT OR IGNORE INTO settings (key, value, description) VALUES\n"
            "    ('smtp_host', 'smtp.gmail.com', 'Outgoing SMTP server hostname'),\n"
            "    ('smtp_port', '587', 'SMTP server port (587 for TLS, 465 for SSL)'),\n"
            "    ('smtp_username', 'school.admin@example.com', 'SMTP authentication username'),\n"
            "    ('smtp_password', '', 'SMTP authentication password or app token'),\n"
            "    ('smtp_encryption', 'STARTTLS', 'SMTP encryption protocol: STARTTLS, SSL, or NONE'),\n"
            "    ('smtp_sender_name', 'School Administration', 'Outgoing email sender display name'),\n"
            "    ('smtp_sender_email', 'noreply@schoolcms.edu', 'Outgoing email from address');\n"
            "INSERT OR IGNORE INTO role_permissions (role_id, module_name, can_view, can_edit) VALUES\n"
            "    (1, 'STUDENTS', 1, 1), (1, 'TEACHERS', 1, 1), (1, 'CLASSES', 1, 1),\n"
            "    (1, 'ATTENDANCE', 1, 1), (1, 'FEES', 1, 1), (1, 'EXAMS', 1, 1),\n"
            "    (1, 'TIMETABLE', 1, 1), (1, 'REPORTS', 1, 1), (1, 'SETTINGS', 1, 1),\n"
            "    (2, 'STUDENTS', 1, 0), (2, 'TEACHERS', 1, 0), (2, 'CLASSES', 1, 0),\n"
            "    (2, 'ATTENDANCE', 1, 1), (2, 'FEES', 0, 0), (2, 'EXAMS', 1, 1),\n"
            "    (2, 'TIMETABLE', 1, 0), (2, 'REPORTS', 1, 0), (2, 'SETTINGS', 0, 0),\n"
            "    (3, 'STUDENTS', 1, 0), (3, 'TEACHERS', 0, 0), (3, 'CLASSES', 1, 0),\n"
            "    (3, 'ATTENDANCE', 1, 0), (3, 'FEES', 1, 1), (3, 'EXAMS', 0, 0),\n"
            "    (3, 'TIMETABLE', 0, 0), (3, 'REPORTS', 1, 1), (3, 'SETTINGS', 0, 0),\n"
            "    (4, 'STUDENTS', 1, 0), (4, 'TEACHERS', 1, 0), (4, 'CLASSES', 1, 0),\n"
            "    (4, 'ATTENDANCE', 1, 0), (4, 'FEES', 0, 0), (4, 'EXAMS', 0, 0),\n"
            "    (4, 'TIMETABLE', 1, 0), (4, 'REPORTS', 0, 0), (4, 'SETTINGS', 0, 0);\n";

        char *errmsg = NULL;
        int rc = sqlite3_exec(s_db, MIGRATION_V2, NULL, NULL, &errmsg);
        if (rc != SQLITE_OK) {
            fprintf(stderr, "[DB ERROR] Migration V2 failed: %s\n", errmsg ? errmsg : "unknown");
            sqlite3_free(errmsg);
            db_rollback();
            return DB_ERROR_MIGRATION;
        }

        if (set_db_version(2) != DB_SUCCESS) {
            db_rollback();
            return DB_ERROR_MIGRATION;
        }

        db_commit();
    }

    return DB_SUCCESS;
}

DbResult db_begin_transaction(void)
{
    if (s_db == NULL) return DB_ERROR_OPEN;
    char *errmsg = NULL;
    int rc = sqlite3_exec(s_db, "BEGIN IMMEDIATE TRANSACTION;", NULL, NULL, &errmsg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[DB ERROR] BEGIN TRANSACTION failed: %s\n", errmsg ? errmsg : "unknown");
        sqlite3_free(errmsg);
        return DB_ERROR_EXEC;
    }
    return DB_SUCCESS;
}

DbResult db_commit(void)
{
    if (s_db == NULL) return DB_ERROR_OPEN;
    char *errmsg = NULL;
    int rc = sqlite3_exec(s_db, "COMMIT;", NULL, NULL, &errmsg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[DB ERROR] COMMIT failed: %s\n", errmsg ? errmsg : "unknown");
        sqlite3_free(errmsg);
        return DB_ERROR_EXEC;
    }
    return DB_SUCCESS;
}

DbResult db_rollback(void)
{
    if (s_db == NULL) return DB_ERROR_OPEN;
    char *errmsg = NULL;
    int rc = sqlite3_exec(s_db, "ROLLBACK;", NULL, NULL, &errmsg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[DB ERROR] ROLLBACK failed: %s\n", errmsg ? errmsg : "unknown");
        sqlite3_free(errmsg);
        return DB_ERROR_EXEC;
    }
    return DB_SUCCESS;
}

DbResult db_backup(const char *dest_path)
{
    if (s_db == NULL || dest_path == NULL) return DB_ERROR_PARAM;

    sqlite3 *pDest = NULL;
    int rc = sqlite3_open_v2(dest_path, &pDest,
                             SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[DB BACKUP ERROR] Could not open destination file '%s'\n", dest_path);
        if (pDest) sqlite3_close_v2(pDest);
        return DB_ERROR_BACKUP;
    }

    sqlite3_backup *pBackup = sqlite3_backup_init(pDest, "main", s_db, "main");
    if (!pBackup) {
        fprintf(stderr, "[DB BACKUP ERROR] Failed to initialize backup: %s\n", sqlite3_errmsg(pDest));
        sqlite3_close_v2(pDest);
        return DB_ERROR_BACKUP;
    }

    rc = sqlite3_backup_step(pBackup, -1);
    sqlite3_backup_finish(pBackup);

    sqlite3_close_v2(pDest);

    if (rc != SQLITE_DONE) {
        fprintf(stderr, "[DB BACKUP ERROR] Backup step failed with code %d\n", rc);
        return DB_ERROR_BACKUP;
    }

    db_log_activity(1, "BACKUP", "DATABASE", dest_path);
    return DB_SUCCESS;
}

DbResult db_restore(const char *src_path)
{
    if (s_db == NULL || src_path == NULL) return DB_ERROR_PARAM;

    sqlite3 *pSrc = NULL;
    int rc = sqlite3_open_v2(src_path, &pSrc, SQLITE_OPEN_READONLY, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[DB RESTORE ERROR] Could not open source backup file '%s'\n", src_path);
        if (pSrc) sqlite3_close_v2(pSrc);
        return DB_ERROR_RESTORE;
    }

    sqlite3_backup *pBackup = sqlite3_backup_init(s_db, "main", pSrc, "main");
    if (!pBackup) {
        fprintf(stderr, "[DB RESTORE ERROR] Failed to initialize restore: %s\n", sqlite3_errmsg(s_db));
        sqlite3_close_v2(pSrc);
        return DB_ERROR_RESTORE;
    }

    rc = sqlite3_backup_step(pBackup, -1);
    sqlite3_backup_finish(pBackup);

    sqlite3_close_v2(pSrc);

    if (rc != SQLITE_DONE) {
        fprintf(stderr, "[DB RESTORE ERROR] Restore step failed with code %d\n", rc);
        return DB_ERROR_RESTORE;
    }

    db_log_activity(1, "RESTORE", "DATABASE", src_path);
    return DB_SUCCESS;
}

DbResult db_log_activity(int user_id, const char *action, const char *module, const char *details)
{
    if (s_db == NULL || action == NULL || module == NULL) return DB_ERROR_PARAM;

    const char *sql = "INSERT INTO activity_logs (user_id, action, module, details) VALUES (?, ?, ?, ?);";
    sqlite3_stmt *stmt = NULL;

    int rc = sqlite3_prepare_v2(s_db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        return DB_ERROR_PREPARE;
    }

    if (user_id > 0) {
        sqlite3_bind_int(stmt, 1, user_id);
    } else {
        sqlite3_bind_null(stmt, 1);
    }

    sqlite3_bind_text(stmt, 2, action, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, module, -1, SQLITE_STATIC);
    if (details != NULL) {
        sqlite3_bind_text(stmt, 4, details, -1, SQLITE_STATIC);
    } else {
        sqlite3_bind_null(stmt, 4);
    }

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return (rc == SQLITE_DONE) ? DB_SUCCESS : DB_ERROR_STEP;
}
