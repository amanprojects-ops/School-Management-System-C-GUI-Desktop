/**
 * @file teacher.c
 * @brief Teacher and staff management database CRUD operations.
 */

#include "teacher.h"
#include "database.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ── Code Generators ──────────────────────────────────────────────── */

bool teacher_service_generate_code(char *out_code, size_t size)
{
    if (!out_code || size < 20) return false;

    sqlite3 *db = db_get_handle();
    if (!db) return false;

    int max_id = 0;
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, "SELECT COALESCE(MAX(id), 0) FROM teachers;", -1, &stmt, NULL) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            max_id = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }

    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    int year = (tm_info) ? (tm_info->tm_year + 1900) : 2026;

    snprintf(out_code, size, "TCH-%d-%04d", year, max_id + 1);
    return true;
}

bool staff_service_generate_code(char *out_code, size_t size)
{
    if (!out_code || size < 20) return false;

    sqlite3 *db = db_get_handle();
    if (!db) return false;

    int max_id = 0;
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, "SELECT COALESCE(MAX(id), 0) FROM staff;", -1, &stmt, NULL) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            max_id = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }

    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    int year = (tm_info) ? (tm_info->tm_year + 1900) : 2026;

    snprintf(out_code, size, "STF-%d-%04d", year, max_id + 1);
    return true;
}

/* ── Teacher Implementation ───────────────────────────────────────── */

static void read_teacher_row(sqlite3_stmt *stmt, Teacher *t)
{
    memset(t, 0, sizeof(Teacher));
    t->id = sqlite3_column_int(stmt, 0);

    const char *code   = (const char *)sqlite3_column_text(stmt, 1);
    t->user_id         = sqlite3_column_int(stmt, 2);
    const char *name   = (const char *)sqlite3_column_text(stmt, 3);
    const char *phone  = (const char *)sqlite3_column_text(stmt, 4);
    const char *email  = (const char *)sqlite3_column_text(stmt, 5);
    const char *addr   = (const char *)sqlite3_column_text(stmt, 6);
    const char *qual   = (const char *)sqlite3_column_text(stmt, 7);
    const char *subj   = (const char *)sqlite3_column_text(stmt, 8);
    const char *join   = (const char *)sqlite3_column_text(stmt, 9);
    t->salary          = sqlite3_column_double(stmt, 10);
    const char *status = (const char *)sqlite3_column_text(stmt, 11);
    const char *created= (const char *)sqlite3_column_text(stmt, 12);

    if (code)    strncpy(t->teacher_code, code, sizeof(t->teacher_code) - 1);
    if (name)    strncpy(t->full_name, name, sizeof(t->full_name) - 1);
    if (phone)   strncpy(t->phone, phone, sizeof(t->phone) - 1);
    if (email)   strncpy(t->email, email, sizeof(t->email) - 1);
    if (addr)    strncpy(t->address, addr, sizeof(t->address) - 1);
    if (qual)    strncpy(t->qualification, qual, sizeof(t->qualification) - 1);
    if (subj)    strncpy(t->subject, subj, sizeof(t->subject) - 1);
    if (join)    strncpy(t->joining_date, join, sizeof(t->joining_date) - 1);
    if (status)  strncpy(t->status, status, sizeof(t->status) - 1);
    if (created) strncpy(t->created_at, created, sizeof(t->created_at) - 1);
}

static const char *TEACHER_SELECT =
    "SELECT id, teacher_code, user_id, full_name, phone, email, "
    "       address, qualification, subject, joining_date, salary, status, created_at "
    "FROM teachers ";

bool teacher_service_get_by_id(int id, Teacher *out_teacher)
{
    if (!out_teacher || id <= 0) return false;

    sqlite3 *db = db_get_handle();
    if (!db) return false;

    char sql[512];
    snprintf(sql, sizeof(sql), "%s WHERE id = ?;", TEACHER_SELECT);

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return false;

    sqlite3_bind_int(stmt, 1, id);
    bool found = false;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        read_teacher_row(stmt, out_teacher);
        found = true;
    }
    sqlite3_finalize(stmt);
    return found;
}

