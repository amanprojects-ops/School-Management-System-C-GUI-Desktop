
/**
 * @file sync_core.c
 * @brief Portable C11 + SQLite3 multi-device delta sync engine.
 *
 * Implements row-level FK-UUID serialization, outbox batching, Last-Write-Wins
 * remote delta resolution, append-only fee ledger protection, tombstone
 * deletions, and trigger-based anti-echo suppression.
 */

#include "sync/sync.h"
#include "database.h"

#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef __ANDROID__
#include <sys/time.h>
#endif

/* ── Portable String & Buffer Helpers ─────────────────────────────── */

static char *sync_strdup(const char *s)
{
    if (!s) return NULL;
    size_t len = strlen(s);
    char *copy = (char *)malloc(len + 1);
    if (!copy) return NULL;
    memcpy(copy, s, len + 1);
    return copy;
}

typedef struct {
    char *data;
    size_t len;
    size_t cap;
} DynBuf;

static bool dynbuf_init(DynBuf *b, size_t initial_cap)
{
    b->cap = initial_cap > 0 ? initial_cap : 256;
    b->data = (char *)malloc(b->cap);
    if (!b->data) {
        b->cap = 0;
        b->len = 0;
        return false;
    }
    b->data[0] = '\0';
    b->len = 0;
    return true;
}

static bool dynbuf_append(DynBuf *b, const char *str, size_t add_len)
{
    if (!b || !str) return false;
    if (b->len + add_len + 1 > b->cap) {
        size_t new_cap = b->cap * 2;
        while (new_cap < b->len + add_len + 1) {
            new_cap *= 2;
        }
        char *resized = (char *)realloc(b->data, new_cap);
        if (!resized) return false;
        b->data = resized;
        b->cap = new_cap;
    }
    memcpy(b->data + b->len, str, add_len);
    b->len += add_len;
    b->data[b->len] = '\0';
    return true;
}

static void dynbuf_free(DynBuf *b)
{
    if (b && b->data) {
        free(b->data);
        b->data = NULL;
        b->len = 0;
        b->cap = 0;
    }
}

/* ── Table Synchronization Descriptors (All 19 Core Tables) ───────── */

#define MAX_TABLE_FKS 4

typedef struct {
    const char *json_uuid_key;
    const char *target_table;
    const char *fallback_json_key;
    const char *fallback_col;
    bool not_null;
    bool allow_dangling_null;
} FkSpec;

typedef struct {
    const char *name;
    bool is_key_pk;         /* true only for settings(key PRIMARY KEY) */
    bool append_only;       /* true for fee_payments financial ledger */
    const char *serialize_sql;
    const char *nat_lookup_sql;
    const char *update_sql;
    const char *insert_sql;
    int fk_count;
    FkSpec fks[MAX_TABLE_FKS];
} SyncTableDef;

