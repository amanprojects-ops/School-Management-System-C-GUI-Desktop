/**
 * @file dashboard.c
 * @brief Dashboard page with dynamic SQLite statistics, Cairo charts, and quick actions.
 */

#include "dashboard.h"
#include "ui/window.h"
#include "app.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#include "auth/auth.h"

typedef struct {
    GtkWidget *scroll_window;
    GtkWidget *val_students;
    GtkWidget *val_teachers;
    GtkWidget *val_staff;
    GtkWidget *val_classes;
    GtkWidget *val_attendance;
    GtkWidget *val_fee_collected;
    GtkWidget *val_fee_pending;
    GtkWidget *val_new_admissions;
    GtkWidget *admissions_box;
    GtkWidget *activities_box;
    GtkWidget *exams_box;
    GtkWidget *attendance_drawing_area;
    GtkWidget *fee_drawing_area;
    GtkWidget *qa_section;
    GtkWidget *qa_btn_student;
    GtkWidget *qa_btn_fees;
    GtkWidget *qa_btn_attendance;
    GtkWidget *qa_btn_teacher;
    GtkWidget *qa_btn_report;
} DashboardUI;

/* ── Cairo Drawing Callbacks ──────────────────────────────────────── */

static void draw_attendance_chart(GtkDrawingArea *area, cairo_t *cr, int width, int height, gpointer user_data)
{
    (void)area;
    (void)user_data;

    AttendanceChartPoint points[10];
    int count = 0;
    dashboard_service_get_weekly_attendance(points, &count);
    if (count <= 0) return;

    /* Background */
    cairo_set_source_rgb(cr, 0.98, 0.99, 1.0);
    cairo_paint(cr);

    /* Grid lines */
    cairo_set_line_width(cr, 1.0);
    cairo_set_source_rgba(cr, 0.88, 0.91, 0.95, 0.8);
    for (int y_pct = 25; y_pct <= 100; y_pct += 25) {
        double y = height - 30.0 - (y_pct / 100.0) * (height - 60.0);
        cairo_move_to(cr, 40, y);
        cairo_line_to(cr, width - 20, y);
        cairo_stroke(cr);
    }

    /* Draw Bars */
    double bar_margin = 16.0;
    double usable_width = (width - 60.0);
    double slot_width = usable_width / count;
    double bar_width = slot_width - bar_margin;

    for (int i = 0; i < count; i++) {
        double x = 45.0 + i * slot_width;
        double bar_height = (points[i].percentage / 100.0) * (height - 60.0);
        double y = height - 30.0 - bar_height;

        /* Bar gradient */
        cairo_pattern_t *pat = cairo_pattern_create_linear(x, y, x, height - 30.0);
        cairo_pattern_add_color_stop_rgb(pat, 0.0, 0.08, 0.72, 0.65); /* #14b8a6 */
        cairo_pattern_add_color_stop_rgb(pat, 1.0, 0.05, 0.58, 0.53); /* #0d9488 */
        cairo_set_source(cr, pat);

        /* Draw rounded-top rectangle */
        double radius = 4.0;
        cairo_new_sub_path(cr);
        cairo_arc(cr, x + bar_width - radius, y + radius, radius, -M_PI / 2, 0);
        cairo_line_to(cr, x + bar_width, height - 30.0);
        cairo_line_to(cr, x, height - 30.0);
        cairo_line_to(cr, x, y + radius);
        cairo_arc(cr, x + radius, y + radius, radius, -M_PI, -M_PI / 2);
        cairo_close_path(cr);
        cairo_fill(cr);
        cairo_pattern_destroy(pat);

        /* Percentage label on top of bar */
        char pct_str[16];
        snprintf(pct_str, sizeof(pct_str), "%.0f%%", points[i].percentage);
        cairo_set_source_rgb(cr, 0.12, 0.16, 0.23);
        cairo_set_font_size(cr, 11.0);
        cairo_move_to(cr, x + (bar_width / 2.0) - 10.0, y - 6.0);
        cairo_show_text(cr, pct_str);

        /* Day label below bar */
        cairo_set_source_rgb(cr, 0.45, 0.52, 0.60);
        cairo_move_to(cr, x + (bar_width / 2.0) - 10.0, height - 12.0);
        cairo_show_text(cr, points[i].label);
    }
}

