/**
 * @file print_pdf.c
 * @brief Vector PDF generation and GTK4 native print engine using Cairo PDF and Pango.
 */

#include "utils/print_pdf.h"
#include "settings.h"
#include "database.h"
#include <cairo.h>
#include <cairo-pdf.h>
#include <pango/pangocairo.h>
#include <glib/gstdio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define A4_PORTRAIT_W  595.28
#define A4_PORTRAIT_H  841.89
#define A4_LANDSCAPE_W 841.89
#define A4_LANDSCAPE_H 595.28

#define PAGE_MARGIN     38.0
#define REPORT_ROW_H    21.0
#define REPORT_HDR_H    25.0

/* ── Internal Helpers ─────────────────────────────────────────────── */

static void load_brand_settings(SchoolSettings *out)
{
    memset(out, 0, sizeof(SchoolSettings));
    if (!settings_service_get(out) || out->school_name[0] == '\0') {
        strncpy(out->school_name, "APCOACHING Institution", sizeof(out->school_name) - 1);
    }
    if (out->currency_symbol[0] == '\0') {
        strncpy(out->currency_symbol, "₹", sizeof(out->currency_symbol) - 1);
    }
    if (out->current_academic_year_name[0] == '\0') {
        strncpy(out->current_academic_year_name, "2026-2027", sizeof(out->current_academic_year_name) - 1);
    }
}

static void format_timestamp(char *buf, size_t size)
{
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    if (tm_info) {
        strftime(buf, size, "%Y-%m-%d %H:%M", tm_info);
    } else {
        snprintf(buf, size, "Unknown Date");
    }
}

static void format_date_slug(char *buf, size_t size)
{
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    if (tm_info) {
        strftime(buf, size, "%Y%m%d_%H%M%S", tm_info);
    } else {
        snprintf(buf, size, "export");
    }
}

static cairo_status_t stdio_cairo_write(void *closure,
                                        const unsigned char *data,
                                        unsigned int length)
{
    FILE *fp = (FILE *)closure;
    if (!fp) return CAIRO_STATUS_WRITE_ERROR;
    if (length == 0) return CAIRO_STATUS_SUCCESS;
    return (fwrite(data, 1, length, fp) == length) ? CAIRO_STATUS_SUCCESS : CAIRO_STATUS_WRITE_ERROR;
}

static void draw_filled_rect(cairo_t *cr, double x, double y, double w, double h,
                             double r, double g, double b, double a)
{
    cairo_save(cr);
    cairo_set_source_rgba(cr, r, g, b, a);
    cairo_rectangle(cr, x, y, w, h);
    cairo_fill(cr);
    cairo_restore(cr);
}

static void draw_stroked_rect(cairo_t *cr, double x, double y, double w, double h,
                              double r, double g, double b, double lw)
{
    cairo_save(cr);
    cairo_set_source_rgb(cr, r, g, b);
    cairo_set_line_width(cr, lw);
    cairo_rectangle(cr, x, y, w, h);
    cairo_stroke(cr);
    cairo_restore(cr);
}

static void draw_hline(cairo_t *cr, double x1, double x2, double y,
                       double r, double g, double b, double lw)
{
    cairo_save(cr);
    cairo_set_source_rgb(cr, r, g, b);
    cairo_set_line_width(cr, lw);
    cairo_move_to(cr, x1, y);
    cairo_line_to(cr, x2, y);
    cairo_stroke(cr);
    cairo_restore(cr);
}

static void draw_text_box(cairo_t *cr,
                          const char *font_desc_str,
                          PangoAlignment align,
                          double r, double g, double b,
                          double x, double y, double max_w, double box_h,
                          gboolean single_line_ellipsize,
                          const char *text)
{
    if (!text) text = "";
    cairo_save(cr);
    PangoLayout *layout = pango_cairo_create_layout(cr);
    PangoFontDescription *desc = pango_font_description_from_string(font_desc_str);
    pango_layout_set_font_description(layout, desc);
    pango_font_description_free(desc);

    pango_layout_set_text(layout, text, -1);
    pango_layout_set_alignment(layout, align);
    if (max_w > 0.0) {
        pango_layout_set_width(layout, (int)(max_w * PANGO_SCALE));
        if (single_line_ellipsize) {
            pango_layout_set_ellipsize(layout, PANGO_ELLIPSIZE_END);
        } else {
            pango_layout_set_wrap(layout, PANGO_WRAP_WORD_CHAR);
        }
    }

    int pw = 0, ph = 0;
    pango_layout_get_pixel_size(layout, &pw, &ph);
    double draw_y = y;
    if (box_h > 0.0 && (double)ph < box_h) {
        draw_y = y + (box_h - (double)ph) * 0.5;
    }

    cairo_set_source_rgb(cr, r, g, b);
    cairo_move_to(cr, x, draw_y);
    pango_cairo_show_layout(cr, layout);
    g_object_unref(layout);
    cairo_restore(cr);
}

static void draw_page_footer(cairo_t *cr, double page_w, double page_h,
                             int page_idx, int total_pages, const char *left_note)
{
    double fy = page_h - PAGE_MARGIN + 6.0;
    double usable_w = page_w - 2.0 * PAGE_MARGIN;
    draw_hline(cr, PAGE_MARGIN, page_w - PAGE_MARGIN, fy - 4.0, 0.80, 0.84, 0.88, 0.75);

    draw_text_box(cr, "Segoe UI 8", PANGO_ALIGN_LEFT,
                  0.38, 0.43, 0.50,
                  PAGE_MARGIN, fy, usable_w * 0.72, 16.0, TRUE,
                  left_note ? left_note : "Generated by APCOACHING School Management System");

    char pg_txt[64];
    snprintf(pg_txt, sizeof(pg_txt), "Page %d of %d", page_idx + 1, total_pages > 0 ? total_pages : 1);
    draw_text_box(cr, "Segoe UI 8", PANGO_ALIGN_RIGHT,
                  0.38, 0.43, 0.50,
                  PAGE_MARGIN + usable_w * 0.72, fy, usable_w * 0.28, 16.0, TRUE,
                  pg_txt);
}

/* ── Report Pagination & Rendering ────────────────────────────────── */

typedef struct {
    int page1_cap;
    int page_n_cap;
    int total_pages;
} ReportLayout;

