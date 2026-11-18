/**
 * @file exams.c
 * @brief Examination schedules, subject marks entry, grading, and report card generation.
 */

#include "exams.h"
#include "database.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char *exam_service_calculate_grade(double percentage)
{
    if (percentage >= 90.0) return "A+";
    if (percentage >= 80.0) return "A";
    if (percentage >= 70.0) return "B+";
    if (percentage >= 60.0) return "B";
    if (percentage >= 50.0) return "C";
    if (percentage >= 35.0) return "D";
    return "F";
}

/* ── Examination CRUD ─────────────────────────────────────────────── */

static const char *EXAM_SELECT =
    "SELECT e.id, e.name, e.academic_year_id, COALESCE(ay.name, 'Default'), "
    "       e.start_date, e.end_date, e.grading_system, e.status "
    "FROM examinations e "
    "LEFT JOIN academic_years ay ON e.academic_year_id = ay.id ";

static void read_exam_row(sqlite3_stmt *stmt, Examination *e)
{
    memset(e, 0, sizeof(Examination));
    e->id = sqlite3_column_int(stmt, 0);
    const char *name = (const char *)sqlite3_column_text(stmt, 1);
    e->academic_year_id = sqlite3_column_int(stmt, 2);
    const char *ay_name = (const char *)sqlite3_column_text(stmt, 3);
    const char *s_dt = (const char *)sqlite3_column_text(stmt, 4);
    const char *e_dt = (const char *)sqlite3_column_text(stmt, 5);
    const char *gsys = (const char *)sqlite3_column_text(stmt, 6);
    const char *st = (const char *)sqlite3_column_text(stmt, 7);

    if (name) strncpy(e->name, name, sizeof(e->name) - 1);
    if (ay_name) strncpy(e->academic_year_name, ay_name, sizeof(e->academic_year_name) - 1);
    if (s_dt) strncpy(e->start_date, s_dt, sizeof(e->start_date) - 1);
    if (e_dt) strncpy(e->end_date, e_dt, sizeof(e->end_date) - 1);
    if (gsys) strncpy(e->grading_system, gsys, sizeof(e->grading_system) - 1);
    if (st) strncpy(e->status, st, sizeof(e->status) - 1);
}

bool exam_service_get_all(Examination *out_list, int max_items, int *out_count)
{
    if (!out_list || max_items <= 0 || !out_count) return false;
    *out_count = 0;

    sqlite3 *db = db_get_handle();
    if (!db) return false;

    char sql[512];
    snprintf(sql, sizeof(sql), "%s ORDER BY e.id DESC LIMIT ?;", EXAM_SELECT);

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return false;

    sqlite3_bind_int(stmt, 1, max_items);
    int count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW && count < max_items) {
        read_exam_row(stmt, &out_list[count]);
        count++;
    }
    sqlite3_finalize(stmt);
    *out_count = count;
    return true;
}

bool exam_service_get_by_id(int id, Examination *out_exam)
{
    if (!out_exam || id <= 0) return false;

    sqlite3 *db = db_get_handle();
    if (!db) return false;

    char sql[512];
    snprintf(sql, sizeof(sql), "%s WHERE e.id = ?;", EXAM_SELECT);

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return false;

    sqlite3_bind_int(stmt, 1, id);
    bool found = false;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        read_exam_row(stmt, out_exam);
        found = true;
    }
    sqlite3_finalize(stmt);
    return found;
}

