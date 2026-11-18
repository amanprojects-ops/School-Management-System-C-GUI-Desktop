/**
 * @file test_exams.c
 * @brief Comprehensive unit tests for examinations, scheduling, grading, report cards, and ranks.
 */

#include "exams.h"
#include "database.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST_DB "test_schoolcms_exams.db"

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
    assert(sqlite3_exec(db, "INSERT INTO classes (id, name, numeric_value, capacity, academic_year_id) "
                            "VALUES (1, 'Class 10-A', 10, 40, 1);", NULL, NULL, NULL) == SQLITE_OK);
    assert(sqlite3_exec(db, "INSERT INTO sections (id, class_id, name) "
                            "VALUES (1, 1, 'A');", NULL, NULL, NULL) == SQLITE_OK);

    /* Insert test subjects */
    assert(sqlite3_exec(db, "INSERT INTO subjects (id, code, name, class_id) "
                            "VALUES (1, 'MAT-101', 'Mathematics', 1),"
                            "       (2, 'SCI-101', 'General Science', 1);", NULL, NULL, NULL) == SQLITE_OK);

    /* Insert test students */
    assert(sqlite3_exec(db, "INSERT INTO students (id, admission_no, full_name, dob, gender, admission_date, class_id, section_id, roll_number, status) "
                            "VALUES (1, 'ADM-001', 'Alice Johnson', '2010-01-01', 'FEMALE', '2026-04-01', 1, 1, 1, 'ACTIVE'),"
                            "       (2, 'ADM-002', 'Bob Smith', '2010-02-02', 'MALE', '2026-04-01', 1, 1, 2, 'ACTIVE');",
                        NULL, NULL, NULL) == SQLITE_OK);
}

static void test_grade_calculation(void)
{
    printf("[TEST] Testing grade calculation...\n");

    assert(strcmp(exam_service_calculate_grade(95.0), "A+") == 0);
    assert(strcmp(exam_service_calculate_grade(85.0), "A") == 0);
    assert(strcmp(exam_service_calculate_grade(75.0), "B+") == 0);
    assert(strcmp(exam_service_calculate_grade(65.0), "B") == 0);
    assert(strcmp(exam_service_calculate_grade(55.0), "C") == 0);
    assert(strcmp(exam_service_calculate_grade(40.0), "D") == 0);
    assert(strcmp(exam_service_calculate_grade(25.0), "F") == 0);

    printf("       Grade scale boundaries verified -> PASS\n");
}

static void test_exam_crud(void)
{
    printf("[TEST] Testing examination CRUD...\n");

    Examination ex = {
        .id = 0,
        .name = "Annual Final Examination 2026",
        .academic_year_id = 1,
        .start_date = "2026-10-10",
        .end_date = "2026-10-25",
        .grading_system = "PERCENTAGE",
        .status = "PLANNED"
    };

    int ex_id = 0;
    bool ok = exam_service_create(&ex, &ex_id);
    assert(ok && ex_id > 0 && "Must create examination");

    Examination fetched;
    ok = exam_service_get_by_id(ex_id, &fetched);
    assert(ok && fetched.id == ex_id);
    assert(strcmp(fetched.name, "Annual Final Examination 2026") == 0);

    /* Update */
    strncpy(fetched.status, "ONGOING", sizeof(fetched.status) - 1);
    ok = exam_service_update(&fetched);
    assert(ok && "Must update examination");

    Examination updated;
    exam_service_get_by_id(ex_id, &updated);
    assert(strcmp(updated.status, "ONGOING") == 0);

    Examination list[MAX_EXAMINATIONS];
    int count = 0;
    ok = exam_service_get_all(list, MAX_EXAMINATIONS, &count);
    assert(ok && count >= 1);

    printf("       Examination CRUD verified -> PASS\n");
}

static void test_exam_subject_scheduling(void)
{
    printf("[TEST] Testing subject scheduling for examination...\n");

    /* Schedule Math */
    ExamSubject es1 = {
        .exam_id = 1,
        .subject_id = 1,
        .exam_date = "2026-10-12",
        .max_marks = 100.0,
        .passing_marks = 35.0
    };
    int es1_id = 0;
    bool ok = exam_subject_create(&es1, &es1_id);
    assert(ok && es1_id > 0 && "Must schedule Math");

    /* Schedule Science */
    ExamSubject es2 = {
        .exam_id = 1,
        .subject_id = 2,
        .exam_date = "2026-10-14",
        .max_marks = 100.0,
        .passing_marks = 35.0
    };
    int es2_id = 0;
    ok = exam_subject_create(&es2, &es2_id);
    assert(ok && es2_id > 0 && "Must schedule Science");

    ExamSubject list[MAX_EXAM_SUBJECTS];
    int count = 0;
    ok = exam_subject_get_by_exam(1, list, MAX_EXAM_SUBJECTS, &count);
    assert(ok && count == 2 && "Must find 2 scheduled subjects");

    printf("       Exam subject scheduling verified -> PASS\n");
}

