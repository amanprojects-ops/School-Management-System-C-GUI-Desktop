/**
 * @file teacher_view.c
 * @brief Teacher and staff management GUI module implementation.
 */

#include "ui/teacher_view.h"
#include "ui/dialog.h"
#include "teacher.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum {
    TAB_TEACHERS = 0,
    TAB_STAFF
} ActiveTab;

typedef struct {
    GtkWidget *root_box;
    GtkWidget *search_entry;
    GtkWidget *status_combo;
    GtkWidget *tab_teachers_btn;
    GtkWidget *tab_staff_btn;
    GtkWidget *table_container;
    GtkWidget *status_banner;
    ActiveTab current_tab;
    char search_text[128];
    char status_text[16];
} TeacherViewWidgets;

static void refresh_table(TeacherViewWidgets *tvw);
static void show_add_edit_teacher_dialog(TeacherViewWidgets *tvw, int teacher_id);
static void show_add_edit_staff_dialog(TeacherViewWidgets *tvw, int staff_id);
static void show_teacher_profile(int teacher_id);
static void show_staff_profile(int staff_id);
static void show_delete_dialog(TeacherViewWidgets *tvw, bool is_teacher, int id, const char *name);

/* ── Callbacks ────────────────────────────────────────────────────── */

static void on_search_changed(GtkSearchEntry *entry, gpointer user_data)
{
    TeacherViewWidgets *tvw = (TeacherViewWidgets *)user_data;
    const char *text = gtk_editable_get_text(GTK_EDITABLE(entry));
    if (text) strncpy(tvw->search_text, text, sizeof(tvw->search_text) - 1);
    else tvw->search_text[0] = '\0';
    refresh_table(tvw);
}

static void on_status_changed(GtkDropDown *dropdown, GParamSpec *pspec, gpointer user_data)
{
    (void)pspec;
    TeacherViewWidgets *tvw = (TeacherViewWidgets *)user_data;
    guint selected = gtk_drop_down_get_selected(dropdown);
    switch (selected) {
        case 1: strcpy(tvw->status_text, "ACTIVE"); break;
        case 2: strcpy(tvw->status_text, "INACTIVE"); break;
        default: tvw->status_text[0] = '\0'; break;
    }
    refresh_table(tvw);
}

static void show_status_message(TeacherViewWidgets *tvw, const char *msg, bool is_success)
{
    if (!tvw || !tvw->status_banner) return;
    gtk_label_set_text(GTK_LABEL(tvw->status_banner), msg);
    if (is_success) {
        gtk_widget_remove_css_class(tvw->status_banner, "card-kpi-red");
        gtk_widget_add_css_class(tvw->status_banner, "card-kpi-green");
    } else {
        gtk_widget_remove_css_class(tvw->status_banner, "card-kpi-green");
        gtk_widget_add_css_class(tvw->status_banner, "card-kpi-red");
    }
    gtk_widget_set_visible(tvw->status_banner, TRUE);
}

static void on_tab_teachers_clicked(GtkButton *btn, gpointer user_data)
{
    (void)btn;
    TeacherViewWidgets *tvw = (TeacherViewWidgets *)user_data;
    tvw->current_tab = TAB_TEACHERS;
    if (tvw->status_banner) gtk_widget_set_visible(tvw->status_banner, FALSE);
    gtk_widget_add_css_class(tvw->tab_teachers_btn, "tab-btn-active");
    gtk_widget_remove_css_class(tvw->tab_staff_btn, "tab-btn-active");
    refresh_table(tvw);
}

static void on_tab_staff_clicked(GtkButton *btn, gpointer user_data)
{
    (void)btn;
    TeacherViewWidgets *tvw = (TeacherViewWidgets *)user_data;
    tvw->current_tab = TAB_STAFF;
    if (tvw->status_banner) gtk_widget_set_visible(tvw->status_banner, FALSE);
    gtk_widget_add_css_class(tvw->tab_staff_btn, "tab-btn-active");
    gtk_widget_remove_css_class(tvw->tab_teachers_btn, "tab-btn-active");
    refresh_table(tvw);
}

static void on_add_clicked(GtkButton *btn, gpointer user_data)
{
    (void)btn;
    TeacherViewWidgets *tvw = (TeacherViewWidgets *)user_data;
    if (tvw->current_tab == TAB_TEACHERS) {
        show_add_edit_teacher_dialog(tvw, 0);
    } else {
        show_add_edit_staff_dialog(tvw, 0);
    }
}

static void on_view_item(GtkButton *btn, gpointer user_data)
{
    (void)btn;
    TeacherViewWidgets *tvw = (TeacherViewWidgets *)g_object_get_data(G_OBJECT(btn), "tvw");
    int id = GPOINTER_TO_INT(user_data);
    if (tvw->current_tab == TAB_TEACHERS) show_teacher_profile(id);
    else show_staff_profile(id);
}

static void on_edit_item(GtkButton *btn, gpointer user_data)
{
    (void)btn;
    TeacherViewWidgets *tvw = (TeacherViewWidgets *)g_object_get_data(G_OBJECT(btn), "tvw");
    int id = GPOINTER_TO_INT(user_data);
    if (tvw->current_tab == TAB_TEACHERS) show_add_edit_teacher_dialog(tvw, id);
    else show_add_edit_staff_dialog(tvw, id);
}

static void on_delete_item(GtkButton *btn, gpointer user_data)
{
    (void)btn;
    TeacherViewWidgets *tvw = (TeacherViewWidgets *)g_object_get_data(G_OBJECT(btn), "tvw");
    int id = GPOINTER_TO_INT(user_data);
    const char *name = (const char *)g_object_get_data(G_OBJECT(btn), "item_name");
    show_delete_dialog(tvw, (tvw->current_tab == TAB_TEACHERS), id, name);
}

/* ── Table Rendering ──────────────────────────────────────────────── */