static void draw_fee_trend_chart(GtkDrawingArea *area, cairo_t *cr, int width, int height, gpointer user_data)
{
    (void)area;
    (void)user_data;

    FeeChartPoint points[10];
    int count = 0;
    dashboard_service_get_fee_trends(points, &count);
    if (count <= 0) return;

    /* Background */
    cairo_set_source_rgb(cr, 0.98, 0.99, 1.0);
    cairo_paint(cr);

    /* Grid lines */
    cairo_set_line_width(cr, 1.0);
    cairo_set_source_rgba(cr, 0.88, 0.91, 0.95, 0.8);
    for (int step = 1; step <= 4; step++) {
        double y = height - 30.0 - (step / 4.0) * (height - 60.0);
        cairo_move_to(cr, 45, y);
        cairo_line_to(cr, width - 20, y);
        cairo_stroke(cr);
    }

    double max_val = 600.0; /* thousands */
    double usable_width = width - 65.0;
    double step_x = usable_width / (count - 1);

    /* Path for filled area */
    cairo_move_to(cr, 45.0, height - 30.0);
    for (int i = 0; i < count; i++) {
        double x = 45.0 + i * step_x;
        double y = height - 30.0 - (points[i].amount_thousands / max_val) * (height - 60.0);
        cairo_line_to(cr, x, y);
    }
    cairo_line_to(cr, 45.0 + (count - 1) * step_x, height - 30.0);
    cairo_close_path(cr);

    /* Gradient fill */
    cairo_pattern_t *pat = cairo_pattern_create_linear(0, 0, 0, height);
    cairo_pattern_add_color_stop_rgba(pat, 0.0, 0.23, 0.51, 0.96, 0.35); /* #3b82f6 */
    cairo_pattern_add_color_stop_rgba(pat, 1.0, 0.23, 0.51, 0.96, 0.02);
    cairo_set_source(cr, pat);
    cairo_fill_preserve(cr);
    cairo_pattern_destroy(pat);

    /* Stroke smooth line */
    cairo_set_source_rgb(cr, 0.15, 0.39, 0.92);
    cairo_set_line_width(cr, 2.5);
    cairo_stroke(cr);

    /* Draw points & labels */
    for (int i = 0; i < count; i++) {
        double x = 45.0 + i * step_x;
        double y = height - 30.0 - (points[i].amount_thousands / max_val) * (height - 60.0);

        /* Outer point circle */
        cairo_set_source_rgb(cr, 0.15, 0.39, 0.92);
        cairo_arc(cr, x, y, 4.5, 0, 2 * M_PI);
        cairo_fill(cr);

        /* Inner white dot */
        cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
        cairo_arc(cr, x, y, 2.0, 0, 2 * M_PI);
        cairo_fill(cr);

        /* Amount label */
        char amt_str[16];
        snprintf(amt_str, sizeof(amt_str), "₹%.0fk", points[i].amount_thousands);
        cairo_set_source_rgb(cr, 0.12, 0.16, 0.23);
        cairo_set_font_size(cr, 10.0);
        cairo_move_to(cr, x - 14.0, y - 8.0);
        cairo_show_text(cr, amt_str);

        /* Month label */
        cairo_set_source_rgb(cr, 0.45, 0.52, 0.60);
        cairo_move_to(cr, x - 10.0, height - 12.0);
        cairo_show_text(cr, points[i].label);
    }
}

/* ── UI Builders ──────────────────────────────────────────────────── */

