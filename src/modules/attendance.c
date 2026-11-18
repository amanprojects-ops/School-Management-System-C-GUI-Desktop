/**
 * @file attendance.c
 * @brief Attendance management database operations and statistical aggregations.
 */

#include "attendance.h"
#include "database.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int get_days_in_month(int year, int month)
{
    if (month < 1 || month > 12) return 30;
    if (month == 2) {
        bool is_leap = ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0));
        return is_leap ? 29 : 28;
    }
    if (month == 4 || month == 6 || month == 9 || month == 11) return 30;
    return 31;
}

bool attendance_service_mark(const AttendanceRecord *rec, int *out_id)
{
    if (!rec || rec->student_id <= 0 || strlen(rec->date) == 0 || strlen(rec->status) == 0) {
        return false;
    }

    sqlite3 *db = db_get_handle();
    if (!db) return false;

    const char *sql =
        "INSERT INTO attendance (student_id, class_id, section_id, date, status, remarks, recorded_by) "
        "VALUES (?, ?, ?, ?, ?, ?, ?) "
        "ON CONFLICT(student_id, date) DO UPDATE SET "
        "    status = excluded.status, "
        "    remarks = excluded.remarks, "
        "    recorded_by = excluded.recorded_by;";

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_int(stmt, 1, rec->student_id);
    sqlite3_bind_int(stmt, 2, rec->class_id);
    if (rec->section_id > 0) sqlite3_bind_int(stmt, 3, rec->section_id);
    else sqlite3_bind_null(stmt, 3);
    sqlite3_bind_text(stmt, 4, rec->date, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 5, rec->status, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 6, rec->remarks, -1, SQLITE_STATIC);
    if (rec->recorded_by > 0) sqlite3_bind_int(stmt, 7, rec->recorded_by);
    else sqlite3_bind_null(stmt, 7);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) return false;

    int record_id = (int)sqlite3_last_insert_rowid(db);
    if (out_id) {
        if (record_id > 0) {
            *out_id = record_id;
        } else {
            /* If updated via UPSERT, query the ID */
            sqlite3_stmt *id_stmt = NULL;
            if (sqlite3_prepare_v2(db, "SELECT id FROM attendance WHERE student_id = ? AND date = ?;", -1, &id_stmt, NULL) == SQLITE_OK) {
                sqlite3_bind_int(id_stmt, 1, rec->student_id);
                sqlite3_bind_text(id_stmt, 2, rec->date, -1, SQLITE_STATIC);
                if (sqlite3_step(id_stmt) == SQLITE_ROW) {
                    *out_id = sqlite3_column_int(id_stmt, 0);
                }
                sqlite3_finalize(id_stmt);
            }
        }
    }

    char details[256];
    snprintf(details, sizeof(details), "Marked attendance for student ID %d on %s as %s",
             rec->student_id, rec->date, rec->status);
    db_log_activity(rec->recorded_by, "MARK_ATTENDANCE", "ATTENDANCE", details);
    return true;
}

bool attendance_service_mark_bulk(int class_id, int section_id, const char *date,
                                  const AttendanceEntry *entries, int count,
                                  int recorded_by)
{
    if (!entries || count <= 0 || !date || strlen(date) == 0 || class_id <= 0) {
        return false;
    }

    sqlite3 *db = db_get_handle();
    if (!db) return false;

    if (db_begin_transaction() != DB_SUCCESS) return false;

    const char *sql =
        "INSERT INTO attendance (student_id, class_id, section_id, date, status, remarks, recorded_by) "
        "VALUES (?, ?, ?, ?, ?, ?, ?) "
        "ON CONFLICT(student_id, date) DO UPDATE SET "
        "    status = excluded.status, "
        "    remarks = excluded.remarks, "
        "    recorded_by = excluded.recorded_by;";

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        db_rollback();
        return false;
    }

    for (int i = 0; i < count; i++) {
        sqlite3_reset(stmt);
        sqlite3_clear_bindings(stmt);

        sqlite3_bind_int(stmt, 1, entries[i].student_id);
        sqlite3_bind_int(stmt, 2, class_id);
        if (section_id > 0) sqlite3_bind_int(stmt, 3, section_id);
        else sqlite3_bind_null(stmt, 3);
        sqlite3_bind_text(stmt, 4, date, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 5, entries[i].status, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 6, entries[i].remarks, -1, SQLITE_STATIC);
        if (recorded_by > 0) sqlite3_bind_int(stmt, 7, recorded_by);
        else sqlite3_bind_null(stmt, 7);

        int rc = sqlite3_step(stmt);
        if (rc != SQLITE_DONE) {
            sqlite3_finalize(stmt);
            db_rollback();
            return false;
        }
    }

    sqlite3_finalize(stmt);
    if (db_commit() != DB_SUCCESS) return false;

    char details[256];
    snprintf(details, sizeof(details), "Bulk marked attendance for class ID %d (%d students) on %s",
             class_id, count, date);
    db_log_activity(recorded_by, "BULK_ATTENDANCE", "ATTENDANCE", details);
    return true;
}

