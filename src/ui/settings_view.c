/**
 * @file settings_view.c
 * @brief Institutional configuration, Account Security, Email SMTP Gateway,
 *        User Management, Role Permissions, and Backup/Restore GUI view.
 */

#include "ui/settings_view.h"
#include "ui/dialog.h"
#include "settings.h"
#include "classes.h"
#include "auth/auth.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <direct.h>
#define MKDIR(dir) _mkdir(dir)
#else
#include <sys/stat.h>
#define MKDIR(dir) mkdir(dir, 0755)
#endif

typedef enum {
    SETTINGS_TAB_PROFILE = 0,
    SETTINGS_TAB_SECURITY,
    SETTINGS_TAB_SMTP,
    SETTINGS_TAB_USERS,
    SETTINGS_TAB_ROLES,
    SETTINGS_TAB_BACKUP,
    SETTINGS_TAB_LOGS,
    SETTINGS_TAB_COUNT
} SettingsTab;

static const char *MODULE_NAMES[9] = {
    "STUDENTS", "TEACHERS", "CLASSES", "ATTENDANCE",
    "FEES", "EXAMS", "TIMETABLE", "REPORTS", "SETTINGS"
};

static const char *MODULE_LABELS[9] = {
    "Students Directory & ID Cards",
    "Faculty & Teachers Management",
    "Classes & Section Allocation",
    "Daily Attendance Marking",
    "Fee Collection & Receipts",
    "Examinations & Report Cards",
    "Timetable & Class Schedules",
    "Analytics & Export Reports",
    "System Settings & Admin Gateway"
};

typedef struct {
    GtkWidget *root_box;
    GtkWidget *status_banner;

    /* Category Navigation */
    GtkWidget *tab_buttons[SETTINGS_TAB_COUNT];
    SettingsTab current_tab;

    /* Content Cards */
    GtkWidget *card_prof;
    GtkWidget *card_security;
    GtkWidget *card_smtp;
    GtkWidget *card_users;
    GtkWidget *card_roles;
    GtkWidget *card_bk;
    GtkWidget *card_log;

    /* General Profile Entries */
    GtkWidget *entry_school_name;
    GtkWidget *entry_school_address;
    GtkWidget *entry_school_phone;
    GtkWidget *entry_school_email;
    GtkWidget *entry_school_website;
    GtkWidget *entry_principal_name;
    GtkWidget *entry_currency;
    GtkWidget *combo_ay;
    GtkWidget *combo_theme;

    /* Academic Year list */
    AcademicYear ays[MAX_ACADEMIC_YEARS];
    int ay_count;
    int selected_ay_id;

    /* Account Security Entries */
    GtkWidget *entry_old_pass;
    GtkWidget *entry_new_pass;
    GtkWidget *entry_confirm_pass;

    /* SMTP Gateway Entries */
    GtkWidget *entry_smtp_host;
    GtkWidget *entry_smtp_port;
    GtkWidget *combo_smtp_crypto;
    GtkWidget *entry_smtp_user;
    GtkWidget *entry_smtp_pass;
    GtkWidget *entry_smtp_sender_name;
    GtkWidget *entry_smtp_sender_email;

    /* Users Management */
    GtkWidget *users_table_box;

    /* Role Permissions Management */
    GtkWidget *combo_perm_role;
    int selected_perm_role_id;
    GtkWidget *chk_view[9];
    GtkWidget *chk_edit[9];

    /* Backup / Restore */
    GtkWidget *entry_restore_path;
    GtkWidget *logs_box;
    GtkWidget *backups_box;
    GtkWidget *lbl_backups_summary;

    SchoolSettings current_settings;
} SettingsWidgets;

static void switch_settings_tab(SettingsWidgets *sw, SettingsTab tab);
static void load_settings_into_form(SettingsWidgets *sw);
static void load_smtp_into_form(SettingsWidgets *sw);
static void refresh_users_list(SettingsWidgets *sw);
static void refresh_role_permissions(SettingsWidgets *sw);
static void refresh_audit_logs(SettingsWidgets *sw);
static void refresh_backup_list(SettingsWidgets *sw);

/* ── Save Profile Handler ─────────────────────────────────────────── */

static void on_save_settings_clicked(GtkButton *btn, gpointer user_data)
{
    (void)btn;
    SettingsWidgets *sw = (SettingsWidgets *)user_data;

    const char *name = gtk_editable_get_text(GTK_EDITABLE(sw->entry_school_name));
    const char *addr = gtk_editable_get_text(GTK_EDITABLE(sw->entry_school_address));
    const char *phone = gtk_editable_get_text(GTK_EDITABLE(sw->entry_school_phone));
    const char *email = gtk_editable_get_text(GTK_EDITABLE(sw->entry_school_email));
    const char *web = gtk_editable_get_text(GTK_EDITABLE(sw->entry_school_website));
    const char *princ = gtk_editable_get_text(GTK_EDITABLE(sw->entry_principal_name));
    const char *curr = gtk_editable_get_text(GTK_EDITABLE(sw->entry_currency));

    guint theme_sel = gtk_drop_down_get_selected(GTK_DROP_DOWN(sw->combo_theme));
    const char *theme = (theme_sel == 1) ? "dark" : "light";

    guint ay_sel = gtk_drop_down_get_selected(GTK_DROP_DOWN(sw->combo_ay));
    if ((int)ay_sel < sw->ay_count) {
        sw->selected_ay_id = sw->ays[ay_sel].id;
    }

    SchoolSettings s;
    memset(&s, 0, sizeof(SchoolSettings));
    if (name) strncpy(s.school_name, name, sizeof(s.school_name) - 1);
    if (addr) strncpy(s.school_address, addr, sizeof(s.school_address) - 1);
    if (phone) strncpy(s.school_phone, phone, sizeof(s.school_phone) - 1);
    if (email) strncpy(s.school_email, email, sizeof(s.school_email) - 1);
    if (web) strncpy(s.school_website, web, sizeof(s.school_website) - 1);
    if (princ) strncpy(s.principal_name, princ, sizeof(s.principal_name) - 1);
    if (curr) strncpy(s.currency_symbol, curr, sizeof(s.currency_symbol) - 1);
    strncpy(s.theme_mode, theme, sizeof(s.theme_mode) - 1);
    s.current_academic_year_id = sw->selected_ay_id;

    bool ok = settings_service_save(&s);
    if (ok) {
        gtk_label_set_text(GTK_LABEL(sw->status_banner), "Institutional profile saved successfully.");
        gtk_widget_remove_css_class(sw->status_banner, "card-kpi-red");
        gtk_widget_add_css_class(sw->status_banner, "card-kpi-green");
        gtk_widget_set_visible(sw->status_banner, TRUE);
    } else {
        gtk_label_set_text(GTK_LABEL(sw->status_banner), "Failed to save settings to database.");
        gtk_widget_remove_css_class(sw->status_banner, "card-kpi-green");
        gtk_widget_add_css_class(sw->status_banner, "card-kpi-red");
        gtk_widget_set_visible(sw->status_banner, TRUE);
    }
}

/* ── Password Change Handler ──────────────────────────────────────── */

static void on_update_password_clicked(GtkButton *btn, gpointer user_data)
{
    (void)btn;
    SettingsWidgets *sw = (SettingsWidgets *)user_data;

    const char *old_pwd = gtk_editable_get_text(GTK_EDITABLE(sw->entry_old_pass));
    const char *new_pwd = gtk_editable_get_text(GTK_EDITABLE(sw->entry_new_pass));
    const char *cfm_pwd = gtk_editable_get_text(GTK_EDITABLE(sw->entry_confirm_pass));

    if (!old_pwd || strlen(old_pwd) == 0 || !new_pwd || strlen(new_pwd) == 0) {
        gtk_label_set_text(GTK_LABEL(sw->status_banner), "Validation Error: Current password and new password are required.");
        gtk_widget_remove_css_class(sw->status_banner, "card-kpi-green");
        gtk_widget_add_css_class(sw->status_banner, "card-kpi-red");
        gtk_widget_set_visible(sw->status_banner, TRUE);
        return;
    }

    if (strlen(new_pwd) < 6) {
        gtk_label_set_text(GTK_LABEL(sw->status_banner), "Validation Error: New password must be at least 6 characters long.");
        gtk_widget_remove_css_class(sw->status_banner, "card-kpi-green");
        gtk_widget_add_css_class(sw->status_banner, "card-kpi-red");
        gtk_widget_set_visible(sw->status_banner, TRUE);
        return;
    }

    if (strcmp(new_pwd, cfm_pwd) != 0) {
        gtk_label_set_text(GTK_LABEL(sw->status_banner), "Validation Error: New password and Confirmation do not match.");
        gtk_widget_remove_css_class(sw->status_banner, "card-kpi-green");
        gtk_widget_add_css_class(sw->status_banner, "card-kpi-red");
        gtk_widget_set_visible(sw->status_banner, TRUE);
        return;
    }

    const UserSession *sess = auth_get_session();
    int uid = (sess && sess->user_id > 0) ? sess->user_id : 1;

    AuthResult res = auth_change_password(uid, old_pwd, new_pwd);
    if (res == AUTH_SUCCESS) {
        gtk_editable_set_text(GTK_EDITABLE(sw->entry_old_pass), "");
        gtk_editable_set_text(GTK_EDITABLE(sw->entry_new_pass), "");
        gtk_editable_set_text(GTK_EDITABLE(sw->entry_confirm_pass), "");

        gtk_label_set_text(GTK_LABEL(sw->status_banner), "Password updated successfully with PBKDF2 cryptographic salt.");
        gtk_widget_remove_css_class(sw->status_banner, "card-kpi-red");
        gtk_widget_add_css_class(sw->status_banner, "card-kpi-green");
        gtk_widget_set_visible(sw->status_banner, TRUE);
        refresh_audit_logs(sw);
    } else if (res == AUTH_ERR_INVALID_CREDENTIALS) {
        gtk_label_set_text(GTK_LABEL(sw->status_banner), "Authentication Failed: Current password is incorrect.");
        gtk_widget_remove_css_class(sw->status_banner, "card-kpi-green");
        gtk_widget_add_css_class(sw->status_banner, "card-kpi-red");
        gtk_widget_set_visible(sw->status_banner, TRUE);
    } else {
        gtk_label_set_text(GTK_LABEL(sw->status_banner), "Failed to update password. Please check database permissions.");
        gtk_widget_remove_css_class(sw->status_banner, "card-kpi-green");
        gtk_widget_add_css_class(sw->status_banner, "card-kpi-red");
        gtk_widget_set_visible(sw->status_banner, TRUE);
    }
}

/* ── SMTP Gateway Handlers ────────────────────────────────────────── */

static void load_smtp_into_form(SettingsWidgets *sw)
{
    SmtpConfig cfg;
    settings_service_get_smtp(&cfg);

    gtk_editable_set_text(GTK_EDITABLE(sw->entry_smtp_host), cfg.smtp_host);
    char p_buf[16];
    snprintf(p_buf, sizeof(p_buf), "%d", cfg.smtp_port);
    gtk_editable_set_text(GTK_EDITABLE(sw->entry_smtp_port), p_buf);
    gtk_editable_set_text(GTK_EDITABLE(sw->entry_smtp_user), cfg.smtp_username);
    gtk_editable_set_text(GTK_EDITABLE(sw->entry_smtp_pass), cfg.smtp_password);
    gtk_editable_set_text(GTK_EDITABLE(sw->entry_smtp_sender_name), cfg.smtp_sender_name);
    gtk_editable_set_text(GTK_EDITABLE(sw->entry_smtp_sender_email), cfg.smtp_sender_email);

    if (strcmp(cfg.smtp_encryption, "SSL") == 0) {
        gtk_drop_down_set_selected(GTK_DROP_DOWN(sw->combo_smtp_crypto), 1);
    } else if (strcmp(cfg.smtp_encryption, "NONE") == 0) {
        gtk_drop_down_set_selected(GTK_DROP_DOWN(sw->combo_smtp_crypto), 2);
    } else {
        gtk_drop_down_set_selected(GTK_DROP_DOWN(sw->combo_smtp_crypto), 0);
    }
}

