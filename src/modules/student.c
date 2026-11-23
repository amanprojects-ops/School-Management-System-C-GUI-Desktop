/**
 * @file student.c
 * @brief Student management database CRUD service implementation.
 */

#include "student.h"
#include "database.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

bool student_service_generate_admission_no(char *out_admission_no, size_t size)
{
    if (!out_admission_no || size < 20) return false;

    sqlite3 *db = db_get_handle();
    if (!db) return false;

    int max_id = 0;
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, "SELECT COALESCE(MAX(id), 0) FROM students;", -1, &stmt, NULL) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            max_id = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }

    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    int year = (tm_info) ? (tm_info->tm_year + 1900) : 2026;

    snprintf(out_admission_no, size, "ADM-%d-%04d", year, max_id + 1);
    return true;
}

static void read_student_row(sqlite3_stmt *stmt, Student *s)
{
    memset(s, 0, sizeof(Student));
    s->id = sqlite3_column_int(stmt, 0);

    const char *adm_no   = (const char *)sqlite3_column_text(stmt, 1);
    const char *name     = (const char *)sqlite3_column_text(stmt, 2);
    const char *father   = (const char *)sqlite3_column_text(stmt, 3);
    const char *mother   = (const char *)sqlite3_column_text(stmt, 4);
    const char *dob      = (const char *)sqlite3_column_text(stmt, 5);
    const char *gender   = (const char *)sqlite3_column_text(stmt, 6);
    const char *category = (const char *)sqlite3_column_text(stmt, 7);
    const char *phone    = (const char *)sqlite3_column_text(stmt, 8);
    const char *email    = (const char *)sqlite3_column_text(stmt, 9);
    const char *addr     = (const char *)sqlite3_column_text(stmt, 10);
    const char *adm_dt   = (const char *)sqlite3_column_text(stmt, 11);
    s->class_id          = sqlite3_column_int(stmt, 12);
    s->section_id        = sqlite3_column_int(stmt, 13);
    s->roll_number       = sqlite3_column_int(stmt, 14);
    const char *prev_sch = (const char *)sqlite3_column_text(stmt, 15);
    const char *em_cont  = (const char *)sqlite3_column_text(stmt, 16);
    const char *b_grp    = (const char *)sqlite3_column_text(stmt, 17);
    const char *photo    = (const char *)sqlite3_column_text(stmt, 18);
    const char *status   = (const char *)sqlite3_column_text(stmt, 19);
    const char *c_name   = (const char *)sqlite3_column_text(stmt, 20);
    const char *sec_name = (const char *)sqlite3_column_text(stmt, 21);

    if (adm_no)   strncpy(s->admission_no, adm_no, sizeof(s->admission_no) - 1);
    if (name)     strncpy(s->full_name, name, sizeof(s->full_name) - 1);
    if (father)   strncpy(s->father_name, father, sizeof(s->father_name) - 1);
    if (mother)   strncpy(s->mother_name, mother, sizeof(s->mother_name) - 1);
    if (dob)      strncpy(s->dob, dob, sizeof(s->dob) - 1);
    if (gender)   strncpy(s->gender, gender, sizeof(s->gender) - 1);
    if (category) strncpy(s->category, category, sizeof(s->category) - 1);
    if (phone)    strncpy(s->phone, phone, sizeof(s->phone) - 1);
    if (email)    strncpy(s->email, email, sizeof(s->email) - 1);
    if (addr)     strncpy(s->address, addr, sizeof(s->address) - 1);
    if (adm_dt)   strncpy(s->admission_date, adm_dt, sizeof(s->admission_date) - 1);
    if (prev_sch) strncpy(s->previous_school, prev_sch, sizeof(s->previous_school) - 1);
    if (em_cont)  strncpy(s->emergency_contact, em_cont, sizeof(s->emergency_contact) - 1);
    if (b_grp)    strncpy(s->blood_group, b_grp, sizeof(s->blood_group) - 1);
    if (photo)    strncpy(s->photo_path, photo, sizeof(s->photo_path) - 1);
    if (status)   strncpy(s->status, status, sizeof(s->status) - 1);
    if (c_name)   strncpy(s->class_name, c_name, sizeof(s->class_name) - 1);
    if (sec_name) strncpy(s->section_name, sec_name, sizeof(s->section_name) - 1);
}

