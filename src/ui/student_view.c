/**
 * @file student_view.c
 * @brief Student management GUI module implementation.
 */

#include "ui/student_view.h"
#include "student.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    GtkWidget *root_box;
    GtkWidget *search_entry;
    GtkWidget *gender_combo;
    GtkWidget *status_combo;
    GtkWidget *table_container;
    GtkWidget *pagination_label;
    GtkWidget *prev_btn;
    GtkWidget *next_btn;
    StudentFilter filter;
    StudentPage current_page;
} StudentViewWidgets;

/* Forward declarations */
static void refresh_student_table(StudentViewWidgets *svw);
static void show_add_edit_dialog(StudentViewWidgets *svw, int student_id);
static void show_profile_dialog(StudentViewWidgets *svw, int student_id);
static void show_delete_dialog(StudentViewWidgets *svw, int student_id, const char *name, const char *adm_no);

/* ── Callbacks ────────────────────────────────────────────────────── */

static void on_search_changed(GtkSearchEntry *entry, gpointer user_data)
{
    StudentViewWidgets *svw = (StudentViewWidgets *)user_data;
    const char *text = gtk_editable_get_text(GTK_EDITABLE(entry));
    if (text) {
        strncpy(svw->filter.search_query, text, sizeof(svw->filter.search_query) - 1);
    } else {
        svw->filter.search_query[0] = '\0';
    }
    svw->filter.page = 1;
    refresh_student_table(svw);
}

static void on_gender_changed(GtkDropDown *dropdown, GParamSpec *pspec, gpointer user_data)
{
    (void)pspec;
    StudentViewWidgets *svw = (StudentViewWidgets *)user_data;
    guint selected = gtk_drop_down_get_selected(dropdown);

    switch (selected) {
        case 1: strcpy(svw->filter.gender, "MALE"); break;
        case 2: strcpy(svw->filter.gender, "FEMALE"); break;
        case 3: strcpy(svw->filter.gender, "OTHER"); break;
        default: svw->filter.gender[0] = '\0'; break;
    }
    svw->filter.page = 1;
    refresh_student_table(svw);
}

static void on_status_changed(GtkDropDown *dropdown, GParamSpec *pspec, gpointer user_data)
{
    (void)pspec;
    StudentViewWidgets *svw = (StudentViewWidgets *)user_data;
    guint selected = gtk_drop_down_get_selected(dropdown);

    switch (selected) {
        case 1: strcpy(svw->filter.status, "ACTIVE"); break;
        case 2: strcpy(svw->filter.status, "INACTIVE"); break;
        case 3: strcpy(svw->filter.status, "TRANSFERRED"); break;
        default: svw->filter.status[0] = '\0'; break;
    }
    svw->filter.page = 1;
    refresh_student_table(svw);
}

static void on_prev_page(GtkButton *btn, gpointer user_data)
{
    (void)btn;
    StudentViewWidgets *svw = (StudentViewWidgets *)user_data;
    if (svw->filter.page > 1) {
        svw->filter.page--;
        refresh_student_table(svw);
    }
}

static void on_next_page(GtkButton *btn, gpointer user_data)
{
    (void)btn;
    StudentViewWidgets *svw = (StudentViewWidgets *)user_data;
    if (svw->filter.page < svw->current_page.total_pages) {
        svw->filter.page++;
        refresh_student_table(svw);
    }
}

static void on_add_student_clicked(GtkButton *btn, gpointer user_data)
{
    (void)btn;
    StudentViewWidgets *svw = (StudentViewWidgets *)user_data;
    show_add_edit_dialog(svw, 0);
}

static void on_view_clicked(GtkButton *btn, gpointer user_data)
{
    (void)btn;
    StudentViewWidgets *svw = (StudentViewWidgets *)g_object_get_data(G_OBJECT(btn), "svw");
    int student_id = GPOINTER_TO_INT(user_data);
    show_profile_dialog(svw, student_id);
}

static void on_edit_clicked(GtkButton *btn, gpointer user_data)
{
    (void)btn;
    StudentViewWidgets *svw = (StudentViewWidgets *)g_object_get_data(G_OBJECT(btn), "svw");
    int student_id = GPOINTER_TO_INT(user_data);
    show_add_edit_dialog(svw, student_id);
}

static void on_delete_clicked(GtkButton *btn, gpointer user_data)
{
    (void)btn;
    StudentViewWidgets *svw = (StudentViewWidgets *)g_object_get_data(G_OBJECT(btn), "svw");
    int student_id = GPOINTER_TO_INT(user_data);
    const char *name = (const char *)g_object_get_data(G_OBJECT(btn), "student_name");
    const char *adm_no = (const char *)g_object_get_data(G_OBJECT(btn), "student_adm");
    show_delete_dialog(svw, student_id, name, adm_no);
}

/* ── Table Rendering ──────────────────────────────────────────────── */

