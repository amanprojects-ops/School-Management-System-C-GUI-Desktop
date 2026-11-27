/**
 * @file fees.c
 * @brief Fee collection, structures, student ledger, and receipt generation.
 */

#include "fees.h"
#include "database.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ── Receipt Number Generator ─────────────────────────────────────── */

bool fee_service_generate_receipt_no(char *out_buf, size_t max_len)
{
    if (!out_buf || max_len < 20) return false;
    return db_sync_generate_prefixed_code("fee_payments", "receipt_no", "RCP", out_buf, max_len);
}

/* ── Fee Structure Services ───────────────────────────────────────── */

static const char *FEE_STRUCTURE_SELECT =
    "SELECT fs.id, fs.name, fs.class_id, COALESCE(c.name, 'All Classes'), "
    "       fs.academic_year_id, COALESCE(ay.name, 'Default'), "
    "       fs.fee_type, fs.amount, COALESCE(fs.due_date, '') "
    "FROM fee_structures fs "
    "JOIN classes c ON fs.class_id = c.id "
    "LEFT JOIN academic_years ay ON fs.academic_year_id = ay.id ";

static void read_fee_structure_row(sqlite3_stmt *stmt, FeeStructure *fs)
{
    memset(fs, 0, sizeof(FeeStructure));
    fs->id = sqlite3_column_int(stmt, 0);
    const char *name = (const char *)sqlite3_column_text(stmt, 1);
    fs->class_id = sqlite3_column_int(stmt, 2);
    const char *c_name = (const char *)sqlite3_column_text(stmt, 3);
    fs->academic_year_id = sqlite3_column_int(stmt, 4);
    const char *ay_name = (const char *)sqlite3_column_text(stmt, 5);
    const char *ftype = (const char *)sqlite3_column_text(stmt, 6);
    fs->amount = sqlite3_column_double(stmt, 7);
    const char *due = (const char *)sqlite3_column_text(stmt, 8);

    if (name) strncpy(fs->name, name, sizeof(fs->name) - 1);
    if (c_name) strncpy(fs->class_name, c_name, sizeof(fs->class_name) - 1);
    if (ay_name) strncpy(fs->academic_year_name, ay_name, sizeof(fs->academic_year_name) - 1);
    if (ftype) strncpy(fs->fee_type, ftype, sizeof(fs->fee_type) - 1);
    if (due) strncpy(fs->due_date, due, sizeof(fs->due_date) - 1);
}

bool fee_structure_get_all(int class_id, int academic_year_id,
                           FeeStructure *out_list, int max_items, int *out_count)
{
    if (!out_list || max_items <= 0 || !out_count) return false;
    *out_count = 0;

    sqlite3 *db = db_get_handle();
    if (!db) return false;

    char sql[1024];
    if (class_id > 0 && academic_year_id > 0) {
        snprintf(sql, sizeof(sql), "%s WHERE fs.class_id = ? AND fs.academic_year_id = ? ORDER BY fs.id ASC LIMIT ?;", FEE_STRUCTURE_SELECT);
    } else if (class_id > 0) {
        snprintf(sql, sizeof(sql), "%s WHERE fs.class_id = ? ORDER BY fs.id ASC LIMIT ?;", FEE_STRUCTURE_SELECT);
    } else if (academic_year_id > 0) {
        snprintf(sql, sizeof(sql), "%s WHERE fs.academic_year_id = ? ORDER BY fs.id ASC LIMIT ?;", FEE_STRUCTURE_SELECT);
    } else {
        snprintf(sql, sizeof(sql), "%s ORDER BY c.numeric_value ASC, fs.id ASC LIMIT ?;", FEE_STRUCTURE_SELECT);
    }

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return false;

    int p = 1;
    if (class_id > 0) sqlite3_bind_int(stmt, p++, class_id);
    if (academic_year_id > 0) sqlite3_bind_int(stmt, p++, academic_year_id);
    sqlite3_bind_int(stmt, p, max_items);

    int count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW && count < max_items) {
        read_fee_structure_row(stmt, &out_list[count]);
        count++;
    }
    sqlite3_finalize(stmt);
    *out_count = count;
    return true;
}

