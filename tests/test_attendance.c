/**
 * @file test_attendance.c
 * @brief Comprehensive unit tests for attendance tracking, bulk marking, and registers.
 */

#include "attendance.h"
#include "database.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST_DB "test_schoolcms_attendance.db"

static void cleanup_files(void)
{
    remove(TEST_DB);
    remove(TEST_DB "-wal");
    remove(TEST_DB "-shm");
}

static void setup_test_data(void)
{
    sqlite3 *db = db_get_handle();
    assert(db != NULL);

    /* Insert test class and section */
    sqlite3_exec(db, "INSERT INTO classes (id, name, numeric_value, capacity, academic_year_id) "
                     "VALUES (1, 'Class 10-A', 10, 40, 1);", NULL, NULL, NULL);
    sqlite3_exec(db, "INSERT INTO sections (id, class_id, name) VALUES (1, 1, 'A');", NULL, NULL, NULL);

    /* Insert 3 test students */
    sqlite3_exec(db, "INSERT INTO students (id, admission_no, full_name, dob, gender, admission_date, class_id, section_id, roll_number, status) "
                     "VALUES (1, 'ADM-001', 'Alice Johnson', '2010-01-01', 'FEMALE', '2026-04-01', 1, 1, 1, 'ACTIVE'),"
                     "       (2, 'ADM-002', 'Bob Smith', '2010-02-02', 'MALE', '2026-04-01', 1, 1, 2, 'ACTIVE'),"
                     "       (3, 'ADM-003', 'Charlie Brown', '2010-03-03', 'MALE', '2026-04-01', 1, 1, 3, 'ACTIVE');",
                 NULL, NULL, NULL);
}

static void test_single_mark_and_upsert(void)
{
    printf("[TEST] Testing single student attendance mark and upsert...\n");

    AttendanceRecord rec = {
        .student_id = 1,
        .class_id = 1,
        .section_id = 1,
        .date = "2026-11-01",
        .status = ATTENDANCE_STATUS_PRESENT,
        .remarks = "On time",
        .recorded_by = 1
    };

    int att_id = 0;
    bool ok = attendance_service_mark(&rec, &att_id);
    assert(ok && att_id > 0 && "Must mark attendance");

    /* Verify via roster */
    AttendanceRecord roster[10];
    int count = 0;
    ok = attendance_service_get_roster(1, 1, "2026-11-01", roster, 10, &count);
    assert(ok && count == 3 && "Roster must return all 3 students");

    assert(roster[0].student_id == 1);
    assert(strcmp(roster[0].status, ATTENDANCE_STATUS_PRESENT) == 0);
    assert(strcmp(roster[0].remarks, "On time") == 0);

    /* Upsert: Update to LATE on same date */
    rec.status[0] = '\0';
    strcpy(rec.status, ATTENDANCE_STATUS_LATE);
    strcpy(rec.remarks, "Late by 15 mins");

    int updated_id = 0;
    ok = attendance_service_mark(&rec, &updated_id);
    assert(ok && "Must upsert attendance without primary key error");

    /* Verify updated status */
    ok = attendance_service_get_roster(1, 1, "2026-11-01", roster, 10, &count);
    assert(ok && count == 3);
    assert(strcmp(roster[0].status, ATTENDANCE_STATUS_LATE) == 0);
    assert(strcmp(roster[0].remarks, "Late by 15 mins") == 0);

    printf("       Single mark and upsert verified -> PASS\n");
}

static void test_bulk_marking(void)
{
    printf("[TEST] Testing bulk attendance marking in transaction...\n");

    AttendanceEntry entries[3] = {
        { .student_id = 1, .status = ATTENDANCE_STATUS_PRESENT, .remarks = "" },
        { .student_id = 2, .status = ATTENDANCE_STATUS_ABSENT,  .remarks = "Sick leave" },
        { .student_id = 3, .status = ATTENDANCE_STATUS_HALF_DAY, .remarks = "Left at noon" }
    };

    bool ok = attendance_service_mark_bulk(1, 1, "2026-11-02", entries, 3, 1);
    assert(ok && "Bulk marking transaction must succeed");

    AttendanceRecord roster[10];
    int count = 0;
    ok = attendance_service_get_roster(1, 1, "2026-11-02", roster, 10, &count);
    assert(ok && count == 3);

    assert(roster[0].student_id == 1 && strcmp(roster[0].status, ATTENDANCE_STATUS_PRESENT) == 0);
    assert(roster[1].student_id == 2 && strcmp(roster[1].status, ATTENDANCE_STATUS_ABSENT) == 0);
    assert(roster[2].student_id == 3 && strcmp(roster[2].status, ATTENDANCE_STATUS_HALF_DAY) == 0);

    printf("       Bulk marking transaction verified -> PASS\n");
}

