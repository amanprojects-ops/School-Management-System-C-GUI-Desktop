/**
 * @file classes.c
 * @brief Class, section, subject, and academic year database operations.
 */

#include "classes.h"
#include "database.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── Academic Year Services ───────────────────────────────────────── */

bool academic_year_get_all(AcademicYear *out_list, int max_items, int *out_count)
{
    if (!out_list || max_items <= 0 || !out_count) return false;
    *out_count = 0;

    sqlite3 *db = db_get_handle();
    if (!db) return false;

    const char *sql = "SELECT id, name, start_date, end_date, is_current FROM academic_years ORDER BY id DESC LIMIT ?;";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return false;

    sqlite3_bind_int(stmt, 1, max_items);
    int count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW && count < max_items) {
        out_list[count].id = sqlite3_column_int(stmt, 0);
        const char *name = (const char *)sqlite3_column_text(stmt, 1);
        const char *s_dt = (const char *)sqlite3_column_text(stmt, 2);
        const char *e_dt = (const char *)sqlite3_column_text(stmt, 3);
        out_list[count].is_current = (sqlite3_column_int(stmt, 4) != 0);

        if (name) strncpy(out_list[count].name, name, sizeof(out_list[count].name) - 1);
        if (s_dt) strncpy(out_list[count].start_date, s_dt, sizeof(out_list[count].start_date) - 1);
        if (e_dt) strncpy(out_list[count].end_date, e_dt, sizeof(out_list[count].end_date) - 1);
        count++;
    }
    sqlite3_finalize(stmt);
    *out_count = count;
    return true;
}

bool academic_year_get_current(AcademicYear *out_year)
{
    if (!out_year) return false;
    memset(out_year, 0, sizeof(AcademicYear));

    sqlite3 *db = db_get_handle();
    if (!db) return false;

    const char *sql = "SELECT id, name, start_date, end_date, is_current FROM academic_years WHERE is_current = 1 LIMIT 1;";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return false;

    bool found = false;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        out_year->id = sqlite3_column_int(stmt, 0);
        const char *name = (const char *)sqlite3_column_text(stmt, 1);
        const char *s_dt = (const char *)sqlite3_column_text(stmt, 2);
        const char *e_dt = (const char *)sqlite3_column_text(stmt, 3);
        out_year->is_current = true;

        if (name) strncpy(out_year->name, name, sizeof(out_year->name) - 1);
        if (s_dt) strncpy(out_year->start_date, s_dt, sizeof(out_year->start_date) - 1);
        if (e_dt) strncpy(out_year->end_date, e_dt, sizeof(out_year->end_date) - 1);
        found = true;
    }
    sqlite3_finalize(stmt);
    return found;
}

bool academic_year_create(const AcademicYear *year, int *out_id)
{
    if (!year || strlen(year->name) == 0) return false;

    sqlite3 *db = db_get_handle();
    if (!db) return false;

    const char *sql = "INSERT INTO academic_years (name, start_date, end_date, is_current) VALUES (?, ?, ?, ?);";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return false;

    sqlite3_bind_text(stmt, 1, year->name, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, year->start_date, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, year->end_date, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 4, year->is_current ? 1 : 0);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) return false;

    int new_id = (int)sqlite3_last_insert_rowid(db);
    if (out_id) *out_id = new_id;

    if (year->is_current) {
        academic_year_set_current(new_id);
    }
    return true;
}

bool academic_year_set_current(int id)
{
    if (id <= 0) return false;
    sqlite3 *db = db_get_handle();
    if (!db) return false;

    sqlite3_exec(db, "UPDATE academic_years SET is_current = 0;", NULL, NULL, NULL);

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, "UPDATE academic_years SET is_current = 1 WHERE id = ?;", -1, &stmt, NULL) != SQLITE_OK) {
        return false;
    }
    sqlite3_bind_int(stmt, 1, id);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return (rc == SQLITE_DONE);
}

/* ── Class Services ───────────────────────────────────────────────── */