bool exam_service_create(const Examination *exam, int *out_id)
{
    if (!exam || strlen(exam->name) == 0) return false;

    sqlite3 *db = db_get_handle();
    if (!db) return false;

    int ay_id = exam->academic_year_id > 0 ? exam->academic_year_id : 1;

    const char *sql =
        "INSERT INTO examinations (name, academic_year_id, start_date, end_date, grading_system, status) "
        "VALUES (?, ?, ?, ?, ?, ?);";

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return false;

    sqlite3_bind_text(stmt, 1, exam->name, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 2, ay_id);
    sqlite3_bind_text(stmt, 3, exam->start_date, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, exam->end_date, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 5, strlen(exam->grading_system) > 0 ? exam->grading_system : "PERCENTAGE", -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 6, strlen(exam->status) > 0 ? exam->status : "PLANNED", -1, SQLITE_STATIC);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) return false;

    int new_id = (int)sqlite3_last_insert_rowid(db);
    if (out_id) *out_id = new_id;

    char details[256];
    snprintf(details, sizeof(details), "Created examination '%s' ID %d", exam->name, new_id);
    db_log_activity(0, "CREATE_EXAM", "EXAMINATIONS", details);
    return true;
}

bool exam_service_update(const Examination *exam)
{
    if (!exam || exam->id <= 0 || strlen(exam->name) == 0) return false;

    sqlite3 *db = db_get_handle();
    if (!db) return false;

    const char *sql =
        "UPDATE examinations SET name = ?, start_date = ?, end_date = ?, status = ? "
        "WHERE id = ?;";

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return false;

    sqlite3_bind_text(stmt, 1, exam->name, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, exam->start_date, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, exam->end_date, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, exam->status, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 5, exam->id);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) return false;

    char details[256];
    snprintf(details, sizeof(details), "Updated examination ID %d (%s)", exam->id, exam->name);
    db_log_activity(0, "UPDATE_EXAM", "EXAMINATIONS", details);
    return true;
}

bool exam_service_delete(int id)
{
    if (id <= 0) return false;

    sqlite3 *db = db_get_handle();
    if (!db) return false;

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, "DELETE FROM examinations WHERE id = ?;", -1, &stmt, NULL) != SQLITE_OK) {
        return false;
    }
    sqlite3_bind_int(stmt, 1, id);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) return false;

    char details[64];
    snprintf(details, sizeof(details), "Deleted examination ID %d", id);
    db_log_activity(0, "DELETE_EXAM", "EXAMINATIONS", details);
    return true;
}

/* ── Exam Subject Scheduling ───────────────────────────────────────── */

static const char *EXAM_SUBJECT_SELECT =
    "SELECT es.id, es.exam_id, e.name, es.subject_id, "
    "       sub.code, sub.name, sub.class_id, COALESCE(c.name, ''), "
    "       es.exam_date, es.max_marks, es.passing_marks "
    "FROM exam_subjects es "
    "JOIN examinations e ON es.exam_id = e.id "
    "JOIN subjects sub ON es.subject_id = sub.id "
    "JOIN classes c ON sub.class_id = c.id ";

static void read_exam_subject_row(sqlite3_stmt *stmt, ExamSubject *es)
{
    memset(es, 0, sizeof(ExamSubject));
    es->id = sqlite3_column_int(stmt, 0);
    es->exam_id = sqlite3_column_int(stmt, 1);
    const char *e_name = (const char *)sqlite3_column_text(stmt, 2);
    es->subject_id = sqlite3_column_int(stmt, 3);
    const char *s_code = (const char *)sqlite3_column_text(stmt, 4);
    const char *s_name = (const char *)sqlite3_column_text(stmt, 5);
    es->class_id = sqlite3_column_int(stmt, 6);
    const char *c_name = (const char *)sqlite3_column_text(stmt, 7);
    const char *e_dt = (const char *)sqlite3_column_text(stmt, 8);
    es->max_marks = sqlite3_column_double(stmt, 9);
    es->passing_marks = sqlite3_column_double(stmt, 10);

    if (e_name) strncpy(es->exam_name, e_name, sizeof(es->exam_name) - 1);
    if (s_code) strncpy(es->subject_code, s_code, sizeof(es->subject_code) - 1);
    if (s_name) strncpy(es->subject_name, s_name, sizeof(es->subject_name) - 1);
    if (c_name) strncpy(es->class_name, c_name, sizeof(es->class_name) - 1);
    if (e_dt) strncpy(es->exam_date, e_dt, sizeof(es->exam_date) - 1);
}

