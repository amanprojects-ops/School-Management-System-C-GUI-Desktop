/**
 * @file attendance_view.c
 * @brief Attendance GUI module implementation with daily roster marking and monthly registers.
 */

#include "ui/attendance_view.h"
#include "attendance.h"
#include "classes.h"
#include "student.h"
#include "auth/auth.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef enum {
    TAB_DAILY = 0,
    TAB_MONTHLY,
    TAB_ANALYTICS
} AttendanceTab;

typedef struct {
    int student_id;
    GtkWidget *combo_status;
    GtkWidget *entry_remarks;
} RosterRowWidgets;

typedef struct {
    GtkWidget *root_box;
    GtkWidget *tab_daily_btn;
    GtkWidget *tab_monthly_btn;
    GtkWidget *tab_analytics_btn;

    /* Filter controls */
    GtkWidget *box_class;
    GtkWidget *combo_class;
    GtkWidget *box_section;
    GtkWidget *combo_section;
    GtkWidget *entry_date;
    GtkWidget *combo_month;
    GtkWidget *entry_year;

    GtkWidget *status_banner;
    GtkWidget *roster_container;
    GtkWidget *stats_container;
    GtkWidget *btn_save_attendance;
    GtkWidget *quick_bar;

    AttendanceTab current_tab;

    /* Class and Section data */
    SchoolClass classes[MAX_CLASSES];
    int class_count;
    ClassSection sections[MAX_SECTIONS];
    int section_count;

    /* Current Daily Roster */
    AttendanceRecord roster[ATTENDANCE_MAX_RECORDS];
    int roster_count;
    RosterRowWidgets row_widgets[ATTENDANCE_MAX_RECORDS];

    /* Current active filters */
    int selected_class_id;
    int selected_section_id;
    char selected_date[16];
    int selected_month;
    int selected_year;
} AttendanceViewWidgets;

static void refresh_view(AttendanceViewWidgets *avw);
static void update_classes_dropdown(AttendanceViewWidgets *avw);
static void update_sections_dropdown(AttendanceViewWidgets *avw);
static void render_daily_sheet(AttendanceViewWidgets *avw);
static void render_monthly_register(AttendanceViewWidgets *avw);
static void render_student_analytics(AttendanceViewWidgets *avw);

/* ── Helper: Format Today's Date ──────────────────────────────────── */

static void get_current_date(char *out, size_t max_len)
{
    time_t t = time(NULL);
    struct tm tm = *localtime(&t);
    snprintf(out, max_len, "%04d-%02d-%02d", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday);
}

/* ── Tab Switching ────────────────────────────────────────────────── */

static void on_tab_daily_clicked(GtkButton *btn, gpointer user_data)
{
    (void)btn;
    AttendanceViewWidgets *avw = (AttendanceViewWidgets *)user_data;
    avw->current_tab = TAB_DAILY;
    if (avw->status_banner) gtk_widget_set_visible(avw->status_banner, FALSE);
    gtk_widget_add_css_class(avw->tab_daily_btn, "tab-btn-active");
    gtk_widget_remove_css_class(avw->tab_monthly_btn, "tab-btn-active");
    gtk_widget_remove_css_class(avw->tab_analytics_btn, "tab-btn-active");
    gboolean can_edit = auth_current_user_can_access("ATTENDANCE", true);
    gtk_widget_set_visible(avw->quick_bar, can_edit);
    gtk_widget_set_visible(avw->btn_save_attendance, can_edit);
    refresh_view(avw);
}

static void on_tab_monthly_clicked(GtkButton *btn, gpointer user_data)
{
    (void)btn;
    AttendanceViewWidgets *avw = (AttendanceViewWidgets *)user_data;
    avw->current_tab = TAB_MONTHLY;
    if (avw->status_banner) gtk_widget_set_visible(avw->status_banner, FALSE);
    gtk_widget_remove_css_class(avw->tab_daily_btn, "tab-btn-active");
    gtk_widget_add_css_class(avw->tab_monthly_btn, "tab-btn-active");
    gtk_widget_remove_css_class(avw->tab_analytics_btn, "tab-btn-active");
    gtk_widget_set_visible(avw->quick_bar, FALSE);
    gtk_widget_set_visible(avw->btn_save_attendance, FALSE);
    refresh_view(avw);
}

static void on_tab_analytics_clicked(GtkButton *btn, gpointer user_data)
{
    (void)btn;
    AttendanceViewWidgets *avw = (AttendanceViewWidgets *)user_data;
    avw->current_tab = TAB_ANALYTICS;
    if (avw->status_banner) gtk_widget_set_visible(avw->status_banner, FALSE);
    gtk_widget_remove_css_class(avw->tab_daily_btn, "tab-btn-active");
    gtk_widget_remove_css_class(avw->tab_monthly_btn, "tab-btn-active");
    gtk_widget_add_css_class(avw->tab_analytics_btn, "tab-btn-active");
    gtk_widget_set_visible(avw->quick_bar, FALSE);
    gtk_widget_set_visible(avw->btn_save_attendance, FALSE);
    refresh_view(avw);
}