static void render_teachers_table(TeacherViewWidgets *tvw)
{
    Teacher list[TEACHER_MAX_LIST];
    int count = 0;
    teacher_service_get_all(tvw->search_text, tvw->status_text, list, TEACHER_MAX_LIST, &count);

    /* Table Header */
    GtkWidget *header_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_add_css_class(header_row, "table-header-row");

    const char *cols[] = { "Code", "Teacher Name", "Subject", "Qualification", "Contact", "Salary", "Status", "Actions" };
    int widths[] = { 120, 180, 140, 130, 140, 100, 90, 110 };

    for (int i = 0; i < 8; i++) {
        GtkWidget *th = gtk_label_new(cols[i]);
        gtk_widget_add_css_class(th, "table-th");
        gtk_label_set_xalign(GTK_LABEL(th), 0.0f);
        gtk_widget_set_size_request(th, widths[i], -1);
        if (i == 1) gtk_widget_set_hexpand(th, TRUE);
        gtk_box_append(GTK_BOX(header_row), th);
    }
    gtk_box_append(GTK_BOX(tvw->table_container), header_row);

    if (count == 0) {
        GtkWidget *empty = gtk_label_new("No teachers match your search.");
        gtk_widget_add_css_class(empty, "dim-label");
        gtk_widget_set_margin_top(empty, 30);
        gtk_widget_set_margin_bottom(empty, 30);
        gtk_box_append(GTK_BOX(tvw->table_container), empty);
        return;
    }

    for (int i = 0; i < count; i++) {
        const Teacher *t = &list[i];
        GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
        gtk_widget_add_css_class(row, "table-data-row");

        /* Code Badge */
        GtkWidget *code_lbl = gtk_label_new(t->teacher_code);
        gtk_widget_add_css_class(code_lbl, "badge-adm");
        gtk_widget_set_size_request(code_lbl, widths[0], -1);
        gtk_label_set_xalign(GTK_LABEL(code_lbl), 0.0f);
        gtk_box_append(GTK_BOX(row), code_lbl);

        /* Full Name */
        GtkWidget *name_lbl = gtk_label_new(t->full_name);
        gtk_widget_add_css_class(name_lbl, "table-cell-bold");
        gtk_label_set_xalign(GTK_LABEL(name_lbl), 0.0f);
        gtk_widget_set_hexpand(name_lbl, TRUE);
        gtk_box_append(GTK_BOX(row), name_lbl);

        /* Subject */
        GtkWidget *subj_lbl = gtk_label_new(t->subject);
        gtk_label_set_xalign(GTK_LABEL(subj_lbl), 0.0f);
        gtk_widget_set_size_request(subj_lbl, widths[2], -1);
        gtk_box_append(GTK_BOX(row), subj_lbl);

        /* Qualification */
        GtkWidget *qual_lbl = gtk_label_new(t->qualification);
        gtk_label_set_xalign(GTK_LABEL(qual_lbl), 0.0f);
        gtk_widget_set_size_request(qual_lbl, widths[3], -1);
        gtk_box_append(GTK_BOX(row), qual_lbl);

        /* Phone */
        GtkWidget *ph_lbl = gtk_label_new(t->phone);
        gtk_label_set_xalign(GTK_LABEL(ph_lbl), 0.0f);
        gtk_widget_set_size_request(ph_lbl, widths[4], -1);
        gtk_box_append(GTK_BOX(row), ph_lbl);

        /* Salary */
        char sal_buf[32];
        snprintf(sal_buf, sizeof(sal_buf), "₹%.2f", t->salary);
        GtkWidget *sal_lbl = gtk_label_new(sal_buf);
        gtk_label_set_xalign(GTK_LABEL(sal_lbl), 0.0f);
        gtk_widget_set_size_request(sal_lbl, widths[5], -1);
        gtk_box_append(GTK_BOX(row), sal_lbl);

        /* Status */
        GtkWidget *st_lbl = gtk_label_new(t->status);
        gtk_widget_add_css_class(st_lbl, (strcmp(t->status, "ACTIVE") == 0) ? "badge-success" : "badge-warning");
        gtk_widget_set_size_request(st_lbl, widths[6], -1);
        gtk_box_append(GTK_BOX(row), st_lbl);

        /* Actions */
        GtkWidget *acts_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
        gtk_widget_set_size_request(acts_box, widths[7], -1);

        GtkWidget *v_btn = gtk_button_new_from_icon_name("avatar-default-symbolic");
        gtk_button_set_has_frame(GTK_BUTTON(v_btn), FALSE);
        gtk_widget_add_css_class(v_btn, "btn-action-view");
        g_object_set_data(G_OBJECT(v_btn), "tvw", tvw);
        g_signal_connect(v_btn, "clicked", G_CALLBACK(on_view_item), GINT_TO_POINTER(t->id));
        gtk_box_append(GTK_BOX(acts_box), v_btn);

        GtkWidget *e_btn = gtk_button_new_from_icon_name("document-edit-symbolic");
        gtk_button_set_has_frame(GTK_BUTTON(e_btn), FALSE);
        gtk_widget_add_css_class(e_btn, "btn-action-edit");
        g_object_set_data(G_OBJECT(e_btn), "tvw", tvw);
        g_signal_connect(e_btn, "clicked", G_CALLBACK(on_edit_item), GINT_TO_POINTER(t->id));
        gtk_box_append(GTK_BOX(acts_box), e_btn);

        GtkWidget *d_btn = gtk_button_new_from_icon_name("user-trash-symbolic");
        gtk_button_set_has_frame(GTK_BUTTON(d_btn), FALSE);
        gtk_widget_add_css_class(d_btn, "btn-action-delete");
        g_object_set_data(G_OBJECT(d_btn), "tvw", tvw);
        g_object_set_data_full(G_OBJECT(d_btn), "item_name", g_strdup(t->full_name), g_free);
        g_signal_connect(d_btn, "clicked", G_CALLBACK(on_delete_item), GINT_TO_POINTER(t->id));
        gtk_box_append(GTK_BOX(acts_box), d_btn);

        gtk_box_append(GTK_BOX(row), acts_box);
        gtk_box_append(GTK_BOX(tvw->table_container), row);
    }
}

