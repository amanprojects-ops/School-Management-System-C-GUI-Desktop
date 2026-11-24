/**
 * @file reports_view.c
 * @brief Reports & Data Export GUI implementation with live preview and CSV export.
 */

#include "ui/reports_view.h"
#include "reports.h"
#include "classes.h"
#include "auth/auth.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>
#define MKDIR(dir) _mkdir(dir)
#else
#define MKDIR(dir) mkdir(dir, 0755)
#endif

typedef struct {
    GtkWidget *root_box;
    GtkWidget *cat_box;
    GtkWidget *filter_box;
    GtkWidget *preview_box;
    GtkWidget *summary_box;
    GtkWidget *summary_label;
    GtkWidget *status_banner;

    /* Category Buttons */
    GtkWidget *cat_buttons[REPORT_TYPE_COUNT];
    ReportType active_category;

    /* Filter dropdowns & inputs */
    GtkWidget *combo_class;
    GtkWidget *combo_section;
    GtkWidget *combo_status;
    GtkWidget *entry_month;

    SchoolClass classes[MAX_CLASSES];
    int class_count;
    ClassSection sections[MAX_SECTIONS];
    int section_count;

    int selected_class_id;
    int selected_section_id;
    char selected_status[32];
    char selected_month[32];

    ReportData current_data;
} ReportsWidgets;

static void refresh_report(ReportsWidgets *rw);
static void update_sections_filter(ReportsWidgets *rw);

/* ── Category Switching ───────────────────────────────────────────── */

static const char *CAT_TITLES[REPORT_TYPE_COUNT] = {
    "Student Directory",
    "Attendance Summary",
    "Fee Collections",
    "Pending Fee Dues",
    "Faculty Directory",
    "Exam Marks & Results"
};

static void on_category_clicked(GtkButton *btn, gpointer user_data)
{
    ReportsWidgets *rw = (ReportsWidgets *)user_data;
    ReportType cat = (ReportType)GPOINTER_TO_INT(g_object_get_data(G_OBJECT(btn), "cat_index"));

    rw->active_category = cat;
    for (int i = 0; i < REPORT_TYPE_COUNT; i++) {
        if (i == (int)cat) {
            gtk_widget_add_css_class(rw->cat_buttons[i], "tab-btn-active");
        } else {
            gtk_widget_remove_css_class(rw->cat_buttons[i], "tab-btn-active");
        }
    }

    /* Hide export status banner on category change */
    gtk_widget_set_visible(rw->status_banner, FALSE);
    refresh_report(rw);
}

/* ── Filter Callbacks ─────────────────────────────────────────────── */

static void on_class_filter_changed(GtkDropDown *dropdown, GParamSpec *pspec, gpointer user_data)
{
    (void)pspec;
    ReportsWidgets *rw = (ReportsWidgets *)user_data;
    guint sel = gtk_drop_down_get_selected(dropdown);
    if (sel == 0) {
        rw->selected_class_id = 0;
    } else if ((int)sel - 1 < rw->class_count) {
        rw->selected_class_id = rw->classes[sel - 1].id;
    }
    update_sections_filter(rw);
    refresh_report(rw);
}

static void on_section_filter_changed(GtkDropDown *dropdown, GParamSpec *pspec, gpointer user_data)
{
    (void)pspec;
    ReportsWidgets *rw = (ReportsWidgets *)user_data;
    guint sel = gtk_drop_down_get_selected(dropdown);
    if (sel == 0) {
        rw->selected_section_id = 0;
    } else if ((int)sel - 1 < rw->section_count) {
        rw->selected_section_id = rw->sections[sel - 1].id;
    }
    refresh_report(rw);
}