static void on_save_smtp_clicked(GtkButton *btn, gpointer user_data)
{
    (void)btn;
    SettingsWidgets *sw = (SettingsWidgets *)user_data;

    SmtpConfig cfg;
    memset(&cfg, 0, sizeof(SmtpConfig));

    const char *host = gtk_editable_get_text(GTK_EDITABLE(sw->entry_smtp_host));
    const char *port_str = gtk_editable_get_text(GTK_EDITABLE(sw->entry_smtp_port));
    const char *user = gtk_editable_get_text(GTK_EDITABLE(sw->entry_smtp_user));
    const char *pass = gtk_editable_get_text(GTK_EDITABLE(sw->entry_smtp_pass));
    const char *s_name = gtk_editable_get_text(GTK_EDITABLE(sw->entry_smtp_sender_name));
    const char *s_email = gtk_editable_get_text(GTK_EDITABLE(sw->entry_smtp_sender_email));

    if (host) strncpy(cfg.smtp_host, host, sizeof(cfg.smtp_host) - 1);
    cfg.smtp_port = port_str ? atoi(port_str) : 587;
    if (user) strncpy(cfg.smtp_username, user, sizeof(cfg.smtp_username) - 1);
    if (pass) strncpy(cfg.smtp_password, pass, sizeof(cfg.smtp_password) - 1);
    if (s_name) strncpy(cfg.smtp_sender_name, s_name, sizeof(cfg.smtp_sender_name) - 1);
    if (s_email) strncpy(cfg.smtp_sender_email, s_email, sizeof(cfg.smtp_sender_email) - 1);

    guint sel = gtk_drop_down_get_selected(GTK_DROP_DOWN(sw->combo_smtp_crypto));
    if (sel == 1) strncpy(cfg.smtp_encryption, "SSL", sizeof(cfg.smtp_encryption) - 1);
    else if (sel == 2) strncpy(cfg.smtp_encryption, "NONE", sizeof(cfg.smtp_encryption) - 1);
    else strncpy(cfg.smtp_encryption, "STARTTLS", sizeof(cfg.smtp_encryption) - 1);

    bool ok = settings_service_save_smtp(&cfg);
    if (ok) {
        gtk_label_set_text(GTK_LABEL(sw->status_banner), "Outgoing Email SMTP Gateway configuration saved successfully.");
        gtk_widget_remove_css_class(sw->status_banner, "card-kpi-red");
        gtk_widget_add_css_class(sw->status_banner, "card-kpi-green");
        gtk_widget_set_visible(sw->status_banner, TRUE);
        refresh_audit_logs(sw);
    } else {
        gtk_label_set_text(GTK_LABEL(sw->status_banner), "Failed to save SMTP configuration.");
        gtk_widget_remove_css_class(sw->status_banner, "card-kpi-green");
        gtk_widget_add_css_class(sw->status_banner, "card-kpi-red");
        gtk_widget_set_visible(sw->status_banner, TRUE);
    }
}

static void on_test_smtp_clicked(GtkButton *btn, gpointer user_data)
{
    (void)btn;
    SettingsWidgets *sw = (SettingsWidgets *)user_data;

    SmtpConfig cfg;
    memset(&cfg, 0, sizeof(SmtpConfig));

    const char *host = gtk_editable_get_text(GTK_EDITABLE(sw->entry_smtp_host));
    const char *port_str = gtk_editable_get_text(GTK_EDITABLE(sw->entry_smtp_port));
    const char *user = gtk_editable_get_text(GTK_EDITABLE(sw->entry_smtp_user));
    const char *pass = gtk_editable_get_text(GTK_EDITABLE(sw->entry_smtp_pass));
    const char *s_name = gtk_editable_get_text(GTK_EDITABLE(sw->entry_smtp_sender_name));
    const char *s_email = gtk_editable_get_text(GTK_EDITABLE(sw->entry_smtp_sender_email));

    if (host) strncpy(cfg.smtp_host, host, sizeof(cfg.smtp_host) - 1);
    cfg.smtp_port = port_str ? atoi(port_str) : 587;
    if (user) strncpy(cfg.smtp_username, user, sizeof(cfg.smtp_username) - 1);
    if (pass) strncpy(cfg.smtp_password, pass, sizeof(cfg.smtp_password) - 1);
    if (s_name) strncpy(cfg.smtp_sender_name, s_name, sizeof(cfg.smtp_sender_name) - 1);
    if (s_email) strncpy(cfg.smtp_sender_email, s_email, sizeof(cfg.smtp_sender_email) - 1);

    guint sel = gtk_drop_down_get_selected(GTK_DROP_DOWN(sw->combo_smtp_crypto));
    if (sel == 1) strncpy(cfg.smtp_encryption, "SSL", sizeof(cfg.smtp_encryption) - 1);
    else if (sel == 2) strncpy(cfg.smtp_encryption, "NONE", sizeof(cfg.smtp_encryption) - 1);
    else strncpy(cfg.smtp_encryption, "STARTTLS", sizeof(cfg.smtp_encryption) - 1);

    char msg[512] = {0};
    bool ok = settings_service_test_smtp(&cfg, msg, sizeof(msg));
    gtk_label_set_text(GTK_LABEL(sw->status_banner), msg);
    if (ok) {
        gtk_widget_remove_css_class(sw->status_banner, "card-kpi-red");
        gtk_widget_add_css_class(sw->status_banner, "card-kpi-green");
    } else {
        gtk_widget_remove_css_class(sw->status_banner, "card-kpi-green");
        gtk_widget_add_css_class(sw->status_banner, "card-kpi-red");
    }
    gtk_widget_set_visible(sw->status_banner, TRUE);
    refresh_audit_logs(sw);
}

/* ── User Management Handlers & Dialogs ───────────────────────────── */

typedef struct {
    SettingsWidgets *sw;
    int target_user_id;
    char target_username[64];
    GtkWidget *dialog;
    GtkWidget *entry_new_pwd;
    GtkWidget *entry_cfm_pwd;
    GtkWidget *lbl_err;
} AdminResetData;

static void on_confirm_admin_reset_clicked(GtkButton *btn, gpointer user_data)
{
    (void)btn;
    AdminResetData *ard = (AdminResetData *)user_data;

    const char *p1 = gtk_editable_get_text(GTK_EDITABLE(ard->entry_new_pwd));
    const char *p2 = gtk_editable_get_text(GTK_EDITABLE(ard->entry_cfm_pwd));

    if (!p1 || strlen(p1) < 6) {
        gtk_label_set_text(GTK_LABEL(ard->lbl_err), "Password must be at least 6 characters.");
        gtk_widget_set_visible(ard->lbl_err, TRUE);
        return;
    }

    if (strcmp(p1, p2) != 0) {
        gtk_label_set_text(GTK_LABEL(ard->lbl_err), "Passwords do not match.");
        gtk_widget_set_visible(ard->lbl_err, TRUE);
        return;
    }

    AuthResult res = auth_admin_reset_password(ard->target_user_id, p1);
    if (res == AUTH_SUCCESS) {
        char msg[256];
        snprintf(msg, sizeof(msg), "Password for user '%s' reset successfully.", ard->target_username);
        gtk_label_set_text(GTK_LABEL(ard->sw->status_banner), msg);
        gtk_widget_remove_css_class(ard->sw->status_banner, "card-kpi-red");
        gtk_widget_add_css_class(ard->sw->status_banner, "card-kpi-green");
        gtk_widget_set_visible(ard->sw->status_banner, TRUE);
        refresh_audit_logs(ard->sw);
        school_dialog_close(ard->dialog);
    } else {
        gtk_label_set_text(GTK_LABEL(ard->lbl_err), "Failed to reset password in database.");
        gtk_widget_set_visible(ard->lbl_err, TRUE);
    }
}

static void on_cancel_admin_reset_clicked(GtkButton *btn, gpointer user_data)
{
    (void)btn;
    AdminResetData *ard = (AdminResetData *)user_data;
    school_dialog_close(ard->dialog);
}

static void on_row_user_reset_pwd_clicked(GtkButton *btn, gpointer user_data)
{
    SettingsWidgets *sw = (SettingsWidgets *)user_data;
    int uid = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(btn), "user_id"));
    const char *uname = (const char *)g_object_get_data(G_OBJECT(btn), "user_name");

    AdminResetData *ard = g_new0(AdminResetData, 1);
    ard->sw = sw;
    ard->target_user_id = uid;
    strncpy(ard->target_username, uname ? uname : "user", sizeof(ard->target_username) - 1);

    GtkWidget *win = school_dialog_new("Admin Password Override", 420, 280);
    ard->dialog = win;

    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_set_margin_start(box, 24);
    gtk_widget_set_margin_end(box, 24);
    gtk_widget_set_margin_top(box, 20);
    gtk_widget_set_margin_bottom(box, 20);
    school_dialog_set_child(win, box);

    char title_buf[128];
    snprintf(title_buf, sizeof(title_buf), "Reset Password for: %s", ard->target_username);
    GtkWidget *lbl_t = gtk_label_new(title_buf);
    gtk_widget_add_css_class(lbl_t, "card-title");
    gtk_label_set_xalign(GTK_LABEL(lbl_t), 0.0f);
    gtk_box_append(GTK_BOX(box), lbl_t);

    GtkWidget *l1 = gtk_label_new("New Password (min 6 characters):");
    gtk_label_set_xalign(GTK_LABEL(l1), 0.0f);
    gtk_box_append(GTK_BOX(box), l1);
    ard->entry_new_pwd = gtk_entry_new();
    gtk_entry_set_visibility(GTK_ENTRY(ard->entry_new_pwd), FALSE);
    gtk_box_append(GTK_BOX(box), ard->entry_new_pwd);

    GtkWidget *l2 = gtk_label_new("Confirm New Password:");
    gtk_label_set_xalign(GTK_LABEL(l2), 0.0f);
    gtk_box_append(GTK_BOX(box), l2);
    ard->entry_cfm_pwd = gtk_entry_new();
    gtk_entry_set_visibility(GTK_ENTRY(ard->entry_cfm_pwd), FALSE);
    gtk_box_append(GTK_BOX(box), ard->entry_cfm_pwd);

    ard->lbl_err = gtk_label_new("");
    gtk_widget_add_css_class(ard->lbl_err, "dim-label");
    gtk_widget_set_visible(ard->lbl_err, FALSE);
    gtk_box_append(GTK_BOX(box), ard->lbl_err);

    GtkWidget *btns = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_set_halign(btns, GTK_ALIGN_END);

    GtkWidget *btn_cancel = gtk_button_new_with_label("Cancel");
    gtk_widget_add_css_class(btn_cancel, "btn-secondary");
    g_signal_connect(btn_cancel, "clicked", G_CALLBACK(on_cancel_admin_reset_clicked), ard);
    gtk_box_append(GTK_BOX(btns), btn_cancel);

    GtkWidget *btn_confirm = gtk_button_new_with_label("Override Password");
    gtk_widget_add_css_class(btn_confirm, "btn-primary");
    g_signal_connect(btn_confirm, "clicked", G_CALLBACK(on_confirm_admin_reset_clicked), ard);
    gtk_box_append(GTK_BOX(btns), btn_confirm);

    gtk_box_append(GTK_BOX(box), btns);
    g_object_set_data_full(G_OBJECT(win), "ard", ard, g_free);
    school_dialog_present(win);
}

static void on_row_user_toggle_status_clicked(GtkButton *btn, gpointer user_data)
{
    SettingsWidgets *sw = (SettingsWidgets *)user_data;
    int uid = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(btn), "user_id"));

    AuthResult res = auth_toggle_user_status(uid);
    if (res == AUTH_SUCCESS) {
        gtk_label_set_text(GTK_LABEL(sw->status_banner), "User account status toggled successfully.");
        gtk_widget_remove_css_class(sw->status_banner, "card-kpi-red");
        gtk_widget_add_css_class(sw->status_banner, "card-kpi-green");
        gtk_widget_set_visible(sw->status_banner, TRUE);
        refresh_users_list(sw);
        refresh_audit_logs(sw);
    } else if (res == AUTH_ERR_LAST_ADMIN) {
        gtk_label_set_text(GTK_LABEL(sw->status_banner), "Safety Protection: Cannot deactivate the only active Administrator.");
        gtk_widget_remove_css_class(sw->status_banner, "card-kpi-green");
        gtk_widget_add_css_class(sw->status_banner, "card-kpi-red");
        gtk_widget_set_visible(sw->status_banner, TRUE);
    } else if (res == AUTH_ERR_PARAM) {
        gtk_label_set_text(GTK_LABEL(sw->status_banner), "Security Protection: Cannot deactivate your own currently active account.");
        gtk_widget_remove_css_class(sw->status_banner, "card-kpi-green");
        gtk_widget_add_css_class(sw->status_banner, "card-kpi-red");
        gtk_widget_set_visible(sw->status_banner, TRUE);
    } else {
        gtk_label_set_text(GTK_LABEL(sw->status_banner), "Failed to change user status.");
        gtk_widget_remove_css_class(sw->status_banner, "card-kpi-green");
        gtk_widget_add_css_class(sw->status_banner, "card-kpi-red");
        gtk_widget_set_visible(sw->status_banner, TRUE);
    }
}