static const SyncTableDef SYNC_TABLE_DEFS[] = {
    /* 1. roles */
    {
        .name = "roles",
        .is_key_pk = false,
        .append_only = false,
        .serialize_sql =
            "SELECT json_object("
            " 'sync_uuid', t.sync_uuid, 'origin_device_id', t.origin_device_id,"
            " 'updated_at_ms', t.updated_at_ms,"
            " 'name', t.name, 'description', t.description"
            ") FROM roles t WHERE t.sync_uuid = ?1;",
        .nat_lookup_sql =
            "SELECT id, COALESCE(updated_at_ms, 0) FROM roles "
            "WHERE name = json_extract(:json, '$.name') LIMIT 1;",
        .update_sql =
            "UPDATE roles SET "
            " name = COALESCE(json_extract(:json, '$.name'), name),"
            " description = json_extract(:json, '$.description'),"
            " sync_uuid = :uuid, origin_device_id = :dev, updated_at_ms = :ts, deleted_at_ms = 0 "
            "WHERE id = :pk;",
        .insert_sql =
            "INSERT INTO roles (name, description, sync_uuid, origin_device_id, updated_at_ms, deleted_at_ms) "
            "VALUES (COALESCE(json_extract(:json, '$.name'), ''), json_extract(:json, '$.description'),"
            " :uuid, :dev, :ts, 0);",
        .fk_count = 0
    },
    /* 2. users */
    {
        .name = "users",
        .is_key_pk = false,
        .append_only = false,
        .serialize_sql =
            "SELECT json_object("
            " 'sync_uuid', t.sync_uuid, 'origin_device_id', t.origin_device_id,"
            " 'updated_at_ms', t.updated_at_ms,"
            " 'username', t.username, 'password_hash', t.password_hash, 'salt', t.salt,"
            " 'role_uuid', (SELECT r.sync_uuid FROM roles r WHERE r.id = t.role_id),"
            " 'role_name', (SELECT r.name FROM roles r WHERE r.id = t.role_id),"
            " 'full_name', t.full_name, 'email', t.email, 'phone', t.phone,"
            " 'status', t.status, 'created_at', t.created_at, 'updated_at', t.updated_at"
            ") FROM users t WHERE t.sync_uuid = ?1;",
        .nat_lookup_sql =
            "SELECT id, COALESCE(updated_at_ms, 0) FROM users "
            "WHERE username = json_extract(:json, '$.username') "
            "OR (json_extract(:json, '$.email') IS NOT NULL "
            "    AND json_extract(:json, '$.email') != '' "
            "    AND email = json_extract(:json, '$.email')) LIMIT 1;",
        .update_sql =
            "UPDATE users SET "
            " username = COALESCE(json_extract(:json, '$.username'), username),"
            " password_hash = COALESCE(json_extract(:json, '$.password_hash'), password_hash),"
            " salt = COALESCE(json_extract(:json, '$.salt'), salt),"
            " role_id = :fk0,"
            " full_name = COALESCE(json_extract(:json, '$.full_name'), full_name),"
            " email = json_extract(:json, '$.email'),"
            " phone = json_extract(:json, '$.phone'),"
            " status = COALESCE(json_extract(:json, '$.status'), status),"
            " updated_at = COALESCE(json_extract(:json, '$.updated_at'), CURRENT_TIMESTAMP),"
            " sync_uuid = :uuid, origin_device_id = :dev, updated_at_ms = :ts, deleted_at_ms = 0 "
            "WHERE id = :pk;",
        .insert_sql =
            "INSERT INTO users (username, password_hash, salt, role_id, full_name, email, phone,"
            " status, created_at, updated_at, sync_uuid, origin_device_id, updated_at_ms, deleted_at_ms) "
            "VALUES (COALESCE(json_extract(:json, '$.username'), ''),"
            " COALESCE(json_extract(:json, '$.password_hash'), ''),"
            " COALESCE(json_extract(:json, '$.salt'), ''), :fk0,"
            " COALESCE(json_extract(:json, '$.full_name'), ''),"
            " json_extract(:json, '$.email'), json_extract(:json, '$.phone'),"
            " COALESCE(json_extract(:json, '$.status'), 'ACTIVE'),"
            " COALESCE(json_extract(:json, '$.created_at'), CURRENT_TIMESTAMP),"
            " COALESCE(json_extract(:json, '$.updated_at'), CURRENT_TIMESTAMP),"
            " :uuid, :dev, :ts, 0);",
        .fk_count = 1,
        .fks = {
            { "role_uuid", "roles", "role_name", "name", true, false }
        }
    },
    /* 3. academic_years */
    {
        .name = "academic_years",
        .is_key_pk = false,
        .append_only = false,
        .serialize_sql =
            "SELECT json_object("
            " 'sync_uuid', t.sync_uuid, 'origin_device_id', t.origin_device_id,"
            " 'updated_at_ms', t.updated_at_ms,"
            " 'name', t.name, 'start_date', t.start_date,"
            " 'end_date', t.end_date, 'is_current', t.is_current"
            ") FROM academic_years t WHERE t.sync_uuid = ?1;",
        .nat_lookup_sql =
            "SELECT id, COALESCE(updated_at_ms, 0) FROM academic_years "
            "WHERE name = json_extract(:json, '$.name') LIMIT 1;",
        .update_sql =
            "UPDATE academic_years SET "
            " name = COALESCE(json_extract(:json, '$.name'), name),"
            " start_date = COALESCE(json_extract(:json, '$.start_date'), start_date),"
            " end_date = COALESCE(json_extract(:json, '$.end_date'), end_date),"
            " is_current = COALESCE(json_extract(:json, '$.is_current'), is_current),"
            " sync_uuid = :uuid, origin_device_id = :dev, updated_at_ms = :ts, deleted_at_ms = 0 "
            "WHERE id = :pk;",
        .insert_sql =
            "INSERT INTO academic_years (name, start_date, end_date, is_current,"
            " sync_uuid, origin_device_id, updated_at_ms, deleted_at_ms) "
            "VALUES (COALESCE(json_extract(:json, '$.name'), ''),"
            " COALESCE(json_extract(:json, '$.start_date'), ''),"
            " COALESCE(json_extract(:json, '$.end_date'), ''),"
            " COALESCE(json_extract(:json, '$.is_current'), 0),"
            " :uuid, :dev, :ts, 0);",
        .fk_count = 0
    },
    /* 4. classes */
    {
        .name = "classes",
        .is_key_pk = false,
        .append_only = false,
        .serialize_sql =
            "SELECT json_object("
            " 'sync_uuid', t.sync_uuid, 'origin_device_id', t.origin_device_id,"
            " 'updated_at_ms', t.updated_at_ms,"
            " 'name', t.name, 'numeric_value', t.numeric_value, 'capacity', t.capacity,"
            " 'academic_year_uuid', (SELECT ay.sync_uuid FROM academic_years ay WHERE ay.id = t.academic_year_id),"
            " 'academic_year_name', (SELECT ay.name FROM academic_years ay WHERE ay.id = t.academic_year_id)"
            ") FROM classes t WHERE t.sync_uuid = ?1;",
        .nat_lookup_sql = NULL,
        .update_sql =
            "UPDATE classes SET "
            " name = COALESCE(json_extract(:json, '$.name'), name),"
            " numeric_value = json_extract(:json, '$.numeric_value'),"
            " capacity = COALESCE(json_extract(:json, '$.capacity'), capacity),"
            " academic_year_id = :fk0,"
            " sync_uuid = :uuid, origin_device_id = :dev, updated_at_ms = :ts, deleted_at_ms = 0 "
            "WHERE id = :pk;",
        .insert_sql =
            "INSERT INTO classes (name, numeric_value, capacity, academic_year_id,"
            " sync_uuid, origin_device_id, updated_at_ms, deleted_at_ms) "
            "VALUES (COALESCE(json_extract(:json, '$.name'), ''),"
            " COALESCE(json_extract(:json, '$.numeric_value'), 0),"
            " COALESCE(json_extract(:json, '$.capacity'), 40), :fk0,"
            " :uuid, :dev, :ts, 0);",
        .fk_count = 1,
        .fks = {
            { "academic_year_uuid", "academic_years", "academic_year_name", "name", false, false }
        }
    },
    /* 5. sections */
    {
        .name = "sections",
        .is_key_pk = false,
        .append_only = false,
        .serialize_sql =
            "SELECT json_object("
            " 'sync_uuid', t.sync_uuid, 'origin_device_id', t.origin_device_id,"
            " 'updated_at_ms', t.updated_at_ms,"
            " 'class_uuid', (SELECT c.sync_uuid FROM classes c WHERE c.id = t.class_id),"
            " 'name', t.name,"
            " 'class_teacher_uuid', (SELECT th.sync_uuid FROM teachers th WHERE th.id = t.class_teacher_id)"
            ") FROM sections t WHERE t.sync_uuid = ?1;",
        .nat_lookup_sql =
            "SELECT id, COALESCE(updated_at_ms, 0) FROM sections "
            "WHERE class_id = :fk0 AND name = json_extract(:json, '$.name') LIMIT 1;",
        .update_sql =
            "UPDATE sections SET "
            " class_id = :fk0,"
            " name = COALESCE(json_extract(:json, '$.name'), name),"
            " class_teacher_id = :fk1,"
            " sync_uuid = :uuid, origin_device_id = :dev, updated_at_ms = :ts, deleted_at_ms = 0 "
            "WHERE id = :pk;",
        .insert_sql =
            "INSERT INTO sections (class_id, name, class_teacher_id,"
            " sync_uuid, origin_device_id, updated_at_ms, deleted_at_ms) "
            "VALUES (:fk0, COALESCE(json_extract(:json, '$.name'), ''), :fk1,"
            " :uuid, :dev, :ts, 0);",
        .fk_count = 2,
        .fks = {
            { "class_uuid", "classes", NULL, NULL, true, false },
            { "class_teacher_uuid", "teachers", NULL, NULL, false, true }
        }
    },
    /* 6. teachers */
    {
        .name = "teachers",
        .is_key_pk = false,
        .append_only = false,
        .serialize_sql =
            "SELECT json_object("
            " 'sync_uuid', t.sync_uuid, 'origin_device_id', t.origin_device_id,"
            " 'updated_at_ms', t.updated_at_ms,"
            " 'teacher_code', t.teacher_code,"
            " 'user_uuid', (SELECT u.sync_uuid FROM users u WHERE u.id = t.user_id),"
            " 'username', (SELECT u.username FROM users u WHERE u.id = t.user_id),"
            " 'full_name', t.full_name, 'phone', t.phone, 'email', t.email,"
            " 'address', t.address, 'qualification', t.qualification, 'subject', t.subject,"
            " 'joining_date', t.joining_date, 'salary', t.salary, 'status', t.status,"
            " 'created_at', t.created_at"
            ") FROM teachers t WHERE t.sync_uuid = ?1;",
        .nat_lookup_sql =
            "SELECT id, COALESCE(updated_at_ms, 0) FROM teachers "
            "WHERE teacher_code = json_extract(:json, '$.teacher_code') "
            "OR (json_extract(:json, '$.email') IS NOT NULL "
            "    AND json_extract(:json, '$.email') != '' "
            "    AND email = json_extract(:json, '$.email')) LIMIT 1;",
        .update_sql =
            "UPDATE teachers SET "
            " teacher_code = COALESCE(json_extract(:json, '$.teacher_code'), teacher_code),"
            " user_id = :fk0,"
            " full_name = COALESCE(json_extract(:json, '$.full_name'), full_name),"
            " phone = COALESCE(json_extract(:json, '$.phone'), phone),"
            " email = json_extract(:json, '$.email'),"
            " address = json_extract(:json, '$.address'), qualification = json_extract(:json, '$.qualification'),"
            " subject = json_extract(:json, '$.subject'),"
            " joining_date = COALESCE(json_extract(:json, '$.joining_date'), joining_date),"
            " salary = COALESCE(json_extract(:json, '$.salary'), salary),"
            " status = COALESCE(json_extract(:json, '$.status'), status),"
            " sync_uuid = :uuid, origin_device_id = :dev, updated_at_ms = :ts, deleted_at_ms = 0 "
            "WHERE id = :pk;",
        .insert_sql =
            "INSERT INTO teachers (teacher_code, user_id, full_name, phone, email, address,"
            " qualification, subject, joining_date, salary, status, created_at,"
            " sync_uuid, origin_device_id, updated_at_ms, deleted_at_ms) "
            "VALUES (COALESCE(json_extract(:json, '$.teacher_code'), ''), :fk0,"
            " COALESCE(json_extract(:json, '$.full_name'), ''),"
            " COALESCE(json_extract(:json, '$.phone'), ''),"
            " json_extract(:json, '$.email'),"
            " json_extract(:json, '$.address'), json_extract(:json, '$.qualification'),"
            " json_extract(:json, '$.subject'),"
            " COALESCE(json_extract(:json, '$.joining_date'), date('now')),"
            " COALESCE(json_extract(:json, '$.salary'), 0.0),"
            " COALESCE(json_extract(:json, '$.status'), 'ACTIVE'),"
            " COALESCE(json_extract(:json, '$.created_at'), CURRENT_TIMESTAMP),"
            " :uuid, :dev, :ts, 0);",
        .fk_count = 1,
        .fks = {
            { "user_uuid", "users", "username", "username", false, true }
        }
    },
    /* 7. staff */
    {
        .name = "staff",
        .is_key_pk = false,
        .append_only = false,
        .serialize_sql =
            "SELECT json_object("
            " 'sync_uuid', t.sync_uuid, 'origin_device_id', t.origin_device_id,"
            " 'updated_at_ms', t.updated_at_ms,"
            " 'staff_code', t.staff_code,"
            " 'user_uuid', (SELECT u.sync_uuid FROM users u WHERE u.id = t.user_id),"
            " 'username', (SELECT u.username FROM users u WHERE u.id = t.user_id),"
            " 'full_name', t.full_name, 'role_title', t.role_title, 'phone', t.phone,"
            " 'email', t.email, 'address', t.address, 'joining_date', t.joining_date,"
            " 'salary', t.salary, 'status', t.status, 'created_at', t.created_at"
            ") FROM staff t WHERE t.sync_uuid = ?1;",
        .nat_lookup_sql =
            "SELECT id, COALESCE(updated_at_ms, 0) FROM staff "
            "WHERE staff_code = json_extract(:json, '$.staff_code') "
            "OR (json_extract(:json, '$.email') IS NOT NULL "
            "    AND json_extract(:json, '$.email') != '' "
            "    AND email = json_extract(:json, '$.email')) LIMIT 1;",
        .update_sql =
            "UPDATE staff SET "
            " staff_code = COALESCE(json_extract(:json, '$.staff_code'), staff_code),"
            " user_id = :fk0,"
            " full_name = COALESCE(json_extract(:json, '$.full_name'), full_name),"
            " role_title = COALESCE(json_extract(:json, '$.role_title'), role_title),"
            " phone = COALESCE(json_extract(:json, '$.phone'), phone),"
            " email = json_extract(:json, '$.email'),"
            " address = json_extract(:json, '$.address'),"
            " joining_date = COALESCE(json_extract(:json, '$.joining_date'), joining_date),"
            " salary = COALESCE(json_extract(:json, '$.salary'), salary),"
            " status = COALESCE(json_extract(:json, '$.status'), status),"
            " sync_uuid = :uuid, origin_device_id = :dev, updated_at_ms = :ts, deleted_at_ms = 0 "
            "WHERE id = :pk;",
        .insert_sql =
            "INSERT INTO staff (staff_code, user_id, full_name, role_title, phone, email, address,"
            " joining_date, salary, status, created_at,"
            " sync_uuid, origin_device_id, updated_at_ms, deleted_at_ms) "
            "VALUES (COALESCE(json_extract(:json, '$.staff_code'), ''), :fk0,"
            " COALESCE(json_extract(:json, '$.full_name'), ''),"
            " COALESCE(json_extract(:json, '$.role_title'), 'Staff'),"
            " COALESCE(json_extract(:json, '$.phone'), ''),"
            " json_extract(:json, '$.email'),"
            " json_extract(:json, '$.address'),"
            " COALESCE(json_extract(:json, '$.joining_date'), date('now')),"
            " COALESCE(json_extract(:json, '$.salary'), 0.0),"
            " COALESCE(json_extract(:json, '$.status'), 'ACTIVE'),"
            " COALESCE(json_extract(:json, '$.created_at'), CURRENT_TIMESTAMP),"
            " :uuid, :dev, :ts, 0);",
        .fk_count = 1,
        .fks = {
            { "user_uuid", "users", "username", "username", false, true }
        }
    },
    /* 8. students */
    {
        .name = "students",
        .is_key_pk = false,
        .append_only = false,
        .serialize_sql =
            "SELECT json_object("
            " 'sync_uuid', t.sync_uuid, 'origin_device_id', t.origin_device_id,"
            " 'updated_at_ms', t.updated_at_ms,"
            " 'admission_no', t.admission_no, 'full_name', t.full_name,"
            " 'father_name', t.father_name, 'mother_name', t.mother_name,"
            " 'dob', t.dob, 'gender', t.gender, 'category', t.category,"
            " 'phone', t.phone, 'email', t.email, 'address', t.address,"
            " 'admission_date', t.admission_date,"
            " 'class_uuid', (SELECT c.sync_uuid FROM classes c WHERE c.id = t.class_id),"
            " 'section_uuid', (SELECT s.sync_uuid FROM sections s WHERE s.id = t.section_id),"
            " 'roll_number', t.roll_number, 'previous_school', t.previous_school,"
            " 'emergency_contact', t.emergency_contact, 'blood_group', t.blood_group,"
            " 'photo_path', t.photo_path, 'status', t.status,"
            " 'created_at', t.created_at, 'updated_at', t.updated_at"
            ") FROM students t WHERE t.sync_uuid = ?1;",
        .nat_lookup_sql =
            "SELECT id, COALESCE(updated_at_ms, 0) FROM students "
            "WHERE admission_no = json_extract(:json, '$.admission_no') LIMIT 1;",
        .update_sql =
            "UPDATE students SET "
            " admission_no = COALESCE(json_extract(:json, '$.admission_no'), admission_no),"
            " full_name = COALESCE(json_extract(:json, '$.full_name'), full_name),"
            " father_name = json_extract(:json, '$.father_name'),"
            " mother_name = json_extract(:json, '$.mother_name'),"
            " dob = json_extract(:json, '$.dob'),"
            " gender = COALESCE(json_extract(:json, '$.gender'), gender),"
            " category = json_extract(:json, '$.category'), phone = json_extract(:json, '$.phone'),"
            " email = json_extract(:json, '$.email'), address = json_extract(:json, '$.address'),"
            " admission_date = COALESCE(json_extract(:json, '$.admission_date'), admission_date),"
            " class_id = :fk0, section_id = :fk1,"
            " roll_number = json_extract(:json, '$.roll_number'),"
            " previous_school = json_extract(:json, '$.previous_school'),"
            " emergency_contact = json_extract(:json, '$.emergency_contact'),"
            " blood_group = json_extract(:json, '$.blood_group'),"
            " photo_path = json_extract(:json, '$.photo_path'),"
            " status = COALESCE(json_extract(:json, '$.status'), status),"
            " updated_at = COALESCE(json_extract(:json, '$.updated_at'), CURRENT_TIMESTAMP),"
            " sync_uuid = :uuid, origin_device_id = :dev, updated_at_ms = :ts, deleted_at_ms = 0 "
            "WHERE id = :pk;",
        .insert_sql =
            "INSERT INTO students (admission_no, full_name, father_name, mother_name, dob, gender,"
            " category, phone, email, address, admission_date, class_id, section_id, roll_number,"
            " previous_school, emergency_contact, blood_group, photo_path, status, created_at, updated_at,"
            " sync_uuid, origin_device_id, updated_at_ms, deleted_at_ms) "
            "VALUES (COALESCE(json_extract(:json, '$.admission_no'), ''),"
            " COALESCE(json_extract(:json, '$.full_name'), ''),"
            " json_extract(:json, '$.father_name'), json_extract(:json, '$.mother_name'),"
            " json_extract(:json, '$.dob'),"
            " COALESCE(json_extract(:json, '$.gender'), 'OTHER'),"
            " json_extract(:json, '$.category'), json_extract(:json, '$.phone'),"
            " json_extract(:json, '$.email'), json_extract(:json, '$.address'),"
            " COALESCE(json_extract(:json, '$.admission_date'), date('now')),"
            " :fk0, :fk1,"
            " json_extract(:json, '$.roll_number'), json_extract(:json, '$.previous_school'),"
            " json_extract(:json, '$.emergency_contact'), json_extract(:json, '$.blood_group'),"
            " json_extract(:json, '$.photo_path'),"
            " COALESCE(json_extract(:json, '$.status'), 'ACTIVE'),"
            " COALESCE(json_extract(:json, '$.created_at'), CURRENT_TIMESTAMP),"
            " COALESCE(json_extract(:json, '$.updated_at'), CURRENT_TIMESTAMP),"
            " :uuid, :dev, :ts, 0);",
        .fk_count = 2,
        .fks = {
            { "class_uuid", "classes", NULL, NULL, true, false },
            { "section_uuid", "sections", NULL, NULL, true, false }
        }
    },
    /* 9. subjects */
    {
        .name = "subjects",
        .is_key_pk = false,
        .append_only = false,
        .serialize_sql =
            "SELECT json_object("
            " 'sync_uuid', t.sync_uuid, 'origin_device_id', t.origin_device_id,"
            " 'updated_at_ms', t.updated_at_ms,"
            " 'code', t.code, 'name', t.name,"
            " 'class_uuid', (SELECT c.sync_uuid FROM classes c WHERE c.id = t.class_id),"
            " 'teacher_uuid', (SELECT th.sync_uuid FROM teachers th WHERE th.id = t.teacher_id)"
            ") FROM subjects t WHERE t.sync_uuid = ?1;",
        .nat_lookup_sql =
            "SELECT id, COALESCE(updated_at_ms, 0) FROM subjects "
            "WHERE code = json_extract(:json, '$.code') AND class_id = :fk0 LIMIT 1;",
        .update_sql =
            "UPDATE subjects SET "
            " code = COALESCE(json_extract(:json, '$.code'), code),"
            " name = COALESCE(json_extract(:json, '$.name'), name),"
            " class_id = :fk0, teacher_id = :fk1,"
            " sync_uuid = :uuid, origin_device_id = :dev, updated_at_ms = :ts, deleted_at_ms = 0 "
            "WHERE id = :pk;",
        .insert_sql =
            "INSERT INTO subjects (code, name, class_id, teacher_id,"
            " sync_uuid, origin_device_id, updated_at_ms, deleted_at_ms) "
            "VALUES (COALESCE(json_extract(:json, '$.code'), ''),"
            " COALESCE(json_extract(:json, '$.name'), ''), :fk0, :fk1,"
            " :uuid, :dev, :ts, 0);",
        .fk_count = 2,
        .fks = {
            { "class_uuid", "classes", NULL, NULL, true, false },
            { "teacher_uuid", "teachers", NULL, NULL, false, true }
        }
    },
    /* 10. attendance */
    {
        .name = "attendance",
        .is_key_pk = false,
        .append_only = false,
        .serialize_sql =
            "SELECT json_object("
            " 'sync_uuid', t.sync_uuid, 'origin_device_id', t.origin_device_id,"
            " 'updated_at_ms', t.updated_at_ms,"
            " 'student_uuid', (SELECT st.sync_uuid FROM students st WHERE st.id = t.student_id),"
            " 'class_uuid', (SELECT c.sync_uuid FROM classes c WHERE c.id = t.class_id),"
            " 'section_uuid', (SELECT s.sync_uuid FROM sections s WHERE s.id = t.section_id),"
            " 'date', t.date, 'status', t.status, 'remarks', t.remarks,"
            " 'recorded_by_uuid', (SELECT u.sync_uuid FROM users u WHERE u.id = t.recorded_by),"
            " 'recorded_by_username', (SELECT u.username FROM users u WHERE u.id = t.recorded_by),"
            " 'created_at', t.created_at"
            ") FROM attendance t WHERE t.sync_uuid = ?1;",
        .nat_lookup_sql =
            "SELECT id, COALESCE(updated_at_ms, 0) FROM attendance "
            "WHERE student_id = :fk0 AND date = json_extract(:json, '$.date') LIMIT 1;",
        .update_sql =
            "UPDATE attendance SET "
            " student_id = :fk0, class_id = :fk1, section_id = :fk2,"
            " date = COALESCE(json_extract(:json, '$.date'), date),"
            " status = COALESCE(json_extract(:json, '$.status'), status),"
            " remarks = json_extract(:json, '$.remarks'),"
            " recorded_by = :fk3,"
            " sync_uuid = :uuid, origin_device_id = :dev, updated_at_ms = :ts, deleted_at_ms = 0 "
            "WHERE id = :pk;",
        .insert_sql =
            "INSERT INTO attendance (student_id, class_id, section_id, date, status, remarks,"
            " recorded_by, created_at, sync_uuid, origin_device_id, updated_at_ms, deleted_at_ms) "
            "VALUES (:fk0, :fk1, :fk2,"
            " COALESCE(json_extract(:json, '$.date'), ''),"
            " COALESCE(json_extract(:json, '$.status'), 'PRESENT'),"
            " json_extract(:json, '$.remarks'), :fk3,"
            " COALESCE(json_extract(:json, '$.created_at'), CURRENT_TIMESTAMP),"
            " :uuid, :dev, :ts, 0);",
        .fk_count = 4,
        .fks = {
            { "student_uuid", "students", NULL, NULL, true, false },
            { "class_uuid", "classes", NULL, NULL, true, false },
            { "section_uuid", "sections", NULL, NULL, true, false },
            { "recorded_by_uuid", "users", "recorded_by_username", "username", false, true }
        }
    },
    /* 11. fee_structures */
    {
        .name = "fee_structures",
        .is_key_pk = false,
        .append_only = false,
        .serialize_sql =
            "SELECT json_object("
            " 'sync_uuid', t.sync_uuid, 'origin_device_id', t.origin_device_id,"
            " 'updated_at_ms', t.updated_at_ms,"
            " 'name', t.name,"
            " 'class_uuid', (SELECT c.sync_uuid FROM classes c WHERE c.id = t.class_id),"
            " 'academic_year_uuid', (SELECT ay.sync_uuid FROM academic_years ay WHERE ay.id = t.academic_year_id),"
            " 'academic_year_name', (SELECT ay.name FROM academic_years ay WHERE ay.id = t.academic_year_id),"
            " 'fee_type', t.fee_type, 'amount', t.amount, 'due_date', t.due_date"
            ") FROM fee_structures t WHERE t.sync_uuid = ?1;",
        .nat_lookup_sql = NULL,
        .update_sql =
            "UPDATE fee_structures SET "
            " name = COALESCE(json_extract(:json, '$.name'), name),"
            " class_id = :fk0, academic_year_id = :fk1,"
            " fee_type = COALESCE(json_extract(:json, '$.fee_type'), fee_type),"
            " amount = COALESCE(json_extract(:json, '$.amount'), amount),"
            " due_date = json_extract(:json, '$.due_date'),"
            " sync_uuid = :uuid, origin_device_id = :dev, updated_at_ms = :ts, deleted_at_ms = 0 "
            "WHERE id = :pk;",
        .insert_sql =
            "INSERT INTO fee_structures (name, class_id, academic_year_id, fee_type, amount, due_date,"
            " sync_uuid, origin_device_id, updated_at_ms, deleted_at_ms) "
            "VALUES (COALESCE(json_extract(:json, '$.name'), ''), :fk0, :fk1,"
            " COALESCE(json_extract(:json, '$.fee_type'), 'OTHER'),"
            " COALESCE(json_extract(:json, '$.amount'), 0.0),"
            " json_extract(:json, '$.due_date'),"
            " :uuid, :dev, :ts, 0);",
        .fk_count = 2,
        .fks = {
            { "class_uuid", "classes", NULL, NULL, true, false },
            { "academic_year_uuid", "academic_years", "academic_year_name", "name", true, false }
        }
    },
    /* 12. fee_payments (append-only financial ledger) */
    {
        .name = "fee_payments",
        .is_key_pk = false,
        .append_only = true,
        .serialize_sql =
            "SELECT json_object("
            " 'sync_uuid', t.sync_uuid, 'origin_device_id', t.origin_device_id,"
            " 'updated_at_ms', t.updated_at_ms,"
            " 'receipt_no', t.receipt_no,"
            " 'student_uuid', (SELECT st.sync_uuid FROM students st WHERE st.id = t.student_id),"
            " 'fee_structure_uuid', (SELECT fs.sync_uuid FROM fee_structures fs WHERE fs.id = t.fee_structure_id),"
            " 'amount_paid', t.amount_paid, 'discount', t.discount, 'late_fine', t.late_fine,"
            " 'total_amount', t.total_amount, 'payment_method', t.payment_method,"
            " 'transaction_ref', t.transaction_ref, 'payment_date', t.payment_date,"
            " 'remarks', t.remarks,"
            " 'collected_by_uuid', (SELECT u.sync_uuid FROM users u WHERE u.id = t.collected_by),"
            " 'collected_by_username', (SELECT u.username FROM users u WHERE u.id = t.collected_by),"
            " 'created_at', t.created_at"
            ") FROM fee_payments t WHERE t.sync_uuid = ?1;",
        .nat_lookup_sql =
            "SELECT id, COALESCE(updated_at_ms, 0) FROM fee_payments "
            "WHERE receipt_no = json_extract(:json, '$.receipt_no') LIMIT 1;",
        .update_sql = NULL,
        .insert_sql =
            "INSERT INTO fee_payments (receipt_no, student_id, fee_structure_id,"
            " amount_paid, discount, late_fine, total_amount, payment_method,"
            " transaction_ref, payment_date, remarks, collected_by, created_at,"
            " sync_uuid, origin_device_id, updated_at_ms, deleted_at_ms) "
            "VALUES (COALESCE(json_extract(:json, '$.receipt_no'), ''), :fk0, :fk1,"
            " COALESCE(json_extract(:json, '$.amount_paid'), 0.0),"
            " COALESCE(json_extract(:json, '$.discount'), 0.0),"
            " COALESCE(json_extract(:json, '$.late_fine'), 0.0),"
            " COALESCE(json_extract(:json, '$.total_amount'), 0.0),"
            " COALESCE(json_extract(:json, '$.payment_method'), 'CASH'),"
            " json_extract(:json, '$.transaction_ref'),"
            " COALESCE(json_extract(:json, '$.payment_date'), date('now')),"
            " json_extract(:json, '$.remarks'), :fk2,"
            " COALESCE(json_extract(:json, '$.created_at'), CURRENT_TIMESTAMP),"
            " :uuid, :dev, :ts, 0);",
        .fk_count = 3,
        .fks = {
            { "student_uuid", "students", NULL, NULL, true, false },
            { "fee_structure_uuid", "fee_structures", NULL, NULL, true, false },
            { "collected_by_uuid", "users", "collected_by_username", "username", false, true }
        }
    },
    /* 13. examinations */
    {
        .name = "examinations",
        .is_key_pk = false,
        .append_only = false,
        .serialize_sql =
            "SELECT json_object("
            " 'sync_uuid', t.sync_uuid, 'origin_device_id', t.origin_device_id,"
            " 'updated_at_ms', t.updated_at_ms,"
            " 'name', t.name,"
            " 'academic_year_uuid', (SELECT ay.sync_uuid FROM academic_years ay WHERE ay.id = t.academic_year_id),"
            " 'academic_year_name', (SELECT ay.name FROM academic_years ay WHERE ay.id = t.academic_year_id),"
            " 'start_date', t.start_date, 'end_date', t.end_date,"
            " 'grading_system', t.grading_system, 'status', t.status"
            ") FROM examinations t WHERE t.sync_uuid = ?1;",
        .nat_lookup_sql = NULL,
        .update_sql =
            "UPDATE examinations SET "
            " name = COALESCE(json_extract(:json, '$.name'), name),"
            " academic_year_id = :fk0,"
            " start_date = COALESCE(json_extract(:json, '$.start_date'), start_date),"
            " end_date = COALESCE(json_extract(:json, '$.end_date'), end_date),"
            " grading_system = COALESCE(json_extract(:json, '$.grading_system'), grading_system),"
            " status = COALESCE(json_extract(:json, '$.status'), status),"
            " sync_uuid = :uuid, origin_device_id = :dev, updated_at_ms = :ts, deleted_at_ms = 0 "
            "WHERE id = :pk;",
        .insert_sql =
            "INSERT INTO examinations (name, academic_year_id, start_date, end_date,"
            " grading_system, status, sync_uuid, origin_device_id, updated_at_ms, deleted_at_ms) "
            "VALUES (COALESCE(json_extract(:json, '$.name'), ''), :fk0,"
            " COALESCE(json_extract(:json, '$.start_date'), date('now')),"
            " COALESCE(json_extract(:json, '$.end_date'), date('now')),"
            " COALESCE(json_extract(:json, '$.grading_system'), 'PERCENTAGE'),"
            " COALESCE(json_extract(:json, '$.status'), 'PLANNED'),"
            " :uuid, :dev, :ts, 0);",
        .fk_count = 1,
        .fks = {
            { "academic_year_uuid", "academic_years", "academic_year_name", "name", true, false }
        }
    },
    /* 14. exam_subjects */
    {
        .name = "exam_subjects",
        .is_key_pk = false,
        .append_only = false,
        .serialize_sql =
            "SELECT json_object("
            " 'sync_uuid', t.sync_uuid, 'origin_device_id', t.origin_device_id,"
            " 'updated_at_ms', t.updated_at_ms,"
            " 'exam_uuid', (SELECT e.sync_uuid FROM examinations e WHERE e.id = t.exam_id),"
            " 'subject_uuid', (SELECT sb.sync_uuid FROM subjects sb WHERE sb.id = t.subject_id),"
            " 'exam_date', t.exam_date, 'max_marks', t.max_marks, 'passing_marks', t.passing_marks"
            ") FROM exam_subjects t WHERE t.sync_uuid = ?1;",
        .nat_lookup_sql =
            "SELECT id, COALESCE(updated_at_ms, 0) FROM exam_subjects "
            "WHERE exam_id = :fk0 AND subject_id = :fk1 LIMIT 1;",
        .update_sql =
            "UPDATE exam_subjects SET "
            " exam_id = :fk0, subject_id = :fk1,"
            " exam_date = COALESCE(json_extract(:json, '$.exam_date'), exam_date),"
            " max_marks = COALESCE(json_extract(:json, '$.max_marks'), max_marks),"
            " passing_marks = COALESCE(json_extract(:json, '$.passing_marks'), passing_marks),"
            " sync_uuid = :uuid, origin_device_id = :dev, updated_at_ms = :ts, deleted_at_ms = 0 "
            "WHERE id = :pk;",
        .insert_sql =
            "INSERT INTO exam_subjects (exam_id, subject_id, exam_date, max_marks, passing_marks,"
            " sync_uuid, origin_device_id, updated_at_ms, deleted_at_ms) "
            "VALUES (:fk0, :fk1, COALESCE(json_extract(:json, '$.exam_date'), date('now')),"
            " COALESCE(json_extract(:json, '$.max_marks'), 100.0),"
            " COALESCE(json_extract(:json, '$.passing_marks'), 35.0),"
            " :uuid, :dev, :ts, 0);",
        .fk_count = 2,
        .fks = {
            { "exam_uuid", "examinations", NULL, NULL, true, false },
            { "subject_uuid", "subjects", NULL, NULL, true, false }
        }
    },
    /* 15. exam_results */
    {
        .name = "exam_results",
        .is_key_pk = false,
        .append_only = false,
        .serialize_sql =
            "SELECT json_object("
            " 'sync_uuid', t.sync_uuid, 'origin_device_id', t.origin_device_id,"
            " 'updated_at_ms', t.updated_at_ms,"
            " 'exam_subject_uuid', (SELECT es.sync_uuid FROM exam_subjects es WHERE es.id = t.exam_subject_id),"
            " 'student_uuid', (SELECT st.sync_uuid FROM students st WHERE st.id = t.student_id),"
            " 'marks_obtained', t.marks_obtained, 'grade', t.grade, 'remarks', t.remarks"
            ") FROM exam_results t WHERE t.sync_uuid = ?1;",
        .nat_lookup_sql =
            "SELECT id, COALESCE(updated_at_ms, 0) FROM exam_results "
            "WHERE exam_subject_id = :fk0 AND student_id = :fk1 LIMIT 1;",
        .update_sql =
            "UPDATE exam_results SET "
            " exam_subject_id = :fk0, student_id = :fk1,"
            " marks_obtained = COALESCE(json_extract(:json, '$.marks_obtained'), marks_obtained),"
            " grade = json_extract(:json, '$.grade'), remarks = json_extract(:json, '$.remarks'),"
            " sync_uuid = :uuid, origin_device_id = :dev, updated_at_ms = :ts, deleted_at_ms = 0 "
            "WHERE id = :pk;",
        .insert_sql =
            "INSERT INTO exam_results (exam_subject_id, student_id, marks_obtained, grade, remarks,"
            " sync_uuid, origin_device_id, updated_at_ms, deleted_at_ms) "
            "VALUES (:fk0, :fk1, COALESCE(json_extract(:json, '$.marks_obtained'), 0.0),"
            " json_extract(:json, '$.grade'), json_extract(:json, '$.remarks'),"
            " :uuid, :dev, :ts, 0);",
        .fk_count = 2,
        .fks = {
            { "exam_subject_uuid", "exam_subjects", NULL, NULL, true, false },
            { "student_uuid", "students", NULL, NULL, true, false }
        }
    },
    /* 16. timetables */
    {
        .name = "timetables",
        .is_key_pk = false,
        .append_only = false,
        .serialize_sql =
            "SELECT json_object("
            " 'sync_uuid', t.sync_uuid, 'origin_device_id', t.origin_device_id,"
            " 'updated_at_ms', t.updated_at_ms,"
            " 'class_uuid', (SELECT c.sync_uuid FROM classes c WHERE c.id = t.class_id),"
            " 'section_uuid', (SELECT s.sync_uuid FROM sections s WHERE s.id = t.section_id),"
            " 'day_of_week', t.day_of_week, 'period_number', t.period_number,"
            " 'start_time', t.start_time, 'end_time', t.end_time,"
            " 'subject_uuid', (SELECT sb.sync_uuid FROM subjects sb WHERE sb.id = t.subject_id),"
            " 'teacher_uuid', (SELECT th.sync_uuid FROM teachers th WHERE th.id = t.teacher_id),"
            " 'room_number', t.room_number"
            ") FROM timetables t WHERE t.sync_uuid = ?1;",
        .nat_lookup_sql =
            "SELECT id, COALESCE(updated_at_ms, 0) FROM timetables "
            "WHERE class_id = :fk0 AND section_id = :fk1 "
            "AND day_of_week = json_extract(:json, '$.day_of_week') "
            "AND period_number = json_extract(:json, '$.period_number') LIMIT 1;",
        .update_sql =
            "UPDATE timetables SET "
            " class_id = :fk0, section_id = :fk1,"
            " day_of_week = COALESCE(json_extract(:json, '$.day_of_week'), day_of_week),"
            " period_number = COALESCE(json_extract(:json, '$.period_number'), period_number),"
            " start_time = COALESCE(json_extract(:json, '$.start_time'), start_time),"
            " end_time = COALESCE(json_extract(:json, '$.end_time'), end_time),"
            " subject_id = :fk2, teacher_id = :fk3,"
            " room_number = json_extract(:json, '$.room_number'),"
            " sync_uuid = :uuid, origin_device_id = :dev, updated_at_ms = :ts, deleted_at_ms = 0 "
            "WHERE id = :pk;",
        .insert_sql =
            "INSERT INTO timetables (class_id, section_id, day_of_week, period_number,"
            " start_time, end_time, subject_id, teacher_id, room_number,"
            " sync_uuid, origin_device_id, updated_at_ms, deleted_at_ms) "
            "VALUES (:fk0, :fk1,"
            " COALESCE(json_extract(:json, '$.day_of_week'), 'MONDAY'),"
            " COALESCE(json_extract(:json, '$.period_number'), 1),"
            " COALESCE(json_extract(:json, '$.start_time'), '09:00'),"
            " COALESCE(json_extract(:json, '$.end_time'), '09:45'),"
            " :fk2, :fk3, json_extract(:json, '$.room_number'),"
            " :uuid, :dev, :ts, 0);",
        .fk_count = 4,
        .fks = {
            { "class_uuid", "classes", NULL, NULL, true, false },
            { "section_uuid", "sections", NULL, NULL, true, false },
            { "subject_uuid", "subjects", NULL, NULL, true, false },
            { "teacher_uuid", "teachers", NULL, NULL, true, false }
        }
    },
    /* 17. notifications */
    {
        .name = "notifications",
        .is_key_pk = false,
        .append_only = false,
        .serialize_sql =
            "SELECT json_object("
            " 'sync_uuid', t.sync_uuid, 'origin_device_id', t.origin_device_id,"
            " 'updated_at_ms', t.updated_at_ms,"
            " 'user_uuid', (SELECT u.sync_uuid FROM users u WHERE u.id = t.user_id),"
            " 'username', (SELECT u.username FROM users u WHERE u.id = t.user_id),"
            " 'title', t.title, 'message', t.message, 'is_read', t.is_read,"
            " 'created_at', t.created_at"
            ") FROM notifications t WHERE t.sync_uuid = ?1;",
        .nat_lookup_sql = NULL,
        .update_sql =
            "UPDATE notifications SET "
            " user_id = :fk0,"
            " title = COALESCE(json_extract(:json, '$.title'), title),"
            " message = COALESCE(json_extract(:json, '$.message'), message),"
            " is_read = COALESCE(json_extract(:json, '$.is_read'), is_read),"
            " sync_uuid = :uuid, origin_device_id = :dev, updated_at_ms = :ts, deleted_at_ms = 0 "
            "WHERE id = :pk;",
        .insert_sql =
            "INSERT INTO notifications (user_id, title, message, is_read, created_at,"
            " sync_uuid, origin_device_id, updated_at_ms, deleted_at_ms) "
            "VALUES (:fk0,"
            " COALESCE(json_extract(:json, '$.title'), ''),"
            " COALESCE(json_extract(:json, '$.message'), ''),"
            " COALESCE(json_extract(:json, '$.is_read'), 0),"
            " COALESCE(json_extract(:json, '$.created_at'), CURRENT_TIMESTAMP),"
            " :uuid, :dev, :ts, 0);",
        .fk_count = 1,
        .fks = {
            { "user_uuid", "users", "username", "username", false, true }
        }
    },
    /* 18. settings (key TEXT PRIMARY KEY) */
    {
        .name = "settings",
        .is_key_pk = true,
        .append_only = false,
        .serialize_sql =
            "SELECT json_object("
            " 'sync_uuid', t.sync_uuid, 'origin_device_id', t.origin_device_id,"
            " 'updated_at_ms', t.updated_at_ms,"
            " 'key', t.key, 'value', t.value, 'description', t.description,"
            " 'updated_at', t.updated_at"
            ") FROM settings t WHERE t.sync_uuid = ?1;",
        .nat_lookup_sql =
            "SELECT key, COALESCE(updated_at_ms, 0) FROM settings "
            "WHERE key = json_extract(:json, '$.key') LIMIT 1;",
        .update_sql =
            "UPDATE settings SET "
            " value = COALESCE(json_extract(:json, '$.value'), value),"
            " description = json_extract(:json, '$.description'),"
            " updated_at = COALESCE(json_extract(:json, '$.updated_at'), CURRENT_TIMESTAMP),"
            " sync_uuid = :uuid, origin_device_id = :dev, updated_at_ms = :ts, deleted_at_ms = 0 "
            "WHERE key = :pk;",
        .insert_sql =
            "INSERT INTO settings (key, value, description, updated_at,"
            " sync_uuid, origin_device_id, updated_at_ms, deleted_at_ms) "
            "VALUES (COALESCE(json_extract(:json, '$.key'), ''),"
            " COALESCE(json_extract(:json, '$.value'), ''),"
            " json_extract(:json, '$.description'),"
            " COALESCE(json_extract(:json, '$.updated_at'), CURRENT_TIMESTAMP),"
            " :uuid, :dev, :ts, 0);",
        .fk_count = 0
    },
    /* 19. role_permissions */
    {
        .name = "role_permissions",
        .is_key_pk = false,
        .append_only = false,
        .serialize_sql =
            "SELECT json_object("
            " 'sync_uuid', t.sync_uuid, 'origin_device_id', t.origin_device_id,"
            " 'updated_at_ms', t.updated_at_ms,"
            " 'role_uuid', (SELECT r.sync_uuid FROM roles r WHERE r.id = t.role_id),"
            " 'role_name', (SELECT r.name FROM roles r WHERE r.id = t.role_id),"
            " 'module_name', t.module_name, 'can_view', t.can_view, 'can_edit', t.can_edit"
            ") FROM role_permissions t WHERE t.sync_uuid = ?1;",
        .nat_lookup_sql =
            "SELECT id, COALESCE(updated_at_ms, 0) FROM role_permissions "
            "WHERE role_id = :fk0 AND module_name = json_extract(:json, '$.module_name') LIMIT 1;",
        .update_sql =
            "UPDATE role_permissions SET "
            " role_id = :fk0,"
            " module_name = COALESCE(json_extract(:json, '$.module_name'), module_name),"
            " can_view = COALESCE(json_extract(:json, '$.can_view'), can_view),"
            " can_edit = COALESCE(json_extract(:json, '$.can_edit'), can_edit),"
            " sync_uuid = :uuid, origin_device_id = :dev, updated_at_ms = :ts, deleted_at_ms = 0 "
            "WHERE id = :pk;",
        .insert_sql =
            "INSERT INTO role_permissions (role_id, module_name, can_view, can_edit,"
            " sync_uuid, origin_device_id, updated_at_ms, deleted_at_ms) "
            "VALUES (:fk0, COALESCE(json_extract(:json, '$.module_name'), ''),"
            " COALESCE(json_extract(:json, '$.can_view'), 1),"
            " COALESCE(json_extract(:json, '$.can_edit'), 0),"
            " :uuid, :dev, :ts, 0);",
        .fk_count = 1,
        .fks = {
            { "role_uuid", "roles", "role_name", "name", true, false }
        }
    }
};