/* ── Filter Selection Callbacks ───────────────────────────────────── */

static void on_class_changed(GtkDropDown *dropdown, GParamSpec *pspec, gpointer user_data)
{
    (void)pspec;
    AttendanceViewWidgets *avw = (AttendanceViewWidgets *)user_data;
    guint sel = gtk_drop_down_get_selected(dropdown);
    if ((int)sel < avw->class_count) {
        avw->selected_class_id = avw->classes[sel].id;
    } else {
        avw->selected_class_id = 0;
    }
    update_sections_dropdown(avw);
    refresh_view(avw);
}

static void on_section_changed(GtkDropDown *dropdown, GParamSpec *pspec, gpointer user_data)
{
    (void)pspec;
    AttendanceViewWidgets *avw = (AttendanceViewWidgets *)user_data;
    guint sel = gtk_drop_down_get_selected(dropdown);
    if (sel == 0) {
        avw->selected_section_id = 0; /* All sections */
    } else if ((int)sel - 1 < avw->section_count) {
        avw->selected_section_id = avw->sections[sel - 1].id;
    }
    refresh_view(avw);
}

static void on_date_changed(GtkEditable *editable, gpointer user_data)
{
    AttendanceViewWidgets *avw = (AttendanceViewWidgets *)user_data;
    const char *dt = gtk_editable_get_text(editable);
    if (dt && strlen(dt) >= 10) {
        strncpy(avw->selected_date, dt, sizeof(avw->selected_date) - 1);
        if (avw->current_tab == TAB_DAILY) {
            refresh_view(avw);
        }
    }
}

static void on_month_changed(GtkDropDown *dropdown, GParamSpec *pspec, gpointer user_data)
{
    (void)pspec;
    AttendanceViewWidgets *avw = (AttendanceViewWidgets *)user_data;
    avw->selected_month = (int)gtk_drop_down_get_selected(dropdown) + 1;
    if (avw->current_tab == TAB_MONTHLY) {
        refresh_view(avw);
    }
}

/* ── Bulk Quick Actions ───────────────────────────────────────────── */

static void on_mark_all_present(GtkButton *btn, gpointer user_data)
{
    (void)btn;
    AttendanceViewWidgets *avw = (AttendanceViewWidgets *)user_data;
    if (!auth_current_user_can_access("ATTENDANCE", true)) return;
    for (int i = 0; i < avw->roster_count; i++) {
        if (avw->row_widgets[i].combo_status) {
            gtk_drop_down_set_selected(GTK_DROP_DOWN(avw->row_widgets[i].combo_status), 0); /* PRESENT */
        }
    }
}

static void on_mark_all_absent(GtkButton *btn, gpointer user_data)
{
    (void)btn;
    AttendanceViewWidgets *avw = (AttendanceViewWidgets *)user_data;
    if (!auth_current_user_can_access("ATTENDANCE", true)) return;
    for (int i = 0; i < avw->roster_count; i++) {
        if (avw->row_widgets[i].combo_status) {
            gtk_drop_down_set_selected(GTK_DROP_DOWN(avw->row_widgets[i].combo_status), 1); /* ABSENT */
        }
    }
}

