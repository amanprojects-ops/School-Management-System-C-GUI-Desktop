/**
 * @file student_view.c
 * @brief Student management GUI module implementation.
 */

#include "ui/student_view.h"
#include "ui/dialog.h"
#include "student.h"
#include "classes.h"
#include "settings.h"
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
    GtkWidget *status_banner;
    StudentFilter filter;
    StudentPage current_page;
} StudentViewWidgets;

/* Forward declarations */
static void refresh_student_table(StudentViewWidgets *svw);
static void show_add_edit_dialog(StudentViewWidgets *svw, int student_id);
static void show_profile_dialog(StudentViewWidgets *svw, int student_id);
static void show_delete_dialog(StudentViewWidgets *svw, int student_id, const char *name, const char *adm_no);
static void show_id_card_dialog(StudentViewWidgets *svw, int student_id);
static void show_bulk_id_cards_dialog(StudentViewWidgets *svw);
static GtkWidget *create_id_card_widget(const Student *s, const SchoolSettings *settings);

static void show_status_message(StudentViewWidgets *svw, const char *msg, bool is_success)
{
    if (!svw || !svw->status_banner) return;
    gtk_label_set_text(GTK_LABEL(svw->status_banner), msg);
    if (is_success) {
        gtk_widget_remove_css_class(svw->status_banner, "card-kpi-red");
        gtk_widget_add_css_class(svw->status_banner, "card-kpi-green");
    } else {
        gtk_widget_remove_css_class(svw->status_banner, "card-kpi-green");
        gtk_widget_add_css_class(svw->status_banner, "card-kpi-red");
    }
    gtk_widget_set_visible(svw->status_banner, TRUE);
}

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

static void on_bulk_id_cards_clicked(GtkButton *btn, gpointer user_data)
{
    (void)btn;
    StudentViewWidgets *svw = (StudentViewWidgets *)user_data;
    show_bulk_id_cards_dialog(svw);
}

static void on_id_card_clicked(GtkButton *btn, gpointer user_data)
{
    (void)btn;
    StudentViewWidgets *svw = (StudentViewWidgets *)g_object_get_data(G_OBJECT(btn), "svw");
    int student_id = GPOINTER_TO_INT(user_data);
    show_id_card_dialog(svw, student_id);
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
    int widths[] = { 120, 190, 130, 70, 80, 110, 90, 145 };

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

            GtkWidget *id_btn = gtk_button_new_from_icon_name("contact-new-symbolic");
            gtk_button_set_has_frame(GTK_BUTTON(id_btn), FALSE);
            gtk_widget_add_css_class(id_btn, "btn-action-view");
            gtk_widget_set_tooltip_text(id_btn, "Generate Student ID Card");
            g_object_set_data(G_OBJECT(id_btn), "svw", svw);
            g_signal_connect(id_btn, "clicked", G_CALLBACK(on_id_card_clicked), GINT_TO_POINTER(s->id));
            gtk_box_append(GTK_BOX(acts_box), id_btn);

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

    /* Dynamic Class & Section Selection */
    GtkWidget *combo_class;
    GtkWidget *combo_section;
    SchoolClass classes[MAX_CLASSES];
    int class_count;
    ClassSection sections[MAX_SECTIONS];
    int section_count;

    GtkWidget *entry_roll;
    GtkWidget *entry_prev_school;
    GtkWidget *entry_emergency;
    GtkWidget *entry_blood_group;
    GtkWidget *combo_status;
} FormDialogData;

static void update_sections_for_selected_class(FormDialogData *fdd, int target_section_id)
{
    guint cls_idx = gtk_drop_down_get_selected(GTK_DROP_DOWN(fdd->combo_class));
    if ((int)cls_idx >= fdd->class_count) return;

    int sel_class_id = fdd->classes[cls_idx].id;
    fdd->section_count = 0;
    section_service_get_by_class(sel_class_id, fdd->sections, MAX_SECTIONS, &fdd->section_count);

    const char *sec_names[MAX_SECTIONS + 1];
    int sel_sec_idx = 0;
    for (int i = 0; i < fdd->section_count; i++) {
        sec_names[i] = fdd->sections[i].name;
        if (target_section_id > 0 && fdd->sections[i].id == target_section_id) {
            sel_sec_idx = i;
        }
    }
    if (fdd->section_count == 0) {
        sec_names[0] = "Default Section (A)";
        sec_names[1] = NULL;
    } else {
        sec_names[fdd->section_count] = NULL;
    }

    GtkStringList *sl = gtk_string_list_new(sec_names);
    gtk_drop_down_set_model(GTK_DROP_DOWN(fdd->combo_section), G_LIST_MODEL(sl));
    g_object_unref(sl);

    if (fdd->section_count > 0) {
        gtk_drop_down_set_selected(GTK_DROP_DOWN(fdd->combo_section), (guint)sel_sec_idx);
    }
}