static const size_t SYNC_TABLE_DEF_COUNT = sizeof(SYNC_TABLE_DEFS) / sizeof(SYNC_TABLE_DEFS[0]);

static const SyncTableDef *find_table_def(const char *table_name)
{
    if (!table_name) return NULL;
    for (size_t i = 0; i < SYNC_TABLE_DEF_COUNT; i++) {
        if (strcmp(SYNC_TABLE_DEFS[i].name, table_name) == 0) {
            return &SYNC_TABLE_DEFS[i];
        }
    }
    return NULL;
}

/* ── SQLite Named-Bind & Anti-Echo Flag Helpers ───────────────────── */

static bool set_sync_applying_remote(sqlite3 *db, bool applying)
{
    if (!db) return false;
    sqlite3_stmt *stmt = NULL;
    const char *sql =
        "INSERT INTO sync_state (key, value) VALUES ('sync_applying_remote', ?1) "
        "ON CONFLICT(key) DO UPDATE SET value = excluded.value;";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return false;
    sqlite3_bind_text(stmt, 1, applying ? "1" : "0", -1, SQLITE_STATIC);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return (rc == SQLITE_DONE);
}

static bool extract_json_text_field(sqlite3 *db,
                                    const char *json_str,
                                    const char *field_name,
                                    char *out_buf,
                                    size_t buf_size,
                                    bool *out_is_nonempty)
{
    if (!db || !json_str || !field_name || !out_buf || buf_size == 0) return false;
    out_buf[0] = '\0';
    if (out_is_nonempty) *out_is_nonempty = false;

    char path[64];
    snprintf(path, sizeof(path), "$.%s", field_name);

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, "SELECT CAST(json_extract(?1, ?2) AS TEXT);", -1, &stmt, NULL) != SQLITE_OK) {
        return false;
    }
    sqlite3_bind_text(stmt, 1, json_str, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, path, -1, SQLITE_STATIC);

    bool ok = true;
    int rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW && sqlite3_column_type(stmt, 0) != SQLITE_NULL) {
        const char *txt = (const char *)sqlite3_column_text(stmt, 0);
        if (txt && txt[0] != '\0') {
            snprintf(out_buf, buf_size, "%s", txt);
            if (out_is_nonempty) *out_is_nonempty = true;
        }
    }
    sqlite3_finalize(stmt);
    return ok;
}