bool exam_subject_create(const ExamSubject *es, int *out_id)
{
    if (!es || es->exam_id <= 0 || es->subject_id <= 0) return false;

    sqlite3 *db = db_get_handle();
    if (!db) return false;

    const char *sql =
        "INSERT INTO exam_subjects (exam_id, subject_id, exam_date, max_marks, passing_marks) "
        "VALUES (?, ?, ?, ?, ?);";

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return false;

    sqlite3_bind_int(stmt, 1, es->exam_id);
    sqlite3_bind_int(stmt, 2, es->subject_id);
    sqlite3_bind_text(stmt, 3, es->exam_date, -1, SQLITE_STATIC);
    sqlite3_bind_double(stmt, 4, es->max_marks > 0.0 ? es->max_marks : 100.0);
    sqlite3_bind_double(stmt, 5, es->passing_marks > 0.0 ? es->passing_marks : 35.0);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) return false;

    int new_id = (int)sqlite3_last_insert_rowid(db);
    if (out_id) *out_id = new_id;

    char details[256];
    snprintf(details, sizeof(details), "Scheduled subject ID %d for exam ID %d", es->subject_id, es->exam_id);
    db_log_activity(0, "SCHEDULE_EXAM_SUBJECT", "EXAMINATIONS", details);
    return true;
}

bool exam_subject_get_by_exam(int exam_id, ExamSubject *out_list, int max_items, int *out_count)
{
    if (!out_list || max_items <= 0 || !out_count || exam_id <= 0) return false;
    *out_count = 0;

    sqlite3 *db = db_get_handle();
    if (!db) return false;

    char sql[1024];
    snprintf(sql, sizeof(sql), "%s WHERE es.exam_id = ? ORDER BY es.exam_date ASC, sub.code ASC LIMIT ?;", EXAM_SUBJECT_SELECT);

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return false;

    sqlite3_bind_int(stmt, 1, exam_id);
    sqlite3_bind_int(stmt, 2, max_items);

    int count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW && count < max_items) {
        read_exam_subject_row(stmt, &out_list[count]);
        count++;
    }
    sqlite3_finalize(stmt);
    *out_count = count;
    return true;
}

bool exam_subject_delete(int id)
{
    if (id <= 0) return false;

    sqlite3 *db = db_get_handle();
    if (!db) return false;

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, "DELETE FROM exam_subjects WHERE id = ?;", -1, &stmt, NULL) != SQLITE_OK) {
        return false;
    }
    sqlite3_bind_int(stmt, 1, id);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return (rc == SQLITE_DONE);
}

/* ── Marks Entry & Results ─────────────────────────────────────────── */

