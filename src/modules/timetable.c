/**
 * @file timetable.c
 * @brief Timetable management, scheduling grids, and double-booking conflict detection.
 */

#include "timetable.h"
#include "database.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void get_default_period_timing(int period, char *out_start, size_t s_len, char *out_end, size_t e_len)
{
    switch (period) {
        case 1: snprintf(out_start, s_len, "08:30"); snprintf(out_end, e_len, "09:15"); break;
        case 2: snprintf(out_start, s_len, "09:15"); snprintf(out_end, e_len, "10:00"); break;
        case 3: snprintf(out_start, s_len, "10:00"); snprintf(out_end, e_len, "10:45"); break;
        case 4: snprintf(out_start, s_len, "11:00"); snprintf(out_end, e_len, "11:45"); break;
        case 5: snprintf(out_start, s_len, "11:45"); snprintf(out_end, e_len, "12:30"); break;
        case 6: snprintf(out_start, s_len, "13:00"); snprintf(out_end, e_len, "13:45"); break;
        case 7: snprintf(out_start, s_len, "13:45"); snprintf(out_end, e_len, "14:30"); break;
        case 8: snprintf(out_start, s_len, "14:30"); snprintf(out_end, e_len, "15:15"); break;
        default: snprintf(out_start, s_len, "08:30"); snprintf(out_end, e_len, "09:15"); break;
    }
}

/* ── Conflict Detection ───────────────────────────────────────────── */

bool timetable_service_check_conflict(const TimetableSlot *slot, ConflictResult *out_result)
{
    if (!slot || !out_result) return false;
    memset(out_result, 0, sizeof(ConflictResult));
    out_result->has_conflict = false;
    out_result->type = CONFLICT_NONE;

    sqlite3 *db = db_get_handle();
    if (!db) return false;

    /* 1. Teacher Double-Booking Check */
    if (slot->teacher_id > 0) {
        const char *tch_sql =
            "SELECT t.id, COALESCE(c.name, ''), COALESCE(sec.name, ''), COALESCE(tch.full_name, '') "
            "FROM timetables t "
            "JOIN classes c ON t.class_id = c.id "
            "LEFT JOIN sections sec ON t.section_id = sec.id "
            "JOIN teachers tch ON t.teacher_id = tch.id "
            "WHERE t.teacher_id = ? AND t.day_of_week = ? AND t.period_number = ? AND t.id != ?;";

        sqlite3_stmt *stmt = NULL;
        if (sqlite3_prepare_v2(db, tch_sql, -1, &stmt, NULL) == SQLITE_OK) {
            sqlite3_bind_int(stmt, 1, slot->teacher_id);
            sqlite3_bind_text(stmt, 2, slot->day_of_week, -1, SQLITE_STATIC);
            sqlite3_bind_int(stmt, 3, slot->period_number);
            sqlite3_bind_int(stmt, 4, slot->id);

            if (sqlite3_step(stmt) == SQLITE_ROW) {
                const char *c_name = (const char *)sqlite3_column_text(stmt, 1);
                const char *s_name = (const char *)sqlite3_column_text(stmt, 2);
                const char *t_name = (const char *)sqlite3_column_text(stmt, 3);

                out_result->has_conflict = true;
                out_result->type = CONFLICT_TEACHER;
                snprintf(out_result->message, sizeof(out_result->message),
                         "Teacher '%s' is already assigned to %s (%s) during Period %d on %s.",
                         t_name ? t_name : "Teacher",
                         c_name ? c_name : "Class",
                         s_name ? s_name : "Section",
                         slot->period_number, slot->day_of_week);

                sqlite3_finalize(stmt);
                return true;
            }
            sqlite3_finalize(stmt);
        }
    }

    /* 2. Room Double-Booking Check */
    if (strlen(slot->room_number) > 0) {
        const char *rm_sql =
            "SELECT t.id, COALESCE(c.name, ''), COALESCE(sec.name, '') "
            "FROM timetables t "
            "JOIN classes c ON t.class_id = c.id "
            "LEFT JOIN sections sec ON t.section_id = sec.id "
            "WHERE t.room_number = ? AND t.day_of_week = ? AND t.period_number = ? AND t.id != ?;";

        sqlite3_stmt *stmt = NULL;
        if (sqlite3_prepare_v2(db, rm_sql, -1, &stmt, NULL) == SQLITE_OK) {
            sqlite3_bind_text(stmt, 1, slot->room_number, -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 2, slot->day_of_week, -1, SQLITE_STATIC);
            sqlite3_bind_int(stmt, 3, slot->period_number);
            sqlite3_bind_int(stmt, 4, slot->id);

            if (sqlite3_step(stmt) == SQLITE_ROW) {
                const char *c_name = (const char *)sqlite3_column_text(stmt, 1);
                const char *s_name = (const char *)sqlite3_column_text(stmt, 2);

                out_result->has_conflict = true;
                out_result->type = CONFLICT_ROOM;
                snprintf(out_result->message, sizeof(out_result->message),
                         "Room '%s' is already occupied by %s (%s) during Period %d on %s.",
                         slot->room_number,
                         c_name ? c_name : "Class",
                         s_name ? s_name : "Section",
                         slot->period_number, slot->day_of_week);

                sqlite3_finalize(stmt);
                return true;
            }
            sqlite3_finalize(stmt);
        }
    }

    return true;
}