static bool lookup_local_id_by_col(sqlite3 *db,
                                   const char *table_name,
                                   const char *col_name,
                                   const char *val,
                                   long long *out_id)
{
    if (!db || !table_name || !col_name || !val || !val[0] || !out_id) return false;
    char sql[160];
    snprintf(sql, sizeof(sql), "SELECT id FROM %s WHERE %s = ?1 LIMIT 1;", table_name, col_name);

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return false;
    sqlite3_bind_text(stmt, 1, val, -1, SQLITE_STATIC);

    bool found = false;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        *out_id = sqlite3_column_int64(stmt, 0);
        found = true;
    }
    sqlite3_finalize(stmt);
    return found;
}

static bool resolve_fk_value(sqlite3 *db,
                             const char *json_str,
                             const FkSpec *fk,
                             long long *out_id,
                             bool *out_has_val)
{
    *out_id = 0;
    *out_has_val = false;

    char uuid_val[SYNC_UUID_LEN] = {0};
    bool has_uuid = false;
    if (!extract_json_text_field(db, json_str, fk->json_uuid_key, uuid_val, sizeof(uuid_val), &has_uuid)) {
        return false;
    }

    if (has_uuid) {
        if (lookup_local_id_by_col(db, fk->target_table, "sync_uuid", uuid_val, out_id)) {
            *out_has_val = true;
            return true;
        }
    }

    if (fk->fallback_json_key && fk->fallback_col) {
        char fallback_val[128] = {0};
        bool has_fb = false;
        if (extract_json_text_field(db, json_str, fk->fallback_json_key, fallback_val, sizeof(fallback_val), &has_fb)
            && has_fb) {
            if (lookup_local_id_by_col(db, fk->target_table, fk->fallback_col, fallback_val, out_id)) {
                *out_has_val = true;
                return true;
            }
        }
    }

    if (has_uuid) {
        if (fk->allow_dangling_null && !fk->not_null) {
            *out_has_val = false;
            return true;
        }
        return false;
    }

    if (fk->not_null) {
        return false;
    }

    *out_has_val = false;
    return true;
}