static void read_class_row(sqlite3_stmt *stmt, SchoolClass *c)
{
    memset(c, 0, sizeof(SchoolClass));
    c->id = sqlite3_column_int(stmt, 0);

    const char *name = (const char *)sqlite3_column_text(stmt, 1);
    c->numeric_value = sqlite3_column_int(stmt, 2);
    c->capacity      = sqlite3_column_int(stmt, 3);
    c->academic_year_id = sqlite3_column_int(stmt, 4);
    const char *ay_name = (const char *)sqlite3_column_text(stmt, 5);
    c->section_count = sqlite3_column_int(stmt, 6);
    c->student_count = sqlite3_column_int(stmt, 7);

    if (name) strncpy(c->name, name, sizeof(c->name) - 1);
    if (ay_name) strncpy(c->academic_year_name, ay_name, sizeof(c->academic_year_name) - 1);
}

static const char *CLASS_SELECT =
    "SELECT c.id, c.name, c.numeric_value, c.capacity, c.academic_year_id, "
    "       COALESCE(ay.name, 'Default') AS academic_year_name, "
    "       (SELECT COUNT(*) FROM sections sec WHERE sec.class_id = c.id) AS section_count, "
    "       (SELECT COUNT(*) FROM students s WHERE s.class_id = c.id) AS student_count "
    "FROM classes c "
    "LEFT JOIN academic_years ay ON c.academic_year_id = ay.id ";

bool class_service_get_all(SchoolClass *out_list, int max_items, int *out_count)
{
    if (!out_list || max_items <= 0 || !out_count) return false;
    *out_count = 0;

    sqlite3 *db = db_get_handle();
    if (!db) return false;

    char sql[1024];
    snprintf(sql, sizeof(sql), "%s ORDER BY c.numeric_value ASC, c.name ASC LIMIT ?;", CLASS_SELECT);

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return false;

    sqlite3_bind_int(stmt, 1, max_items);
    int count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW && count < max_items) {
        read_class_row(stmt, &out_list[count]);
        count++;
    }
    sqlite3_finalize(stmt);
    *out_count = count;
    return true;
}

bool class_service_get_by_id(int id, SchoolClass *out_class)
{
    if (!out_class || id <= 0) return false;

    sqlite3 *db = db_get_handle();
    if (!db) return false;

    char sql[1024];
    snprintf(sql, sizeof(sql), "%s WHERE c.id = ?;", CLASS_SELECT);

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return false;

    sqlite3_bind_int(stmt, 1, id);
    bool found = false;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        read_class_row(stmt, out_class);
        found = true;
    }
    sqlite3_finalize(stmt);
    return found;
}

bool class_service_create(const SchoolClass *cls, int *out_new_id)
{
    if (!cls || strlen(cls->name) == 0) return false;

    sqlite3 *db = db_get_handle();
    if (!db) return false;

    int ay_id = cls->academic_year_id;
    if (ay_id <= 0) {
        AcademicYear cur;
        if (academic_year_get_current(&cur)) ay_id = cur.id;
        else ay_id = 1;
    }

    const char *sql = "INSERT INTO classes (name, numeric_value, capacity, academic_year_id) VALUES (?, ?, ?, ?);";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return false;

    sqlite3_bind_text(stmt, 1, cls->name, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 2, cls->numeric_value);
    sqlite3_bind_int(stmt, 3, (cls->capacity > 0) ? cls->capacity : 40);
    sqlite3_bind_int(stmt, 4, ay_id);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) return false;

    int new_id = (int)sqlite3_last_insert_rowid(db);
    if (out_new_id) *out_new_id = new_id;

    char details[256];
    snprintf(details, sizeof(details), "Created class %s", cls->name);
    db_log_activity(0, "CREATE_CLASS", "CLASSES", details);
    return true;
}

