/**
 * @file test_system_integration.c
 * @brief End-to-end system integration and cross-module regression test suite for SchoolCMS.
 */

#include "database.h"
#include "auth/auth.h"
#include "student.h"
#include "teacher.h"
#include "classes.h"
#include "attendance.h"
#include "fees.h"
#include "exams.h"
#include "timetable.h"
#include "reports.h"
#include "settings.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST_DB "test_schoolcms_integration.db"
#define TEST_BACKUP "test_integration_backup.db"
#define TEST_CSV "test_integration_export.csv"

static void cleanup_files(void)
{
    remove(TEST_DB);
    remove(TEST_DB "-wal");
    remove(TEST_DB "-shm");
    remove(TEST_BACKUP);
    remove(TEST_BACKUP "-wal");
    remove(TEST_BACKUP "-shm");
    remove(TEST_CSV);
}

/* ── 1. Academic & Staff Setup ────────────────────────────────────── */

static void test_academic_and_staff_flow(int *out_class_id, int *out_sec_id, int *out_sub_id, int *out_tch_id)
{
    printf("[1/8] Testing Academic Structure & Staff Lifecycle...\n");

    /* Create Class */
    SchoolClass c = { .id = 0, .numeric_value = 10, .capacity = 45, .academic_year_id = 1 };
    strncpy(c.name, "Grade 10", sizeof(c.name) - 1);
    bool ok = class_service_create(&c, out_class_id);
    assert(ok && *out_class_id > 0);

    /* Create Section */
    ClassSection sec = { .id = 0, .class_id = *out_class_id };
    strncpy(sec.name, "Section Alpha", sizeof(sec.name) - 1);
    ok = section_service_create(&sec, out_sec_id);
    assert(ok && *out_sec_id > 0);

    /* Create Teacher */
    Teacher tch = {
        .id = 0,
        .salary = 75000.0
    };
    strncpy(tch.full_name, "Dr. Richard Feynman", sizeof(tch.full_name) - 1);
    strncpy(tch.phone, "555-8881", sizeof(tch.phone) - 1);
    strncpy(tch.email, "feynman@schoolcms.edu", sizeof(tch.email) - 1);
    strncpy(tch.qualification, "Ph.D. Physics", sizeof(tch.qualification) - 1);
    strncpy(tch.subject, "Quantum Physics", sizeof(tch.subject) - 1);
    strncpy(tch.joining_date, "2026-01-01", sizeof(tch.joining_date) - 1);
    strncpy(tch.status, "ACTIVE", sizeof(tch.status) - 1);

    ok = teacher_service_create(&tch, out_tch_id);
    assert(ok && *out_tch_id > 0);

    /* Create Subject assigned to class and teacher */
    Subject sub = {
        .id = 0,
        .class_id = *out_class_id,
        .teacher_id = *out_tch_id
    };
    strncpy(sub.code, "PHY-101", sizeof(sub.code) - 1);
    strncpy(sub.name, "Introductory Physics", sizeof(sub.name) - 1);
    ok = subject_service_create(&sub, out_sub_id);
    assert(ok && *out_sub_id > 0);

    printf("      Class ID %d, Section ID %d, Subject ID %d, Teacher ID %d created -> PASS\n",
           *out_class_id, *out_sec_id, *out_sub_id, *out_tch_id);
}

/* ── 2. Student Enrollment Flow ───────────────────────────────────── */