static ReportLayout compute_report_layout(const ReportData *data, double page_h)
{
    ReportLayout rl = { 1, 1, 1 };
    double p1_header_h = 84.0;
    double pn_header_h = 40.0;
    double footer_reserve = 38.0;
    double summary_reserve = 28.0;

    double p1_avail = page_h - 2.0 * PAGE_MARGIN - p1_header_h - REPORT_HDR_H - footer_reserve - summary_reserve;
    double pn_avail = page_h - 2.0 * PAGE_MARGIN - pn_header_h - REPORT_HDR_H - footer_reserve - summary_reserve;

    rl.page1_cap = (int)(p1_avail / REPORT_ROW_H);
    rl.page_n_cap = (int)(pn_avail / REPORT_ROW_H);
    if (rl.page1_cap < 1) rl.page1_cap = 1;
    if (rl.page_n_cap < 1) rl.page_n_cap = 1;

    if (!data || data->row_count <= rl.page1_cap) {
        rl.total_pages = 1;
    } else {
        int rem = data->row_count - rl.page1_cap;
        rl.total_pages = 1 + (rem + rl.page_n_cap - 1) / rl.page_n_cap;
    }
    return rl;
}

static void draw_report_page(cairo_t *cr, double page_w, double page_h,
                             const ReportData *data, const SchoolSettings *settings,
                             int page_idx)
{
    ReportLayout rl = compute_report_layout(data, page_h);
    double usable_w = page_w - 2.0 * PAGE_MARGIN;
    double cur_y = PAGE_MARGIN;

    char ts[48];
    format_timestamp(ts, sizeof(ts));

    if (page_idx == 0) {
        /* Top institutional accent bar */
        draw_filled_rect(cr, PAGE_MARGIN, cur_y, usable_w, 54.0, 0.06, 0.16, 0.29, 1.0);
        draw_text_box(cr, "Segoe UI Bold 13", PANGO_ALIGN_LEFT,
                      1.0, 1.0, 1.0,
                      PAGE_MARGIN + 14.0, cur_y + 7.0, usable_w * 0.65, 22.0, TRUE,
                      settings->school_name);

        char contact_line[384];
        snprintf(contact_line, sizeof(contact_line), "%s%s%s",
                 settings->school_address[0] ? settings->school_address : "Institutional Report",
                 settings->school_phone[0] ? "  •  Tel: " : "",
                 settings->school_phone);
        draw_text_box(cr, "Segoe UI 8.5", PANGO_ALIGN_LEFT,
                      0.82, 0.88, 0.96,
                      PAGE_MARGIN + 14.0, cur_y + 30.0, usable_w * 0.65, 18.0, TRUE,
                      contact_line);

        char yr_lbl[128];
        snprintf(yr_lbl, sizeof(yr_lbl), "Session %s\n%s", settings->current_academic_year_name, ts);
        draw_text_box(cr, "Segoe UI 8.5", PANGO_ALIGN_RIGHT,
                      0.85, 0.91, 0.99,
                      PAGE_MARGIN + usable_w * 0.65, cur_y + 10.0, usable_w * 0.35 - 14.0, 36.0, FALSE,
                      yr_lbl);

        cur_y += 60.0;
        draw_text_box(cr, "Segoe UI Bold 12", PANGO_ALIGN_LEFT,
                      0.06, 0.16, 0.29,
                      PAGE_MARGIN, cur_y, usable_w * 0.75, 20.0, TRUE,
                      data->title[0] ? data->title : "Institutional Report");

        char row_badge[64];
        snprintf(row_badge, sizeof(row_badge), "Total Records: %d", data->row_count);
        draw_text_box(cr, "Segoe UI Bold 9", PANGO_ALIGN_RIGHT,
                      0.15, 0.39, 0.92,
                      PAGE_MARGIN + usable_w * 0.75, cur_y, usable_w * 0.25, 20.0, TRUE,
                      row_badge);
        cur_y += 24.0;
    } else {
        char cont_hdr[384];
        snprintf(cont_hdr, sizeof(cont_hdr), "%s — %s (Continued)",
                 settings->school_name, data->title[0] ? data->title : "Report");
        draw_text_box(cr, "Segoe UI Bold 10", PANGO_ALIGN_LEFT,
                      0.06, 0.16, 0.29,
                      PAGE_MARGIN, cur_y, usable_w, 22.0, TRUE, cont_hdr);
        draw_hline(cr, PAGE_MARGIN, page_w - PAGE_MARGIN, cur_y + 24.0, 0.15, 0.39, 0.92, 1.0);
        cur_y += 32.0;
    }

    int cols = data->column_count;
    if (cols < 1) cols = 1;
    if (cols > MAX_REPORT_COLUMNS) cols = MAX_REPORT_COLUMNS;

    /* Distribute column widths proportionally, giving column 1 (usually Full Name) slightly more room */
    double col_w[MAX_REPORT_COLUMNS];
    double weight_sum = 0.0;
    for (int c = 0; c < cols; c++) {
        weight_sum += (c == 1 && cols > 2) ? 1.45 : 1.0;
    }
    for (int c = 0; c < cols; c++) {
        double w = (c == 1 && cols > 2) ? 1.45 : 1.0;
        col_w[c] = (usable_w * w) / weight_sum;
    }

    /* Table Header Strip */
    draw_filled_rect(cr, PAGE_MARGIN, cur_y, usable_w, REPORT_HDR_H, 0.10, 0.21, 0.36, 1.0);
    double cx = PAGE_MARGIN;
    for (int c = 0; c < cols; c++) {
        draw_text_box(cr, "Segoe UI Bold 8.5", PANGO_ALIGN_LEFT,
                      1.0, 1.0, 1.0,
                      cx + 5.0, cur_y, col_w[c] - 10.0, REPORT_HDR_H, TRUE,
                      data->header[c]);
        cx += col_w[c];
    }
    cur_y += REPORT_HDR_H;

    int start_row = 0;
    int end_row = 0;
    if (page_idx == 0) {
        start_row = 0;
        end_row = MIN(data->row_count, rl.page1_cap);
    } else {
        start_row = rl.page1_cap + (page_idx - 1) * rl.page_n_cap;
        end_row = MIN(data->row_count, start_row + rl.page_n_cap);
    }

    if (data->row_count == 0) {
        draw_filled_rect(cr, PAGE_MARGIN, cur_y, usable_w, 34.0, 0.96, 0.97, 0.99, 1.0);
        draw_stroked_rect(cr, PAGE_MARGIN, cur_y, usable_w, 34.0, 0.82, 0.85, 0.90, 0.6);
        draw_text_box(cr, "Segoe UI Italic 9.5", PANGO_ALIGN_CENTER,
                      0.40, 0.45, 0.52,
                      PAGE_MARGIN, cur_y, usable_w, 34.0, TRUE,
                      "No records matched the active filter criteria.");
        cur_y += 38.0;
    } else {
        double table_top = cur_y;
        for (int r = start_row; r < end_row; r++) {
            if ((r - start_row) % 2 == 1) {
                draw_filled_rect(cr, PAGE_MARGIN, cur_y, usable_w, REPORT_ROW_H, 0.95, 0.97, 0.99, 1.0);
            }
            cx = PAGE_MARGIN;
            for (int c = 0; c < cols; c++) {
                const char *font = (c == 1) ? "Segoe UI Bold 8" : "Segoe UI 8";
                draw_text_box(cr, font, PANGO_ALIGN_LEFT,
                              0.12, 0.16, 0.23,
                              cx + 5.0, cur_y, col_w[c] - 10.0, REPORT_ROW_H, TRUE,
                              data->rows[r][c]);
                cx += col_w[c];
            }
            draw_hline(cr, PAGE_MARGIN, PAGE_MARGIN + usable_w, cur_y + REPORT_ROW_H,
                       0.88, 0.90, 0.93, 0.5);
            cur_y += REPORT_ROW_H;
        }
        draw_stroked_rect(cr, PAGE_MARGIN, table_top - REPORT_HDR_H, usable_w,
                          (cur_y - table_top) + REPORT_HDR_H, 0.78, 0.82, 0.88, 0.75);
    }

    /* Summary bar after final row on last page */
    if (page_idx == rl.total_pages - 1 && data->summary_text[0] != '\0') {
        cur_y += 8.0;
        draw_filled_rect(cr, PAGE_MARGIN, cur_y, usable_w, 24.0, 0.93, 0.96, 1.0, 1.0);
        draw_stroked_rect(cr, PAGE_MARGIN, cur_y, usable_w, 24.0, 0.75, 0.84, 0.97, 0.75);
        draw_text_box(cr, "Segoe UI Bold 9", PANGO_ALIGN_LEFT,
                      0.06, 0.16, 0.29,
                      PAGE_MARGIN + 10.0, cur_y, usable_w - 20.0, 24.0, TRUE,
                      data->summary_text);
    }

    char footer_note[256];
    snprintf(footer_note, sizeof(footer_note), "%s  •  Exported %s", settings->school_name, ts);
    draw_page_footer(cr, page_w, page_h, page_idx, rl.total_pages, footer_note);
}