static GtkWidget *make_stat_card(const char *icon_name, const char *title,
                                 const char *default_val, const char *css_class,
                                 GtkWidget **out_label)
{
    GtkWidget *card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_add_css_class(card, "dashboard-card");
    if (css_class) gtk_widget_add_css_class(card, css_class);
    gtk_widget_set_hexpand(card, TRUE);

    GtkWidget *top = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget *icon = gtk_image_new_from_icon_name(icon_name);
    gtk_image_set_pixel_size(GTK_IMAGE(icon), 24);
    gtk_widget_add_css_class(icon, "card-icon");
    gtk_box_append(GTK_BOX(top), icon);

    GtkWidget *title_lbl = gtk_label_new(title);
    gtk_widget_add_css_class(title_lbl, "card-title");
    gtk_label_set_xalign(GTK_LABEL(title_lbl), 0.0f);
    gtk_widget_set_hexpand(title_lbl, TRUE);
    gtk_box_append(GTK_BOX(top), title_lbl);
    gtk_box_append(GTK_BOX(card), top);

    GtkWidget *val_lbl = gtk_label_new(default_val);
    gtk_widget_add_css_class(val_lbl, "card-value");
    gtk_label_set_xalign(GTK_LABEL(val_lbl), 0.0f);
    gtk_box_append(GTK_BOX(card), val_lbl);

    if (out_label) *out_label = val_lbl;
    return card;
}

static void on_qa_action(GtkButton *btn, gpointer user_data)
{
    (void)btn;
    NavPage page = (NavPage)GPOINTER_TO_INT(user_data);
    school_window_navigate(page);
}

static GtkWidget *make_quick_action(const char *icon_name, const char *label, NavPage target_page)
{
    GtkWidget *btn = gtk_button_new();
    gtk_widget_add_css_class(btn, "quick-action-btn");

    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget *icon = gtk_image_new_from_icon_name(icon_name);
    gtk_image_set_pixel_size(GTK_IMAGE(icon), 18);
    gtk_box_append(GTK_BOX(box), icon);

    GtkWidget *lbl = gtk_label_new(label);
    gtk_box_append(GTK_BOX(box), lbl);
    gtk_button_set_child(GTK_BUTTON(btn), box);

    g_signal_connect(btn, "clicked", G_CALLBACK(on_qa_action), GINT_TO_POINTER((int)target_page));
    return btn;
}

static GtkWidget *make_section(const char *title, GtkWidget *child)
{
    GtkWidget *frame = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_add_css_class(frame, "dashboard-section");

    GtkWidget *lbl = gtk_label_new(NULL);
    char buf[256];
    snprintf(buf, sizeof(buf), "<span weight='bold' size='medium'>%s</span>", title);
    gtk_label_set_markup(GTK_LABEL(lbl), buf);
    gtk_label_set_xalign(GTK_LABEL(lbl), 0.0f);
    gtk_widget_add_css_class(lbl, "section-title");
    gtk_box_append(GTK_BOX(frame), lbl);

    gtk_box_append(GTK_BOX(frame), child);
    return frame;
}

/* ── Refresh Implementation ───────────────────────────────────────── */