static void on_status_filter_changed(GtkDropDown *dropdown, GParamSpec *pspec, gpointer user_data)
{
    (void)pspec;
    ReportsWidgets *rw = (ReportsWidgets *)user_data;
    guint sel = gtk_drop_down_get_selected(dropdown);
    if (sel == 1) {
        strncpy(rw->selected_status, "ACTIVE", sizeof(rw->selected_status) - 1);
    } else if (sel == 2) {
        strncpy(rw->selected_status, "INACTIVE", sizeof(rw->selected_status) - 1);
    } else {
        rw->selected_status[0] = '\0';
    }
    refresh_report(rw);
}

static void update_sections_filter(ReportsWidgets *rw)
{
    if (rw->selected_class_id <= 0) {
        const char *none[] = { "All Sections", NULL };
        GtkWidget *new_dd = gtk_drop_down_new_from_strings(none);
        GtkWidget *parent = gtk_widget_get_parent(rw->combo_section);
        gtk_box_remove(GTK_BOX(parent), rw->combo_section);
        gtk_box_append(GTK_BOX(parent), new_dd);
        rw->combo_section = new_dd;
        rw->section_count = 0;
        rw->selected_section_id = 0;
        return;
    }

    rw->section_count = 0;
    section_service_get_by_class(rw->selected_class_id, rw->sections, MAX_SECTIONS, &rw->section_count);

    const char *sec_names[MAX_SECTIONS + 2];
    sec_names[0] = "All Sections";
    for (int i = 0; i < rw->section_count; i++) {
        sec_names[i + 1] = rw->sections[i].name;
    }
    sec_names[rw->section_count + 1] = NULL;

    GtkWidget *parent = gtk_widget_get_parent(rw->combo_section);
    GtkWidget *new_dd = gtk_drop_down_new_from_strings(sec_names);
    g_signal_connect(new_dd, "notify::selected", G_CALLBACK(on_section_filter_changed), rw);

    gtk_box_remove(GTK_BOX(parent), rw->combo_section);
    gtk_box_append(GTK_BOX(parent), new_dd);
    rw->combo_section = new_dd;
    rw->selected_section_id = 0;
}

/* ── CSV Export Handler ───────────────────────────────────────────── */

static void on_export_csv_clicked(GtkButton *btn, gpointer user_data)
{
    (void)btn;
    ReportsWidgets *rw = (ReportsWidgets *)user_data;

    MKDIR("exports");

    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char time_str[32];
    strftime(time_str, sizeof(time_str), "%Y%m%d_%H%M%S", t);

    const char *cat_slugs[REPORT_TYPE_COUNT] = {
        "students", "attendance", "fee_collection", "pending_dues", "teachers", "exam_results"
    };

    char out_path[256];
    snprintf(out_path, sizeof(out_path), "exports/SchoolCMS_%s_%s.csv",
             cat_slugs[rw->active_category], time_str);

    bool ok = report_service_export_csv(&rw->current_data, out_path);
    if (ok) {
        char msg[512];
        snprintf(msg, sizeof(msg), "Successfully exported %d records to CSV: %s",
                 rw->current_data.row_count, out_path);
        gtk_label_set_text(GTK_LABEL(rw->status_banner), msg);
        gtk_widget_remove_css_class(rw->status_banner, "card-kpi-red");
        gtk_widget_add_css_class(rw->status_banner, "card-kpi-green");
        gtk_widget_set_visible(rw->status_banner, TRUE);
    } else {
        gtk_label_set_text(GTK_LABEL(rw->status_banner), "Failed to export CSV file.");
        gtk_widget_remove_css_class(rw->status_banner, "card-kpi-green");
        gtk_widget_add_css_class(rw->status_banner, "card-kpi-red");
        gtk_widget_set_visible(rw->status_banner, TRUE);
    }
}

/* ── Report Generation and Table Preview ──────────────────────────── */

