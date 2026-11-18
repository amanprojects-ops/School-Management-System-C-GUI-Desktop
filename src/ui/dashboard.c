/**
 * @file dashboard.c
 * @brief Dashboard page — stat cards, quick-action buttons, and
 *        placeholder sections for charts and recent data.
 *
 * In Phase 1 all values are static placeholders. Phase 2+
 * will wire these to live SQLite queries.
 */

#include "ui/dashboard.h"
#include <stdio.h>

/* ── Helpers ──────────────────────────────────────────────────────── */

/**
 * Create a single dashboard stat card.
 *
 * @param icon_name  GTK symbolic icon.
 * @param title      e.g. "Total Students".
 * @param value      e.g. "1,245".
 * @param css_class  Extra CSS class for colour coding.
 */
static GtkWidget *make_stat_card(const char *icon_name,
                                  const char *title,
                                  const char *value,
                                  const char *css_class)
{
    GtkWidget *card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_add_css_class(card, "dashboard-card");
    if (css_class)
        gtk_widget_add_css_class(card, css_class);
    gtk_widget_set_hexpand(card, TRUE);

    /* Top row: icon + title */
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

    /* Value */
    GtkWidget *val_lbl = gtk_label_new(value);
    gtk_widget_add_css_class(val_lbl, "card-value");
    gtk_label_set_xalign(GTK_LABEL(val_lbl), 0.0f);
    gtk_box_append(GTK_BOX(card), val_lbl);

    return card;
}

/**
 * Create a quick-action button.
 */
static GtkWidget *make_quick_action(const char *icon_name, const char *label)
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

    return btn;
}

/**
 * Create a section with a title and a child widget.
 */
static GtkWidget *make_section(const char *title, GtkWidget *child)
{
    GtkWidget *frame = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_add_css_class(frame, "dashboard-section");

    GtkWidget *lbl = gtk_label_new(NULL);
    char buf[256];
    snprintf(buf, sizeof(buf),
             "<span weight='bold' size='medium'>%s</span>", title);
    gtk_label_set_markup(GTK_LABEL(lbl), buf);
    gtk_label_set_xalign(GTK_LABEL(lbl), 0.0f);
    gtk_widget_add_css_class(lbl, "section-title");
    gtk_box_append(GTK_BOX(frame), lbl);

    gtk_box_append(GTK_BOX(frame), child);

    return frame;
}

/* ── Placeholder content builders ─────────────────────────────────── */

