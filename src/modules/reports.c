/**
 * @file reports.c
 * @brief Report generation, SQL aggregations, and RFC 4180 CSV export implementation.
 */

#include "reports.h"
#include "database.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void report_escape_csv_field(const char *src, char *dst, size_t dst_size)
{
    if (!src || !dst || dst_size == 0) return;

    bool needs_quotes = false;
    for (const char *p = src; *p; p++) {
        if (*p == ',' || *p == '"' || *p == '\n' || *p == '\r') {
            needs_quotes = true;
            break;
        }
    }

    if (!needs_quotes) {
        snprintf(dst, dst_size, "%s", src);
        return;
    }

    size_t out_idx = 0;
    if (out_idx < dst_size - 1) dst[out_idx++] = '"';
    for (const char *p = src; *p && out_idx < dst_size - 2; p++) {
        if (*p == '"') {
            dst[out_idx++] = '"';
            if (out_idx < dst_size - 2) dst[out_idx++] = '"';
        } else {
            dst[out_idx++] = *p;
        }
    }
    if (out_idx < dst_size - 1) dst[out_idx++] = '"';
    dst[out_idx] = '\0';
}

bool report_service_export_csv(const ReportData *data, const char *filepath)
{
    if (!data || !filepath || strlen(filepath) == 0) return false;

    FILE *fp = fopen(filepath, "wb");
    if (!fp) return false;

    /* Write UTF-8 Byte Order Mark for Excel compatibility */
    static const unsigned char bom[] = { 0xEF, 0xBB, 0xBF };
    fwrite(bom, 1, 3, fp);

    /* Header Row */
    for (int col = 0; col < data->column_count; col++) {
        char esc[256];
        report_escape_csv_field(data->header[col], esc, sizeof(esc));
        fprintf(fp, "%s%s", esc, (col + 1 < data->column_count) ? "," : "\r\n");
    }

    /* Data Rows */
    for (int r = 0; r < data->row_count; r++) {
        for (int col = 0; col < data->column_count; col++) {
            char esc[512];
            report_escape_csv_field(data->rows[r][col], esc, sizeof(esc));
            fprintf(fp, "%s%s", esc, (col + 1 < data->column_count) ? "," : "\r\n");
        }
    }

    fclose(fp);

    char details[256];
    snprintf(details, sizeof(details), "Exported %s with %d rows to CSV: %s",
             data->title, data->row_count, filepath);
    db_log_activity(0, "EXPORT_REPORT_CSV", "REPORTS", details);
    return true;
}

/* ── Specific Report Generators ────────────────────────────────────── */

static bool generate_student_list(const ReportFilter *filter, ReportData *d)
{
    d->type = REPORT_STUDENT_LIST;
    strncpy(d->title, "Student Directory Report", sizeof(d->title) - 1);
    d->column_count = 10;
    strncpy(d->header[0], "Admission No", 63);
    strncpy(d->header[1], "Full Name", 63);
    strncpy(d->header[2], "Gender", 63);
    strncpy(d->header[3], "Class", 63);
    strncpy(d->header[4], "Section", 63);
    strncpy(d->header[5], "Roll No", 63);
    strncpy(d->header[6], "Date of Birth", 63);
    strncpy(d->header[7], "Father Name", 63);
    strncpy(d->header[8], "Contact Phone", 63);
    strncpy(d->header[9], "Status", 63);

    sqlite3 *db = db_get_handle();
    if (!db) return false;

    char where[512] = "WHERE 1=1";
    if (filter->class_id > 0) {
        char buf[64];
        snprintf(buf, sizeof(buf), " AND s.class_id = %d", filter->class_id);
        strcat(where, buf);
    }
    if (filter->section_id > 0) {
        char buf[64];
        snprintf(buf, sizeof(buf), " AND s.section_id = %d", filter->section_id);
        strcat(where, buf);
    }
    if (strlen(filter->status_filter) > 0) {
        char buf[64];
        snprintf(buf, sizeof(buf), " AND s.status = '%s'", filter->status_filter);
        strcat(where, buf);
    }

    char sql[1024];
    snprintf(sql, sizeof(sql),
             "SELECT s.admission_no, s.full_name, s.gender, COALESCE(c.name, ''), "
             "       COALESCE(sec.name, ''), COALESCE(s.roll_number, 0), COALESCE(s.dob, ''), "
             "       COALESCE(s.father_name, ''), COALESCE(s.phone, ''), s.status "
             "FROM students s "
             "LEFT JOIN classes c ON s.class_id = c.id "
             "LEFT JOIN sections sec ON s.section_id = sec.id "
             "%s ORDER BY c.name ASC, sec.name ASC, s.roll_number ASC, s.admission_no ASC LIMIT %d;",
             where, MAX_REPORT_ROWS);

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return false;

    int r = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW && r < MAX_REPORT_ROWS) {
        for (int c = 0; c < 10; c++) {
            const char *val = (const char *)sqlite3_column_text(stmt, c);
            strncpy(d->rows[r][c], val ? val : "", MAX_REPORT_CELL_LEN - 1);
        }
        r++;
    }
    sqlite3_finalize(stmt);
    d->row_count = r;
    snprintf(d->summary_text, sizeof(d->summary_text), "Total Students: %d", r);
    return true;
}