/* ── Save / Update Slot ───────────────────────────────────────────── */

bool timetable_service_save_slot(const TimetableSlot *slot, int *out_id, ConflictResult *out_conflict)
{
    if (!slot || slot->class_id <= 0 || slot->section_id <= 0 ||
        strlen(slot->day_of_week) == 0 || slot->period_number <= 0) {
        return false;
    }

    ConflictResult conf;
    if (!timetable_service_check_conflict(slot, &conf)) return false;

    if (conf.has_conflict) {
        if (out_conflict) *out_conflict = conf;
        return false;
    }

    sqlite3 *db = db_get_handle();
    if (!db) return false;

    char s_time[16], e_time[16];
    if (strlen(slot->start_time) > 0 && strlen(slot->end_time) > 0) {
        strncpy(s_time, slot->start_time, sizeof(s_time) - 1);
        strncpy(e_time, slot->end_time, sizeof(e_time) - 1);
    } else {
        get_default_period_timing(slot->period_number, s_time, sizeof(s_time), e_time, sizeof(e_time));
    }

    const char *sql =
        "INSERT INTO timetables (class_id, section_id, day_of_week, period_number, "
        "                        start_time, end_time, subject_id, teacher_id, room_number) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?) "
        "ON CONFLICT(class_id, section_id, day_of_week, period_number) DO UPDATE SET "
        "    start_time = excluded.start_time, "
        "    end_time = excluded.end_time, "
        "    subject_id = excluded.subject_id, "
        "    teacher_id = excluded.teacher_id, "
        "    room_number = excluded.room_number;";

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return false;

    sqlite3_bind_int(stmt, 1, slot->class_id);
    sqlite3_bind_int(stmt, 2, slot->section_id);
    sqlite3_bind_text(stmt, 3, slot->day_of_week, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 4, slot->period_number);
    sqlite3_bind_text(stmt, 5, s_time, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 6, e_time, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 7, slot->subject_id);
    sqlite3_bind_int(stmt, 8, slot->teacher_id);
    if (strlen(slot->room_number) > 0) sqlite3_bind_text(stmt, 9, slot->room_number, -1, SQLITE_STATIC);
    else sqlite3_bind_null(stmt, 9);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) return false;

    int new_id = (int)sqlite3_last_insert_rowid(db);
    if (out_id) *out_id = new_id;

    char details[256];
    snprintf(details, sizeof(details), "Saved timetable slot: Class ID %d, Sec ID %d, Day %s, Period %d",
             slot->class_id, slot->section_id, slot->day_of_week, slot->period_number);
    db_log_activity(0, "SAVE_TIMETABLE_SLOT", "TIMETABLE", details);
    return true;
}

/* ── Query Services ───────────────────────────────────────────────── */

static const char *TIMETABLE_SELECT =
    "SELECT t.id, t.class_id, COALESCE(c.name, ''), "
    "       t.section_id, COALESCE(sec.name, ''), "
    "       t.day_of_week, t.period_number, "
    "       t.start_time, t.end_time, "
    "       t.subject_id, COALESCE(sub.code, ''), COALESCE(sub.name, ''), "
    "       t.teacher_id, COALESCE(tch.full_name, ''), "
    "       COALESCE(t.room_number, ''), '' AS created_at "
    "FROM timetables t "
    "JOIN classes c ON t.class_id = c.id "
    "JOIN sections sec ON t.section_id = sec.id "
    "JOIN subjects sub ON t.subject_id = sub.id "
    "JOIN teachers tch ON t.teacher_id = tch.id ";

