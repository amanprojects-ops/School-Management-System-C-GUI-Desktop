/**
 * @file window.c
 * @brief Main application window: assembles sidebar, header, and content area.
 */

#include "ui/window.h"
#include "ui/dialog.h"
#include "ui/sidebar.h"
#include "ui/header.h"
#include "ui/dashboard.h"
#include "ui/student_view.h"
#include "ui/teacher_view.h"
#include "ui/classes_view.h"
#include "ui/attendance_view.h"
#include "ui/fees_view.h"
#include "ui/exams_view.h"
#include "ui/timetable_view.h"
#include "ui/reports_view.h"
#include "ui/settings_view.h"
#include "ui/login.h"
#include "auth/auth.h"
#include <stdio.h>

/* ── Module state ─────────────────────────────────────────────────── */

static GtkStack *root_stack = NULL;
static GtkStack *content_stack = NULL;
static GtkWidget *login_view = NULL;

/* Page names matching NavPage enum order. */
static const char *page_names[NAV_PAGE_COUNT] = {
    "dashboard",
    "students",
    "teachers",
    "classes",
    "attendance",
    "fees",
    "exams",
    "timetable",
    "reports",
    "settings"
};

/* Human-readable titles for breadcrumb. */
static const char *page_titles[NAV_PAGE_COUNT] = {
    "Dashboard",
    "Student Management",
    "Teacher & Staff",
    "Classes & Sections",
    "Attendance",
    "Fee Management",
    "Examinations",
    "Timetable",
    "Reports",
    "Settings"
};

/* ── Callbacks ────────────────────────────────────────────────────── */

static GtkWidget *status_bar_label = NULL;

const char *school_window_page_module(NavPage page)
{
    switch (page) {
        case NAV_STUDENTS:   return "STUDENTS";
        case NAV_TEACHERS:   return "TEACHERS";
        case NAV_CLASSES:    return "CLASSES";
        case NAV_ATTENDANCE: return "ATTENDANCE";
        case NAV_FEES:       return "FEES";
        case NAV_EXAMS:      return "EXAMS";
        case NAV_TIMETABLE:  return "TIMETABLE";
        case NAV_REPORTS:    return "REPORTS";
        case NAV_SETTINGS:   return "SETTINGS";
        case NAV_DASHBOARD:
        default:
            return NULL;
    }
}

static void update_status_bar_session(void)
{
    if (!status_bar_label)
        return;

    const UserSession *sess = auth_get_session();
    if (sess && sess->full_name[0] != '\0') {
        char buf[256];
        g_snprintf(buf, sizeof(buf), "APCOACHING  •  %s (%s)",
                   sess->full_name, sess->role_name);
        gtk_label_set_text(GTK_LABEL(status_bar_label), buf);
    } else {
        gtk_label_set_text(GTK_LABEL(status_bar_label), "APCOACHING  •  Not Logged In");
    }
}

static void on_login_success(gpointer user_data)
{
    (void)user_data;
    const UserSession *sess = auth_get_session();
    if (sess) {
        school_header_update_user(sess->full_name, sess->role_name);
    }
    school_sidebar_apply_permissions();
    update_status_bar_session();
    school_window_show_app();
    school_window_navigate(NAV_DASHBOARD);
}

/* ── Public API ───────────────────────────────────────────────────── */