static bool generate_attendance_summary(const ReportFilter *filter, ReportData *d)
{
    d->type = REPORT_ATTENDANCE_SUMMARY;
    strncpy(d->title, "Class Attendance Summary Report", sizeof(d->title) - 1);
    d->column_count = 9;
    strncpy(d->header[0], "Admission No", 63);
    strncpy(d->header[1], "Student Name", 63);
    strncpy(d->header[2], "Class", 63);
    strncpy(d->header[3], "Section", 63);
    strncpy(d->header[4], "Total Days", 63);
    strncpy(d->header[5], "Present", 63);
    strncpy(d->header[6], "Absent", 63);
    strncpy(d->header[7], "Late", 63);
    strncpy(d->header[8], "Attendance %", 63);

    sqlite3 *db = db_get_handle();
    if (!db) return false;

    char where[512] = "WHERE 1=1";
    if (filter->class_id > 0) {
        char buf[64];
        snprintf(buf, sizeof(buf), " AND s.class_id = %d", filter->class_id);
        strcat(where, buf);
    }
    if (filter->section_id > 0) {
        char buf[64];
        snprintf(buf, sizeof(buf), " AND s.section_id = %d", filter->section_id);
        strcat(where, buf);
    }
    if (strlen(filter->month_str) > 0) {
        char buf[64];
        snprintf(buf, sizeof(buf), " AND strftime('%%Y-%%m', a.date) = '%s'", filter->month_str);
        strcat(where, buf);
    }

    char sql[1024];
    snprintf(sql, sizeof(sql),
             "SELECT s.admission_no, s.full_name, COALESCE(c.name, ''), COALESCE(sec.name, ''), "
             "       COUNT(a.id) as tot, "
             "       SUM(CASE WHEN a.status = 'PRESENT' THEN 1 ELSE 0 END) as pres, "
             "       SUM(CASE WHEN a.status = 'ABSENT' THEN 1 ELSE 0 END) as abs, "
             "       SUM(CASE WHEN a.status = 'LATE' THEN 1 ELSE 0 END) as lat "
             "FROM students s "
             "JOIN classes c ON s.class_id = c.id "
             "JOIN sections sec ON s.section_id = sec.id "
             "LEFT JOIN attendance a ON s.id = a.student_id "
             "%s GROUP BY s.id ORDER BY s.admission_no ASC LIMIT %d;",
             where, MAX_REPORT_ROWS);

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return false;

    int r = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW && r < MAX_REPORT_ROWS) {
        const char *adm = (const char *)sqlite3_column_text(stmt, 0);
        const char *name = (const char *)sqlite3_column_text(stmt, 1);
        const char *cls = (const char *)sqlite3_column_text(stmt, 2);
        const char *sec = (const char *)sqlite3_column_text(stmt, 3);
        int tot = sqlite3_column_int(stmt, 4);
        int pres = sqlite3_column_int(stmt, 5);
        int ab = sqlite3_column_int(stmt, 6);
        int lt = sqlite3_column_int(stmt, 7);
        double pct = (tot > 0) ? ((double)pres / (double)tot) * 100.0 : 0.0;

        strncpy(d->rows[r][0], adm ? adm : "", MAX_REPORT_CELL_LEN - 1);
        strncpy(d->rows[r][1], name ? name : "", MAX_REPORT_CELL_LEN - 1);
        strncpy(d->rows[r][2], cls ? cls : "", MAX_REPORT_CELL_LEN - 1);
        strncpy(d->rows[r][3], sec ? sec : "", MAX_REPORT_CELL_LEN - 1);
        snprintf(d->rows[r][4], MAX_REPORT_CELL_LEN, "%d", tot);
        snprintf(d->rows[r][5], MAX_REPORT_CELL_LEN, "%d", pres);
        snprintf(d->rows[r][6], MAX_REPORT_CELL_LEN, "%d", ab);
        snprintf(d->rows[r][7], MAX_REPORT_CELL_LEN, "%d", lt);
        snprintf(d->rows[r][8], MAX_REPORT_CELL_LEN, "%.1f%%", pct);
        r++;
    }
    sqlite3_finalize(stmt);
    d->row_count = r;
    snprintf(d->summary_text, sizeof(d->summary_text), "Attendance Records: %d Students", r);
    return true;
}