static void on_save_attendance_clicked(GtkButton *btn, gpointer user_data)
{
    (void)btn;
    AttendanceViewWidgets *avw = (AttendanceViewWidgets *)user_data;
    if (!avw) return;
    if (!auth_current_user_can_access("ATTENDANCE", true)) {
        gtk_label_set_text(GTK_LABEL(avw->status_banner), "Read-only access: your role cannot modify attendance.");
        gtk_widget_remove_css_class(avw->status_banner, "card-kpi-green");
        gtk_widget_add_css_class(avw->status_banner, "card-kpi-red");
        gtk_widget_set_visible(avw->status_banner, TRUE);
        return;
    }

    if (avw->selected_class_id <= 0) {
        gtk_label_set_text(GTK_LABEL(avw->status_banner), "Please select a valid class before saving attendance.");
        gtk_widget_remove_css_class(avw->status_banner, "card-kpi-green");
        gtk_widget_add_css_class(avw->status_banner, "card-kpi-red");
        gtk_widget_set_visible(avw->status_banner, TRUE);
        return;
    }

    if (avw->roster_count == 0) {
        gtk_label_set_text(GTK_LABEL(avw->status_banner), "No students found in the selected class/section to record attendance.");
        gtk_widget_remove_css_class(avw->status_banner, "card-kpi-green");
        gtk_widget_add_css_class(avw->status_banner, "card-kpi-red");
        gtk_widget_set_visible(avw->status_banner, TRUE);
        return;
    }

    AttendanceEntry entries[ATTENDANCE_MAX_RECORDS];
    const char *status_opts[] = { ATTENDANCE_STATUS_PRESENT, ATTENDANCE_STATUS_ABSENT,
                                  ATTENDANCE_STATUS_LATE, ATTENDANCE_STATUS_HALF_DAY };

    int user_id = 1;
    const UserSession *sess = auth_get_session();
    if (sess && sess->user_id > 0) user_id = sess->user_id;

    for (int i = 0; i < avw->roster_count; i++) {
        entries[i].student_id = avw->roster[i].student_id;

        guint sel = gtk_drop_down_get_selected(GTK_DROP_DOWN(avw->row_widgets[i].combo_status));
        if (sel > 3) sel = 0;
        strncpy(entries[i].status, status_opts[sel], sizeof(entries[i].status) - 1);

        const char *rm = gtk_editable_get_text(GTK_EDITABLE(avw->row_widgets[i].entry_remarks));
        if (rm) strncpy(entries[i].remarks, rm, sizeof(entries[i].remarks) - 1);
        else entries[i].remarks[0] = '\0';
    }

    int rc = attendance_service_mark_bulk(avw->selected_class_id, avw->selected_section_id,
                                          avw->selected_date, entries, avw->roster_count, user_id);

    if (rc == 0) {
        char msg[256];
        snprintf(msg, sizeof(msg), "Attendance for %d students saved successfully on %s.",
                 avw->roster_count, avw->selected_date);
        gtk_label_set_text(GTK_LABEL(avw->status_banner), msg);
        gtk_widget_remove_css_class(avw->status_banner, "card-kpi-red");
        gtk_widget_add_css_class(avw->status_banner, "card-kpi-green");
        gtk_widget_set_visible(avw->status_banner, TRUE);
    } else {
        gtk_label_set_text(GTK_LABEL(avw->status_banner), "Failed to save attendance records to database.");
        gtk_widget_remove_css_class(avw->status_banner, "card-kpi-green");
        gtk_widget_add_css_class(avw->status_banner, "card-kpi-red");
        gtk_widget_set_visible(avw->status_banner, TRUE);
    }

    refresh_view(avw);
}

/* ── Section & Class Dropdown Synchronizers ────────────────────────── */

static void update_sections_dropdown(AttendanceViewWidgets *avw)
{
    if (!avw || !avw->box_section) return;

    if (avw->combo_section && gtk_widget_get_parent(avw->combo_section)) {
        gtk_box_remove(GTK_BOX(avw->box_section), avw->combo_section);
    }

    if (avw->selected_class_id <= 0) {
        const char *none[] = { "All Sections", NULL };
        avw->combo_section = gtk_drop_down_new_from_strings(none);
        gtk_box_append(GTK_BOX(avw->box_section), avw->combo_section);
        avw->selected_section_id = 0;
        return;
    }

    avw->section_count = 0;
    section_service_get_by_class(avw->selected_class_id, avw->sections, MAX_SECTIONS, &avw->section_count);

    const char *sec_names[MAX_SECTIONS + 2];
    sec_names[0] = "All Sections";
    for (int i = 0; i < avw->section_count; i++) {
        sec_names[i + 1] = avw->sections[i].name;
    }
    sec_names[avw->section_count + 1] = NULL;

    avw->combo_section = gtk_drop_down_new_from_strings(sec_names);
    g_signal_connect(avw->combo_section, "notify::selected", G_CALLBACK(on_section_changed), avw);
    gtk_box_append(GTK_BOX(avw->box_section), avw->combo_section);
    avw->selected_section_id = 0;
}

static void update_classes_dropdown(AttendanceViewWidgets *avw)
{
    if (!avw || !avw->box_class) return;

    avw->class_count = 0;
    class_service_get_all(avw->classes, MAX_CLASSES, &avw->class_count);

    if (avw->combo_class && gtk_widget_get_parent(avw->combo_class)) {
        gtk_box_remove(GTK_BOX(avw->box_class), avw->combo_class);
    }

    if (avw->class_count == 0) {
        const char *none[] = { "No Classes Found", NULL };
        avw->combo_class = gtk_drop_down_new_from_strings(none);
        avw->selected_class_id = 0;
    } else {
        const char *class_names[MAX_CLASSES + 1];
        int sel_idx = 0;
        bool found = false;

        for (int i = 0; i < avw->class_count; i++) {
            class_names[i] = avw->classes[i].name;
            if (avw->selected_class_id > 0 && avw->classes[i].id == avw->selected_class_id) {
                sel_idx = i;
                found = true;
            }
        }
        class_names[avw->class_count] = NULL;

        if (!found) {
            sel_idx = 0;
            avw->selected_class_id = avw->classes[0].id;
        }

        avw->combo_class = gtk_drop_down_new_from_strings(class_names);
        gtk_drop_down_set_selected(GTK_DROP_DOWN(avw->combo_class), (guint)sel_idx);
        g_signal_connect(avw->combo_class, "notify::selected", G_CALLBACK(on_class_changed), avw);
    }

    gtk_box_append(GTK_BOX(avw->box_class), avw->combo_class);
    update_sections_dropdown(avw);
}

/* ── View Renderers ───────────────────────────────────────────────── */