static void test_student_enrollment_flow(int class_id, int section_id, int *out_st1, int *out_st2)
{
    printf("[2/8] Testing Student Enrollment & Registration Flow...\n");

    Student s1 = {
        .id = 0,
        .class_id = class_id,
        .section_id = section_id,
        .roll_number = 1
    };
    strncpy(s1.full_name, "Ada Lovelace", sizeof(s1.full_name) - 1);
    strncpy(s1.gender, "FEMALE", sizeof(s1.gender) - 1);
    strncpy(s1.dob, "2010-12-10", sizeof(s1.dob) - 1);
    strncpy(s1.admission_date, "2026-04-01", sizeof(s1.admission_date) - 1);
    strncpy(s1.phone, "555-1212", sizeof(s1.phone) - 1);
    strncpy(s1.status, "ACTIVE", sizeof(s1.status) - 1);

    bool ok = student_service_create(&s1, out_st1);
    assert(ok && *out_st1 > 0);

    Student s2 = {
        .id = 0,
        .class_id = class_id,
        .section_id = section_id,
        .roll_number = 2
    };
    strncpy(s2.full_name, "Charles Babbage", sizeof(s2.full_name) - 1);
    strncpy(s2.gender, "MALE", sizeof(s2.gender) - 1);
    strncpy(s2.dob, "2010-06-25", sizeof(s2.dob) - 1);
    strncpy(s2.admission_date, "2026-04-01", sizeof(s2.admission_date) - 1);
    strncpy(s2.phone, "555-3434", sizeof(s2.phone) - 1);
    strncpy(s2.status, "ACTIVE", sizeof(s2.status) - 1);

    ok = student_service_create(&s2, out_st2);
    assert(ok && *out_st2 > 0);

    /* Verify pagination */
    StudentFilter filter;
    memset(&filter, 0, sizeof(filter));
    filter.class_id = class_id;
    filter.page = 1;
    filter.page_size = 10;

    StudentPage spage;
    ok = student_service_get_page(&filter, &spage);
    assert(ok && spage.total_records >= 2);

    printf("      Students admitted: ID %d (Ada Lovelace), ID %d (Charles Babbage) -> PASS\n",
           *out_st1, *out_st2);
}

/* ── 3. Attendance Tracking Flow ──────────────────────────────────── */

static void test_attendance_flow(int class_id, int section_id, int st1, int st2)
{
    printf("[3/8] Testing Transactional Attendance Lifecycle...\n");

    AttendanceEntry entries[2];
    memset(entries, 0, sizeof(entries));

    entries[0].student_id = st1;
    strncpy(entries[0].status, "PRESENT", sizeof(entries[0].status) - 1);
    strncpy(entries[0].remarks, "On time", sizeof(entries[0].remarks) - 1);

    entries[1].student_id = st2;
    strncpy(entries[1].status, "LATE", sizeof(entries[1].status) - 1);
    strncpy(entries[1].remarks, "Bus delay", sizeof(entries[1].remarks) - 1);

    bool ok = attendance_service_mark_bulk(class_id, section_id, "2026-10-15", entries, 2, 1);
    assert(ok && "Bulk attendance marking must succeed in transaction");

    AttendanceStats stats;
    ok = attendance_service_get_class_stats(class_id, section_id, "2026-10-15", &stats);
    assert(ok);
    assert(stats.present_count == 1);
    assert(stats.late_count == 1);

    printf("      Daily attendance roster marked and verified -> PASS\n");
}

/* ── 4. Fee Management Flow ───────────────────────────────────────── */

static void test_fee_management_flow(int class_id, int st1, int st2)
{
    printf("[4/8] Testing Institutional Fee Accounting & Dues Flow...\n");

    FeeStructure fs = {
        .id = 0,
        .class_id = class_id,
        .academic_year_id = 1,
        .amount = 8000.0
    };
    strncpy(fs.name, "Term 1 Composite Fee", sizeof(fs.name) - 1);
    strncpy(fs.fee_type, FEE_TYPE_ANNUAL, sizeof(fs.fee_type) - 1);
    strncpy(fs.due_date, "2026-10-31", sizeof(fs.due_date) - 1);

    int fs_id = 0;
    bool ok = fee_structure_create(&fs, &fs_id);
    assert(ok && fs_id > 0);

    /* Pay for Student 1 */
    FeePayment pay = {
        .id = 0,
        .student_id = st1,
        .fee_structure_id = fs_id,
        .amount_paid = 8000.0,
        .discount = 0.0,
        .late_fine = 0.0,
        .total_amount = 8000.0,
        .collected_by = 1
    };
    strncpy(pay.payment_method, PAYMENT_METHOD_UPI, sizeof(pay.payment_method) - 1);
    strncpy(pay.payment_date, "2026-10-16", sizeof(pay.payment_date) - 1);
    strncpy(pay.transaction_ref, "UPI-REF-998877", sizeof(pay.transaction_ref) - 1);
    strncpy(pay.remarks, "Term 1 Complete Clearance", sizeof(pay.remarks) - 1);

    int pay_id = 0;
    ok = fee_payment_create(&pay, &pay_id);
    assert(ok && pay_id > 0);
    assert(strlen(pay.receipt_no) > 0);

    /* Verify Pending Dues (only students with positive dues) */
    StudentFeeSummary dues[10];
    int dues_count = 0;
    ok = fee_service_get_pending_dues(class_id, dues, 10, &dues_count);
    assert(ok && dues_count == 1);
    assert(dues[0].student_id == st2);
    assert(dues[0].balance_due == 8000.0);

    /* Verify Student 1 Ledger has 0 balance and 1 payment in history */
    StudentFeeSummary led1;
    FeePayment history[5];
    int hist_count = 0;
    ok = fee_service_get_student_ledger(st1, &led1, history, 5, &hist_count);
    assert(ok);
    assert(led1.balance_due == 0.0);
    assert(hist_count == 1);
    assert(strcmp(history[0].receipt_no, pay.receipt_no) == 0);

    printf("      Fee collection, receipt '%s', and dues verified -> PASS\n", pay.receipt_no);
}