/* ── Receipt Rendering ────────────────────────────────────────────── */

static void draw_receipt_page(cairo_t *cr, double page_w, double page_h,
                              const FeePayment *p, const SchoolSettings *settings)
{
    double usable_w = page_w - 2.0 * PAGE_MARGIN;
    double cur_y = PAGE_MARGIN;
    const char *curr = settings->currency_symbol;

    char ts[48];
    format_timestamp(ts, sizeof(ts));

    /* Outer subtle receipt border */
    double box_h = 540.0;
    if (box_h > page_h - 2.0 * PAGE_MARGIN - 30.0) {
        box_h = page_h - 2.0 * PAGE_MARGIN - 30.0;
    }
    draw_stroked_rect(cr, PAGE_MARGIN, cur_y, usable_w, box_h, 0.78, 0.82, 0.88, 1.0);

    /* Top Institution Header Banner */
    draw_filled_rect(cr, PAGE_MARGIN, cur_y, usable_w, 72.0, 0.06, 0.16, 0.29, 1.0);
    draw_text_box(cr, "Segoe UI Bold 15", PANGO_ALIGN_CENTER,
                  1.0, 1.0, 1.0,
                  PAGE_MARGIN + 12.0, cur_y + 10.0, usable_w - 24.0, 24.0, TRUE,
                  settings->school_name);

    char sub_hdr[384];
    snprintf(sub_hdr, sizeof(sub_hdr), "%s%s%s",
             settings->school_address[0] ? settings->school_address : "Official Academic Campus",
             settings->school_phone[0] ? "   |   Tel: " : "",
             settings->school_phone);
    draw_text_box(cr, "Segoe UI 9", PANGO_ALIGN_CENTER,
                  0.82, 0.88, 0.96,
                  PAGE_MARGIN + 12.0, cur_y + 34.0, usable_w - 24.0, 18.0, TRUE,
                  sub_hdr);

    draw_text_box(cr, "Segoe UI Bold 9.5", PANGO_ALIGN_CENTER,
                  0.58, 0.77, 0.99,
                  PAGE_MARGIN + 12.0, cur_y + 51.0, usable_w - 24.0, 16.0, TRUE,
                  "OFFICIAL FEE PAYMENT RECEIPT");

    cur_y += 86.0;
    double inner_x = PAGE_MARGIN + 18.0;
    double inner_w = usable_w - 36.0;

    /* Receipt Number & Date */
    char rcp_line[96], dt_line[64];
    snprintf(rcp_line, sizeof(rcp_line), "Receipt No: %s", p->receipt_no);
    snprintf(dt_line, sizeof(dt_line), "Payment Date: %s", p->payment_date);
    draw_text_box(cr, "Segoe UI Bold 11", PANGO_ALIGN_LEFT,
                  0.06, 0.16, 0.29,
                  inner_x, cur_y, inner_w * 0.55, 22.0, TRUE, rcp_line);
    draw_text_box(cr, "Segoe UI Bold 10", PANGO_ALIGN_RIGHT,
                  0.25, 0.32, 0.42,
                  inner_x + inner_w * 0.55, cur_y, inner_w * 0.45, 22.0, TRUE, dt_line);
    cur_y += 28.0;

    /* Student Particulars Card */
    draw_filled_rect(cr, inner_x, cur_y, inner_w, 60.0, 0.96, 0.97, 0.99, 1.0);
    draw_stroked_rect(cr, inner_x, cur_y, inner_w, 60.0, 0.84, 0.87, 0.92, 0.75);

    char stu_l1[256], stu_l2[256];
    snprintf(stu_l1, sizeof(stu_l1), "Student Name: %s    (Admission No: %s)",
             p->student_name[0] ? p->student_name : "N/A",
             p->admission_no[0] ? p->admission_no : "N/A");
    snprintf(stu_l2, sizeof(stu_l2), "Class & Section: %s %s    |    Roll No: %d    |    Session: %s",
             p->class_name[0] ? p->class_name : "—",
             p->section_name[0] ? p->section_name : "",
             p->roll_number,
             settings->current_academic_year_name);

    draw_text_box(cr, "Segoe UI Bold 10", PANGO_ALIGN_LEFT,
                  0.09, 0.14, 0.21,
                  inner_x + 12.0, cur_y + 9.0, inner_w - 24.0, 20.0, TRUE, stu_l1);
    draw_text_box(cr, "Segoe UI 9.5", PANGO_ALIGN_LEFT,
                  0.28, 0.33, 0.41,
                  inner_x + 12.0, cur_y + 32.0, inner_w - 24.0, 20.0, TRUE, stu_l2);
    cur_y += 74.0;

    /* Fee Breakdown Table */
    draw_filled_rect(cr, inner_x, cur_y, inner_w, 26.0, 0.12, 0.25, 0.42, 1.0);
    draw_text_box(cr, "Segoe UI Bold 9.5", PANGO_ALIGN_LEFT,
                  1.0, 1.0, 1.0,
                  inner_x + 12.0, cur_y, inner_w * 0.65, 26.0, TRUE, "Particulars");
    draw_text_box(cr, "Segoe UI Bold 9.5", PANGO_ALIGN_RIGHT,
                  1.0, 1.0, 1.0,
                  inner_x + inner_w * 0.65, cur_y, inner_w * 0.35 - 12.0, 26.0, TRUE, "Amount / Details");
    cur_y += 26.0;

    const char *labels[5];
    char vals[5][160];

    labels[0] = "Fee Category / Structure";
    snprintf(vals[0], sizeof(vals[0]), "%s", p->fee_structure_name[0] ? p->fee_structure_name : "Standard Tuition");

    labels[1] = "Payment Mode & Reference";
    if (p->transaction_ref[0] != '\0') {
        snprintf(vals[1], sizeof(vals[1]), "%s  (Ref: %s)", p->payment_method, p->transaction_ref);
    } else {
        snprintf(vals[1], sizeof(vals[1]), "%s", p->payment_method[0] ? p->payment_method : "CASH");
    }

    labels[2] = "Base Fee Amount";
    snprintf(vals[2], sizeof(vals[2]), "%s%.2f", curr, p->amount_paid);

    labels[3] = "Concession / Discount";
    snprintf(vals[3], sizeof(vals[3]), "- %s%.2f", curr, p->discount);

    labels[4] = "Late Fine";
    snprintf(vals[4], sizeof(vals[4]), "+ %s%.2f", curr, p->late_fine);

    double tbl_top = cur_y;
    for (int i = 0; i < 5; i++) {
        if (i % 2 == 1) {
            draw_filled_rect(cr, inner_x, cur_y, inner_w, 26.0, 0.97, 0.98, 0.99, 1.0);
        }
        draw_text_box(cr, "Segoe UI 9.5", PANGO_ALIGN_LEFT,
                      0.18, 0.23, 0.31,
                      inner_x + 12.0, cur_y, inner_w * 0.55, 26.0, TRUE, labels[i]);
        draw_text_box(cr, "Segoe UI Bold 9.5", PANGO_ALIGN_RIGHT,
                      0.10, 0.14, 0.20,
                      inner_x + inner_w * 0.55, cur_y, inner_w * 0.45 - 12.0, 26.0, TRUE, vals[i]);
        draw_hline(cr, inner_x, inner_x + inner_w, cur_y + 26.0, 0.89, 0.91, 0.94, 0.6);
        cur_y += 26.0;
    }

    /* Highlighted Total Paid Row */
    draw_filled_rect(cr, inner_x, cur_y, inner_w, 32.0, 0.92, 0.96, 1.0, 1.0);
    draw_text_box(cr, "Segoe UI Bold 11", PANGO_ALIGN_LEFT,
                  0.06, 0.16, 0.29,
                  inner_x + 12.0, cur_y, inner_w * 0.55, 32.0, TRUE, "TOTAL AMOUNT PAID");

    char tot_str[64];
    snprintf(tot_str, sizeof(tot_str), "%s%.2f", curr, p->total_amount);
    draw_text_box(cr, "Segoe UI Bold 12.5", PANGO_ALIGN_RIGHT,
                  0.05, 0.45, 0.30,
                  inner_x + inner_w * 0.55, cur_y, inner_w * 0.45 - 12.0, 32.0, TRUE, tot_str);
    cur_y += 32.0;

    draw_stroked_rect(cr, inner_x, tbl_top - 26.0, inner_w, (cur_y - tbl_top) + 26.0,
                      0.78, 0.82, 0.88, 0.8);

    cur_y += 12.0;
    if (p->remarks[0] != '\0') {
        char rem_txt[320];
        snprintf(rem_txt, sizeof(rem_txt), "Remarks: %s", p->remarks);
        draw_text_box(cr, "Segoe UI Italic 9", PANGO_ALIGN_LEFT,
                      0.35, 0.40, 0.48,
                      inner_x, cur_y, inner_w, 20.0, TRUE, rem_txt);
        cur_y += 26.0;
    } else {
        cur_y += 14.0;
    }

    /* Sign-off & Verified Stamp */
    char auth_line[256];
    snprintf(auth_line, sizeof(auth_line), "Collected / Authorized By: %s",
             p->collected_by_name[0] ? p->collected_by_name : "Accounts Office");
    draw_text_box(cr, "Segoe UI 9.5", PANGO_ALIGN_LEFT,
                  0.25, 0.31, 0.39,
                  inner_x, cur_y + 12.0, inner_w * 0.62, 20.0, TRUE, auth_line);

    draw_filled_rect(cr, inner_x + inner_w - 140.0, cur_y + 6.0, 140.0, 26.0, 0.87, 0.97, 0.91, 1.0);
    draw_stroked_rect(cr, inner_x + inner_w - 140.0, cur_y + 6.0, 140.0, 26.0, 0.08, 0.58, 0.35, 0.9);
    draw_text_box(cr, "Segoe UI Bold 9", PANGO_ALIGN_CENTER,
                  0.06, 0.46, 0.27,
                  inner_x + inner_w - 140.0, cur_y + 6.0, 140.0, 26.0, TRUE,
                  "PAID & VERIFIED");

    char sig_y_note[128];
    snprintf(sig_y_note, sizeof(sig_y_note),
             "Authorized Signature & Seal   |   Printed %s", ts);
    draw_text_box(cr, "Segoe UI 8", PANGO_ALIGN_LEFT,
                  0.42, 0.47, 0.54,
                  inner_x, PAGE_MARGIN + box_h - 26.0, inner_w, 18.0, TRUE, sig_y_note);

    draw_page_footer(cr, page_w, page_h, 0, 1, "Computer-generated Official Fee Receipt");
}