static void refresh_student_table(StudentViewWidgets *svw)
{
    student_service_get_page(&svw->filter, &svw->current_page);

    /* Clear existing rows */
    GtkWidget *child = gtk_widget_get_first_child(svw->table_container);
    while (child) {
        GtkWidget *next = gtk_widget_get_next_sibling(child);
        gtk_box_remove(GTK_BOX(svw->table_container), child);
        child = next;
    }

    /* Table Header Row */
    GtkWidget *header_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_add_css_class(header_row, "table-header-row");

    const char *cols[] = { "Admission No", "Student Name", "Class & Section", "Roll No", "Gender", "Phone", "Status", "Actions" };
    int widths[] = { 130, 200, 140, 80, 90, 120, 100, 110 };

    for (int i = 0; i < 8; i++) {
        GtkWidget *th = gtk_label_new(cols[i]);
        gtk_widget_add_css_class(th, "table-th");
        gtk_label_set_xalign(GTK_LABEL(th), 0.0f);
        gtk_widget_set_size_request(th, widths[i], -1);
        if (i == 1) gtk_widget_set_hexpand(th, TRUE);
        gtk_box_append(GTK_BOX(header_row), th);
    }
    gtk_box_append(GTK_BOX(svw->table_container), header_row);

    /* Data Rows */
    if (svw->current_page.count == 0) {
        GtkWidget *empty_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
        gtk_widget_set_margin_top(empty_box, 30);
        gtk_widget_set_margin_bottom(empty_box, 30);
        gtk_widget_set_halign(empty_box, GTK_ALIGN_CENTER);

        GtkWidget *icon = gtk_image_new_from_icon_name("system-users-symbolic");
        gtk_image_set_pixel_size(GTK_IMAGE(icon), 48);
        gtk_widget_add_css_class(icon, "dim-label");
        gtk_box_append(GTK_BOX(empty_box), icon);

        GtkWidget *lbl = gtk_label_new("No students match your criteria.");
        gtk_widget_add_css_class(lbl, "dim-label");
        gtk_box_append(GTK_BOX(empty_box), lbl);

        gtk_box_append(GTK_BOX(svw->table_container), empty_box);
    } else {
        for (int i = 0; i < svw->current_page.count; i++) {
            const Student *s = &svw->current_page.items[i];
            GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
            gtk_widget_add_css_class(row, "table-data-row");

            /* Admission No Badge */
            GtkWidget *adm_lbl = gtk_label_new(s->admission_no);
            gtk_widget_add_css_class(adm_lbl, "badge-adm");
            gtk_widget_set_size_request(adm_lbl, widths[0], -1);
            gtk_label_set_xalign(GTK_LABEL(adm_lbl), 0.0f);
            gtk_box_append(GTK_BOX(row), adm_lbl);

            /* Full Name */
            GtkWidget *name_lbl = gtk_label_new(s->full_name);
            gtk_widget_add_css_class(name_lbl, "table-cell-bold");
            gtk_label_set_xalign(GTK_LABEL(name_lbl), 0.0f);
            gtk_widget_set_hexpand(name_lbl, TRUE);
            gtk_box_append(GTK_BOX(row), name_lbl);

            /* Class & Section */
            char cls_buf[128];
            snprintf(cls_buf, sizeof(cls_buf), "%s - %s", s->class_name, s->section_name);
            GtkWidget *cls_lbl = gtk_label_new(cls_buf);
            gtk_label_set_xalign(GTK_LABEL(cls_lbl), 0.0f);
            gtk_widget_set_size_request(cls_lbl, widths[2], -1);
            gtk_box_append(GTK_BOX(row), cls_lbl);

            /* Roll No */
            char roll_buf[16];
            snprintf(roll_buf, sizeof(roll_buf), "%d", s->roll_number);
            GtkWidget *roll_lbl = gtk_label_new(roll_buf);
            gtk_label_set_xalign(GTK_LABEL(roll_lbl), 0.0f);
            gtk_widget_set_size_request(roll_lbl, widths[3], -1);
            gtk_box_append(GTK_BOX(row), roll_lbl);

            /* Gender */
            GtkWidget *gen_lbl = gtk_label_new(s->gender);
            gtk_label_set_xalign(GTK_LABEL(gen_lbl), 0.0f);
            gtk_widget_set_size_request(gen_lbl, widths[4], -1);
            gtk_box_append(GTK_BOX(row), gen_lbl);

            /* Phone */
            GtkWidget *ph_lbl = gtk_label_new(s->phone);
            gtk_label_set_xalign(GTK_LABEL(ph_lbl), 0.0f);
            gtk_widget_set_size_request(ph_lbl, widths[5], -1);
            gtk_box_append(GTK_BOX(row), ph_lbl);

            /* Status Badge */
            GtkWidget *st_lbl = gtk_label_new(s->status);
            if (strcmp(s->status, "ACTIVE") == 0) {
                gtk_widget_add_css_class(st_lbl, "badge-success");
            } else {
                gtk_widget_add_css_class(st_lbl, "badge-warning");
            }
            gtk_widget_set_size_request(st_lbl, widths[6], -1);
            gtk_box_append(GTK_BOX(row), st_lbl);

            /* Action Buttons */
            GtkWidget *acts_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
            gtk_widget_set_size_request(acts_box, widths[7], -1);

            GtkWidget *view_btn = gtk_button_new_from_icon_name("avatar-default-symbolic");
            gtk_button_set_has_frame(GTK_BUTTON(view_btn), FALSE);
            gtk_widget_add_css_class(view_btn, "btn-action-view");
            gtk_widget_set_tooltip_text(view_btn, "View Profile");
            g_object_set_data(G_OBJECT(view_btn), "svw", svw);
            g_signal_connect(view_btn, "clicked", G_CALLBACK(on_view_clicked), GINT_TO_POINTER(s->id));
            gtk_box_append(GTK_BOX(acts_box), view_btn);

            GtkWidget *edit_btn = gtk_button_new_from_icon_name("document-edit-symbolic");
            gtk_button_set_has_frame(GTK_BUTTON(edit_btn), FALSE);
            gtk_widget_add_css_class(edit_btn, "btn-action-edit");
            gtk_widget_set_tooltip_text(edit_btn, "Edit Details");
            g_object_set_data(G_OBJECT(edit_btn), "svw", svw);
            g_signal_connect(edit_btn, "clicked", G_CALLBACK(on_edit_clicked), GINT_TO_POINTER(s->id));
            gtk_box_append(GTK_BOX(acts_box), edit_btn);

            GtkWidget *del_btn = gtk_button_new_from_icon_name("user-trash-symbolic");
            gtk_button_set_has_frame(GTK_BUTTON(del_btn), FALSE);
            gtk_widget_add_css_class(del_btn, "btn-action-delete");
            gtk_widget_set_tooltip_text(del_btn, "Delete Record");
            g_object_set_data(G_OBJECT(del_btn), "svw", svw);
            g_object_set_data_full(G_OBJECT(del_btn), "student_name", g_strdup(s->full_name), g_free);
            g_object_set_data_full(G_OBJECT(del_btn), "student_adm", g_strdup(s->admission_no), g_free);
            g_signal_connect(del_btn, "clicked", G_CALLBACK(on_delete_clicked), GINT_TO_POINTER(s->id));
            gtk_box_append(GTK_BOX(acts_box), del_btn);

            gtk_box_append(GTK_BOX(row), acts_box);
            gtk_box_append(GTK_BOX(svw->table_container), row);
        }
    }

    /* Update Pagination Status */
    char pag_text[64];
    snprintf(pag_text, sizeof(pag_text), "Page %d of %d  (%d total students)",
             svw->current_page.current_page,
             svw->current_page.total_pages,
             svw->current_page.total_records);
    gtk_label_set_text(GTK_LABEL(svw->pagination_label), pag_text);

    gtk_widget_set_sensitive(svw->prev_btn, svw->current_page.current_page > 1);
    gtk_widget_set_sensitive(svw->next_btn, svw->current_page.current_page < svw->current_page.total_pages);
}