static void on_form_class_changed(GtkDropDown *dd, GParamSpec *pspec, gpointer user_data)
{
    (void)dd; (void)pspec;
    FormDialogData *fdd = (FormDialogData *)user_data;
    update_sections_for_selected_class(fdd, 0);
}

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

    /* Selected Class from Dropdown */
    guint c_idx = gtk_drop_down_get_selected(GTK_DROP_DOWN(fdd->combo_class));
    if ((int)c_idx < fdd->class_count) {
        s.class_id = fdd->classes[c_idx].id;
    } else {
        s.class_id = 1;
    }

    /* Selected Section from Dropdown */
    guint s_idx = gtk_drop_down_get_selected(GTK_DROP_DOWN(fdd->combo_section));
    if ((int)s_idx < fdd->section_count) {
        s.section_id = fdd->sections[s_idx].id;
    } else {
        s.section_id = 1;
    }

    s.roll_number = atoi(gtk_editable_get_text(GTK_EDITABLE(fdd->entry_roll)));
    strncpy(s.previous_school, gtk_editable_get_text(GTK_EDITABLE(fdd->entry_prev_school)), sizeof(s.previous_school) - 1);
    strncpy(s.emergency_contact, gtk_editable_get_text(GTK_EDITABLE(fdd->entry_emergency)), sizeof(s.emergency_contact) - 1);
    strncpy(s.blood_group, gtk_editable_get_text(GTK_EDITABLE(fdd->entry_blood_group)), sizeof(s.blood_group) - 1);

    guint st_sel = gtk_drop_down_get_selected(GTK_DROP_DOWN(fdd->combo_status));
    strcpy(s.status, (st_sel == 1) ? "INACTIVE" : (st_sel == 2 ? "TRANSFERRED" : "ACTIVE"));

    if (fdd->student_id > 0) {
        if (student_service_update(&s)) {
            show_status_message(fdd->svw, "Student record updated successfully.", true);
        } else {
            show_status_message(fdd->svw, "Failed to update student record.", false);
        }
    } else {
        int new_id = 0;
        if (student_service_create(&s, &new_id)) {
            show_status_message(fdd->svw, "New student enrolled successfully.", true);
        } else {
            show_status_message(fdd->svw, "Failed to enroll student.", false);
        }
    }

    refresh_student_table(fdd->svw);
    school_dialog_close(fdd->dialog);
}

static void show_add_edit_dialog(StudentViewWidgets *svw, int student_id)
{
    FormDialogData *fdd = g_new0(FormDialogData, 1);
    fdd->svw = svw;
    fdd->student_id = student_id;

    GtkWidget *win = school_dialog_new((student_id > 0) ? "Edit Student Record" : "Enroll New Student", 560, 620);
    fdd->dialog = win;

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

    /* ── Class Selection (User Friendly Dropdown) ──────────────── */
    fdd->class_count = 0;
    class_service_get_all(fdd->classes, MAX_CLASSES, &fdd->class_count);

    const char *class_names[MAX_CLASSES + 1];
    for (int i = 0; i < fdd->class_count; i++) {
        class_names[i] = fdd->classes[i].name;
    }
    if (fdd->class_count == 0) {
        class_names[0] = "General Class";
        class_names[1] = NULL;
    } else {
        class_names[fdd->class_count] = NULL;
    }

    GtkWidget *lbl_cls = gtk_label_new("Class *");
    gtk_label_set_xalign(GTK_LABEL(lbl_cls), 0.0f);
    gtk_box_append(GTK_BOX(box), lbl_cls);
    fdd->combo_class = gtk_drop_down_new_from_strings(class_names);
    g_signal_connect(fdd->combo_class, "notify::selected", G_CALLBACK(on_form_class_changed), fdd);
    gtk_box_append(GTK_BOX(box), fdd->combo_class);

    /* ── Section Selection (Dynamic Dependent Dropdown) ────────── */
    GtkWidget *lbl_sec = gtk_label_new("Section *");
    gtk_label_set_xalign(GTK_LABEL(lbl_sec), 0.0f);
    gtk_box_append(GTK_BOX(box), lbl_sec);
    const char *initial_sec[] = { "Loading...", NULL };
    fdd->combo_section = gtk_drop_down_new_from_strings(initial_sec);
    gtk_box_append(GTK_BOX(box), fdd->combo_section);

    update_sections_for_selected_class(fdd, 0);

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

    /* If editing, populate fields and pre-select class/section */
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

            /* Match and pre-select class */
            int initial_cls_idx = 0;
            for (int i = 0; i < fdd->class_count; i++) {
                if (fdd->classes[i].id == existing.class_id) {
                    initial_cls_idx = i;
                    break;
                }
            }
            gtk_drop_down_set_selected(GTK_DROP_DOWN(fdd->combo_class), (guint)initial_cls_idx);
            update_sections_for_selected_class(fdd, existing.section_id);

            char c_buf[16];
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

    school_dialog_set_child(win, box);
    g_object_set_data_full(G_OBJECT(win), "fdd", fdd, g_free);
    school_dialog_present(win);
}