bool attendance_service_get_roster(int class_id, int section_id, const char *date,
                                   AttendanceRecord *out_list, int max_items, int *out_count)
{
    if (!out_list || max_items <= 0 || !out_count || !date || class_id <= 0) {
        return false;
    }
    *out_count = 0;

    sqlite3 *db = db_get_handle();
    if (!db) return false;

    char sql[1024];
    if (section_id > 0) {
        snprintf(sql, sizeof(sql),
            "SELECT COALESCE(a.id, 0), s.id, s.admission_no, s.full_name, s.roll_number, "
            "       s.class_id, COALESCE(c.name, ''), s.section_id, COALESCE(sec.name, ''), "
            "       COALESCE(a.status, 'UNMARKED'), COALESCE(a.remarks, ''), "
            "       COALESCE(a.recorded_by, 0), COALESCE(a.created_at, '') "
            "FROM students s "
            "JOIN classes c ON s.class_id = c.id "
            "LEFT JOIN sections sec ON s.section_id = sec.id "
            "LEFT JOIN attendance a ON s.id = a.student_id AND a.date = ? "
            "WHERE s.status = 'ACTIVE' AND s.class_id = ? AND s.section_id = ? "
            "ORDER BY s.roll_number ASC, s.full_name ASC LIMIT ?;");
    } else {
        snprintf(sql, sizeof(sql),
            "SELECT COALESCE(a.id, 0), s.id, s.admission_no, s.full_name, s.roll_number, "
            "       s.class_id, COALESCE(c.name, ''), s.section_id, COALESCE(sec.name, ''), "
            "       COALESCE(a.status, 'UNMARKED'), COALESCE(a.remarks, ''), "
            "       COALESCE(a.recorded_by, 0), COALESCE(a.created_at, '') "
            "FROM students s "
            "JOIN classes c ON s.class_id = c.id "
            "LEFT JOIN sections sec ON s.section_id = sec.id "
            "LEFT JOIN attendance a ON s.id = a.student_id AND a.date = ? "
            "WHERE s.status = 'ACTIVE' AND s.class_id = ? "
            "ORDER BY s.roll_number ASC, s.full_name ASC LIMIT ?;");
    }

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_text(stmt, 1, date, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 2, class_id);
    if (section_id > 0) {
        sqlite3_bind_int(stmt, 3, section_id);
        sqlite3_bind_int(stmt, 4, max_items);
    } else {
        sqlite3_bind_int(stmt, 3, max_items);
    }

    int count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW && count < max_items) {
        AttendanceRecord *r = &out_list[count];
        memset(r, 0, sizeof(AttendanceRecord));

        r->id = sqlite3_column_int(stmt, 0);
        r->student_id = sqlite3_column_int(stmt, 1);

        const char *adm = (const char *)sqlite3_column_text(stmt, 2);
        const char *name = (const char *)sqlite3_column_text(stmt, 3);
        r->roll_number = sqlite3_column_int(stmt, 4);
        r->class_id = sqlite3_column_int(stmt, 5);
        const char *cname = (const char *)sqlite3_column_text(stmt, 6);
        r->section_id = sqlite3_column_int(stmt, 7);
        const char *sname = (const char *)sqlite3_column_text(stmt, 8);
        const char *st = (const char *)sqlite3_column_text(stmt, 9);
        const char *rm = (const char *)sqlite3_column_text(stmt, 10);
        r->recorded_by = sqlite3_column_int(stmt, 11);
        const char *cat = (const char *)sqlite3_column_text(stmt, 12);

        strncpy(r->date, date, sizeof(r->date) - 1);
        if (adm) strncpy(r->admission_no, adm, sizeof(r->admission_no) - 1);
        if (name) strncpy(r->student_name, name, sizeof(r->student_name) - 1);
        if (cname) strncpy(r->class_name, cname, sizeof(r->class_name) - 1);
        if (sname) strncpy(r->section_name, sname, sizeof(r->section_name) - 1);
        if (st) strncpy(r->status, st, sizeof(r->status) - 1);
        if (rm) strncpy(r->remarks, rm, sizeof(r->remarks) - 1);
        if (cat) strncpy(r->created_at, cat, sizeof(r->created_at) - 1);

        count++;
    }

    sqlite3_finalize(stmt);
    *out_count = count;
    return true;
}