/* ── Dialogs ──────────────────────────────────────────────────────── */

typedef struct {
    GtkWidget *dialog;
    StudentViewWidgets *svw;
    int student_id;
    GtkWidget *entry_name;
    GtkWidget *entry_father;
    GtkWidget *entry_mother;
    GtkWidget *entry_dob;
    GtkWidget *combo_gender;
    GtkWidget *entry_category;
    GtkWidget *entry_phone;
    GtkWidget *entry_email;
    GtkWidget *entry_address;
    GtkWidget *entry_class_id;
    GtkWidget *entry_section_id;
    GtkWidget *entry_roll;
    GtkWidget *entry_prev_school;
    GtkWidget *entry_emergency;
    GtkWidget *entry_blood_group;
    GtkWidget *combo_status;
} FormDialogData;

static void on_save_student(GtkButton *btn, gpointer user_data)
{
    (void)btn;
    FormDialogData *fdd = (FormDialogData *)user_data;

    const char *name = gtk_editable_get_text(GTK_EDITABLE(fdd->entry_name));
    if (!name || strlen(name) == 0) {
        return;
    }

    Student s = {0};
    s.id = fdd->student_id;
    strncpy(s.full_name, name, sizeof(s.full_name) - 1);
    strncpy(s.father_name, gtk_editable_get_text(GTK_EDITABLE(fdd->entry_father)), sizeof(s.father_name) - 1);
    strncpy(s.mother_name, gtk_editable_get_text(GTK_EDITABLE(fdd->entry_mother)), sizeof(s.mother_name) - 1);
    strncpy(s.dob, gtk_editable_get_text(GTK_EDITABLE(fdd->entry_dob)), sizeof(s.dob) - 1);

    guint g_sel = gtk_drop_down_get_selected(GTK_DROP_DOWN(fdd->combo_gender));
    strcpy(s.gender, (g_sel == 1) ? "FEMALE" : (g_sel == 2 ? "OTHER" : "MALE"));

    strncpy(s.category, gtk_editable_get_text(GTK_EDITABLE(fdd->entry_category)), sizeof(s.category) - 1);
    strncpy(s.phone, gtk_editable_get_text(GTK_EDITABLE(fdd->entry_phone)), sizeof(s.phone) - 1);
    strncpy(s.email, gtk_editable_get_text(GTK_EDITABLE(fdd->entry_email)), sizeof(s.email) - 1);
    strncpy(s.address, gtk_editable_get_text(GTK_EDITABLE(fdd->entry_address)), sizeof(s.address) - 1);

    s.class_id = atoi(gtk_editable_get_text(GTK_EDITABLE(fdd->entry_class_id)));
    if (s.class_id <= 0) s.class_id = 1;

    s.section_id = atoi(gtk_editable_get_text(GTK_EDITABLE(fdd->entry_section_id)));
    if (s.section_id <= 0) s.section_id = 1;

    s.roll_number = atoi(gtk_editable_get_text(GTK_EDITABLE(fdd->entry_roll)));
    strncpy(s.previous_school, gtk_editable_get_text(GTK_EDITABLE(fdd->entry_prev_school)), sizeof(s.previous_school) - 1);
    strncpy(s.emergency_contact, gtk_editable_get_text(GTK_EDITABLE(fdd->entry_emergency)), sizeof(s.emergency_contact) - 1);
    strncpy(s.blood_group, gtk_editable_get_text(GTK_EDITABLE(fdd->entry_blood_group)), sizeof(s.blood_group) - 1);

    guint st_sel = gtk_drop_down_get_selected(GTK_DROP_DOWN(fdd->combo_status));
    strcpy(s.status, (st_sel == 1) ? "INACTIVE" : (st_sel == 2 ? "TRANSFERRED" : "ACTIVE"));

    if (fdd->student_id > 0) {
        student_service_update(&s);
    } else {
        student_service_create(&s, NULL);
    }

    refresh_student_table(fdd->svw);
    gtk_window_destroy(GTK_WINDOW(fdd->dialog));
}