/* ── 5. Examination & Grading Flow ────────────────────────────────── */

static void test_examination_and_results_flow(int class_id, int subject_id, int st1, int st2)
{
    printf("[5/8] Testing Examination Scheduling, Marks Entry & Grading...\n");

    Examination ex = {
        .id = 0,
        .academic_year_id = 1
    };
    strncpy(ex.name, "Autumn Terminal Examination 2026", sizeof(ex.name) - 1);
    strncpy(ex.start_date, "2026-11-01", sizeof(ex.start_date) - 1);
    strncpy(ex.end_date, "2026-11-15", sizeof(ex.end_date) - 1);
    strncpy(ex.grading_system, "PERCENTAGE", sizeof(ex.grading_system) - 1);
    strncpy(ex.status, "COMPLETED", sizeof(ex.status) - 1);

    int ex_id = 0;
    bool ok = exam_service_create(&ex, &ex_id);
    assert(ok && ex_id > 0);

    ExamSubject es = {
        .id = 0,
        .exam_id = ex_id,
        .subject_id = subject_id,
        .max_marks = 100.0,
        .passing_marks = 40.0
    };
    strncpy(es.exam_date, "2026-11-05", sizeof(es.exam_date) - 1);
    int es_id = 0;
    ok = exam_subject_create(&es, &es_id);
    assert(ok && es_id > 0);

    /* Record Marks */
    ExamResult r1 = { .id = 0, .exam_subject_id = es_id, .student_id = st1, .marks_obtained = 95.0 };
    ExamResult r2 = { .id = 0, .exam_subject_id = es_id, .student_id = st2, .marks_obtained = 68.0 };

    int r1_id = 0, r2_id = 0;
    ok = exam_result_record(&r1, &r1_id);
    assert(ok && strcmp(r1.grade, "A+") == 0);

    ok = exam_result_record(&r2, &r2_id);
    assert(ok && strcmp(r2.grade, "B") == 0);

    /* Verify Merit Ranking */
    StudentReportCard rc1;
    ok = exam_service_generate_report_card(ex_id, st1, &rc1);
    assert(ok && rc1.class_rank == 1);
    assert(rc1.percentage == 95.0);

    StudentReportCard rc2;
    ok = exam_service_generate_report_card(ex_id, st2, &rc2);
    assert(ok && rc2.class_rank == 2);
    assert(rc2.percentage == 68.0);

    printf("      Examinations, grading (Rank 1: 95.0%%, Rank 2: 68.0%%) verified -> PASS\n");
}

/* ── 6. Timetable & Conflict Prevention Flow ──────────────────────── */