typedef struct {
    SettingsWidgets *sw;
    GtkWidget *dialog;
    GtkWidget *entry_uname;
    GtkWidget *entry_pwd;
    GtkWidget *entry_fname;
    GtkWidget *combo_role;
    GtkWidget *entry_email;
    GtkWidget *entry_phone;
    GtkWidget *lbl_err;
} AddUserData;

static void on_cancel_add_user_clicked(GtkButton *btn, gpointer user_data)
{
    (void)btn;
    AddUserData *aud = (AddUserData *)user_data;
    school_dialog_close(aud->dialog);
}

static void on_confirm_add_user_clicked(GtkButton *btn, gpointer user_data)
{
    (void)btn;
    AddUserData *aud = (AddUserData *)user_data;

    const char *uname = gtk_editable_get_text(GTK_EDITABLE(aud->entry_uname));
    const char *pwd = gtk_editable_get_text(GTK_EDITABLE(aud->entry_pwd));
    const char *fname = gtk_editable_get_text(GTK_EDITABLE(aud->entry_fname));
    const char *email = gtk_editable_get_text(GTK_EDITABLE(aud->entry_email));
    const char *phone = gtk_editable_get_text(GTK_EDITABLE(aud->entry_phone));

    guint sel_role = gtk_drop_down_get_selected(GTK_DROP_DOWN(aud->combo_role));
    int role_id = (int)sel_role + 1; /* 1=Admin, 2=Teacher, 3=Accountant, 4=Staff */

    if (!uname || strlen(uname) == 0 || !pwd || strlen(pwd) < 6 || !fname || strlen(fname) == 0) {
        gtk_label_set_text(GTK_LABEL(aud->lbl_err), "Username, full name, and min 6-char password required.");
        gtk_widget_set_visible(aud->lbl_err, TRUE);
        return;
    }

    AuthResult res = auth_create_user(uname, pwd, role_id, fname, email, phone);
    if (res == AUTH_SUCCESS) {
        char msg[256];
        snprintf(msg, sizeof(msg), "User account '%s' created successfully.", uname);
        gtk_label_set_text(GTK_LABEL(aud->sw->status_banner), msg);
        gtk_widget_remove_css_class(aud->sw->status_banner, "card-kpi-red");
        gtk_widget_add_css_class(aud->sw->status_banner, "card-kpi-green");
        gtk_widget_set_visible(aud->sw->status_banner, TRUE);
        refresh_users_list(aud->sw);
        refresh_audit_logs(aud->sw);
        school_dialog_close(aud->dialog);
    } else {
        gtk_label_set_text(GTK_LABEL(aud->lbl_err), "Error: Username or email may already exist in database.");
        gtk_widget_set_visible(aud->lbl_err, TRUE);
    }
}

static void on_add_new_user_clicked(GtkButton *btn, gpointer user_data)
{
    (void)btn;
    SettingsWidgets *sw = (SettingsWidgets *)user_data;

    AddUserData *aud = g_new0(AddUserData, 1);
    aud->sw = sw;

    GtkWidget *win = school_dialog_new("Create System User Account", 460, 520);
    aud->dialog = win;

    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_margin_start(box, 24);
    gtk_widget_set_margin_end(box, 24);
    gtk_widget_set_margin_top(box, 20);
    gtk_widget_set_margin_bottom(box, 20);
    school_dialog_set_child(win, box);

    GtkWidget *lbl_t = gtk_label_new("Register New Institutional User");
    gtk_widget_add_css_class(lbl_t, "card-title");
    gtk_label_set_xalign(GTK_LABEL(lbl_t), 0.0f);
    gtk_box_append(GTK_BOX(box), lbl_t);

    GtkWidget *l1 = gtk_label_new("Username (unique login ID):");
    gtk_label_set_xalign(GTK_LABEL(l1), 0.0f);
    gtk_box_append(GTK_BOX(box), l1);
    aud->entry_uname = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(aud->entry_uname), "e.g. t_sharma");
    gtk_box_append(GTK_BOX(box), aud->entry_uname);

    GtkWidget *l2 = gtk_label_new("Initial Password (min 6 characters):");
    gtk_label_set_xalign(GTK_LABEL(l2), 0.0f);
    gtk_box_append(GTK_BOX(box), l2);
    aud->entry_pwd = gtk_entry_new();
    gtk_entry_set_visibility(GTK_ENTRY(aud->entry_pwd), FALSE);
    gtk_box_append(GTK_BOX(box), aud->entry_pwd);

    GtkWidget *l3 = gtk_label_new("Full Display Name:");
    gtk_label_set_xalign(GTK_LABEL(l3), 0.0f);
    gtk_box_append(GTK_BOX(box), l3);
    aud->entry_fname = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(aud->entry_fname), "e.g. Ramesh Sharma");
    gtk_box_append(GTK_BOX(box), aud->entry_fname);

    GtkWidget *l4 = gtk_label_new("Assigned System Role:");
    gtk_label_set_xalign(GTK_LABEL(l4), 0.0f);
    gtk_box_append(GTK_BOX(box), l4);
    const char *roles[] = { "Administrator (Full Access)", "Teacher (Academics & Attendance)", "Accountant (Fees & Billing)", "Staff (Inquiry & Records)", NULL };
    aud->combo_role = gtk_drop_down_new_from_strings(roles);
    gtk_box_append(GTK_BOX(box), aud->combo_role);

    GtkWidget *l5 = gtk_label_new("Email Address:");
    gtk_label_set_xalign(GTK_LABEL(l5), 0.0f);
    gtk_box_append(GTK_BOX(box), l5);
    aud->entry_email = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(aud->entry_email), "e.g. ramesh@school.edu");
    gtk_box_append(GTK_BOX(box), aud->entry_email);

    GtkWidget *l6 = gtk_label_new("Contact Phone:");
    gtk_label_set_xalign(GTK_LABEL(l6), 0.0f);
    gtk_box_append(GTK_BOX(box), l6);
    aud->entry_phone = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(aud->entry_phone), "e.g. +91 9876543210");
    gtk_box_append(GTK_BOX(box), aud->entry_phone);

    aud->lbl_err = gtk_label_new("");
    gtk_widget_add_css_class(aud->lbl_err, "dim-label");
    gtk_widget_set_visible(aud->lbl_err, FALSE);
    gtk_box_append(GTK_BOX(box), aud->lbl_err);

    GtkWidget *btns = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_set_halign(btns, GTK_ALIGN_END);

    GtkWidget *btn_cancel = gtk_button_new_with_label("Cancel");
    gtk_widget_add_css_class(btn_cancel, "btn-secondary");
    g_signal_connect(btn_cancel, "clicked", G_CALLBACK(on_cancel_add_user_clicked), aud);
    gtk_box_append(GTK_BOX(btns), btn_cancel);

    GtkWidget *btn_confirm = gtk_button_new_with_label("Create User Account");
    gtk_widget_add_css_class(btn_confirm, "btn-primary");
    g_signal_connect(btn_confirm, "clicked", G_CALLBACK(on_confirm_add_user_clicked), aud);
    gtk_box_append(GTK_BOX(btns), btn_confirm);

    gtk_box_append(GTK_BOX(box), btns);
    g_object_set_data_full(G_OBJECT(win), "aud", aud, g_free);
    school_dialog_present(win);
}

static void refresh_users_list(SettingsWidgets *sw)
{
    if (!sw->users_table_box) return;

    GtkWidget *child = gtk_widget_get_first_child(sw->users_table_box);
    while (child) {
        GtkWidget *next = gtk_widget_get_next_sibling(child);
        gtk_box_remove(GTK_BOX(sw->users_table_box), child);
        child = next;
    }

    ManagedUser users[64];
    int count = 0;
    auth_get_all_users(users, 64, &count);

    /* Table Header */
    GtkWidget *hdr_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_add_css_class(hdr_row, "table-header-row");

    GtkWidget *h_id = gtk_label_new("ID");
    gtk_widget_set_size_request(h_id, 40, -1);
    gtk_box_append(GTK_BOX(hdr_row), h_id);

    GtkWidget *h_u = gtk_label_new("Username");
    gtk_widget_set_size_request(h_u, 110, -1);
    gtk_label_set_xalign(GTK_LABEL(h_u), 0.0f);
    gtk_box_append(GTK_BOX(hdr_row), h_u);

    GtkWidget *h_fn = gtk_label_new("Full Name");
    gtk_widget_set_hexpand(h_fn, TRUE);
    gtk_label_set_xalign(GTK_LABEL(h_fn), 0.0f);
    gtk_box_append(GTK_BOX(hdr_row), h_fn);

    GtkWidget *h_r = gtk_label_new("Role");
    gtk_widget_set_size_request(h_r, 100, -1);
    gtk_box_append(GTK_BOX(hdr_row), h_r);

    GtkWidget *h_em = gtk_label_new("Email");
    gtk_widget_set_size_request(h_em, 160, -1);
    gtk_label_set_xalign(GTK_LABEL(h_em), 0.0f);
    gtk_box_append(GTK_BOX(hdr_row), h_em);

    GtkWidget *h_st = gtk_label_new("Status");
    gtk_widget_set_size_request(h_st, 80, -1);
    gtk_box_append(GTK_BOX(hdr_row), h_st);

    GtkWidget *h_act = gtk_label_new("Actions");
    gtk_widget_set_size_request(h_act, 180, -1);
    gtk_box_append(GTK_BOX(hdr_row), h_act);

    gtk_box_append(GTK_BOX(sw->users_table_box), hdr_row);

    for (int i = 0; i < count; i++) {
        GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
        gtk_widget_add_css_class(row, "table-data-row");

        char id_str[16];
        snprintf(id_str, sizeof(id_str), "#%d", users[i].id);
        GtkWidget *l_id = gtk_label_new(id_str);
        gtk_widget_set_size_request(l_id, 40, -1);
        gtk_widget_add_css_class(l_id, "dim-label");
        gtk_box_append(GTK_BOX(row), l_id);

        GtkWidget *l_u = gtk_label_new(users[i].username);
        gtk_widget_set_size_request(l_u, 110, -1);
        gtk_widget_add_css_class(l_u, "table-cell-bold");
        gtk_label_set_xalign(GTK_LABEL(l_u), 0.0f);
        gtk_box_append(GTK_BOX(row), l_u);

        GtkWidget *l_fn = gtk_label_new(users[i].full_name);
        gtk_widget_set_hexpand(l_fn, TRUE);
        gtk_label_set_xalign(GTK_LABEL(l_fn), 0.0f);
        gtk_box_append(GTK_BOX(row), l_fn);

        GtkWidget *l_r = gtk_label_new(users[i].role_name);
        if (users[i].role_id == ROLE_ADMIN) gtk_widget_add_css_class(l_r, "badge-adm");
        else if (users[i].role_id == ROLE_TEACHER) gtk_widget_add_css_class(l_r, "badge-info");
        else if (users[i].role_id == ROLE_ACCOUNTANT) gtk_widget_add_css_class(l_r, "card-kpi-green");
        else gtk_widget_add_css_class(l_r, "dim-label");
        gtk_widget_set_size_request(l_r, 100, -1);
        gtk_box_append(GTK_BOX(row), l_r);

        GtkWidget *l_em = gtk_label_new(strlen(users[i].email) > 0 ? users[i].email : "—");
        gtk_widget_set_size_request(l_em, 160, -1);
        gtk_label_set_xalign(GTK_LABEL(l_em), 0.0f);
        gtk_box_append(GTK_BOX(row), l_em);

        bool is_act = (strcmp(users[i].status, "ACTIVE") == 0);
        GtkWidget *l_st = gtk_label_new(is_act ? "ACTIVE" : "INACTIVE");
        gtk_widget_add_css_class(l_st, is_act ? "card-kpi-green" : "card-kpi-red");
        gtk_widget_set_size_request(l_st, 80, -1);
        gtk_box_append(GTK_BOX(row), l_st);

        GtkWidget *btn_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
        gtk_widget_set_size_request(btn_box, 180, -1);

        GtkWidget *btn_status = gtk_button_new_with_label(is_act ? "Deactivate" : "Activate");
        gtk_widget_add_css_class(btn_status, is_act ? "btn-secondary" : "btn-primary");
        g_object_set_data(G_OBJECT(btn_status), "user_id", GINT_TO_POINTER(users[i].id));
        g_signal_connect(btn_status, "clicked", G_CALLBACK(on_row_user_toggle_status_clicked), sw);
        gtk_box_append(GTK_BOX(btn_box), btn_status);

        GtkWidget *btn_pwd = gtk_button_new_with_label("Reset Pwd");
        gtk_widget_add_css_class(btn_pwd, "btn-secondary");
        g_object_set_data(G_OBJECT(btn_pwd), "user_id", GINT_TO_POINTER(users[i].id));
        g_object_set_data_full(G_OBJECT(btn_pwd), "user_name", g_strdup(users[i].username), g_free);
        g_signal_connect(btn_pwd, "clicked", G_CALLBACK(on_row_user_reset_pwd_clicked), sw);
        gtk_box_append(GTK_BOX(btn_box), btn_pwd);

        gtk_box_append(GTK_BOX(row), btn_box);
        gtk_box_append(GTK_BOX(sw->users_table_box), row);
    }
}

