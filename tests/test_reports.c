/**
 * @file test_reports.c
 * @brief Unit tests for reports generation, aggregations, and RFC 4180 CSV export.
 */

#include "reports.h"
#include "database.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST_DB "test_schoolcms_reports.db"
#define TEST_CSV "test_export_output.csv"

static void cleanup_files(void)
{
    remove(TEST_DB);
    remove(TEST_DB "-wal");
    remove(TEST_DB "-shm");
    remove(TEST_CSV);
}

static void setup_test_data(void)
{
    sqlite3 *db = db_get_handle();
    assert(db != NULL);

    /* Classes & Sections */
    assert(sqlite3_exec(db, "INSERT INTO classes (id, name, numeric_value, capacity, academic_year_id) "
                            "VALUES (1, 'Grade 10', 10, 40, 1);", NULL, NULL, NULL) == SQLITE_OK);
    assert(sqlite3_exec(db, "INSERT INTO sections (id, class_id, name) "
                            "VALUES (1, 1, 'Section A');", NULL, NULL, NULL) == SQLITE_OK);

    /* Teachers */
    assert(sqlite3_exec(db, "INSERT INTO teachers (id, teacher_code, full_name, email, phone, qualification, subject, joining_date, salary, status) "
                            "VALUES (1, 'TCH-001', 'Dr. Alan Turing', 'alan@school.edu', '555-0101', 'Ph.D. CS', 'Computer Science', '2020-01-01', 65000.0, 'ACTIVE');",
                        NULL, NULL, NULL) == SQLITE_OK);

    /* Subjects */
    assert(sqlite3_exec(db, "INSERT INTO subjects (id, code, name, class_id, teacher_id) "
                            "VALUES (1, 'CS101', 'Computer Science', 1, 1);", NULL, NULL, NULL) == SQLITE_OK);

    /* Students */
    assert(sqlite3_exec(db, "INSERT INTO students (id, admission_no, full_name, gender, admission_date, class_id, section_id, roll_number, father_name, phone, status) "
                            "VALUES (1, 'ADM-001', 'Alice Johnson', 'FEMALE', '2026-04-01', 1, 1, 1, 'Robert Johnson', '555-1111', 'ACTIVE'),"
                            "       (2, 'ADM-002', 'Bob Smith', 'MALE', '2026-04-01', 1, 1, 2, 'David Smith', '555-2222', 'ACTIVE');",
                        NULL, NULL, NULL) == SQLITE_OK);

    /* Attendance */
    assert(sqlite3_exec(db, "INSERT INTO attendance (student_id, class_id, section_id, date, status) "
                            "VALUES (1, 1, 1, '2026-10-01', 'PRESENT'),"
                            "       (2, 1, 1, '2026-10-01', 'ABSENT');", NULL, NULL, NULL) == SQLITE_OK);

    /* Fee Structures & Payments */
    assert(sqlite3_exec(db, "INSERT INTO fee_structures (id, name, class_id, fee_type, amount, due_date, academic_year_id) "
                            "VALUES (1, 'Tuition Fee', 1, 'MONTHLY', 5000.0, '2026-10-10', 1);", NULL, NULL, NULL) == SQLITE_OK);
    assert(sqlite3_exec(db, "INSERT INTO fee_payments (id, receipt_no, student_id, fee_structure_id, amount_paid, total_amount, payment_date, payment_method, remarks) "
                            "VALUES (1, 'RCP-2026-0001', 1, 1, 5000.0, 5000.0, '2026-10-05', 'CASH', 'Full tuition payment');",
                        NULL, NULL, NULL) == SQLITE_OK);

    /* Examinations, Subjects & Results */
    assert(sqlite3_exec(db, "INSERT INTO examinations (id, name, academic_year_id, start_date, end_date, grading_system, status) "
                            "VALUES (1, 'Midterm Exam 2026', 1, '2026-10-10', '2026-10-20', 'PERCENTAGE', 'COMPLETED');",
                        NULL, NULL, NULL) == SQLITE_OK);
    assert(sqlite3_exec(db, "INSERT INTO exam_subjects (id, exam_id, subject_id, exam_date, max_marks, passing_marks) "
                            "VALUES (1, 1, 1, '2026-10-12', 100.0, 40.0);", NULL, NULL, NULL) == SQLITE_OK);
    assert(sqlite3_exec(db, "INSERT INTO exam_results (id, exam_subject_id, student_id, marks_obtained, grade, remarks) "
                            "VALUES (1, 1, 1, 92.5, 'A+', 'Excellent performance');", NULL, NULL, NULL) == SQLITE_OK);
}

static void test_csv_field_escaping(void)
{
    printf("[TEST] Testing CSV field escaping (RFC 4180)...\n");

    char out[128];

    /* Standard string without delimiters */
    report_escape_csv_field("Standard Text", out, sizeof(out));
    assert(strcmp(out, "Standard Text") == 0);

    /* String with comma */
    report_escape_csv_field("Johnson, Alice", out, sizeof(out));
    assert(strcmp(out, "\"Johnson, Alice\"") == 0);

    /* String with quotes */
    report_escape_csv_field("He said \"Hello\"", out, sizeof(out));
    assert(strcmp(out, "\"He said \"\"Hello\"\"\"") == 0);

    /* String with newline */
    report_escape_csv_field("Line 1\nLine 2", out, sizeof(out));
    assert(strcmp(out, "\"Line 1\nLine 2\"") == 0);

    printf("       CSV escaping verified -> PASS\n");
}