bool fee_structure_get_by_id(int id, FeeStructure *out_struct)
{
    if (!out_struct || id <= 0) return false;

    sqlite3 *db = db_get_handle();
    if (!db) return false;

    char sql[1024];
    snprintf(sql, sizeof(sql), "%s WHERE fs.id = ?;", FEE_STRUCTURE_SELECT);

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return false;

    sqlite3_bind_int(stmt, 1, id);
    bool found = false;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        read_fee_structure_row(stmt, out_struct);
        found = true;
    }
    sqlite3_finalize(stmt);
    return found;
}

bool fee_structure_create(const FeeStructure *st, int *out_id)
{
    if (!st || st->class_id <= 0 || strlen(st->name) == 0 || st->amount < 0.0) {
        return false;
    }

    sqlite3 *db = db_get_handle();
    if (!db) return false;

    int ay_id = st->academic_year_id > 0 ? st->academic_year_id : 1;

    const char *sql =
        "INSERT INTO fee_structures (name, class_id, academic_year_id, fee_type, amount, due_date) "
        "VALUES (?, ?, ?, ?, ?, ?);";

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return false;

    sqlite3_bind_text(stmt, 1, st->name, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 2, st->class_id);
    sqlite3_bind_int(stmt, 3, ay_id);
    sqlite3_bind_text(stmt, 4, strlen(st->fee_type) > 0 ? st->fee_type : FEE_TYPE_MONTHLY, -1, SQLITE_STATIC);
    sqlite3_bind_double(stmt, 5, st->amount);
    if (strlen(st->due_date) > 0) sqlite3_bind_text(stmt, 6, st->due_date, -1, SQLITE_STATIC);
    else sqlite3_bind_null(stmt, 6);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) return false;

    int new_id = (int)sqlite3_last_insert_rowid(db);
    if (out_id) *out_id = new_id;

    char details[256];
    snprintf(details, sizeof(details), "Created fee structure '%s' amount %.2f for class ID %d",
             st->name, st->amount, st->class_id);
    db_log_activity(0, "CREATE_FEE_STRUCTURE", "FEES", details);
    return true;
}

bool fee_structure_update(const FeeStructure *st)
{
    if (!st || st->id <= 0 || strlen(st->name) == 0) return false;

    sqlite3 *db = db_get_handle();
    if (!db) return false;

    const char *sql =
        "UPDATE fee_structures SET name = ?, fee_type = ?, amount = ?, due_date = ? "
        "WHERE id = ?;";

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return false;

    sqlite3_bind_text(stmt, 1, st->name, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, st->fee_type, -1, SQLITE_STATIC);
    sqlite3_bind_double(stmt, 3, st->amount);
    if (strlen(st->due_date) > 0) sqlite3_bind_text(stmt, 4, st->due_date, -1, SQLITE_STATIC);
    else sqlite3_bind_null(stmt, 4);
    sqlite3_bind_int(stmt, 5, st->id);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) return false;

    char details[256];
    snprintf(details, sizeof(details), "Updated fee structure ID %d (%s)", st->id, st->name);
    db_log_activity(0, "UPDATE_FEE_STRUCTURE", "FEES", details);
    return true;
}

bool fee_structure_delete(int id)
{
    if (id <= 0) return false;

    sqlite3 *db = db_get_handle();
    if (!db) return false;

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, "DELETE FROM fee_structures WHERE id = ?;", -1, &stmt, NULL) != SQLITE_OK) {
        return false;
    }
    sqlite3_bind_int(stmt, 1, id);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) return false;

    char details[64];
    snprintf(details, sizeof(details), "Deleted fee structure ID %d", id);
    db_log_activity(0, "DELETE_FEE_STRUCTURE", "FEES", details);
    return true;
}

/* ── Fee Payment Services ─────────────────────────────────────────── */

