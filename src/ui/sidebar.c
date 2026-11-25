/**
 * @file sidebar.c
 * @brief Left sidebar navigation with icons, labels, and active-state highlighting.
 */

#include "ui/sidebar.h"
#include "ui/window.h"
#include <string.h>

/* ── Sidebar item definition ──────────────────────────────────────── */

typedef struct {
    const char *icon_name;   /* GTK symbolic icon */
    const char *label;       /* Display text      */
    NavPage     page;        /* Target page       */
} SidebarItem;

static const SidebarItem sidebar_items[] = {
    { "view-grid-symbolic",           "Dashboard",   NAV_DASHBOARD  },
    { "system-users-symbolic",        "Students",    NAV_STUDENTS   },
    { "avatar-default-symbolic",      "Teachers",    NAV_TEACHERS   },
    { "view-list-symbolic",           "Classes",     NAV_CLASSES    },
    { "document-open-recent-symbolic","Attendance",  NAV_ATTENDANCE },
    { "accessories-calculator-symbolic", "Fees",     NAV_FEES       },
    { "document-edit-symbolic",       "Exams",       NAV_EXAMS      },
    { "x-office-calendar-symbolic",   "Timetable",   NAV_TIMETABLE  },
    { "document-print-symbolic",      "Reports",     NAV_REPORTS    },
    { "emblem-system-symbolic",       "Settings",    NAV_SETTINGS   },
};

#define SIDEBAR_ITEM_COUNT (sizeof(sidebar_items) / sizeof(sidebar_items[0]))

/* ── Module state ─────────────────────────────────────────────────── */

static GtkWidget *nav_buttons[NAV_PAGE_COUNT];

/* ── Callbacks ────────────────────────────────────────────────────── */

static void on_nav_clicked(GtkButton *button, gpointer user_data)
{
    NavPage page = (NavPage)GPOINTER_TO_INT(user_data);
    school_window_navigate(page);
}

/* ── Public API ───────────────────────────────────────────────────── */

GtkWidget *school_sidebar_new(void)
{
    /* Outer container */
    GtkWidget *sidebar = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_add_css_class(sidebar, "sidebar");
    gtk_widget_set_size_request(sidebar, SIDEBAR_WIDTH, -1);
    gtk_widget_set_hexpand(sidebar, FALSE);
    gtk_widget_set_halign(sidebar, GTK_ALIGN_START);

    /* ── Brand area ─────────────────────────────────────────────── */
    GtkWidget *brand_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_add_css_class(brand_box, "sidebar-brand");

    GtkWidget *logo_icon = gtk_image_new_from_file("assets/images/app_logo.png");
    gtk_image_set_pixel_size(GTK_IMAGE(logo_icon), 32);
    gtk_widget_add_css_class(logo_icon, "sidebar-logo");
    gtk_box_append(GTK_BOX(brand_box), logo_icon);

    GtkWidget *brand_label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(brand_label),
                         "<span weight='bold' size='large'>APCOACHING</span>");
    gtk_widget_add_css_class(brand_label, "sidebar-brand-text");
    gtk_box_append(GTK_BOX(brand_box), brand_label);

    gtk_box_append(GTK_BOX(sidebar), brand_box);

    /* ── Separator ──────────────────────────────────────────────── */
    GtkWidget *sep = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_widget_add_css_class(sep, "sidebar-separator");
    gtk_box_append(GTK_BOX(sidebar), sep);

    /* ── Navigation items (scrollable vertically, strictly fixed width) ── */
    GtkWidget *nav_scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(nav_scroll),
                                   GTK_POLICY_NEVER,
                                   GTK_POLICY_AUTOMATIC);
    gtk_widget_set_vexpand(nav_scroll, TRUE);
    gtk_widget_set_hexpand(nav_scroll, FALSE);

    GtkWidget *nav_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_widget_add_css_class(nav_box, "sidebar-nav");
    gtk_widget_set_vexpand(nav_box, TRUE);
    gtk_widget_set_margin_top(nav_box, 8);
    gtk_widget_set_margin_start(nav_box, 8);
    gtk_widget_set_margin_end(nav_box, 8);

    for (size_t i = 0; i < SIDEBAR_ITEM_COUNT; i++) {
        const SidebarItem *item = &sidebar_items[i];

        GtkWidget *btn = gtk_button_new();
        gtk_widget_add_css_class(btn, "sidebar-item");
        gtk_button_set_has_frame(GTK_BUTTON(btn), FALSE);

        /* Icon + label inside the button */
        GtkWidget *btn_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);

        GtkWidget *icon = gtk_image_new_from_icon_name(item->icon_name);
        gtk_image_set_pixel_size(GTK_IMAGE(icon), 20);
        gtk_box_append(GTK_BOX(btn_box), icon);

        GtkWidget *lbl = gtk_label_new(item->label);
        gtk_label_set_xalign(GTK_LABEL(lbl), 0.0f);
        gtk_widget_set_hexpand(lbl, TRUE);
        gtk_box_append(GTK_BOX(btn_box), lbl);

        gtk_button_set_child(GTK_BUTTON(btn), btn_box);

        g_signal_connect(btn, "clicked",
                         G_CALLBACK(on_nav_clicked),
                         GINT_TO_POINTER((int)item->page));

        gtk_box_append(GTK_BOX(nav_box), btn);
        nav_buttons[item->page] = btn;
    }

    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(nav_scroll), nav_box);
    gtk_box_append(GTK_BOX(sidebar), nav_scroll);

    /* ── Bottom section: version info ───────────────────────────── */
    GtkWidget *bottom = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_add_css_class(bottom, "sidebar-bottom");

    GtkWidget *ver_sep = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_append(GTK_BOX(bottom), ver_sep);

    GtkWidget *ver_label = gtk_label_new("APCOACHING v1.1.6");
    gtk_widget_add_css_class(ver_label, "sidebar-version");
    gtk_box_append(GTK_BOX(bottom), ver_label);

    gtk_box_append(GTK_BOX(sidebar), bottom);

    return sidebar;
}

void school_sidebar_set_active(int page_index)
{
    for (int i = 0; i < NAV_PAGE_COUNT; i++) {
        if (nav_buttons[i] == NULL)
            continue;
        if (i == page_index) {
            gtk_widget_add_css_class(nav_buttons[i], "sidebar-item-active");
        } else {
            gtk_widget_remove_css_class(nav_buttons[i], "sidebar-item-active");
        }
    }
}