bool teacher_service_create(const Teacher *t, int *out_new_id)
{
    if (!t || strlen(t->full_name) == 0) return false;

    sqlite3 *db = db_get_handle();
    if (!db) return false;

    char code[32];
    if (strlen(t->teacher_code) > 0) {
        strncpy(code, t->teacher_code, sizeof(code) - 1);
        code[sizeof(code) - 1] = '\0';
    } else {
        teacher_service_generate_code(code, sizeof(code));
    }

    const char *sql =
        "INSERT INTO teachers (teacher_code, user_id, full_name, phone, email, "
        "                     address, qualification, subject, joining_date, salary, status) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return false;

    sqlite3_bind_text(stmt, 1, code, -1, SQLITE_STATIC);
    if (t->user_id > 0) sqlite3_bind_int(stmt, 2, t->user_id);
    else sqlite3_bind_null(stmt, 2);
    sqlite3_bind_text(stmt, 3, t->full_name, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, t->phone, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 5, t->email, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 6, t->address, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 7, t->qualification, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 8, t->subject, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 9, (strlen(t->joining_date) > 0) ? t->joining_date : "date('now')", -1, SQLITE_STATIC);
    sqlite3_bind_double(stmt, 10, t->salary);
    sqlite3_bind_text(stmt, 11, (strlen(t->status) > 0) ? t->status : "ACTIVE", -1, SQLITE_STATIC);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) return false;

    int new_id = (int)sqlite3_last_insert_rowid(db);
    if (out_new_id) *out_new_id = new_id;

    char details[256];
    snprintf(details, sizeof(details), "Added teacher %s (%s)", t->full_name, code);
    db_log_activity(0, "CREATE_TEACHER", "TEACHERS", details);

    return true;
}

bool teacher_service_update(const Teacher *t)
{
    if (!t || t->id <= 0) return false;

    sqlite3 *db = db_get_handle();
    if (!db) return false;

    const char *sql =
        "UPDATE teachers SET full_name = ?, phone = ?, email = ?, address = ?, "
        "                   qualification = ?, subject = ?, joining_date = ?, "
        "                   salary = ?, status = ? "
        "WHERE id = ?;";

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return false;

    sqlite3_bind_text(stmt, 1, t->full_name, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, t->phone, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, t->email, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, t->address, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 5, t->qualification, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 6, t->subject, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 7, t->joining_date, -1, SQLITE_STATIC);
    sqlite3_bind_double(stmt, 8, t->salary);
    sqlite3_bind_text(stmt, 9, t->status, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 10, t->id);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) return false;

    char details[256];
    snprintf(details, sizeof(details), "Updated teacher ID %d (%s)", t->id, t->full_name);
    db_log_activity(0, "UPDATE_TEACHER", "TEACHERS", details);
    return true;
}

bool teacher_service_delete(int id)
{
    if (id <= 0) return false;

    sqlite3 *db = db_get_handle();
    if (!db) return false;

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, "DELETE FROM teachers WHERE id = ?;", -1, &stmt, NULL) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_int(stmt, 1, id);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) return false;

    char details[64];
    snprintf(details, sizeof(details), "Deleted teacher ID %d", id);
    db_log_activity(0, "DELETE_TEACHER", "TEACHERS", details);
    return true;
}