static const char *PAYMENT_SELECT =
    "SELECT fp.id, fp.receipt_no, fp.student_id, "
    "       s.admission_no, s.full_name, s.roll_number, "
    "       COALESCE(c.name, ''), COALESCE(sec.name, ''), "
    "       fp.fee_structure_id, COALESCE(fs.name, ''), "
    "       fp.amount_paid, fp.discount, fp.late_fine, fp.total_amount, "
    "       fp.payment_method, COALESCE(fp.transaction_ref, ''), "
    "       fp.payment_date, COALESCE(fp.remarks, ''), "
    "       fp.collected_by, COALESCE(u.full_name, 'Cashier'), "
    "       fp.created_at "
    "FROM fee_payments fp "
    "JOIN students s ON fp.student_id = s.id "
    "LEFT JOIN classes c ON s.class_id = c.id "
    "LEFT JOIN sections sec ON s.section_id = sec.id "
    "LEFT JOIN fee_structures fs ON fp.fee_structure_id = fs.id "
    "LEFT JOIN users u ON fp.collected_by = u.id ";

static void read_payment_row(sqlite3_stmt *stmt, FeePayment *p)
{
    memset(p, 0, sizeof(FeePayment));
    p->id = sqlite3_column_int(stmt, 0);

    const char *rcp = (const char *)sqlite3_column_text(stmt, 1);
    p->student_id = sqlite3_column_int(stmt, 2);
    const char *adm = (const char *)sqlite3_column_text(stmt, 3);
    const char *s_name = (const char *)sqlite3_column_text(stmt, 4);
    p->roll_number = sqlite3_column_int(stmt, 5);
    const char *c_name = (const char *)sqlite3_column_text(stmt, 6);
    const char *sec_name = (const char *)sqlite3_column_text(stmt, 7);
    p->fee_structure_id = sqlite3_column_int(stmt, 8);
    const char *fs_name = (const char *)sqlite3_column_text(stmt, 9);
    p->amount_paid = sqlite3_column_double(stmt, 10);
    p->discount = sqlite3_column_double(stmt, 11);
    p->late_fine = sqlite3_column_double(stmt, 12);
    p->total_amount = sqlite3_column_double(stmt, 13);
    const char *pm = (const char *)sqlite3_column_text(stmt, 14);
    const char *tx = (const char *)sqlite3_column_text(stmt, 15);
    const char *p_date = (const char *)sqlite3_column_text(stmt, 16);
    const char *rm = (const char *)sqlite3_column_text(stmt, 17);
    p->collected_by = sqlite3_column_int(stmt, 18);
    const char *col_name = (const char *)sqlite3_column_text(stmt, 19);
    const char *cat = (const char *)sqlite3_column_text(stmt, 20);

    if (rcp) strncpy(p->receipt_no, rcp, sizeof(p->receipt_no) - 1);
    if (adm) strncpy(p->admission_no, adm, sizeof(p->admission_no) - 1);
    if (s_name) strncpy(p->student_name, s_name, sizeof(p->student_name) - 1);
    if (c_name) strncpy(p->class_name, c_name, sizeof(p->class_name) - 1);
    if (sec_name) strncpy(p->section_name, sec_name, sizeof(p->section_name) - 1);
    if (fs_name) strncpy(p->fee_structure_name, fs_name, sizeof(p->fee_structure_name) - 1);
    if (pm) strncpy(p->payment_method, pm, sizeof(p->payment_method) - 1);
    if (tx) strncpy(p->transaction_ref, tx, sizeof(p->transaction_ref) - 1);
    if (p_date) strncpy(p->payment_date, p_date, sizeof(p->payment_date) - 1);
    if (rm) strncpy(p->remarks, rm, sizeof(p->remarks) - 1);
    if (col_name) strncpy(p->collected_by_name, col_name, sizeof(p->collected_by_name) - 1);
    if (cat) strncpy(p->created_at, cat, sizeof(p->created_at) - 1);
}