/* ── Student Marksheet Rendering ──────────────────────────────────── */

static void draw_marksheet_page(cairo_t *cr, double page_w, double page_h,
                                const StudentReportCard *card, const SchoolSettings *settings)
{
    double usable_w = page_w - 2.0 * PAGE_MARGIN;
    double cur_y = PAGE_MARGIN;

    char ts[48];
    format_timestamp(ts, sizeof(ts));

    draw_filled_rect(cr, PAGE_MARGIN, cur_y, usable_w, 72.0, 0.06, 0.16, 0.29, 1.0);
    draw_text_box(cr, "Segoe UI Bold 15", PANGO_ALIGN_CENTER,
                  1.0, 1.0, 1.0,
                  PAGE_MARGIN + 12.0, cur_y + 9.0, usable_w - 24.0, 24.0, TRUE,
                  settings->school_name);

    char sub[256];
    snprintf(sub, sizeof(sub), "%s — Academic Session %s\nSTUDENT ACADEMIC PERFORMANCE REPORT CARD",
             card->exam_name[0] ? card->exam_name : "Examination",
             card->academic_year[0] ? card->academic_year : settings->current_academic_year_name);
    draw_text_box(cr, "Segoe UI 9", PANGO_ALIGN_CENTER,
                  0.85, 0.90, 0.98,
                  PAGE_MARGIN + 12.0, cur_y + 34.0, usable_w - 24.0, 32.0, FALSE, sub);
    cur_y += 84.0;

    /* Student Identity Box */
    draw_filled_rect(cr, PAGE_MARGIN, cur_y, usable_w, 56.0, 0.96, 0.97, 0.99, 1.0);
    draw_stroked_rect(cr, PAGE_MARGIN, cur_y, usable_w, 56.0, 0.80, 0.84, 0.90, 0.8);

    char l1[256], l2[256];
    snprintf(l1, sizeof(l1), "Student Name: %s    |    Admission No: %s",
             card->student_name, card->admission_no);
    snprintf(l2, sizeof(l2), "Class & Section: %s %s    |    Roll Number: %d    |    Class Rank: #%d",
             card->class_name, card->section_name, card->roll_number, card->class_rank);

    draw_text_box(cr, "Segoe UI Bold 10", PANGO_ALIGN_LEFT,
                  0.08, 0.13, 0.20,
                  PAGE_MARGIN + 12.0, cur_y + 8.0, usable_w - 24.0, 20.0, TRUE, l1);
    draw_text_box(cr, "Segoe UI 9.5", PANGO_ALIGN_LEFT,
                  0.27, 0.33, 0.41,
                  PAGE_MARGIN + 12.0, cur_y + 30.0, usable_w - 24.0, 20.0, TRUE, l2);
    cur_y += 68.0;

    /* Subject Scores Table */
    const char *cols[] = { "Code", "Subject Name", "Max", "Pass", "Obtained", "Grade", "Status" };
    double frac[] = { 0.13, 0.37, 0.09, 0.09, 0.12, 0.10, 0.10 };
    double col_w[7];
    for (int i = 0; i < 7; i++) col_w[i] = usable_w * frac[i];

    draw_filled_rect(cr, PAGE_MARGIN, cur_y, usable_w, 25.0, 0.10, 0.21, 0.36, 1.0);
    double cx = PAGE_MARGIN;
    for (int i = 0; i < 7; i++) {
        draw_text_box(cr, "Segoe UI Bold 9", (i >= 2) ? PANGO_ALIGN_CENTER : PANGO_ALIGN_LEFT,
                      1.0, 1.0, 1.0,
                      cx + 6.0, cur_y, col_w[i] - 12.0, 25.0, TRUE, cols[i]);
        cx += col_w[i];
    }
    cur_y += 25.0;

    double row_h = 22.0;
    double max_table_bottom = page_h - PAGE_MARGIN - 190.0;
    if (card->score_count > 0 && cur_y + card->score_count * row_h > max_table_bottom) {
        row_h = (max_table_bottom - cur_y) / (double)card->score_count;
        if (row_h < 14.0) row_h = 14.0;
    }

    double t_top = cur_y;
    for (int i = 0; i < card->score_count; i++) {
        const SubjectScore *sc = &card->scores[i];
        if (i % 2 == 1) {
            draw_filled_rect(cr, PAGE_MARGIN, cur_y, usable_w, row_h, 0.96, 0.97, 0.99, 1.0);
        }
        char mx[16], ps[16], ob[16];
        snprintf(mx, sizeof(mx), "%.0f", sc->max_marks);
        snprintf(ps, sizeof(ps), "%.0f", sc->passing_marks);
        snprintf(ob, sizeof(ob), "%.1f", sc->marks_obtained);
        const char *cells[7] = { sc->code, sc->subject_name, mx, ps, ob, sc->grade, sc->is_pass ? "PASS" : "FAIL" };

        cx = PAGE_MARGIN;
        for (int c = 0; c < 7; c++) {
            const char *font = (c == 1 || c == 4 || c == 5) ? "Segoe UI Bold 8.5" : "Segoe UI 8.5";
            double cr_col = (c == 6 && !sc->is_pass) ? 0.78 : (c == 6 ? 0.06 : 0.12);
            double cg_col = (c == 6 && sc->is_pass) ? 0.50 : 0.16;
            double cb_col = (c == 6 && sc->is_pass) ? 0.25 : 0.23;
            draw_text_box(cr, font, (c >= 2) ? PANGO_ALIGN_CENTER : PANGO_ALIGN_LEFT,
                          cr_col, cg_col, cb_col,
                          cx + 6.0, cur_y, col_w[c] - 12.0, row_h, TRUE, cells[c]);
            cx += col_w[c];
        }
        draw_hline(cr, PAGE_MARGIN, PAGE_MARGIN + usable_w, cur_y + row_h, 0.88, 0.90, 0.93, 0.5);
        cur_y += row_h;
    }
    draw_stroked_rect(cr, PAGE_MARGIN, t_top - 25.0, usable_w, (cur_y - t_top) + 25.0,
                      0.78, 0.82, 0.88, 0.8);

    cur_y += 14.0;

    /* Summary Card */
    draw_filled_rect(cr, PAGE_MARGIN, cur_y, usable_w, 54.0, 0.93, 0.96, 1.0, 1.0);
    draw_stroked_rect(cr, PAGE_MARGIN, cur_y, usable_w, 54.0, 0.72, 0.82, 0.96, 0.9);

    char s_txt[256];
    snprintf(s_txt, sizeof(s_txt),
             "Grand Total: %.1f / %.0f    |    Percentage: %.1f%%    |    Overall Grade: %s\n"
             "Final Result: %s    |    Class Rank: #%d",
             card->total_marks_obtained, card->total_max_marks, card->percentage, card->overall_grade,
             card->is_pass ? "PASSED & PROMOTED" : "NEEDS IMPROVEMENT", card->class_rank);
    draw_text_box(cr, "Segoe UI Bold 10", PANGO_ALIGN_LEFT,
                  0.06, 0.16, 0.29,
                  PAGE_MARGIN + 14.0, cur_y + 8.0, usable_w - 28.0, 38.0, FALSE, s_txt);

    double sig_y = page_h - PAGE_MARGIN - 64.0;
    draw_text_box(cr, "Segoe UI 9", PANGO_ALIGN_CENTER,
                  0.22, 0.28, 0.36,
                  PAGE_MARGIN + 20.0, sig_y, 180.0, 34.0, FALSE,
                  "______________________\nClass Teacher Signature");
    draw_text_box(cr, "Segoe UI 9", PANGO_ALIGN_CENTER,
                  0.22, 0.28, 0.36,
                  page_w - PAGE_MARGIN - 200.0, sig_y, 180.0, 34.0, FALSE,
                  "______________________\nPrincipal Signature & Seal");

    char footer_note[256];
    snprintf(footer_note, sizeof(footer_note), "%s  •  Generated %s", settings->school_name, ts);
    draw_page_footer(cr, page_w, page_h, 0, 1, footer_note);
}

