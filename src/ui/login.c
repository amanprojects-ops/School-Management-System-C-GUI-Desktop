/**
 * @file login.c
 * @brief Modern login view widget implementation.
 */

#include "ui/login.h"
#include "auth/auth.h"
#include <string.h>

typedef struct {
    GtkWidget *username_entry;
    GtkWidget *password_entry;
    GtkWidget *error_label;
    GtkWidget *submit_btn;
    LoginSuccessCallback on_success;
    gpointer user_data;
} LoginWidgets;

static void do_login_attempt(LoginWidgets *lw)
{
    const char *username = gtk_editable_get_text(GTK_EDITABLE(lw->username_entry));
    const char *password = gtk_editable_get_text(GTK_EDITABLE(lw->password_entry));

    if (username == NULL || strlen(username) == 0 ||
        password == NULL || strlen(password) == 0) {
        gtk_label_set_text(GTK_LABEL(lw->error_label), "Please enter both username and password.");
        gtk_widget_set_visible(lw->error_label, TRUE);
        return;
    }

    AuthResult res = auth_login(username, password, NULL);
    switch (res) {
        case AUTH_SUCCESS:
            gtk_widget_set_visible(lw->error_label, FALSE);
            gtk_editable_set_text(GTK_EDITABLE(lw->password_entry), "");
            if (lw->on_success) {
                lw->on_success(lw->user_data);
            }
            break;

        case AUTH_ERR_ACCOUNT_LOCKED:
            gtk_label_set_text(GTK_LABEL(lw->error_label),
                               "Account locked due to multiple failed attempts. Try again in 15 minutes.");
            gtk_widget_set_visible(lw->error_label, TRUE);
            break;

        case AUTH_ERR_INACTIVE:
            gtk_label_set_text(GTK_LABEL(lw->error_label),
                               "This account is disabled. Please contact the administrator.");
            gtk_widget_set_visible(lw->error_label, TRUE);
            break;

        case AUTH_ERR_INVALID_CREDENTIALS:
        default:
            gtk_label_set_text(GTK_LABEL(lw->error_label),
                               "Invalid username or password. Please try again.");
            gtk_widget_set_visible(lw->error_label, TRUE);
            break;
    }
}

static void on_submit_clicked(GtkButton *btn, gpointer user_data)
{
    (void)btn;
    LoginWidgets *lw = (LoginWidgets *)user_data;
    do_login_attempt(lw);
}

static void on_entry_activate(GtkEntry *entry, gpointer user_data)
{
    (void)entry;
    LoginWidgets *lw = (LoginWidgets *)user_data;
    do_login_attempt(lw);
}