bool exam_result_record(ExamResult *res, int *out_id)
{
    if (!res || res->exam_subject_id <= 0 || res->student_id <= 0) return false;

    sqlite3 *db = db_get_handle();
    if (!db) return false;

    /* 1. Fetch ExamSubject max and passing marks if not populated */
    if (res->max_marks <= 0.0) {
        sqlite3_stmt *es_stmt = NULL;
        if (sqlite3_prepare_v2(db, "SELECT max_marks, passing_marks FROM exam_subjects WHERE id = ?;", -1, &es_stmt, NULL) == SQLITE_OK) {
            sqlite3_bind_int(es_stmt, 1, res->exam_subject_id);
            if (sqlite3_step(es_stmt) == SQLITE_ROW) {
                res->max_marks = sqlite3_column_double(es_stmt, 0);
                res->passing_marks = sqlite3_column_double(es_stmt, 1);
            }
            sqlite3_finalize(es_stmt);
        }
    }
    if (res->max_marks <= 0.0) res->max_marks = 100.0;
    if (res->passing_marks <= 0.0) res->passing_marks = 35.0;

    /* 2. Compute Grade and Pass/Fail */
    double pct = (res->marks_obtained / res->max_marks) * 100.0;
    const char *calculated_grade = exam_service_calculate_grade(pct);
    strncpy(res->grade, calculated_grade, sizeof(res->grade) - 1);
    res->is_pass = (res->marks_obtained >= res->passing_marks);

    /* 3. Upsert */
    const char *sql =
        "INSERT INTO exam_results (exam_subject_id, student_id, marks_obtained, grade, remarks) "
        "VALUES (?, ?, ?, ?, ?) "
        "ON CONFLICT(exam_subject_id, student_id) DO UPDATE SET "
        "    marks_obtained = excluded.marks_obtained, "
        "    grade = excluded.grade, "
        "    remarks = excluded.remarks;";

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return false;

    sqlite3_bind_int(stmt, 1, res->exam_subject_id);
    sqlite3_bind_int(stmt, 2, res->student_id);
    sqlite3_bind_double(stmt, 3, res->marks_obtained);
    sqlite3_bind_text(stmt, 4, res->grade, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 5, res->remarks, -1, SQLITE_STATIC);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) return false;

    int new_id = (int)sqlite3_last_insert_rowid(db);
    res->id = new_id;
    if (out_id) *out_id = new_id;

    char details[256];
    snprintf(details, sizeof(details), "Recorded marks %.1f (%s) for student ID %d, exam_subject ID %d",
             res->marks_obtained, res->grade, res->student_id, res->exam_subject_id);
    db_log_activity(0, "RECORD_MARKS", "EXAMINATIONS", details);
    return true;
}