/* ── Headless Cairo PDF Export Implementations ────────────────────── */

bool print_pdf_export_report(const ReportData *data, const char *filepath)
{
    if (!data || !filepath || filepath[0] == '\0') return false;

    FILE *fp = g_fopen(filepath, "wb");
    if (!fp) return false;

    bool landscape = (data->column_count > 6);
    double pw = landscape ? A4_LANDSCAPE_W : A4_PORTRAIT_W;
    double ph = landscape ? A4_LANDSCAPE_H : A4_PORTRAIT_H;

    cairo_surface_t *surface = cairo_pdf_surface_create_for_stream(stdio_cairo_write, fp, pw, ph);
    if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) {
        cairo_surface_destroy(surface);
        fclose(fp);
        return false;
    }

    cairo_t *cr = cairo_create(surface);
    SchoolSettings settings;
    load_brand_settings(&settings);

    ReportLayout rl = compute_report_layout(data, ph);
    for (int p = 0; p < rl.total_pages; p++) {
        draw_report_page(cr, pw, ph, data, &settings, p);
        cairo_show_page(cr);
    }

    cairo_status_t st = cairo_surface_status(surface);
    cairo_destroy(cr);
    cairo_surface_finish(surface);
    if (st == CAIRO_STATUS_SUCCESS) {
        st = cairo_surface_status(surface);
    }
    cairo_surface_destroy(surface);
    fclose(fp);

    if (st == CAIRO_STATUS_SUCCESS) {
        char details[256];
        snprintf(details, sizeof(details), "Exported %s (%d rows) to PDF: %s",
                 data->title[0] ? data->title : "Report", data->row_count, filepath);
        db_log_activity(0, "EXPORT_REPORT_PDF", "REPORTS", details);
        return true;
    }
    return false;
}