static GtkWidget *make_chart_placeholder(const char *text)
{
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_set_size_request(box, -1, 200);
    gtk_widget_add_css_class(box, "chart-placeholder");
    gtk_widget_set_halign(box, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(box, GTK_ALIGN_CENTER);
    gtk_widget_set_hexpand(box, TRUE);

    GtkWidget *icon = gtk_image_new_from_icon_name("utilities-system-monitor-symbolic");
    gtk_image_set_pixel_size(GTK_IMAGE(icon), 48);
    gtk_widget_add_css_class(icon, "dim-label");
    gtk_box_append(GTK_BOX(box), icon);

    GtkWidget *lbl = gtk_label_new(text);
    gtk_widget_add_css_class(lbl, "dim-label");
    gtk_box_append(GTK_BOX(box), lbl);

    return box;
}

static GtkWidget *make_recent_admissions_placeholder(void)
{
    /* Simple list / table placeholder */
    const char *names[] = {
        "Aarav Sharma — Class 10A — 15 Nov 2026",
        "Priya Patel — Class 8B — 12 Nov 2026",
        "Rahul Singh — Class 6C — 10 Nov 2026",
        "Ananya Gupta — Class 9A — 8 Nov 2026",
        "Vikram Reddy — Class 7B — 5 Nov 2026",
    };

    GtkWidget *list = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_add_css_class(list, "recent-list");

    for (int i = 0; i < 5; i++) {
        GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
        gtk_widget_add_css_class(row, "recent-row");

        GtkWidget *icon = gtk_image_new_from_icon_name("avatar-default-symbolic");
        gtk_image_set_pixel_size(GTK_IMAGE(icon), 20);
        gtk_box_append(GTK_BOX(row), icon);

        GtkWidget *lbl = gtk_label_new(names[i]);
        gtk_label_set_xalign(GTK_LABEL(lbl), 0.0f);
        gtk_widget_set_hexpand(lbl, TRUE);
        gtk_box_append(GTK_BOX(row), lbl);

        gtk_box_append(GTK_BOX(list), row);
    }

    return list;
}

static GtkWidget *make_upcoming_exams_placeholder(void)
{
    const char *exams[] = {
        "Mid-Term Examination — 25 Nov 2026",
        "Science Practical — 28 Nov 2026",
        "Annual Sports Day — 5 Dec 2026",
    };

    GtkWidget *list = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_add_css_class(list, "recent-list");

    for (int i = 0; i < 3; i++) {
        GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
        gtk_widget_add_css_class(row, "recent-row");

        GtkWidget *icon = gtk_image_new_from_icon_name("x-office-calendar-symbolic");
        gtk_image_set_pixel_size(GTK_IMAGE(icon), 20);
        gtk_box_append(GTK_BOX(row), icon);

        GtkWidget *lbl = gtk_label_new(exams[i]);
        gtk_label_set_xalign(GTK_LABEL(lbl), 0.0f);
        gtk_widget_set_hexpand(lbl, TRUE);
        gtk_box_append(GTK_BOX(row), lbl);

        gtk_box_append(GTK_BOX(list), row);
    }

    return list;
}

/* ── Public API ───────────────────────────────────────────────────── */

GtkWidget *school_dashboard_new(void)
{
    /* Scrollable wrapper */
    GtkWidget *scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
                                   GTK_POLICY_NEVER,
                                   GTK_POLICY_AUTOMATIC);
    gtk_widget_set_hexpand(scroll, TRUE);
    gtk_widget_set_vexpand(scroll, TRUE);

    /* Main vertical container */
    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 20);
    gtk_widget_add_css_class(main_box, "dashboard-page");
    gtk_widget_set_margin_top(main_box, 20);
    gtk_widget_set_margin_bottom(main_box, 20);
    gtk_widget_set_margin_start(main_box, 24);
    gtk_widget_set_margin_end(main_box, 24);

    /* ── Welcome banner ─────────────────────────────────────────── */
    GtkWidget *welcome = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_add_css_class(welcome, "welcome-banner");

    GtkWidget *greeting = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(greeting),
        "<span size='x-large' weight='bold'>Welcome back, Administrator!</span>");
    gtk_label_set_xalign(GTK_LABEL(greeting), 0.0f);
    gtk_box_append(GTK_BOX(welcome), greeting);

    GtkWidget *sub_greeting = gtk_label_new(
        "Here's what's happening at your school today.");
    gtk_label_set_xalign(GTK_LABEL(sub_greeting), 0.0f);
    gtk_widget_add_css_class(sub_greeting, "dim-label");
    gtk_box_append(GTK_BOX(welcome), sub_greeting);

    gtk_box_append(GTK_BOX(main_box), welcome);

    /* ── Stat cards — row 1 ─────────────────────────────────────── */
    GtkWidget *cards_row1 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 16);
    gtk_box_append(GTK_BOX(cards_row1),
        make_stat_card("system-users-symbolic",   "Total Students",   "1,245",  "card-blue"));
    gtk_box_append(GTK_BOX(cards_row1),
        make_stat_card("avatar-default-symbolic",  "Total Teachers",   "68",     "card-green"));
    gtk_box_append(GTK_BOX(cards_row1),
        make_stat_card("contact-new-symbolic",     "Total Staff",      "32",     "card-purple"));
    gtk_box_append(GTK_BOX(cards_row1),
        make_stat_card("view-list-symbolic",       "Total Classes",    "42",     "card-orange"));
    gtk_box_append(GTK_BOX(main_box), cards_row1);

    /* ── Stat cards — row 2 ─────────────────────────────────────── */
    GtkWidget *cards_row2 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 16);
    gtk_box_append(GTK_BOX(cards_row2),
        make_stat_card("document-open-recent-symbolic", "Today's Attendance", "92%", "card-teal"));
    gtk_box_append(GTK_BOX(cards_row2),
        make_stat_card("wallet-symbolic",          "Fee Collection",   "₹4,52,000", "card-green"));
    gtk_box_append(GTK_BOX(cards_row2),
        make_stat_card("dialog-warning-symbolic",  "Pending Fees",     "₹1,28,500", "card-red"));
    gtk_box_append(GTK_BOX(cards_row2),
        make_stat_card("contact-new-symbolic",     "New Admissions",   "18",        "card-blue"));
    gtk_box_append(GTK_BOX(main_box), cards_row2);

    /* ── Quick Actions ──────────────────────────────────────────── */
    GtkWidget *qa_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_box_append(GTK_BOX(qa_box),
        make_quick_action("list-add-symbolic",           "Add Student"));
    gtk_box_append(GTK_BOX(qa_box),
        make_quick_action("wallet-symbolic",             "Collect Fees"));
    gtk_box_append(GTK_BOX(qa_box),
        make_quick_action("document-open-recent-symbolic","Mark Attendance"));
    gtk_box_append(GTK_BOX(qa_box),
        make_quick_action("avatar-default-symbolic",     "Add Teacher"));
    gtk_box_append(GTK_BOX(qa_box),
        make_quick_action("document-print-symbolic",     "Generate Report"));

    GtkWidget *qa_section = make_section("Quick Actions", qa_box);
    gtk_box_append(GTK_BOX(main_box), qa_section);

    /* ── Charts row ─────────────────────────────────────────────── */
    GtkWidget *charts_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 16);

    GtkWidget *att_chart = make_section("Attendance Overview",
        make_chart_placeholder("Attendance chart will appear here\n(requires Cairo — Phase 4+)"));
    gtk_widget_set_hexpand(att_chart, TRUE);
    gtk_box_append(GTK_BOX(charts_row), att_chart);

    GtkWidget *fee_chart = make_section("Fee Collection",
        make_chart_placeholder("Fee collection chart will appear here\n(requires Cairo — Phase 9+)"));
    gtk_widget_set_hexpand(fee_chart, TRUE);
    gtk_box_append(GTK_BOX(charts_row), fee_chart);

    gtk_box_append(GTK_BOX(main_box), charts_row);

    /* ── Bottom row: Recent Admissions + Upcoming Exams ─────────── */
    GtkWidget *bottom_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 16);

    GtkWidget *admissions = make_section("Recent Admissions",
                                          make_recent_admissions_placeholder());
    gtk_widget_set_hexpand(admissions, TRUE);
    gtk_box_append(GTK_BOX(bottom_row), admissions);

    GtkWidget *exams = make_section("Upcoming Examinations",
                                     make_upcoming_exams_placeholder());
    gtk_widget_set_hexpand(exams, TRUE);
    gtk_box_append(GTK_BOX(bottom_row), exams);

    gtk_box_append(GTK_BOX(main_box), bottom_row);

    /* Put it all inside the scroll window */
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), main_box);

    return scroll;
}
