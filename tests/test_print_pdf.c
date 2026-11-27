/**
 * @file test_print_pdf.c
 * @brief Unit tests for Cairo vector PDF export across Reports, Fee Receipts, and Marksheets.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "database.h"
#include "utils/print_pdf.h"

#define TEST_DB "test_print_pdf.db"
#define TEST_REPORT_PDF "test_out_report.pdf"
#define TEST_RECEIPT_PDF "test_out_receipt.pdf"
#define TEST_MARKSHEET_PDF "test_out_marksheet.pdf"

static void cleanup_artifacts(void)
{
    remove(TEST_DB);
    remove("test_print_pdf.db-wal");
    remove("test_print_pdf.db-shm");
    remove(TEST_REPORT_PDF);
    remove(TEST_RECEIPT_PDF);
    remove(TEST_MARKSHEET_PDF);
}

static void assert_valid_pdf_file(const char *path, long min_bytes)
{
    FILE *fp = fopen(path, "rb");
    assert(fp != NULL);

    char hdr[8] = {0};
    size_t n = fread(hdr, 1, 5, fp);
    assert(n == 5);
    assert(strncmp(hdr, "%PDF-", 5) == 0);

    fseek(fp, 0, SEEK_END);
    long sz = ftell(fp);
    assert(sz >= min_bytes);
    fclose(fp);
}

static void test_export_multipage_report_pdf(void)
{
    printf("[TEST] Exporting multi-page landscape report PDF...\n");

    ReportData *rd = calloc(1, sizeof(ReportData));
    assert(rd != NULL);
    rd->type = REPORT_STUDENT_LIST;
    strncpy(rd->title, "Student Directory Report (Multi-Page Test)", sizeof(rd->title) - 1);
    rd->column_count = 8;
    const char *headers[8] = { "Adm No", "Student Name", "Gender", "Class", "Sec", "Roll", "Phone", "Status" };
    for (int c = 0; c < 8; c++) {
        strncpy(rd->header[c], headers[c], 63);
    }

    rd->row_count = 65; /* Forces 2+ pages */
    for (int r = 0; r < rd->row_count; r++) {
        snprintf(rd->rows[r][0], MAX_REPORT_CELL_LEN, "ADM-2026-%04d", r + 1);
        snprintf(rd->rows[r][1], MAX_REPORT_CELL_LEN, "Test Student %d", r + 1);
        snprintf(rd->rows[r][2], MAX_REPORT_CELL_LEN, (r % 2 == 0) ? "MALE" : "FEMALE");
        snprintf(rd->rows[r][3], MAX_REPORT_CELL_LEN, "10th");
        snprintf(rd->rows[r][4], MAX_REPORT_CELL_LEN, "A");
        snprintf(rd->rows[r][5], MAX_REPORT_CELL_LEN, "%d", r + 1);
        snprintf(rd->rows[r][6], MAX_REPORT_CELL_LEN, "+91 98765 %05d", 10000 + r);
        snprintf(rd->rows[r][7], MAX_REPORT_CELL_LEN, "ACTIVE");
    }
    snprintf(rd->summary_text, sizeof(rd->summary_text), "Total Institutional Strength: 65 Active Students");

    assert(print_pdf_export_report(rd, TEST_REPORT_PDF));
    free(rd);
    assert_valid_pdf_file(TEST_REPORT_PDF, 1500);
    printf("       Multi-page report PDF verified -> PASS\n");
}