bool exam_result_get_class_sheet(int exam_subject_id, ExamResult *out_list, int max_items, int *out_count)
{
    if (!out_list || max_items <= 0 || !out_count || exam_subject_id <= 0) return false;
    *out_count = 0;

    sqlite3 *db = db_get_handle();
    if (!db) return false;

    /* Get ExamSubject details to identify class */
    const char *meta_sql =
        "SELECT es.exam_id, e.name, sub.code, sub.name, sub.class_id, "
        "       es.max_marks, es.passing_marks "
        "FROM exam_subjects es "
        "JOIN examinations e ON es.exam_id = e.id "
        "JOIN subjects sub ON es.subject_id = sub.id "
        "WHERE es.id = ?;";

    sqlite3_stmt *meta_stmt = NULL;
    if (sqlite3_prepare_v2(db, meta_sql, -1, &meta_stmt, NULL) != SQLITE_OK) return false;

    sqlite3_bind_int(meta_stmt, 1, exam_subject_id);
    if (sqlite3_step(meta_stmt) != SQLITE_ROW) {
        sqlite3_finalize(meta_stmt);
        return false;
    }

    int exam_id = sqlite3_column_int(meta_stmt, 0);
    const char *exam_name = (const char *)sqlite3_column_text(meta_stmt, 1);
    const char *sub_code = (const char *)sqlite3_column_text(meta_stmt, 2);
    const char *sub_name = (const char *)sqlite3_column_text(meta_stmt, 3);
    int class_id = sqlite3_column_int(meta_stmt, 4);
    double max_marks = sqlite3_column_double(meta_stmt, 5);
    double passing_marks = sqlite3_column_double(meta_stmt, 6);

    char ex_name_buf[128], scode_buf[32], sname_buf[128];
    strncpy(ex_name_buf, exam_name ? exam_name : "", sizeof(ex_name_buf) - 1);
    strncpy(scode_buf, sub_code ? sub_code : "", sizeof(scode_buf) - 1);
    strncpy(sname_buf, sub_name ? sub_name : "", sizeof(sname_buf) - 1);
    sqlite3_finalize(meta_stmt);

    /* Query active students in class and LEFT JOIN exam_results */
    const char *sheet_sql =
        "SELECT s.id, s.admission_no, s.full_name, s.roll_number, "
        "       COALESCE(c.name, ''), COALESCE(sec.name, ''), "
        "       COALESCE(er.id, 0), COALESCE(er.marks_obtained, 0.0), "
        "       COALESCE(er.grade, '-'), COALESCE(er.remarks, '') "
        "FROM students s "
        "JOIN classes c ON s.class_id = c.id "
        "LEFT JOIN sections sec ON s.section_id = sec.id "
        "LEFT JOIN exam_results er ON s.id = er.student_id AND er.exam_subject_id = ? "
        "WHERE s.status = 'ACTIVE' AND s.class_id = ? "
        "ORDER BY s.roll_number ASC, s.full_name ASC LIMIT ?;";

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sheet_sql, -1, &stmt, NULL) != SQLITE_OK) return false;

    sqlite3_bind_int(stmt, 1, exam_subject_id);
    sqlite3_bind_int(stmt, 2, class_id);
    sqlite3_bind_int(stmt, 3, max_items);

    int count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW && count < max_items) {
        ExamResult *r = &out_list[count];
        memset(r, 0, sizeof(ExamResult));

        r->student_id = sqlite3_column_int(stmt, 0);
        const char *adm = (const char *)sqlite3_column_text(stmt, 1);
        const char *name = (const char *)sqlite3_column_text(stmt, 2);
        r->roll_number = sqlite3_column_int(stmt, 3);
        const char *cname = (const char *)sqlite3_column_text(stmt, 4);
        const char *sname = (const char *)sqlite3_column_text(stmt, 5);
        r->id = sqlite3_column_int(stmt, 6);
        r->marks_obtained = sqlite3_column_double(stmt, 7);
        const char *grd = (const char *)sqlite3_column_text(stmt, 8);
        const char *rm = (const char *)sqlite3_column_text(stmt, 9);

        r->exam_subject_id = exam_subject_id;
        r->exam_id = exam_id;
        r->class_id = class_id;
        r->max_marks = max_marks;
        r->passing_marks = passing_marks;
        r->is_pass = (r->marks_obtained >= passing_marks);

        strncpy(r->exam_name, ex_name_buf, sizeof(r->exam_name) - 1);
        strncpy(r->subject_code, scode_buf, sizeof(r->subject_code) - 1);
        strncpy(r->subject_name, sname_buf, sizeof(r->subject_name) - 1);
        if (adm) strncpy(r->admission_no, adm, sizeof(r->admission_no) - 1);
        if (name) strncpy(r->student_name, name, sizeof(r->student_name) - 1);
        if (cname) strncpy(r->class_name, cname, sizeof(r->class_name) - 1);
        if (sname) strncpy(r->section_name, sname, sizeof(r->section_name) - 1);
        if (grd) strncpy(r->grade, grd, sizeof(r->grade) - 1);
        if (rm) strncpy(r->remarks, rm, sizeof(r->remarks) - 1);

        count++;
    }
    sqlite3_finalize(stmt);
    *out_count = count;
    return true;
}

/* ── Report Card & Class Ranking ───────────────────────────────────── */