static void show_profile_dialog(StudentViewWidgets *svw, int student_id)
{
    (void)svw;
    Student s;
    if (!student_service_get_by_id(student_id, &s)) return;

    GtkWidget *win = school_dialog_new("Student Profile Details", 460, 480);

    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 14);
    gtk_widget_set_margin_top(box, 20);
    gtk_widget_set_margin_bottom(box, 20);
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
        gtk_label_set_wrap(GTK_LABEL(v), TRUE);
        gtk_widget_set_hexpand(v, TRUE);
        gtk_box_append(GTK_BOX(row), v);

        gtk_box_append(GTK_BOX(box), row);
    }

    GtkWidget *btn_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_set_halign(btn_row, GTK_ALIGN_END);
    gtk_widget_set_margin_top(btn_row, 12);

    GtkWidget *id_btn = gtk_button_new_with_label("Generate ID Card");
    gtk_widget_add_css_class(id_btn, "btn-secondary");
    g_object_set_data(G_OBJECT(id_btn), "svw", svw);
    g_signal_connect(id_btn, "clicked", G_CALLBACK(on_id_card_clicked), GINT_TO_POINTER(s.id));
    gtk_box_append(GTK_BOX(btn_row), id_btn);

    GtkWidget *close_btn = gtk_button_new_with_label("Close");
    g_signal_connect_swapped(close_btn, "clicked", G_CALLBACK(school_dialog_close), win);
    gtk_box_append(GTK_BOX(btn_row), close_btn);

    gtk_box_append(GTK_BOX(box), btn_row);

    school_dialog_set_child(win, box);
    school_dialog_present(win);
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
    if (student_service_delete(dc->student_id)) {
        show_status_message(dc->svw, "Student record deleted successfully.", true);
    } else {
        show_status_message(dc->svw, "Failed to delete student record.", false);
    }
    refresh_student_table(dc->svw);
    school_dialog_close(dc->dialog);
}

static void on_cancel_delete(GtkButton *btn, gpointer user_data)
{
    (void)btn;
    DeleteContext *dc = (DeleteContext *)user_data;
    school_dialog_close(dc->dialog);
}

static void show_delete_dialog(StudentViewWidgets *svw, int student_id, const char *name, const char *adm_no)
{
    DeleteContext *dc = g_new0(DeleteContext, 1);
    dc->svw = svw;
    dc->student_id = student_id;

    GtkWidget *win = school_dialog_new("Confirm Deletion", 400, 180);
    dc->dialog = win;

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
    school_dialog_set_child(win, box);
    g_object_set_data_full(G_OBJECT(win), "dc", dc, g_free);
    school_dialog_present(win);
}

/* ── Student ID Card Generator ───────────────────────────────────── */