bool attendance_service_get_student_stats(int student_id,
                                          const char *start_date,
                                          const char *end_date,
                                          AttendanceStats *out_stats)
{
    if (!out_stats || student_id <= 0) return false;
    memset(out_stats, 0, sizeof(AttendanceStats));

    sqlite3 *db = db_get_handle();
    if (!db) return false;

    char sql[512];
    if (start_date && end_date && strlen(start_date) > 0 && strlen(end_date) > 0) {
        snprintf(sql, sizeof(sql),
            "SELECT status, COUNT(*) FROM attendance "
            "WHERE student_id = ? AND date >= ? AND date <= ? "
            "GROUP BY status;");
    } else {
        snprintf(sql, sizeof(sql),
            "SELECT status, COUNT(*) FROM attendance "
            "WHERE student_id = ? "
            "GROUP BY status;");
    }

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return false;

    sqlite3_bind_int(stmt, 1, student_id);
    if (start_date && end_date && strlen(start_date) > 0 && strlen(end_date) > 0) {
        sqlite3_bind_text(stmt, 2, start_date, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 3, end_date, -1, SQLITE_STATIC);
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char *st = (const char *)sqlite3_column_text(stmt, 0);
        int c = sqlite3_column_int(stmt, 1);
        if (!st) continue;

        if (strcmp(st, ATTENDANCE_STATUS_PRESENT) == 0) out_stats->present_count += c;
        else if (strcmp(st, ATTENDANCE_STATUS_ABSENT) == 0) out_stats->absent_count += c;
        else if (strcmp(st, ATTENDANCE_STATUS_LATE) == 0) out_stats->late_count += c;
        else if (strcmp(st, ATTENDANCE_STATUS_HALF_DAY) == 0) out_stats->half_day_count += c;
    }
    sqlite3_finalize(stmt);

    out_stats->total_days = out_stats->present_count + out_stats->absent_count +
                            out_stats->late_count + out_stats->half_day_count;

    if (out_stats->total_days > 0) {
        double effective_present = (double)out_stats->present_count +
                                   (double)out_stats->late_count +
                                   0.5 * (double)out_stats->half_day_count;
        out_stats->percentage = (effective_present / (double)out_stats->total_days) * 100.0;
    } else {
        out_stats->percentage = 0.0;
    }

    return true;
}