GtkWidget *school_login_view_new(LoginSuccessCallback on_success, gpointer user_data)
{
    LoginWidgets *lw = g_new0(LoginWidgets, 1);
    lw->on_success = on_success;
    lw->user_data = user_data;

    /* Root container filling entire viewport */
    GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_add_css_class(root, "login-page");
    gtk_widget_set_hexpand(root, TRUE);
    gtk_widget_set_vexpand(root, TRUE);
    gtk_widget_set_halign(root, GTK_ALIGN_FILL);
    gtk_widget_set_valign(root, GTK_ALIGN_FILL);

    /* Center wrapper */
    GtkWidget *center_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_hexpand(center_box, TRUE);
    gtk_widget_set_vexpand(center_box, TRUE);
    gtk_widget_set_halign(center_box, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(center_box, GTK_ALIGN_CENTER);

    /* Login Card */
    GtkWidget *card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 16);
    gtk_widget_add_css_class(card, "login-card");
    gtk_widget_set_size_request(card, 420, -1);

    /* Brand Header */
    GtkWidget *brand_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_set_halign(brand_box, GTK_ALIGN_CENTER);

    GtkWidget *logo = gtk_image_new_from_file("assets/images/app_logo.png");
    gtk_image_set_pixel_size(GTK_IMAGE(logo), 54);
    gtk_widget_add_css_class(logo, "login-logo");
    gtk_box_append(GTK_BOX(brand_box), logo);

    GtkWidget *title = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(title), "<span size='xx-large' weight='bold'>APCOACHING</span>");
    gtk_widget_add_css_class(title, "login-title");
    gtk_box_append(GTK_BOX(brand_box), title);

    GtkWidget *subtitle = gtk_label_new("Sign in to your administration dashboard");
    gtk_widget_add_css_class(subtitle, "login-subtitle");
    gtk_box_append(GTK_BOX(brand_box), subtitle);

    gtk_box_append(GTK_BOX(card), brand_box);

    /* Error Banner */
    lw->error_label = gtk_label_new("");
    gtk_widget_add_css_class(lw->error_label, "login-error-banner");
    gtk_label_set_wrap(GTK_LABEL(lw->error_label), TRUE);
    gtk_widget_set_visible(lw->error_label, FALSE);
    gtk_box_append(GTK_BOX(card), lw->error_label);

    /* Form Fields */
    GtkWidget *form = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);

    /* Username Entry */
    GtkWidget *user_lbl = gtk_label_new("Username");
    gtk_label_set_xalign(GTK_LABEL(user_lbl), 0.0f);
    gtk_widget_add_css_class(user_lbl, "form-label");
    gtk_box_append(GTK_BOX(form), user_lbl);

    lw->username_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(lw->username_entry), "e.g. admin");
    gtk_widget_add_css_class(lw->username_entry, "login-input");
    g_signal_connect(lw->username_entry, "activate", G_CALLBACK(on_entry_activate), lw);
    gtk_box_append(GTK_BOX(form), lw->username_entry);

    /* Password Entry */
    GtkWidget *pwd_lbl = gtk_label_new("Password");
    gtk_label_set_xalign(GTK_LABEL(pwd_lbl), 0.0f);
    gtk_widget_add_css_class(pwd_lbl, "form-label");
    gtk_box_append(GTK_BOX(form), pwd_lbl);

    lw->password_entry = gtk_entry_new();
    gtk_entry_set_visibility(GTK_ENTRY(lw->password_entry), FALSE);
    gtk_entry_set_placeholder_text(GTK_ENTRY(lw->password_entry), "••••••••");
    gtk_widget_add_css_class(lw->password_entry, "login-input");
    g_signal_connect(lw->password_entry, "activate", G_CALLBACK(on_entry_activate), lw);
    gtk_box_append(GTK_BOX(form), lw->password_entry);

    /* Hint / Default note */
    GtkWidget *hint = gtk_label_new("Default credentials: admin / admin123");
    gtk_widget_add_css_class(hint, "login-hint");
    gtk_box_append(GTK_BOX(form), hint);

    /* Submit Button */
    lw->submit_btn = gtk_button_new_with_label("Sign In");
    gtk_widget_add_css_class(lw->submit_btn, "btn-primary");
    gtk_widget_set_margin_top(lw->submit_btn, 8);
    g_signal_connect(lw->submit_btn, "clicked", G_CALLBACK(on_submit_clicked), lw);
    gtk_box_append(GTK_BOX(form), lw->submit_btn);

    gtk_box_append(GTK_BOX(card), form);
    gtk_box_append(GTK_BOX(center_box), card);
    gtk_box_append(GTK_BOX(root), center_box);

    /* Store cleanup pointer */
    g_object_set_data_full(G_OBJECT(root), "login_widgets", lw, g_free);

    return root;
}

void school_login_view_reset(GtkWidget *view)
{
    LoginWidgets *lw = (LoginWidgets *)g_object_get_data(G_OBJECT(view), "login_widgets");
    if (!lw) return;

    gtk_editable_set_text(GTK_EDITABLE(lw->username_entry), "");
    gtk_editable_set_text(GTK_EDITABLE(lw->password_entry), "");
    gtk_widget_set_visible(lw->error_label, FALSE);
}