static bool generate_fee_collection(const ReportFilter *filter, ReportData *d)
{
    d->type = REPORT_FEE_COLLECTION;
    strncpy(d->title, "Fee Collection Register", sizeof(d->title) - 1);
    d->column_count = 8;
    strncpy(d->header[0], "Receipt No", 63);
    strncpy(d->header[1], "Date", 63);
    strncpy(d->header[2], "Admission No", 63);
    strncpy(d->header[3], "Student Name", 63);
    strncpy(d->header[4], "Class", 63);
    strncpy(d->header[5], "Payment Method", 63);
    strncpy(d->header[6], "Amount (INR)", 63);
    strncpy(d->header[7], "Remarks", 63);

    sqlite3 *db = db_get_handle();
    if (!db) return false;

    char where[512] = "WHERE 1=1";
    if (strlen(filter->date_from) > 0) {
        char buf[64];
        snprintf(buf, sizeof(buf), " AND p.payment_date >= '%s'", filter->date_from);
        strcat(where, buf);
    }
    if (strlen(filter->date_to) > 0) {
        char buf[64];
        snprintf(buf, sizeof(buf), " AND p.payment_date <= '%s'", filter->date_to);
        strcat(where, buf);
    }

    char sql[1024];
    snprintf(sql, sizeof(sql),
             "SELECT p.receipt_no, p.payment_date, s.admission_no, s.full_name, "
             "       COALESCE(c.name, ''), p.payment_method, p.total_amount, COALESCE(p.remarks, '') "
             "FROM fee_payments p "
             "JOIN students s ON p.student_id = s.id "
             "LEFT JOIN classes c ON s.class_id = c.id "
             "%s ORDER BY p.payment_date DESC, p.id DESC LIMIT %d;",
             where, MAX_REPORT_ROWS);

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return false;

    int r = 0;
    double total = 0.0;
    while (sqlite3_step(stmt) == SQLITE_ROW && r < MAX_REPORT_ROWS) {
        const char *rcp = (const char *)sqlite3_column_text(stmt, 0);
        const char *dt = (const char *)sqlite3_column_text(stmt, 1);
        const char *adm = (const char *)sqlite3_column_text(stmt, 2);
        const char *name = (const char *)sqlite3_column_text(stmt, 3);
        const char *cls = (const char *)sqlite3_column_text(stmt, 4);
        const char *method = (const char *)sqlite3_column_text(stmt, 5);
        double amt = sqlite3_column_double(stmt, 6);
        const char *rem = (const char *)sqlite3_column_text(stmt, 7);

        total += amt;

        strncpy(d->rows[r][0], rcp ? rcp : "", MAX_REPORT_CELL_LEN - 1);
        strncpy(d->rows[r][1], dt ? dt : "", MAX_REPORT_CELL_LEN - 1);
        strncpy(d->rows[r][2], adm ? adm : "", MAX_REPORT_CELL_LEN - 1);
        strncpy(d->rows[r][3], name ? name : "", MAX_REPORT_CELL_LEN - 1);
        strncpy(d->rows[r][4], cls ? cls : "", MAX_REPORT_CELL_LEN - 1);
        strncpy(d->rows[r][5], method ? method : "", MAX_REPORT_CELL_LEN - 1);
        snprintf(d->rows[r][6], MAX_REPORT_CELL_LEN, "%.2f", amt);
        strncpy(d->rows[r][7], rem ? rem : "", MAX_REPORT_CELL_LEN - 1);
        r++;
    }
    sqlite3_finalize(stmt);
    d->row_count = r;
    d->total_amount = total;
    snprintf(d->summary_text, sizeof(d->summary_text), "Total Receipts: %d  |  Total Collected: INR %.2f", r, total);
    return true;
}