static const char *STUDENT_SELECT_BASE =
    "SELECT s.id, s.admission_no, s.full_name, s.father_name, s.mother_name, "
    "       s.dob, s.gender, s.category, s.phone, s.email, s.address, "
    "       s.admission_date, s.class_id, s.section_id, s.roll_number, "
    "       s.previous_school, s.emergency_contact, s.blood_group, "
    "       s.photo_path, s.status, "
    "       COALESCE(c.name, 'Unassigned') AS class_name, "
    "       COALESCE(sec.name, '-') AS section_name "
    "FROM students s "
    "LEFT JOIN classes c ON s.class_id = c.id "
    "LEFT JOIN sections sec ON s.section_id = sec.id ";

bool student_service_get_by_id(int id, Student *out_student)
{
    if (!out_student || id <= 0) return false;

    sqlite3 *db = db_get_handle();
    if (!db) return false;

    char sql[1024];
    snprintf(sql, sizeof(sql), "%s WHERE s.id = ?;", STUDENT_SELECT_BASE);

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_int(stmt, 1, id);
    bool found = false;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        read_student_row(stmt, out_student);
        found = true;
    }
    sqlite3_finalize(stmt);
    return found;
}

bool student_service_get_by_admission_no(const char *admission_no, Student *out_student)
{
    if (!out_student || !admission_no) return false;

    sqlite3 *db = db_get_handle();
    if (!db) return false;

    char sql[1024];
    snprintf(sql, sizeof(sql), "%s WHERE s.admission_no = ?;", STUDENT_SELECT_BASE);

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return false;

    sqlite3_bind_text(stmt, 1, admission_no, -1, SQLITE_STATIC);
    bool found = false;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        read_student_row(stmt, out_student);
        found = true;
    }
    sqlite3_finalize(stmt);
    return found;
}

bool student_service_create(const Student *student, int *out_new_id)
{
    if (!student || strlen(student->full_name) == 0) return false;

    sqlite3 *db = db_get_handle();
    if (!db) return false;

    char adm_no[32];
    if (strlen(student->admission_no) > 0) {
        strncpy(adm_no, student->admission_no, sizeof(adm_no) - 1);
        adm_no[sizeof(adm_no) - 1] = '\0';
    } else {
        student_service_generate_admission_no(adm_no, sizeof(adm_no));
    }

    const char *sql =
        "INSERT INTO students (admission_no, full_name, father_name, mother_name, "
        "                     dob, gender, category, phone, email, address, "
        "                     admission_date, class_id, section_id, roll_number, "
        "                     previous_school, emergency_contact, blood_group, photo_path, status) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return false;

    sqlite3_bind_text(stmt, 1, adm_no, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, student->full_name, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, student->father_name, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, student->mother_name, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 5, student->dob, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 6, (strlen(student->gender) > 0) ? student->gender : "MALE", -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 7, (strlen(student->category) > 0) ? student->category : "GENERAL", -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 8, student->phone, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 9, student->email, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 10, student->address, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 11, (strlen(student->admission_date) > 0) ? student->admission_date : "date('now')", -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 12, student->class_id);
    sqlite3_bind_int(stmt, 13, student->section_id);
    sqlite3_bind_int(stmt, 14, student->roll_number);
    sqlite3_bind_text(stmt, 15, student->previous_school, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 16, student->emergency_contact, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 17, student->blood_group, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 18, student->photo_path, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 19, (strlen(student->status) > 0) ? student->status : "ACTIVE", -1, SQLITE_STATIC);

    int rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        fprintf(stderr, "[STUDENT CREATE ERROR] %s\n", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        return false;
    }
    sqlite3_finalize(stmt);

    int new_id = (int)sqlite3_last_insert_rowid(db);
    if (out_new_id) *out_new_id = new_id;

    char details[256];
    snprintf(details, sizeof(details), "Added student %s (%s)", student->full_name, adm_no);
    db_log_activity(0, "CREATE_STUDENT", "STUDENTS", details);

    return true;
}