static void render_staff_table(TeacherViewWidgets *tvw)
{
    Staff list[STAFF_MAX_LIST];
    int count = 0;
    staff_service_get_all(tvw->search_text, tvw->status_text, list, STAFF_MAX_LIST, &count);

    /* Table Header */
    GtkWidget *header_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_add_css_class(header_row, "table-header-row");

    const char *cols[] = { "Staff Code", "Staff Name", "Role / Title", "Contact Phone", "Email", "Salary", "Status", "Actions" };
    int widths[] = { 120, 180, 140, 130, 140, 100, 90, 110 };

    for (int i = 0; i < 8; i++) {
        GtkWidget *th = gtk_label_new(cols[i]);
        gtk_widget_add_css_class(th, "table-th");
        gtk_label_set_xalign(GTK_LABEL(th), 0.0f);
        gtk_widget_set_size_request(th, widths[i], -1);
        if (i == 1) gtk_widget_set_hexpand(th, TRUE);
        gtk_box_append(GTK_BOX(header_row), th);
    }
    gtk_box_append(GTK_BOX(tvw->table_container), header_row);

    if (count == 0) {
        GtkWidget *empty = gtk_label_new("No staff members match your search.");
        gtk_widget_add_css_class(empty, "dim-label");
        gtk_widget_set_margin_top(empty, 30);
        gtk_widget_set_margin_bottom(empty, 30);
        gtk_box_append(GTK_BOX(tvw->table_container), empty);
        return;
    }

    for (int i = 0; i < count; i++) {
        const Staff *s = &list[i];
        GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
        gtk_widget_add_css_class(row, "table-data-row");

        /* Code Badge */
        GtkWidget *code_lbl = gtk_label_new(s->staff_code);
        gtk_widget_add_css_class(code_lbl, "badge-adm");
        gtk_widget_set_size_request(code_lbl, widths[0], -1);
        gtk_label_set_xalign(GTK_LABEL(code_lbl), 0.0f);
        gtk_box_append(GTK_BOX(row), code_lbl);

        /* Full Name */
        GtkWidget *name_lbl = gtk_label_new(s->full_name);
        gtk_widget_add_css_class(name_lbl, "table-cell-bold");
        gtk_label_set_xalign(GTK_LABEL(name_lbl), 0.0f);
        gtk_widget_set_hexpand(name_lbl, TRUE);
        gtk_box_append(GTK_BOX(row), name_lbl);

        /* Role Title */
        GtkWidget *role_lbl = gtk_label_new(s->role_title);
        gtk_label_set_xalign(GTK_LABEL(role_lbl), 0.0f);
        gtk_widget_set_size_request(role_lbl, widths[2], -1);
        gtk_box_append(GTK_BOX(row), role_lbl);

        /* Phone */
        GtkWidget *ph_lbl = gtk_label_new(s->phone);
        gtk_label_set_xalign(GTK_LABEL(ph_lbl), 0.0f);
        gtk_widget_set_size_request(ph_lbl, widths[3], -1);
        gtk_box_append(GTK_BOX(row), ph_lbl);

        /* Email */
        GtkWidget *em_lbl = gtk_label_new(s->email);
        gtk_label_set_xalign(GTK_LABEL(em_lbl), 0.0f);
        gtk_widget_set_size_request(em_lbl, widths[4], -1);
        gtk_box_append(GTK_BOX(row), em_lbl);

        /* Salary */
        char sal_buf[32];
        snprintf(sal_buf, sizeof(sal_buf), "₹%.2f", s->salary);
        GtkWidget *sal_lbl = gtk_label_new(sal_buf);
        gtk_label_set_xalign(GTK_LABEL(sal_lbl), 0.0f);
        gtk_widget_set_size_request(sal_lbl, widths[5], -1);
        gtk_box_append(GTK_BOX(row), sal_lbl);

        /* Status */
        GtkWidget *st_lbl = gtk_label_new(s->status);
        gtk_widget_add_css_class(st_lbl, (strcmp(s->status, "ACTIVE") == 0) ? "badge-success" : "badge-warning");
        gtk_widget_set_size_request(st_lbl, widths[6], -1);
        gtk_box_append(GTK_BOX(row), st_lbl);

        /* Actions */
        GtkWidget *acts_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
        gtk_widget_set_size_request(acts_box, widths[7], -1);

        GtkWidget *v_btn = gtk_button_new_from_icon_name("avatar-default-symbolic");
        gtk_button_set_has_frame(GTK_BUTTON(v_btn), FALSE);
        gtk_widget_add_css_class(v_btn, "btn-action-view");
        g_object_set_data(G_OBJECT(v_btn), "tvw", tvw);
        g_signal_connect(v_btn, "clicked", G_CALLBACK(on_view_item), GINT_TO_POINTER(s->id));
        gtk_box_append(GTK_BOX(acts_box), v_btn);

        GtkWidget *e_btn = gtk_button_new_from_icon_name("document-edit-symbolic");
        gtk_button_set_has_frame(GTK_BUTTON(e_btn), FALSE);
        gtk_widget_add_css_class(e_btn, "btn-action-edit");
        g_object_set_data(G_OBJECT(e_btn), "tvw", tvw);
        g_signal_connect(e_btn, "clicked", G_CALLBACK(on_edit_item), GINT_TO_POINTER(s->id));
        gtk_box_append(GTK_BOX(acts_box), e_btn);

        GtkWidget *d_btn = gtk_button_new_from_icon_name("user-trash-symbolic");
        gtk_button_set_has_frame(GTK_BUTTON(d_btn), FALSE);
        gtk_widget_add_css_class(d_btn, "btn-action-delete");
        g_object_set_data(G_OBJECT(d_btn), "tvw", tvw);
        g_object_set_data_full(G_OBJECT(d_btn), "item_name", g_strdup(s->full_name), g_free);
        g_signal_connect(d_btn, "clicked", G_CALLBACK(on_delete_item), GINT_TO_POINTER(s->id));
        gtk_box_append(GTK_BOX(acts_box), d_btn);

        gtk_box_append(GTK_BOX(row), acts_box);
        gtk_box_append(GTK_BOX(tvw->table_container), row);
    }
}

