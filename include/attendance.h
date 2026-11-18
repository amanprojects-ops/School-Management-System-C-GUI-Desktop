/**
 * @file attendance.h
 * @brief Attendance tracking data models, daily marking, bulk entry, and statistics services.
 */

#ifndef SCHOOLCMS_ATTENDANCE_H
#define SCHOOLCMS_ATTENDANCE_H

#include <stdbool.h>
#include <stddef.h>

#define ATTENDANCE_MAX_RECORDS 200
#define ATTENDANCE_MAX_MONTH_DAYS 31

/**
 * @brief Attendance Status string constants
 */
#define ATTENDANCE_STATUS_PRESENT  "PRESENT"
#define ATTENDANCE_STATUS_ABSENT   "ABSENT"
#define ATTENDANCE_STATUS_LATE     "LATE"
#define ATTENDANCE_STATUS_HALF_DAY "HALF_DAY"

/**
 * @brief Individual Student Attendance Record
 */
typedef struct {
    int id;                      /* attendance record ID (0 if not yet marked) */
    int student_id;
    char admission_no[32];
    char student_name[128];
    int roll_number;
    int class_id;
    char class_name[64];
    int section_id;
    char section_name[32];
    char date[16];               /* YYYY-MM-DD */
    char status[16];             /* PRESENT, ABSENT, LATE, HALF_DAY */
    char remarks[128];
    int recorded_by;
    char created_at[32];
} AttendanceRecord;

/**
 * @brief Bulk Marking Item Entry
 */
typedef struct {
    int student_id;
    char status[16];
    char remarks[128];
} AttendanceEntry;

/**
 * @brief Aggregated Attendance Statistics
 */
typedef struct {
    int total_days;
    int present_count;
    int absent_count;
    int late_count;
    int half_day_count;
    double percentage;          /* e.g. 92.5 */
} AttendanceStats;

/**
 * @brief Monthly Register Row for a Student
 */
typedef struct {
    int student_id;
    char admission_no[32];
    char student_name[128];
    int roll_number;
    char class_name[64];
    char section_name[32];
    int days_in_month;
    char daily_status[ATTENDANCE_MAX_MONTH_DAYS + 1][8]; /* day 1..31: "P", "A", "L", "H", or "-" */
    int total_present;
    int total_absent;
    int total_late;
    int total_half_day;
    double percentage;
} AttendanceMonthlyReport;

/* ── Service Functions ────────────────────────────────────────────── */

/**
 * @brief Mark or update single student attendance for a date.
 * If attendance already exists for (student_id, date), updates status and remarks.
 */
bool attendance_service_mark(const AttendanceRecord *rec, int *out_id);

/**
 * @brief Bulk mark attendance for multiple students of a class/section in a single transaction.
 */
bool attendance_service_mark_bulk(int class_id, int section_id, const char *date,
                                  const AttendanceEntry *entries, int count,
                                  int recorded_by);

/**
 * @brief Retrieve attendance sheet for a class and section on a specific date.
 * Lists all active students in the class/section with their attendance status (or UNMARKED if absent).
 */
bool attendance_service_get_roster(int class_id, int section_id, const char *date,
                                   AttendanceRecord *out_list, int max_items, int *out_count);

/**
 * @brief Calculate attendance statistics for a student over a date range.
 * If start_date or end_date is NULL, covers all recorded dates.
 */
bool attendance_service_get_student_stats(int student_id,
                                          const char *start_date,
                                          const char *end_date,
                                          AttendanceStats *out_stats);

/**
 * @brief Calculate class-wide attendance summary metrics for a given date.
 */
bool attendance_service_get_class_stats(int class_id, int section_id, const char *date,
                                        AttendanceStats *out_stats);

/**
 * @brief Generate a full monthly attendance register for a class and section.
 */
bool attendance_service_get_monthly_report(int class_id, int section_id,
                                           int year, int month,
                                           AttendanceMonthlyReport *out_reports,
                                           int max_students, int *out_count);

/**
 * @brief Delete an attendance record by ID.
 */
bool attendance_service_delete(int id);

#endif /* SCHOOLCMS_ATTENDANCE_H */