/* ── Role Permissions Handlers ────────────────────────────────────── */

static void refresh_role_permissions(SettingsWidgets *sw)
{
    guint sel = gtk_drop_down_get_selected(GTK_DROP_DOWN(sw->combo_perm_role));
    sw->selected_perm_role_id = (int)sel + 1; /* 1=Admin, 2=Teacher, 3=Accountant, 4=Staff */

    RolePermission perms[16];
    int count = 0;
    auth_get_role_permissions(sw->selected_perm_role_id, perms, 16, &count);

    bool is_admin = (sw->selected_perm_role_id == ROLE_ADMIN);

    for (int i = 0; i < 9; i++) {
        bool can_v = false;
        bool can_e = false;

        if (is_admin) {
            can_v = true;
            can_e = true;
        } else {
            for (int k = 0; k < count; k++) {
                if (strcmp(perms[k].module_name, MODULE_NAMES[i]) == 0) {
                    can_v = perms[k].can_view;
                    can_e = perms[k].can_edit;
                    break;
                }
            }
        }

        gtk_check_button_set_active(GTK_CHECK_BUTTON(sw->chk_view[i]), can_v);
        gtk_check_button_set_active(GTK_CHECK_BUTTON(sw->chk_edit[i]), can_e);

        /* Admin role always locked to full access */
        gtk_widget_set_sensitive(sw->chk_view[i], !is_admin);
        gtk_widget_set_sensitive(sw->chk_edit[i], !is_admin);
    }
}

static void on_perm_role_changed(GObject *gobject, GParamSpec *pspec, gpointer user_data)
{
    (void)gobject;
    (void)pspec;
    SettingsWidgets *sw = (SettingsWidgets *)user_data;
    refresh_role_permissions(sw);
}

static void on_save_permissions_clicked(GtkButton *btn, gpointer user_data)
{
    (void)btn;
    SettingsWidgets *sw = (SettingsWidgets *)user_data;

    int role_id = sw->selected_perm_role_id;
    for (int i = 0; i < 9; i++) {
        gboolean can_v = gtk_check_button_get_active(GTK_CHECK_BUTTON(sw->chk_view[i]));
        gboolean can_e = gtk_check_button_get_active(GTK_CHECK_BUTTON(sw->chk_edit[i]));
        auth_set_role_permission(role_id, MODULE_NAMES[i], can_v, can_e);
    }

    gtk_label_set_text(GTK_LABEL(sw->status_banner), "Role permissions matrix saved successfully.");
    gtk_widget_remove_css_class(sw->status_banner, "card-kpi-red");
    gtk_widget_add_css_class(sw->status_banner, "card-kpi-green");
    gtk_widget_set_visible(sw->status_banner, TRUE);
    refresh_audit_logs(sw);
}

/* ── Backup & Restore Handlers ────────────────────────────────────── */

typedef struct {
    SettingsWidgets *sw;
    char filepath[256];
    char filename[128];
    GtkWidget *dialog;
} BackupActionData;

static void on_cancel_action_clicked(GtkButton *btn, gpointer user_data)
{
    (void)btn;
    BackupActionData *bad = (BackupActionData *)user_data;
    school_dialog_close(bad->dialog);
}

static void on_confirm_restore_clicked(GtkButton *btn, gpointer user_data)
{
    (void)btn;
    BackupActionData *bad = (BackupActionData *)user_data;
    SettingsWidgets *sw = bad->sw;

    bool ok = settings_service_restore_backup(bad->filepath);
    if (ok) {
        char msg[512];
        snprintf(msg, sizeof(msg), "Database successfully restored from: %s", bad->filename);
        gtk_label_set_text(GTK_LABEL(sw->status_banner), msg);
        gtk_widget_remove_css_class(sw->status_banner, "card-kpi-red");
        gtk_widget_add_css_class(sw->status_banner, "card-kpi-green");
        gtk_widget_set_visible(sw->status_banner, TRUE);
        load_settings_into_form(sw);
        load_smtp_into_form(sw);
        refresh_audit_logs(sw);
        refresh_backup_list(sw);
        refresh_users_list(sw);
        refresh_role_permissions(sw);
    } else {
        gtk_label_set_text(GTK_LABEL(sw->status_banner), "Failed to restore database from backup.");
        gtk_widget_remove_css_class(sw->status_banner, "card-kpi-green");
        gtk_widget_add_css_class(sw->status_banner, "card-kpi-red");
        gtk_widget_set_visible(sw->status_banner, TRUE);
    }

    school_dialog_close(bad->dialog);
}

static void on_confirm_delete_clicked(GtkButton *btn, gpointer user_data)
{
    (void)btn;
    BackupActionData *bad = (BackupActionData *)user_data;
    SettingsWidgets *sw = bad->sw;

    bool ok = settings_service_delete_backup(bad->filepath);
    if (ok) {
        char msg[512];
        snprintf(msg, sizeof(msg), "Backup '%s' deleted successfully.", bad->filename);
        gtk_label_set_text(GTK_LABEL(sw->status_banner), msg);
        gtk_widget_remove_css_class(sw->status_banner, "card-kpi-red");
        gtk_widget_add_css_class(sw->status_banner, "card-kpi-green");
        gtk_widget_set_visible(sw->status_banner, TRUE);
        refresh_audit_logs(sw);
        refresh_backup_list(sw);
    } else {
        gtk_label_set_text(GTK_LABEL(sw->status_banner), "Failed to delete backup file.");
        gtk_widget_remove_css_class(sw->status_banner, "card-kpi-green");
        gtk_widget_add_css_class(sw->status_banner, "card-kpi-red");
        gtk_widget_set_visible(sw->status_banner, TRUE);
    }

    school_dialog_close(bad->dialog);
}

static void on_row_restore_clicked(GtkButton *btn, gpointer user_data)
{
    SettingsWidgets *sw = (SettingsWidgets *)user_data;
    const char *fpath = (const char *)g_object_get_data(G_OBJECT(btn), "backup_path");
    const char *fname = (const char *)g_object_get_data(G_OBJECT(btn), "backup_name");
    if (!fpath) return;

    BackupActionData *bad = g_new0(BackupActionData, 1);
    bad->sw = sw;
    strncpy(bad->filepath, fpath, sizeof(bad->filepath) - 1);
    strncpy(bad->filename, fname ? fname : fpath, sizeof(bad->filename) - 1);

    GtkWidget *win = school_dialog_new("Confirm Database Restore", 460, 220);
    bad->dialog = win;

    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 16);
    gtk_widget_set_margin_start(box, 24);
    gtk_widget_set_margin_end(box, 24);
    gtk_widget_set_margin_top(box, 20);
    gtk_widget_set_margin_bottom(box, 20);
    school_dialog_set_child(win, box);

    GtkWidget *lbl_warn = gtk_label_new("Warning: Restoring Database Snapshot");
    gtk_widget_add_css_class(lbl_warn, "card-title");
    gtk_label_set_xalign(GTK_LABEL(lbl_warn), 0.0f);
    gtk_box_append(GTK_BOX(box), lbl_warn);

    char msg[512];
    snprintf(msg, sizeof(msg),
             "Are you sure you want to restore the database from '%s'?\n\n"
             "All active database records will be reverted to this exact snapshot. Unsaved changes since this backup will be lost.",
             bad->filename);
    GtkWidget *lbl_msg = gtk_label_new(msg);
    gtk_label_set_wrap(GTK_LABEL(lbl_msg), TRUE);
    gtk_label_set_xalign(GTK_LABEL(lbl_msg), 0.0f);
    gtk_box_append(GTK_BOX(box), lbl_msg);

    GtkWidget *btns = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_set_halign(btns, GTK_ALIGN_END);

    GtkWidget *btn_cancel = gtk_button_new_with_label("Cancel");
    gtk_widget_add_css_class(btn_cancel, "btn-secondary");
    g_signal_connect(btn_cancel, "clicked", G_CALLBACK(on_cancel_action_clicked), bad);
    gtk_box_append(GTK_BOX(btns), btn_cancel);

    GtkWidget *btn_confirm = gtk_button_new_with_label("Restore Snapshot");
    gtk_widget_add_css_class(btn_confirm, "btn-primary");
    g_signal_connect(btn_confirm, "clicked", G_CALLBACK(on_confirm_restore_clicked), bad);
    gtk_box_append(GTK_BOX(btns), btn_confirm);

    gtk_box_append(GTK_BOX(box), btns);
    g_object_set_data_full(G_OBJECT(win), "bad", bad, g_free);
    school_dialog_present(win);
}

static void on_row_delete_clicked(GtkButton *btn, gpointer user_data)
{
    SettingsWidgets *sw = (SettingsWidgets *)user_data;
    const char *fpath = (const char *)g_object_get_data(G_OBJECT(btn), "backup_path");
    const char *fname = (const char *)g_object_get_data(G_OBJECT(btn), "backup_name");
    if (!fpath) return;

    BackupActionData *bad = g_new0(BackupActionData, 1);
    bad->sw = sw;
    strncpy(bad->filepath, fpath, sizeof(bad->filepath) - 1);
    strncpy(bad->filename, fname ? fname : fpath, sizeof(bad->filename) - 1);

    GtkWidget *win = school_dialog_new("Confirm Backup Deletion", 440, 180);
    bad->dialog = win;

    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 16);
    gtk_widget_set_margin_start(box, 24);
    gtk_widget_set_margin_end(box, 24);
    gtk_widget_set_margin_top(box, 20);
    gtk_widget_set_margin_bottom(box, 20);
    school_dialog_set_child(win, box);

    GtkWidget *lbl_warn = gtk_label_new("Delete Backup File");
    gtk_widget_add_css_class(lbl_warn, "card-title");
    gtk_label_set_xalign(GTK_LABEL(lbl_warn), 0.0f);
    gtk_box_append(GTK_BOX(box), lbl_warn);

    char msg[512];
    snprintf(msg, sizeof(msg),
             "Are you sure you want to permanently delete the backup file '%s'?",
             bad->filename);
    GtkWidget *lbl_msg = gtk_label_new(msg);
    gtk_label_set_wrap(GTK_LABEL(lbl_msg), TRUE);
    gtk_label_set_xalign(GTK_LABEL(lbl_msg), 0.0f);
    gtk_box_append(GTK_BOX(box), lbl_msg);

    GtkWidget *btns = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_set_halign(btns, GTK_ALIGN_END);

    GtkWidget *btn_cancel = gtk_button_new_with_label("Cancel");
    gtk_widget_add_css_class(btn_cancel, "btn-secondary");
    g_signal_connect(btn_cancel, "clicked", G_CALLBACK(on_cancel_action_clicked), bad);
    gtk_box_append(GTK_BOX(btns), btn_cancel);

    GtkWidget *btn_confirm = gtk_button_new_with_label("Delete");
    gtk_widget_add_css_class(btn_confirm, "btn-action-delete");
    g_signal_connect(btn_confirm, "clicked", G_CALLBACK(on_confirm_delete_clicked), bad);
    gtk_box_append(GTK_BOX(btns), btn_confirm);

    gtk_box_append(GTK_BOX(box), btns);
    g_object_set_data_full(G_OBJECT(win), "bad", bad, g_free);
    school_dialog_present(win);
}