void school_dashboard_refresh(GtkWidget *dashboard_widget)
{
    DashboardUI *ui = (DashboardUI *)g_object_get_data(G_OBJECT(dashboard_widget), "dashboard_ui");
    if (!ui) return;

    /* 1. Fetch live metrics */
    DashboardStats stats;
    dashboard_service_get_stats(&stats);

    bool logged_in      = auth_is_logged_in();
    bool can_view_stu   = logged_in && auth_current_user_can_access("STUDENTS", false);
    bool can_view_tch   = logged_in && auth_current_user_can_access("TEACHERS", false);
    bool can_view_cls   = logged_in && auth_current_user_can_access("CLASSES", false);
    bool can_view_att   = logged_in && auth_current_user_can_access("ATTENDANCE", false);
    bool can_view_fee   = logged_in && auth_current_user_can_access("FEES", false);
    bool can_view_rep   = logged_in && auth_current_user_can_access("REPORTS", false);

    bool can_edit_stu   = logged_in && auth_current_user_can_access("STUDENTS", true);
    bool can_edit_fee   = logged_in && auth_current_user_can_access("FEES", true);
    bool can_edit_att   = logged_in && auth_current_user_can_access("ATTENDANCE", true);
    bool can_edit_tch   = logged_in && auth_current_user_can_access("TEACHERS", true);

    if (ui->qa_btn_student)    gtk_widget_set_visible(ui->qa_btn_student, can_edit_stu);
    if (ui->qa_btn_fees)       gtk_widget_set_visible(ui->qa_btn_fees, can_edit_fee);
    if (ui->qa_btn_attendance) gtk_widget_set_visible(ui->qa_btn_attendance, can_edit_att);
    if (ui->qa_btn_teacher)    gtk_widget_set_visible(ui->qa_btn_teacher, can_edit_tch);
    if (ui->qa_btn_report)     gtk_widget_set_visible(ui->qa_btn_report, can_view_rep);
    if (ui->qa_section) {
        gtk_widget_set_visible(ui->qa_section,
            can_edit_stu || can_edit_fee || can_edit_att || can_edit_tch || can_view_rep);
    }

    char buf[64];
    if (can_view_stu) {
        snprintf(buf, sizeof(buf), "%d", stats.total_students);
    } else {
        snprintf(buf, sizeof(buf), "—");
    }
    gtk_label_set_text(GTK_LABEL(ui->val_students), buf);

    if (can_view_tch) {
        snprintf(buf, sizeof(buf), "%d", stats.total_teachers);
    } else {
        snprintf(buf, sizeof(buf), "—");
    }
    gtk_label_set_text(GTK_LABEL(ui->val_teachers), buf);

    if (can_view_tch) {
        snprintf(buf, sizeof(buf), "%d", stats.total_staff);
    } else {
        snprintf(buf, sizeof(buf), "—");
    }
    gtk_label_set_text(GTK_LABEL(ui->val_staff), buf);

    if (can_view_cls) {
        snprintf(buf, sizeof(buf), "%d", stats.total_classes);
    } else {
        snprintf(buf, sizeof(buf), "—");
    }
    gtk_label_set_text(GTK_LABEL(ui->val_classes), buf);

    if (!can_view_att) {
        snprintf(buf, sizeof(buf), "Restricted");
    } else if (stats.today_attendance_total > 0) {
        snprintf(buf, sizeof(buf), "%.1f%% (%d/%d)",
                 stats.today_attendance_pct,
                 stats.today_attendance_present,
                 stats.today_attendance_total);
    } else {
        snprintf(buf, sizeof(buf), "Not Marked");
    }
    gtk_label_set_text(GTK_LABEL(ui->val_attendance), buf);

    if (can_view_fee) {
        snprintf(buf, sizeof(buf), "₹%.2f", stats.fee_collection_this_month);
    } else {
        snprintf(buf, sizeof(buf), "Restricted");
    }
    gtk_label_set_text(GTK_LABEL(ui->val_fee_collected), buf);

    if (can_view_fee) {
        snprintf(buf, sizeof(buf), "₹%.2f", stats.fee_pending_total);
    } else {
        snprintf(buf, sizeof(buf), "Restricted");
    }
    gtk_label_set_text(GTK_LABEL(ui->val_fee_pending), buf);

    if (ui->fee_drawing_area) {
        gtk_widget_set_visible(ui->fee_drawing_area, can_view_fee);
    }

    if (can_view_stu) {
        snprintf(buf, sizeof(buf), "%d", stats.new_admissions_count);
    } else {
        snprintf(buf, sizeof(buf), "—");
    }
    gtk_label_set_text(GTK_LABEL(ui->val_new_admissions), buf);

    /* 2. Populate Recent Admissions */
    GtkWidget *child = gtk_widget_get_first_child(ui->admissions_box);
    while (child) {
        GtkWidget *next = gtk_widget_get_next_sibling(child);
        gtk_box_remove(GTK_BOX(ui->admissions_box), child);
        child = next;
    }

    if (!can_view_stu) {
        GtkWidget *empty = gtk_label_new("Admissions details restricted for your role.");
        gtk_widget_add_css_class(empty, "dim-label");
        gtk_box_append(GTK_BOX(ui->admissions_box), empty);
    } else {
    RecentAdmission adm[5];
    int adm_count = 0;
    dashboard_service_get_recent_admissions(adm, 5, &adm_count);
    if (adm_count == 0) {
        GtkWidget *empty = gtk_label_new("No recent admissions recorded.");
        gtk_widget_add_css_class(empty, "dim-label");
        gtk_box_append(GTK_BOX(ui->admissions_box), empty);
    } else {
        for (int i = 0; i < adm_count; i++) {
            GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
            gtk_widget_add_css_class(row, "recent-row");

            GtkWidget *icon = gtk_image_new_from_icon_name("avatar-default-symbolic");
            gtk_image_set_pixel_size(GTK_IMAGE(icon), 18);
            gtk_box_append(GTK_BOX(row), icon);

            char line[256];
            snprintf(line, sizeof(line), "%s (%s) — %s",
                     adm[i].full_name, adm[i].admission_no, adm[i].class_name);
            GtkWidget *lbl = gtk_label_new(line);
            gtk_label_set_xalign(GTK_LABEL(lbl), 0.0f);
            gtk_widget_set_hexpand(lbl, TRUE);
            gtk_box_append(GTK_BOX(row), lbl);

            GtkWidget *date_lbl = gtk_label_new(adm[i].admission_date);
            gtk_widget_add_css_class(date_lbl, "dim-label");
            gtk_box_append(GTK_BOX(row), date_lbl);

            gtk_box_append(GTK_BOX(ui->admissions_box), row);
        }
    }
    }

    /* 3. Populate Recent Activities */
    child = gtk_widget_get_first_child(ui->activities_box);
    while (child) {
        GtkWidget *next = gtk_widget_get_next_sibling(child);
        gtk_box_remove(GTK_BOX(ui->activities_box), child);
        child = next;
    }

    RecentActivity act[5];
    int act_count = 0;
    dashboard_service_get_recent_activities(act, 5, &act_count);
    if (act_count == 0) {
        GtkWidget *empty = gtk_label_new("No activity logged yet.");
        gtk_widget_add_css_class(empty, "dim-label");
        gtk_box_append(GTK_BOX(ui->activities_box), empty);
    } else {
        for (int i = 0; i < act_count; i++) {
            GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
            gtk_widget_add_css_class(row, "recent-row");

            GtkWidget *icon = gtk_image_new_from_icon_name("dialog-information-symbolic");
            gtk_image_set_pixel_size(GTK_IMAGE(icon), 18);
            gtk_box_append(GTK_BOX(row), icon);

            GtkWidget *lbl = gtk_label_new(act[i].action);
            gtk_widget_add_css_class(lbl, "badge-info");
            gtk_box_append(GTK_BOX(row), lbl);

            GtkWidget *det_lbl = gtk_label_new(act[i].details);
            gtk_label_set_xalign(GTK_LABEL(det_lbl), 0.0f);
            gtk_widget_set_hexpand(det_lbl, TRUE);
            gtk_box_append(GTK_BOX(row), det_lbl);

            GtkWidget *dt_lbl = gtk_label_new(act[i].created_at);
            gtk_widget_add_css_class(dt_lbl, "dim-label");
            gtk_box_append(GTK_BOX(row), dt_lbl);

            gtk_box_append(GTK_BOX(ui->activities_box), row);
        }
    }

    /* 4. Redraw Cairo charts */
    if (ui->attendance_drawing_area) {
        gtk_widget_queue_draw(ui->attendance_drawing_area);
    }
    if (ui->fee_drawing_area) {
        gtk_widget_queue_draw(ui->fee_drawing_area);
    }
}