static void test_report_generation(void)
{
    printf("[TEST] Testing in-memory report generators across all categories...\n");

    ReportFilter f;
    memset(&f, 0, sizeof(f));
    ReportData data;

    /* 1. Student List */
    f.type = REPORT_STUDENT_LIST;
    f.class_id = 1;
    bool ok = report_service_generate(&f, &data);
    assert(ok && data.row_count == 2);
    assert(data.column_count == 10);
    assert(strcmp(data.rows[0][0], "ADM-001") == 0);
    assert(strcmp(data.rows[0][1], "Alice Johnson") == 0);

    /* 2. Attendance Summary */
    memset(&f, 0, sizeof(f));
    f.type = REPORT_ATTENDANCE_SUMMARY;
    f.class_id = 1;
    ok = report_service_generate(&f, &data);
    assert(ok && data.row_count == 2);
    assert(data.column_count == 9);
    assert(strcmp(data.rows[0][5], "1") == 0); /* Present */
    assert(strcmp(data.rows[1][6], "1") == 0); /* Absent */

    /* 3. Fee Collection */
    memset(&f, 0, sizeof(f));
    f.type = REPORT_FEE_COLLECTION;
    ok = report_service_generate(&f, &data);
    assert(ok && data.row_count == 1);
    assert(data.column_count == 8);
    assert(strcmp(data.rows[0][0], "RCP-2026-0001") == 0);
    assert(data.total_amount == 5000.0);

    /* 4. Pending Dues */
    memset(&f, 0, sizeof(f));
    f.type = REPORT_PENDING_DUES;
    f.class_id = 1;
    ok = report_service_generate(&f, &data);
    assert(ok && data.row_count == 2);
    assert(data.column_count == 8);
    /* Student 1 paid 5000 -> CLEARED (due 0.00) */
    assert(strcmp(data.rows[0][7], "CLEARED") == 0);
    /* Student 2 paid 0 -> PENDING (due 5000.00) */
    assert(strcmp(data.rows[1][7], "PENDING") == 0);
    assert(data.total_amount == 5000.0);

    /* 5. Teachers */
    memset(&f, 0, sizeof(f));
    f.type = REPORT_TEACHER_LIST;
    ok = report_service_generate(&f, &data);
    assert(ok && data.row_count == 1);
    assert(data.column_count == 9);
    assert(strcmp(data.rows[0][0], "TCH-001") == 0);
    assert(strcmp(data.rows[0][1], "Dr. Alan Turing") == 0);

    /* 6. Exam Results */
    memset(&f, 0, sizeof(f));
    f.type = REPORT_EXAM_RESULTS;
    f.exam_id = 1;
    ok = report_service_generate(&f, &data);
    assert(ok && data.row_count == 1);
    assert(data.column_count == 10);
    assert(strcmp(data.rows[0][1], "ADM-001") == 0);
    assert(strcmp(data.rows[0][8], "A+") == 0);
    assert(strcmp(data.rows[0][9], "PASS") == 0);

    printf("       All 6 report generators verified -> PASS\n");
}

static void test_csv_file_export(void)
{
    printf("[TEST] Testing CSV file creation with UTF-8 BOM...\n");

    ReportFilter f;
    memset(&f, 0, sizeof(f));
    f.type = REPORT_STUDENT_LIST;
    f.class_id = 1;

    ReportData data;
    bool ok = report_service_generate(&f, &data);
    assert(ok);

    ok = report_service_export_csv(&data, TEST_CSV);
    assert(ok && "Exporting CSV should succeed");

    /* Verify file exists and has UTF-8 BOM */
    FILE *fp = fopen(TEST_CSV, "rb");
    assert(fp != NULL);

    unsigned char bom[3] = {0};
    size_t n = fread(bom, 1, 3, fp);
    assert(n == 3);
    assert(bom[0] == 0xEF && bom[1] == 0xBB && bom[2] == 0xBF && "UTF-8 BOM must be present");

    char buffer[1024];
    assert(fgets(buffer, sizeof(buffer), fp) != NULL);
    /* Verify header contains Admission No */
    assert(strstr(buffer, "Admission No") != NULL);

    fclose(fp);

    /* Test convenience function */
    int rows_written = 0;
    f.type = REPORT_FEE_COLLECTION;
    ok = report_service_generate_and_export_csv(&f, TEST_CSV, &rows_written);
    assert(ok && rows_written == 1);

    printf("       CSV file export with UTF-8 BOM verified -> PASS\n");
}

int main(void)
{
    printf("====================================================\n");
    printf("   SchoolCMS Reports & Export Unit Tests            \n");
    printf("====================================================\n");

    cleanup_files();

    int rc = db_init(TEST_DB);
    assert(rc == DB_SUCCESS && "Database init should succeed");

    setup_test_data();

    test_csv_field_escaping();
    test_report_generation();
    test_csv_file_export();

    db_close();
    cleanup_files();

    printf("\n>>> ALL REPORTS TESTS PASSED (100%%) <<<\n");
    return 0;
}