bool exam_service_get_class_ranks(int exam_id, int class_id,
                                  ClassExamRank *out_ranks, int max_ranks, int *out_count)
{
    if (!out_ranks || max_ranks <= 0 || !out_count || exam_id <= 0 || class_id <= 0) return false;
    *out_count = 0;

    sqlite3 *db = db_get_handle();
    if (!db) return false;

    /* Compute total obtained and total max for all students in class */
    const char *sql =
        "SELECT s.id, s.admission_no, s.full_name, s.roll_number, "
        "       COALESCE(SUM(er.marks_obtained), 0.0) AS total_obtained, "
        "       COALESCE(SUM(es.max_marks), 0.0) AS total_max "
        "FROM students s "
        "JOIN exam_subjects es ON es.exam_id = ? "
        "JOIN subjects sub ON es.subject_id = sub.id AND sub.class_id = s.class_id "
        "LEFT JOIN exam_results er ON er.exam_subject_id = es.id AND er.student_id = s.id "
        "WHERE s.status = 'ACTIVE' AND s.class_id = ? "
        "GROUP BY s.id "
        "ORDER BY (CASE WHEN SUM(es.max_marks) > 0 THEN (SUM(er.marks_obtained)*1.0 / SUM(es.max_marks)) ELSE 0 END) DESC, s.roll_number ASC "
        "LIMIT ?;";

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return false;

    sqlite3_bind_int(stmt, 1, exam_id);
    sqlite3_bind_int(stmt, 2, class_id);
    sqlite3_bind_int(stmt, 3, max_ranks);

    int count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW && count < max_ranks) {
        ClassExamRank *cr = &out_ranks[count];
        memset(cr, 0, sizeof(ClassExamRank));

        cr->student_id = sqlite3_column_int(stmt, 0);
        const char *adm = (const char *)sqlite3_column_text(stmt, 1);
        const char *name = (const char *)sqlite3_column_text(stmt, 2);
        cr->roll_number = sqlite3_column_int(stmt, 3);
        cr->total_obtained = sqlite3_column_double(stmt, 4);
        cr->total_max = sqlite3_column_double(stmt, 5);

        if (adm) strncpy(cr->admission_no, adm, sizeof(cr->admission_no) - 1);
        if (name) strncpy(cr->student_name, name, sizeof(cr->student_name) - 1);

        if (cr->total_max > 0.0) {
            cr->percentage = (cr->total_obtained / cr->total_max) * 100.0;
        } else {
            cr->percentage = 0.0;
        }

        strncpy(cr->overall_grade, exam_service_calculate_grade(cr->percentage), sizeof(cr->overall_grade) - 1);
        cr->is_pass = (cr->percentage >= 35.0);
        cr->rank = count + 1;

        count++;
    }
    sqlite3_finalize(stmt);
    *out_count = count;
    return true;
}