static void refresh_table(TeacherViewWidgets *tvw)
{
    /* Clear current container */
    GtkWidget *child = gtk_widget_get_first_child(tvw->table_container);
    while (child) {
        GtkWidget *next = gtk_widget_get_next_sibling(child);
        gtk_box_remove(GTK_BOX(tvw->table_container), child);
        child = next;
    }

    if (tvw->current_tab == TAB_TEACHERS) render_teachers_table(tvw);
    else render_staff_table(tvw);
}

/* ── Dialogs ──────────────────────────────────────────────────────── */

typedef struct {
    GtkWidget *dialog;
    TeacherViewWidgets *tvw;
    int id;
    GtkWidget *entry_name;
    GtkWidget *entry_phone;
    GtkWidget *entry_email;
    GtkWidget *entry_addr;
    GtkWidget *entry_qual_or_role;
    GtkWidget *entry_subj;
    GtkWidget *entry_salary;
    GtkWidget *combo_status;
} StaffFormDialog;

static void on_save_teacher(GtkButton *btn, gpointer user_data)
{
    (void)btn;
    StaffFormDialog *sfd = (StaffFormDialog *)user_data;
    const char *name = gtk_editable_get_text(GTK_EDITABLE(sfd->entry_name));
    if (!name || strlen(name) == 0) return;

    Teacher t = {0};
    t.id = sfd->id;
    strncpy(t.full_name, name, sizeof(t.full_name) - 1);
    strncpy(t.phone, gtk_editable_get_text(GTK_EDITABLE(sfd->entry_phone)), sizeof(t.phone) - 1);
    strncpy(t.email, gtk_editable_get_text(GTK_EDITABLE(sfd->entry_email)), sizeof(t.email) - 1);
    strncpy(t.address, gtk_editable_get_text(GTK_EDITABLE(sfd->entry_addr)), sizeof(t.address) - 1);
    strncpy(t.qualification, gtk_editable_get_text(GTK_EDITABLE(sfd->entry_qual_or_role)), sizeof(t.qualification) - 1);
    strncpy(t.subject, gtk_editable_get_text(GTK_EDITABLE(sfd->entry_subj)), sizeof(t.subject) - 1);
    t.salary = atof(gtk_editable_get_text(GTK_EDITABLE(sfd->entry_salary)));

    guint st_sel = gtk_drop_down_get_selected(GTK_DROP_DOWN(sfd->combo_status));
    strcpy(t.status, (st_sel == 1) ? "INACTIVE" : "ACTIVE");

    if (sfd->id > 0) {
        if (teacher_service_update(&t)) {
            show_status_message(sfd->tvw, "Teacher details updated successfully.", true);
        } else {
            show_status_message(sfd->tvw, "Failed to update teacher.", false);
        }
    } else {
        int new_id = 0;
        if (teacher_service_create(&t, &new_id)) {
            show_status_message(sfd->tvw, "New teacher added successfully.", true);
        } else {
            show_status_message(sfd->tvw, "Failed to add teacher.", false);
        }
    }

    refresh_table(sfd->tvw);
    school_dialog_close(sfd->dialog);
}

