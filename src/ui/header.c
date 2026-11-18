/**
 * @file header.c
 * @brief Top header bar with search field, breadcrumb, notifications,
 *        and user profile area.
 */

#include "ui/header.h"
#include <string.h>

/* ── Module state ─────────────────────────────────────────────────── */

static GtkWidget *breadcrumb_label = NULL;

/* ── Callbacks ────────────────────────────────────────────────────── */

static void on_search_changed(GtkSearchEntry *entry, gpointer user_data)
{
    (void)user_data;
    const char *text = gtk_editable_get_text(GTK_EDITABLE(entry));
    g_print("Search: %s\n", text);
}

static void on_notification_clicked(GtkButton *btn, gpointer user_data)
{
    (void)btn;
    (void)user_data;
    g_print("Notifications clicked (not yet implemented)\n");
}

/* ── Public API ───────────────────────────────────────────────────── */

GtkWidget *school_header_new(void)
{
    /* Root container */
    GtkWidget *header = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_add_css_class(header, "top-header");

    /* ── Main header row ────────────────────────────────────────── */
    GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_add_css_class(row, "header-row");

    /* Breadcrumb / page title */
    breadcrumb_label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(breadcrumb_label),
                         "<span weight='bold' size='large'>Dashboard</span>");
    gtk_label_set_xalign(GTK_LABEL(breadcrumb_label), 0.0f);
    gtk_widget_add_css_class(breadcrumb_label, "header-breadcrumb");
    gtk_box_append(GTK_BOX(row), breadcrumb_label);

    /* Spacer */
    GtkWidget *spacer = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_hexpand(spacer, TRUE);
    gtk_box_append(GTK_BOX(row), spacer);

    /* Search entry */
    GtkWidget *search = gtk_search_entry_new();
    gtk_widget_set_size_request(search, 260, -1);
    gtk_widget_add_css_class(search, "header-search");
    g_signal_connect(search, "search-changed",
                     G_CALLBACK(on_search_changed), NULL);
    gtk_box_append(GTK_BOX(row), search);

    /* Notification bell */
    GtkWidget *notif_btn = gtk_button_new_from_icon_name("preferences-system-notifications-symbolic");
    gtk_button_set_has_frame(GTK_BUTTON(notif_btn), FALSE);
    gtk_widget_add_css_class(notif_btn, "header-icon-btn");
    g_signal_connect(notif_btn, "clicked",
                     G_CALLBACK(on_notification_clicked), NULL);
    gtk_box_append(GTK_BOX(row), notif_btn);

    /* User avatar / profile */
    GtkWidget *user_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_add_css_class(user_box, "header-user");

    GtkWidget *avatar = gtk_image_new_from_icon_name("avatar-default-symbolic");
    gtk_image_set_pixel_size(GTK_IMAGE(avatar), 28);
    gtk_box_append(GTK_BOX(user_box), avatar);

    GtkWidget *user_name = gtk_label_new("Administrator");
    gtk_widget_add_css_class(user_name, "header-username");
    gtk_box_append(GTK_BOX(user_box), user_name);

    gtk_box_append(GTK_BOX(row), user_box);

    gtk_box_append(GTK_BOX(header), row);

    /* ── Bottom separator ───────────────────────────────────────── */
    GtkWidget *sep = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_append(GTK_BOX(header), sep);

    return header;
}

void school_header_set_breadcrumb(const char *text)
{
    if (breadcrumb_label == NULL || text == NULL)
        return;

    char buf[512];
    snprintf(buf, sizeof(buf),
             "<span weight='bold' size='large'>%s</span>", text);
    gtk_label_set_markup(GTK_LABEL(breadcrumb_label), buf);
}