bool teacher_service_get_all(const char *search, const char *status,
                             Teacher *out_list, int max_items, int *out_count)
{
    if (!out_list || max_items <= 0 || !out_count) return false;
    *out_count = 0;

    sqlite3 *db = db_get_handle();
    if (!db) return false;

    char where_clause[512] = "WHERE 1=1 ";
    if (search && strlen(search) > 0) {
        strcat(where_clause, "AND (full_name LIKE ?1 OR teacher_code LIKE ?1 OR subject LIKE ?1) ");
    }
    if (status && strlen(status) > 0) {
        strcat(where_clause, "AND status = ?2 ");
    }

    char sql[1024];
    snprintf(sql, sizeof(sql), "%s %s ORDER BY id DESC LIMIT ?3;", TEACHER_SELECT, where_clause);

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return false;

    char pattern[140] = {0};
    if (search && strlen(search) > 0) {
        snprintf(pattern, sizeof(pattern), "%%%s%%", search);
        sqlite3_bind_text(stmt, 1, pattern, -1, SQLITE_STATIC);
    }
    if (status && strlen(status) > 0) {
        sqlite3_bind_text(stmt, 2, status, -1, SQLITE_STATIC);
    }
    sqlite3_bind_int(stmt, 3, max_items);

    int count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW && count < max_items) {
        read_teacher_row(stmt, &out_list[count]);
        count++;
    }
    sqlite3_finalize(stmt);

    *out_count = count;
    return true;
}

/* ── Staff Implementation ─────────────────────────────────────────── */

static void read_staff_row(sqlite3_stmt *stmt, Staff *s)
{
    memset(s, 0, sizeof(Staff));
    s->id = sqlite3_column_int(stmt, 0);

    const char *code   = (const char *)sqlite3_column_text(stmt, 1);
    s->user_id         = sqlite3_column_int(stmt, 2);
    const char *name   = (const char *)sqlite3_column_text(stmt, 3);
    const char *role   = (const char *)sqlite3_column_text(stmt, 4);
    const char *phone  = (const char *)sqlite3_column_text(stmt, 5);
    const char *email  = (const char *)sqlite3_column_text(stmt, 6);
    const char *addr   = (const char *)sqlite3_column_text(stmt, 7);
    const char *join   = (const char *)sqlite3_column_text(stmt, 8);
    s->salary          = sqlite3_column_double(stmt, 9);
    const char *status = (const char *)sqlite3_column_text(stmt, 10);
    const char *created= (const char *)sqlite3_column_text(stmt, 11);

    if (code)    strncpy(s->staff_code, code, sizeof(s->staff_code) - 1);
    if (name)    strncpy(s->full_name, name, sizeof(s->full_name) - 1);
    if (role)    strncpy(s->role_title, role, sizeof(s->role_title) - 1);
    if (phone)   strncpy(s->phone, phone, sizeof(s->phone) - 1);
    if (email)   strncpy(s->email, email, sizeof(s->email) - 1);
    if (addr)    strncpy(s->address, addr, sizeof(s->address) - 1);
    if (join)    strncpy(s->joining_date, join, sizeof(s->joining_date) - 1);
    if (status)  strncpy(s->status, status, sizeof(s->status) - 1);
    if (created) strncpy(s->created_at, created, sizeof(s->created_at) - 1);
}

static const char *STAFF_SELECT =
    "SELECT id, staff_code, user_id, full_name, role_title, phone, email, "
    "       address, joining_date, salary, status, created_at "
    "FROM staff ";

bool staff_service_get_by_id(int id, Staff *out_staff)
{
    if (!out_staff || id <= 0) return false;

    sqlite3 *db = db_get_handle();
    if (!db) return false;

    char sql[512];
    snprintf(sql, sizeof(sql), "%s WHERE id = ?;", STAFF_SELECT);

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return false;

    sqlite3_bind_int(stmt, 1, id);
    bool found = false;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        read_staff_row(stmt, out_staff);
        found = true;
    }
    sqlite3_finalize(stmt);
    return found;
}