static void bind_named_text(sqlite3_stmt *stmt, const char *param, const char *val)
{
    int idx = sqlite3_bind_parameter_index(stmt, param);
    if (idx > 0) {
        if (val) sqlite3_bind_text(stmt, idx, val, -1, SQLITE_TRANSIENT);
        else sqlite3_bind_null(stmt, idx);
    }
}

static void bind_named_int64(sqlite3_stmt *stmt, const char *param, long long val)
{
    int idx = sqlite3_bind_parameter_index(stmt, param);
    if (idx > 0) sqlite3_bind_int64(stmt, idx, val);
}

static void bind_named_opt_fk(sqlite3_stmt *stmt, int slot, bool has_val, long long id_val)
{
    char pname[16];
    snprintf(pname, sizeof(pname), ":fk%d", slot);
    int idx = sqlite3_bind_parameter_index(stmt, pname);
    if (idx > 0) {
        if (has_val) sqlite3_bind_int64(stmt, idx, id_val);
        else sqlite3_bind_null(stmt, idx);
    }
}

/* ── Identity, Time & Config Public Wrappers ──────────────────────── */

bool sync_core_generate_uuid(char *out_uuid, size_t size)
{
    return db_sync_generate_uuid(out_uuid, size);
}

long long sync_core_now_ms(void)
{
    sqlite3 *db = db_get_handle();
    if (db) {
        sqlite3_stmt *stmt = NULL;
        if (sqlite3_prepare_v2(
                db,
                "SELECT CAST((julianday('now') - 2440587.5) * 86400000.0 AS INTEGER);",
                -1, &stmt, NULL) == SQLITE_OK) {
            long long ms = 0;
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                ms = sqlite3_column_int64(stmt, 0);
            }
            sqlite3_finalize(stmt);
            if (ms > 0) return ms;
        }
    }