static void on_create_backup_clicked(GtkButton *btn, gpointer user_data)
{
    (void)btn;
    SettingsWidgets *sw = (SettingsWidgets *)user_data;

    MKDIR("backups");

    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char time_str[32];
    strftime(time_str, sizeof(time_str), "%Y%m%d_%H%M%S", t);

    char backup_path[256];
    snprintf(backup_path, sizeof(backup_path), "backups/schoolcms_backup_%s.db", time_str);

    bool ok = settings_service_create_backup(backup_path);
    if (ok) {
        char msg[512];
        snprintf(msg, sizeof(msg), "Online backup completed successfully: %s", backup_path);
        gtk_label_set_text(GTK_LABEL(sw->status_banner), msg);
        gtk_widget_remove_css_class(sw->status_banner, "card-kpi-red");
        gtk_widget_add_css_class(sw->status_banner, "card-kpi-green");
        gtk_widget_set_visible(sw->status_banner, TRUE);
        gtk_editable_set_text(GTK_EDITABLE(sw->entry_restore_path), backup_path);
        refresh_audit_logs(sw);
        refresh_backup_list(sw);
    } else {
        gtk_label_set_text(GTK_LABEL(sw->status_banner), "Backup generation failed.");
        gtk_widget_remove_css_class(sw->status_banner, "card-kpi-green");
        gtk_widget_add_css_class(sw->status_banner, "card-kpi-red");
        gtk_widget_set_visible(sw->status_banner, TRUE);
    }
}

static void on_restore_backup_clicked(GtkButton *btn, gpointer user_data)
{
    (void)btn;
    SettingsWidgets *sw = (SettingsWidgets *)user_data;

    const char *path = gtk_editable_get_text(GTK_EDITABLE(sw->entry_restore_path));
    if (!path || strlen(path) == 0) {
        gtk_label_set_text(GTK_LABEL(sw->status_banner), "Please specify a backup file path to restore.");
        gtk_widget_remove_css_class(sw->status_banner, "card-kpi-green");
        gtk_widget_add_css_class(sw->status_banner, "card-kpi-red");
        gtk_widget_set_visible(sw->status_banner, TRUE);
        return;
    }

    bool ok = settings_service_restore_backup(path);
    if (ok) {
        char msg[512];
        snprintf(msg, sizeof(msg), "Database restored successfully from: %s", path);
        gtk_label_set_text(GTK_LABEL(sw->status_banner), msg);
        gtk_widget_remove_css_class(sw->status_banner, "card-kpi-red");
        gtk_widget_add_css_class(sw->status_banner, "card-kpi-green");
        gtk_widget_set_visible(sw->status_banner, TRUE);
        load_settings_into_form(sw);
        load_smtp_into_form(sw);
        refresh_audit_logs(sw);
        refresh_backup_list(sw);
        refresh_users_list(sw);
        refresh_role_permissions(sw);
    } else {
        gtk_label_set_text(GTK_LABEL(sw->status_banner), "Failed to restore database from backup file.");
        gtk_widget_remove_css_class(sw->status_banner, "card-kpi-green");
        gtk_widget_add_css_class(sw->status_banner, "card-kpi-red");
        gtk_widget_set_visible(sw->status_banner, TRUE);
    }
}

static void on_refresh_backups_clicked(GtkButton *btn, gpointer user_data)
{
    (void)btn;
    SettingsWidgets *sw = (SettingsWidgets *)user_data;
    refresh_backup_list(sw);
}

static void refresh_backup_list(SettingsWidgets *sw)
{
    if (!sw->backups_box) return;

    GtkWidget *child = gtk_widget_get_first_child(sw->backups_box);
    while (child) {
        GtkWidget *next = gtk_widget_get_next_sibling(child);
        gtk_box_remove(GTK_BOX(sw->backups_box), child);
        child = next;
    }

    BackupInfo backups[64];
    int count = 0;
    settings_service_list_backups(backups, 64, &count);

    long total_bytes = 0;
    for (int i = 0; i < count; i++) {
        total_bytes += backups[i].file_size_bytes;
    }

    char sum_buf[128];
    if (total_bytes < 1024 * 1024) {
        snprintf(sum_buf, sizeof(sum_buf), "%d snapshot(s) found in backups/  (Total: %.1f KB)",
                 count, (double)total_bytes / 1024.0);
    } else {
        snprintf(sum_buf, sizeof(sum_buf), "%d snapshot(s) found in backups/  (Total: %.2f MB)",
                 count, (double)total_bytes / (1024.0 * 1024.0));
    }
    if (sw->lbl_backups_summary) {
        gtk_label_set_text(GTK_LABEL(sw->lbl_backups_summary), sum_buf);
    }

    if (count == 0) {
        GtkWidget *empty_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
        gtk_widget_set_margin_top(empty_box, 8);
        gtk_widget_set_margin_bottom(empty_box, 8);
        GtkWidget *lbl = gtk_label_new("No backup snapshots currently available in the backups/ directory.");
        gtk_widget_add_css_class(lbl, "dim-label");
        gtk_label_set_xalign(GTK_LABEL(lbl), 0.0f);
        gtk_box_append(GTK_BOX(empty_box), lbl);
        gtk_box_append(GTK_BOX(sw->backups_box), empty_box);
        return;
    }

    GtkWidget *hdr_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_add_css_class(hdr_row, "table-header-row");
    gtk_widget_set_margin_top(hdr_row, 6);

    GtkWidget *h_name = gtk_label_new("Backup Filename");
    gtk_label_set_xalign(GTK_LABEL(h_name), 0.0f);
    gtk_widget_set_hexpand(h_name, TRUE);
    gtk_box_append(GTK_BOX(hdr_row), h_name);

    GtkWidget *h_time = gtk_label_new("Created Date & Time");
    gtk_widget_set_size_request(h_time, 160, -1);
    gtk_box_append(GTK_BOX(hdr_row), h_time);

    GtkWidget *h_size = gtk_label_new("File Size");
    gtk_widget_set_size_request(h_size, 90, -1);
    gtk_box_append(GTK_BOX(hdr_row), h_size);

    GtkWidget *h_act = gtk_label_new("Actions");
    gtk_widget_set_size_request(h_act, 160, -1);
    gtk_box_append(GTK_BOX(hdr_row), h_act);

    gtk_box_append(GTK_BOX(sw->backups_box), hdr_row);

    for (int i = 0; i < count; i++) {
        GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
        gtk_widget_add_css_class(row, "table-data-row");

        GtkWidget *fn_lbl = gtk_label_new(backups[i].filename);
        gtk_widget_add_css_class(fn_lbl, "table-cell-bold");
        gtk_label_set_xalign(GTK_LABEL(fn_lbl), 0.0f);
        gtk_widget_set_hexpand(fn_lbl, TRUE);
        gtk_box_append(GTK_BOX(row), fn_lbl);

        GtkWidget *ts_lbl = gtk_label_new(backups[i].created_at);
        gtk_widget_set_size_request(ts_lbl, 160, -1);
        gtk_box_append(GTK_BOX(row), ts_lbl);

        char sz_buf[32];
        if (backups[i].file_size_bytes < 1024) {
            snprintf(sz_buf, sizeof(sz_buf), "%ld B", backups[i].file_size_bytes);
        } else if (backups[i].file_size_bytes < 1024 * 1024) {
            snprintf(sz_buf, sizeof(sz_buf), "%.1f KB", (double)backups[i].file_size_bytes / 1024.0);
        } else {
            snprintf(sz_buf, sizeof(sz_buf), "%.2f MB", (double)backups[i].file_size_bytes / (1024.0 * 1024.0));
        }
        GtkWidget *sz_lbl = gtk_label_new(sz_buf);
        gtk_widget_set_size_request(sz_lbl, 90, -1);
        gtk_box_append(GTK_BOX(row), sz_lbl);

        GtkWidget *btn_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
        gtk_widget_set_size_request(btn_box, 160, -1);

        GtkWidget *btn_rest = gtk_button_new_with_label("Restore");
        gtk_widget_add_css_class(btn_rest, "btn-secondary");
        g_object_set_data_full(G_OBJECT(btn_rest), "backup_path", g_strdup(backups[i].filepath), g_free);
        g_object_set_data_full(G_OBJECT(btn_rest), "backup_name", g_strdup(backups[i].filename), g_free);
        g_signal_connect(btn_rest, "clicked", G_CALLBACK(on_row_restore_clicked), sw);
        gtk_box_append(GTK_BOX(btn_box), btn_rest);

        GtkWidget *btn_del = gtk_button_new_with_label("Delete");
        gtk_widget_add_css_class(btn_del, "btn-action-delete");
        g_object_set_data_full(G_OBJECT(btn_del), "backup_path", g_strdup(backups[i].filepath), g_free);
        g_object_set_data_full(G_OBJECT(btn_del), "backup_name", g_strdup(backups[i].filename), g_free);
        g_signal_connect(btn_del, "clicked", G_CALLBACK(on_row_delete_clicked), sw);
        gtk_box_append(GTK_BOX(btn_box), btn_del);

        gtk_box_append(GTK_BOX(row), btn_box);
        gtk_box_append(GTK_BOX(sw->backups_box), row);
    }
}

static void load_settings_into_form(SettingsWidgets *sw)
{
    settings_service_get(&sw->current_settings);

    gtk_editable_set_text(GTK_EDITABLE(sw->entry_school_name), sw->current_settings.school_name);
    gtk_editable_set_text(GTK_EDITABLE(sw->entry_school_address), sw->current_settings.school_address);
    gtk_editable_set_text(GTK_EDITABLE(sw->entry_school_phone), sw->current_settings.school_phone);
    gtk_editable_set_text(GTK_EDITABLE(sw->entry_school_email), sw->current_settings.school_email);
    gtk_editable_set_text(GTK_EDITABLE(sw->entry_school_website), sw->current_settings.school_website);
    gtk_editable_set_text(GTK_EDITABLE(sw->entry_principal_name), sw->current_settings.principal_name);
    gtk_editable_set_text(GTK_EDITABLE(sw->entry_currency), sw->current_settings.currency_symbol);

    if (strcmp(sw->current_settings.theme_mode, "dark") == 0) {
        gtk_drop_down_set_selected(GTK_DROP_DOWN(sw->combo_theme), 1);
    } else {
        gtk_drop_down_set_selected(GTK_DROP_DOWN(sw->combo_theme), 0);
    }

    sw->ay_count = 0;
    academic_year_get_all(sw->ays, MAX_ACADEMIC_YEARS, &sw->ay_count);
    for (int i = 0; i < sw->ay_count; i++) {
        if (sw->ays[i].id == sw->current_settings.current_academic_year_id) {
            gtk_drop_down_set_selected(GTK_DROP_DOWN(sw->combo_ay), i);
            break;
        }
    }
}