bool fee_payment_create(FeePayment *p, int *out_id)
{
    if (!p || p->student_id <= 0 || p->fee_structure_id <= 0 || p->amount_paid <= 0.0) {
        return false;
    }

    sqlite3 *db = db_get_handle();
    if (!db) return false;

    /* Generate unique receipt number if not supplied */
    if (strlen(p->receipt_no) == 0) {
        if (!fee_service_generate_receipt_no(p->receipt_no, sizeof(p->receipt_no))) {
            return false;
        }
    }

    /* Duplicate check */
    sqlite3_stmt *chk_stmt = NULL;
    if (sqlite3_prepare_v2(db, "SELECT id FROM fee_payments WHERE receipt_no = ?;", -1, &chk_stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_text(chk_stmt, 1, p->receipt_no, -1, SQLITE_STATIC);
        if (sqlite3_step(chk_stmt) == SQLITE_ROW) {
            sqlite3_finalize(chk_stmt);
            return false; /* Duplicate receipt prevention */
        }
        sqlite3_finalize(chk_stmt);
    }

    /* Compute total amount: paid + late_fine - discount */
    p->total_amount = p->amount_paid - p->discount + p->late_fine;

    /* Default date to today if empty */
    if (strlen(p->payment_date) == 0) {
        time_t t = time(NULL);
        struct tm tm = *localtime(&t);
        char date_tmp[32];
        snprintf(date_tmp, sizeof(date_tmp), "%04d-%02d-%02d",
                 tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday);
        strncpy(p->payment_date, date_tmp, sizeof(p->payment_date) - 1);
    }

    if (strlen(p->payment_method) == 0) {
        strncpy(p->payment_method, PAYMENT_METHOD_CASH, sizeof(p->payment_method) - 1);
    }

    const char *sql =
        "INSERT INTO fee_payments (receipt_no, student_id, fee_structure_id, amount_paid, "
        "                          discount, late_fine, total_amount, payment_method, "
        "                          transaction_ref, payment_date, remarks, collected_by) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return false;

    sqlite3_bind_text(stmt, 1, p->receipt_no, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 2, p->student_id);
    sqlite3_bind_int(stmt, 3, p->fee_structure_id);
    sqlite3_bind_double(stmt, 4, p->amount_paid);
    sqlite3_bind_double(stmt, 5, p->discount);
    sqlite3_bind_double(stmt, 6, p->late_fine);
    sqlite3_bind_double(stmt, 7, p->total_amount);
    sqlite3_bind_text(stmt, 8, p->payment_method, -1, SQLITE_STATIC);
    if (strlen(p->transaction_ref) > 0) sqlite3_bind_text(stmt, 9, p->transaction_ref, -1, SQLITE_STATIC);
    else sqlite3_bind_null(stmt, 9);
    sqlite3_bind_text(stmt, 10, p->payment_date, -1, SQLITE_STATIC);
    if (strlen(p->remarks) > 0) sqlite3_bind_text(stmt, 11, p->remarks, -1, SQLITE_STATIC);
    else sqlite3_bind_null(stmt, 11);
    if (p->collected_by > 0) sqlite3_bind_int(stmt, 12, p->collected_by);
    else sqlite3_bind_null(stmt, 12);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) return false;

    int new_id = (int)sqlite3_last_insert_rowid(db);
    p->id = new_id;
    if (out_id) *out_id = new_id;

    char details[256];
    snprintf(details, sizeof(details), "Recorded fee payment %s: amount %.2f for student ID %d",
             p->receipt_no, p->amount_paid, p->student_id);
    db_log_activity(p->collected_by, "FEE_PAYMENT", "FEES", details);
    return true;
}