static void test_timetable_and_conflicts_flow(int class_id, int section_id, int subject_id, int teacher_id)
{
    printf("[6/8] Testing Timetable Allocation & Double-Booking Prevention...\n");

    TimetableSlot slot1 = {
        .id = 0,
        .class_id = class_id,
        .section_id = section_id,
        .period_number = 1,
        .subject_id = subject_id,
        .teacher_id = teacher_id
    };
    strncpy(slot1.day_of_week, DAY_MONDAY, sizeof(slot1.day_of_week) - 1);
    strncpy(slot1.room_number, "Hall 1", sizeof(slot1.room_number) - 1);

    int s1_id = 0;
    ConflictResult conf;
    bool ok = timetable_service_save_slot(&slot1, &s1_id, &conf);
    assert(ok && s1_id > 0 && !conf.has_conflict);

    /* Conflict Test: Same teacher, same day, same period */
    TimetableSlot conf_slot = {
        .id = 0,
        .class_id = class_id,
        .section_id = section_id,
        .period_number = 1,
        .subject_id = subject_id,
        .teacher_id = teacher_id
    };
    strncpy(conf_slot.day_of_week, DAY_MONDAY, sizeof(conf_slot.day_of_week) - 1);
    strncpy(conf_slot.room_number, "Hall 2", sizeof(conf_slot.room_number) - 1);

    int bad_id = 0;
    ok = timetable_service_save_slot(&conf_slot, &bad_id, &conf);
    assert(!ok && conf.has_conflict && conf.type == CONFLICT_TEACHER);

    printf("      Slot allocated and teacher conflict detected -> PASS\n");
}

/* ── 7. Reports & CSV Export Flow ─────────────────────────────────── */

static void test_reports_and_export_flow(void)
{
    printf("[7/8] Testing Institutional Reporting & CSV Generation...\n");

    ReportFilter f;
    memset(&f, 0, sizeof(f));
    f.type = REPORT_STUDENT_LIST;

    int rows = 0;
    bool ok = report_service_generate_and_export_csv(&f, TEST_CSV, &rows);
    assert(ok && rows >= 2);

    /* Check file */
    FILE *fp = fopen(TEST_CSV, "rb");
    assert(fp != NULL);
    unsigned char bom[3] = {0};
    size_t n = fread(bom, 1, 3, fp);
    assert(n == 3 && bom[0] == 0xEF && bom[1] == 0xBB && bom[2] == 0xBF);
    fclose(fp);

    printf("      Report exported %d rows to CSV with UTF-8 BOM -> PASS\n", rows);
}

/* ── 8. Disaster Recovery & Settings Flow ─────────────────────────── */

static void test_settings_and_disaster_recovery_flow(void)
{
    printf("[8/8] Testing Settings Configuration & Backup/Restore Disaster Recovery...\n");

    SchoolSettings s;
    bool ok = settings_service_get(&s);
    assert(ok);

    strncpy(s.school_name, "International Institute of Science", sizeof(s.school_name) - 1);
    ok = settings_service_save(&s);
    assert(ok);

    /* Create Backup */
    ok = settings_service_create_backup(TEST_BACKUP);
    assert(ok);

    /* Modify State */
    settings_service_set_value("school_name", "Corrupted State Name");

    /* Restore Backup */
    ok = settings_service_restore_backup(TEST_BACKUP);
    assert(ok);

    /* Verify State Integrity */
    char restored[128];
    settings_service_get_value("school_name", restored, sizeof(restored));
    assert(strcmp(restored, "International Institute of Science") == 0);

    printf("      Online snapshot backup created and restored with full consistency -> PASS\n");
}

int main(void)
{
    printf("==============================================================\n");
    printf("  SchoolCMS End-to-End System Integration & Regression Suite  \n");
    printf("==============================================================\n");

    cleanup_files();

    int rc = db_init(TEST_DB);
    assert(rc == DB_SUCCESS && "Database init must succeed");

    int class_id = 0, sec_id = 0, sub_id = 0, tch_id = 0;
    int st1 = 0, st2 = 0;

    test_academic_and_staff_flow(&class_id, &sec_id, &sub_id, &tch_id);
    test_student_enrollment_flow(class_id, sec_id, &st1, &st2);
    test_attendance_flow(class_id, sec_id, st1, st2);
    test_fee_management_flow(class_id, st1, st2);
    test_examination_and_results_flow(class_id, sub_id, st1, st2);
    test_timetable_and_conflicts_flow(class_id, sec_id, sub_id, tch_id);
    test_reports_and_export_flow();
    test_settings_and_disaster_recovery_flow();

    db_close();
    cleanup_files();

    printf("\n==============================================================\n");
    printf(">>> ALL END-TO-END SYSTEM INTEGRATION TESTS PASSED (100%%) <<<\n");
    printf("==============================================================\n");
    return 0;
}
