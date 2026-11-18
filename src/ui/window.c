/**
 * @file window.c
 * @brief Main application window: assembles sidebar, header, and content area.
 */

#include "ui/window.h"
#include "ui/sidebar.h"
#include "ui/header.h"
#include "ui/dashboard.h"
#include "ui/student_view.h"
#include "ui/teacher_view.h"
#include "ui/classes_view.h"
#include "ui/attendance_view.h"
#include "ui/fees_view.h"
#include "ui/exams_view.h"
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

static void on_login_success(gpointer user_data)
{
    (void)user_data;
    const UserSession *sess = auth_get_session();
    if (sess) {
        school_header_update_user(sess->full_name, sess->role_name);
    }
    school_window_show_app();
    school_window_navigate(NAV_DASHBOARD);
}

/* ── Placeholder page builder ─────────────────────────────────────── */

/**
 * Build a simple centred label as a placeholder for modules
 * that have not been implemented yet.
 */
static GtkWidget *make_placeholder_page(const char *title)
{
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_set_hexpand(box, TRUE);
    gtk_widget_set_vexpand(box, TRUE);
    gtk_widget_set_halign(box, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(box, GTK_ALIGN_CENTER);

    GtkWidget *icon = gtk_image_new_from_icon_name("applications-system-symbolic");
    gtk_image_set_pixel_size(GTK_IMAGE(icon), 64);
    gtk_widget_add_css_class(icon, "dim-label");
    gtk_box_append(GTK_BOX(box), icon);

    char buf[256];
    snprintf(buf, sizeof(buf), "<span size='x-large' weight='bold'>%s</span>", title);
    GtkWidget *label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), buf);
    gtk_box_append(GTK_BOX(box), label);

    GtkWidget *sub = gtk_label_new("This module will be implemented in a future phase.");
    gtk_widget_add_css_class(sub, "dim-label");
    gtk_box_append(GTK_BOX(box), sub);

    return box;
}

/* ── Public API ───────────────────────────────────────────────────── */

void school_window_new(GtkApplication *app)
{
    /* ── Main window ────────────────────────────────────────────── */
    GtkWidget *window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window), "SchoolCMS — School Management System");
    gtk_window_set_default_size(GTK_WINDOW(window),
                                WINDOW_DEFAULT_WIDTH,
                                WINDOW_DEFAULT_HEIGHT);
    gtk_widget_set_size_request(window, WINDOW_MIN_WIDTH, WINDOW_MIN_HEIGHT);
    gtk_widget_add_css_class(window, "main-window");

    /* ── Outer Root Stack (switches between Login and Main App Shell) ── */
    root_stack = GTK_STACK(gtk_stack_new());
    gtk_stack_set_transition_type(root_stack, GTK_STACK_TRANSITION_TYPE_CROSSFADE);
    gtk_stack_set_transition_duration(root_stack, 250);
    gtk_window_set_child(GTK_WINDOW(window), GTK_WIDGET(root_stack));

    /* 1. Login View */
    login_view = school_login_view_new(on_login_success, NULL);
    gtk_stack_add_named(root_stack, login_view, "login");

    /* 2. Main App Shell Layout */
    GtkWidget *app_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_hexpand(app_box, TRUE);
    gtk_widget_set_vexpand(app_box, TRUE);

    /* Sidebar */
    GtkWidget *sidebar = school_sidebar_new();
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

    for (int i = 7; i < NAV_PAGE_COUNT; i++) {
        GtkWidget *ph = make_placeholder_page(page_titles[i]);
        gtk_stack_add_named(content_stack, ph, page_names[i]);
    }

    /* ── Status bar ─────────────────────────────────────────────── */
    GtkWidget *status_bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_add_css_class(status_bar, "status-bar");

    GtkWidget *status_label = gtk_label_new("SchoolCMS v1.0.0  •  Authenticated");
    gtk_widget_set_halign(status_label, GTK_ALIGN_START);
    gtk_widget_set_hexpand(status_label, TRUE);
    gtk_box_append(GTK_BOX(status_bar), status_label);

    GtkWidget *status_right = gtk_label_new("Phase 3 — Authentication");
    gtk_widget_set_halign(status_right, GTK_ALIGN_END);
    gtk_box_append(GTK_BOX(status_bar), status_right);

    gtk_box_append(GTK_BOX(right_box), status_bar);

    gtk_stack_add_named(root_stack, app_box, "app");

    /* Initial state: if authenticated show app, else show login */
    if (auth_is_logged_in()) {
        school_window_show_app();
    } else {
        school_window_show_login();
    }

    /* Show */
    gtk_window_present(GTK_WINDOW(window));
}

void school_window_show_login(void)
{
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

void school_window_navigate(NavPage page)
{
    if (page < 0 || page >= NAV_PAGE_COUNT || content_stack == NULL)
        return;

    gtk_stack_set_visible_child_name(content_stack, page_names[page]);
    school_header_set_breadcrumb(page_titles[page]);
    school_sidebar_set_active((int)page);
}

GtkStack *school_window_get_content_stack(void)
{
    return content_stack;
}