static void show_add_edit_dialog(StudentViewWidgets *svw, int student_id)
{
    FormDialogData *fdd = g_new0(FormDialogData, 1);
    fdd->svw = svw;
    fdd->student_id = student_id;

    GtkWidget *win = gtk_window_new();
    fdd->dialog = win;
    gtk_window_set_modal(GTK_WINDOW(win), TRUE);
    gtk_window_set_title(GTK_WINDOW(win), (student_id > 0) ? "Edit Student Record" : "Enroll New Student");
    gtk_window_set_default_size(GTK_WINDOW(win), 560, 620);

    GtkWidget *scroll = gtk_scrolled_window_new();
    gtk_window_set_child(GTK_WINDOW(win), scroll);

    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 14);
    gtk_widget_set_margin_top(box, 20);
    gtk_widget_set_margin_bottom(box, 20);
    gtk_widget_set_margin_start(box, 24);
    gtk_widget_set_margin_end(box, 24);

    /* Form Fields */
    GtkWidget *lbl_name = gtk_label_new("Full Name *");
    gtk_label_set_xalign(GTK_LABEL(lbl_name), 0.0f);
    gtk_box_append(GTK_BOX(box), lbl_name);
    fdd->entry_name = gtk_entry_new();
    gtk_box_append(GTK_BOX(box), fdd->entry_name);

    GtkWidget *lbl_father = gtk_label_new("Father's Name");
    gtk_label_set_xalign(GTK_LABEL(lbl_father), 0.0f);
    gtk_box_append(GTK_BOX(box), lbl_father);
    fdd->entry_father = gtk_entry_new();
    gtk_box_append(GTK_BOX(box), fdd->entry_father);

    GtkWidget *lbl_mother = gtk_label_new("Mother's Name");
    gtk_label_set_xalign(GTK_LABEL(lbl_mother), 0.0f);
    gtk_box_append(GTK_BOX(box), lbl_mother);
    fdd->entry_mother = gtk_entry_new();
    gtk_box_append(GTK_BOX(box), fdd->entry_mother);

    GtkWidget *lbl_dob = gtk_label_new("Date of Birth (YYYY-MM-DD)");
    gtk_label_set_xalign(GTK_LABEL(lbl_dob), 0.0f);
    gtk_box_append(GTK_BOX(box), lbl_dob);
    fdd->entry_dob = gtk_entry_new();
    gtk_box_append(GTK_BOX(box), fdd->entry_dob);

    GtkWidget *lbl_gender = gtk_label_new("Gender");
    gtk_label_set_xalign(GTK_LABEL(lbl_gender), 0.0f);
    gtk_box_append(GTK_BOX(box), lbl_gender);
    const char *genders[] = { "Male", "Female", "Other", NULL };
    fdd->combo_gender = gtk_drop_down_new_from_strings(genders);
    gtk_box_append(GTK_BOX(box), fdd->combo_gender);

    GtkWidget *lbl_cat = gtk_label_new("Category");
    gtk_label_set_xalign(GTK_LABEL(lbl_cat), 0.0f);
    gtk_box_append(GTK_BOX(box), lbl_cat);
    fdd->entry_category = gtk_entry_new();
    gtk_editable_set_text(GTK_EDITABLE(fdd->entry_category), "GENERAL");
    gtk_box_append(GTK_BOX(box), fdd->entry_category);

    GtkWidget *lbl_phone = gtk_label_new("Phone");
    gtk_label_set_xalign(GTK_LABEL(lbl_phone), 0.0f);
    gtk_box_append(GTK_BOX(box), lbl_phone);
    fdd->entry_phone = gtk_entry_new();
    gtk_box_append(GTK_BOX(box), fdd->entry_phone);

    GtkWidget *lbl_email = gtk_label_new("Email");
    gtk_label_set_xalign(GTK_LABEL(lbl_email), 0.0f);
    gtk_box_append(GTK_BOX(box), lbl_email);
    fdd->entry_email = gtk_entry_new();
    gtk_box_append(GTK_BOX(box), fdd->entry_email);

    GtkWidget *lbl_address = gtk_label_new("Address");
    gtk_label_set_xalign(GTK_LABEL(lbl_address), 0.0f);
    gtk_box_append(GTK_BOX(box), lbl_address);
    fdd->entry_address = gtk_entry_new();
    gtk_box_append(GTK_BOX(box), fdd->entry_address);

    GtkWidget *lbl_cls = gtk_label_new("Class ID (numeric)");
    gtk_label_set_xalign(GTK_LABEL(lbl_cls), 0.0f);
    gtk_box_append(GTK_BOX(box), lbl_cls);
    fdd->entry_class_id = gtk_entry_new();
    gtk_editable_set_text(GTK_EDITABLE(fdd->entry_class_id), "1");
    gtk_box_append(GTK_BOX(box), fdd->entry_class_id);

    GtkWidget *lbl_sec = gtk_label_new("Section ID (numeric)");
    gtk_label_set_xalign(GTK_LABEL(lbl_sec), 0.0f);
    gtk_box_append(GTK_BOX(box), lbl_sec);
    fdd->entry_section_id = gtk_entry_new();
    gtk_editable_set_text(GTK_EDITABLE(fdd->entry_section_id), "1");
    gtk_box_append(GTK_BOX(box), fdd->entry_section_id);

    GtkWidget *lbl_roll = gtk_label_new("Roll Number");
    gtk_label_set_xalign(GTK_LABEL(lbl_roll), 0.0f);
    gtk_box_append(GTK_BOX(box), lbl_roll);
    fdd->entry_roll = gtk_entry_new();
    gtk_box_append(GTK_BOX(box), fdd->entry_roll);

    GtkWidget *lbl_prev = gtk_label_new("Previous School");
    gtk_label_set_xalign(GTK_LABEL(lbl_prev), 0.0f);
    gtk_box_append(GTK_BOX(box), lbl_prev);
    fdd->entry_prev_school = gtk_entry_new();
    gtk_box_append(GTK_BOX(box), fdd->entry_prev_school);

    GtkWidget *lbl_em = gtk_label_new("Emergency Contact Phone");
    gtk_label_set_xalign(GTK_LABEL(lbl_em), 0.0f);
    gtk_box_append(GTK_BOX(box), lbl_em);
    fdd->entry_emergency = gtk_entry_new();
    gtk_box_append(GTK_BOX(box), fdd->entry_emergency);

    GtkWidget *lbl_bg = gtk_label_new("Blood Group");
    gtk_label_set_xalign(GTK_LABEL(lbl_bg), 0.0f);
    gtk_box_append(GTK_BOX(box), lbl_bg);
    fdd->entry_blood_group = gtk_entry_new();
    gtk_box_append(GTK_BOX(box), fdd->entry_blood_group);

    GtkWidget *lbl_st = gtk_label_new("Status");
    gtk_label_set_xalign(GTK_LABEL(lbl_st), 0.0f);
    gtk_box_append(GTK_BOX(box), lbl_st);
    const char *statuses[] = { "Active", "Inactive", "Transferred", NULL };
    fdd->combo_status = gtk_drop_down_new_from_strings(statuses);
    gtk_box_append(GTK_BOX(box), fdd->combo_status);

    /* If editing, populate fields */
    if (student_id > 0) {
        Student existing;
        if (student_service_get_by_id(student_id, &existing)) {
            gtk_editable_set_text(GTK_EDITABLE(fdd->entry_name), existing.full_name);
            gtk_editable_set_text(GTK_EDITABLE(fdd->entry_father), existing.father_name);
            gtk_editable_set_text(GTK_EDITABLE(fdd->entry_mother), existing.mother_name);
            gtk_editable_set_text(GTK_EDITABLE(fdd->entry_dob), existing.dob);
            gtk_editable_set_text(GTK_EDITABLE(fdd->entry_category), existing.category);
            gtk_editable_set_text(GTK_EDITABLE(fdd->entry_phone), existing.phone);
            gtk_editable_set_text(GTK_EDITABLE(fdd->entry_email), existing.email);
            gtk_editable_set_text(GTK_EDITABLE(fdd->entry_address), existing.address);
            char c_buf[16]; snprintf(c_buf, sizeof(c_buf), "%d", existing.class_id);
            gtk_editable_set_text(GTK_EDITABLE(fdd->entry_class_id), c_buf);
            snprintf(c_buf, sizeof(c_buf), "%d", existing.section_id);
            gtk_editable_set_text(GTK_EDITABLE(fdd->entry_section_id), c_buf);
            snprintf(c_buf, sizeof(c_buf), "%d", existing.roll_number);
            gtk_editable_set_text(GTK_EDITABLE(fdd->entry_roll), c_buf);
            gtk_editable_set_text(GTK_EDITABLE(fdd->entry_prev_school), existing.previous_school);
            gtk_editable_set_text(GTK_EDITABLE(fdd->entry_emergency), existing.emergency_contact);
            gtk_editable_set_text(GTK_EDITABLE(fdd->entry_blood_group), existing.blood_group);

            if (strcmp(existing.gender, "FEMALE") == 0) gtk_drop_down_set_selected(GTK_DROP_DOWN(fdd->combo_gender), 1);
            else if (strcmp(existing.gender, "OTHER") == 0) gtk_drop_down_set_selected(GTK_DROP_DOWN(fdd->combo_gender), 2);
            else gtk_drop_down_set_selected(GTK_DROP_DOWN(fdd->combo_gender), 0);

            if (strcmp(existing.status, "INACTIVE") == 0) gtk_drop_down_set_selected(GTK_DROP_DOWN(fdd->combo_status), 1);
            else if (strcmp(existing.status, "TRANSFERRED") == 0) gtk_drop_down_set_selected(GTK_DROP_DOWN(fdd->combo_status), 2);
            else gtk_drop_down_set_selected(GTK_DROP_DOWN(fdd->combo_status), 0);
        }
    }

    /* Submit Button */
    GtkWidget *save_btn = gtk_button_new_with_label((student_id > 0) ? "Update Student" : "Save Student");
    gtk_widget_add_css_class(save_btn, "btn-primary");
    gtk_widget_set_margin_top(save_btn, 12);
    g_signal_connect(save_btn, "clicked", G_CALLBACK(on_save_student), fdd);
    gtk_box_append(GTK_BOX(box), save_btn);

    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), box);
    g_object_set_data_full(G_OBJECT(win), "fdd", fdd, g_free);
    gtk_window_present(GTK_WINDOW(win));
}