static void show_add_edit_teacher_dialog(TeacherViewWidgets *tvw, int teacher_id)
{
    StaffFormDialog *sfd = g_new0(StaffFormDialog, 1);
    sfd->tvw = tvw;
    sfd->id = teacher_id;

    GtkWidget *win = school_dialog_new((teacher_id > 0) ? "Edit Faculty Teacher" : "Add Faculty Teacher", 460, 520);
    sfd->dialog = win;

    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_set_margin_top(box, 20);
    gtk_widget_set_margin_bottom(box, 20);
    gtk_widget_set_margin_start(box, 24);
    gtk_widget_set_margin_end(box, 24);

    GtkWidget *lbl_name = gtk_label_new("Full Name *");
    gtk_label_set_xalign(GTK_LABEL(lbl_name), 0.0f);
    gtk_box_append(GTK_BOX(box), lbl_name);
    sfd->entry_name = gtk_entry_new();
    gtk_box_append(GTK_BOX(box), sfd->entry_name);

    GtkWidget *lbl_subj = gtk_label_new("Primary Subject *");
    gtk_label_set_xalign(GTK_LABEL(lbl_subj), 0.0f);
    gtk_box_append(GTK_BOX(box), lbl_subj);
    sfd->entry_subj = gtk_entry_new();
    gtk_box_append(GTK_BOX(box), sfd->entry_subj);

    GtkWidget *lbl_qual = gtk_label_new("Qualification");
    gtk_label_set_xalign(GTK_LABEL(lbl_qual), 0.0f);
    gtk_box_append(GTK_BOX(box), lbl_qual);
    sfd->entry_qual_or_role = gtk_entry_new();
    gtk_box_append(GTK_BOX(box), sfd->entry_qual_or_role);

    GtkWidget *lbl_ph = gtk_label_new("Phone Number");
    gtk_label_set_xalign(GTK_LABEL(lbl_ph), 0.0f);
    gtk_box_append(GTK_BOX(box), lbl_ph);
    sfd->entry_phone = gtk_entry_new();
    gtk_box_append(GTK_BOX(box), sfd->entry_phone);

    GtkWidget *lbl_em = gtk_label_new("Email");
    gtk_label_set_xalign(GTK_LABEL(lbl_em), 0.0f);
    gtk_box_append(GTK_BOX(box), lbl_em);
    sfd->entry_email = gtk_entry_new();
    gtk_box_append(GTK_BOX(box), sfd->entry_email);

    GtkWidget *lbl_addr = gtk_label_new("Address");
    gtk_label_set_xalign(GTK_LABEL(lbl_addr), 0.0f);
    gtk_box_append(GTK_BOX(box), lbl_addr);
    sfd->entry_addr = gtk_entry_new();
    gtk_box_append(GTK_BOX(box), sfd->entry_addr);

    GtkWidget *lbl_sal = gtk_label_new("Monthly Salary (₹)");
    gtk_label_set_xalign(GTK_LABEL(lbl_sal), 0.0f);
    gtk_box_append(GTK_BOX(box), lbl_sal);
    sfd->entry_salary = gtk_entry_new();
    gtk_editable_set_text(GTK_EDITABLE(sfd->entry_salary), "45000.00");
    gtk_box_append(GTK_BOX(box), sfd->entry_salary);

    GtkWidget *lbl_st = gtk_label_new("Employment Status");
    gtk_label_set_xalign(GTK_LABEL(lbl_st), 0.0f);
    gtk_box_append(GTK_BOX(box), lbl_st);
    const char *statuses[] = { "Active", "Inactive", NULL };
    sfd->combo_status = gtk_drop_down_new_from_strings(statuses);
    gtk_box_append(GTK_BOX(box), sfd->combo_status);

    if (teacher_id > 0) {
        Teacher existing;
        if (teacher_service_get_by_id(teacher_id, &existing)) {
            gtk_editable_set_text(GTK_EDITABLE(sfd->entry_name), existing.full_name);
            gtk_editable_set_text(GTK_EDITABLE(sfd->entry_subj), existing.subject);
            gtk_editable_set_text(GTK_EDITABLE(sfd->entry_qual_or_role), existing.qualification);
            gtk_editable_set_text(GTK_EDITABLE(sfd->entry_phone), existing.phone);
            gtk_editable_set_text(GTK_EDITABLE(sfd->entry_email), existing.email);
            gtk_editable_set_text(GTK_EDITABLE(sfd->entry_addr), existing.address);
            char sal_s[32]; snprintf(sal_s, sizeof(sal_s), "%.2f", existing.salary);
            gtk_editable_set_text(GTK_EDITABLE(sfd->entry_salary), sal_s);
            if (strcmp(existing.status, "INACTIVE") == 0) gtk_drop_down_set_selected(GTK_DROP_DOWN(sfd->combo_status), 1);
        }
    }

    GtkWidget *save_btn = gtk_button_new_with_label((teacher_id > 0) ? "Update Teacher" : "Save Teacher");
    gtk_widget_add_css_class(save_btn, "btn-primary");
    gtk_widget_set_margin_top(save_btn, 12);
    g_signal_connect(save_btn, "clicked", G_CALLBACK(on_save_teacher), sfd);
    gtk_box_append(GTK_BOX(box), save_btn);

    school_dialog_set_child(win, box);
    g_object_set_data_full(G_OBJECT(win), "sfd", sfd, g_free);
    school_dialog_present(win);
}

static void on_save_staff(GtkButton *btn, gpointer user_data)
{
    (void)btn;
    StaffFormDialog *sfd = (StaffFormDialog *)user_data;
    const char *name = gtk_editable_get_text(GTK_EDITABLE(sfd->entry_name));
    if (!name || strlen(name) == 0) return;

    Staff s = {0};
    s.id = sfd->id;
    strncpy(s.full_name, name, sizeof(s.full_name) - 1);
    strncpy(s.role_title, gtk_editable_get_text(GTK_EDITABLE(sfd->entry_qual_or_role)), sizeof(s.role_title) - 1);
    strncpy(s.phone, gtk_editable_get_text(GTK_EDITABLE(sfd->entry_phone)), sizeof(s.phone) - 1);
    strncpy(s.email, gtk_editable_get_text(GTK_EDITABLE(sfd->entry_email)), sizeof(s.email) - 1);
    strncpy(s.address, gtk_editable_get_text(GTK_EDITABLE(sfd->entry_addr)), sizeof(s.address) - 1);
    s.salary = atof(gtk_editable_get_text(GTK_EDITABLE(sfd->entry_salary)));

    guint st_sel = gtk_drop_down_get_selected(GTK_DROP_DOWN(sfd->combo_status));
    strcpy(s.status, (st_sel == 1) ? "INACTIVE" : "ACTIVE");

    if (sfd->id > 0) {
        if (staff_service_update(&s)) {
            show_status_message(sfd->tvw, "Staff details updated successfully.", true);
        } else {
            show_status_message(sfd->tvw, "Failed to update staff.", false);
        }
    } else {
        int new_id = 0;
        if (staff_service_create(&s, &new_id)) {
            show_status_message(sfd->tvw, "New staff member added successfully.", true);
        } else {
            show_status_message(sfd->tvw, "Failed to add staff member.", false);
        }
    }

    refresh_table(sfd->tvw);
    school_dialog_close(sfd->dialog);
}