static void test_marks_entry_and_results(void)
{
    printf("[TEST] Testing marks entry, upsert, and class marksheet...\n");

    /* Student 1: Math 92, Science 85 */
    ExamResult r1 = {
        .exam_subject_id = 1,
        .student_id = 1,
        .marks_obtained = 92.0,
        .remarks = "Excellent work"
    };
    int res1_id = 0;
    assert(exam_result_record(&r1, &res1_id) && res1_id > 0);
    assert(strcmp(r1.grade, "A+") == 0);
    assert(r1.is_pass == true);

    ExamResult r2 = {
        .exam_subject_id = 2,
        .student_id = 1,
        .marks_obtained = 85.0,
        .remarks = "Great job"
    };
    assert(exam_result_record(&r2, NULL));
    assert(strcmp(r2.grade, "A") == 0);

    /* Student 2: Math 30 (Fail), Science 45 (Pass) */
    ExamResult r3 = {
        .exam_subject_id = 1,
        .student_id = 2,
        .marks_obtained = 30.0,
        .remarks = "Needs tuition"
    };
    assert(exam_result_record(&r3, NULL));
    assert(strcmp(r3.grade, "F") == 0);
    assert(r3.is_pass == false);

    ExamResult r4 = {
        .exam_subject_id = 2,
        .student_id = 2,
        .marks_obtained = 45.0,
        .remarks = "Average"
    };
    assert(exam_result_record(&r4, NULL));
    assert(strcmp(r4.grade, "D") == 0);
    assert(r4.is_pass == true);

    /* Verify class sheet for Math (exam_subject_id = 1) */
    ExamResult sheet[10];
    int sheet_count = 0;
    bool ok = exam_result_get_class_sheet(1, sheet, 10, &sheet_count);
    assert(ok && sheet_count == 2 && "Class sheet must return both students");
    assert(sheet[0].student_id == 1 && sheet[0].marks_obtained == 92.0);
    assert(sheet[1].student_id == 2 && sheet[1].marks_obtained == 30.0);

    printf("       Marks entry and class sheet verified -> PASS\n");
}

static void test_report_card_and_class_ranks(void)
{
    printf("[TEST] Testing comprehensive report card and class merit ranking...\n");

    /* Student 1 Report Card */
    StudentReportCard card1;
    bool ok = exam_service_generate_report_card(1, 1, &card1);
    assert(ok && card1.score_count == 2);
    assert(card1.total_marks_obtained == 177.0); /* 92 + 85 */
    assert(card1.total_max_marks == 200.0);
    assert(card1.percentage == 88.5);
    assert(strcmp(card1.overall_grade, "A") == 0);
    assert(card1.is_pass == true);
    assert(card1.class_rank == 1 && "Alice Johnson must be Rank 1");

    /* Student 2 Report Card */
    StudentReportCard card2;
    ok = exam_service_generate_report_card(1, 2, &card2);
    assert(ok && card2.score_count == 2);
    assert(card2.total_marks_obtained == 75.0); /* 30 + 45 */
    assert(card2.percentage == 37.5);
    assert(card2.is_pass == false && "Bob Smith failed Math so overall result is fail");
    assert(card2.class_rank == 2 && "Bob Smith must be Rank 2");

    /* Class Rank list */
    ClassExamRank ranks[10];
    int rank_count = 0;
    ok = exam_service_get_class_ranks(1, 1, ranks, 10, &rank_count);
    assert(ok && rank_count == 2);
    assert(ranks[0].student_id == 1 && ranks[0].rank == 1);
    assert(ranks[1].student_id == 2 && ranks[1].rank == 2);

    printf("       Report card and merit ranking verified -> PASS\n");
}

int main(void)
{
    printf("=========================================\n");
    printf("  SchoolCMS Examination Module Unit Tests \n");
    printf("=========================================\n\n");

    cleanup_files();
    assert(db_init(TEST_DB) == DB_SUCCESS && "Database init failed");

    setup_test_data();
    test_grade_calculation();
    test_exam_crud();
    test_exam_subject_scheduling();
    test_marks_entry_and_results();
    test_report_card_and_class_ranks();

    db_close();
    cleanup_files();

    printf("\nAll examination unit tests passed successfully! 100%% OK.\n");
    return 0;
}