bool attendance_service_get_class_stats(int class_id, int section_id, const char *date,
                                        AttendanceStats *out_stats)
{
    if (!out_stats || class_id <= 0 || !date || strlen(date) == 0) return false;
    memset(out_stats, 0, sizeof(AttendanceStats));

    sqlite3 *db = db_get_handle();
    if (!db) return false;

    char sql[512];
    if (section_id > 0) {
        snprintf(sql, sizeof(sql),
            "SELECT a.status, COUNT(*) FROM attendance a "
            "WHERE a.class_id = ? AND a.section_id = ? AND a.date = ? "
            "GROUP BY a.status;");
    } else {
        snprintf(sql, sizeof(sql),
            "SELECT a.status, COUNT(*) FROM attendance a "
            "WHERE a.class_id = ? AND a.date = ? "
            "GROUP BY a.status;");
    }

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return false;

    sqlite3_bind_int(stmt, 1, class_id);
    if (section_id > 0) {
        sqlite3_bind_int(stmt, 2, section_id);
        sqlite3_bind_text(stmt, 3, date, -1, SQLITE_STATIC);
    } else {
        sqlite3_bind_text(stmt, 2, date, -1, SQLITE_STATIC);
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char *st = (const char *)sqlite3_column_text(stmt, 0);
        int c = sqlite3_column_int(stmt, 1);
        if (!st) continue;

        if (strcmp(st, ATTENDANCE_STATUS_PRESENT) == 0) out_stats->present_count += c;
        else if (strcmp(st, ATTENDANCE_STATUS_ABSENT) == 0) out_stats->absent_count += c;
        else if (strcmp(st, ATTENDANCE_STATUS_LATE) == 0) out_stats->late_count += c;
        else if (strcmp(st, ATTENDANCE_STATUS_HALF_DAY) == 0) out_stats->half_day_count += c;
    }
    sqlite3_finalize(stmt);

    out_stats->total_days = out_stats->present_count + out_stats->absent_count +
                            out_stats->late_count + out_stats->half_day_count;

    if (out_stats->total_days > 0) {
        double effective_present = (double)out_stats->present_count +
                                   (double)out_stats->late_count +
                                   0.5 * (double)out_stats->half_day_count;
        out_stats->percentage = (effective_present / (double)out_stats->total_days) * 100.0;
    } else {
        out_stats->percentage = 0.0;
    }

    return true;
}