static bool generate_pending_dues(const ReportFilter *filter, ReportData *d)
{
    d->type = REPORT_PENDING_DUES;
    strncpy(d->title, "Pending Fee Dues Report", sizeof(d->title) - 1);
    d->column_count = 8;
    strncpy(d->header[0], "Admission No", 63);
    strncpy(d->header[1], "Student Name", 63);
    strncpy(d->header[2], "Class", 63);
    strncpy(d->header[3], "Section", 63);
    strncpy(d->header[4], "Total Applicable (INR)", 63);
    strncpy(d->header[5], "Total Paid (INR)", 63);
    strncpy(d->header[6], "Pending Due (INR)", 63);
    strncpy(d->header[7], "Fee Status", 63);

    sqlite3 *db = db_get_handle();
    if (!db) return false;

    char where[256] = "WHERE s.status = 'ACTIVE'";
    if (filter->class_id > 0) {
        char buf[64];
        snprintf(buf, sizeof(buf), " AND s.class_id = %d", filter->class_id);
        strcat(where, buf);
    }

    char sql[1536];
    snprintf(sql, sizeof(sql),
             "SELECT s.admission_no, s.full_name, COALESCE(c.name, ''), COALESCE(sec.name, ''), "
             "       COALESCE((SELECT SUM(fs.amount) FROM fee_structures fs WHERE fs.class_id = s.class_id), 0.0) as applicable, "
             "       COALESCE((SELECT SUM(fp.total_amount) FROM fee_payments fp WHERE fp.student_id = s.id), 0.0) as paid "
             "FROM students s "
             "JOIN classes c ON s.class_id = c.id "
             "JOIN sections sec ON s.section_id = sec.id "
             "%s ORDER BY c.name ASC, s.admission_no ASC LIMIT %d;",
             where, MAX_REPORT_ROWS);

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return false;

    int r = 0;
    double total_pending = 0.0;
    while (sqlite3_step(stmt) == SQLITE_ROW && r < MAX_REPORT_ROWS) {
        const char *adm = (const char *)sqlite3_column_text(stmt, 0);
        const char *name = (const char *)sqlite3_column_text(stmt, 1);
        const char *cls = (const char *)sqlite3_column_text(stmt, 2);
        const char *sec = (const char *)sqlite3_column_text(stmt, 3);
        double applicable = sqlite3_column_double(stmt, 4);
        double paid = sqlite3_column_double(stmt, 5);
        double due = applicable - paid;
        if (due < 0.0) due = 0.0;

        total_pending += due;

        const char *st = (due <= 0.01) ? "CLEARED" : (paid > 0.0 ? "PARTIAL" : "PENDING");

        strncpy(d->rows[r][0], adm ? adm : "", MAX_REPORT_CELL_LEN - 1);
        strncpy(d->rows[r][1], name ? name : "", MAX_REPORT_CELL_LEN - 1);
        strncpy(d->rows[r][2], cls ? cls : "", MAX_REPORT_CELL_LEN - 1);
        strncpy(d->rows[r][3], sec ? sec : "", MAX_REPORT_CELL_LEN - 1);
        snprintf(d->rows[r][4], MAX_REPORT_CELL_LEN, "%.2f", applicable);
        snprintf(d->rows[r][5], MAX_REPORT_CELL_LEN, "%.2f", paid);
        snprintf(d->rows[r][6], MAX_REPORT_CELL_LEN, "%.2f", due);
        strncpy(d->rows[r][7], st, MAX_REPORT_CELL_LEN - 1);
        r++;
    }
    sqlite3_finalize(stmt);
    d->row_count = r;
    d->total_amount = total_pending;
    snprintf(d->summary_text, sizeof(d->summary_text), "Students: %d  |  Total Pending Dues: INR %.2f", r, total_pending);
    return true;
}