static void show_add_edit_staff_dialog(TeacherViewWidgets *tvw, int staff_id)
{
    StaffFormDialog *sfd = g_new0(StaffFormDialog, 1);
    sfd->tvw = tvw;
    sfd->id = staff_id;

    GtkWidget *win = school_dialog_new((staff_id > 0) ? "Edit Support Staff" : "Add Support Staff", 460, 480);
    sfd->dialog = win;

    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_set_margin_top(box, 20);
    gtk_widget_set_margin_bottom(box, 20);
    gtk_widget_set_margin_start(box, 24);
    gtk_widget_set_margin_end(box, 24);

    GtkWidget *lbl_name = gtk_label_new("Full Name *");
    gtk_label_set_xalign(GTK_LABEL(lbl_name), 0.0f);
    gtk_box_append(GTK_BOX(box), lbl_name);
    sfd->entry_name = gtk_entry_new();
    gtk_box_append(GTK_BOX(box), sfd->entry_name);

    GtkWidget *lbl_role = gtk_label_new("Role Title (e.g. Librarian, Accountant, Clerk)");
    gtk_label_set_xalign(GTK_LABEL(lbl_role), 0.0f);
    gtk_box_append(GTK_BOX(box), lbl_role);
    sfd->entry_qual_or_role = gtk_entry_new();
    gtk_box_append(GTK_BOX(box), sfd->entry_qual_or_role);

    GtkWidget *lbl_ph = gtk_label_new("Phone Number");
    gtk_label_set_xalign(GTK_LABEL(lbl_ph), 0.0f);
    gtk_box_append(GTK_BOX(box), lbl_ph);
    sfd->entry_phone = gtk_entry_new();
    gtk_box_append(GTK_BOX(box), sfd->entry_phone);

    GtkWidget *lbl_em = gtk_label_new("Email");
    gtk_label_set_xalign(GTK_LABEL(lbl_em), 0.0f);
    gtk_box_append(GTK_BOX(box), lbl_em);
    sfd->entry_email = gtk_entry_new();
    gtk_box_append(GTK_BOX(box), sfd->entry_email);

    GtkWidget *lbl_addr = gtk_label_new("Address");
    gtk_label_set_xalign(GTK_LABEL(lbl_addr), 0.0f);
    gtk_box_append(GTK_BOX(box), lbl_addr);
    sfd->entry_addr = gtk_entry_new();
    gtk_box_append(GTK_BOX(box), sfd->entry_addr);

    GtkWidget *lbl_sal = gtk_label_new("Monthly Salary (₹)");
    gtk_label_set_xalign(GTK_LABEL(lbl_sal), 0.0f);
    gtk_box_append(GTK_BOX(box), lbl_sal);
    sfd->entry_salary = gtk_entry_new();
    gtk_editable_set_text(GTK_EDITABLE(sfd->entry_salary), "30000.00");
    gtk_box_append(GTK_BOX(box), sfd->entry_salary);

    GtkWidget *lbl_st = gtk_label_new("Employment Status");
    gtk_label_set_xalign(GTK_LABEL(lbl_st), 0.0f);
    gtk_box_append(GTK_BOX(box), lbl_st);
    const char *statuses[] = { "Active", "Inactive", NULL };
    sfd->combo_status = gtk_drop_down_new_from_strings(statuses);
    gtk_box_append(GTK_BOX(box), sfd->combo_status);

    if (staff_id > 0) {
        Staff existing;
        if (staff_service_get_by_id(staff_id, &existing)) {
            gtk_editable_set_text(GTK_EDITABLE(sfd->entry_name), existing.full_name);
            gtk_editable_set_text(GTK_EDITABLE(sfd->entry_qual_or_role), existing.role_title);
            gtk_editable_set_text(GTK_EDITABLE(sfd->entry_phone), existing.phone);
            gtk_editable_set_text(GTK_EDITABLE(sfd->entry_email), existing.email);
            gtk_editable_set_text(GTK_EDITABLE(sfd->entry_addr), existing.address);
            char sal_s[32]; snprintf(sal_s, sizeof(sal_s), "%.2f", existing.salary);
            gtk_editable_set_text(GTK_EDITABLE(sfd->entry_salary), sal_s);
            if (strcmp(existing.status, "INACTIVE") == 0) gtk_drop_down_set_selected(GTK_DROP_DOWN(sfd->combo_status), 1);
        }
    }

    GtkWidget *save_btn = gtk_button_new_with_label((staff_id > 0) ? "Update Staff" : "Save Staff");
    gtk_widget_add_css_class(save_btn, "btn-primary");
    gtk_widget_set_margin_top(save_btn, 12);
    g_signal_connect(save_btn, "clicked", G_CALLBACK(on_save_staff), sfd);
    gtk_box_append(GTK_BOX(box), save_btn);

    school_dialog_set_child(win, box);
    g_object_set_data_full(G_OBJECT(win), "sfd", sfd, g_free);
    school_dialog_present(win);
}

static void show_teacher_profile(int teacher_id)
{
    Teacher t;
    if (!teacher_service_get_by_id(teacher_id, &t)) return;

    GtkWidget *win = school_dialog_new("Faculty Teacher Profile", 440, 420);

    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 14);
    gtk_widget_set_margin_top(box, 20);
    gtk_widget_set_margin_bottom(box, 20);
    gtk_widget_set_margin_start(box, 28);
    gtk_widget_set_margin_end(box, 28);

    GtkWidget *avatar = gtk_image_new_from_icon_name("avatar-default-symbolic");
    gtk_image_set_pixel_size(GTK_IMAGE(avatar), 64);
    gtk_box_append(GTK_BOX(box), avatar);

    GtkWidget *name_lbl = gtk_label_new(NULL);
    char buf[256];
    snprintf(buf, sizeof(buf), "<span size='x-large' weight='bold'>%s</span>", t.full_name);
    gtk_label_set_markup(GTK_LABEL(name_lbl), buf);
    gtk_box_append(GTK_BOX(box), name_lbl);

    snprintf(buf, sizeof(buf), "Teacher Code: %s  •  Subject: %s", t.teacher_code, t.subject);
    GtkWidget *sub_lbl = gtk_label_new(buf);
    gtk_widget_add_css_class(sub_lbl, "dim-label");
    gtk_box_append(GTK_BOX(box), sub_lbl);

    GtkWidget *sep = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_append(GTK_BOX(box), sep);

    const char *labels[] = { "Qualification", "Phone", "Email", "Joining Date", "Salary", "Status" };
    char sal_str[32]; snprintf(sal_str, sizeof(sal_str), "₹%.2f", t.salary);
    const char *vals[] = { t.qualification, t.phone, t.email, t.joining_date, sal_str, t.status };

    for (int i = 0; i < 6; i++) {
        GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
        GtkWidget *k = gtk_label_new(labels[i]);
        gtk_widget_set_size_request(k, 140, -1);
        gtk_label_set_xalign(GTK_LABEL(k), 0.0f);
        gtk_widget_add_css_class(k, "dim-label");
        gtk_box_append(GTK_BOX(row), k);

        GtkWidget *v = gtk_label_new(vals[i]);
        gtk_label_set_xalign(GTK_LABEL(v), 0.0f);
        gtk_label_set_wrap(GTK_LABEL(v), TRUE);
        gtk_widget_set_hexpand(v, TRUE);
        gtk_box_append(GTK_BOX(row), v);
        gtk_box_append(GTK_BOX(box), row);
    }

    GtkWidget *btn_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_set_halign(btn_row, GTK_ALIGN_END);
    gtk_widget_set_margin_top(btn_row, 8);
    GtkWidget *close_btn = gtk_button_new_with_label("Close");
    gtk_widget_add_css_class(close_btn, "btn-secondary");
    g_signal_connect_swapped(close_btn, "clicked", G_CALLBACK(school_dialog_close), win);
    gtk_box_append(GTK_BOX(btn_row), close_btn);
    gtk_box_append(GTK_BOX(box), btn_row);

    school_dialog_set_child(win, box);
    school_dialog_present(win);
}