bool exam_service_generate_report_card(int exam_id, int student_id, StudentReportCard *out_card)
{
    if (!out_card || exam_id <= 0 || student_id <= 0) return false;
    memset(out_card, 0, sizeof(StudentReportCard));

    sqlite3 *db = db_get_handle();
    if (!db) return false;

    /* 1. Student Details */
    const char *stu_sql =
        "SELECT s.id, s.admission_no, s.full_name, s.roll_number, "
        "       s.class_id, COALESCE(c.name, ''), COALESCE(sec.name, '') "
        "FROM students s "
        "LEFT JOIN classes c ON s.class_id = c.id "
        "LEFT JOIN sections sec ON s.section_id = sec.id "
        "WHERE s.id = ?;";

    sqlite3_stmt *stu_stmt = NULL;
    if (sqlite3_prepare_v2(db, stu_sql, -1, &stu_stmt, NULL) != SQLITE_OK) return false;

    sqlite3_bind_int(stu_stmt, 1, student_id);
    if (sqlite3_step(stu_stmt) != SQLITE_ROW) {
        sqlite3_finalize(stu_stmt);
        return false;
    }

    out_card->student_id = sqlite3_column_int(stu_stmt, 0);
    const char *adm = (const char *)sqlite3_column_text(stu_stmt, 1);
    const char *name = (const char *)sqlite3_column_text(stu_stmt, 2);
    out_card->roll_number = sqlite3_column_int(stu_stmt, 3);
    int class_id = sqlite3_column_int(stu_stmt, 4);
    const char *cname = (const char *)sqlite3_column_text(stu_stmt, 5);
    const char *sname = (const char *)sqlite3_column_text(stu_stmt, 6);

    if (adm) strncpy(out_card->admission_no, adm, sizeof(out_card->admission_no) - 1);
    if (name) strncpy(out_card->student_name, name, sizeof(out_card->student_name) - 1);
    if (cname) strncpy(out_card->class_name, cname, sizeof(out_card->class_name) - 1);
    if (sname) strncpy(out_card->section_name, sname, sizeof(out_card->section_name) - 1);
    sqlite3_finalize(stu_stmt);

    /* 2. Exam Details */
    Examination exam;
    if (exam_service_get_by_id(exam_id, &exam)) {
        strncpy(out_card->exam_name, exam.name, sizeof(out_card->exam_name) - 1);
        strncpy(out_card->academic_year, exam.academic_year_name, sizeof(out_card->academic_year) - 1);
    }

    /* 3. Subject scores */
    const char *score_sql =
        "SELECT sub.code, sub.name, es.max_marks, es.passing_marks, "
        "       COALESCE(er.marks_obtained, 0.0), COALESCE(er.grade, 'F'), "
        "       COALESCE(er.remarks, '') "
        "FROM exam_subjects es "
        "JOIN subjects sub ON es.subject_id = sub.id "
        "LEFT JOIN exam_results er ON er.exam_subject_id = es.id AND er.student_id = ? "
        "WHERE es.exam_id = ? AND sub.class_id = ? "
        "ORDER BY sub.code ASC;";

    sqlite3_stmt *sc_stmt = NULL;
    if (sqlite3_prepare_v2(db, score_sql, -1, &sc_stmt, NULL) != SQLITE_OK) return false;

    sqlite3_bind_int(sc_stmt, 1, student_id);
    sqlite3_bind_int(sc_stmt, 2, exam_id);
    sqlite3_bind_int(sc_stmt, 3, class_id);

    int count = 0;
    bool all_passed = true;

    while (sqlite3_step(sc_stmt) == SQLITE_ROW && count < MAX_REPORT_CARD_SUBJECTS) {
        SubjectScore *sc = &out_card->scores[count];
        memset(sc, 0, sizeof(SubjectScore));

        const char *code = (const char *)sqlite3_column_text(sc_stmt, 0);
        const char *s_name = (const char *)sqlite3_column_text(sc_stmt, 1);
        sc->max_marks = sqlite3_column_double(sc_stmt, 2);
        sc->passing_marks = sqlite3_column_double(sc_stmt, 3);
        sc->marks_obtained = sqlite3_column_double(sc_stmt, 4);
        const char *grd = (const char *)sqlite3_column_text(sc_stmt, 5);
        const char *rm = (const char *)sqlite3_column_text(sc_stmt, 6);

        if (code) strncpy(sc->code, code, sizeof(sc->code) - 1);
        if (s_name) strncpy(sc->subject_name, s_name, sizeof(sc->subject_name) - 1);
        if (grd) strncpy(sc->grade, grd, sizeof(sc->grade) - 1);
        if (rm) strncpy(sc->remarks, rm, sizeof(sc->remarks) - 1);

        sc->is_pass = (sc->marks_obtained >= sc->passing_marks);
        if (!sc->is_pass) all_passed = false;

        out_card->total_marks_obtained += sc->marks_obtained;
        out_card->total_max_marks += sc->max_marks;
        count++;
    }
    sqlite3_finalize(sc_stmt);
    out_card->score_count = count;

    if (out_card->total_max_marks > 0.0) {
        out_card->percentage = (out_card->total_marks_obtained / out_card->total_max_marks) * 100.0;
    } else {
        out_card->percentage = 0.0;
    }

    strncpy(out_card->overall_grade, exam_service_calculate_grade(out_card->percentage), sizeof(out_card->overall_grade) - 1);
    out_card->is_pass = (all_passed && out_card->percentage >= 35.0);

    /* 4. Find Rank */
    ClassExamRank ranks[MAX_CLASS_RANKS];
    int rank_count = 0;
    if (exam_service_get_class_ranks(exam_id, class_id, ranks, MAX_CLASS_RANKS, &rank_count)) {
        for (int i = 0; i < rank_count; i++) {
            if (ranks[i].student_id == student_id) {
                out_card->class_rank = ranks[i].rank;
                break;
            }
        }
    }

    return true;
}