static bool generate_teacher_list(const ReportFilter *filter, ReportData *d)
{
    d->type = REPORT_TEACHER_LIST;
    strncpy(d->title, "Faculty & Staff Directory Report", sizeof(d->title) - 1);
    d->column_count = 9;
    strncpy(d->header[0], "Teacher Code", 63);
    strncpy(d->header[1], "Full Name", 63);
    strncpy(d->header[2], "Subject Specialization", 63);
    strncpy(d->header[3], "Phone", 63);
    strncpy(d->header[4], "Email", 63);
    strncpy(d->header[5], "Qualification", 63);
    strncpy(d->header[6], "Joining Date", 63);
    strncpy(d->header[7], "Salary (INR)", 63);
    strncpy(d->header[8], "Status", 63);

    sqlite3 *db = db_get_handle();
    if (!db) return false;

    char where[256] = "WHERE 1=1";
    if (strlen(filter->status_filter) > 0) {
        char buf[64];
        snprintf(buf, sizeof(buf), " AND status = '%s'", filter->status_filter);
        strcat(where, buf);
    }

    char sql[1024];
    snprintf(sql, sizeof(sql),
             "SELECT teacher_code, full_name, COALESCE(subject, ''), phone, "
             "       COALESCE(email, ''), COALESCE(qualification, ''), joining_date, "
             "       salary, status "
             "FROM teachers %s ORDER BY id ASC LIMIT %d;",
             where, MAX_REPORT_ROWS);

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return false;

    int r = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW && r < MAX_REPORT_ROWS) {
        for (int c = 0; c < 9; c++) {
            if (c == 7) {
                double sal = sqlite3_column_double(stmt, c);
                snprintf(d->rows[r][c], MAX_REPORT_CELL_LEN, "%.2f", sal);
            } else {
                const char *val = (const char *)sqlite3_column_text(stmt, c);
                strncpy(d->rows[r][c], val ? val : "", MAX_REPORT_CELL_LEN - 1);
            }
        }
        r++;
    }
    sqlite3_finalize(stmt);
    d->row_count = r;
    snprintf(d->summary_text, sizeof(d->summary_text), "Total Faculty Members: %d", r);
    return true;
}