static void render_stats_banner(AttendanceViewWidgets *avw)
{
    /* Clear stats container */
    GtkWidget *c = gtk_widget_get_first_child(avw->stats_container);
    while (c) {
        GtkWidget *next = gtk_widget_get_next_sibling(c);
        gtk_box_remove(GTK_BOX(avw->stats_container), c);
        c = next;
    }

    if (avw->selected_class_id <= 0) return;

    AttendanceStats stats;
    attendance_service_get_class_stats(avw->selected_class_id, avw->selected_section_id,
                                       avw->selected_date, &stats);

    GtkWidget *banner = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 16);
    gtk_widget_add_css_class(banner, "card");
    gtk_widget_set_margin_bottom(banner, 12);

    /* Present */
    char p_buf[64];
    snprintf(p_buf, sizeof(p_buf), "Present: %d", stats.present_count);
    GtkWidget *p_lbl = gtk_label_new(p_buf);
    gtk_widget_add_css_class(p_lbl, "badge-status-active");
    gtk_box_append(GTK_BOX(banner), p_lbl);

    /* Absent */
    char a_buf[64];
    snprintf(a_buf, sizeof(a_buf), "Absent: %d", stats.absent_count);
    GtkWidget *a_lbl = gtk_label_new(a_buf);
    gtk_widget_add_css_class(a_lbl, "badge-status-inactive");
    gtk_box_append(GTK_BOX(banner), a_lbl);

    /* Late */
    char l_buf[64];
    snprintf(l_buf, sizeof(l_buf), "Late: %d", stats.late_count);
    GtkWidget *l_lbl = gtk_label_new(l_buf);
    gtk_widget_add_css_class(l_lbl, "badge-role");
    gtk_box_append(GTK_BOX(banner), l_lbl);

    /* Half-Day */
    char h_buf[64];
    snprintf(h_buf, sizeof(h_buf), "Half-Day: %d", stats.half_day_count);
    GtkWidget *h_lbl = gtk_label_new(h_buf);
    gtk_widget_add_css_class(h_lbl, "badge-role");
    gtk_box_append(GTK_BOX(banner), h_lbl);

    /* Attendance Rate */
    char r_buf[64];
    snprintf(r_buf, sizeof(r_buf), "Class Attendance: %.1f%%", stats.percentage);
    GtkWidget *r_lbl = gtk_label_new(r_buf);
    gtk_widget_add_css_class(r_lbl, "badge-adm");
    gtk_widget_set_hexpand(r_lbl, TRUE);
    gtk_label_set_xalign(GTK_LABEL(r_lbl), 1.0f);
    gtk_box_append(GTK_BOX(banner), r_lbl);

    gtk_box_append(GTK_BOX(avw->stats_container), banner);
}

