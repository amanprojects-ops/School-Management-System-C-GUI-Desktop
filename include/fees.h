/**
 * @file fees.h
 * @brief Fee structures, collection, student ledger, duplicate prevention, and receipt generation.
 */

#ifndef SCHOOLCMS_FEES_H
#define SCHOOLCMS_FEES_H

#include <stdbool.h>
#include <stddef.h>

#define MAX_FEE_STRUCTURES 100
#define MAX_FEE_PAYMENTS 200

/**
 * @brief Fee Types
 */
#define FEE_TYPE_MONTHLY    "MONTHLY"
#define FEE_TYPE_ANNUAL     "ANNUAL"
#define FEE_TYPE_ADMISSION  "ADMISSION"
#define FEE_TYPE_TRANSPORT  "TRANSPORT"
#define FEE_TYPE_OTHER      "OTHER"

/**
 * @brief Payment Methods
 */
#define PAYMENT_METHOD_CASH     "CASH"
#define PAYMENT_METHOD_UPI      "UPI"
#define PAYMENT_METHOD_BANK     "BANK_TRANSFER"
#define PAYMENT_METHOD_CHEQUE   "CHEQUE"

/**
 * @brief Fee Structure Definition
 */
typedef struct {
    int id;
    char name[128];              /* e.g. "Term 1 Tuition Fee" */
    int class_id;
    char class_name[64];
    int academic_year_id;
    char academic_year_name[32];
    char fee_type[32];           /* MONTHLY, ANNUAL, ADMISSION, TRANSPORT, OTHER */
    double amount;               /* standard fee amount */
    char due_date[16];           /* YYYY-MM-DD */
} FeeStructure;

/**
 * @brief Fee Payment Record
 */
typedef struct {
    int id;
    char receipt_no[32];         /* e.g. "RCP-2026-0001" */
    int student_id;
    char admission_no[32];
    char student_name[128];
    int roll_number;
    char class_name[64];
    char section_name[32];
    int fee_structure_id;
    char fee_structure_name[128];
    double amount_paid;
    double discount;
    double late_fine;
    double total_amount;         /* amount_paid - discount + late_fine */
    char payment_method[32];     /* CASH, UPI, BANK_TRANSFER, CHEQUE */
    char transaction_ref[64];    /* UPI ref or cheque no */
    char payment_date[16];       /* YYYY-MM-DD */
    char remarks[256];
    int collected_by;
    char collected_by_name[128];
    char created_at[32];
} FeePayment;

/**
 * @brief Filter for Querying Payments
 */
typedef struct {
    int class_id;
    int student_id;
    int fee_structure_id;
    char payment_method[32];
    char date_from[16];
    char date_to[16];
    char search[64];             /* receipt no or student name */
} FeeFilter;

/**
 * @brief Student Fee Ledger / Due Summary
 */
typedef struct {
    int student_id;
    char admission_no[32];
    char student_name[128];
    int roll_number;
    int class_id;
    char class_name[64];
    char section_name[32];
    double total_assigned;       /* sum of fee structures applicable to student class */
    double total_paid;           /* sum of actual payments */
    double total_discount;
    double total_fine;
    double balance_due;          /* total_assigned + total_fine - total_discount - total_paid */
    int payment_count;
} StudentFeeSummary;

/* ── Service Functions ────────────────────────────────────────────── */

/**
 * @brief Generate a thread-safe sequential unique receipt number (RCP-YYYY-XXXX).
 */
bool fee_service_generate_receipt_no(char *out_buf, size_t max_len);

/* Fee Structure Services */
bool fee_structure_get_all(int class_id, int academic_year_id,
                           FeeStructure *out_list, int max_items, int *out_count);
bool fee_structure_get_by_id(int id, FeeStructure *out_struct);
bool fee_structure_create(const FeeStructure *st, int *out_id);
bool fee_structure_update(const FeeStructure *st);
bool fee_structure_delete(int id);

/* Fee Payment Services */
bool fee_payment_create(FeePayment *payment, int *out_id);
bool fee_payment_get_all(const FeeFilter *filter,
                         FeePayment *out_list, int max_items, int *out_count);
bool fee_payment_get_by_id(int id, FeePayment *out_payment);
bool fee_payment_get_by_receipt(const char *receipt_no, FeePayment *out_payment);

/* Ledger and Dues Services */
bool fee_service_get_student_ledger(int student_id,
                                    StudentFeeSummary *out_summary,
                                    FeePayment *out_history,
                                    int max_history,
                                    int *out_history_count);

bool fee_service_get_pending_dues(int class_id,
                                  StudentFeeSummary *out_list,
                                  int max_items,
                                  int *out_count);

#endif /* SCHOOLCMS_FEES_H */