bool fee_payment_get_all(const FeeFilter *filter,
                         FeePayment *out_list, int max_items, int *out_count)
{
    if (!out_list || max_items <= 0 || !out_count) return false;
    *out_count = 0;

    sqlite3 *db = db_get_handle();
    if (!db) return false;

    char where[1024] = " WHERE 1=1 ";
    if (filter) {
        if (filter->class_id > 0) {
            char buf[64];
            snprintf(buf, sizeof(buf), " AND s.class_id = %d ", filter->class_id);
            strcat(where, buf);
        }
        if (filter->student_id > 0) {
            char buf[64];
            snprintf(buf, sizeof(buf), " AND fp.student_id = %d ", filter->student_id);
            strcat(where, buf);
        }
        if (filter->fee_structure_id > 0) {
            char buf[64];
            snprintf(buf, sizeof(buf), " AND fp.fee_structure_id = %d ", filter->fee_structure_id);
            strcat(where, buf);
        }
        if (strlen(filter->payment_method) > 0) {
            char buf[64];
            snprintf(buf, sizeof(buf), " AND fp.payment_method = '%s' ", filter->payment_method);
            strcat(where, buf);
        }
        if (strlen(filter->date_from) > 0) {
            char buf[64];
            snprintf(buf, sizeof(buf), " AND fp.payment_date >= '%s' ", filter->date_from);
            strcat(where, buf);
        }
        if (strlen(filter->date_to) > 0) {
            char buf[64];
            snprintf(buf, sizeof(buf), " AND fp.payment_date <= '%s' ", filter->date_to);
            strcat(where, buf);
        }
        if (strlen(filter->search) > 0) {
            char buf[256];
            snprintf(buf, sizeof(buf), " AND (fp.receipt_no LIKE '%%%s%%' OR s.full_name LIKE '%%%s%%') ",
                     filter->search, filter->search);
            strcat(where, buf);
        }
    }

    char sql[2048];
    snprintf(sql, sizeof(sql), "%s %s ORDER BY fp.id DESC LIMIT ?;", PAYMENT_SELECT, where);

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return false;

    sqlite3_bind_int(stmt, 1, max_items);

    int count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW && count < max_items) {
        read_payment_row(stmt, &out_list[count]);
        count++;
    }
    sqlite3_finalize(stmt);
    *out_count = count;
    return true;
}

bool fee_payment_get_by_id(int id, FeePayment *out_payment)
{
    if (!out_payment || id <= 0) return false;

    sqlite3 *db = db_get_handle();
    if (!db) return false;

    char sql[1024];
    snprintf(sql, sizeof(sql), "%s WHERE fp.id = ?;", PAYMENT_SELECT);

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return false;

    sqlite3_bind_int(stmt, 1, id);
    bool found = false;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        read_payment_row(stmt, out_payment);
        found = true;
    }
    sqlite3_finalize(stmt);
    return found;
}

bool fee_payment_get_by_receipt(const char *receipt_no, FeePayment *out_payment)
{
    if (!out_payment || !receipt_no || strlen(receipt_no) == 0) return false;

    sqlite3 *db = db_get_handle();
    if (!db) return false;

    char sql[1024];
    snprintf(sql, sizeof(sql), "%s WHERE fp.receipt_no = ?;", PAYMENT_SELECT);

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return false;

    sqlite3_bind_text(stmt, 1, receipt_no, -1, SQLITE_STATIC);
    bool found = false;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        read_payment_row(stmt, out_payment);
        found = true;
    }
    sqlite3_finalize(stmt);
    return found;
}

/* ── Ledger and Dues Services ─────────────────────────────────────── */