void school_window_new(GtkApplication *app)
{
    /* ── Main window ────────────────────────────────────────────── */
    GtkWidget *window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window), "APCOACHING — School Management System");
    gtk_window_set_icon_name(GTK_WINDOW(window), "applications-education-symbolic");

    /* Auto-detect primary display monitor geometry to adapt window size */
    int screen_w = WINDOW_DEFAULT_WIDTH;
    int screen_h = WINDOW_DEFAULT_HEIGHT;

    GdkDisplay *display = gdk_display_get_default();
    if (display) {
        GListModel *monitors = gdk_display_get_monitors(display);
        if (monitors && g_list_model_get_n_items(monitors) > 0) {
            GdkMonitor *mon = GDK_MONITOR(g_list_model_get_item(monitors, 0));
            if (mon) {
                GdkRectangle geom;
                gdk_monitor_get_geometry(mon, &geom);
                if (geom.width > 0 && geom.height > 0) {
                    screen_w = geom.width;
                    screen_h = geom.height;
                }
                g_object_unref(mon);
            }
        }
    }

    /* Auto-adjust default dimensions to fit comfortably inside screen workspace:
     * - Leave margins for taskbar, desktop padding, and titlebars
     * - Clamp width between WINDOW_MIN_WIDTH (640) and 1280
     * - Clamp height between WINDOW_MIN_HEIGHT (450) and 760
     */
    int auto_width = (int)(screen_w * 0.88);
    int auto_height = (int)(screen_h * 0.82);

    if (auto_width > 1280) auto_width = 1280;
    if (auto_width < WINDOW_MIN_WIDTH) auto_width = WINDOW_MIN_WIDTH;

    if (auto_height > 760) auto_height = 720;
    if (auto_height < WINDOW_MIN_HEIGHT) auto_height = WINDOW_MIN_HEIGHT;

    gtk_window_set_default_size(GTK_WINDOW(window), auto_width, auto_height);
    gtk_widget_set_size_request(window, WINDOW_MIN_WIDTH, WINDOW_MIN_HEIGHT);
    gtk_window_set_resizable(GTK_WINDOW(window), TRUE);

    /* Maximize on launch for full immersive layout while allowing clean Restore Down */
    gtk_window_maximize(GTK_WINDOW(window));
    gtk_widget_add_css_class(window, "main-window");

    /* ── Outer Root Stack wrapped in Modal Overlay Host ─────────── */
    root_stack = GTK_STACK(gtk_stack_new());
    gtk_stack_set_transition_type(root_stack, GTK_STACK_TRANSITION_TYPE_CROSSFADE);
    gtk_stack_set_transition_duration(root_stack, 250);
    gtk_stack_set_hhomogeneous(root_stack, FALSE);
    gtk_stack_set_vhomogeneous(root_stack, FALSE);

    GtkWidget *main_overlay = gtk_overlay_new();
    gtk_overlay_set_child(GTK_OVERLAY(main_overlay), GTK_WIDGET(root_stack));
    gtk_window_set_child(GTK_WINDOW(window), main_overlay);
    school_dialog_init(GTK_WINDOW(window), GTK_OVERLAY(main_overlay), GTK_WIDGET(root_stack));

    /* 1. Login View */
    login_view = school_login_view_new(on_login_success, NULL);
    gtk_stack_add_named(root_stack, login_view, "login");

    /* 2. Main App Shell Layout */
    GtkWidget *app_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_hexpand(app_box, TRUE);
    gtk_widget_set_vexpand(app_box, TRUE);

    /* Sidebar (strictly fixed width, no horizontal expansion) */
    GtkWidget *sidebar = school_sidebar_new();
    gtk_widget_set_hexpand(sidebar, FALSE);
    gtk_box_append(GTK_BOX(app_box), sidebar);

    /* Right pane: header on top, content below */
    GtkWidget *right_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_hexpand(right_box, TRUE);
    gtk_widget_set_vexpand(right_box, TRUE);
    gtk_box_append(GTK_BOX(app_box), right_box);

    /* Header */
    GtkWidget *header = school_header_new();
    gtk_box_append(GTK_BOX(right_box), header);

    /* ── Content stack ──────────────────────────────────────────── */
    content_stack = GTK_STACK(gtk_stack_new());
    gtk_stack_set_transition_type(content_stack, GTK_STACK_TRANSITION_TYPE_CROSSFADE);
    gtk_stack_set_transition_duration(content_stack, 200);
    gtk_stack_set_hhomogeneous(content_stack, FALSE);
    gtk_stack_set_vhomogeneous(content_stack, FALSE);
    gtk_widget_set_hexpand(GTK_WIDGET(content_stack), TRUE);
    gtk_widget_set_vexpand(GTK_WIDGET(content_stack), TRUE);
    gtk_box_append(GTK_BOX(right_box), GTK_WIDGET(content_stack));

    /* Add pages ── dashboard, students, teachers are real, the rest are placeholders. */
    GtkWidget *dashboard_page = school_dashboard_new();
    gtk_stack_add_named(content_stack, dashboard_page, page_names[NAV_DASHBOARD]);

    GtkWidget *students_page = school_student_view_new();
    gtk_stack_add_named(content_stack, students_page, page_names[NAV_STUDENTS]);

    GtkWidget *teachers_page = school_teacher_view_new();
    gtk_stack_add_named(content_stack, teachers_page, page_names[NAV_TEACHERS]);

    GtkWidget *classes_page = school_classes_view_new();
    gtk_stack_add_named(content_stack, classes_page, page_names[NAV_CLASSES]);

    GtkWidget *attendance_page = school_attendance_view_new();
    gtk_stack_add_named(content_stack, attendance_page, page_names[NAV_ATTENDANCE]);

    GtkWidget *fees_page = school_fees_view_new();
    gtk_stack_add_named(content_stack, fees_page, page_names[NAV_FEES]);

    GtkWidget *exams_page = school_exams_view_new();
    gtk_stack_add_named(content_stack, exams_page, page_names[NAV_EXAMS]);

    GtkWidget *timetable_page = school_timetable_view_new();
    gtk_stack_add_named(content_stack, timetable_page, page_names[NAV_TIMETABLE]);

    GtkWidget *reports_page = school_reports_view_new();
    gtk_stack_add_named(content_stack, reports_page, page_names[NAV_REPORTS]);

    GtkWidget *settings_page = school_settings_view_new();
    gtk_stack_add_named(content_stack, settings_page, page_names[NAV_SETTINGS]);

    /* ── Status bar ─────────────────────────────────────────────── */
    GtkWidget *status_bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_add_css_class(status_bar, "status-bar");

    GtkWidget *status_label = gtk_label_new("APCOACHING v2.0.0  •  Authenticated");
    status_bar_label = status_label;
    gtk_widget_set_halign(status_label, GTK_ALIGN_START);
    gtk_widget_set_hexpand(status_label, TRUE);
    gtk_label_set_ellipsize(GTK_LABEL(status_label), PANGO_ELLIPSIZE_END);
    gtk_box_append(GTK_BOX(status_bar), status_label);

    GtkWidget *status_right = gtk_label_new("School Management System  •  Pure C11 & GTK4");
    gtk_widget_set_halign(status_right, GTK_ALIGN_END);
    gtk_label_set_ellipsize(GTK_LABEL(status_right), PANGO_ELLIPSIZE_END);
    gtk_box_append(GTK_BOX(status_bar), status_right);

    gtk_box_append(GTK_BOX(right_box), status_bar);

    gtk_stack_add_named(root_stack, app_box, "app");

    school_sidebar_apply_permissions();

    /* Initial state: if authenticated show app, else show login */
    if (auth_is_logged_in()) {
        const UserSession *sess = auth_get_session();
        if (sess) {
            school_header_update_user(sess->full_name, sess->role_name);
        }
        update_status_bar_session();
        school_window_show_app();
    } else {
        update_status_bar_session();
        school_window_show_login();
    }

    /* Show */
    gtk_window_present(GTK_WINDOW(window));
}