static void show_profile_dialog(StudentViewWidgets *svw, int student_id)
{
    (void)svw;
    Student s;
    if (!student_service_get_by_id(student_id, &s)) return;

    GtkWidget *win = gtk_window_new();
    gtk_window_set_modal(GTK_WINDOW(win), TRUE);
    gtk_window_set_title(GTK_WINDOW(win), "Student Profile Details");
    gtk_window_set_default_size(GTK_WINDOW(win), 460, 480);

    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 14);
    gtk_widget_set_margin_top(box, 24);
    gtk_widget_set_margin_bottom(box, 24);
    gtk_widget_set_margin_start(box, 28);
    gtk_widget_set_margin_end(box, 28);

    /* Avatar & Header */
    GtkWidget *avatar = gtk_image_new_from_icon_name("avatar-default-symbolic");
    gtk_image_set_pixel_size(GTK_IMAGE(avatar), 64);
    gtk_box_append(GTK_BOX(box), avatar);

    GtkWidget *name_lbl = gtk_label_new(NULL);
    char buf[256];
    snprintf(buf, sizeof(buf), "<span size='x-large' weight='bold'>%s</span>", s.full_name);
    gtk_label_set_markup(GTK_LABEL(name_lbl), buf);
    gtk_box_append(GTK_BOX(box), name_lbl);

    snprintf(buf, sizeof(buf), "Admission No: %s  •  Class: %s (%s)", s.admission_no, s.class_name, s.section_name);
    GtkWidget *sub_lbl = gtk_label_new(buf);
    gtk_widget_add_css_class(sub_lbl, "dim-label");
    gtk_box_append(GTK_BOX(box), sub_lbl);

    GtkWidget *sep = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_append(GTK_BOX(box), sep);

    /* Details Grid */
    const char *labels[] = {
        "Roll Number", "Gender", "Date of Birth", "Blood Group",
        "Father's Name", "Mother's Name", "Phone", "Email",
        "Emergency Contact", "Address", "Status"
    };
    char roll_str[16]; snprintf(roll_str, sizeof(roll_str), "%d", s.roll_number);
    const char *vals[] = {
        roll_str, s.gender, s.dob, s.blood_group,
        s.father_name, s.mother_name, s.phone, s.email,
        s.emergency_contact, s.address, s.status
    };

    for (int i = 0; i < 11; i++) {
        GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
        GtkWidget *k = gtk_label_new(labels[i]);
        gtk_widget_set_size_request(k, 150, -1);
        gtk_label_set_xalign(GTK_LABEL(k), 0.0f);
        gtk_widget_add_css_class(k, "dim-label");
        gtk_box_append(GTK_BOX(row), k);

        GtkWidget *v = gtk_label_new(vals[i]);
        gtk_label_set_xalign(GTK_LABEL(v), 0.0f);
        gtk_widget_set_hexpand(v, TRUE);
        gtk_box_append(GTK_BOX(row), v);

        gtk_box_append(GTK_BOX(box), row);
    }

    gtk_window_set_child(GTK_WINDOW(win), box);
    gtk_window_present(GTK_WINDOW(win));
}

