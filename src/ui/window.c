/**
 * @file window.c
 * @brief Main application window: assembles sidebar, header, and content area.
 */

#include "ui/window.h"
#include "ui/sidebar.h"
#include "ui/header.h"
#include "ui/dashboard.h"
#include <stdio.h>

/* ── Module state ─────────────────────────────────────────────────── */

static GtkStack *content_stack = NULL;

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

    /* ── Root horizontal layout: sidebar | right pane ───────────── */
    GtkWidget *root_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_window_set_child(GTK_WINDOW(window), root_box);

    /* Sidebar */
    GtkWidget *sidebar = school_sidebar_new();
    gtk_box_append(GTK_BOX(root_box), sidebar);

    /* Right pane: header on top, content below */
    GtkWidget *right_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_hexpand(right_box, TRUE);
    gtk_widget_set_vexpand(right_box, TRUE);
    gtk_box_append(GTK_BOX(root_box), right_box);

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

    /* Add pages ── dashboard is real, the rest are placeholders. */
    GtkWidget *dashboard_page = school_dashboard_new();
    gtk_stack_add_named(content_stack, dashboard_page, page_names[NAV_DASHBOARD]);

    for (int i = 1; i < NAV_PAGE_COUNT; i++) {
        GtkWidget *ph = make_placeholder_page(page_titles[i]);
        gtk_stack_add_named(content_stack, ph, page_names[i]);
    }

    /* Start on the dashboard */
    gtk_stack_set_visible_child_name(content_stack, "dashboard");
    school_sidebar_set_active(NAV_DASHBOARD);

    /* ── Status bar ─────────────────────────────────────────────── */
    GtkWidget *status_bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_add_css_class(status_bar, "status-bar");

    GtkWidget *status_label = gtk_label_new("SchoolCMS v1.0.0  •  Ready");
    gtk_widget_set_halign(status_label, GTK_ALIGN_START);
    gtk_widget_set_hexpand(status_label, TRUE);
    gtk_box_append(GTK_BOX(status_bar), status_label);

    GtkWidget *status_right = gtk_label_new("Phase 1 — Application Shell");
    gtk_widget_set_halign(status_right, GTK_ALIGN_END);
    gtk_box_append(GTK_BOX(status_bar), status_right);

    gtk_box_append(GTK_BOX(right_box), status_bar);

    /* Show */
    gtk_window_present(GTK_WINDOW(window));
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