void school_window_show_login(void)
{
    school_dialog_close_all();
    if (root_stack && login_view) {
        school_login_view_reset(login_view);
        gtk_stack_set_visible_child_name(root_stack, "login");
    }
}

void school_window_show_app(void)
{
    if (root_stack) {
        gtk_stack_set_visible_child_name(root_stack, "app");
    }
}

static void show_access_denied_dialog(const char *page_title)
{
    GtkWidget *dlg = school_dialog_new("Access Denied", 420, 210);
    if (!dlg)
        return;

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 14);
    gtk_widget_set_margin_start(vbox, 20);
    gtk_widget_set_margin_end(vbox, 20);
    gtk_widget_set_margin_top(vbox, 18);
    gtk_widget_set_margin_bottom(vbox, 18);

    const UserSession *sess = auth_get_session();
    const char *role = (sess && sess->role_name[0] != '\0') ? sess->role_name : "Current role";
    char msg[256];
    g_snprintf(msg, sizeof(msg),
               "%s does not have permission to view \"%s\".\n"
               "Ask an administrator to grant view access in Settings.",
               role, page_title ? page_title : "this module");

    GtkWidget *lbl = gtk_label_new(msg);
    gtk_label_set_wrap(GTK_LABEL(lbl), TRUE);
    gtk_label_set_xalign(GTK_LABEL(lbl), 0.0f);
    gtk_box_append(GTK_BOX(vbox), lbl);

    GtkWidget *btn_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_halign(btn_box, GTK_ALIGN_END);
    GtkWidget *ok_btn = gtk_button_new_with_label("OK");
    gtk_widget_add_css_class(ok_btn, "btn-primary");
    g_signal_connect_swapped(ok_btn, "clicked", G_CALLBACK(school_dialog_close), dlg);
    gtk_box_append(GTK_BOX(btn_box), ok_btn);
    gtk_box_append(GTK_BOX(vbox), btn_box);

    school_dialog_set_child(dlg, vbox);
    school_dialog_present(dlg);
}

void school_window_navigate(NavPage page)
{
    if (page < 0 || page >= NAV_PAGE_COUNT || content_stack == NULL)
        return;

    const char *mod = school_window_page_module(page);
    if (mod != NULL && (!auth_is_logged_in() || !auth_current_user_can_access(mod, false))) {
        if (auth_is_logged_in()) {
            show_access_denied_dialog(page_titles[page]);
        }
        if (page != NAV_DASHBOARD) {
            school_window_navigate(NAV_DASHBOARD);
        }
        return;
    }

    gtk_stack_set_visible_child_name(content_stack, page_names[page]);
    school_header_set_breadcrumb(page_titles[page]);
    school_sidebar_set_active((int)page);

    GtkWidget *target = gtk_stack_get_child_by_name(content_stack, page_names[page]);
    if (target) {
        switch (page) {
            case NAV_DASHBOARD:
                school_dashboard_refresh(target);
                break;
            case NAV_STUDENTS:
                school_student_view_reload(target);
                break;
            case NAV_TEACHERS:
                school_teacher_view_reload(target);
                break;
            case NAV_CLASSES:
                school_classes_view_reload(target);
                break;
            case NAV_ATTENDANCE:
                school_attendance_view_reload(target);
                break;
            case NAV_FEES:
                school_fees_view_reload(target);
                break;
            case NAV_EXAMS:
                school_exams_view_reload(target);
                break;
            case NAV_TIMETABLE:
                school_timetable_view_reload(target);
                break;
            case NAV_REPORTS:
                school_reports_view_reload(target);
                break;
            case NAV_SETTINGS:
                school_settings_view_reload(target);
                break;
            default:
                break;
        }
    }
}

GtkStack *school_window_get_content_stack(void)
{
    return content_stack;
}
