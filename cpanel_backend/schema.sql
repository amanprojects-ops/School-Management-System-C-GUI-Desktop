-- ============================================================================
-- SchoolCMS (APCOACHING) — cPanel MySQL 5.7+ / MariaDB 10.3+ Sync Schema
-- Upload via phpMyAdmin (Import) after creating your database in cPanel.
-- All strings use utf8mb4 for full Unicode & Indian Rupee (₹) symbol support.
-- ============================================================================

SET NAMES utf8mb4;
SET time_zone = '+00:00';

-- ----------------------------------------------------------------------------
-- 1. Sync Infrastructure Tables
-- ----------------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS sync_devices (
    device_id VARCHAR(64) NOT NULL PRIMARY KEY,
    device_prefix VARCHAR(8) NOT NULL DEFAULT 'D',
    last_seen_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS sync_changelog (
    server_seq BIGINT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY,
    table_name VARCHAR(48) NOT NULL,
    record_uuid CHAR(36) NOT NULL,
    operation ENUM('UPSERT', 'DELETE') NOT NULL,
    origin_device_id VARCHAR(64) NOT NULL,
    updated_at_ms BIGINT NOT NULL,
    payload_json LONGTEXT NOT NULL,
    created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    KEY idx_changelog_seq_dev (server_seq, origin_device_id),
    KEY idx_changelog_uuid (table_name, record_uuid)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS sync_tombstones (
    id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY,
    table_name VARCHAR(48) NOT NULL,
    record_uuid CHAR(36) NOT NULL,
    origin_device_id VARCHAR(64) NOT NULL DEFAULT '',
    deleted_at_ms BIGINT NOT NULL,
    created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    UNIQUE KEY uk_tombstone_tbl_uuid (table_name, record_uuid)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- ----------------------------------------------------------------------------
-- 2. Mirrored Operational Tables (All 19 Synced Entities)
-- ----------------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS roles (
    sync_uuid CHAR(36) NOT NULL PRIMARY KEY,
    name VARCHAR(64) NOT NULL,
    description TEXT NULL,
    origin_device_id VARCHAR(64) NOT NULL DEFAULT '',
    updated_at_ms BIGINT NOT NULL DEFAULT 0,
    UNIQUE KEY uk_roles_name (name)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS users (
    sync_uuid CHAR(36) NOT NULL PRIMARY KEY,
    username VARCHAR(64) NOT NULL,
    password_hash VARCHAR(191) NOT NULL,
    salt VARCHAR(191) NOT NULL,
    role_uuid CHAR(36) NULL,
    role_name VARCHAR(64) NULL,
    full_name VARCHAR(128) NOT NULL,
    email VARCHAR(191) NULL,
    phone VARCHAR(32) NULL,
    status VARCHAR(16) NOT NULL DEFAULT 'ACTIVE',
    created_at VARCHAR(32) NULL,
    updated_at VARCHAR(32) NULL,
    origin_device_id VARCHAR(64) NOT NULL DEFAULT '',
    updated_at_ms BIGINT NOT NULL DEFAULT 0,
    UNIQUE KEY uk_users_username (username),
    UNIQUE KEY uk_users_email (email),
    KEY idx_users_role_uuid (role_uuid)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS academic_years (
    sync_uuid CHAR(36) NOT NULL PRIMARY KEY,
    name VARCHAR(64) NOT NULL,
    start_date VARCHAR(16) NOT NULL,
    end_date VARCHAR(16) NOT NULL,
    is_current TINYINT(1) NOT NULL DEFAULT 0,
    origin_device_id VARCHAR(64) NOT NULL DEFAULT '',
    updated_at_ms BIGINT NOT NULL DEFAULT 0,
    UNIQUE KEY uk_academic_years_name (name)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS classes (
    sync_uuid CHAR(36) NOT NULL PRIMARY KEY,
    name VARCHAR(64) NOT NULL,
    numeric_value INT NOT NULL DEFAULT 0,
    capacity INT NOT NULL DEFAULT 40,
    academic_year_uuid CHAR(36) NULL,
    academic_year_name VARCHAR(64) NULL,
    origin_device_id VARCHAR(64) NOT NULL DEFAULT '',
    updated_at_ms BIGINT NOT NULL DEFAULT 0,
    KEY idx_classes_ay_uuid (academic_year_uuid)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS sections (
    sync_uuid CHAR(36) NOT NULL PRIMARY KEY,
    class_uuid CHAR(36) NOT NULL,
    name VARCHAR(32) NOT NULL,
    class_teacher_uuid CHAR(36) NULL,
    origin_device_id VARCHAR(64) NOT NULL DEFAULT '',
    updated_at_ms BIGINT NOT NULL DEFAULT 0,
    UNIQUE KEY uk_sections_class_name (class_uuid, name)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS teachers (
    sync_uuid CHAR(36) NOT NULL PRIMARY KEY,
    teacher_code VARCHAR(32) NOT NULL,
    user_uuid CHAR(36) NULL,
    username VARCHAR(64) NULL,
    full_name VARCHAR(128) NOT NULL,
    phone VARCHAR(32) NOT NULL DEFAULT '',
    email VARCHAR(191) NULL,
    address TEXT NULL,
    qualification VARCHAR(128) NULL,
    subject VARCHAR(128) NULL,
    joining_date VARCHAR(16) NOT NULL,
    salary DOUBLE NOT NULL DEFAULT 0.0,
    status VARCHAR(16) NOT NULL DEFAULT 'ACTIVE',
    created_at VARCHAR(32) NULL,
    origin_device_id VARCHAR(64) NOT NULL DEFAULT '',
    updated_at_ms BIGINT NOT NULL DEFAULT 0,
    UNIQUE KEY uk_teachers_code (teacher_code),
    UNIQUE KEY uk_teachers_email (email)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS staff (
    sync_uuid CHAR(36) NOT NULL PRIMARY KEY,
    staff_code VARCHAR(32) NOT NULL,
    user_uuid CHAR(36) NULL,
    username VARCHAR(64) NULL,
    full_name VARCHAR(128) NOT NULL,
    role_title VARCHAR(64) NOT NULL DEFAULT 'Staff',
    phone VARCHAR(32) NOT NULL DEFAULT '',
    email VARCHAR(191) NULL,
    address TEXT NULL,
    joining_date VARCHAR(16) NOT NULL,
    salary DOUBLE NOT NULL DEFAULT 0.0,
    status VARCHAR(16) NOT NULL DEFAULT 'ACTIVE',
    created_at VARCHAR(32) NULL,
    origin_device_id VARCHAR(64) NOT NULL DEFAULT '',
    updated_at_ms BIGINT NOT NULL DEFAULT 0,
    UNIQUE KEY uk_staff_code (staff_code),
    UNIQUE KEY uk_staff_email (email)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS students (
    sync_uuid CHAR(36) NOT NULL PRIMARY KEY,
    admission_no VARCHAR(32) NOT NULL,
    full_name VARCHAR(128) NOT NULL,
    father_name VARCHAR(128) NULL,
    mother_name VARCHAR(128) NULL,
    dob VARCHAR(16) NULL,
    gender VARCHAR(16) NOT NULL DEFAULT 'OTHER',
    category VARCHAR(32) NULL,
    phone VARCHAR(32) NULL,
    email VARCHAR(191) NULL,
    address TEXT NULL,
    admission_date VARCHAR(16) NOT NULL,
    class_uuid CHAR(36) NULL,
    section_uuid CHAR(36) NULL,
    roll_number INT NULL,
    previous_school VARCHAR(128) NULL,
    emergency_contact VARCHAR(32) NULL,
    blood_group VARCHAR(16) NULL,
    photo_path VARCHAR(255) NULL,
    status VARCHAR(16) NOT NULL DEFAULT 'ACTIVE',
    created_at VARCHAR(32) NULL,
    updated_at VARCHAR(32) NULL,
    origin_device_id VARCHAR(64) NOT NULL DEFAULT '',
    updated_at_ms BIGINT NOT NULL DEFAULT 0,
    UNIQUE KEY uk_students_admission_no (admission_no),
    KEY idx_students_class_section (class_uuid, section_uuid)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS subjects (
    sync_uuid CHAR(36) NOT NULL PRIMARY KEY,
    code VARCHAR(32) NOT NULL,
    name VARCHAR(128) NOT NULL,
    class_uuid CHAR(36) NOT NULL,
    teacher_uuid CHAR(36) NULL,
    origin_device_id VARCHAR(64) NOT NULL DEFAULT '',
    updated_at_ms BIGINT NOT NULL DEFAULT 0,
    UNIQUE KEY uk_subjects_code_class (code, class_uuid)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS attendance (
    sync_uuid CHAR(36) NOT NULL PRIMARY KEY,
    student_uuid CHAR(36) NOT NULL,
    class_uuid CHAR(36) NULL,
    section_uuid CHAR(36) NULL,
    date VARCHAR(16) NOT NULL,
    status VARCHAR(16) NOT NULL DEFAULT 'PRESENT',
    remarks TEXT NULL,
    recorded_by_uuid CHAR(36) NULL,
    recorded_by_username VARCHAR(64) NULL,
    created_at VARCHAR(32) NULL,
    origin_device_id VARCHAR(64) NOT NULL DEFAULT '',
    updated_at_ms BIGINT NOT NULL DEFAULT 0,
    UNIQUE KEY uk_attendance_student_date (student_uuid, date),
    KEY idx_attendance_date_class (date, class_uuid)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS fee_structures (
    sync_uuid CHAR(36) NOT NULL PRIMARY KEY,
    name VARCHAR(128) NOT NULL,
    class_uuid CHAR(36) NULL,
    academic_year_uuid CHAR(36) NULL,
    academic_year_name VARCHAR(64) NULL,
    fee_type VARCHAR(32) NOT NULL DEFAULT 'OTHER',
    amount DOUBLE NOT NULL DEFAULT 0.0,
    due_date VARCHAR(16) NULL,
    origin_device_id VARCHAR(64) NOT NULL DEFAULT '',
    updated_at_ms BIGINT NOT NULL DEFAULT 0
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- Append-Only Financial Ledger: updates to existing receipts are rejected server-side
CREATE TABLE IF NOT EXISTS fee_payments (
    sync_uuid CHAR(36) NOT NULL PRIMARY KEY,
    receipt_no VARCHAR(32) NOT NULL,
    student_uuid CHAR(36) NOT NULL,
    fee_structure_uuid CHAR(36) NULL,
    amount_paid DOUBLE NOT NULL DEFAULT 0.0,
    discount DOUBLE NOT NULL DEFAULT 0.0,
    late_fine DOUBLE NOT NULL DEFAULT 0.0,
    total_amount DOUBLE NOT NULL DEFAULT 0.0,
    payment_method VARCHAR(32) NOT NULL DEFAULT 'CASH',
    transaction_ref VARCHAR(64) NULL,
    payment_date VARCHAR(16) NOT NULL,
    remarks TEXT NULL,
    collected_by_uuid CHAR(36) NULL,
    collected_by_username VARCHAR(64) NULL,
    created_at VARCHAR(32) NULL,
    origin_device_id VARCHAR(64) NOT NULL DEFAULT '',
    updated_at_ms BIGINT NOT NULL DEFAULT 0,
    UNIQUE KEY uk_fee_payments_receipt (receipt_no),
    KEY idx_fee_payments_student (student_uuid)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS examinations (
    sync_uuid CHAR(36) NOT NULL PRIMARY KEY,
    name VARCHAR(128) NOT NULL,
    academic_year_uuid CHAR(36) NULL,
    academic_year_name VARCHAR(64) NULL,
    start_date VARCHAR(16) NOT NULL,
    end_date VARCHAR(16) NOT NULL,
    grading_system VARCHAR(32) NULL DEFAULT 'PERCENTAGE',
    status VARCHAR(32) NOT NULL DEFAULT 'PLANNED',
    origin_device_id VARCHAR(64) NOT NULL DEFAULT '',
    updated_at_ms BIGINT NOT NULL DEFAULT 0
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS exam_subjects (
    sync_uuid CHAR(36) NOT NULL PRIMARY KEY,
    exam_uuid CHAR(36) NOT NULL,
    subject_uuid CHAR(36) NOT NULL,
    exam_date VARCHAR(16) NOT NULL,
    max_marks DOUBLE NOT NULL DEFAULT 100.0,
    passing_marks DOUBLE NOT NULL DEFAULT 35.0,
    origin_device_id VARCHAR(64) NOT NULL DEFAULT '',
    updated_at_ms BIGINT NOT NULL DEFAULT 0,
    UNIQUE KEY uk_exam_subjects_pair (exam_uuid, subject_uuid)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS exam_results (
    sync_uuid CHAR(36) NOT NULL PRIMARY KEY,
    exam_subject_uuid CHAR(36) NOT NULL,
    student_uuid CHAR(36) NOT NULL,
    marks_obtained DOUBLE NOT NULL DEFAULT 0.0,
    grade VARCHAR(16) NULL,
    remarks TEXT NULL,
    origin_device_id VARCHAR(64) NOT NULL DEFAULT '',
    updated_at_ms BIGINT NOT NULL DEFAULT 0,
    UNIQUE KEY uk_exam_results_pair (exam_subject_uuid, student_uuid)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS timetables (
    sync_uuid CHAR(36) NOT NULL PRIMARY KEY,
    class_uuid CHAR(36) NOT NULL,
    section_uuid CHAR(36) NOT NULL,
    day_of_week VARCHAR(16) NOT NULL,
    period_number INT NOT NULL,
    start_time VARCHAR(16) NOT NULL,
    end_time VARCHAR(16) NOT NULL,
    subject_uuid CHAR(36) NOT NULL,
    teacher_uuid CHAR(36) NULL,
    room_number VARCHAR(32) NULL,
    origin_device_id VARCHAR(64) NOT NULL DEFAULT '',
    updated_at_ms BIGINT NOT NULL DEFAULT 0,
    UNIQUE KEY uk_timetable_slot (class_uuid, section_uuid, day_of_week, period_number)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS notifications (
    sync_uuid CHAR(36) NOT NULL PRIMARY KEY,
    user_uuid CHAR(36) NULL,
    username VARCHAR(64) NULL,
    title VARCHAR(191) NOT NULL,
    message TEXT NOT NULL,
    is_read TINYINT(1) NOT NULL DEFAULT 0,
    created_at VARCHAR(32) NULL,
    origin_device_id VARCHAR(64) NOT NULL DEFAULT '',
    updated_at_ms BIGINT NOT NULL DEFAULT 0
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS settings (
    sync_uuid CHAR(36) NOT NULL PRIMARY KEY,
    `key` VARCHAR(128) NOT NULL,
    value TEXT NOT NULL,
    description TEXT NULL,
    updated_at VARCHAR(32) NULL,
    origin_device_id VARCHAR(64) NOT NULL DEFAULT '',
    updated_at_ms BIGINT NOT NULL DEFAULT 0,
    UNIQUE KEY uk_settings_key (`key`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS role_permissions (
    sync_uuid CHAR(36) NOT NULL PRIMARY KEY,
    role_uuid CHAR(36) NOT NULL,
    role_name VARCHAR(64) NULL,
    module_name VARCHAR(64) NOT NULL,
    can_view TINYINT(1) NOT NULL DEFAULT 1,
    can_edit TINYINT(1) NOT NULL DEFAULT 0,
    origin_device_id VARCHAR(64) NOT NULL DEFAULT '',
    updated_at_ms BIGINT NOT NULL DEFAULT 0,
    UNIQUE KEY uk_role_perms_pair (role_uuid, module_name)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