#ifdef __ANDROID__
    struct timeval tv;
    if (gettimeofday(&tv, NULL) == 0) {
        return (long long)tv.tv_sec * 1000LL + (long long)(tv.tv_usec / 1000LL);
    }
#else
    struct timespec ts;
    if (timespec_get(&ts, TIME_UTC) == TIME_UTC) {
        return (long long)ts.tv_sec * 1000LL + (long long)(ts.tv_nsec / 1000000LL);
    }
#endif
    return (long long)time(NULL) * 1000LL;
}

bool sync_core_get_device_id(char *out_id, size_t size)
{
    return db_sync_get_device_id(out_id, size);
}

bool sync_core_get_device_prefix(char *out_prefix, size_t size)
{
    return db_sync_get_device_prefix(out_prefix, size);
}

bool sync_core_get_state(const char *key, char *out_val, size_t size)
{
    return db_sync_get_state(key, out_val, size);
}

bool sync_core_set_state(const char *key, const char *value)
{
    return db_sync_set_state(key, value);
}

/* ── Outbox Query & Serialization ─────────────────────────────────── */

int sync_core_get_pending_outbox_count(void)
{
    sqlite3 *db = db_get_handle();
    if (!db) return -1;

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM sync_outbox WHERE synced = 0;", -1, &stmt, NULL) != SQLITE_OK) {
        return -1;
    }

    int count = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        count = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);
    return count;
}

bool sync_core_serialize_row_json(const char *table_name,
                                  const char *record_uuid,
                                  char **out_json)
{
    if (!table_name || !record_uuid || !out_json) return false;
    *out_json = NULL;

    sqlite3 *db = db_get_handle();
    const SyncTableDef *def = find_table_def(table_name);
    if (!db || !def) return false;

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, def->serialize_sql, -1, &stmt, NULL) != SQLITE_OK) {
        fprintf(stderr, "[SYNC ERROR] Serialize prepare failed for %s: %s\n",
                table_name, sqlite3_errmsg(db));
        return false;
    }
    sqlite3_bind_text(stmt, 1, record_uuid, -1, SQLITE_STATIC);

    bool ok = false;
    if (sqlite3_step(stmt) == SQLITE_ROW && sqlite3_column_type(stmt, 0) != SQLITE_NULL) {
        const char *json = (const char *)sqlite3_column_text(stmt, 0);
        if (json) {
            *out_json = sync_strdup(json);
            ok = (*out_json != NULL);
        }
    }
    sqlite3_finalize(stmt);
    return ok;
}

void sync_core_free_outbox_items(SyncOutboxItem *items, int count)
{
    if (!items || count <= 0) return;
    for (int i = 0; i < count; i++) {
        if (items[i].payload_json) {
            free(items[i].payload_json);
            items[i].payload_json = NULL;
        }
    }
}