static void render_daily_sheet(AttendanceViewWidgets *avw)
{
    render_stats_banner(avw);

    if (avw->selected_class_id <= 0) {
        GtkWidget *msg = gtk_label_new("Please select a class above to view and record daily attendance.");
        gtk_widget_add_css_class(msg, "dim-label");
        gtk_box_append(GTK_BOX(avw->roster_container), msg);
        return;
    }

    avw->roster_count = 0;
    attendance_service_get_roster(avw->selected_class_id, avw->selected_section_id,
                                  avw->selected_date, avw->roster, ATTENDANCE_MAX_RECORDS,
                                  &avw->roster_count);

    if (avw->roster_count == 0) {
        GtkWidget *empty_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
        gtk_widget_set_margin_top(empty_box, 30);
        gtk_widget_set_margin_bottom(empty_box, 30);
        gtk_widget_set_halign(empty_box, GTK_ALIGN_CENTER);

        GtkWidget *lbl = gtk_label_new("No active students found in this class/section.");
        gtk_widget_add_css_class(lbl, "dim-label");
        gtk_box_append(GTK_BOX(empty_box), lbl);
        gtk_box_append(GTK_BOX(avw->roster_container), empty_box);
        return;
    }

    GtkWidget *card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_add_css_class(card, "card");
    gtk_box_append(GTK_BOX(avw->roster_container), card);

    /* Table Header */
    GtkWidget *header_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_add_css_class(header_row, "table-header-row");

    const char *cols[] = { "Roll", "Admission No", "Student Name", "Section", "Status", "Remarks" };
    int widths[] = { 60, 130, 220, 100, 150, 180 };

    for (int i = 0; i < 6; i++) {
        GtkWidget *th = gtk_label_new(cols[i]);
        gtk_widget_add_css_class(th, "table-th");
        gtk_label_set_xalign(GTK_LABEL(th), 0.0f);
        gtk_widget_set_size_request(th, widths[i], -1);
        if (i == 2) gtk_widget_set_hexpand(th, TRUE);
        gtk_box_append(GTK_BOX(header_row), th);
    }
    gtk_box_append(GTK_BOX(card), header_row);

    const char *status_options[] = { "PRESENT", "ABSENT", "LATE", "HALF_DAY", NULL };

    for (int i = 0; i < avw->roster_count; i++) {
        const AttendanceRecord *r = &avw->roster[i];
        GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
        gtk_widget_add_css_class(row, "table-data-row");

        /* Roll */
        char roll_buf[16];
        snprintf(roll_buf, sizeof(roll_buf), "%d", r->roll_number);
        GtkWidget *roll_lbl = gtk_label_new(roll_buf);
        gtk_widget_set_size_request(roll_lbl, widths[0], -1);
        gtk_label_set_xalign(GTK_LABEL(roll_lbl), 0.0f);
        gtk_box_append(GTK_BOX(row), roll_lbl);

        /* Admission No */
        GtkWidget *adm_lbl = gtk_label_new(r->admission_no);
        gtk_widget_add_css_class(adm_lbl, "badge-adm");
        gtk_widget_set_size_request(adm_lbl, widths[1], -1);
        gtk_label_set_xalign(GTK_LABEL(adm_lbl), 0.0f);
        gtk_box_append(GTK_BOX(row), adm_lbl);

        /* Student Name */
        GtkWidget *name_lbl = gtk_label_new(r->student_name);
        gtk_widget_add_css_class(name_lbl, "table-cell-bold");
        gtk_label_set_xalign(GTK_LABEL(name_lbl), 0.0f);
        gtk_widget_set_hexpand(name_lbl, TRUE);
        gtk_box_append(GTK_BOX(row), name_lbl);

        /* Section */
        GtkWidget *sec_lbl = gtk_label_new(strlen(r->section_name) > 0 ? r->section_name : "-");
        gtk_widget_set_size_request(sec_lbl, widths[3], -1);
        gtk_label_set_xalign(GTK_LABEL(sec_lbl), 0.0f);
        gtk_box_append(GTK_BOX(row), sec_lbl);

        /* Status Dropdown */
        gboolean can_edit = auth_current_user_can_access("ATTENDANCE", true);
        GtkWidget *combo = gtk_drop_down_new_from_strings(status_options);
        gtk_widget_set_size_request(combo, widths[4], -1);

        int sel_idx = 0;
        if (strcmp(r->status, ATTENDANCE_STATUS_ABSENT) == 0) sel_idx = 1;
        else if (strcmp(r->status, ATTENDANCE_STATUS_LATE) == 0) sel_idx = 2;
        else if (strcmp(r->status, ATTENDANCE_STATUS_HALF_DAY) == 0) sel_idx = 3;
        gtk_drop_down_set_selected(GTK_DROP_DOWN(combo), sel_idx);
        gtk_widget_set_sensitive(combo, can_edit);
        gtk_box_append(GTK_BOX(row), combo);

        /* Remarks Entry */
        GtkWidget *entry_rm = gtk_entry_new();
        gtk_widget_set_size_request(entry_rm, widths[5], -1);
        gtk_editable_set_text(GTK_EDITABLE(entry_rm), r->remarks);
        gtk_editable_set_editable(GTK_EDITABLE(entry_rm), can_edit);
        gtk_widget_set_sensitive(entry_rm, can_edit);
        gtk_box_append(GTK_BOX(row), entry_rm);

        /* Track widgets for bulk saving */
        avw->row_widgets[i].student_id = r->student_id;
        avw->row_widgets[i].combo_status = combo;
        avw->row_widgets[i].entry_remarks = entry_rm;

        gtk_box_append(GTK_BOX(card), row);
    }
}