typedef struct {
    GtkWidget *dialog;
    StudentViewWidgets *svw;
    int student_id;
} DeleteContext;

static void on_confirm_delete(GtkButton *btn, gpointer user_data)
{
    (void)btn;
    DeleteContext *dc = (DeleteContext *)user_data;
    student_service_delete(dc->student_id);
    refresh_student_table(dc->svw);
    gtk_window_destroy(GTK_WINDOW(dc->dialog));
}

static void on_cancel_delete(GtkButton *btn, gpointer user_data)
{
    (void)btn;
    DeleteContext *dc = (DeleteContext *)user_data;
    gtk_window_destroy(GTK_WINDOW(dc->dialog));
}

static void show_delete_dialog(StudentViewWidgets *svw, int student_id, const char *name, const char *adm_no)
{
    DeleteContext *dc = g_new0(DeleteContext, 1);
    dc->svw = svw;
    dc->student_id = student_id;

    GtkWidget *win = gtk_window_new();
    dc->dialog = win;
    gtk_window_set_modal(GTK_WINDOW(win), TRUE);
    gtk_window_set_title(GTK_WINDOW(win), "Confirm Deletion");
    gtk_window_set_default_size(GTK_WINDOW(win), 400, 180);

    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 16);
    gtk_widget_set_margin_top(box, 20);
    gtk_widget_set_margin_bottom(box, 20);
    gtk_widget_set_margin_start(box, 24);
    gtk_widget_set_margin_end(box, 24);

    char prompt[256];
    snprintf(prompt, sizeof(prompt),
             "Are you sure you want to delete student <b>%s</b> (%s)?\nThis will remove their academic history permanently.",
             name ? name : "Student", adm_no ? adm_no : "");
    GtkWidget *lbl = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(lbl), prompt);
    gtk_label_set_wrap(GTK_LABEL(lbl), TRUE);
    gtk_box_append(GTK_BOX(box), lbl);

    GtkWidget *btns = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_set_halign(btns, GTK_ALIGN_END);

    GtkWidget *cancel = gtk_button_new_with_label("Cancel");
    g_signal_connect(cancel, "clicked", G_CALLBACK(on_cancel_delete), dc);
    gtk_box_append(GTK_BOX(btns), cancel);

    GtkWidget *del = gtk_button_new_with_label("Delete Record");
    gtk_widget_add_css_class(del, "btn-danger");
    g_signal_connect(del, "clicked", G_CALLBACK(on_confirm_delete), dc);
    gtk_box_append(GTK_BOX(btns), del);

    gtk_box_append(GTK_BOX(box), btns);
    gtk_window_set_child(GTK_WINDOW(win), box);
    g_object_set_data_full(G_OBJECT(win), "dc", dc, g_free);
    gtk_window_present(GTK_WINDOW(win));
}