bool class_service_update(const SchoolClass *cls)
{
    if (!cls || cls->id <= 0) return false;

    sqlite3 *db = db_get_handle();
    if (!db) return false;

    const char *sql = "UPDATE classes SET name = ?, numeric_value = ?, capacity = ? WHERE id = ?;";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return false;

    sqlite3_bind_text(stmt, 1, cls->name, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 2, cls->numeric_value);
    sqlite3_bind_int(stmt, 3, cls->capacity);
    sqlite3_bind_int(stmt, 4, cls->id);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) return false;

    char details[256];
    snprintf(details, sizeof(details), "Updated class ID %d (%s)", cls->id, cls->name);
    db_log_activity(0, "UPDATE_CLASS", "CLASSES", details);
    return true;
}

bool class_service_delete(int id)
{
    if (id <= 0) return false;

    sqlite3 *db = db_get_handle();
    if (!db) return false;

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, "DELETE FROM classes WHERE id = ?;", -1, &stmt, NULL) != SQLITE_OK) {
        return false;
    }
    sqlite3_bind_int(stmt, 1, id);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) return false;

    char details[64];
    snprintf(details, sizeof(details), "Deleted class ID %d", id);
    db_log_activity(0, "DELETE_CLASS", "CLASSES", details);
    return true;
}

/* ── Section Services ─────────────────────────────────────────────── */

static const char *SECTION_SELECT =
    "SELECT sec.id, sec.class_id, COALESCE(c.name, 'Unassigned') AS class_name, "
    "       sec.name, sec.class_teacher_id, "
    "       COALESCE(t.full_name, 'Unassigned') AS class_teacher_name, "
    "       (SELECT COUNT(*) FROM students s WHERE s.section_id = sec.id) AS student_count "
    "FROM sections sec "
    "JOIN classes c ON sec.class_id = c.id "
    "LEFT JOIN teachers t ON sec.class_teacher_id = t.id ";

static void read_section_row(sqlite3_stmt *stmt, ClassSection *s)
{
    memset(s, 0, sizeof(ClassSection));
    s->id = sqlite3_column_int(stmt, 0);
    s->class_id = sqlite3_column_int(stmt, 1);
    const char *c_name = (const char *)sqlite3_column_text(stmt, 2);
    const char *name   = (const char *)sqlite3_column_text(stmt, 3);
    s->class_teacher_id = sqlite3_column_int(stmt, 4);
    const char *t_name = (const char *)sqlite3_column_text(stmt, 5);
    s->student_count   = sqlite3_column_int(stmt, 6);

    if (c_name) strncpy(s->class_name, c_name, sizeof(s->class_name) - 1);
    if (name)   strncpy(s->name, name, sizeof(s->name) - 1);
    if (t_name) strncpy(s->class_teacher_name, t_name, sizeof(s->class_teacher_name) - 1);
}

bool section_service_get_by_class(int class_id, ClassSection *out_list, int max_items, int *out_count)
{
    if (!out_list || max_items <= 0 || !out_count || class_id <= 0) return false;
    *out_count = 0;

    sqlite3 *db = db_get_handle();
    if (!db) return false;

    char sql[1024];
    snprintf(sql, sizeof(sql), "%s WHERE sec.class_id = ? ORDER BY sec.name ASC LIMIT ?;", SECTION_SELECT);

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return false;

    sqlite3_bind_int(stmt, 1, class_id);
    sqlite3_bind_int(stmt, 2, max_items);

    int count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW && count < max_items) {
        read_section_row(stmt, &out_list[count]);
        count++;
    }
    sqlite3_finalize(stmt);
    *out_count = count;
    return true;
}

bool section_service_get_all(ClassSection *out_list, int max_items, int *out_count)
{
    if (!out_list || max_items <= 0 || !out_count) return false;
    *out_count = 0;

    sqlite3 *db = db_get_handle();
    if (!db) return false;

    char sql[1024];
    snprintf(sql, sizeof(sql), "%s ORDER BY c.numeric_value ASC, sec.name ASC LIMIT ?;", SECTION_SELECT);

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return false;

    sqlite3_bind_int(stmt, 1, max_items);
    int count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW && count < max_items) {
        read_section_row(stmt, &out_list[count]);
        count++;
    }
    sqlite3_finalize(stmt);
    *out_count = count;
    return true;
}