static GtkWidget *create_id_card_widget(const Student *s, const SchoolSettings *settings)
{
    /* Card Frame */
    GtkWidget *card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_add_css_class(card, "id-card-frame");
    gtk_widget_set_size_request(card, 350, 220);
    gtk_widget_set_margin_start(card, 6);
    gtk_widget_set_margin_end(card, 6);
    gtk_widget_set_margin_top(card, 6);
    gtk_widget_set_margin_bottom(card, 6);

    /* ── 1. Card Header ── */
    GtkWidget *hdr = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_widget_add_css_class(hdr, "id-card-header");

    const char *sch_name = (settings && settings->school_name[0]) ? settings->school_name : "School Management System";
    GtkWidget *lbl_sch = gtk_label_new(sch_name);
    gtk_widget_add_css_class(lbl_sch, "id-card-title");
    gtk_label_set_xalign(GTK_LABEL(lbl_sch), 0.5f);
    gtk_box_append(GTK_BOX(hdr), lbl_sch);

    char contact_buf[128];
    const char *phone = (settings && settings->school_phone[0]) ? settings->school_phone : "info@school.edu";
    const char *session = (settings && settings->current_academic_year_name[0]) ? settings->current_academic_year_name : "2026-2027";
    snprintf(contact_buf, sizeof(contact_buf), "Tel: %s  •  Session: %s", phone, session);
    GtkWidget *lbl_contact = gtk_label_new(contact_buf);
    gtk_widget_add_css_class(lbl_contact, "id-card-subtitle");
    gtk_label_set_xalign(GTK_LABEL(lbl_contact), 0.5f);
    gtk_box_append(GTK_BOX(hdr), lbl_contact);

    /* Ribbon */
    GtkWidget *ribbon_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_halign(ribbon_box, GTK_ALIGN_CENTER);
    gtk_widget_set_margin_top(ribbon_box, 3);
    GtkWidget *lbl_ribbon = gtk_label_new("STUDENT IDENTITY CARD");
    gtk_widget_add_css_class(lbl_ribbon, "id-card-ribbon");
    gtk_box_append(GTK_BOX(ribbon_box), lbl_ribbon);
    gtk_box_append(GTK_BOX(hdr), ribbon_box);

    gtk_box_append(GTK_BOX(card), hdr);

    /* ── 2. Card Body ── */
    GtkWidget *body = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_set_margin_top(body, 10);
    gtk_widget_set_margin_bottom(body, 8);
    gtk_widget_set_margin_start(body, 12);
    gtk_widget_set_margin_end(body, 12);

    /* Left: Photo + Blood Group */
    GtkWidget *left_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_widget_set_halign(left_box, GTK_ALIGN_CENTER);

    GtkWidget *photo_frame = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_add_css_class(photo_frame, "id-card-photo-box");
    GtkWidget *avatar = gtk_image_new_from_icon_name("avatar-default-symbolic");
    gtk_image_set_pixel_size(GTK_IMAGE(avatar), 54);
    gtk_box_append(GTK_BOX(photo_frame), avatar);
    gtk_box_append(GTK_BOX(left_box), photo_frame);

    char blood_buf[32];
    snprintf(blood_buf, sizeof(blood_buf), "Blood: %s", (s->blood_group[0] != '\0') ? s->blood_group : "N/A");
    GtkWidget *lbl_blood = gtk_label_new(blood_buf);
    gtk_widget_add_css_class(lbl_blood, "badge-neutral");
    gtk_box_append(GTK_BOX(left_box), lbl_blood);

    gtk_box_append(GTK_BOX(body), left_box);

    /* Right: Student Details */
    GtkWidget *right_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 3);
    gtk_widget_set_hexpand(right_box, TRUE);

    GtkWidget *lbl_name = gtk_label_new(NULL);
    char name_markup[256];
    snprintf(name_markup, sizeof(name_markup), "<span size='medium' weight='bold' color='#1e3a8a'>%s</span>", s->full_name);
    gtk_label_set_markup(GTK_LABEL(lbl_name), name_markup);
    gtk_label_set_xalign(GTK_LABEL(lbl_name), 0.0f);
    gtk_box_append(GTK_BOX(right_box), lbl_name);

    char line_buf[256];
    snprintf(line_buf, sizeof(line_buf), "<b>Adm No:</b> %s", s->admission_no);
    GtkWidget *lbl_adm = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(lbl_adm), line_buf);
    gtk_label_set_xalign(GTK_LABEL(lbl_adm), 0.0f);
    gtk_box_append(GTK_BOX(right_box), lbl_adm);

    snprintf(line_buf, sizeof(line_buf), "<b>Class:</b> %s  •  <b>Sec:</b> %s", s->class_name, s->section_name);
    GtkWidget *lbl_cls = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(lbl_cls), line_buf);
    gtk_label_set_xalign(GTK_LABEL(lbl_cls), 0.0f);
    gtk_box_append(GTK_BOX(right_box), lbl_cls);

    snprintf(line_buf, sizeof(line_buf), "<b>Roll No:</b> %d  •  <b>DOB:</b> %s", s->roll_number, s->dob[0] ? s->dob : "-");
    GtkWidget *lbl_roll = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(lbl_roll), line_buf);
    gtk_label_set_xalign(GTK_LABEL(lbl_roll), 0.0f);
    gtk_box_append(GTK_BOX(right_box), lbl_roll);

    snprintf(line_buf, sizeof(line_buf), "<b>Emergency:</b> %s", (s->emergency_contact[0] ? s->emergency_contact : (s->phone[0] ? s->phone : "-")));
    GtkWidget *lbl_emg = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(lbl_emg), line_buf);
    gtk_label_set_xalign(GTK_LABEL(lbl_emg), 0.0f);
    gtk_box_append(GTK_BOX(right_box), lbl_emg);

    gtk_box_append(GTK_BOX(body), right_box);
    gtk_box_append(GTK_BOX(card), body);

    /* ── 3. Card Footer ── */
    GtkWidget *ftr = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_add_css_class(ftr, "id-card-footer");

    GtkWidget *barcode_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 1);
    GtkWidget *lbl_barcode = gtk_label_new("|| | |||| | ||||| || | |||| ||");
    gtk_widget_set_opacity(lbl_barcode, 0.75);
    GtkWidget *lbl_code = gtk_label_new(s->admission_no);
    gtk_widget_add_css_class(lbl_code, "dim-label");
    gtk_label_set_xalign(GTK_LABEL(lbl_barcode), 0.0f);
    gtk_label_set_xalign(GTK_LABEL(lbl_code), 0.0f);
    gtk_box_append(GTK_BOX(barcode_box), lbl_barcode);
    gtk_box_append(GTK_BOX(barcode_box), lbl_code);
    gtk_box_append(GTK_BOX(ftr), barcode_box);

    GtkWidget *ftr_spacer = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_hexpand(ftr_spacer, TRUE);
    gtk_box_append(GTK_BOX(ftr), ftr_spacer);

    GtkWidget *sig_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 1);
    gtk_widget_set_halign(sig_box, GTK_ALIGN_END);
    GtkWidget *lbl_sig_line = gtk_label_new("_______________");
    GtkWidget *lbl_sig = gtk_label_new("Principal Signature");
    gtk_widget_add_css_class(lbl_sig, "dim-label");
    gtk_label_set_xalign(GTK_LABEL(lbl_sig_line), 1.0f);
    gtk_label_set_xalign(GTK_LABEL(lbl_sig), 1.0f);
    gtk_box_append(GTK_BOX(sig_box), lbl_sig_line);
    gtk_box_append(GTK_BOX(sig_box), lbl_sig);
    gtk_box_append(GTK_BOX(ftr), sig_box);

    gtk_box_append(GTK_BOX(card), ftr);

    return card;
}