static void test_attendance_statistics(void)
{
    printf("[TEST] Testing attendance statistics and percentage calculations...\n");

    /* Add more days for student 1:
       2026-11-01: LATE (from test 1)
       2026-11-02: PRESENT (from test 2)
       2026-11-03: PRESENT
       2026-11-04: ABSENT
    */
    AttendanceRecord r3 = { .student_id = 1, .class_id = 1, .section_id = 1, .date = "2026-11-03", .status = ATTENDANCE_STATUS_PRESENT, .recorded_by = 1 };
    AttendanceRecord r4 = { .student_id = 1, .class_id = 1, .section_id = 1, .date = "2026-11-04", .status = ATTENDANCE_STATUS_ABSENT, .recorded_by = 1 };

    assert(attendance_service_mark(&r3, NULL));
    assert(attendance_service_mark(&r4, NULL));

    AttendanceStats s_stats;
    bool ok = attendance_service_get_student_stats(1, NULL, NULL, &s_stats);
    assert(ok);
    assert(s_stats.total_days == 4);
    assert(s_stats.present_count == 2);
    assert(s_stats.absent_count == 1);
    assert(s_stats.late_count == 1);
    assert(s_stats.half_day_count == 0);
    /* effective present = 2 + 1 = 3 / 4 = 75.0% */
    assert(s_stats.percentage == 75.0);

    /* Test class stats on 2026-11-02 */
    AttendanceStats c_stats;
    ok = attendance_service_get_class_stats(1, 1, "2026-11-02", &c_stats);
    assert(ok);
    assert(c_stats.total_days == 3);
    assert(c_stats.present_count == 1);
    assert(c_stats.absent_count == 1);
    assert(c_stats.half_day_count == 1);

    printf("       Attendance statistics calculation verified -> PASS\n");
}

static void test_monthly_register(void)
{
    printf("[TEST] Testing monthly register generation...\n");

    AttendanceMonthlyReport reports[10];
    int count = 0;
    bool ok = attendance_service_get_monthly_report(1, 1, 2026, 11, reports, 10, &count);
    assert(ok && count == 3 && "Must generate register for all 3 students");
    assert(reports[0].days_in_month == 30 && "November has 30 days");

    /* Student 1:
       day 1: L
       day 2: P
       day 3: P
       day 4: A
       day 5..30: -
    */
    assert(strcmp(reports[0].daily_status[1], "L") == 0);
    assert(strcmp(reports[0].daily_status[2], "P") == 0);
    assert(strcmp(reports[0].daily_status[3], "P") == 0);
    assert(strcmp(reports[0].daily_status[4], "A") == 0);
    assert(strcmp(reports[0].daily_status[5], "-") == 0);

    assert(reports[0].total_present == 2);
    assert(reports[0].total_absent == 1);
    assert(reports[0].total_late == 1);
    assert(reports[0].percentage == 75.0);

    printf("       Monthly register grid verified -> PASS\n");
}

int main(void)
{
    printf("=========================================\n");
    printf("  SchoolCMS Attendance Module Unit Tests \n");
    printf("=========================================\n\n");

    cleanup_files();
    assert(db_init(TEST_DB) == DB_SUCCESS && "Database init failed");

    setup_test_data();
    test_single_mark_and_upsert();
    test_bulk_marking();
    test_attendance_statistics();
    test_monthly_register();

    db_close();
    cleanup_files();

    printf("\nAll attendance unit tests passed successfully! 100%% OK.\n");
    return 0;
}