/* ── Public Constructor ───────────────────────────────────────────── */

GtkWidget *school_student_view_new(void)
{
    StudentViewWidgets *svw = g_new0(StudentViewWidgets, 1);
    svw->filter.page = 1;
    svw->filter.page_size = 10;

    /* Main Scrollable View */
    GtkWidget *scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_hexpand(scroll, TRUE);
    gtk_widget_set_vexpand(scroll, TRUE);

    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 16);
    gtk_widget_add_css_class(main_box, "module-page");
    gtk_widget_set_margin_top(main_box, 20);
    gtk_widget_set_margin_bottom(main_box, 20);
    gtk_widget_set_margin_start(main_box, 24);
    gtk_widget_set_margin_end(main_box, 24);
    svw->root_box = main_box;

    /* ── Action Header Row ───────────────────────────────────────── */
    GtkWidget *header_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);

    GtkWidget *title_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    GtkWidget *title_lbl = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(title_lbl), "<span size='x-large' weight='bold'>Student Directory</span>");
    gtk_label_set_xalign(GTK_LABEL(title_lbl), 0.0f);
    gtk_box_append(GTK_BOX(title_box), title_lbl);

    GtkWidget *sub_lbl = gtk_label_new("Manage enrollments, student records, and profile details.");
    gtk_widget_add_css_class(sub_lbl, "dim-label");
    gtk_label_set_xalign(GTK_LABEL(sub_lbl), 0.0f);
    gtk_box_append(GTK_BOX(title_box), sub_lbl);
    gtk_box_append(GTK_BOX(header_row), title_box);

    GtkWidget *spacer = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_hexpand(spacer, TRUE);
    gtk_box_append(GTK_BOX(header_row), spacer);

    GtkWidget *add_btn = gtk_button_new_with_label("+ Add Student");
    gtk_widget_add_css_class(add_btn, "btn-primary");
    g_signal_connect(add_btn, "clicked", G_CALLBACK(on_add_student_clicked), svw);
    gtk_box_append(GTK_BOX(header_row), add_btn);

    gtk_box_append(GTK_BOX(main_box), header_row);

    /* ── Filter / Search Bar ─────────────────────────────────────── */
    GtkWidget *filter_card = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_add_css_class(filter_card, "filter-card");

    svw->search_entry = gtk_search_entry_new();
    gtk_widget_set_size_request(svw->search_entry, 280, -1);
    gtk_search_entry_set_key_capture_widget(GTK_SEARCH_ENTRY(svw->search_entry), svw->search_entry);
    g_signal_connect(svw->search_entry, "search-changed", G_CALLBACK(on_search_changed), svw);
    gtk_box_append(GTK_BOX(filter_card), svw->search_entry);

    const char *genders[] = { "All Genders", "Male", "Female", "Other", NULL };
    svw->gender_combo = gtk_drop_down_new_from_strings(genders);
    g_signal_connect(svw->gender_combo, "notify::selected", G_CALLBACK(on_gender_changed), svw);
    gtk_box_append(GTK_BOX(filter_card), svw->gender_combo);

    const char *statuses[] = { "All Statuses", "Active", "Inactive", "Transferred", NULL };
    svw->status_combo = gtk_drop_down_new_from_strings(statuses);
    g_signal_connect(svw->status_combo, "notify::selected", G_CALLBACK(on_status_changed), svw);
    gtk_box_append(GTK_BOX(filter_card), svw->status_combo);

    gtk_box_append(GTK_BOX(main_box), filter_card);

    /* ── Data Table Container ────────────────────────────────────── */
    GtkWidget *table_card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_add_css_class(table_card, "table-card");

    svw->table_container = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_append(GTK_BOX(table_card), svw->table_container);

    /* ── Pagination Bar ──────────────────────────────────────────── */
    GtkWidget *pag_bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_add_css_class(pag_bar, "pagination-bar");

    svw->pagination_label = gtk_label_new("Loading students...");
    gtk_label_set_xalign(GTK_LABEL(svw->pagination_label), 0.0f);
    gtk_widget_set_hexpand(svw->pagination_label, TRUE);
    gtk_box_append(GTK_BOX(pag_bar), svw->pagination_label);

    svw->prev_btn = gtk_button_new_from_icon_name("go-previous-symbolic");
    gtk_button_set_has_frame(GTK_BUTTON(svw->prev_btn), FALSE);
    g_signal_connect(svw->prev_btn, "clicked", G_CALLBACK(on_prev_page), svw);
    gtk_box_append(GTK_BOX(pag_bar), svw->prev_btn);

    svw->next_btn = gtk_button_new_from_icon_name("go-next-symbolic");
    gtk_button_set_has_frame(GTK_BUTTON(svw->next_btn), FALSE);
    g_signal_connect(svw->next_btn, "clicked", G_CALLBACK(on_next_page), svw);
    gtk_box_append(GTK_BOX(pag_bar), svw->next_btn);

    gtk_box_append(GTK_BOX(table_card), pag_bar);
    gtk_box_append(GTK_BOX(main_box), table_card);

    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), main_box);
    g_object_set_data_full(G_OBJECT(scroll), "svw", svw, g_free);

    refresh_student_table(svw);
    return scroll;
}

void school_student_view_reload(GtkWidget *student_view)
{
    StudentViewWidgets *svw = (StudentViewWidgets *)g_object_get_data(G_OBJECT(student_view), "svw");
    if (svw) {
        refresh_student_table(svw);
    }
}