static void render_monthly_register(AttendanceViewWidgets *avw)
{
    if (avw->selected_class_id <= 0) {
        GtkWidget *msg = gtk_label_new("Please select a class to view the monthly attendance register.");
        gtk_widget_add_css_class(msg, "dim-label");
        gtk_box_append(GTK_BOX(avw->roster_container), msg);
        return;
    }

    AttendanceMonthlyReport reports[ATTENDANCE_MAX_RECORDS];
    int count = 0;
    attendance_service_get_monthly_report(avw->selected_class_id, avw->selected_section_id,
                                          avw->selected_year, avw->selected_month,
                                          reports, ATTENDANCE_MAX_RECORDS, &count);

    if (count == 0) {
        GtkWidget *empty_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
        gtk_widget_set_margin_top(empty_box, 30);
        GtkWidget *lbl = gtk_label_new("No active students found in this class.");
        gtk_widget_add_css_class(lbl, "dim-label");
        gtk_box_append(GTK_BOX(empty_box), lbl);
        gtk_box_append(GTK_BOX(avw->roster_container), empty_box);
        return;
    }

    int days = reports[0].days_in_month;

    /* Card holding the register */
    GtkWidget *card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_add_css_class(card, "card");
    gtk_box_append(GTK_BOX(avw->roster_container), card);

    /* Header Row */
    GtkWidget *h_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_widget_add_css_class(h_row, "table-header-row");

    GtkWidget *th_roll = gtk_label_new("Roll");
    gtk_widget_set_size_request(th_roll, 40, -1);
    gtk_box_append(GTK_BOX(h_row), th_roll);

    GtkWidget *th_name = gtk_label_new("Student Name");
    gtk_widget_set_size_request(th_name, 180, -1);
    gtk_label_set_xalign(GTK_LABEL(th_name), 0.0f);
    gtk_box_append(GTK_BOX(h_row), th_name);

    for (int d = 1; d <= days; d++) {
        char day_num[16];
        snprintf(day_num, sizeof(day_num), "%d", d);
        GtkWidget *th_d = gtk_label_new(day_num);
        gtk_widget_set_size_request(th_d, 22, -1);
        gtk_box_append(GTK_BOX(h_row), th_d);
    }

    GtkWidget *th_p = gtk_label_new("P");
    gtk_widget_set_size_request(th_p, 36, -1);
    gtk_box_append(GTK_BOX(h_row), th_p);

    GtkWidget *th_a = gtk_label_new("A");
    gtk_widget_set_size_request(th_a, 36, -1);
    gtk_box_append(GTK_BOX(h_row), th_a);

    GtkWidget *th_pct = gtk_label_new("Rate %");
    gtk_widget_set_size_request(th_pct, 60, -1);
    gtk_box_append(GTK_BOX(h_row), th_pct);

    gtk_box_append(GTK_BOX(card), h_row);

    /* Data Rows */
    for (int i = 0; i < count; i++) {
        const AttendanceMonthlyReport *mr = &reports[i];
        GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
        gtk_widget_add_css_class(row, "table-data-row");

        char roll_buf[16];
        snprintf(roll_buf, sizeof(roll_buf), "%d", mr->roll_number);
        GtkWidget *r_lbl = gtk_label_new(roll_buf);
        gtk_widget_set_size_request(r_lbl, 40, -1);
        gtk_box_append(GTK_BOX(row), r_lbl);

        GtkWidget *n_lbl = gtk_label_new(mr->student_name);
        gtk_widget_set_size_request(n_lbl, 180, -1);
        gtk_label_set_xalign(GTK_LABEL(n_lbl), 0.0f);
        gtk_box_append(GTK_BOX(row), n_lbl);

        for (int d = 1; d <= days; d++) {
            GtkWidget *d_lbl = gtk_label_new(mr->daily_status[d]);
            gtk_widget_set_size_request(d_lbl, 22, -1);
            if (strcmp(mr->daily_status[d], "P") == 0) {
                gtk_widget_add_css_class(d_lbl, "table-cell-bold");
            } else if (strcmp(mr->daily_status[d], "A") == 0) {
                gtk_widget_add_css_class(d_lbl, "badge-status-inactive");
            }
            gtk_box_append(GTK_BOX(row), d_lbl);
        }

        char p_str[16], a_str[16], pct_str[16];
        snprintf(p_str, sizeof(p_str), "%d", mr->total_present);
        snprintf(a_str, sizeof(a_str), "%d", mr->total_absent);
        snprintf(pct_str, sizeof(pct_str), "%.0f%%", mr->percentage);

        GtkWidget *p_lbl = gtk_label_new(p_str);
        gtk_widget_set_size_request(p_lbl, 36, -1);
        gtk_box_append(GTK_BOX(row), p_lbl);

        GtkWidget *a_lbl = gtk_label_new(a_str);
        gtk_widget_set_size_request(a_lbl, 36, -1);
        gtk_box_append(GTK_BOX(row), a_lbl);

        GtkWidget *pct_lbl = gtk_label_new(pct_str);
        gtk_widget_set_size_request(pct_lbl, 60, -1);
        gtk_widget_add_css_class(pct_lbl, "badge-adm");
        gtk_box_append(GTK_BOX(row), pct_lbl);

        gtk_box_append(GTK_BOX(card), row);
    }
}

static void render_student_analytics(AttendanceViewWidgets *avw)
{
    GtkWidget *card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 16);
    gtk_widget_add_css_class(card, "card");

    GtkWidget *lbl_info = gtk_label_new("Class Attendance Summary & Analytics");
    gtk_widget_add_css_class(lbl_info, "dashboard-card-title");
    gtk_label_set_xalign(GTK_LABEL(lbl_info), 0.0f);
    gtk_box_append(GTK_BOX(card), lbl_info);

    if (avw->selected_class_id <= 0) {
        GtkWidget *msg = gtk_label_new("Select a class above to see analytics.");
        gtk_widget_add_css_class(msg, "dim-label");
        gtk_box_append(GTK_BOX(card), msg);
        gtk_box_append(GTK_BOX(avw->roster_container), card);
        return;
    }

    AttendanceStats stats;
    attendance_service_get_class_stats(avw->selected_class_id, avw->selected_section_id,
                                       avw->selected_date, &stats);

    char sum_buf[256];
    snprintf(sum_buf, sizeof(sum_buf),
             "Total Recorded Today: %d\n"
             "Present: %d | Absent: %d | Late: %d | Half-Day: %d\n"
             "Overall Daily Rate: %.1f%%",
             stats.total_days, stats.present_count, stats.absent_count,
             stats.late_count, stats.half_day_count, stats.percentage);

    GtkWidget *sum_lbl = gtk_label_new(sum_buf);
    gtk_label_set_xalign(GTK_LABEL(sum_lbl), 0.0f);
    gtk_box_append(GTK_BOX(card), sum_lbl);

    gtk_box_append(GTK_BOX(avw->roster_container), card);
}