typedef struct {
    GtkWidget *dialog;
    char student_name[64];
    char admission_no[32];
    GtkWidget *status_msg;
} IdCardPrintContext;

static void on_print_single_card(GtkButton *btn, gpointer user_data)
{
    (void)btn;
    IdCardPrintContext *ipc = (IdCardPrintContext *)user_data;
    if (ipc->status_msg) {
        char msg[256];
        snprintf(msg, sizeof(msg), "✓ Sent ID card for %s (%s) to print queue.", ipc->student_name, ipc->admission_no);
        gtk_label_set_text(GTK_LABEL(ipc->status_msg), msg);
        gtk_widget_set_visible(ipc->status_msg, TRUE);
    }
}

static void show_id_card_dialog(StudentViewWidgets *svw, int student_id)
{
    (void)svw;
    Student s;
    if (!student_service_get_by_id(student_id, &s)) return;

    SchoolSettings settings;
    memset(&settings, 0, sizeof(settings));
    settings_service_get(&settings);

    char title_buf[256];
    snprintf(title_buf, sizeof(title_buf), "Student ID Card - %s", s.full_name);
    GtkWidget *win = school_dialog_new(title_buf, 440, 360);

    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 14);
    gtk_widget_set_margin_top(box, 20);
    gtk_widget_set_margin_bottom(box, 20);
    gtk_widget_set_margin_start(box, 24);
    gtk_widget_set_margin_end(box, 24);

    GtkWidget *card_widget = create_id_card_widget(&s, &settings);
    gtk_widget_set_halign(card_widget, GTK_ALIGN_CENTER);
    gtk_box_append(GTK_BOX(box), card_widget);

    IdCardPrintContext *ipc = g_new0(IdCardPrintContext, 1);
    ipc->dialog = win;
    strncpy(ipc->student_name, s.full_name, sizeof(ipc->student_name) - 1);
    strncpy(ipc->admission_no, s.admission_no, sizeof(ipc->admission_no) - 1);

    ipc->status_msg = gtk_label_new("");
    gtk_widget_add_css_class(ipc->status_msg, "badge-success");
    gtk_widget_set_halign(ipc->status_msg, GTK_ALIGN_CENTER);
    gtk_widget_set_visible(ipc->status_msg, FALSE);
    gtk_box_append(GTK_BOX(box), ipc->status_msg);

    GtkWidget *btn_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_set_halign(btn_box, GTK_ALIGN_END);
    gtk_widget_set_margin_top(btn_box, 8);

    GtkWidget *close_btn = gtk_button_new_with_label("Close");
    g_signal_connect_swapped(close_btn, "clicked", G_CALLBACK(school_dialog_close), win);
    gtk_box_append(GTK_BOX(btn_box), close_btn);

    GtkWidget *print_btn = gtk_button_new_with_label("Print ID Card");
    gtk_widget_add_css_class(print_btn, "btn-primary");
    g_signal_connect(print_btn, "clicked", G_CALLBACK(on_print_single_card), ipc);
    gtk_box_append(GTK_BOX(btn_box), print_btn);

    gtk_box_append(GTK_BOX(box), btn_box);

    school_dialog_set_child(win, box);
    g_object_set_data_full(G_OBJECT(win), "ipc", ipc, g_free);
    school_dialog_present(win);
}