bool print_pdf_export_receipt(const FeePayment *payment, const char *filepath)
{
    if (!payment || !filepath || filepath[0] == '\0') return false;

    FILE *fp = g_fopen(filepath, "wb");
    if (!fp) return false;

    cairo_surface_t *surface = cairo_pdf_surface_create_for_stream(
        stdio_cairo_write, fp, A4_PORTRAIT_W, A4_PORTRAIT_H);
    if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) {
        cairo_surface_destroy(surface);
        fclose(fp);
        return false;
    }

    cairo_t *cr = cairo_create(surface);
    SchoolSettings settings;
    load_brand_settings(&settings);

    draw_receipt_page(cr, A4_PORTRAIT_W, A4_PORTRAIT_H, payment, &settings);
    cairo_show_page(cr);

    cairo_destroy(cr);
    cairo_surface_finish(surface);
    cairo_status_t st = cairo_surface_status(surface);
    cairo_surface_destroy(surface);
    fclose(fp);

    if (st == CAIRO_STATUS_SUCCESS) {
        char details[256];
        snprintf(details, sizeof(details), "Downloaded PDF receipt %s: %s",
                 payment->receipt_no, filepath);
        db_log_activity(0, "EXPORT_RECEIPT_PDF", "FEES", details);
        return true;
    }
    return false;
}

bool print_pdf_export_marksheet(const StudentReportCard *card, const char *filepath)
{
    if (!card || !filepath || filepath[0] == '\0') return false;

    FILE *fp = g_fopen(filepath, "wb");
    if (!fp) return false;

    cairo_surface_t *surface = cairo_pdf_surface_create_for_stream(
        stdio_cairo_write, fp, A4_PORTRAIT_W, A4_PORTRAIT_H);
    if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) {
        cairo_surface_destroy(surface);
        fclose(fp);
        return false;
    }

    cairo_t *cr = cairo_create(surface);
    SchoolSettings settings;
    load_brand_settings(&settings);

    draw_marksheet_page(cr, A4_PORTRAIT_W, A4_PORTRAIT_H, card, &settings);
    cairo_show_page(cr);

    cairo_destroy(cr);
    cairo_surface_finish(surface);
    cairo_status_t st = cairo_surface_status(surface);
    cairo_surface_destroy(surface);
    fclose(fp);

    if (st == CAIRO_STATUS_SUCCESS) {
        char details[256];
        snprintf(details, sizeof(details), "Exported marksheet PDF for %s (%s): %s",
                 card->student_name, card->admission_no, filepath);
        db_log_activity(0, "EXPORT_MARKSHEET_PDF", "EXAMS", details);
        return true;
    }
    return false;
}

/* ── GTK4 Native Print Operation Engine ───────────────────────────── */

typedef enum {
    PRINT_KIND_REPORT = 0,
    PRINT_KIND_RECEIPT,
    PRINT_KIND_MARKSHEET
} PrintDocKind;

typedef struct {
    PrintDocKind kind;
    ReportData report;
    FeePayment receipt;
    StudentReportCard marksheet;
    SchoolSettings settings;
} PrintJobData;

static void on_print_begin(GtkPrintOperation *op, GtkPrintContext *ctx, gpointer user_data)
{
    PrintJobData *job = (PrintJobData *)user_data;
    if (job->kind == PRINT_KIND_REPORT) {
        double h = gtk_print_context_get_height(ctx);
        ReportLayout rl = compute_report_layout(&job->report, h);
        gtk_print_operation_set_n_pages(op, rl.total_pages);
    } else {
        gtk_print_operation_set_n_pages(op, 1);
    }
}