static void refresh_audit_logs(SettingsWidgets *sw)
{
    if (!sw->logs_box) return;

    GtkWidget *child = gtk_widget_get_first_child(sw->logs_box);
    while (child) {
        GtkWidget *next = gtk_widget_get_next_sibling(child);
        gtk_box_remove(GTK_BOX(sw->logs_box), child);
        child = next;
    }

    AuditLogEntry logs[15];
    int count = 0;
    settings_service_get_recent_logs(logs, 15, &count);

    if (count == 0) {
        GtkWidget *lbl = gtk_label_new("No activity recorded yet.");
        gtk_widget_add_css_class(lbl, "dim-label");
        gtk_box_append(GTK_BOX(sw->logs_box), lbl);
        return;
    }

    for (int i = 0; i < count; i++) {
        GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
        gtk_widget_add_css_class(row, "table-data-row");

        GtkWidget *act_lbl = gtk_label_new(logs[i].action);
        gtk_widget_add_css_class(act_lbl, "badge-adm");
        gtk_widget_set_size_request(act_lbl, 140, -1);
        gtk_box_append(GTK_BOX(row), act_lbl);

        GtkWidget *mod_lbl = gtk_label_new(logs[i].module);
        gtk_widget_set_size_request(mod_lbl, 100, -1);
        gtk_box_append(GTK_BOX(row), mod_lbl);

        GtkWidget *det_lbl = gtk_label_new(logs[i].details);
        gtk_label_set_xalign(GTK_LABEL(det_lbl), 0.0f);
        gtk_widget_set_hexpand(det_lbl, TRUE);
        gtk_box_append(GTK_BOX(row), det_lbl);

        GtkWidget *ts_lbl = gtk_label_new(logs[i].created_at);
        gtk_widget_add_css_class(ts_lbl, "dim-label");
        gtk_widget_set_size_request(ts_lbl, 140, -1);
        gtk_box_append(GTK_BOX(row), ts_lbl);

        gtk_box_append(GTK_BOX(sw->logs_box), row);
    }
}

/* ── Category Switching ───────────────────────────────────────────── */

static void switch_settings_tab(SettingsWidgets *sw, SettingsTab tab)
{
    sw->current_tab = tab;

    /* Update button styles */
    for (int i = 0; i < SETTINGS_TAB_COUNT; i++) {
        if (sw->tab_buttons[i]) {
            gtk_widget_remove_css_class(sw->tab_buttons[i], "btn-primary");
            gtk_widget_remove_css_class(sw->tab_buttons[i], "btn-secondary");
            if (i == (int)tab) {
                gtk_widget_add_css_class(sw->tab_buttons[i], "btn-primary");
            } else {
                gtk_widget_add_css_class(sw->tab_buttons[i], "btn-secondary");
            }
        }
    }

    /* Toggle visibility of cards */
    gtk_widget_set_visible(sw->card_prof, tab == SETTINGS_TAB_PROFILE);
    gtk_widget_set_visible(sw->card_security, tab == SETTINGS_TAB_SECURITY);
    gtk_widget_set_visible(sw->card_smtp, tab == SETTINGS_TAB_SMTP);
    gtk_widget_set_visible(sw->card_users, tab == SETTINGS_TAB_USERS);
    gtk_widget_set_visible(sw->card_roles, tab == SETTINGS_TAB_ROLES);
    gtk_widget_set_visible(sw->card_bk, tab == SETTINGS_TAB_BACKUP);
    gtk_widget_set_visible(sw->card_log, tab == SETTINGS_TAB_LOGS);

    /* Contextual refresh */
    if (tab == SETTINGS_TAB_PROFILE) load_settings_into_form(sw);
    else if (tab == SETTINGS_TAB_SMTP) load_smtp_into_form(sw);
    else if (tab == SETTINGS_TAB_USERS) refresh_users_list(sw);
    else if (tab == SETTINGS_TAB_ROLES) refresh_role_permissions(sw);
    else if (tab == SETTINGS_TAB_BACKUP) refresh_backup_list(sw);
    else if (tab == SETTINGS_TAB_LOGS) refresh_audit_logs(sw);
}

static void on_tab_button_clicked(GtkButton *btn, gpointer user_data)
{
    SettingsWidgets *sw = (SettingsWidgets *)user_data;
    SettingsTab tab = (SettingsTab)GPOINTER_TO_INT(g_object_get_data(G_OBJECT(btn), "tab_index"));
    switch_settings_tab(sw, tab);
}

/* ── Public Constructor ───────────────────────────────────────────── */