static void test_export_receipt_pdf(void)
{
    printf("[TEST] Exporting official Fee Payment Receipt PDF...\n");

    FeePayment pay;
    memset(&pay, 0, sizeof(FeePayment));
    strncpy(pay.receipt_no, "RCP-2026-0042", sizeof(pay.receipt_no) - 1);
    strncpy(pay.student_name, "Mohan Kumar", sizeof(pay.student_name) - 1);
    strncpy(pay.admission_no, "ADM-2026-0001", sizeof(pay.admission_no) - 1);
    strncpy(pay.class_name, "10th", sizeof(pay.class_name) - 1);
    strncpy(pay.section_name, "A", sizeof(pay.section_name) - 1);
    pay.roll_number = 1;
    strncpy(pay.fee_structure_name, "Monthly Tuition Fee", sizeof(pay.fee_structure_name) - 1);
    strncpy(pay.payment_method, PAYMENT_METHOD_UPI, sizeof(pay.payment_method) - 1);
    strncpy(pay.transaction_ref, "UPI-99887766", sizeof(pay.transaction_ref) - 1);
    strncpy(pay.payment_date, "2026-11-27", sizeof(pay.payment_date) - 1);
    strncpy(pay.collected_by_name, "School Administrator", sizeof(pay.collected_by_name) - 1);
    strncpy(pay.remarks, "Paid on time", sizeof(pay.remarks) - 1);
    pay.amount_paid = 1500.0;
    pay.discount = 100.0;
    pay.late_fine = 0.0;
    pay.total_amount = 1400.0;

    assert(print_pdf_export_receipt(&pay, TEST_RECEIPT_PDF));
    assert_valid_pdf_file(TEST_RECEIPT_PDF, 1000);
    printf("       Fee receipt PDF verified -> PASS\n");
}

static void test_export_marksheet_pdf(void)
{
    printf("[TEST] Exporting official Student Report Card PDF...\n");

    StudentReportCard card;
    memset(&card, 0, sizeof(StudentReportCard));
    strncpy(card.student_name, "Mohan Kumar", sizeof(card.student_name) - 1);
    strncpy(card.admission_no, "ADM-2026-0001", sizeof(card.admission_no) - 1);
    strncpy(card.class_name, "10th", sizeof(card.class_name) - 1);
    strncpy(card.section_name, "A", sizeof(card.section_name) - 1);
    strncpy(card.exam_name, "Mid-Term Examination", sizeof(card.exam_name) - 1);
    strncpy(card.academic_year, "2026-2027", sizeof(card.academic_year) - 1);
    card.roll_number = 1;
    card.class_rank = 1;
    card.score_count = 2;

    strncpy(card.scores[0].code, "CS-101", 31);
    strncpy(card.scores[0].subject_name, "C Programming", 127);
    card.scores[0].max_marks = 100.0;
    card.scores[0].passing_marks = 35.0;
    card.scores[0].marks_obtained = 92.0;
    strncpy(card.scores[0].grade, "A+", 7);
    card.scores[0].is_pass = true;

    strncpy(card.scores[1].code, "DB-102", 31);
    strncpy(card.scores[1].subject_name, "SQLite Systems", 127);
    card.scores[1].max_marks = 100.0;
    card.scores[1].passing_marks = 35.0;
    card.scores[1].marks_obtained = 85.0;
    strncpy(card.scores[1].grade, "A", 7);
    card.scores[1].is_pass = true;

    card.total_marks_obtained = 177.0;
    card.total_max_marks = 200.0;
    card.percentage = 88.5;
    strncpy(card.overall_grade, "A", 7);
    card.is_pass = true;

    assert(print_pdf_export_marksheet(&card, TEST_MARKSHEET_PDF));
    assert_valid_pdf_file(TEST_MARKSHEET_PDF, 1000);
    printf("       Student marksheet PDF verified -> PASS\n");
}

static void test_null_guards(void)
{
    printf("[TEST] Verifying NULL parameter guards...\n");
    ReportData *empty_rd = calloc(1, sizeof(ReportData));
    assert(empty_rd != NULL);
    assert(print_pdf_export_report(NULL, TEST_REPORT_PDF) == false);
    assert(print_pdf_export_report(empty_rd, "") == false);
    free(empty_rd);
    assert(print_pdf_export_receipt(NULL, TEST_RECEIPT_PDF) == false);
    assert(print_pdf_export_marksheet(NULL, TEST_MARKSHEET_PDF) == false);
    printf("       NULL guards verified -> PASS\n");
}

int main(void)
{
    printf("=========================================\n");
    printf("    SchoolCMS Vector PDF Export Tests    \n");
    printf("=========================================\n\n");

    cleanup_artifacts();
    assert(db_init(TEST_DB) == DB_SUCCESS);

    test_null_guards();
    test_export_multipage_report_pdf();
    test_export_receipt_pdf();
    test_export_marksheet_pdf();

    db_close();
    cleanup_artifacts();

    printf("\nAll vector PDF export tests passed successfully!\n");
    return 0;
}