static void on_print_draw_page(GtkPrintOperation *op, GtkPrintContext *ctx, int page_nr, gpointer user_data)
{
    (void)op;
    PrintJobData *job = (PrintJobData *)user_data;
    cairo_t *cr = gtk_print_context_get_cairo_context(ctx);
    double w = gtk_print_context_get_width(ctx);
    double h = gtk_print_context_get_height(ctx);

    if (job->kind == PRINT_KIND_REPORT) {
        draw_report_page(cr, w, h, &job->report, &job->settings, page_nr);
    } else if (job->kind == PRINT_KIND_RECEIPT) {
        draw_receipt_page(cr, w, h, &job->receipt, &job->settings);
    } else {
        draw_marksheet_page(cr, w, h, &job->marksheet, &job->settings);
    }
}

static GtkWindow *resolve_parent_window(GtkWidget *anchor)
{
    if (!anchor) return NULL;
    GtkRoot *root = gtk_widget_get_root(anchor);
    return (root && GTK_IS_WINDOW(root)) ? GTK_WINDOW(root) : NULL;
}

static PrintPdfStatus run_print_job(GtkWidget *anchor_widget,
                                    PrintJobData *job,
                                    const char *job_name,
                                    bool landscape,
                                    const char *audit_action,
                                    const char *audit_module,
                                    char *err_buf,
                                    size_t err_size)
{
    if (err_buf && err_size > 0) err_buf[0] = '\0';

    GtkPrintOperation *op = gtk_print_operation_new();
    gtk_print_operation_set_job_name(op, job_name);
    gtk_print_operation_set_unit(op, GTK_UNIT_POINTS);
    gtk_print_operation_set_use_full_page(op, TRUE);

    GtkPageSetup *setup = gtk_page_setup_new();
    GtkPaperSize *a4 = gtk_paper_size_new(GTK_PAPER_NAME_A4);
    gtk_page_setup_set_paper_size_and_default_margins(setup, a4);
    gtk_page_setup_set_orientation(
        setup, landscape ? GTK_PAGE_ORIENTATION_LANDSCAPE : GTK_PAGE_ORIENTATION_PORTRAIT);
    gtk_print_operation_set_default_page_setup(op, setup);
    gtk_paper_size_free(a4);
    g_object_unref(setup);

    g_signal_connect(op, "begin-print", G_CALLBACK(on_print_begin), job);
    g_signal_connect(op, "draw-page", G_CALLBACK(on_print_draw_page), job);

    GError *err = NULL;
    GtkPrintOperationResult res = gtk_print_operation_run(
        op,
        GTK_PRINT_OPERATION_ACTION_PRINT_DIALOG,
        resolve_parent_window(anchor_widget),
        &err);

    g_object_unref(op);

    if (res == GTK_PRINT_OPERATION_RESULT_ERROR) {
        if (err_buf && err_size > 0) {
            snprintf(err_buf, err_size, "%s", (err && err->message) ? err->message : "Print operation failed.");
        }
        if (err) g_error_free(err);
        return PRINT_PDF_ERROR;
    }
    if (err) g_error_free(err);

    if (res == GTK_PRINT_OPERATION_RESULT_CANCEL) {
        return PRINT_PDF_CANCELLED;
    }

    db_log_activity(0, audit_action, audit_module, job_name);
    return PRINT_PDF_OK;
}

PrintPdfStatus print_pdf_print_report(GtkWidget *anchor_widget,
                                      const ReportData *data,
                                      char *err_buf,
                                      size_t err_size)
{
    if (!data) {
        if (err_buf && err_size > 0) snprintf(err_buf, err_size, "No report dataset to print.");
        return PRINT_PDF_ERROR;
    }

    PrintJobData *job = g_new0(PrintJobData, 1);
    job->kind = PRINT_KIND_REPORT;
    job->report = *data;
    load_brand_settings(&job->settings);

    char name[160];
    snprintf(name, sizeof(name), "APCOACHING - %s", data->title[0] ? data->title : "Report");
    PrintPdfStatus st = run_print_job(anchor_widget, job, name, (data->column_count > 6),
                                      "PRINT_REPORT", "REPORTS", err_buf, err_size);
    g_free(job);
    return st;
}

PrintPdfStatus print_pdf_print_receipt(GtkWidget *anchor_widget,
                                       const FeePayment *payment,
                                       char *err_buf,
                                       size_t err_size)
{
    if (!payment) {
        if (err_buf && err_size > 0) snprintf(err_buf, err_size, "No receipt record to print.");
        return PRINT_PDF_ERROR;
    }

    PrintJobData *job = g_new0(PrintJobData, 1);
    job->kind = PRINT_KIND_RECEIPT;
    job->receipt = *payment;
    load_brand_settings(&job->settings);

    char name[128];
    snprintf(name, sizeof(name), "APCOACHING Receipt %s", payment->receipt_no);
    PrintPdfStatus st = run_print_job(anchor_widget, job, name, false,
                                      "PRINT_RECEIPT", "FEES", err_buf, err_size);
    g_free(job);
    return st;
}

PrintPdfStatus print_pdf_print_marksheet(GtkWidget *anchor_widget,
                                         const StudentReportCard *card,
                                         char *err_buf,
                                         size_t err_size)
{
    if (!card) {
        if (err_buf && err_size > 0) snprintf(err_buf, err_size, "No marksheet data to print.");
        return PRINT_PDF_ERROR;
    }

    PrintJobData *job = g_new0(PrintJobData, 1);
    job->kind = PRINT_KIND_MARKSHEET;
    job->marksheet = *card;
    load_brand_settings(&job->settings);

    char name[160];
    snprintf(name, sizeof(name), "APCOACHING Marksheet %s", card->admission_no);
    PrintPdfStatus st = run_print_job(anchor_widget, job, name, false,
                                      "PRINT_MARKSHEET", "EXAMS", err_buf, err_size);
    g_free(job);
    return st;
}

/* ── GTK4 Save As PDF File Picker Helpers ─────────────────────────── */

typedef struct {
    PrintDocKind kind;
    ReportData report;
    FeePayment receipt;
    StudentReportCard marksheet;
    PrintPdfResultCallback cb;
    gpointer user_data;
} PdfSaveAsyncCtx;