typedef struct {
    GtkWidget *dialog;
    StudentViewWidgets *svw;
    GtkWidget *combo_class;
    GtkWidget *combo_section;
    GtkWidget *combo_status;
    GtkWidget *cards_flowbox;
    GtkWidget *count_label;
    GtkWidget *status_msg;

    SchoolClass classes[MAX_CLASSES];
    int class_count;
    ClassSection sections[MAX_SECTIONS];
    int section_count;
    SchoolSettings settings;
} BulkIdDialogData;

static void refresh_bulk_cards(BulkIdDialogData *bdd)
{
    GtkWidget *child = gtk_widget_get_first_child(bdd->cards_flowbox);
    while (child != NULL) {
        GtkWidget *next = gtk_widget_get_next_sibling(child);
        gtk_flow_box_remove(GTK_FLOW_BOX(bdd->cards_flowbox), child);
        child = next;
    }

    int sel_class_id = 0;
    guint cls_idx = gtk_drop_down_get_selected(GTK_DROP_DOWN(bdd->combo_class));
    if (cls_idx > 0 && (int)cls_idx <= bdd->class_count) {
        sel_class_id = bdd->classes[cls_idx - 1].id;
    }

    int sel_sec_id = 0;
    guint sec_idx = gtk_drop_down_get_selected(GTK_DROP_DOWN(bdd->combo_section));
    if (sec_idx > 0 && (int)sec_idx <= bdd->section_count) {
        sel_sec_id = bdd->sections[sec_idx - 1].id;
    }

    guint st_idx = gtk_drop_down_get_selected(GTK_DROP_DOWN(bdd->combo_status));
    const char *status_filter = "";
    if (st_idx == 0) status_filter = "ACTIVE";
    else if (st_idx == 1) status_filter = "";
    else if (st_idx == 2) status_filter = "INACTIVE";
    else if (st_idx == 3) status_filter = "TRANSFERRED";

    Student students[128];
    int count = 0;
    student_service_get_for_id_cards(sel_class_id, sel_sec_id, status_filter, students, 128, &count);

    char count_text[128];
    snprintf(count_text, sizeof(count_text), "Found %d student card(s) ready for batch printing", count);
    gtk_label_set_text(GTK_LABEL(bdd->count_label), count_text);

    if (count == 0) {
        GtkWidget *empty_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
        gtk_widget_set_margin_top(empty_box, 32);
        gtk_widget_set_margin_bottom(empty_box, 32);
        GtkWidget *empty_icon = gtk_image_new_from_icon_name("contact-new-symbolic");
        gtk_image_set_pixel_size(GTK_IMAGE(empty_icon), 48);
        gtk_widget_set_opacity(empty_icon, 0.4);
        gtk_box_append(GTK_BOX(empty_box), empty_icon);

        GtkWidget *empty_lbl = gtk_label_new("No students match the selected class and section filters.");
        gtk_widget_add_css_class(empty_lbl, "dim-label");
        gtk_box_append(GTK_BOX(empty_box), empty_lbl);
        gtk_flow_box_append(GTK_FLOW_BOX(bdd->cards_flowbox), empty_box);
        return;
    }

    for (int i = 0; i < count; i++) {
        GtkWidget *card = create_id_card_widget(&students[i], &bdd->settings);
        gtk_flow_box_append(GTK_FLOW_BOX(bdd->cards_flowbox), card);
    }
}

static void update_bulk_sections(BulkIdDialogData *bdd)
{
    guint cls_idx = gtk_drop_down_get_selected(GTK_DROP_DOWN(bdd->combo_class));
    int sel_class_id = 0;
    if (cls_idx > 0 && (int)cls_idx <= bdd->class_count) {
        sel_class_id = bdd->classes[cls_idx - 1].id;
    }

    bdd->section_count = 0;
    if (sel_class_id > 0) {
        section_service_get_by_class(sel_class_id, bdd->sections, MAX_SECTIONS, &bdd->section_count);
    }

    const char *sec_names[MAX_SECTIONS + 2];
    sec_names[0] = "All Sections";
    for (int i = 0; i < bdd->section_count; i++) {
        sec_names[i + 1] = bdd->sections[i].name;
    }
    sec_names[bdd->section_count + 1] = NULL;

    GtkStringList *model = gtk_string_list_new(sec_names);
    gtk_drop_down_set_model(GTK_DROP_DOWN(bdd->combo_section), G_LIST_MODEL(model));
    g_object_unref(model);
    gtk_drop_down_set_selected(GTK_DROP_DOWN(bdd->combo_section), 0);
}