static void read_slot_row(sqlite3_stmt *stmt, TimetableSlot *s)
{
    memset(s, 0, sizeof(TimetableSlot));
    s->id = sqlite3_column_int(stmt, 0);
    s->class_id = sqlite3_column_int(stmt, 1);
    const char *c_name = (const char *)sqlite3_column_text(stmt, 2);
    s->section_id = sqlite3_column_int(stmt, 3);
    const char *sec_name = (const char *)sqlite3_column_text(stmt, 4);
    const char *day = (const char *)sqlite3_column_text(stmt, 5);
    s->period_number = sqlite3_column_int(stmt, 6);
    const char *s_time = (const char *)sqlite3_column_text(stmt, 7);
    const char *e_time = (const char *)sqlite3_column_text(stmt, 8);
    s->subject_id = sqlite3_column_int(stmt, 9);
    const char *sub_code = (const char *)sqlite3_column_text(stmt, 10);
    const char *sub_name = (const char *)sqlite3_column_text(stmt, 11);
    s->teacher_id = sqlite3_column_int(stmt, 12);
    const char *tch_name = (const char *)sqlite3_column_text(stmt, 13);
    const char *room = (const char *)sqlite3_column_text(stmt, 14);
    const char *cat = (const char *)sqlite3_column_text(stmt, 15);

    if (c_name) strncpy(s->class_name, c_name, sizeof(s->class_name) - 1);
    if (sec_name) strncpy(s->section_name, sec_name, sizeof(s->section_name) - 1);
    if (day) strncpy(s->day_of_week, day, sizeof(s->day_of_week) - 1);
    if (s_time) strncpy(s->start_time, s_time, sizeof(s->start_time) - 1);
    if (e_time) strncpy(s->end_time, e_time, sizeof(s->end_time) - 1);
    if (sub_code) strncpy(s->subject_code, sub_code, sizeof(s->subject_code) - 1);
    if (sub_name) strncpy(s->subject_name, sub_name, sizeof(s->subject_name) - 1);
    if (tch_name) strncpy(s->teacher_name, tch_name, sizeof(s->teacher_name) - 1);
    if (room) strncpy(s->room_number, room, sizeof(s->room_number) - 1);
    if (cat) strncpy(s->created_at, cat, sizeof(s->created_at) - 1);
}

bool timetable_service_get_by_class(int class_id, int section_id, const char *day_of_week,
                                    TimetableSlot *out_list, int max_items, int *out_count)
{
    if (!out_list || max_items <= 0 || !out_count || class_id <= 0) return false;
    *out_count = 0;

    sqlite3 *db = db_get_handle();
    if (!db) return false;

    char where[256];
    if (section_id > 0 && day_of_week && strlen(day_of_week) > 0) {
        snprintf(where, sizeof(where), "WHERE t.class_id = %d AND t.section_id = %d AND t.day_of_week = '%s'",
                 class_id, section_id, day_of_week);
    } else if (section_id > 0) {
        snprintf(where, sizeof(where), "WHERE t.class_id = %d AND t.section_id = %d", class_id, section_id);
    } else if (day_of_week && strlen(day_of_week) > 0) {
        snprintf(where, sizeof(where), "WHERE t.class_id = %d AND t.day_of_week = '%s'", class_id, day_of_week);
    } else {
        snprintf(where, sizeof(where), "WHERE t.class_id = %d", class_id);
    }

    char sql[1024];
    snprintf(sql, sizeof(sql), "%s %s ORDER BY t.day_of_week ASC, t.period_number ASC LIMIT ?;",
             TIMETABLE_SELECT, where);

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return false;

    sqlite3_bind_int(stmt, 1, max_items);

    int count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW && count < max_items) {
        read_slot_row(stmt, &out_list[count]);
        count++;
    }
    sqlite3_finalize(stmt);
    *out_count = count;
    return true;
}

bool timetable_service_get_by_teacher(int teacher_id, const char *day_of_week,
                                      TimetableSlot *out_list, int max_items, int *out_count)
{
    if (!out_list || max_items <= 0 || !out_count || teacher_id <= 0) return false;
    *out_count = 0;

    sqlite3 *db = db_get_handle();
    if (!db) return false;

    char where[256];
    if (day_of_week && strlen(day_of_week) > 0) {
        snprintf(where, sizeof(where), "WHERE t.teacher_id = %d AND t.day_of_week = '%s'",
                 teacher_id, day_of_week);
    } else {
        snprintf(where, sizeof(where), "WHERE t.teacher_id = %d", teacher_id);
    }

    char sql[1024];
    snprintf(sql, sizeof(sql), "%s %s ORDER BY t.day_of_week ASC, t.period_number ASC LIMIT ?;",
             TIMETABLE_SELECT, where);

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return false;

    sqlite3_bind_int(stmt, 1, max_items);

    int count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW && count < max_items) {
        read_slot_row(stmt, &out_list[count]);
        count++;
    }
    sqlite3_finalize(stmt);
    *out_count = count;
    return true;
}

bool timetable_service_get_by_id(int id, TimetableSlot *out_slot)
{
    if (!out_slot || id <= 0) return false;

    sqlite3 *db = db_get_handle();
    if (!db) return false;

    char sql[1024];
    snprintf(sql, sizeof(sql), "%s WHERE t.id = ?;", TIMETABLE_SELECT);

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return false;

    sqlite3_bind_int(stmt, 1, id);
    bool found = false;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        read_slot_row(stmt, out_slot);
        found = true;
    }
    sqlite3_finalize(stmt);
    return found;
}

bool timetable_service_delete(int id)
{
    if (id <= 0) return false;

    sqlite3 *db = db_get_handle();
    if (!db) return false;

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, "DELETE FROM timetables WHERE id = ?;", -1, &stmt, NULL) != SQLITE_OK) {
        return false;
    }
    sqlite3_bind_int(stmt, 1, id);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) return false;

    char details[64];
    snprintf(details, sizeof(details), "Deleted timetable slot ID %d", id);
    db_log_activity(0, "DELETE_TIMETABLE_SLOT", "TIMETABLE", details);
    return true;
}