static void ensure_pdf_extension(const char *raw, char *out, size_t out_sz)
{
    size_t len = strlen(raw);
    if (len >= 4 && g_ascii_strcasecmp(raw + len - 4, ".pdf") == 0) {
        snprintf(out, out_sz, "%s", raw);
    } else {
        snprintf(out, out_sz, "%s.pdf", raw);
    }
}

static void on_file_dialog_save_done(GObject *source, GAsyncResult *res, gpointer user_data)
{
    PdfSaveAsyncCtx *ctx = (PdfSaveAsyncCtx *)user_data;
    GError *err = NULL;
    GFile *file = gtk_file_dialog_save_finish(GTK_FILE_DIALOG(source), res, &err);

    if (!file) {
        gboolean cancelled = (err && g_error_matches(err, GTK_DIALOG_ERROR, GTK_DIALOG_ERROR_DISMISSED));
        if (ctx->cb) {
            if (cancelled) {
                ctx->cb(PRINT_PDF_CANCELLED, NULL, ctx->user_data);
            } else {
                ctx->cb(PRINT_PDF_ERROR,
                        (err && err->message) ? err->message : "PDF save canceled or failed.",
                        ctx->user_data);
            }
        }
        if (err) g_error_free(err);
        g_free(ctx);
        return;
    }

    char *raw_path = g_file_get_path(file);
    g_object_unref(file);
    if (!raw_path) {
        if (ctx->cb) ctx->cb(PRINT_PDF_ERROR, "Invalid destination file path.", ctx->user_data);
        g_free(ctx);
        return;
    }

    char final_path[512];
    ensure_pdf_extension(raw_path, final_path, sizeof(final_path));
    g_free(raw_path);

    bool ok = false;
    if (ctx->kind == PRINT_KIND_REPORT) {
        ok = print_pdf_export_report(&ctx->report, final_path);
    } else if (ctx->kind == PRINT_KIND_RECEIPT) {
        ok = print_pdf_export_receipt(&ctx->receipt, final_path);
    } else {
        ok = print_pdf_export_marksheet(&ctx->marksheet, final_path);
    }

    if (ctx->cb) {
        if (ok) {
            ctx->cb(PRINT_PDF_OK, final_path, ctx->user_data);
        } else {
            ctx->cb(PRINT_PDF_ERROR, "Could not write PDF file to selected destination.", ctx->user_data);
        }
    }
    g_free(ctx);
}

static void launch_pdf_save_dialog(GtkWidget *anchor_widget,
                                   const char *dialog_title,
                                   const char *initial_name,
                                   PdfSaveAsyncCtx *ctx)
{
    g_mkdir_with_parents("exports", 0755);

    GtkFileDialog *dlg = gtk_file_dialog_new();
    gtk_file_dialog_set_title(dlg, dialog_title);
    gtk_file_dialog_set_modal(dlg, TRUE);
    gtk_file_dialog_set_initial_name(dlg, initial_name);

    GFile *exp_dir = g_file_new_for_path("exports");
    if (exp_dir) {
        gtk_file_dialog_set_initial_folder(dlg, exp_dir);
        g_object_unref(exp_dir);
    }

    GtkFileFilter *pdf_filter = gtk_file_filter_new();
    gtk_file_filter_set_name(pdf_filter, "PDF Documents (*.pdf)");
    gtk_file_filter_add_pattern(pdf_filter, "*.pdf");
    gtk_file_filter_add_mime_type(pdf_filter, "application/pdf");

    GListStore *filters = g_list_store_new(GTK_TYPE_FILE_FILTER);
    g_list_store_append(filters, pdf_filter);
    gtk_file_dialog_set_filters(dlg, G_LIST_MODEL(filters));
    gtk_file_dialog_set_default_filter(dlg, pdf_filter);
    g_object_unref(pdf_filter);
    g_object_unref(filters);

    gtk_file_dialog_save(dlg, resolve_parent_window(anchor_widget), NULL, on_file_dialog_save_done, ctx);
    g_object_unref(dlg);
}

void print_pdf_prompt_export_report(GtkWidget *anchor_widget,
                                    const ReportData *data,
                                    PrintPdfResultCallback cb,
                                    gpointer user_data)
{
    if (!data) {
        if (cb) cb(PRINT_PDF_ERROR, "No report dataset to export.", user_data);
        return;
    }

    PdfSaveAsyncCtx *ctx = g_new0(PdfSaveAsyncCtx, 1);
    ctx->kind = PRINT_KIND_REPORT;
    ctx->report = *data;
    ctx->cb = cb;
    ctx->user_data = user_data;

    char ts[32], fname[128];
    format_date_slug(ts, sizeof(ts));
    snprintf(fname, sizeof(fname), "SchoolCMS_Report_%s.pdf", ts);
    launch_pdf_save_dialog(anchor_widget, "Download Report as PDF", fname, ctx);
}

void print_pdf_prompt_export_receipt(GtkWidget *anchor_widget,
                                     const FeePayment *payment,
                                     PrintPdfResultCallback cb,
                                     gpointer user_data)
{
    if (!payment) {
        if (cb) cb(PRINT_PDF_ERROR, "No fee receipt record to export.", user_data);
        return;
    }

    PdfSaveAsyncCtx *ctx = g_new0(PdfSaveAsyncCtx, 1);
    ctx->kind = PRINT_KIND_RECEIPT;
    ctx->receipt = *payment;
    ctx->cb = cb;
    ctx->user_data = user_data;

    char fname[128];
    snprintf(fname, sizeof(fname), "Fee_Receipt_%s.pdf",
             payment->receipt_no[0] ? payment->receipt_no : "Payment");
    launch_pdf_save_dialog(anchor_widget, "Download Fee Receipt as PDF", fname, ctx);
}

void print_pdf_prompt_export_marksheet(GtkWidget *anchor_widget,
                                       const StudentReportCard *card,
                                       PrintPdfResultCallback cb,
                                       gpointer user_data)
{
    if (!card) {
        if (cb) cb(PRINT_PDF_ERROR, "No marksheet data to export.", user_data);
        return;
    }

    PdfSaveAsyncCtx *ctx = g_new0(PdfSaveAsyncCtx, 1);
    ctx->kind = PRINT_KIND_MARKSHEET;
    ctx->marksheet = *card;
    ctx->cb = cb;
    ctx->user_data = user_data;

    char fname[160];
    snprintf(fname, sizeof(fname), "Report_Card_%s.pdf",
             card->admission_no[0] ? card->admission_no : "Student");
    launch_pdf_save_dialog(anchor_widget, "Download Report Card as PDF", fname, ctx);
}
