/**
 * @file test_fees.c
 * @brief Unit tests for fee structures, payments, ledger calculation, and duplicate prevention.
 */

#include "fees.h"
#include "database.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST_DB "test_schoolcms_fees.db"

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

    /* Insert test students */
    assert(sqlite3_exec(db, "INSERT INTO students (id, admission_no, full_name, dob, gender, admission_date, class_id, section_id, roll_number, status) "
                            "VALUES (1, 'ADM-001', 'Alice Johnson', '2010-01-01', 'FEMALE', '2026-04-01', 1, 1, 1, 'ACTIVE'),"
                            "       (2, 'ADM-002', 'Bob Smith', '2010-02-02', 'MALE', '2026-04-01', 1, 1, 2, 'ACTIVE');",
                        NULL, NULL, NULL) == SQLITE_OK);
}

static void test_receipt_no_generation(void)
{
    printf("[TEST] Testing receipt number generation...\n");

    char rcp1[32];
    assert(fee_service_generate_receipt_no(rcp1, sizeof(rcp1)));
    assert(strncmp(rcp1, "RCP-", 4) == 0);

    printf("       Generated receipt number format: %s -> PASS\n", rcp1);
}

static void test_fee_structures(void)
{
    printf("[TEST] Testing fee structure CRUD...\n");

    FeeStructure st = {
        .id = 0,
        .name = "Term 1 Tuition Fee",
        .class_id = 1,
        .academic_year_id = 1,
        .fee_type = FEE_TYPE_MONTHLY,
        .amount = 3000.0,
        .due_date = "2026-05-10"
    };

    int st_id = 0;
    bool ok = fee_structure_create(&st, &st_id);
    assert(ok && st_id > 0 && "Must create fee structure");

    FeeStructure fetched;
    ok = fee_structure_get_by_id(st_id, &fetched);
    assert(ok && fetched.id == st_id);
    assert(strcmp(fetched.name, "Term 1 Tuition Fee") == 0);
    assert(fetched.amount == 3000.0);

    /* Update structure */
    strncpy(fetched.name, "Term 1 Tuition (Revised)", sizeof(fetched.name) - 1);
    fetched.amount = 3200.0;
    ok = fee_structure_update(&fetched);
    assert(ok && "Must update fee structure");

    FeeStructure updated;
    fee_structure_get_by_id(st_id, &updated);
    assert(updated.amount == 3200.0);

    /* Create second fee structure: Annual Activity Fee */
    FeeStructure st2 = {
        .name = "Annual Activity Fee",
        .class_id = 1,
        .academic_year_id = 1,
        .fee_type = FEE_TYPE_ANNUAL,
        .amount = 1500.0,
        .due_date = "2026-06-15"
    };
    int st2_id = 0;
    assert(fee_structure_create(&st2, &st2_id) && st2_id > 0);

    FeeStructure list[MAX_FEE_STRUCTURES];
    int count = 0;
    ok = fee_structure_get_all(1, 0, list, MAX_FEE_STRUCTURES, &count);
    assert(ok && count >= 2 && "Must find both fee structures");

    printf("       Fee structure CRUD verified -> PASS\n");
}

static void test_fee_payments_and_duplicate_prevention(void)
{
    printf("[TEST] Testing payment recording, total calculation, and duplicate receipt prevention...\n");

    FeePayment p1 = {
        .receipt_no = "", /* auto-generate */
        .student_id = 1,
        .fee_structure_id = 1,
        .amount_paid = 2000.0,
        .discount = 200.0,
        .late_fine = 50.0,
        .payment_method = PAYMENT_METHOD_UPI,
        .transaction_ref = "UPI/2026/8989",
        .payment_date = "2026-05-02",
        .remarks = "First installment",
        .collected_by = 1
    };

    int pay_id = 0;
    bool ok = fee_payment_create(&p1, &pay_id);
    assert(ok && pay_id > 0 && "Must record fee payment");
    assert(strlen(p1.receipt_no) > 0 && "Receipt number must be generated");
    assert(p1.total_amount == (2000.0 - 200.0 + 50.0) && "Total amount must be net calculation");

    /* Attempt duplicate insertion with same receipt number -> MUST FAIL */
    FeePayment dup = p1;
    dup.id = 0;
    int dup_id = 0;
    bool dup_res = fee_payment_create(&dup, &dup_id);
    assert(!dup_res && "Duplicate payment submission must be prevented");

    /* Query payment by receipt */
    FeePayment fetched;
    ok = fee_payment_get_by_receipt(p1.receipt_no, &fetched);
    assert(ok && fetched.id == pay_id);
    assert(strcmp(fetched.payment_method, PAYMENT_METHOD_UPI) == 0);
    assert(fetched.amount_paid == 2000.0);

    printf("       Payment recording & duplicate prevention verified -> PASS\n");
}

static void test_student_ledger_and_dues(void)
{
    printf("[TEST] Testing student fee ledger and pending dues calculation...\n");

    /* Student 1:
       Total assigned fee for Class 10 = 3200 (Term 1) + 1500 (Annual) = 4700.0
       Student 1 paid: amount_paid = 2000.0, discount = 200.0, late_fine = 50.0
       balance due = 4700 + 50 - 200 - 2000 = 2550.0
    */
    StudentFeeSummary s1_summary;
    FeePayment history[10];
    int hist_count = 0;
    bool ok = fee_service_get_student_ledger(1, &s1_summary, history, 10, &hist_count);
    assert(ok);
    assert(s1_summary.total_assigned == 4700.0);
    assert(s1_summary.total_paid == 2000.0);
    assert(s1_summary.total_discount == 200.0);
    assert(s1_summary.total_fine == 50.0);
    assert(s1_summary.balance_due == 2550.0);
    assert(hist_count == 1);

    /* Student 2:
       Has paid 0 so far.
       balance due = 4700.0
    */
    StudentFeeSummary s2_summary;
    ok = fee_service_get_student_ledger(2, &s2_summary, NULL, 0, NULL);
    assert(ok);
    assert(s2_summary.total_assigned == 4700.0);
    assert(s2_summary.total_paid == 0.0);
    assert(s2_summary.balance_due == 4700.0);

    /* Pending dues query */
    StudentFeeSummary dues_list[10];
    int dues_count = 0;
    ok = fee_service_get_pending_dues(1, dues_list, 10, &dues_count);
    assert(ok && dues_count == 2 && "Both students have pending dues");

    printf("       Student ledger and pending dues calculations verified -> PASS\n");
}

int main(void)
{
    printf("=========================================\n");
    printf("     SchoolCMS Fee Module Unit Tests     \n");
    printf("=========================================\n\n");

    cleanup_files();
    assert(db_init(TEST_DB) == DB_SUCCESS && "Database init failed");

    setup_test_data();
    test_receipt_no_generation();
    test_fee_structures();
    test_fee_payments_and_duplicate_prevention();
    test_student_ledger_and_dues();

    db_close();
    cleanup_files();

    printf("\nAll fee management unit tests passed successfully! 100%% OK.\n");
    return 0;
}