bool fee_service_get_student_ledger(int student_id,
                                    StudentFeeSummary *out_summary,
                                    FeePayment *out_history,
                                    int max_history,
                                    int *out_history_count)
{
    if (!out_summary || student_id <= 0) return false;
    memset(out_summary, 0, sizeof(StudentFeeSummary));
    if (out_history_count) *out_history_count = 0;

    sqlite3 *db = db_get_handle();
    if (!db) return false;

    /* 1. Student metadata */
    const char *stu_sql =
        "SELECT s.id, s.admission_no, s.full_name, s.roll_number, s.class_id, "
        "       COALESCE(c.name, ''), COALESCE(sec.name, '') "
        "FROM students s "
        "LEFT JOIN classes c ON s.class_id = c.id "
        "LEFT JOIN sections sec ON s.section_id = sec.id "
        "WHERE s.id = ?;";

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, stu_sql, -1, &stmt, NULL) != SQLITE_OK) return false;

    sqlite3_bind_int(stmt, 1, student_id);
    if (sqlite3_step(stmt) != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        return false;
    }

    out_summary->student_id = sqlite3_column_int(stmt, 0);
    const char *adm = (const char *)sqlite3_column_text(stmt, 1);
    const char *name = (const char *)sqlite3_column_text(stmt, 2);
    out_summary->roll_number = sqlite3_column_int(stmt, 3);
    out_summary->class_id = sqlite3_column_int(stmt, 4);
    const char *cname = (const char *)sqlite3_column_text(stmt, 5);
    const char *sname = (const char *)sqlite3_column_text(stmt, 6);

    if (adm) strncpy(out_summary->admission_no, adm, sizeof(out_summary->admission_no) - 1);
    if (name) strncpy(out_summary->student_name, name, sizeof(out_summary->student_name) - 1);
    if (cname) strncpy(out_summary->class_name, cname, sizeof(out_summary->class_name) - 1);
    if (sname) strncpy(out_summary->section_name, sname, sizeof(out_summary->section_name) - 1);
    sqlite3_finalize(stmt);

    /* 2. Total fee assigned to this student's class */
    const char *fee_sql = "SELECT COALESCE(SUM(amount), 0.0) FROM fee_structures WHERE class_id = ?;";
    if (sqlite3_prepare_v2(db, fee_sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, out_summary->class_id);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            out_summary->total_assigned = sqlite3_column_double(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }

    /* 3. Total paid, discount, late fine by this student */
    const char *pay_sql =
        "SELECT COUNT(*), COALESCE(SUM(amount_paid), 0.0), "
        "       COALESCE(SUM(discount), 0.0), COALESCE(SUM(late_fine), 0.0) "
        "FROM fee_payments WHERE student_id = ?;";

    if (sqlite3_prepare_v2(db, pay_sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, student_id);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            out_summary->payment_count  = sqlite3_column_int(stmt, 0);
            out_summary->total_paid     = sqlite3_column_double(stmt, 1);
            out_summary->total_discount = sqlite3_column_double(stmt, 2);
            out_summary->total_fine     = sqlite3_column_double(stmt, 3);
        }
        sqlite3_finalize(stmt);
    }

    /* Balance due = assigned + late_fine - discount - paid */
    out_summary->balance_due = out_summary->total_assigned + out_summary->total_fine
                             - out_summary->total_discount - out_summary->total_paid;
    if (out_summary->balance_due < 0.0) out_summary->balance_due = 0.0;

    /* 4. Payment history list */
    if (out_history && max_history > 0 && out_history_count) {
        FeeFilter f = { .student_id = student_id };
        fee_payment_get_all(&f, out_history, max_history, out_history_count);
    }

    return true;
}

bool fee_service_get_pending_dues(int class_id,
                                  StudentFeeSummary *out_list,
                                  int max_items,
                                  int *out_count)
{
    if (!out_list || max_items <= 0 || !out_count) return false;
    *out_count = 0;

    sqlite3 *db = db_get_handle();
    if (!db) return false;

    char sql[512];
    if (class_id > 0) {
        snprintf(sql, sizeof(sql),
            "SELECT id FROM students WHERE status = 'ACTIVE' AND class_id = ? ORDER BY roll_number ASC;");
    } else {
        snprintf(sql, sizeof(sql),
            "SELECT id FROM students WHERE status = 'ACTIVE' ORDER BY class_id ASC, roll_number ASC;");
    }

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return false;

    if (class_id > 0) sqlite3_bind_int(stmt, 1, class_id);

    int count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW && count < max_items) {
        int sid = sqlite3_column_int(stmt, 0);
        StudentFeeSummary summary;
        if (fee_service_get_student_ledger(sid, &summary, NULL, 0, NULL)) {
            if (summary.balance_due > 0.001) {
                out_list[count] = summary;
                count++;
            }
        }
    }
    sqlite3_finalize(stmt);
    *out_count = count;
    return true;
}