bool section_service_create(const ClassSection *s, int *out_new_id)
{
    if (!s || s->class_id <= 0 || strlen(s->name) == 0) return false;

    sqlite3 *db = db_get_handle();
    if (!db) return false;

    const char *sql = "INSERT INTO sections (class_id, name, class_teacher_id) VALUES (?, ?, ?);";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return false;

    sqlite3_bind_int(stmt, 1, s->class_id);
    sqlite3_bind_text(stmt, 2, s->name, -1, SQLITE_STATIC);
    if (s->class_teacher_id > 0) sqlite3_bind_int(stmt, 3, s->class_teacher_id);
    else sqlite3_bind_null(stmt, 3);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) return false;

    int new_id = (int)sqlite3_last_insert_rowid(db);
    if (out_new_id) *out_new_id = new_id;

    char details[256];
    snprintf(details, sizeof(details), "Created section %s for class ID %d", s->name, s->class_id);
    db_log_activity(0, "CREATE_SECTION", "CLASSES", details);
    return true;
}

bool section_service_update(const ClassSection *s)
{
    if (!s || s->id <= 0) return false;

    sqlite3 *db = db_get_handle();
    if (!db) return false;

    const char *sql = "UPDATE sections SET name = ?, class_teacher_id = ? WHERE id = ?;";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return false;

    sqlite3_bind_text(stmt, 1, s->name, -1, SQLITE_STATIC);
    if (s->class_teacher_id > 0) sqlite3_bind_int(stmt, 2, s->class_teacher_id);
    else sqlite3_bind_null(stmt, 2);
    sqlite3_bind_int(stmt, 3, s->id);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) return false;

    char details[256];
    snprintf(details, sizeof(details), "Updated section ID %d (%s)", s->id, s->name);
    db_log_activity(0, "UPDATE_SECTION", "CLASSES", details);
    return true;
}

bool section_service_delete(int id)
{
    if (id <= 0) return false;

    sqlite3 *db = db_get_handle();
    if (!db) return false;

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, "DELETE FROM sections WHERE id = ?;", -1, &stmt, NULL) != SQLITE_OK) {
        return false;
    }
    sqlite3_bind_int(stmt, 1, id);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) return false;

    char details[64];
    snprintf(details, sizeof(details), "Deleted section ID %d", id);
    db_log_activity(0, "DELETE_SECTION", "CLASSES", details);
    return true;
}

/* ── Subject Services ─────────────────────────────────────────────── */

static const char *SUBJECT_SELECT =
    "SELECT sub.id, sub.code, sub.name, sub.class_id, "
    "       COALESCE(c.name, 'Unassigned') AS class_name, "
    "       sub.teacher_id, "
    "       COALESCE(t.full_name, 'Unassigned') AS teacher_name "
    "FROM subjects sub "
    "JOIN classes c ON sub.class_id = c.id "
    "LEFT JOIN teachers t ON sub.teacher_id = t.id ";

static void read_subject_row(sqlite3_stmt *stmt, Subject *s)
{
    memset(s, 0, sizeof(Subject));
    s->id = sqlite3_column_int(stmt, 0);
    const char *code   = (const char *)sqlite3_column_text(stmt, 1);
    const char *name   = (const char *)sqlite3_column_text(stmt, 2);
    s->class_id        = sqlite3_column_int(stmt, 3);
    const char *c_name = (const char *)sqlite3_column_text(stmt, 4);
    s->teacher_id      = sqlite3_column_int(stmt, 5);
    const char *t_name = (const char *)sqlite3_column_text(stmt, 6);

    if (code)   strncpy(s->code, code, sizeof(s->code) - 1);
    if (name)   strncpy(s->name, name, sizeof(s->name) - 1);
    if (c_name) strncpy(s->class_name, c_name, sizeof(s->class_name) - 1);
    if (t_name) strncpy(s->teacher_name, t_name, sizeof(s->teacher_name) - 1);
}

