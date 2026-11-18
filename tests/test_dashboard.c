/**
 * @file test_dashboard.c
 * @brief Unit tests for dashboard service statistics, live aggregations, and trends.
 */

#include "dashboard.h"
#include "database.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define TEST_DB "test_schoolcms_dashboard.db"

static void cleanup_files(void)
{
    remove(TEST_DB);
    remove(TEST_DB "-wal");
    remove(TEST_DB "-shm");
}

static void seed_dashboard_test_data(void)
{
    sqlite3 *db = db_get_handle();
    assert(db != NULL);

    char *errmsg = NULL;
    const char *sql =
        "INSERT INTO classes (id, name, numeric_value, capacity) VALUES (1, 'Class 10-A', 10, 40);\n"
        "INSERT INTO sections (id, class_id, name) VALUES (1, 1, 'A');\n"
        "INSERT INTO teachers (teacher_code, full_name, phone, joining_date, salary, status) "
        "VALUES ('T-001', 'Prof. Sharma', '9876543210', '2026-01-01', 50000.0, 'ACTIVE');\n"
        "INSERT INTO staff (staff_code, full_name, role_title, phone, joining_date, salary, status) "
        "VALUES ('S-001', 'Rajesh Clerk', 'Head Clerk', '9876543211', '2026-01-01', 30000.0, 'ACTIVE');\n"
        "INSERT INTO students (admission_no, full_name, gender, admission_date, class_id, section_id, status) "
        "VALUES ('ADM-101', 'Aarav Sharma', 'MALE', date('now'), 1, 1, 'ACTIVE');\n"
        "INSERT INTO students (admission_no, full_name, gender, admission_date, class_id, section_id, status) "
        "VALUES ('ADM-102', 'Priya Patel', 'FEMALE', date('now'), 1, 1, 'ACTIVE');\n"
        "INSERT INTO attendance (student_id, class_id, section_id, date, status) "
        "VALUES (1, 1, 1, date('now'), 'PRESENT');\n"
        "INSERT INTO attendance (student_id, class_id, section_id, date, status) "
        "VALUES (2, 1, 1, date('now'), 'ABSENT');\n"
        "INSERT INTO fee_structures (id, name, class_id, academic_year_id, fee_type, amount) "
        "VALUES (1, 'Term 1 Tuition', 1, 1, 'TUITION', 15000.0);\n"
        "INSERT INTO fee_payments (receipt_no, student_id, fee_structure_id, amount_paid, total_amount, payment_method, payment_date) "
        "VALUES ('REC-001', 1, 1, 15000.0, 15000.0, 'CASH', date('now'));\n";

    int rc = sqlite3_exec(db, sql, NULL, NULL, &errmsg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Seed error: %s\n", errmsg ? errmsg : "unknown");
        sqlite3_free(errmsg);
    }
    assert(rc == SQLITE_OK);
}

static void test_live_statistics(void)
{
    printf("[TEST] Testing live dashboard statistics calculation...\n");

    DashboardStats stats;
    assert(dashboard_service_get_stats(&stats));

    assert(stats.total_students == 2);
    assert(stats.total_teachers == 1);
    assert(stats.total_staff == 1);
    assert(stats.total_classes == 1);
    printf("       Total entities counts (students=2, teachers=1, staff=1, classes=1) -> PASS\n");

    assert(stats.today_attendance_present == 1);
    assert(stats.today_attendance_total == 2);
    assert(stats.today_attendance_pct >= 49.9 && stats.today_attendance_pct <= 50.1);
    printf("       Today's attendance metrics (50%%, 1/2) -> PASS\n");

    assert(stats.fee_collection_this_month >= 14999.0);
    printf("       Fee collection this month (₹%.2f) -> PASS\n", stats.fee_collection_this_month);

    assert(stats.new_admissions_count == 2);
    printf("       New admissions this month (%d) -> PASS\n", stats.new_admissions_count);
}

static void test_recent_lists(void)
{
    printf("[TEST] Testing recent admissions and activity logs...\n");

    RecentAdmission adm[5];
    int adm_count = 0;
    assert(dashboard_service_get_recent_admissions(adm, 5, &adm_count));
    assert(adm_count == 2);
    assert(strcmp(adm[0].admission_no, "ADM-102") == 0); /* DESC order */
    assert(strcmp(adm[1].admission_no, "ADM-101") == 0);
    printf("       Recent admissions retrieved in correct order -> PASS\n");

    RecentActivity act[5];
    int act_count = 0;
    assert(dashboard_service_get_recent_activities(act, 5, &act_count));
    assert(act_count > 0);
    printf("       Recent activities retrieved -> PASS\n");
}

int main(void)
{
    printf("=========================================\n");
    printf("  SchoolCMS Dashboard Module Unit Tests  \n");
    printf("=========================================\n\n");

    cleanup_files();
    assert(db_init(TEST_DB) == DB_SUCCESS);

    seed_dashboard_test_data();
    test_live_statistics();
    test_recent_lists();

    db_close();
    cleanup_files();

    printf("\nAll dashboard unit tests passed successfully!\n");
    return 0;
}