static void refresh_view(AttendanceViewWidgets *avw)
{
    gboolean can_edit = auth_current_user_can_access("ATTENDANCE", true);
    if (avw->quick_bar) {
        gtk_widget_set_visible(avw->quick_bar, can_edit && avw->current_tab == TAB_DAILY);
    }
    if (avw->btn_save_attendance) {
        gtk_widget_set_visible(avw->btn_save_attendance, can_edit && avw->current_tab == TAB_DAILY);
    }

    /* Clear roster container */
    GtkWidget *c = gtk_widget_get_first_child(avw->roster_container);
    while (c) {
        GtkWidget *next = gtk_widget_get_next_sibling(c);
        gtk_box_remove(GTK_BOX(avw->roster_container), c);
        c = next;
    }

    if (avw->current_tab == TAB_DAILY) {
        render_daily_sheet(avw);
    } else if (avw->current_tab == TAB_MONTHLY) {
        render_monthly_register(avw);
    } else {
        render_student_analytics(avw);
    }
}

/* ── Main View Constructor ────────────────────────────────────────── */

GtkWidget *school_attendance_view_new(void)
{
    AttendanceViewWidgets *avw = g_new0(AttendanceViewWidgets, 1);
    avw->current_tab = TAB_DAILY;
    get_current_date(avw->selected_date, sizeof(avw->selected_date));
    avw->selected_month = 4; /* Default April (start of session) or current */
    avw->selected_year = 2026;

    time_t t = time(NULL);
    struct tm tm = *localtime(&t);
    avw->selected_month = tm.tm_mon + 1;
    avw->selected_year = tm.tm_year + 1900;

    /* Main Scrollable View */
    GtkWidget *scrolled = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
                                  GTK_POLICY_AUTOMATIC,
                                  GTK_POLICY_AUTOMATIC);

    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 20);
    gtk_widget_add_css_class(box, "page-container");
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled), box);
    avw->root_box = box;
    g_object_set_data_full(G_OBJECT(scrolled), "avw", avw, g_free);

    /* ── Header Row ───────────────────────────────────────────────── */
    GtkWidget *top_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 16);
    gtk_widget_set_valign(top_row, GTK_ALIGN_CENTER);

    GtkWidget *title_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    GtkWidget *title = gtk_label_new("Attendance Management");
    gtk_widget_add_css_class(title, "page-title");
    gtk_label_set_xalign(GTK_LABEL(title), 0.0f);
    gtk_box_append(GTK_BOX(title_box), title);

    GtkWidget *sub = gtk_label_new("Record daily attendance, bulk mark rosters, and review monthly registers");
    gtk_widget_add_css_class(sub, "page-subtitle");
    gtk_label_set_xalign(GTK_LABEL(sub), 0.0f);
    gtk_box_append(GTK_BOX(title_box), sub);

    gtk_widget_set_hexpand(title_box, TRUE);
    gtk_box_append(GTK_BOX(top_row), title_box);

    /* Save Attendance Primary Action */
    avw->btn_save_attendance = gtk_button_new_with_label("Save Attendance");
    gtk_widget_add_css_class(avw->btn_save_attendance, "btn-primary");
    g_signal_connect(avw->btn_save_attendance, "clicked", G_CALLBACK(on_save_attendance_clicked), avw);
    gtk_box_append(GTK_BOX(top_row), avw->btn_save_attendance);

    gtk_box_append(GTK_BOX(box), top_row);

    /* ── Status Banner ───────────────────────────────────────────── */
    avw->status_banner = gtk_label_new("");
    gtk_widget_add_css_class(avw->status_banner, "card");
    gtk_widget_set_visible(avw->status_banner, FALSE);
    gtk_box_append(GTK_BOX(box), avw->status_banner);

    /* ── Tab Switcher ─────────────────────────────────────────────── */
    GtkWidget *tab_bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);

    avw->tab_daily_btn = gtk_button_new_with_label("Daily Attendance Sheet");
    gtk_widget_add_css_class(avw->tab_daily_btn, "tab-btn");
    gtk_widget_add_css_class(avw->tab_daily_btn, "tab-btn-active");
    g_signal_connect(avw->tab_daily_btn, "clicked", G_CALLBACK(on_tab_daily_clicked), avw);
    gtk_box_append(GTK_BOX(tab_bar), avw->tab_daily_btn);

    avw->tab_monthly_btn = gtk_button_new_with_label("Monthly Register");
    gtk_widget_add_css_class(avw->tab_monthly_btn, "tab-btn");
    g_signal_connect(avw->tab_monthly_btn, "clicked", G_CALLBACK(on_tab_monthly_clicked), avw);
    gtk_box_append(GTK_BOX(tab_bar), avw->tab_monthly_btn);

    avw->tab_analytics_btn = gtk_button_new_with_label("Attendance Analytics");
    gtk_widget_add_css_class(avw->tab_analytics_btn, "tab-btn");
    g_signal_connect(avw->tab_analytics_btn, "clicked", G_CALLBACK(on_tab_analytics_clicked), avw);
    gtk_box_append(GTK_BOX(tab_bar), avw->tab_analytics_btn);

    gtk_box_append(GTK_BOX(box), tab_bar);

    /* ── Filter Toolbar ───────────────────────────────────────────── */
    GtkWidget *filter_card = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 16);
    gtk_widget_add_css_class(filter_card, "card");
    gtk_widget_set_valign(filter_card, GTK_ALIGN_CENTER);

    /* Class Selector */
    avw->box_class = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    GtkWidget *lbl_c = gtk_label_new("Class:");
    gtk_label_set_xalign(GTK_LABEL(lbl_c), 0.0f);
    gtk_box_append(GTK_BOX(avw->box_class), lbl_c);
    gtk_box_append(GTK_BOX(filter_card), avw->box_class);

    /* Section Selector */
    avw->box_section = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    GtkWidget *lbl_s = gtk_label_new("Section:");
    gtk_label_set_xalign(GTK_LABEL(lbl_s), 0.0f);
    gtk_box_append(GTK_BOX(avw->box_section), lbl_s);
    gtk_box_append(GTK_BOX(filter_card), avw->box_section);

    update_classes_dropdown(avw);

    /* Date Selector */
    GtkWidget *box_dt = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    GtkWidget *lbl_d = gtk_label_new("Date (YYYY-MM-DD):");
    gtk_label_set_xalign(GTK_LABEL(lbl_d), 0.0f);
    gtk_box_append(GTK_BOX(box_dt), lbl_d);

    avw->entry_date = gtk_entry_new();
    gtk_editable_set_text(GTK_EDITABLE(avw->entry_date), avw->selected_date);
    g_signal_connect(avw->entry_date, "changed", G_CALLBACK(on_date_changed), avw);
    gtk_box_append(GTK_BOX(box_dt), avw->entry_date);
    gtk_box_append(GTK_BOX(filter_card), box_dt);

    /* Month Selector for Register */
    GtkWidget *box_m = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    GtkWidget *lbl_m = gtk_label_new("Month:");
    gtk_label_set_xalign(GTK_LABEL(lbl_m), 0.0f);
    gtk_box_append(GTK_BOX(box_m), lbl_m);

    const char *months[] = { "January", "February", "March", "April", "May", "June",
                             "July", "August", "September", "October", "November", "December", NULL };
    avw->combo_month = gtk_drop_down_new_from_strings(months);
    gtk_drop_down_set_selected(GTK_DROP_DOWN(avw->combo_month), avw->selected_month - 1);
    g_signal_connect(avw->combo_month, "notify::selected", G_CALLBACK(on_month_changed), avw);
    gtk_box_append(GTK_BOX(box_m), avw->combo_month);
    gtk_box_append(GTK_BOX(filter_card), box_m);

    gtk_box_append(GTK_BOX(box), filter_card);

    /* ── Quick Action Bar ─────────────────────────────────────────── */
    avw->quick_bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_set_margin_bottom(avw->quick_bar, 4);

    GtkWidget *btn_all_p = gtk_button_new_with_label("Mark All Present");
    gtk_widget_add_css_class(btn_all_p, "btn-secondary");
    g_signal_connect(btn_all_p, "clicked", G_CALLBACK(on_mark_all_present), avw);
    gtk_box_append(GTK_BOX(avw->quick_bar), btn_all_p);

    GtkWidget *btn_all_a = gtk_button_new_with_label("Mark All Absent");
    gtk_widget_add_css_class(btn_all_a, "btn-secondary");
    g_signal_connect(btn_all_a, "clicked", G_CALLBACK(on_mark_all_absent), avw);
    gtk_box_append(GTK_BOX(avw->quick_bar), btn_all_a);

    gtk_box_append(GTK_BOX(box), avw->quick_bar);

    /* ── Stats Banner Container ───────────────────────────────────── */
    avw->stats_container = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_append(GTK_BOX(box), avw->stats_container);

    /* ── Roster & Table Container ─────────────────────────────────── */
    avw->roster_container = gtk_box_new(GTK_ORIENTATION_VERTICAL, 16);
    gtk_box_append(GTK_BOX(box), avw->roster_container);

    refresh_view(avw);

    return scrolled;
}

void school_attendance_view_reload(GtkWidget *view)
{
    if (!view) return;
    AttendanceViewWidgets *avw = (AttendanceViewWidgets *)g_object_get_data(G_OBJECT(view), "avw");
    if (avw) {
        update_classes_dropdown(avw);
        refresh_view(avw);
    }
}