static void refresh_report(ReportsWidgets *rw)
{
    /* Clear table preview */
    GtkWidget *child = gtk_widget_get_first_child(rw->preview_box);
    while (child) {
        GtkWidget *next = gtk_widget_get_next_sibling(child);
        gtk_box_remove(GTK_BOX(rw->preview_box), child);
        child = next;
    }

    ReportFilter f;
    memset(&f, 0, sizeof(ReportFilter));
    f.type = rw->active_category;
    f.class_id = rw->selected_class_id;
    f.section_id = rw->selected_section_id;
    strncpy(f.status_filter, rw->selected_status, sizeof(f.status_filter) - 1);
    if (rw->entry_month) {
        const char *txt = gtk_editable_get_text(GTK_EDITABLE(rw->entry_month));
        if (txt) strncpy(f.month_str, txt, sizeof(f.month_str) - 1);
    }

    bool ok = report_service_generate(&f, &rw->current_data);
    if (!ok) {
        GtkWidget *err = gtk_label_new("Error loading report dataset from database.");
        gtk_widget_add_css_class(err, "dim-label");
        gtk_box_append(GTK_BOX(rw->preview_box), err);
        return;
    }

    /* Update summary */
    gtk_label_set_text(GTK_LABEL(rw->summary_label), rw->current_data.summary_text);

    /* Render Header */
    GtkWidget *card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_add_css_class(card, "card");
    gtk_box_append(GTK_BOX(rw->preview_box), card);

    GtkWidget *h_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_add_css_class(h_row, "table-header-row");

    for (int col = 0; col < rw->current_data.column_count; col++) {
        GtkWidget *th = gtk_label_new(rw->current_data.header[col]);
        gtk_widget_add_css_class(th, "table-th");
        gtk_label_set_xalign(GTK_LABEL(th), 0.0f);
        gtk_widget_set_size_request(th, 100, -1);
        if (col == 1) gtk_widget_set_hexpand(th, TRUE);
        gtk_box_append(GTK_BOX(h_row), th);
    }
    gtk_box_append(GTK_BOX(card), h_row);

    if (rw->current_data.row_count == 0) {
        GtkWidget *empty_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
        gtk_widget_set_margin_top(empty_box, 30);
        gtk_widget_set_margin_bottom(empty_box, 30);
        GtkWidget *lbl = gtk_label_new("No data matching current filter criteria.");
        gtk_widget_add_css_class(lbl, "dim-label");
        gtk_box_append(GTK_BOX(empty_box), lbl);
        gtk_box_append(GTK_BOX(card), empty_box);
        return;
    }

    /* Data Rows in Scrollable Container */
    GtkWidget *scrolled = gtk_scrolled_window_new();
    gtk_scrolled_window_set_min_content_height(GTK_SCROLLED_WINDOW(scrolled), 180);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

    GtkWidget *rows_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

    for (int r = 0; r < rw->current_data.row_count; r++) {
        GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
        gtk_widget_add_css_class(row, "table-data-row");

        for (int col = 0; col < rw->current_data.column_count; col++) {
            GtkWidget *cell = gtk_label_new(rw->current_data.rows[r][col]);
            gtk_label_set_xalign(GTK_LABEL(cell), 0.0f);
            gtk_widget_set_size_request(cell, 100, -1);
            if (col == 0) gtk_widget_add_css_class(cell, "badge-adm");
            if (col == 1) {
                gtk_widget_add_css_class(cell, "table-cell-bold");
                gtk_widget_set_hexpand(cell, TRUE);
            }
            gtk_box_append(GTK_BOX(row), cell);
        }
        gtk_box_append(GTK_BOX(rows_box), row);
    }

    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled), rows_box);
    gtk_box_append(GTK_BOX(card), scrolled);
}

static void on_month_changed(GtkEditable *editable, gpointer user_data)
{
    (void)editable;
    ReportsWidgets *rw = (ReportsWidgets *)user_data;
    refresh_report(rw);
}

/* ── Constructor ─────────────────────────────────────────────────── */