/* ── Public Constructor ───────────────────────────────────────────── */

GtkWidget *school_dashboard_new(void)
{
    DashboardUI *ui = g_new0(DashboardUI, 1);

    GtkWidget *scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
                                   GTK_POLICY_AUTOMATIC,
                                   GTK_POLICY_AUTOMATIC);
    gtk_widget_set_hexpand(scroll, TRUE);
    gtk_widget_set_vexpand(scroll, TRUE);
    ui->scroll_window = scroll;

    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 20);
    gtk_widget_add_css_class(main_box, "dashboard-page");
    gtk_widget_set_margin_top(main_box, 20);
    gtk_widget_set_margin_bottom(main_box, 20);
    gtk_widget_set_margin_start(main_box, 24);
    gtk_widget_set_margin_end(main_box, 24);

    /* Welcome Banner */
    GtkWidget *welcome = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_add_css_class(welcome, "welcome-banner");

    GtkWidget *welcome_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_set_hexpand(welcome_row, TRUE);

    GtkWidget *text_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_hexpand(text_box, TRUE);

    GtkWidget *greeting = gtk_label_new(NULL);
    char greet_markup[256];
    snprintf(greet_markup, sizeof(greet_markup),
             "<span size='x-large' weight='bold'>Welcome to %s Dashboard</span>", APP_NAME);
    gtk_label_set_markup(GTK_LABEL(greeting), greet_markup);
    gtk_label_set_xalign(GTK_LABEL(greeting), 0.0f);
    gtk_box_append(GTK_BOX(text_box), greeting);

    GtkWidget *sub_greeting = gtk_label_new(
        "Real-time overview of academic performance, attendance, and administrative statistics.");
    gtk_label_set_xalign(GTK_LABEL(sub_greeting), 0.0f);
    gtk_widget_add_css_class(sub_greeting, "dim-label");
    gtk_box_append(GTK_BOX(text_box), sub_greeting);
    gtk_box_append(GTK_BOX(welcome_row), text_box);

    /* Dynamic Version Badge */
    GtkWidget *ver_badge = gtk_label_new(APP_NAME " v" APP_VERSION "  •  Active Release");
    gtk_widget_add_css_class(ver_badge, "badge-adm");
    gtk_widget_set_valign(ver_badge, GTK_ALIGN_CENTER);
    gtk_box_append(GTK_BOX(welcome_row), ver_badge);

    gtk_box_append(GTK_BOX(welcome), welcome_row);
    gtk_box_append(GTK_BOX(main_box), welcome);

    /* Stat Cards Row 1 */
    GtkWidget *cards_row1 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 16);
    gtk_box_append(GTK_BOX(cards_row1),
        make_stat_card("system-users-symbolic", "Total Students", "0", "card-blue", &ui->val_students));
    gtk_box_append(GTK_BOX(cards_row1),
        make_stat_card("avatar-default-symbolic", "Total Teachers", "0", "card-green", &ui->val_teachers));
    gtk_box_append(GTK_BOX(cards_row1),
        make_stat_card("contact-new-symbolic", "Total Staff", "0", "card-purple", &ui->val_staff));
    gtk_box_append(GTK_BOX(cards_row1),
        make_stat_card("view-list-symbolic", "Total Classes", "0", "card-orange", &ui->val_classes));
    gtk_box_append(GTK_BOX(main_box), cards_row1);

    /* Stat Cards Row 2 */
    GtkWidget *cards_row2 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 16);
    gtk_box_append(GTK_BOX(cards_row2),
        make_stat_card("document-open-recent-symbolic", "Today's Attendance", "0%", "card-teal", &ui->val_attendance));
    gtk_box_append(GTK_BOX(cards_row2),
        make_stat_card("accessories-calculator-symbolic", "Fee Collection (Month)", "₹0.00", "card-green", &ui->val_fee_collected));
    gtk_box_append(GTK_BOX(cards_row2),
        make_stat_card("dialog-warning-symbolic", "Pending Fees", "₹0.00", "card-red", &ui->val_fee_pending));
    gtk_box_append(GTK_BOX(cards_row2),
        make_stat_card("contact-new-symbolic", "New Admissions", "0", "card-blue", &ui->val_new_admissions));
    gtk_box_append(GTK_BOX(main_box), cards_row2);

    /* Quick Actions */
    GtkWidget *qa_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    ui->qa_btn_student = make_quick_action("list-add-symbolic", "Add Student", NAV_STUDENTS);
    ui->qa_btn_fees = make_quick_action("accessories-calculator-symbolic", "Collect Fees", NAV_FEES);
    ui->qa_btn_attendance = make_quick_action("document-open-recent-symbolic", "Mark Attendance", NAV_ATTENDANCE);
    ui->qa_btn_teacher = make_quick_action("avatar-default-symbolic", "Add Teacher", NAV_TEACHERS);
    ui->qa_btn_report = make_quick_action("document-print-symbolic", "Generate Report", NAV_REPORTS);

    gtk_box_append(GTK_BOX(qa_box), ui->qa_btn_student);
    gtk_box_append(GTK_BOX(qa_box), ui->qa_btn_fees);
    gtk_box_append(GTK_BOX(qa_box), ui->qa_btn_attendance);
    gtk_box_append(GTK_BOX(qa_box), ui->qa_btn_teacher);
    gtk_box_append(GTK_BOX(qa_box), ui->qa_btn_report);

    GtkWidget *qa_section = make_section("Quick Actions", qa_box);
    ui->qa_section = qa_section;
    gtk_box_append(GTK_BOX(main_box), qa_section);

    /* Charts Row with Cairo Drawing Areas */
    GtkWidget *charts_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 16);

    /* Attendance Chart */
    ui->attendance_drawing_area = gtk_drawing_area_new();
    gtk_drawing_area_set_content_width(GTK_DRAWING_AREA(ui->attendance_drawing_area), 350);
    gtk_drawing_area_set_content_height(GTK_DRAWING_AREA(ui->attendance_drawing_area), 200);
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(ui->attendance_drawing_area), draw_attendance_chart, NULL, NULL);

    GtkWidget *att_section = make_section("Weekly Attendance Trend", ui->attendance_drawing_area);
    gtk_widget_set_hexpand(att_section, TRUE);
    gtk_box_append(GTK_BOX(charts_row), att_section);

    /* Fee Collection Chart */
    ui->fee_drawing_area = gtk_drawing_area_new();
    gtk_drawing_area_set_content_width(GTK_DRAWING_AREA(ui->fee_drawing_area), 350);
    gtk_drawing_area_set_content_height(GTK_DRAWING_AREA(ui->fee_drawing_area), 200);
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(ui->fee_drawing_area), draw_fee_trend_chart, NULL, NULL);

    GtkWidget *fee_section = make_section("Fee Collection Trend (6 Months)", ui->fee_drawing_area);
    gtk_widget_set_hexpand(fee_section, TRUE);
    gtk_box_append(GTK_BOX(charts_row), fee_section);

    gtk_box_append(GTK_BOX(main_box), charts_row);

    /* Bottom Row: Recent Admissions & Activity Logs */
    GtkWidget *bottom_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 16);

    ui->admissions_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_add_css_class(ui->admissions_box, "recent-list");
    GtkWidget *adm_section = make_section("Recent Admissions", ui->admissions_box);
    gtk_widget_set_hexpand(adm_section, TRUE);
    gtk_box_append(GTK_BOX(bottom_row), adm_section);

    ui->activities_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_add_css_class(ui->activities_box, "recent-list");
    GtkWidget *act_section = make_section("Recent Activity Logs", ui->activities_box);
    gtk_widget_set_hexpand(act_section, TRUE);
    gtk_box_append(GTK_BOX(bottom_row), act_section);

    gtk_box_append(GTK_BOX(main_box), bottom_row);

    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), main_box);
    g_object_set_data_full(G_OBJECT(scroll), "dashboard_ui", ui, g_free);

    /* Initial data load */
    school_dashboard_refresh(scroll);

    return scroll;
}