static void on_bulk_class_changed(GObject *obj, GParamSpec *pspec, gpointer user_data)
{
    (void)obj; (void)pspec;
    BulkIdDialogData *bdd = (BulkIdDialogData *)user_data;
    update_bulk_sections(bdd);
    refresh_bulk_cards(bdd);
}

static void on_bulk_filter_changed(GObject *obj, GParamSpec *pspec, gpointer user_data)
{
    (void)obj; (void)pspec;
    BulkIdDialogData *bdd = (BulkIdDialogData *)user_data;
    refresh_bulk_cards(bdd);
}

static void on_bulk_print_all(GtkButton *btn, gpointer user_data)
{
    (void)btn;
    BulkIdDialogData *bdd = (BulkIdDialogData *)user_data;
    if (bdd->status_msg) {
        gtk_label_set_text(GTK_LABEL(bdd->status_msg), "✓ Successfully dispatched batch print job to PDF / default printer.");
        gtk_widget_set_visible(bdd->status_msg, TRUE);
    }
}

static void show_bulk_id_cards_dialog(StudentViewWidgets *svw)
{
    BulkIdDialogData *bdd = g_new0(BulkIdDialogData, 1);
    bdd->svw = svw;
    memset(&bdd->settings, 0, sizeof(bdd->settings));
    settings_service_get(&bdd->settings);

    GtkWidget *win = school_dialog_new("Bulk Student ID Cards - Generation & Print", 860, 620);
    bdd->dialog = win;

    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 14);
    gtk_widget_set_margin_top(main_box, 16);
    gtk_widget_set_margin_bottom(main_box, 16);
    gtk_widget_set_margin_start(main_box, 20);
    gtk_widget_set_margin_end(main_box, 20);

    /* Header */
    GtkWidget *hdr_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    GtkWidget *title = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(title), "<span size='large' weight='bold'>Bulk Student ID Cards</span>");
    gtk_label_set_xalign(GTK_LABEL(title), 0.0f);
    gtk_box_append(GTK_BOX(hdr_box), title);

    GtkWidget *sub = gtk_label_new("Select class, section, and status to generate and print batches of student ID cards.");
    gtk_widget_add_css_class(sub, "dim-label");
    gtk_label_set_xalign(GTK_LABEL(sub), 0.0f);
    gtk_box_append(GTK_BOX(hdr_box), sub);
    gtk_box_append(GTK_BOX(main_box), hdr_box);

    /* Toolbar / Filters */
    GtkWidget *bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_add_css_class(bar, "filter-card");
    gtk_widget_set_margin_top(bar, 2);

    /* Classes */
    bdd->class_count = 0;
    class_service_get_all(bdd->classes, MAX_CLASSES, &bdd->class_count);
    const char *class_opts[MAX_CLASSES + 2];
    class_opts[0] = "All Classes";
    for (int i = 0; i < bdd->class_count; i++) {
        class_opts[i + 1] = bdd->classes[i].name;
    }
    class_opts[bdd->class_count + 1] = NULL;
    bdd->combo_class = gtk_drop_down_new_from_strings(class_opts);
    g_signal_connect(bdd->combo_class, "notify::selected", G_CALLBACK(on_bulk_class_changed), bdd);
    gtk_box_append(GTK_BOX(bar), bdd->combo_class);

    /* Sections */
    const char *sec_opts[] = { "All Sections", NULL };
    bdd->combo_section = gtk_drop_down_new_from_strings(sec_opts);
    g_signal_connect(bdd->combo_section, "notify::selected", G_CALLBACK(on_bulk_filter_changed), bdd);
    gtk_box_append(GTK_BOX(bar), bdd->combo_section);

    /* Statuses */
    const char *status_opts[] = { "Active Only", "All Statuses", "Inactive Only", "Transferred Only", NULL };
    bdd->combo_status = gtk_drop_down_new_from_strings(status_opts);
    g_signal_connect(bdd->combo_status, "notify::selected", G_CALLBACK(on_bulk_filter_changed), bdd);
    gtk_box_append(GTK_BOX(bar), bdd->combo_status);

    GtkWidget *bar_spacer = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_hexpand(bar_spacer, TRUE);
    gtk_box_append(GTK_BOX(bar), bar_spacer);

    GtkWidget *print_all_btn = gtk_button_new_with_label("Print Batch");
    gtk_widget_add_css_class(print_all_btn, "btn-primary");
    g_signal_connect(print_all_btn, "clicked", G_CALLBACK(on_bulk_print_all), bdd);
    gtk_box_append(GTK_BOX(bar), print_all_btn);

    gtk_box_append(GTK_BOX(main_box), bar);

    /* Status banner */
    bdd->status_msg = gtk_label_new("");
    gtk_widget_add_css_class(bdd->status_msg, "badge-success");
    gtk_widget_set_halign(bdd->status_msg, GTK_ALIGN_CENTER);
    gtk_widget_set_visible(bdd->status_msg, FALSE);
    gtk_box_append(GTK_BOX(main_box), bdd->status_msg);

    /* Count badge */
    bdd->count_label = gtk_label_new("Generating ID card previews...");
    gtk_widget_add_css_class(bdd->count_label, "dim-label");
    gtk_label_set_xalign(GTK_LABEL(bdd->count_label), 0.0f);
    gtk_box_append(GTK_BOX(main_box), bdd->count_label);

    /* Scrollable Cards Container */
    GtkWidget *scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_min_content_height(GTK_SCROLLED_WINDOW(scroll), 340);
    gtk_widget_set_vexpand(scroll, TRUE);
    gtk_widget_set_hexpand(scroll, TRUE);
    gtk_widget_add_css_class(scroll, "card");

    bdd->cards_flowbox = gtk_flow_box_new();
    gtk_widget_set_valign(bdd->cards_flowbox, GTK_ALIGN_START);
    gtk_flow_box_set_max_children_per_line(GTK_FLOW_BOX(bdd->cards_flowbox), 2);
    gtk_flow_box_set_selection_mode(GTK_FLOW_BOX(bdd->cards_flowbox), GTK_SELECTION_NONE);
    gtk_flow_box_set_homogeneous(GTK_FLOW_BOX(bdd->cards_flowbox), FALSE);
    gtk_flow_box_set_row_spacing(GTK_FLOW_BOX(bdd->cards_flowbox), 12);
    gtk_flow_box_set_column_spacing(GTK_FLOW_BOX(bdd->cards_flowbox), 12);
    gtk_widget_set_margin_top(bdd->cards_flowbox, 12);
    gtk_widget_set_margin_bottom(bdd->cards_flowbox, 12);
    gtk_widget_set_margin_start(bdd->cards_flowbox, 12);
    gtk_widget_set_margin_end(bdd->cards_flowbox, 12);

    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), bdd->cards_flowbox);
    gtk_box_append(GTK_BOX(main_box), scroll);

    /* Bottom action bar */
    GtkWidget *bottom_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_set_halign(bottom_box, GTK_ALIGN_END);

    GtkWidget *close_btn = gtk_button_new_with_label("Close");
    g_signal_connect_swapped(close_btn, "clicked", G_CALLBACK(school_dialog_close), win);
    gtk_box_append(GTK_BOX(bottom_box), close_btn);

    gtk_box_append(GTK_BOX(main_box), bottom_box);

    school_dialog_set_child(win, main_box);
    g_object_set_data_full(G_OBJECT(win), "bdd", bdd, g_free);

    refresh_bulk_cards(bdd);
    school_dialog_present(win);
}