bool attendance_service_get_monthly_report(int class_id, int section_id,
                                           int year, int month,
                                           AttendanceMonthlyReport *out_reports,
                                           int max_students, int *out_count)
{
    if (!out_reports || max_students <= 0 || !out_count || class_id <= 0 || month < 1 || month > 12) {
        return false;
    }
    *out_count = 0;

    sqlite3 *db = db_get_handle();
    if (!db) return false;

    int days_in_month = get_days_in_month(year, month);

    /* 1. Fetch Students in Class */
    char stu_sql[512];
    if (section_id > 0) {
        snprintf(stu_sql, sizeof(stu_sql),
            "SELECT s.id, s.admission_no, s.full_name, s.roll_number, "
            "       COALESCE(c.name, ''), COALESCE(sec.name, '') "
            "FROM students s "
            "JOIN classes c ON s.class_id = c.id "
            "LEFT JOIN sections sec ON s.section_id = sec.id "
            "WHERE s.status = 'ACTIVE' AND s.class_id = ? AND s.section_id = ? "
            "ORDER BY s.roll_number ASC, s.full_name ASC LIMIT ?;");
    } else {
        snprintf(stu_sql, sizeof(stu_sql),
            "SELECT s.id, s.admission_no, s.full_name, s.roll_number, "
            "       COALESCE(c.name, ''), COALESCE(sec.name, '') "
            "FROM students s "
            "JOIN classes c ON s.class_id = c.id "
            "LEFT JOIN sections sec ON s.section_id = sec.id "
            "WHERE s.status = 'ACTIVE' AND s.class_id = ? "
            "ORDER BY s.roll_number ASC, s.full_name ASC LIMIT ?;");
    }

    sqlite3_stmt *stu_stmt = NULL;
    if (sqlite3_prepare_v2(db, stu_sql, -1, &stu_stmt, NULL) != SQLITE_OK) return false;

    sqlite3_bind_int(stu_stmt, 1, class_id);
    if (section_id > 0) {
        sqlite3_bind_int(stu_stmt, 2, section_id);
        sqlite3_bind_int(stu_stmt, 3, max_students);
    } else {
        sqlite3_bind_int(stu_stmt, 2, max_students);
    }

    int student_count = 0;
    while (sqlite3_step(stu_stmt) == SQLITE_ROW && student_count < max_students) {
        AttendanceMonthlyReport *mr = &out_reports[student_count];
        memset(mr, 0, sizeof(AttendanceMonthlyReport));

        mr->student_id = sqlite3_column_int(stu_stmt, 0);
        const char *adm = (const char *)sqlite3_column_text(stu_stmt, 1);
        const char *name = (const char *)sqlite3_column_text(stu_stmt, 2);
        mr->roll_number = sqlite3_column_int(stu_stmt, 3);
        const char *cname = (const char *)sqlite3_column_text(stu_stmt, 4);
        const char *sname = (const char *)sqlite3_column_text(stu_stmt, 5);

        if (adm) strncpy(mr->admission_no, adm, sizeof(mr->admission_no) - 1);
        if (name) strncpy(mr->student_name, name, sizeof(mr->student_name) - 1);
        if (cname) strncpy(mr->class_name, cname, sizeof(mr->class_name) - 1);
        if (sname) strncpy(mr->section_name, sname, sizeof(mr->section_name) - 1);

        mr->days_in_month = days_in_month;
        for (int d = 1; d <= days_in_month; d++) {
            strcpy(mr->daily_status[d], "-");
        }

        student_count++;
    }
    sqlite3_finalize(stu_stmt);

    if (student_count == 0) {
        *out_count = 0;
        return true;
    }

    /* 2. Fetch all attendance for this month */
    char start_date[16], end_date[16];
    snprintf(start_date, sizeof(start_date), "%04d-%02d-01", year, month);
    snprintf(end_date, sizeof(end_date), "%04d-%02d-%02d", year, month, days_in_month);

    char att_sql[512];
    snprintf(att_sql, sizeof(att_sql),
        "SELECT a.student_id, CAST(strftime('%%d', a.date) AS INTEGER), a.status "
        "FROM attendance a "
        "WHERE a.class_id = ? AND a.date >= ? AND a.date <= ?;");

    sqlite3_stmt *att_stmt = NULL;
    if (sqlite3_prepare_v2(db, att_sql, -1, &att_stmt, NULL) != SQLITE_OK) return false;

    sqlite3_bind_int(att_stmt, 1, class_id);
    sqlite3_bind_text(att_stmt, 2, start_date, -1, SQLITE_STATIC);
    sqlite3_bind_text(att_stmt, 3, end_date, -1, SQLITE_STATIC);

    while (sqlite3_step(att_stmt) == SQLITE_ROW) {
        int sid = sqlite3_column_int(att_stmt, 0);
        int day = sqlite3_column_int(att_stmt, 1);
        const char *st = (const char *)sqlite3_column_text(att_stmt, 2);

        if (day < 1 || day > days_in_month || !st) continue;

        for (int i = 0; i < student_count; i++) {
            if (out_reports[i].student_id == sid) {
                if (strcmp(st, ATTENDANCE_STATUS_PRESENT) == 0) {
                    strcpy(out_reports[i].daily_status[day], "P");
                    out_reports[i].total_present++;
                } else if (strcmp(st, ATTENDANCE_STATUS_ABSENT) == 0) {
                    strcpy(out_reports[i].daily_status[day], "A");
                    out_reports[i].total_absent++;
                } else if (strcmp(st, ATTENDANCE_STATUS_LATE) == 0) {
                    strcpy(out_reports[i].daily_status[day], "L");
                    out_reports[i].total_late++;
                } else if (strcmp(st, ATTENDANCE_STATUS_HALF_DAY) == 0) {
                    strcpy(out_reports[i].daily_status[day], "H");
                    out_reports[i].total_half_day++;
                }
                break;
            }
        }
    }
    sqlite3_finalize(att_stmt);

    /* 3. Compute percentage for each student */
    for (int i = 0; i < student_count; i++) {
        int recorded_days = out_reports[i].total_present + out_reports[i].total_absent +
                            out_reports[i].total_late + out_reports[i].total_half_day;
        if (recorded_days > 0) {
            double effective = (double)out_reports[i].total_present +
                               (double)out_reports[i].total_late +
                               0.5 * (double)out_reports[i].total_half_day;
            out_reports[i].percentage = (effective / (double)recorded_days) * 100.0;
        } else {
            out_reports[i].percentage = 0.0;
        }
    }

    *out_count = student_count;
    return true;
}

bool attendance_service_delete(int id)
{
    if (id <= 0) return false;

    sqlite3 *db = db_get_handle();
    if (!db) return false;

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, "DELETE FROM attendance WHERE id = ?;", -1, &stmt, NULL) != SQLITE_OK) {
        return false;
    }
    sqlite3_bind_int(stmt, 1, id);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) return false;

    char details[64];
    snprintf(details, sizeof(details), "Deleted attendance record ID %d", id);
    db_log_activity(0, "DELETE_ATTENDANCE", "ATTENDANCE", details);
    return true;
}