bool sync_core_fetch_pending_outbox(SyncOutboxItem *out_items,
                                    int max_items,
                                    int *out_count)
{
    if (!out_items || max_items <= 0 || !out_count) return false;
    *out_count = 0;
    memset(out_items, 0, sizeof(SyncOutboxItem) * (size_t)max_items);

    sqlite3 *db = db_get_handle();
    if (!db) return false;

    const char *sql =
        "SELECT id, table_name, record_uuid, operation, updated_at_ms, synced "
        "FROM sync_outbox WHERE synced = 0 ORDER BY id ASC LIMIT ?1;";

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return false;
    }
    sqlite3_bind_int(stmt, 1, max_items);

    int n = 0;
    while (n < max_items && sqlite3_step(stmt) == SQLITE_ROW) {
        SyncOutboxItem *it = &out_items[n];
        it->id = sqlite3_column_int64(stmt, 0);

        const char *tbl = (const char *)sqlite3_column_text(stmt, 1);
        const char *uuid = (const char *)sqlite3_column_text(stmt, 2);
        const char *op = (const char *)sqlite3_column_text(stmt, 3);
        it->updated_at_ms = sqlite3_column_int64(stmt, 4);
        it->synced = sqlite3_column_int(stmt, 5);

        snprintf(it->table_name, sizeof(it->table_name), "%s", tbl ? tbl : "");
        snprintf(it->record_uuid, sizeof(it->record_uuid), "%s", uuid ? uuid : "");
        snprintf(it->operation, sizeof(it->operation), "%s", op ? op : "UPSERT");

        if (strcmp(it->operation, "UPSERT") == 0) {
            if (!sync_core_serialize_row_json(it->table_name, it->record_uuid, &it->payload_json)) {
                /* Row was subsequently deleted before push; degrade to DELETE tombstone */
                snprintf(it->operation, sizeof(it->operation), "DELETE");
                it->payload_json = sync_strdup("{}");
            }
        } else {
            it->payload_json = sync_strdup("{}");
        }

        if (!it->payload_json) {
            sqlite3_finalize(stmt);
            sync_core_free_outbox_items(out_items, n);
            return false;
        }
        n++;
    }

    sqlite3_finalize(stmt);
    *out_count = n;
    return true;
}

bool sync_core_build_push_payload_json(const SyncOutboxItem *items,
                                       int count,
                                       char **out_json)
{
    if (!out_json || count < 0 || (count > 0 && !items)) return false;
    *out_json = NULL;

    sqlite3 *db = db_get_handle();
    if (!db) return false;

    char device_id[SYNC_DEVICE_ID_LEN] = {0};
    char device_prefix[8] = {0};
    sync_core_get_device_id(device_id, sizeof(device_id));
    sync_core_get_device_prefix(device_prefix, sizeof(device_prefix));

    DynBuf changes_buf;
    if (!dynbuf_init(&changes_buf, count > 0 ? 4096 : 16)) return false;
    if (!dynbuf_append(&changes_buf, "[", 1)) {
        dynbuf_free(&changes_buf);
        return false;
    }

    if (count > 0) {
        const char *elem_sql =
            "SELECT json_object("
            " 'outbox_id', ?1,"
            " 'table_name', ?2,"
            " 'record_uuid', ?3,"
            " 'operation', ?4,"
            " 'updated_at_ms', ?5,"
            " 'payload', json(CASE WHEN ?6 IS NOT NULL AND json_valid(?6) THEN ?6 ELSE '{}' END)"
            ");";

        sqlite3_stmt *elem_stmt = NULL;
        if (sqlite3_prepare_v2(db, elem_sql, -1, &elem_stmt, NULL) != SQLITE_OK) {
            dynbuf_free(&changes_buf);
            return false;
        }

        for (int i = 0; i < count; i++) {
            const SyncOutboxItem *it = &items[i];
            sqlite3_reset(elem_stmt);
            sqlite3_clear_bindings(elem_stmt);

            sqlite3_bind_int64(elem_stmt, 1, it->id);
            sqlite3_bind_text(elem_stmt, 2, it->table_name, -1, SQLITE_STATIC);
            sqlite3_bind_text(elem_stmt, 3, it->record_uuid, -1, SQLITE_STATIC);
            sqlite3_bind_text(elem_stmt, 4, it->operation, -1, SQLITE_STATIC);
            sqlite3_bind_int64(elem_stmt, 5, it->updated_at_ms);
            sqlite3_bind_text(elem_stmt, 6, it->payload_json ? it->payload_json : "{}", -1, SQLITE_STATIC);

            if (sqlite3_step(elem_stmt) != SQLITE_ROW || sqlite3_column_type(elem_stmt, 0) == SQLITE_NULL) {
                sqlite3_finalize(elem_stmt);
                dynbuf_free(&changes_buf);
                return false;
            }

            const char *elem_json = (const char *)sqlite3_column_text(elem_stmt, 0);
            if (!elem_json) {
                sqlite3_finalize(elem_stmt);
                dynbuf_free(&changes_buf);
                return false;
            }

            if (i > 0 && !dynbuf_append(&changes_buf, ",", 1)) {
                sqlite3_finalize(elem_stmt);
                dynbuf_free(&changes_buf);
                return false;
            }
            if (!dynbuf_append(&changes_buf, elem_json, strlen(elem_json))) {
                sqlite3_finalize(elem_stmt);
                dynbuf_free(&changes_buf);
                return false;
            }
        }
        sqlite3_finalize(elem_stmt);
    }

    if (!dynbuf_append(&changes_buf, "]", 1)) {
        dynbuf_free(&changes_buf);
        return false;
    }

    const char *env_sql =
        "SELECT json_object('device_id', ?1, 'device_prefix', ?2, 'changes', json(?3));";
    sqlite3_stmt *env_stmt = NULL;
    if (sqlite3_prepare_v2(db, env_sql, -1, &env_stmt, NULL) != SQLITE_OK) {
        dynbuf_free(&changes_buf);
        return false;
    }

    sqlite3_bind_text(env_stmt, 1, device_id, -1, SQLITE_STATIC);
    sqlite3_bind_text(env_stmt, 2, device_prefix, -1, SQLITE_STATIC);
    sqlite3_bind_text(env_stmt, 3, changes_buf.data, (int)changes_buf.len, SQLITE_STATIC);

    bool ok = false;
    if (sqlite3_step(env_stmt) == SQLITE_ROW && sqlite3_column_type(env_stmt, 0) != SQLITE_NULL) {
        const char *full_json = (const char *)sqlite3_column_text(env_stmt, 0);
        if (full_json) {
            *out_json = sync_strdup(full_json);
            ok = (*out_json != NULL);
        }
    }

    sqlite3_finalize(env_stmt);
    dynbuf_free(&changes_buf);
    return ok;
}

bool sync_core_mark_outbox_synced(long long max_outbox_id)
{
    if (max_outbox_id <= 0) return false;
    sqlite3 *db = db_get_handle();
    if (!db) return false;

    sqlite3_stmt *stmt = NULL;
    const char *sql = "UPDATE sync_outbox SET synced = 1 WHERE synced = 0 AND id <= ?1;";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return false;
    sqlite3_bind_int64(stmt, 1, max_outbox_id);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return (rc == SQLITE_DONE);
}

/* ── Remote Delta Applier Internals ───────────────────────────────── */

static bool apply_remote_delete_internal(sqlite3 *db,
                                         const SyncTableDef *def,
                                         const SyncRemoteChange *change,
                                         bool *out_applied)
{
    char check_sql[128];
    snprintf(check_sql, sizeof(check_sql),
             "SELECT COALESCE(updated_at_ms, 0) FROM %s WHERE sync_uuid = ?1 LIMIT 1;",
             def->name);

    sqlite3_stmt *chk = NULL;
    if (sqlite3_prepare_v2(db, check_sql, -1, &chk, NULL) != SQLITE_OK) return false;
    sqlite3_bind_text(chk, 1, change->record_uuid, -1, SQLITE_STATIC);

    bool exists_locally = false;
    long long local_ts = 0;
    if (sqlite3_step(chk) == SQLITE_ROW) {
        exists_locally = true;
        local_ts = sqlite3_column_int64(chk, 0);
    }
    sqlite3_finalize(chk);

    if (exists_locally && local_ts > change->updated_at_ms) {
        if (out_applied) *out_applied = false;
        return true;
    }

    if (exists_locally) {
        char del_sql[128];
        snprintf(del_sql, sizeof(del_sql), "DELETE FROM %s WHERE sync_uuid = ?1;", def->name);
        sqlite3_stmt *del = NULL;
        if (sqlite3_prepare_v2(db, del_sql, -1, &del, NULL) != SQLITE_OK) return false;
        sqlite3_bind_text(del, 1, change->record_uuid, -1, SQLITE_STATIC);
        int rc = sqlite3_step(del);
        sqlite3_finalize(del);
        if (rc != SQLITE_DONE) return false;
    }

    const char *tomb_sql =
        "INSERT OR REPLACE INTO sync_tombstones (table_name, record_uuid, origin_device_id, deleted_at_ms) "
        "VALUES (?1, ?2, ?3, ?4);";
    sqlite3_stmt *tb = NULL;
    if (sqlite3_prepare_v2(db, tomb_sql, -1, &tb, NULL) == SQLITE_OK) {
        sqlite3_bind_text(tb, 1, def->name, -1, SQLITE_STATIC);
        sqlite3_bind_text(tb, 2, change->record_uuid, -1, SQLITE_STATIC);
        sqlite3_bind_text(tb, 3, change->origin_device_id, -1, SQLITE_STATIC);
        sqlite3_bind_int64(tb, 4, change->updated_at_ms);
        sqlite3_step(tb);
        sqlite3_finalize(tb);
    }

    if (out_applied) *out_applied = true;
    return true;
}