GtkWidget *school_reports_view_new(void)
{
    ReportsWidgets *rw = g_new0(ReportsWidgets, 1);

    rw->root_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 16);
    gtk_widget_set_margin_start(rw->root_box, 24);
    gtk_widget_set_margin_end(rw->root_box, 24);
    gtk_widget_set_margin_top(rw->root_box, 20);
    gtk_widget_set_margin_bottom(rw->root_box, 24);

    g_object_set_data(G_OBJECT(rw->root_box), "reports_state", rw);

    /* ── Header Bar ─────────────────────────────────────────────── */
    GtkWidget *hdr = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_set_valign(hdr, GTK_ALIGN_CENTER);

    GtkWidget *title_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    GtkWidget *title = gtk_label_new("Reports & Data Export");
    gtk_widget_add_css_class(title, "page-title");
    gtk_label_set_xalign(GTK_LABEL(title), 0.0f);
    gtk_box_append(GTK_BOX(title_box), title);

    GtkWidget *sub = gtk_label_new("Generate and export institutional records, registers, collections, and merit lists to CSV.");
    gtk_widget_add_css_class(sub, "page-subtitle");
    gtk_label_set_xalign(GTK_LABEL(sub), 0.0f);
    gtk_box_append(GTK_BOX(title_box), sub);
    gtk_box_append(GTK_BOX(hdr), title_box);

    GtkWidget *spacer = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_hexpand(spacer, TRUE);
    gtk_box_append(GTK_BOX(hdr), spacer);

    GtkWidget *export_btn = gtk_button_new_with_label("Export to CSV");
    gtk_widget_add_css_class(export_btn, "btn-primary");
    g_signal_connect(export_btn, "clicked", G_CALLBACK(on_export_csv_clicked), rw);
    gtk_box_append(GTK_BOX(hdr), export_btn);

    gtk_box_append(GTK_BOX(rw->root_box), hdr);

    /* ── Status Banner ──────────────────────────────────────────── */
    rw->status_banner = gtk_label_new("");
    gtk_widget_add_css_class(rw->status_banner, "card");
    gtk_widget_set_margin_top(rw->status_banner, 4);
    gtk_widget_set_visible(rw->status_banner, FALSE);
    gtk_box_append(GTK_BOX(rw->root_box), rw->status_banner);

    /* ── Category Tabs Bar ──────────────────────────────────────── */
    rw->cat_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_add_css_class(rw->cat_box, "segmented-bar");

    for (int i = 0; i < REPORT_TYPE_COUNT; i++) {
        GtkWidget *btn = gtk_button_new_with_label(CAT_TITLES[i]);
        gtk_widget_add_css_class(btn, "tab-btn");
        g_object_set_data(G_OBJECT(btn), "cat_index", GINT_TO_POINTER(i));
        g_signal_connect(btn, "clicked", G_CALLBACK(on_category_clicked), rw);
        if (i == 0) gtk_widget_add_css_class(btn, "tab-btn-active");
        rw->cat_buttons[i] = btn;
        gtk_box_append(GTK_BOX(rw->cat_box), btn);
    }
    gtk_box_append(GTK_BOX(rw->root_box), rw->cat_box);

    /* ── Filter Controls Card ───────────────────────────────────── */
    rw->filter_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 16);
    gtk_widget_add_css_class(rw->filter_box, "card");
    gtk_widget_set_valign(rw->filter_box, GTK_ALIGN_CENTER);

    /* Class Filter */
    GtkWidget *b_c = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    GtkWidget *lbl_c = gtk_label_new("Class:");
    gtk_label_set_xalign(GTK_LABEL(lbl_c), 0.0f);
    gtk_box_append(GTK_BOX(b_c), lbl_c);

    rw->class_count = 0;
    class_service_get_all(rw->classes, MAX_CLASSES, &rw->class_count);

    const char *c_names[MAX_CLASSES + 2];
    c_names[0] = "All Classes";
    for (int i = 0; i < rw->class_count; i++) {
        c_names[i + 1] = rw->classes[i].name;
    }
    c_names[rw->class_count + 1] = NULL;

    rw->combo_class = gtk_drop_down_new_from_strings(c_names);
    g_signal_connect(rw->combo_class, "notify::selected", G_CALLBACK(on_class_filter_changed), rw);
    gtk_box_append(GTK_BOX(b_c), rw->combo_class);
    gtk_box_append(GTK_BOX(rw->filter_box), b_c);

    /* Section Filter */
    GtkWidget *b_s = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    GtkWidget *lbl_s = gtk_label_new("Section:");
    gtk_label_set_xalign(GTK_LABEL(lbl_s), 0.0f);
    gtk_box_append(GTK_BOX(b_s), lbl_s);

    const char *init_sec[] = { "All Sections", NULL };
    rw->combo_section = gtk_drop_down_new_from_strings(init_sec);
    gtk_box_append(GTK_BOX(b_s), rw->combo_section);
    gtk_box_append(GTK_BOX(rw->filter_box), b_s);

    /* Status Filter */
    GtkWidget *b_st = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    GtkWidget *lbl_st = gtk_label_new("Status:");
    gtk_label_set_xalign(GTK_LABEL(lbl_st), 0.0f);
    gtk_box_append(GTK_BOX(b_st), lbl_st);

    const char *st_opts[] = { "All Statuses", "ACTIVE", "INACTIVE", NULL };
    rw->combo_status = gtk_drop_down_new_from_strings(st_opts);
    g_signal_connect(rw->combo_status, "notify::selected", G_CALLBACK(on_status_filter_changed), rw);
    gtk_box_append(GTK_BOX(b_st), rw->combo_status);
    gtk_box_append(GTK_BOX(rw->filter_box), b_st);

    /* Month Filter (for Attendance) */
    GtkWidget *b_m = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    GtkWidget *lbl_m = gtk_label_new("Month (YYYY-MM):");
    gtk_label_set_xalign(GTK_LABEL(lbl_m), 0.0f);
    gtk_box_append(GTK_BOX(b_m), lbl_m);

    rw->entry_month = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(rw->entry_month), "e.g. 2026-10");
    g_signal_connect(rw->entry_month, "changed", G_CALLBACK(on_month_changed), rw);
    gtk_box_append(GTK_BOX(b_m), rw->entry_month);
    gtk_box_append(GTK_BOX(rw->filter_box), b_m);

    gtk_box_append(GTK_BOX(rw->root_box), rw->filter_box);

    /* ── Summary & Stats Label ──────────────────────────────────── */
    rw->summary_label = gtk_label_new("Ready");
    gtk_widget_add_css_class(rw->summary_label, "card-kpi-sub");
    gtk_label_set_xalign(GTK_LABEL(rw->summary_label), 0.0f);
    gtk_box_append(GTK_BOX(rw->root_box), rw->summary_label);

    /* ── Preview Box ────────────────────────────────────────────── */
    rw->preview_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_append(GTK_BOX(rw->root_box), rw->preview_box);

    /* Initial Load */
    refresh_report(rw);

    /* Wrap in scrolled window so reports page shrinks gracefully */
    GtkWidget *page_scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(page_scroll),
                                   GTK_POLICY_AUTOMATIC,
                                   GTK_POLICY_AUTOMATIC);
    gtk_widget_set_hexpand(page_scroll, TRUE);
    gtk_widget_set_vexpand(page_scroll, TRUE);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(page_scroll), rw->root_box);

    g_object_set_data_full(G_OBJECT(page_scroll), "reports_state", rw, g_free);

    return page_scroll;
}

void school_reports_view_reload(GtkWidget *view)
{
    if (!view) return;
    ReportsWidgets *rw = (ReportsWidgets *)g_object_get_data(G_OBJECT(view), "reports_state");
    if (rw) {
        refresh_report(rw);
    }
}