static void show_staff_profile(int staff_id)
{
    Staff s;
    if (!staff_service_get_by_id(staff_id, &s)) return;

    GtkWidget *win = school_dialog_new("Support Staff Profile", 440, 420);

    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 14);
    gtk_widget_set_margin_top(box, 20);
    gtk_widget_set_margin_bottom(box, 20);
    gtk_widget_set_margin_start(box, 28);
    gtk_widget_set_margin_end(box, 28);

    GtkWidget *avatar = gtk_image_new_from_icon_name("contact-new-symbolic");
    gtk_image_set_pixel_size(GTK_IMAGE(avatar), 64);
    gtk_box_append(GTK_BOX(box), avatar);

    GtkWidget *name_lbl = gtk_label_new(NULL);
    char buf[256];
    snprintf(buf, sizeof(buf), "<span size='x-large' weight='bold'>%s</span>", s.full_name);
    gtk_label_set_markup(GTK_LABEL(name_lbl), buf);
    gtk_box_append(GTK_BOX(box), name_lbl);

    snprintf(buf, sizeof(buf), "Staff Code: %s  •  Role: %s", s.staff_code, s.role_title);
    GtkWidget *sub_lbl = gtk_label_new(buf);
    gtk_widget_add_css_class(sub_lbl, "dim-label");
    gtk_box_append(GTK_BOX(box), sub_lbl);

    GtkWidget *sep = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_append(GTK_BOX(box), sep);

    const char *labels[] = { "Role Title", "Phone", "Email", "Joining Date", "Salary", "Status" };
    char sal_str[32]; snprintf(sal_str, sizeof(sal_str), "₹%.2f", s.salary);
    const char *vals[] = { s.role_title, s.phone, s.email, s.joining_date, sal_str, s.status };

    for (int i = 0; i < 6; i++) {
        GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
        GtkWidget *k = gtk_label_new(labels[i]);
        gtk_widget_set_size_request(k, 140, -1);
        gtk_label_set_xalign(GTK_LABEL(k), 0.0f);
        gtk_widget_add_css_class(k, "dim-label");
        gtk_box_append(GTK_BOX(row), k);

        GtkWidget *v = gtk_label_new(vals[i]);
        gtk_label_set_xalign(GTK_LABEL(v), 0.0f);
        gtk_label_set_wrap(GTK_LABEL(v), TRUE);
        gtk_widget_set_hexpand(v, TRUE);
        gtk_box_append(GTK_BOX(row), v);
        gtk_box_append(GTK_BOX(box), row);
    }

    GtkWidget *btn_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_set_halign(btn_row, GTK_ALIGN_END);
    gtk_widget_set_margin_top(btn_row, 8);
    GtkWidget *close_btn = gtk_button_new_with_label("Close");
    gtk_widget_add_css_class(close_btn, "btn-secondary");
    g_signal_connect_swapped(close_btn, "clicked", G_CALLBACK(school_dialog_close), win);
    gtk_box_append(GTK_BOX(btn_row), close_btn);
    gtk_box_append(GTK_BOX(box), btn_row);

    school_dialog_set_child(win, box);
    school_dialog_present(win);
}

typedef struct {
    GtkWidget *dialog;
    TeacherViewWidgets *tvw;
    bool is_teacher;
    int id;
} DelContext;

static void on_confirm_del(GtkButton *btn, gpointer user_data)
{
    (void)btn;
    DelContext *dc = (DelContext *)user_data;
    if (dc->is_teacher) {
        if (teacher_service_delete(dc->id)) {
            show_status_message(dc->tvw, "Teacher removed successfully.", true);
        } else {
            show_status_message(dc->tvw, "Failed to remove teacher.", false);
        }
    } else {
        if (staff_service_delete(dc->id)) {
            show_status_message(dc->tvw, "Staff member removed successfully.", true);
        } else {
            show_status_message(dc->tvw, "Failed to remove staff member.", false);
        }
    }

    refresh_table(dc->tvw);
    school_dialog_close(dc->dialog);
}

static void on_cancel_del(GtkButton *btn, gpointer user_data)
{
    (void)btn;
    DelContext *dc = (DelContext *)user_data;
    school_dialog_close(dc->dialog);
}

static void show_delete_dialog(TeacherViewWidgets *tvw, bool is_teacher, int id, const char *name)
{
    DelContext *dc = g_new0(DelContext, 1);
    dc->tvw = tvw;
    dc->is_teacher = is_teacher;
    dc->id = id;

    GtkWidget *win = school_dialog_new("Confirm Removal", 400, 180);
    dc->dialog = win;

    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 16);
    gtk_widget_set_margin_top(box, 20);
    gtk_widget_set_margin_bottom(box, 20);
    gtk_widget_set_margin_start(box, 24);
    gtk_widget_set_margin_end(box, 24);

    char prompt[256];
    snprintf(prompt, sizeof(prompt), "Are you sure you want to remove <b>%s</b>?\nThis action cannot be undone.",
             name ? name : "Record");
    GtkWidget *lbl = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(lbl), prompt);
    gtk_label_set_wrap(GTK_LABEL(lbl), TRUE);
    gtk_box_append(GTK_BOX(box), lbl);

    GtkWidget *btns = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_set_halign(btns, GTK_ALIGN_END);

    GtkWidget *cancel = gtk_button_new_with_label("Cancel");
    g_signal_connect(cancel, "clicked", G_CALLBACK(on_cancel_del), dc);
    gtk_box_append(GTK_BOX(btns), cancel);

    GtkWidget *del = gtk_button_new_with_label("Delete");
    gtk_widget_add_css_class(del, "btn-danger");
    g_signal_connect(del, "clicked", G_CALLBACK(on_confirm_del), dc);
    gtk_box_append(GTK_BOX(btns), del);

    gtk_box_append(GTK_BOX(box), btns);
    school_dialog_set_child(win, box);
    g_object_set_data_full(G_OBJECT(win), "dc", dc, g_free);
    school_dialog_present(win);
}

