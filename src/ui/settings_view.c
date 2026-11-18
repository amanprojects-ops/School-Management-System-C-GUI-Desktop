/**
 * @file settings_view.c
 * @brief Institutional configuration, theme switching, and backup/restore GUI view implementation.
 */

#include "ui/settings_view.h"
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

typedef struct {
    GtkWidget *root_box;
    GtkWidget *status_banner;

    /* General Profile Entries */
    GtkWidget *entry_school_name;
    GtkWidget *entry_school_address;
    GtkWidget *entry_school_phone;
    GtkWidget *entry_school_email;
    GtkWidget *entry_school_website;
    GtkWidget *entry_principal_name;
    GtkWidget *entry_currency;
    GtkWidget *combo_ay;

    /* Theme Toggle */
    GtkWidget *combo_theme;

    /* Academic Year list */
    AcademicYear ays[MAX_ACADEMIC_YEARS];
    int ay_count;
    int selected_ay_id;

    /* Backup / Restore */
    GtkWidget *entry_restore_path;
    GtkWidget *logs_box;

    SchoolSettings current_settings;
} SettingsWidgets;

static void load_settings_into_form(SettingsWidgets *sw);
static void refresh_audit_logs(SettingsWidgets *sw);

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
        gtk_label_set_text(GTK_LABEL(sw->status_banner), "Institutional settings saved successfully.");
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

/* ── Backup & Restore Handlers ────────────────────────────────────── */

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
        refresh_audit_logs(sw);
    } else {
        gtk_label_set_text(GTK_LABEL(sw->status_banner), "Failed to restore database from backup file.");
        gtk_widget_remove_css_class(sw->status_banner, "card-kpi-green");
        gtk_widget_add_css_class(sw->status_banner, "card-kpi-red");
        gtk_widget_set_visible(sw->status_banner, TRUE);
    }
}

/* ── Form Data Loading & Audit Logs ───────────────────────────────── */

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
    GtkWidget *child = gtk_widget_get_first_child(sw->logs_box);
    while (child) {
        GtkWidget *next = gtk_widget_get_next_sibling(child);
        gtk_box_remove(GTK_BOX(sw->logs_box), child);
        child = next;
    }

    AuditLogEntry logs[10];
    int count = 0;
    settings_service_get_recent_logs(logs, 10, &count);

    if (count == 0) {
        GtkWidget *lbl = gtk_label_new("No activity recorded yet.");
        gtk_widget_add_css_class(lbl, "dim-label");
        gtk_box_append(GTK_BOX(sw->logs_box), lbl);
        return;
    }

    for (int i = 0; i < count; i++) {
        GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
        gtk_widget_add_css_class(row, "table-data-row");

        /* Action Badge */
        GtkWidget *act_lbl = gtk_label_new(logs[i].action);
        gtk_widget_add_css_class(act_lbl, "badge-adm");
        gtk_widget_set_size_request(act_lbl, 130, -1);
        gtk_box_append(GTK_BOX(row), act_lbl);

        /* Module */
        GtkWidget *mod_lbl = gtk_label_new(logs[i].module);
        gtk_widget_set_size_request(mod_lbl, 100, -1);
        gtk_box_append(GTK_BOX(row), mod_lbl);

        /* Details */
        GtkWidget *det_lbl = gtk_label_new(logs[i].details);
        gtk_label_set_xalign(GTK_LABEL(det_lbl), 0.0f);
        gtk_widget_set_hexpand(det_lbl, TRUE);
        gtk_box_append(GTK_BOX(row), det_lbl);

        /* Timestamp */
        GtkWidget *ts_lbl = gtk_label_new(logs[i].created_at);
        gtk_widget_add_css_class(ts_lbl, "dim-label");
        gtk_widget_set_size_request(ts_lbl, 140, -1);
        gtk_box_append(GTK_BOX(row), ts_lbl);

        gtk_box_append(GTK_BOX(sw->logs_box), row);
    }
}

/* ── Constructor ─────────────────────────────────────────────────── */