bool student_service_update(const Student *student)
{
    if (!student || student->id <= 0) return false;

    sqlite3 *db = db_get_handle();
    if (!db) return false;

    const char *sql =
        "UPDATE students SET full_name = ?, father_name = ?, mother_name = ?, "
        "                   dob = ?, gender = ?, category = ?, phone = ?, "
        "                   email = ?, address = ?, admission_date = ?, "
        "                   class_id = ?, section_id = ?, roll_number = ?, "
        "                   previous_school = ?, emergency_contact = ?, "
        "                   blood_group = ?, photo_path = ?, status = ?, "
        "                   updated_at = CURRENT_TIMESTAMP "
        "WHERE id = ?;";

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return false;

    sqlite3_bind_text(stmt, 1, student->full_name, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, student->father_name, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, student->mother_name, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, student->dob, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 5, student->gender, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 6, student->category, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 7, student->phone, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 8, student->email, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 9, student->address, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 10, student->admission_date, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 11, student->class_id);
    sqlite3_bind_int(stmt, 12, student->section_id);
    sqlite3_bind_int(stmt, 13, student->roll_number);
    sqlite3_bind_text(stmt, 14, student->previous_school, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 15, student->emergency_contact, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 16, student->blood_group, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 17, student->photo_path, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 18, student->status, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 19, student->id);

    int rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        fprintf(stderr, "[STUDENT CREATE ERROR] %s\n", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        return false;
    }
    sqlite3_finalize(stmt);

    char details[256];
    snprintf(details, sizeof(details), "Updated student ID %d (%s)", student->id, student->full_name);
    db_log_activity(0, "UPDATE_STUDENT", "STUDENTS", details);
    return true;
}

bool student_service_delete(int id)
{
    if (id <= 0) return false;

    sqlite3 *db = db_get_handle();
    if (!db) return false;

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, "DELETE FROM students WHERE id = ?;", -1, &stmt, NULL) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_int(stmt, 1, id);
    int rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        fprintf(stderr, "[STUDENT CREATE ERROR] %s\n", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        return false;
    }
    sqlite3_finalize(stmt);

    char details[64];
    snprintf(details, sizeof(details), "Deleted student ID %d", id);
    db_log_activity(0, "DELETE_STUDENT", "STUDENTS", details);
    return true;
}