bool subject_service_get_by_class(int class_id, Subject *out_list, int max_items, int *out_count)
{
    if (!out_list || max_items <= 0 || !out_count || class_id <= 0) return false;
    *out_count = 0;

    sqlite3 *db = db_get_handle();
    if (!db) return false;

    char sql[1024];
    snprintf(sql, sizeof(sql), "%s WHERE sub.class_id = ? ORDER BY sub.code ASC LIMIT ?;", SUBJECT_SELECT);

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return false;

    sqlite3_bind_int(stmt, 1, class_id);
    sqlite3_bind_int(stmt, 2, max_items);

    int count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW && count < max_items) {
        read_subject_row(stmt, &out_list[count]);
        count++;
    }
    sqlite3_finalize(stmt);
    *out_count = count;
    return true;
}

bool subject_service_get_all(Subject *out_list, int max_items, int *out_count)
{
    if (!out_list || max_items <= 0 || !out_count) return false;
    *out_count = 0;

    sqlite3 *db = db_get_handle();
    if (!db) return false;

    char sql[1024];
    snprintf(sql, sizeof(sql), "%s ORDER BY c.numeric_value ASC, sub.code ASC LIMIT ?;", SUBJECT_SELECT);

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return false;

    sqlite3_bind_int(stmt, 1, max_items);
    int count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW && count < max_items) {
        read_subject_row(stmt, &out_list[count]);
        count++;
    }
    sqlite3_finalize(stmt);
    *out_count = count;
    return true;
}

bool subject_service_create(const Subject *s, int *out_new_id)
{
    if (!s || s->class_id <= 0 || strlen(s->code) == 0 || strlen(s->name) == 0) return false;

    sqlite3 *db = db_get_handle();
    if (!db) return false;

    const char *sql = "INSERT INTO subjects (code, name, class_id, teacher_id) VALUES (?, ?, ?, ?);";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return false;

    sqlite3_bind_text(stmt, 1, s->code, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, s->name, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 3, s->class_id);
    if (s->teacher_id > 0) sqlite3_bind_int(stmt, 4, s->teacher_id);
    else sqlite3_bind_null(stmt, 4);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) return false;

    int new_id = (int)sqlite3_last_insert_rowid(db);
    if (out_new_id) *out_new_id = new_id;

    char details[256];
    snprintf(details, sizeof(details), "Created subject %s (%s)", s->name, s->code);
    db_log_activity(0, "CREATE_SUBJECT", "CLASSES", details);
    return true;
}

bool subject_service_update(const Subject *s)
{
    if (!s || s->id <= 0) return false;

    sqlite3 *db = db_get_handle();
    if (!db) return false;

    const char *sql = "UPDATE subjects SET code = ?, name = ?, teacher_id = ? WHERE id = ?;";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return false;

    sqlite3_bind_text(stmt, 1, s->code, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, s->name, -1, SQLITE_STATIC);
    if (s->teacher_id > 0) sqlite3_bind_int(stmt, 3, s->teacher_id);
    else sqlite3_bind_null(stmt, 3);
    sqlite3_bind_int(stmt, 4, s->id);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) return false;

    char details[256];
    snprintf(details, sizeof(details), "Updated subject ID %d (%s)", s->id, s->name);
    db_log_activity(0, "UPDATE_SUBJECT", "CLASSES", details);
    return true;
}

bool subject_service_delete(int id)
{
    if (id <= 0) return false;

    sqlite3 *db = db_get_handle();
    if (!db) return false;

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, "DELETE FROM subjects WHERE id = ?;", -1, &stmt, NULL) != SQLITE_OK) {
        return false;
    }
    sqlite3_bind_int(stmt, 1, id);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) return false;

    char details[64];
    snprintf(details, sizeof(details), "Deleted subject ID %d", id);
    db_log_activity(0, "DELETE_SUBJECT", "CLASSES", details);
    return true;
}