GtkWidget *school_settings_view_new(void)
{
    SettingsWidgets *sw = g_new0(SettingsWidgets, 1);

    sw->root_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 16);
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

    GtkWidget *sub = gtk_label_new("Institutional profile, academic year, UI theme, and SQLite disaster recovery.");
    gtk_widget_add_css_class(sub, "page-subtitle");
    gtk_label_set_xalign(GTK_LABEL(sub), 0.0f);
    gtk_box_append(GTK_BOX(title_box), sub);
    gtk_box_append(GTK_BOX(hdr), title_box);

    GtkWidget *spacer = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_hexpand(spacer, TRUE);
    gtk_box_append(GTK_BOX(hdr), spacer);

    GtkWidget *save_btn = gtk_button_new_with_label("Save Changes");
    gtk_widget_add_css_class(save_btn, "btn-primary");
    g_signal_connect(save_btn, "clicked", G_CALLBACK(on_save_settings_clicked), sw);
    gtk_box_append(GTK_BOX(hdr), save_btn);

    gtk_box_append(GTK_BOX(sw->root_box), hdr);

    /* ── Status Banner ──────────────────────────────────────────── */
    sw->status_banner = gtk_label_new("");
    gtk_widget_add_css_class(sw->status_banner, "card");
    gtk_widget_set_visible(sw->status_banner, FALSE);
    gtk_box_append(GTK_BOX(sw->root_box), sw->status_banner);

    /* Scrolled Window for Settings Layout */
    GtkWidget *scrolled = gtk_scrolled_window_new();
    gtk_scrolled_window_set_min_content_height(GTK_SCROLLED_WINDOW(scrolled), 520);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

    GtkWidget *content_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 16);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled), content_box);
    gtk_box_append(GTK_BOX(sw->root_box), scrolled);

    /* ── 1. General Institutional Profile Card ──────────────────── */
    GtkWidget *card_prof = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_add_css_class(card_prof, "card");

    GtkWidget *lbl_sec1 = gtk_label_new("Institutional Profile");
    gtk_widget_add_css_class(lbl_sec1, "card-title");
    gtk_label_set_xalign(GTK_LABEL(lbl_sec1), 0.0f);
    gtk_box_append(GTK_BOX(card_prof), lbl_sec1);

    /* Row 1: School Name & Principal */
    GtkWidget *r1 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 16);
    GtkWidget *b_name = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_hexpand(b_name, TRUE);
    GtkWidget *l_name = gtk_label_new("School Name:");
    gtk_label_set_xalign(GTK_LABEL(l_name), 0.0f);
    gtk_box_append(GTK_BOX(b_name), l_name);
    sw->entry_school_name = gtk_entry_new();
    gtk_box_append(GTK_BOX(b_name), sw->entry_school_name);
    gtk_box_append(GTK_BOX(r1), b_name);

    GtkWidget *b_princ = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_hexpand(b_princ, TRUE);
    GtkWidget *l_princ = gtk_label_new("Principal Name:");
    gtk_label_set_xalign(GTK_LABEL(l_princ), 0.0f);
    gtk_box_append(GTK_BOX(b_princ), l_princ);
    sw->entry_principal_name = gtk_entry_new();
    gtk_box_append(GTK_BOX(b_princ), sw->entry_principal_name);
    gtk_box_append(GTK_BOX(r1), b_princ);
    gtk_box_append(GTK_BOX(card_prof), r1);

    /* Row 2: Address */
    GtkWidget *b_addr = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    GtkWidget *l_addr = gtk_label_new("Address:");
    gtk_label_set_xalign(GTK_LABEL(l_addr), 0.0f);
    gtk_box_append(GTK_BOX(b_addr), l_addr);
    sw->entry_school_address = gtk_entry_new();
    gtk_box_append(GTK_BOX(b_addr), sw->entry_school_address);
    gtk_box_append(GTK_BOX(card_prof), b_addr);

    /* Row 3: Phone, Email, Website */
    GtkWidget *r3 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 16);
    GtkWidget *b_ph = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_hexpand(b_ph, TRUE);
    GtkWidget *l_ph = gtk_label_new("Contact Phone:");
    gtk_label_set_xalign(GTK_LABEL(l_ph), 0.0f);
    gtk_box_append(GTK_BOX(b_ph), l_ph);
    sw->entry_school_phone = gtk_entry_new();
    gtk_box_append(GTK_BOX(b_ph), sw->entry_school_phone);
    gtk_box_append(GTK_BOX(r3), b_ph);

    GtkWidget *b_em = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_hexpand(b_em, TRUE);
    GtkWidget *l_em = gtk_label_new("Email Address:");
    gtk_label_set_xalign(GTK_LABEL(l_em), 0.0f);
    gtk_box_append(GTK_BOX(b_em), l_em);
    sw->entry_school_email = gtk_entry_new();
    gtk_box_append(GTK_BOX(b_em), sw->entry_school_email);
    gtk_box_append(GTK_BOX(r3), b_em);

    GtkWidget *b_web = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_hexpand(b_web, TRUE);
    GtkWidget *l_web = gtk_label_new("Official Website:");
    gtk_label_set_xalign(GTK_LABEL(l_web), 0.0f);
    gtk_box_append(GTK_BOX(b_web), l_web);
    sw->entry_school_website = gtk_entry_new();
    gtk_box_append(GTK_BOX(b_web), sw->entry_school_website);
    gtk_box_append(GTK_BOX(r3), b_web);
    gtk_box_append(GTK_BOX(card_prof), r3);

    /* Row 4: Currency, Academic Year, Theme */
    GtkWidget *r4 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 16);
    GtkWidget *b_curr = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_hexpand(b_curr, TRUE);
    GtkWidget *l_curr = gtk_label_new("Currency Symbol:");
    gtk_label_set_xalign(GTK_LABEL(l_curr), 0.0f);
    gtk_box_append(GTK_BOX(b_curr), l_curr);
    sw->entry_currency = gtk_entry_new();
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
    for (int i = 0; i < sw->ay_count; i++) {
        ay_names[i] = sw->ays[i].name;
    }
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

    gtk_box_append(GTK_BOX(card_prof), r4);
    gtk_box_append(GTK_BOX(content_box), card_prof);

    /* ── 2. Database Backup & Disaster Recovery ─────────────────── */
    GtkWidget *card_bk = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_add_css_class(card_bk, "card");

    GtkWidget *lbl_sec2 = gtk_label_new("Database Backup & Disaster Recovery");
    gtk_widget_add_css_class(lbl_sec2, "card-title");
    gtk_label_set_xalign(GTK_LABEL(lbl_sec2), 0.0f);
    gtk_box_append(GTK_BOX(card_bk), lbl_sec2);

    GtkWidget *bk_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 16);
    GtkWidget *btn_backup = gtk_button_new_with_label("Create Backup Now");
    gtk_widget_add_css_class(btn_backup, "btn-primary");
    g_signal_connect(btn_backup, "clicked", G_CALLBACK(on_create_backup_clicked), sw);
    gtk_box_append(GTK_BOX(bk_row), btn_backup);

    GtkWidget *lbl_bk_hint = gtk_label_new("Creates an atomic online snapshot in the backups/ directory.");
    gtk_widget_add_css_class(lbl_bk_hint, "dim-label");
    gtk_box_append(GTK_BOX(bk_row), lbl_bk_hint);
    gtk_box_append(GTK_BOX(card_bk), bk_row);

    /* Restore Row */
    GtkWidget *rest_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_margin_top(rest_box, 8);
    GtkWidget *lbl_rest = gtk_label_new("Restore from Backup File:");
    gtk_label_set_xalign(GTK_LABEL(lbl_rest), 0.0f);
    gtk_box_append(GTK_BOX(rest_box), lbl_rest);

    GtkWidget *r_input = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    sw->entry_restore_path = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(sw->entry_restore_path), "e.g. backups/schoolcms_backup_20261015.db");
    gtk_widget_set_hexpand(sw->entry_restore_path, TRUE);
    gtk_box_append(GTK_BOX(r_input), sw->entry_restore_path);

    GtkWidget *btn_rest = gtk_button_new_with_label("Restore Database");
    gtk_widget_add_css_class(btn_rest, "btn-secondary");
    g_signal_connect(btn_rest, "clicked", G_CALLBACK(on_restore_backup_clicked), sw);
    gtk_box_append(GTK_BOX(r_input), btn_rest);

    gtk_box_append(GTK_BOX(rest_box), r_input);
    gtk_box_append(GTK_BOX(card_bk), rest_box);
    gtk_box_append(GTK_BOX(content_box), card_bk);

    /* ── 3. Audit Activity Log ──────────────────────────────────── */
    GtkWidget *card_log = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_add_css_class(card_log, "card");

    GtkWidget *lbl_sec3 = gtk_label_new("Recent Security & Activity Logs");
    gtk_widget_add_css_class(lbl_sec3, "card-title");
    gtk_label_set_xalign(GTK_LABEL(lbl_sec3), 0.0f);
    gtk_box_append(GTK_BOX(card_log), lbl_sec3);

    sw->logs_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_append(GTK_BOX(card_log), sw->logs_box);
    gtk_box_append(GTK_BOX(content_box), card_log);

    /* Initial Load */
    load_settings_into_form(sw);
    refresh_audit_logs(sw);

    return sw->root_box;
}

void school_settings_view_reload(GtkWidget *view)
{
    if (!view) return;
    SettingsWidgets *sw = (SettingsWidgets *)g_object_get_data(G_OBJECT(view), "settings_state");
    if (sw) {
        load_settings_into_form(sw);
        refresh_audit_logs(sw);
    }
}