bool student_service_get_page(const StudentFilter *filter, StudentPage *out_page)
{
    if (!out_page) return false;
    memset(out_page, 0, sizeof(StudentPage));

    sqlite3 *db = db_get_handle();
    if (!db) return false;

    int page = (filter && filter->page > 0) ? filter->page : 1;
    int page_size = (filter && filter->page_size > 0 && filter->page_size <= STUDENT_MAX_PER_PAGE)
                        ? filter->page_size
                        : STUDENT_PAGE_SIZE_DEFAULT;

    /* Build dynamic WHERE clause */
    char where_clause[1024] = "WHERE 1=1 ";
    if (filter) {
        if (strlen(filter->search_query) > 0) {
            strcat(where_clause, "AND (s.full_name LIKE ?1 OR s.admission_no LIKE ?1) ");
        }
        if (filter->class_id > 0) {
            strcat(where_clause, "AND s.class_id = ?2 ");
        }
        if (filter->section_id > 0) {
            strcat(where_clause, "AND s.section_id = ?3 ");
        }
        if (strlen(filter->gender) > 0) {
            strcat(where_clause, "AND s.gender = ?4 ");
        }
        if (strlen(filter->status) > 0) {
            strcat(where_clause, "AND s.status = ?5 ");
        }
        if (filter->admission_year > 0) {
            strcat(where_clause, "AND strftime('%Y', s.admission_date) = ?6 ");
        }
    }

    /* 1. Count total matching rows */
    char count_sql[1200];
    snprintf(count_sql, sizeof(count_sql), "SELECT COUNT(*) FROM students s %s;", where_clause);

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, count_sql, -1, &stmt, NULL) != SQLITE_OK) return false;

    char search_pattern[140] = {0};
    if (filter) {
        if (strlen(filter->search_query) > 0) {
            snprintf(search_pattern, sizeof(search_pattern), "%%%s%%", filter->search_query);
            sqlite3_bind_text(stmt, 1, search_pattern, -1, SQLITE_STATIC);
        }
        if (filter->class_id > 0) {
            sqlite3_bind_int(stmt, 2, filter->class_id);
        }
        if (filter->section_id > 0) {
            sqlite3_bind_int(stmt, 3, filter->section_id);
        }
        if (strlen(filter->gender) > 0) {
            sqlite3_bind_text(stmt, 4, filter->gender, -1, SQLITE_STATIC);
        }
        if (strlen(filter->status) > 0) {
            sqlite3_bind_text(stmt, 5, filter->status, -1, SQLITE_STATIC);
        }
        if (filter->admission_year > 0) {
            char yr_str[8];
            snprintf(yr_str, sizeof(yr_str), "%d", filter->admission_year);
            sqlite3_bind_text(stmt, 6, yr_str, -1, SQLITE_TRANSIENT);
        }
    }

    int total_records = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        total_records = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);

    out_page->total_records = total_records;
    out_page->page_size     = page_size;
    out_page->current_page  = page;
    out_page->total_pages   = (total_records + page_size - 1) / page_size;
    if (out_page->total_pages == 0) out_page->total_pages = 1;

    /* 2. Fetch paginated records */
    char select_sql[1400];
    snprintf(select_sql, sizeof(select_sql),
             "%s %s ORDER BY s.id DESC LIMIT ?7 OFFSET ?8;",
             STUDENT_SELECT_BASE, where_clause);

    stmt = NULL;
    if (sqlite3_prepare_v2(db, select_sql, -1, &stmt, NULL) != SQLITE_OK) return false;

    if (filter) {
        if (strlen(filter->search_query) > 0) {
            sqlite3_bind_text(stmt, 1, search_pattern, -1, SQLITE_STATIC);
        }
        if (filter->class_id > 0) {
            sqlite3_bind_int(stmt, 2, filter->class_id);
        }
        if (filter->section_id > 0) {
            sqlite3_bind_int(stmt, 3, filter->section_id);
        }
        if (strlen(filter->gender) > 0) {
            sqlite3_bind_text(stmt, 4, filter->gender, -1, SQLITE_STATIC);
        }
        if (strlen(filter->status) > 0) {
            sqlite3_bind_text(stmt, 5, filter->status, -1, SQLITE_STATIC);
        }
        if (filter->admission_year > 0) {
            char yr_str[8];
            snprintf(yr_str, sizeof(yr_str), "%d", filter->admission_year);
            sqlite3_bind_text(stmt, 6, yr_str, -1, SQLITE_TRANSIENT);
        }
    }

    int offset = (page - 1) * page_size;
    sqlite3_bind_int(stmt, 7, page_size);
    sqlite3_bind_int(stmt, 8, offset);

    int count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW && count < STUDENT_MAX_PER_PAGE) {
        read_student_row(stmt, &out_page->items[count]);
        count++;
    }
    sqlite3_finalize(stmt);

    out_page->count = count;
    return true;
}

bool student_service_get_for_id_cards(int class_id, int section_id, const char *status,
                                     Student *out_students, int max_students, int *out_count)
{
    if (!out_students || max_students <= 0 || !out_count) return false;
    *out_count = 0;

    sqlite3 *db = db_get_handle();
    if (!db) return false;

    char sql[1024];
    snprintf(sql, sizeof(sql),
             "%s "
             "WHERE (?1 = 0 OR s.class_id = ?1) "
             "  AND (?2 = 0 OR s.section_id = ?2) "
             "  AND (?3 = '' OR s.status = ?3) "
             "ORDER BY s.class_id ASC, s.section_id ASC, s.roll_number ASC, s.full_name ASC "
             "LIMIT ?4;",
             STUDENT_SELECT_BASE);

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return false;

    sqlite3_bind_int(stmt, 1, class_id);
    sqlite3_bind_int(stmt, 2, section_id);
    const char *status_filter = (status && strlen(status) > 0 && strcmp(status, "ALL") != 0 && strcmp(status, "All Statuses") != 0) ? status : "";
    sqlite3_bind_text(stmt, 3, status_filter, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 4, max_students);

    int count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW && count < max_students) {
        read_student_row(stmt, &out_students[count]);
        count++;
    }
    sqlite3_finalize(stmt);
    *out_count = count;
    return true;
}