static bool generate_exam_results(const ReportFilter *filter, ReportData *d)
{
    d->type = REPORT_EXAM_RESULTS;
    strncpy(d->title, "Examination Marks & Merit Report", sizeof(d->title) - 1);
    d->column_count = 10;
    strncpy(d->header[0], "Exam Name", 63);
    strncpy(d->header[1], "Admission No", 63);
    strncpy(d->header[2], "Student Name", 63);
    strncpy(d->header[3], "Class", 63);
    strncpy(d->header[4], "Subject", 63);
    strncpy(d->header[5], "Max Marks", 63);
    strncpy(d->header[6], "Pass Marks", 63);
    strncpy(d->header[7], "Marks Obtained", 63);
    strncpy(d->header[8], "Grade", 63);
    strncpy(d->header[9], "Result Status", 63);

    sqlite3 *db = db_get_handle();
    if (!db) return false;

    char where[512] = "WHERE 1=1";
    if (filter->exam_id > 0) {
        char buf[64];
        snprintf(buf, sizeof(buf), " AND es.exam_id = %d", filter->exam_id);
        strcat(where, buf);
    }
    if (filter->class_id > 0) {
        char buf[64];
        snprintf(buf, sizeof(buf), " AND st.class_id = %d", filter->class_id);
        strcat(where, buf);
    }

    char sql[1536];
    snprintf(sql, sizeof(sql),
             "SELECT e.name, st.admission_no, st.full_name, COALESCE(c.name, ''), "
             "       sub.name, es.max_marks, es.passing_marks, r.marks_obtained, r.grade, "
             "       (CASE WHEN r.marks_obtained >= es.passing_marks THEN 1 ELSE 0 END) "
             "FROM exam_results r "
             "JOIN exam_subjects es ON r.exam_subject_id = es.id "
             "JOIN examinations e ON es.exam_id = e.id "
             "JOIN subjects sub ON es.subject_id = sub.id "
             "JOIN students st ON r.student_id = st.id "
             "LEFT JOIN classes c ON st.class_id = c.id "
             "%s ORDER BY e.name ASC, c.name ASC, st.admission_no ASC LIMIT %d;",
             where, MAX_REPORT_ROWS);

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return false;

    int r = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW && r < MAX_REPORT_ROWS) {
        const char *ex_name = (const char *)sqlite3_column_text(stmt, 0);
        const char *adm = (const char *)sqlite3_column_text(stmt, 1);
        const char *name = (const char *)sqlite3_column_text(stmt, 2);
        const char *cls = (const char *)sqlite3_column_text(stmt, 3);
        const char *sub = (const char *)sqlite3_column_text(stmt, 4);
        double max_m = sqlite3_column_double(stmt, 5);
        double pass_m = sqlite3_column_double(stmt, 6);
        double obt_m = sqlite3_column_double(stmt, 7);
        const char *grd = (const char *)sqlite3_column_text(stmt, 8);
        int passed = sqlite3_column_int(stmt, 9);

        strncpy(d->rows[r][0], ex_name ? ex_name : "", MAX_REPORT_CELL_LEN - 1);
        strncpy(d->rows[r][1], adm ? adm : "", MAX_REPORT_CELL_LEN - 1);
        strncpy(d->rows[r][2], name ? name : "", MAX_REPORT_CELL_LEN - 1);
        strncpy(d->rows[r][3], cls ? cls : "", MAX_REPORT_CELL_LEN - 1);
        strncpy(d->rows[r][4], sub ? sub : "", MAX_REPORT_CELL_LEN - 1);
        snprintf(d->rows[r][5], MAX_REPORT_CELL_LEN, "%.1f", max_m);
        snprintf(d->rows[r][6], MAX_REPORT_CELL_LEN, "%.1f", pass_m);
        snprintf(d->rows[r][7], MAX_REPORT_CELL_LEN, "%.1f", obt_m);
        strncpy(d->rows[r][8], grd ? grd : "", MAX_REPORT_CELL_LEN - 1);
        strncpy(d->rows[r][9], passed ? "PASS" : "FAIL", MAX_REPORT_CELL_LEN - 1);
        r++;
    }
    sqlite3_finalize(stmt);
    d->row_count = r;
    snprintf(d->summary_text, sizeof(d->summary_text), "Exam Subject Scores Recorded: %d", r);
    return true;
}

/* ── Service Dispatcher ───────────────────────────────────────────── */

bool report_service_generate(const ReportFilter *filter, ReportData *out_data)
{
    if (!filter || !out_data) return false;
    memset(out_data, 0, sizeof(ReportData));

    switch (filter->type) {
        case REPORT_STUDENT_LIST:
            return generate_student_list(filter, out_data);
        case REPORT_ATTENDANCE_SUMMARY:
            return generate_attendance_summary(filter, out_data);
        case REPORT_FEE_COLLECTION:
            return generate_fee_collection(filter, out_data);
        case REPORT_PENDING_DUES:
            return generate_pending_dues(filter, out_data);
        case REPORT_TEACHER_LIST:
            return generate_teacher_list(filter, out_data);
        case REPORT_EXAM_RESULTS:
            return generate_exam_results(filter, out_data);
        default:
            return false;
    }
}

bool report_service_generate_and_export_csv(const ReportFilter *filter, const char *filepath, int *out_rows)
{
    if (!filter || !filepath) return false;

    ReportData *data = (ReportData *)malloc(sizeof(ReportData));
    if (!data) return false;

    bool ok = report_service_generate(filter, data);
    if (ok) {
        ok = report_service_export_csv(data, filepath);
        if (ok && out_rows) {
            *out_rows = data->row_count;
        }
    }

    free(data);
    return ok;
}