bool staff_service_create(const Staff *s, int *out_new_id)
{
    if (!s || strlen(s->full_name) == 0) return false;

    sqlite3 *db = db_get_handle();
    if (!db) return false;

    char code[32];
    if (strlen(s->staff_code) > 0) {
        strncpy(code, s->staff_code, sizeof(code) - 1);
        code[sizeof(code) - 1] = '\0';
    } else {
        staff_service_generate_code(code, sizeof(code));
    }

    const char *sql =
        "INSERT INTO staff (staff_code, user_id, full_name, role_title, phone, email, "
        "                  address, joining_date, salary, status) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return false;

    sqlite3_bind_text(stmt, 1, code, -1, SQLITE_STATIC);
    if (s->user_id > 0) sqlite3_bind_int(stmt, 2, s->user_id);
    else sqlite3_bind_null(stmt, 2);
    sqlite3_bind_text(stmt, 3, s->full_name, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, (strlen(s->role_title) > 0) ? s->role_title : "General Staff", -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 5, s->phone, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 6, s->email, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 7, s->address, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 8, (strlen(s->joining_date) > 0) ? s->joining_date : "date('now')", -1, SQLITE_STATIC);
    sqlite3_bind_double(stmt, 9, s->salary);
    sqlite3_bind_text(stmt, 10, (strlen(s->status) > 0) ? s->status : "ACTIVE", -1, SQLITE_STATIC);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) return false;

    int new_id = (int)sqlite3_last_insert_rowid(db);
    if (out_new_id) *out_new_id = new_id;

    char details[256];
    snprintf(details, sizeof(details), "Added staff member %s (%s)", s->full_name, code);
    db_log_activity(0, "CREATE_STAFF", "STAFF", details);

    return true;
}

bool staff_service_update(const Staff *s)
{
    if (!s || s->id <= 0) return false;

    sqlite3 *db = db_get_handle();
    if (!db) return false;

    const char *sql =
        "UPDATE staff SET full_name = ?, role_title = ?, phone = ?, email = ?, address = ?, "
        "                joining_date = ?, salary = ?, status = ? "
        "WHERE id = ?;";

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return false;

    sqlite3_bind_text(stmt, 1, s->full_name, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, s->role_title, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, s->phone, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, s->email, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 5, s->address, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 6, s->joining_date, -1, SQLITE_STATIC);
    sqlite3_bind_double(stmt, 7, s->salary);
    sqlite3_bind_text(stmt, 8, s->status, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 9, s->id);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) return false;

    char details[256];
    snprintf(details, sizeof(details), "Updated staff ID %d (%s)", s->id, s->full_name);
    db_log_activity(0, "UPDATE_STAFF", "STAFF", details);
    return true;
}

bool staff_service_delete(int id)
{
    if (id <= 0) return false;

    sqlite3 *db = db_get_handle();
    if (!db) return false;

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, "DELETE FROM staff WHERE id = ?;", -1, &stmt, NULL) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_int(stmt, 1, id);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) return false;

    char details[64];
    snprintf(details, sizeof(details), "Deleted staff ID %d", id);
    db_log_activity(0, "DELETE_STAFF", "STAFF", details);
    return true;
}

bool staff_service_get_all(const char *search, const char *status,
                           Staff *out_list, int max_items, int *out_count)
{
    if (!out_list || max_items <= 0 || !out_count) return false;
    *out_count = 0;

    sqlite3 *db = db_get_handle();
    if (!db) return false;

    char where_clause[512] = "WHERE 1=1 ";
    if (search && strlen(search) > 0) {
        strcat(where_clause, "AND (full_name LIKE ?1 OR staff_code LIKE ?1 OR role_title LIKE ?1) ");
    }
    if (status && strlen(status) > 0) {
        strcat(where_clause, "AND status = ?2 ");
    }

    char sql[1024];
    snprintf(sql, sizeof(sql), "%s %s ORDER BY id DESC LIMIT ?3;", STAFF_SELECT, where_clause);

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return false;

    char pattern[140] = {0};
    if (search && strlen(search) > 0) {
        snprintf(pattern, sizeof(pattern), "%%%s%%", search);
        sqlite3_bind_text(stmt, 1, pattern, -1, SQLITE_STATIC);
    }
    if (status && strlen(status) > 0) {
        sqlite3_bind_text(stmt, 2, status, -1, SQLITE_STATIC);
    }
    sqlite3_bind_int(stmt, 3, max_items);

    int count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW && count < max_items) {
        read_staff_row(stmt, &out_list[count]);
        count++;
    }
    sqlite3_finalize(stmt);

    *out_count = count;
    return true;
}