GtkWidget *school_settings_view_new(void)
{
    SettingsWidgets *sw = g_new0(SettingsWidgets, 1);

    sw->root_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 16);
    gtk_widget_set_hexpand(sw->root_box, TRUE);
    gtk_widget_set_vexpand(sw->root_box, TRUE);
    gtk_widget_set_margin_start(sw->root_box, 24);
    gtk_widget_set_margin_end(sw->root_box, 24);
    gtk_widget_set_margin_top(sw->root_box, 20);
    gtk_widget_set_margin_bottom(sw->root_box, 24);

    g_object_set_data_full(G_OBJECT(sw->root_box), "settings_state", sw, g_free);

    /* ── Header Bar ─────────────────────────────────────────────── */
    GtkWidget *hdr = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_set_valign(hdr, GTK_ALIGN_CENTER);

    GtkWidget *title_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    GtkWidget *title = gtk_label_new("Settings & System Administration");
    gtk_widget_add_css_class(title, "page-title");
    gtk_label_set_xalign(GTK_LABEL(title), 0.0f);
    gtk_box_append(GTK_BOX(title_box), title);

    GtkWidget *sub = gtk_label_new("Institutional profile, Admin Security, Email SMTP Gateway, User RBAC & Disaster Recovery.");
    gtk_widget_add_css_class(sub, "page-subtitle");
    gtk_label_set_xalign(GTK_LABEL(sub), 0.0f);
    gtk_box_append(GTK_BOX(title_box), sub);
    gtk_box_append(GTK_BOX(hdr), title_box);

    GtkWidget *spacer = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_hexpand(spacer, TRUE);
    gtk_box_append(GTK_BOX(hdr), spacer);

    gtk_box_append(GTK_BOX(sw->root_box), hdr);

    /* ── Category Navigation Bar ────────────────────────────────── */
    GtkWidget *cat_bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_margin_top(cat_bar, 4);
    gtk_widget_set_margin_bottom(cat_bar, 4);

    const char *tab_titles[SETTINGS_TAB_COUNT] = {
        "Profile",
        "Account Security",
        "Email SMTP",
        "Users & Roles",
        "Role Permissions",
        "Disaster Recovery",
        "Audit Logs"
    };

    for (int i = 0; i < SETTINGS_TAB_COUNT; i++) {
        GtkWidget *b = gtk_button_new_with_label(tab_titles[i]);
        gtk_widget_add_css_class(b, (i == 0) ? "btn-primary" : "btn-secondary");
        g_object_set_data(G_OBJECT(b), "tab_index", GINT_TO_POINTER(i));
        g_signal_connect(b, "clicked", G_CALLBACK(on_tab_button_clicked), sw);
        gtk_box_append(GTK_BOX(cat_bar), b);
        sw->tab_buttons[i] = b;
    }
    gtk_box_append(GTK_BOX(sw->root_box), cat_bar);

    /* ── Status Banner ──────────────────────────────────────────── */
    sw->status_banner = gtk_label_new("");
    gtk_widget_add_css_class(sw->status_banner, "card");
    gtk_widget_set_visible(sw->status_banner, FALSE);
    gtk_box_append(GTK_BOX(sw->root_box), sw->status_banner);

    /* Scrolled Window for Active Tab Content */
    GtkWidget *scrolled = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_hexpand(scrolled, TRUE);
    gtk_widget_set_vexpand(scrolled, TRUE);
    gtk_scrolled_window_set_min_content_height(GTK_SCROLLED_WINDOW(scrolled), 200);

    GtkWidget *content_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 16);
    gtk_widget_set_hexpand(content_box, TRUE);
    gtk_widget_set_vexpand(content_box, TRUE);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled), content_box);
    gtk_box_append(GTK_BOX(sw->root_box), scrolled);

    /* ═══════════════════════════════════════════════════════════════
       CARD 1: INSTITUTIONAL PROFILE
       ═══════════════════════════════════════════════════════════════ */
    sw->card_prof = gtk_box_new(GTK_ORIENTATION_VERTICAL, 14);
    gtk_widget_add_css_class(sw->card_prof, "card");

    GtkWidget *hdr_prof = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    GtkWidget *lbl_sec1 = gtk_label_new("Institutional Organization Profile");
    gtk_widget_add_css_class(lbl_sec1, "card-title");
    gtk_label_set_xalign(GTK_LABEL(lbl_sec1), 0.0f);
    gtk_widget_set_hexpand(lbl_sec1, TRUE);
    gtk_box_append(GTK_BOX(hdr_prof), lbl_sec1);

    GtkWidget *save_prof_btn = gtk_button_new_with_label("Save Profile");
    gtk_widget_add_css_class(save_prof_btn, "btn-primary");
    g_signal_connect(save_prof_btn, "clicked", G_CALLBACK(on_save_settings_clicked), sw);
    gtk_box_append(GTK_BOX(hdr_prof), save_prof_btn);
    gtk_box_append(GTK_BOX(sw->card_prof), hdr_prof);

    /* Row 1 */
    GtkWidget *r1 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 16);
    GtkWidget *b_name = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_hexpand(b_name, TRUE);
    GtkWidget *l_name = gtk_label_new("School Name:");
    gtk_label_set_xalign(GTK_LABEL(l_name), 0.0f);
    gtk_box_append(GTK_BOX(b_name), l_name);
    sw->entry_school_name = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(sw->entry_school_name), "e.g. St. Mary Higher Secondary School");
    gtk_box_append(GTK_BOX(b_name), sw->entry_school_name);
    gtk_box_append(GTK_BOX(r1), b_name);

    GtkWidget *b_princ = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_hexpand(b_princ, TRUE);
    GtkWidget *l_princ = gtk_label_new("Principal / Director Name:");
    gtk_label_set_xalign(GTK_LABEL(l_princ), 0.0f);
    gtk_box_append(GTK_BOX(b_princ), l_princ);
    sw->entry_principal_name = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(sw->entry_principal_name), "e.g. Dr. Ramesh Kumar");
    gtk_box_append(GTK_BOX(b_princ), sw->entry_principal_name);
    gtk_box_append(GTK_BOX(r1), b_princ);
    gtk_box_append(GTK_BOX(sw->card_prof), r1);

    /* Row 2 */
    GtkWidget *b_addr = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    GtkWidget *l_addr = gtk_label_new("Physical Address:");
    gtk_label_set_xalign(GTK_LABEL(l_addr), 0.0f);
    gtk_box_append(GTK_BOX(b_addr), l_addr);
    sw->entry_school_address = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(sw->entry_school_address), "e.g. 12 School Road, New Delhi - 110001");
    gtk_box_append(GTK_BOX(b_addr), sw->entry_school_address);
    gtk_box_append(GTK_BOX(sw->card_prof), b_addr);

    /* Row 3 */
    GtkWidget *r3 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 16);
    GtkWidget *b_ph = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_hexpand(b_ph, TRUE);
    GtkWidget *l_ph = gtk_label_new("Contact Phone:");
    gtk_label_set_xalign(GTK_LABEL(l_ph), 0.0f);
    gtk_box_append(GTK_BOX(b_ph), l_ph);
    sw->entry_school_phone = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(sw->entry_school_phone), "e.g. +91 9876543210");
    gtk_box_append(GTK_BOX(b_ph), sw->entry_school_phone);
    gtk_box_append(GTK_BOX(r3), b_ph);

    GtkWidget *b_em = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_hexpand(b_em, TRUE);
    GtkWidget *l_em = gtk_label_new("Email Address:");
    gtk_label_set_xalign(GTK_LABEL(l_em), 0.0f);
    gtk_box_append(GTK_BOX(b_em), l_em);
    sw->entry_school_email = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(sw->entry_school_email), "e.g. admin@school.edu.in");
    gtk_box_append(GTK_BOX(b_em), sw->entry_school_email);
    gtk_box_append(GTK_BOX(r3), b_em);

    GtkWidget *b_web = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_hexpand(b_web, TRUE);
    GtkWidget *l_web = gtk_label_new("Official Website:");
    gtk_label_set_xalign(GTK_LABEL(l_web), 0.0f);
    gtk_box_append(GTK_BOX(b_web), l_web);
    sw->entry_school_website = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(sw->entry_school_website), "e.g. https://www.school.edu.in");
    gtk_box_append(GTK_BOX(b_web), sw->entry_school_website);
    gtk_box_append(GTK_BOX(r3), b_web);
    gtk_box_append(GTK_BOX(sw->card_prof), r3);

    /* Row 4 */
    GtkWidget *r4 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 16);
    GtkWidget *b_curr = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_hexpand(b_curr, TRUE);
    GtkWidget *l_curr = gtk_label_new("Currency Symbol:");
    gtk_label_set_xalign(GTK_LABEL(l_curr), 0.0f);
    gtk_box_append(GTK_BOX(b_curr), l_curr);
    sw->entry_currency = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(sw->entry_currency), "e.g. ₹ or $ or INR");
    gtk_box_append(GTK_BOX(b_curr), sw->entry_currency);
    gtk_box_append(GTK_BOX(r4), b_curr);

    GtkWidget *b_ay = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_hexpand(b_ay, TRUE);
    GtkWidget *l_ay = gtk_label_new("Active Academic Year:");
    gtk_label_set_xalign(GTK_LABEL(l_ay), 0.0f);
    gtk_box_append(GTK_BOX(b_ay), l_ay);
    sw->ay_count = 0;
    academic_year_get_all(sw->ays, MAX_ACADEMIC_YEARS, &sw->ay_count);
    const char *ay_names[MAX_ACADEMIC_YEARS + 1];
    for (int i = 0; i < sw->ay_count; i++) ay_names[i] = sw->ays[i].name;
    ay_names[sw->ay_count] = NULL;
    sw->combo_ay = gtk_drop_down_new_from_strings(ay_names);
    gtk_box_append(GTK_BOX(b_ay), sw->combo_ay);
    gtk_box_append(GTK_BOX(r4), b_ay);

    GtkWidget *b_th = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_hexpand(b_th, TRUE);
    GtkWidget *l_th = gtk_label_new("Interface Theme:");
    gtk_label_set_xalign(GTK_LABEL(l_th), 0.0f);
    gtk_box_append(GTK_BOX(b_th), l_th);
    const char *th_opts[] = { "Light Theme", "Dark Theme", NULL };
    sw->combo_theme = gtk_drop_down_new_from_strings(th_opts);
    gtk_box_append(GTK_BOX(b_th), sw->combo_theme);
    gtk_box_append(GTK_BOX(r4), b_th);

    gtk_box_append(GTK_BOX(sw->card_prof), r4);
    gtk_box_append(GTK_BOX(content_box), sw->card_prof);

    /* ═══════════════════════════════════════════════════════════════
       CARD 2: ACCOUNT SECURITY & PASSWORD RESET
       ═══════════════════════════════════════════════════════════════ */
    sw->card_security = gtk_box_new(GTK_ORIENTATION_VERTICAL, 14);
    gtk_widget_add_css_class(sw->card_security, "card");

    GtkWidget *lbl_sec_title = gtk_label_new("Administrator Account Security & Credentials");
    gtk_widget_add_css_class(lbl_sec_title, "card-title");
    gtk_label_set_xalign(GTK_LABEL(lbl_sec_title), 0.0f);
    gtk_box_append(GTK_BOX(sw->card_security), lbl_sec_title);

    GtkWidget *lbl_sec_desc = gtk_label_new("Safely update your login credentials. Passwords are cryptographically salted and hashed using PBKDF2-HMAC-SHA256 (10,000 iterations).");
    gtk_widget_add_css_class(lbl_sec_desc, "dim-label");
    gtk_label_set_xalign(GTK_LABEL(lbl_sec_desc), 0.0f);
    gtk_box_append(GTK_BOX(sw->card_security), lbl_sec_desc);

    GtkWidget *sec_grid = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_margin_top(sec_grid, 6);

    GtkWidget *b_old = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    GtkWidget *l_old = gtk_label_new("Current Password:");
    gtk_label_set_xalign(GTK_LABEL(l_old), 0.0f);
    gtk_box_append(GTK_BOX(b_old), l_old);
    sw->entry_old_pass = gtk_entry_new();
    gtk_entry_set_visibility(GTK_ENTRY(sw->entry_old_pass), FALSE);
    gtk_box_append(GTK_BOX(b_old), sw->entry_old_pass);
    gtk_box_append(GTK_BOX(sec_grid), b_old);

    GtkWidget *b_new = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    GtkWidget *l_new = gtk_label_new("New Password (minimum 6 characters):");
    gtk_label_set_xalign(GTK_LABEL(l_new), 0.0f);
    gtk_box_append(GTK_BOX(b_new), l_new);
    sw->entry_new_pass = gtk_entry_new();
    gtk_entry_set_visibility(GTK_ENTRY(sw->entry_new_pass), FALSE);
    gtk_box_append(GTK_BOX(b_new), sw->entry_new_pass);
    gtk_box_append(GTK_BOX(sec_grid), b_new);

    GtkWidget *b_cfm = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    GtkWidget *l_cfm = gtk_label_new("Confirm New Password:");
    gtk_label_set_xalign(GTK_LABEL(l_cfm), 0.0f);
    gtk_box_append(GTK_BOX(b_cfm), l_cfm);
    sw->entry_confirm_pass = gtk_entry_new();
    gtk_entry_set_visibility(GTK_ENTRY(sw->entry_confirm_pass), FALSE);
    gtk_box_append(GTK_BOX(b_cfm), sw->entry_confirm_pass);
    gtk_box_append(GTK_BOX(sec_grid), b_cfm);

    GtkWidget *btn_upd_pass = gtk_button_new_with_label("Update Password");
    gtk_widget_add_css_class(btn_upd_pass, "btn-primary");
    gtk_widget_set_halign(btn_upd_pass, GTK_ALIGN_START);
    gtk_widget_set_margin_top(btn_upd_pass, 8);
    g_signal_connect(btn_upd_pass, "clicked", G_CALLBACK(on_update_password_clicked), sw);
    gtk_box_append(GTK_BOX(sec_grid), btn_upd_pass);

    gtk_box_append(GTK_BOX(sw->card_security), sec_grid);
    gtk_box_append(GTK_BOX(content_box), sw->card_security);

    /* ═══════════════════════════════════════════════════════════════
       CARD 3: EMAIL SMTP GATEWAY CONFIGURATION
       ═══════════════════════════════════════════════════════════════ */
    sw->card_smtp = gtk_box_new(GTK_ORIENTATION_VERTICAL, 14);
    gtk_widget_add_css_class(sw->card_smtp, "card");

    GtkWidget *lbl_smtp_title = gtk_label_new("Email SMTP Gateway Configuration");
    gtk_widget_add_css_class(lbl_smtp_title, "card-title");
    gtk_label_set_xalign(GTK_LABEL(lbl_smtp_title), 0.0f);
    gtk_box_append(GTK_BOX(sw->card_smtp), lbl_smtp_title);

    GtkWidget *lbl_smtp_desc = gtk_label_new("Configure outbound SMTP credentials to send automated student report cards, fee payment receipts, and attendance notifications.");
    gtk_widget_add_css_class(lbl_smtp_desc, "dim-label");
    gtk_label_set_xalign(GTK_LABEL(lbl_smtp_desc), 0.0f);
    gtk_box_append(GTK_BOX(sw->card_smtp), lbl_smtp_desc);

    GtkWidget *smtp_r1 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 16);
    GtkWidget *b_shost = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_hexpand(b_shost, TRUE);
    GtkWidget *l_shost = gtk_label_new("SMTP Server Host:");
    gtk_label_set_xalign(GTK_LABEL(l_shost), 0.0f);
    gtk_box_append(GTK_BOX(b_shost), l_shost);
    sw->entry_smtp_host = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(sw->entry_smtp_host), "e.g. smtp.gmail.com");
    gtk_box_append(GTK_BOX(b_shost), sw->entry_smtp_host);
    gtk_box_append(GTK_BOX(smtp_r1), b_shost);

    GtkWidget *b_sport = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_size_request(b_sport, 110, -1);
    GtkWidget *l_sport = gtk_label_new("Port:");
    gtk_label_set_xalign(GTK_LABEL(l_sport), 0.0f);
    gtk_box_append(GTK_BOX(b_sport), l_sport);
    sw->entry_smtp_port = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(sw->entry_smtp_port), "587");
    gtk_box_append(GTK_BOX(b_sport), sw->entry_smtp_port);
    gtk_box_append(GTK_BOX(smtp_r1), b_sport);

    GtkWidget *b_scrypto = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_size_request(b_scrypto, 160, -1);
    GtkWidget *l_scrypto = gtk_label_new("Security Protocol:");
    gtk_label_set_xalign(GTK_LABEL(l_scrypto), 0.0f);
    gtk_box_append(GTK_BOX(b_scrypto), l_scrypto);
    const char *crypto_opts[] = { "STARTTLS (Port 587)", "SSL / TLS (Port 465)", "None (Unencrypted)", NULL };
    sw->combo_smtp_crypto = gtk_drop_down_new_from_strings(crypto_opts);
    gtk_box_append(GTK_BOX(b_scrypto), sw->combo_smtp_crypto);
    gtk_box_append(GTK_BOX(smtp_r1), b_scrypto);
    gtk_box_append(GTK_BOX(sw->card_smtp), smtp_r1);

    GtkWidget *smtp_r2 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 16);
    GtkWidget *b_suser = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_hexpand(b_suser, TRUE);
    GtkWidget *l_suser = gtk_label_new("SMTP Username / Email:");
    gtk_label_set_xalign(GTK_LABEL(l_suser), 0.0f);
    gtk_box_append(GTK_BOX(b_suser), l_suser);
    sw->entry_smtp_user = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(sw->entry_smtp_user), "e.g. notifications@apcoaching.edu");
    gtk_box_append(GTK_BOX(b_suser), sw->entry_smtp_user);
    gtk_box_append(GTK_BOX(smtp_r2), b_suser);

    GtkWidget *b_spass = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_hexpand(b_spass, TRUE);
    GtkWidget *l_spass = gtk_label_new("SMTP Password / App Key:");
    gtk_label_set_xalign(GTK_LABEL(l_spass), 0.0f);
    gtk_box_append(GTK_BOX(b_spass), l_spass);
    sw->entry_smtp_pass = gtk_entry_new();
    gtk_entry_set_visibility(GTK_ENTRY(sw->entry_smtp_pass), FALSE);
    gtk_box_append(GTK_BOX(b_spass), sw->entry_smtp_pass);
    gtk_box_append(GTK_BOX(smtp_r2), b_spass);
    gtk_box_append(GTK_BOX(sw->card_smtp), smtp_r2);

    GtkWidget *smtp_r3 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 16);
    GtkWidget *b_ssname = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_hexpand(b_ssname, TRUE);
    GtkWidget *l_ssname = gtk_label_new("Sender Display Name:");
    gtk_label_set_xalign(GTK_LABEL(l_ssname), 0.0f);
    gtk_box_append(GTK_BOX(b_ssname), l_ssname);
    sw->entry_smtp_sender_name = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(sw->entry_smtp_sender_name), "e.g. APCOACHING Administration");
    gtk_box_append(GTK_BOX(b_ssname), sw->entry_smtp_sender_name);
    gtk_box_append(GTK_BOX(smtp_r3), b_ssname);

    GtkWidget *b_ssemail = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_hexpand(b_ssemail, TRUE);
    GtkWidget *l_ssemail = gtk_label_new("Sender From Email:");
    gtk_label_set_xalign(GTK_LABEL(l_ssemail), 0.0f);
    gtk_box_append(GTK_BOX(b_ssemail), l_ssemail);
    sw->entry_smtp_sender_email = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(sw->entry_smtp_sender_email), "e.g. noreply@apcoaching.edu");
    gtk_box_append(GTK_BOX(b_ssemail), sw->entry_smtp_sender_email);
    gtk_box_append(GTK_BOX(smtp_r3), b_ssemail);
    gtk_box_append(GTK_BOX(sw->card_smtp), smtp_r3);

    GtkWidget *smtp_btns = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_set_margin_top(smtp_btns, 8);

    GtkWidget *btn_save_smtp = gtk_button_new_with_label("Save SMTP Settings");
    gtk_widget_add_css_class(btn_save_smtp, "btn-primary");
    g_signal_connect(btn_save_smtp, "clicked", G_CALLBACK(on_save_smtp_clicked), sw);
    gtk_box_append(GTK_BOX(smtp_btns), btn_save_smtp);

    GtkWidget *btn_test_smtp = gtk_button_new_with_label("Test SMTP Connection");
    gtk_widget_add_css_class(btn_test_smtp, "btn-secondary");
    g_signal_connect(btn_test_smtp, "clicked", G_CALLBACK(on_test_smtp_clicked), sw);
    gtk_box_append(GTK_BOX(smtp_btns), btn_test_smtp);

    gtk_box_append(GTK_BOX(sw->card_smtp), smtp_btns);
    gtk_box_append(GTK_BOX(content_box), sw->card_smtp);

    /* ═══════════════════════════════════════════════════════════════
       CARD 4: USER DIRECTORY & ROLE-BASED LOGIN (RBAC)
       ═══════════════════════════════════════════════════════════════ */
    sw->card_users = gtk_box_new(GTK_ORIENTATION_VERTICAL, 14);
    gtk_widget_add_css_class(sw->card_users, "card");

    GtkWidget *hdr_users = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    GtkWidget *lbl_u_title = gtk_label_new("System User Directory & Role Management");
    gtk_widget_add_css_class(lbl_u_title, "card-title");
    gtk_label_set_xalign(GTK_LABEL(lbl_u_title), 0.0f);
    gtk_widget_set_hexpand(lbl_u_title, TRUE);
    gtk_box_append(GTK_BOX(hdr_users), lbl_u_title);

    GtkWidget *btn_add_user = gtk_button_new_with_label("+ Register New User");
    gtk_widget_add_css_class(btn_add_user, "btn-primary");
    g_signal_connect(btn_add_user, "clicked", G_CALLBACK(on_add_new_user_clicked), sw);
    gtk_box_append(GTK_BOX(hdr_users), btn_add_user);
    gtk_box_append(GTK_BOX(sw->card_users), hdr_users);

    GtkWidget *lbl_u_desc = gtk_label_new("Manage authorized operators across Administrator, Teacher, Accountant, and Staff roles. Administrators can activate/deactivate accounts and override forgotten credentials safely.");
    gtk_widget_add_css_class(lbl_u_desc, "dim-label");
    gtk_label_set_xalign(GTK_LABEL(lbl_u_desc), 0.0f);
    gtk_box_append(GTK_BOX(sw->card_users), lbl_u_desc);

    sw->users_table_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_widget_set_margin_top(sw->users_table_box, 6);
    gtk_box_append(GTK_BOX(sw->card_users), sw->users_table_box);

    gtk_box_append(GTK_BOX(content_box), sw->card_users);

    /* ═══════════════════════════════════════════════════════════════
       CARD 5: ROLE PERMISSIONS MATRIX
       ═══════════════════════════════════════════════════════════════ */
    sw->card_roles = gtk_box_new(GTK_ORIENTATION_VERTICAL, 14);
    gtk_widget_add_css_class(sw->card_roles, "card");

    GtkWidget *hdr_roles = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    GtkWidget *lbl_r_title = gtk_label_new("Granular Role Permissions Matrix");
    gtk_widget_add_css_class(lbl_r_title, "card-title");
    gtk_label_set_xalign(GTK_LABEL(lbl_r_title), 0.0f);
    gtk_widget_set_hexpand(lbl_r_title, TRUE);
    gtk_box_append(GTK_BOX(hdr_roles), lbl_r_title);

    GtkWidget *btn_save_perms = gtk_button_new_with_label("Save Permissions");
    gtk_widget_add_css_class(btn_save_perms, "btn-primary");
    g_signal_connect(btn_save_perms, "clicked", G_CALLBACK(on_save_permissions_clicked), sw);
    gtk_box_append(GTK_BOX(hdr_roles), btn_save_perms);
    gtk_box_append(GTK_BOX(sw->card_roles), hdr_roles);

    GtkWidget *lbl_r_desc = gtk_label_new("Configure read/view and write/edit privileges per functional module. Administrator privileges are permanently safeguarded.");
    gtk_widget_add_css_class(lbl_r_desc, "dim-label");
    gtk_label_set_xalign(GTK_LABEL(lbl_r_desc), 0.0f);
    gtk_box_append(GTK_BOX(sw->card_roles), lbl_r_desc);

    GtkWidget *sel_role_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    GtkWidget *lbl_sel = gtk_label_new("Select Role to Inspect / Modify:");
    gtk_widget_add_css_class(lbl_sel, "table-cell-bold");
    gtk_box_append(GTK_BOX(sel_role_box), lbl_sel);

    const char *all_roles[] = { "Administrator", "Teacher", "Accountant", "Staff", NULL };
    sw->combo_perm_role = gtk_drop_down_new_from_strings(all_roles);
    g_signal_connect(sw->combo_perm_role, "notify::selected", G_CALLBACK(on_perm_role_changed), sw);
    gtk_box_append(GTK_BOX(sel_role_box), sw->combo_perm_role);
    gtk_box_append(GTK_BOX(sw->card_roles), sel_role_box);

    /* Permissions Matrix Table */
    GtkWidget *perm_table = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_widget_set_margin_top(perm_table, 6);

    GtkWidget *p_hdr = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_add_css_class(p_hdr, "table-header-row");

    GtkWidget *ph_mod = gtk_label_new("Functional System Module");
    gtk_label_set_xalign(GTK_LABEL(ph_mod), 0.0f);
    gtk_widget_set_hexpand(ph_mod, TRUE);
    gtk_box_append(GTK_BOX(p_hdr), ph_mod);

    GtkWidget *ph_v = gtk_label_new("Can View / Read");
    gtk_widget_set_size_request(ph_v, 140, -1);
    gtk_box_append(GTK_BOX(p_hdr), ph_v);

    GtkWidget *ph_e = gtk_label_new("Can Edit / Manage");
    gtk_widget_set_size_request(ph_e, 140, -1);
    gtk_box_append(GTK_BOX(p_hdr), ph_e);
    gtk_box_append(GTK_BOX(perm_table), p_hdr);

    for (int i = 0; i < 9; i++) {
        GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
        gtk_widget_add_css_class(row, "table-data-row");

        GtkWidget *lbl_m = gtk_label_new(MODULE_LABELS[i]);
        gtk_widget_add_css_class(lbl_m, "table-cell-bold");
        gtk_label_set_xalign(GTK_LABEL(lbl_m), 0.0f);
        gtk_widget_set_hexpand(lbl_m, TRUE);
        gtk_box_append(GTK_BOX(row), lbl_m);

        GtkWidget *bv = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
        gtk_widget_set_size_request(bv, 140, -1);
        gtk_widget_set_halign(bv, GTK_ALIGN_CENTER);
        sw->chk_view[i] = gtk_check_button_new();
        gtk_box_append(GTK_BOX(bv), sw->chk_view[i]);
        gtk_box_append(GTK_BOX(row), bv);

        GtkWidget *be = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
        gtk_widget_set_size_request(be, 140, -1);
        gtk_widget_set_halign(be, GTK_ALIGN_CENTER);
        sw->chk_edit[i] = gtk_check_button_new();
        gtk_box_append(GTK_BOX(be), sw->chk_edit[i]);
        gtk_box_append(GTK_BOX(row), be);

        gtk_box_append(GTK_BOX(perm_table), row);
    }
    gtk_box_append(GTK_BOX(sw->card_roles), perm_table);
    gtk_box_append(GTK_BOX(content_box), sw->card_roles);

    /* ═══════════════════════════════════════════════════════════════
       CARD 6: DATABASE BACKUP & DISASTER RECOVERY
       ═══════════════════════════════════════════════════════════════ */
    sw->card_bk = gtk_box_new(GTK_ORIENTATION_VERTICAL, 14);
    gtk_widget_add_css_class(sw->card_bk, "card");

    GtkWidget *lbl_sec2 = gtk_label_new("Database Backup & Disaster Recovery");
    gtk_widget_add_css_class(lbl_sec2, "card-title");
    gtk_label_set_xalign(GTK_LABEL(lbl_sec2), 0.0f);
    gtk_box_append(GTK_BOX(sw->card_bk), lbl_sec2);

    GtkWidget *bk_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    GtkWidget *btn_backup = gtk_button_new_with_label("Create Backup Now");
    gtk_widget_add_css_class(btn_backup, "btn-primary");
    g_signal_connect(btn_backup, "clicked", G_CALLBACK(on_create_backup_clicked), sw);
    gtk_box_append(GTK_BOX(bk_row), btn_backup);

    GtkWidget *btn_refresh_bk = gtk_button_new_with_label("Refresh List");
    gtk_widget_add_css_class(btn_refresh_bk, "btn-secondary");
    g_signal_connect(btn_refresh_bk, "clicked", G_CALLBACK(on_refresh_backups_clicked), sw);
    gtk_box_append(GTK_BOX(bk_row), btn_refresh_bk);

    sw->lbl_backups_summary = gtk_label_new("Scanning backups...");
    gtk_widget_add_css_class(sw->lbl_backups_summary, "dim-label");
    gtk_label_set_xalign(GTK_LABEL(sw->lbl_backups_summary), 0.0f);
    gtk_widget_set_hexpand(sw->lbl_backups_summary, TRUE);
    gtk_box_append(GTK_BOX(bk_row), sw->lbl_backups_summary);
    gtk_box_append(GTK_BOX(sw->card_bk), bk_row);

    GtkWidget *bk_list_title = gtk_label_new("Available Database Snapshots:");
    gtk_widget_add_css_class(bk_list_title, "table-cell-bold");
    gtk_label_set_xalign(GTK_LABEL(bk_list_title), 0.0f);
    gtk_widget_set_margin_top(bk_list_title, 8);
    gtk_box_append(GTK_BOX(sw->card_bk), bk_list_title);

    sw->backups_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_box_append(GTK_BOX(sw->card_bk), sw->backups_box);

    GtkWidget *sep_bk = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_widget_set_margin_top(sep_bk, 8);
    gtk_widget_set_margin_bottom(sep_bk, 8);
    gtk_box_append(GTK_BOX(sw->card_bk), sep_bk);

    GtkWidget *rest_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    GtkWidget *lbl_rest = gtk_label_new("Custom Path Restore:");
    gtk_label_set_xalign(GTK_LABEL(lbl_rest), 0.0f);
    gtk_box_append(GTK_BOX(rest_box), lbl_rest);

    GtkWidget *r_input = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    sw->entry_restore_path = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(sw->entry_restore_path), "e.g. backups/schoolcms_backup_20261015.db");
    gtk_widget_set_hexpand(sw->entry_restore_path, TRUE);
    gtk_box_append(GTK_BOX(r_input), sw->entry_restore_path);

    GtkWidget *btn_rest = gtk_button_new_with_label("Restore File");
    gtk_widget_add_css_class(btn_rest, "btn-secondary");
    g_signal_connect(btn_rest, "clicked", G_CALLBACK(on_restore_backup_clicked), sw);
    gtk_box_append(GTK_BOX(r_input), btn_rest);

    gtk_box_append(GTK_BOX(rest_box), r_input);
    gtk_box_append(GTK_BOX(sw->card_bk), rest_box);
    gtk_box_append(GTK_BOX(content_box), sw->card_bk);

    /* ═══════════════════════════════════════════════════════════════
       CARD 7: SECURITY AUDIT LOGS
       ═══════════════════════════════════════════════════════════════ */
    sw->card_log = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_add_css_class(sw->card_log, "card");

    GtkWidget *lbl_sec3 = gtk_label_new("Institutional Security & Activity Audit Trail");
    gtk_widget_add_css_class(lbl_sec3, "card-title");
    gtk_label_set_xalign(GTK_LABEL(lbl_sec3), 0.0f);
    gtk_box_append(GTK_BOX(sw->card_log), lbl_sec3);

    sw->logs_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_append(GTK_BOX(sw->card_log), sw->logs_box);
    gtk_box_append(GTK_BOX(content_box), sw->card_log);

    /* Initial state: activate Profile tab */
    switch_settings_tab(sw, SETTINGS_TAB_PROFILE);

    return sw->root_box;
}

void school_settings_view_reload(GtkWidget *view)
{
    if (!view) return;
    SettingsWidgets *sw = (SettingsWidgets *)g_object_get_data(G_OBJECT(view), "settings_state");
    if (sw) {
        load_settings_into_form(sw);
        load_smtp_into_form(sw);
        refresh_audit_logs(sw);
        refresh_backup_list(sw);
        refresh_users_list(sw);
        refresh_role_permissions(sw);
    }
}