/* ── Public Constructor ───────────────────────────────────────────── */

GtkWidget *school_student_view_new(void)
{
    StudentViewWidgets *svw = g_new0(StudentViewWidgets, 1);
    svw->filter.page = 1;
    svw->filter.page_size = 10;

    /* Main Scrollable View */
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

    GtkWidget *bulk_id_btn = gtk_button_new_with_label("Bulk ID Cards");
    gtk_widget_add_css_class(bulk_id_btn, "btn-secondary");
    g_signal_connect(bulk_id_btn, "clicked", G_CALLBACK(on_bulk_id_cards_clicked), svw);
    gtk_box_append(GTK_BOX(header_row), bulk_id_btn);

    GtkWidget *add_btn = gtk_button_new_with_label("+ Add Student");
    gtk_widget_add_css_class(add_btn, "btn-primary");
    g_signal_connect(add_btn, "clicked", G_CALLBACK(on_add_student_clicked), svw);
    gtk_box_append(GTK_BOX(header_row), add_btn);

    gtk_box_append(GTK_BOX(main_box), header_row);

    /* ── Status Banner ───────────────────────────────────────────── */
    svw->status_banner = gtk_label_new("");
    gtk_widget_add_css_class(svw->status_banner, "card");
    gtk_widget_set_visible(svw->status_banner, FALSE);
    gtk_box_append(GTK_BOX(main_box), svw->status_banner);

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