static bool is_valid_json_object(sqlite3 *db, const char *json_str)
{
    if (!db || !json_str || json_str[0] == '\0') return false;
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, "SELECT json_valid(?1), json_type(?1);", -1, &stmt, NULL) != SQLITE_OK) {
        return false;
    }
    sqlite3_bind_text(stmt, 1, json_str, -1, SQLITE_STATIC);
    bool valid = false;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        int is_valid = sqlite3_column_int(stmt, 0);
        const char *jtype = (const char *)sqlite3_column_text(stmt, 1);
        if (is_valid == 1 && jtype && strcmp(jtype, "object") == 0) {
            valid = true;
        }
    }
    sqlite3_finalize(stmt);
    return valid;
}

static bool apply_remote_upsert_internal(sqlite3 *db,
                                         const SyncTableDef *def,
                                         const SyncRemoteChange *change,
                                         bool *out_applied)
{
    if (!is_valid_json_object(db, change->payload_json)) {
        return false;
    }

    long long fk_ids[MAX_TABLE_FKS] = {0};
    bool fk_has[MAX_TABLE_FKS] = {false};

    for (int i = 0; i < def->fk_count; i++) {
        if (!resolve_fk_value(db, change->payload_json, &def->fks[i], &fk_ids[i], &fk_has[i])) {
            return false;
        }
    }

    /* Check if record already exists locally by sync_uuid first */
    bool found = false;
    bool matched_by_uuid = false;
    long long local_ts = 0;
    long long existing_id = 0;
    char existing_key[128] = {0};

    char uuid_sql[128];
    snprintf(uuid_sql, sizeof(uuid_sql),
             "SELECT %s, COALESCE(updated_at_ms, 0) FROM %s WHERE sync_uuid = ?1 LIMIT 1;",
             def->is_key_pk ? "key" : "id", def->name);

    sqlite3_stmt *ustmt = NULL;
    if (sqlite3_prepare_v2(db, uuid_sql, -1, &ustmt, NULL) != SQLITE_OK) return false;
    sqlite3_bind_text(ustmt, 1, change->record_uuid, -1, SQLITE_STATIC);

    if (sqlite3_step(ustmt) == SQLITE_ROW) {
        found = true;
        matched_by_uuid = true;
        if (def->is_key_pk) {
            const char *k = (const char *)sqlite3_column_text(ustmt, 0);
            snprintf(existing_key, sizeof(existing_key), "%s", k ? k : "");
        } else {
            existing_id = sqlite3_column_int64(ustmt, 0);
        }
        local_ts = sqlite3_column_int64(ustmt, 1);
    }
    sqlite3_finalize(ustmt);

    /* Fallback lookup by natural unique key (prevents constraint failure on seeded/unique rows) */
    if (!found && def->nat_lookup_sql) {
        sqlite3_stmt *nstmt = NULL;
        if (sqlite3_prepare_v2(db, def->nat_lookup_sql, -1, &nstmt, NULL) != SQLITE_OK) return false;
        bind_named_text(nstmt, ":json", change->payload_json);
        for (int i = 0; i < def->fk_count; i++) {
            bind_named_opt_fk(nstmt, i, fk_has[i], fk_ids[i]);
        }

        if (sqlite3_step(nstmt) == SQLITE_ROW) {
            found = true;
            if (def->is_key_pk) {
                const char *k = (const char *)sqlite3_column_text(nstmt, 0);
                snprintf(existing_key, sizeof(existing_key), "%s", k ? k : "");
            } else {
                existing_id = sqlite3_column_int64(nstmt, 0);
            }
            local_ts = sqlite3_column_int64(nstmt, 1);
        }
        sqlite3_finalize(nstmt);
    }

    if (found) {
        /* Append-only safety for immutable financial receipts */
        if (def->append_only || !def->update_sql) {
            if (out_applied) *out_applied = false;
            return true;
        }
        /* Last-Write-Wins conflict check */
        if (change->updated_at_ms < local_ts ||
            (change->updated_at_ms == local_ts && matched_by_uuid)) {
            if (out_applied) *out_applied = false;
            return true;
        }
    }

    const char *exec_sql = found ? def->update_sql : def->insert_sql;
    sqlite3_stmt *estmt = NULL;
    if (sqlite3_prepare_v2(db, exec_sql, -1, &estmt, NULL) != SQLITE_OK) {
        fprintf(stderr, "[SYNC ERROR] Prepare failed for %s: %s\n",
                def->name, sqlite3_errmsg(db));
        return false;
    }

    bind_named_text(estmt, ":json", change->payload_json);
    bind_named_text(estmt, ":uuid", change->record_uuid);
    bind_named_text(estmt, ":dev", change->origin_device_id);
    bind_named_int64(estmt, ":ts", change->updated_at_ms);

    for (int i = 0; i < def->fk_count; i++) {
        bind_named_opt_fk(estmt, i, fk_has[i], fk_ids[i]);
    }

    if (found) {
        if (def->is_key_pk) {
            bind_named_text(estmt, ":pk", existing_key);
        } else {
            bind_named_int64(estmt, ":pk", existing_id);
        }
    }

    int rc = sqlite3_step(estmt);
    if (rc != SQLITE_DONE) {
        fprintf(stderr, "[SYNC ERROR] Apply %s failed on %s: %s\n",
                found ? "UPDATE" : "INSERT", def->name, sqlite3_errmsg(db));
        sqlite3_finalize(estmt);
        return false;
    }
    sqlite3_finalize(estmt);

    if (out_applied) *out_applied = true;
    return true;
}

static bool apply_single_change_internal(sqlite3 *db,
                                         const SyncRemoteChange *change,
                                         bool *out_applied)
{
    if (!db || !change || change->record_uuid[0] == '\0') return false;
    const SyncTableDef *def = find_table_def(change->table_name);
    if (!def) return false;

    if (strcmp(change->operation, "DELETE") == 0) {
        return apply_remote_delete_internal(db, def, change, out_applied);
    }
    if (strcmp(change->operation, "UPSERT") == 0) {
        return apply_remote_upsert_internal(db, def, change, out_applied);
    }
    return false;
}

/* ── Public Remote Delta Applier Functions ────────────────────────── */

bool sync_core_apply_remote_change(const SyncRemoteChange *change, bool *out_applied)
{
    if (out_applied) *out_applied = false;
    sqlite3 *db = db_get_handle();
    if (!db || !change) return false;

    if (db_begin_transaction() != DB_SUCCESS) return false;

    if (!set_sync_applying_remote(db, true)) {
        db_rollback();
        return false;
    }

    bool applied = false;
    bool ok = apply_single_change_internal(db, change, &applied);

    /* Always clear anti-echo flag before finishing transaction */
    if (!set_sync_applying_remote(db, false)) {
        ok = false;
    }

    if (ok) {
        if (db_commit() != DB_SUCCESS) {
            db_rollback();
            set_sync_applying_remote(db, false);
            return false;
        }
        if (out_applied) *out_applied = applied;
        return true;
    }

    db_rollback();
    set_sync_applying_remote(db, false);
    return false;
}

bool sync_core_apply_remote_batch(const SyncRemoteChange *changes,
                                  int count,
                                  SyncApplyResult *out_result)
{
    if (out_result) {
        memset(out_result, 0, sizeof(*out_result));
    }
    if (count < 0 || (count > 0 && !changes)) return false;

    sqlite3 *db = db_get_handle();
    if (!db) return false;

    char seq_buf[32] = {0};
    long long last_seq = 0;
    if (db_sync_get_state("last_server_seq", seq_buf, sizeof(seq_buf))) {
        last_seq = strtoll(seq_buf, NULL, 10);
    }

    if (count == 0) {
        if (out_result) out_result->last_server_seq = last_seq;
        return true;
    }

    if (db_begin_transaction() != DB_SUCCESS) return false;

    if (!set_sync_applying_remote(db, true)) {
        db_rollback();
        return false;
    }

    int applied_cnt = 0;
    int skipped_cnt = 0;
    int error_cnt = 0;
    long long contiguous_seq = last_seq;

    for (int i = 0; i < count; i++) {
        char *errmsg = NULL;
        if (sqlite3_exec(db, "SAVEPOINT sp_sync_item;", NULL, NULL, &errmsg) != SQLITE_OK) {
            sqlite3_free(errmsg);
            error_cnt++;
            continue;
        }

        bool applied = false;
        if (apply_single_change_internal(db, &changes[i], &applied)) {
            sqlite3_exec(db, "RELEASE SAVEPOINT sp_sync_item;", NULL, NULL, NULL);
            if (applied) applied_cnt++;
            else skipped_cnt++;

            if (error_cnt == 0 && changes[i].server_seq > contiguous_seq) {
                contiguous_seq = changes[i].server_seq;
            }
        } else {
            sqlite3_exec(db, "ROLLBACK TO SAVEPOINT sp_sync_item;", NULL, NULL, NULL);
            sqlite3_exec(db, "RELEASE SAVEPOINT sp_sync_item;", NULL, NULL, NULL);
            error_cnt++;
        }
    }

    if (contiguous_seq > last_seq) {
        char new_seq[32];
        snprintf(new_seq, sizeof(new_seq), "%lld", contiguous_seq);
        db_sync_set_state("last_server_seq", new_seq);
        last_seq = contiguous_seq;
    }

    if (!set_sync_applying_remote(db, false)) {
        db_rollback();
        set_sync_applying_remote(db, false);
        return false;
    }

    if (db_commit() != DB_SUCCESS) {
        db_rollback();
        set_sync_applying_remote(db, false);
        return false;
    }

    if (out_result) {
        out_result->applied_count = applied_cnt;
        out_result->skipped_conflict_count = skipped_cnt;
        out_result->error_count = error_cnt;
        out_result->last_server_seq = last_seq;
    }

    return (error_cnt == 0);
}