/* ── Public Constructor ───────────────────────────────────────────── */

GtkWidget *school_teacher_view_new(void)
{
    TeacherViewWidgets *tvw = g_new0(TeacherViewWidgets, 1);
    tvw->current_tab = TAB_TEACHERS;

    GtkWidget *scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_hexpand(scroll, TRUE);
    gtk_widget_set_vexpand(scroll, TRUE);

    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 16);
    gtk_widget_add_css_class(main_box, "module-page");
    gtk_widget_set_margin_top(main_box, 20);
    gtk_widget_set_margin_bottom(main_box, 20);
    gtk_widget_set_margin_start(main_box, 24);
    gtk_widget_set_margin_end(main_box, 24);
    tvw->root_box = main_box;

    /* Header Row */
    GtkWidget *header_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);

    GtkWidget *title_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    GtkWidget *title_lbl = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(title_lbl), "<span size='x-large' weight='bold'>Teacher & Staff Directory</span>");
    gtk_label_set_xalign(GTK_LABEL(title_lbl), 0.0f);
    gtk_box_append(GTK_BOX(title_box), title_lbl);

    GtkWidget *sub_lbl = gtk_label_new("Manage academic teachers, subject allocations, and school administration staff.");
    gtk_widget_add_css_class(sub_lbl, "dim-label");
    gtk_label_set_xalign(GTK_LABEL(sub_lbl), 0.0f);
    gtk_box_append(GTK_BOX(title_box), sub_lbl);
    gtk_box_append(GTK_BOX(header_row), title_box);

    GtkWidget *spacer = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_hexpand(spacer, TRUE);
    gtk_box_append(GTK_BOX(header_row), spacer);

    GtkWidget *add_btn = gtk_button_new_with_label("+ Add Member");
    gtk_widget_add_css_class(add_btn, "btn-primary");
    g_signal_connect(add_btn, "clicked", G_CALLBACK(on_add_clicked), tvw);
    gtk_box_append(GTK_BOX(header_row), add_btn);

    gtk_box_append(GTK_BOX(main_box), header_row);

    /* ── Status Banner ───────────────────────────────────────────── */
    tvw->status_banner = gtk_label_new("");
    gtk_widget_add_css_class(tvw->status_banner, "card");
    gtk_widget_set_visible(tvw->status_banner, FALSE);
    gtk_box_append(GTK_BOX(main_box), tvw->status_banner);

    /* Segmented Tab Bar */
    GtkWidget *tab_card = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_add_css_class(tab_card, "filter-card");

    tvw->tab_teachers_btn = gtk_button_new_with_label("Faculty Teachers");
    gtk_widget_add_css_class(tvw->tab_teachers_btn, "tab-btn");
    gtk_widget_add_css_class(tvw->tab_teachers_btn, "tab-btn-active");
    g_signal_connect(tvw->tab_teachers_btn, "clicked", G_CALLBACK(on_tab_teachers_clicked), tvw);
    gtk_box_append(GTK_BOX(tab_card), tvw->tab_teachers_btn);

    tvw->tab_staff_btn = gtk_button_new_with_label("Support & Admin Staff");
    gtk_widget_add_css_class(tvw->tab_staff_btn, "tab-btn");
    g_signal_connect(tvw->tab_staff_btn, "clicked", G_CALLBACK(on_tab_staff_clicked), tvw);
    gtk_box_append(GTK_BOX(tab_card), tvw->tab_staff_btn);

    /* Spacer in tab bar */
    GtkWidget *tab_spacer = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_hexpand(tab_spacer, TRUE);
    gtk_box_append(GTK_BOX(tab_card), tab_spacer);

    /* Search */
    tvw->search_entry = gtk_search_entry_new();
    gtk_widget_set_size_request(tvw->search_entry, 240, -1);
    gtk_search_entry_set_key_capture_widget(GTK_SEARCH_ENTRY(tvw->search_entry), tvw->search_entry);
    g_signal_connect(tvw->search_entry, "search-changed", G_CALLBACK(on_search_changed), tvw);
    gtk_box_append(GTK_BOX(tab_card), tvw->search_entry);

    /* Status filter */
    const char *statuses[] = { "All Statuses", "Active", "Inactive", NULL };
    tvw->status_combo = gtk_drop_down_new_from_strings(statuses);
    g_signal_connect(tvw->status_combo, "notify::selected", G_CALLBACK(on_status_changed), tvw);
    gtk_box_append(GTK_BOX(tab_card), tvw->status_combo);

    gtk_box_append(GTK_BOX(main_box), tab_card);

    /* Table Container */
    GtkWidget *table_card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_add_css_class(table_card, "table-card");

    tvw->table_container = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_append(GTK_BOX(table_card), tvw->table_container);

    gtk_box_append(GTK_BOX(main_box), table_card);

    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), main_box);
    g_object_set_data_full(G_OBJECT(scroll), "tvw", tvw, g_free);

    refresh_table(tvw);
    return scroll;
}

void school_teacher_view_reload(GtkWidget *teacher_view)
{
    TeacherViewWidgets *tvw = (TeacherViewWidgets *)g_object_get_data(G_OBJECT(teacher_view), "tvw");
    if (tvw) refresh_table(tvw);
}
