/**
 * @file classes_view.c
 * @brief Class, section, subject, and academic year GUI module implementation.
 */

#include "ui/classes_view.h"
#include "classes.h"
#include "teacher.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum {
    TAB_CLASSES = 0,
    TAB_SUBJECTS,
    TAB_ACADEMIC_YEARS
} ActiveClassesTab;

typedef struct {
    GtkWidget *root_box;
    GtkWidget *tab_classes_btn;
    GtkWidget *tab_subjects_btn;
    GtkWidget *tab_ay_btn;
    GtkWidget *content_container;
    GtkWidget *add_btn;
    ActiveClassesTab current_tab;
} ClassesViewWidgets;

static void refresh_view(ClassesViewWidgets *cvw);
static void show_add_edit_class_dialog(ClassesViewWidgets *cvw, int class_id);
static void show_add_edit_section_dialog(ClassesViewWidgets *cvw, int class_id, int section_id);
static void show_add_edit_subject_dialog(ClassesViewWidgets *cvw, int subject_id);
static void show_add_academic_year_dialog(ClassesViewWidgets *cvw);
static void show_delete_class_dialog(ClassesViewWidgets *cvw, int class_id, const char *name);
static void show_delete_section_dialog(ClassesViewWidgets *cvw, int section_id, const char *name);
static void show_delete_subject_dialog(ClassesViewWidgets *cvw, int subject_id, const char *name);

/* ── Tab Switch Handlers ─────────────────────────────────────────── */

static void on_tab_classes_clicked(GtkButton *btn, gpointer user_data)
{
    (void)btn;
    ClassesViewWidgets *cvw = (ClassesViewWidgets *)user_data;
    cvw->current_tab = TAB_CLASSES;
    gtk_widget_add_css_class(cvw->tab_classes_btn, "tab-btn-active");
    gtk_widget_remove_css_class(cvw->tab_subjects_btn, "tab-btn-active");
    gtk_widget_remove_css_class(cvw->tab_ay_btn, "tab-btn-active");
    gtk_button_set_label(GTK_BUTTON(cvw->add_btn), "+ Add Class");
    refresh_view(cvw);
}

static void on_tab_subjects_clicked(GtkButton *btn, gpointer user_data)
{
    (void)btn;
    ClassesViewWidgets *cvw = (ClassesViewWidgets *)user_data;
    cvw->current_tab = TAB_SUBJECTS;
    gtk_widget_remove_css_class(cvw->tab_classes_btn, "tab-btn-active");
    gtk_widget_add_css_class(cvw->tab_subjects_btn, "tab-btn-active");
    gtk_widget_remove_css_class(cvw->tab_ay_btn, "tab-btn-active");
    gtk_button_set_label(GTK_BUTTON(cvw->add_btn), "+ Add Subject");
    refresh_view(cvw);
}

static void on_tab_ay_clicked(GtkButton *btn, gpointer user_data)
{
    (void)btn;
    ClassesViewWidgets *cvw = (ClassesViewWidgets *)user_data;
    cvw->current_tab = TAB_ACADEMIC_YEARS;
    gtk_widget_remove_css_class(cvw->tab_classes_btn, "tab-btn-active");
    gtk_widget_remove_css_class(cvw->tab_subjects_btn, "tab-btn-active");
    gtk_widget_add_css_class(cvw->tab_ay_btn, "tab-btn-active");
    gtk_button_set_label(GTK_BUTTON(cvw->add_btn), "+ Add Academic Year");
    refresh_view(cvw);
}

static void on_add_primary_clicked(GtkButton *btn, gpointer user_data)
{
    (void)btn;
    ClassesViewWidgets *cvw = (ClassesViewWidgets *)user_data;
    if (cvw->current_tab == TAB_CLASSES) {
        show_add_edit_class_dialog(cvw, 0);
    } else if (cvw->current_tab == TAB_SUBJECTS) {
        show_add_edit_subject_dialog(cvw, 0);
    } else {
        show_add_academic_year_dialog(cvw);
    }
}

/* ── Dialog Callbacks & Actions ───────────────────────────────────── */

static void on_edit_class_clicked(GtkButton *btn, gpointer user_data)
{
    ClassesViewWidgets *cvw = (ClassesViewWidgets *)g_object_get_data(G_OBJECT(btn), "cvw");
    int class_id = GPOINTER_TO_INT(user_data);
    show_add_edit_class_dialog(cvw, class_id);
}

static void on_delete_class_clicked(GtkButton *btn, gpointer user_data)
{
    ClassesViewWidgets *cvw = (ClassesViewWidgets *)g_object_get_data(G_OBJECT(btn), "cvw");
    int class_id = GPOINTER_TO_INT(user_data);
    const char *name = (const char *)g_object_get_data(G_OBJECT(btn), "class_name");
    show_delete_class_dialog(cvw, class_id, name ? name : "Class");
}

static void on_add_section_for_class_clicked(GtkButton *btn, gpointer user_data)
{
    ClassesViewWidgets *cvw = (ClassesViewWidgets *)g_object_get_data(G_OBJECT(btn), "cvw");
    int class_id = GPOINTER_TO_INT(user_data);
    show_add_edit_section_dialog(cvw, class_id, 0);
}

static void on_edit_section_clicked(GtkButton *btn, gpointer user_data)
{
    ClassesViewWidgets *cvw = (ClassesViewWidgets *)g_object_get_data(G_OBJECT(btn), "cvw");
    int section_id = GPOINTER_TO_INT(user_data);
    show_add_edit_section_dialog(cvw, 0, section_id);
}

static void on_delete_section_clicked(GtkButton *btn, gpointer user_data)
{
    ClassesViewWidgets *cvw = (ClassesViewWidgets *)g_object_get_data(G_OBJECT(btn), "cvw");
    int section_id = GPOINTER_TO_INT(user_data);
    const char *name = (const char *)g_object_get_data(G_OBJECT(btn), "sec_name");
    show_delete_section_dialog(cvw, section_id, name ? name : "Section");
}

static void on_edit_subject_clicked(GtkButton *btn, gpointer user_data)
{
    ClassesViewWidgets *cvw = (ClassesViewWidgets *)g_object_get_data(G_OBJECT(btn), "cvw");
    int subject_id = GPOINTER_TO_INT(user_data);
    show_add_edit_subject_dialog(cvw, subject_id);
}

static void on_delete_subject_clicked(GtkButton *btn, gpointer user_data)
{
    ClassesViewWidgets *cvw = (ClassesViewWidgets *)g_object_get_data(G_OBJECT(btn), "cvw");
    int subject_id = GPOINTER_TO_INT(user_data);
    const char *name = (const char *)g_object_get_data(G_OBJECT(btn), "sub_name");
    show_delete_subject_dialog(cvw, subject_id, name ? name : "Subject");
}

static void on_set_current_ay_clicked(GtkButton *btn, gpointer user_data)
{
    ClassesViewWidgets *cvw = (ClassesViewWidgets *)g_object_get_data(G_OBJECT(btn), "cvw");
    int ay_id = GPOINTER_TO_INT(user_data);
    academic_year_set_current(ay_id);
    refresh_view(cvw);
}

/* ── View Renderers ───────────────────────────────────────────────── */

static void render_classes_tab(ClassesViewWidgets *cvw)
{
    SchoolClass classes[MAX_CLASSES];
    int class_count = 0;
    class_service_get_all(classes, MAX_CLASSES, &class_count);

    if (class_count == 0) {
        GtkWidget *empty_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
        gtk_widget_set_margin_top(empty_box, 40);
        gtk_widget_set_margin_bottom(empty_box, 40);
        gtk_widget_set_halign(empty_box, GTK_ALIGN_CENTER);

        GtkWidget *icon = gtk_image_new_from_icon_name("folder-symbolic");
        gtk_image_set_pixel_size(GTK_IMAGE(icon), 48);
        gtk_widget_add_css_class(icon, "dim-label");
        gtk_box_append(GTK_BOX(empty_box), icon);

        GtkWidget *lbl = gtk_label_new("No classes found. Click '+ Add Class' to configure your first class.");
        gtk_widget_add_css_class(lbl, "dim-label");
        gtk_box_append(GTK_BOX(empty_box), lbl);

        gtk_box_append(GTK_BOX(cvw->content_container), empty_box);
        return;
    }

    for (int i = 0; i < class_count; i++) {
        const SchoolClass *c = &classes[i];

        /* Class Card */
        GtkWidget *card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
        gtk_widget_add_css_class(card, "card");
        gtk_widget_set_margin_bottom(card, 16);

        /* Card Header Row */
        GtkWidget *card_header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
        gtk_widget_set_valign(card_header, GTK_ALIGN_CENTER);

        /* Class Title */
        char title_buf[128];
        snprintf(title_buf, sizeof(title_buf), "%s", c->name);
        GtkWidget *title_lbl = gtk_label_new(title_buf);
        gtk_widget_add_css_class(title_lbl, "dashboard-card-title");
        gtk_label_set_xalign(GTK_LABEL(title_lbl), 0.0f);
        gtk_widget_set_hexpand(title_lbl, TRUE);
        gtk_box_append(GTK_BOX(card_header), title_lbl);

        /* Academic Year Badge */
        if (strlen(c->academic_year_name) > 0) {
            GtkWidget *ay_badge = gtk_label_new(c->academic_year_name);
            gtk_widget_add_css_class(ay_badge, "badge-role");
            gtk_box_append(GTK_BOX(card_header), ay_badge);
        }

        /* Capacity & Student Count Badge */
        char stat_buf[64];
        snprintf(stat_buf, sizeof(stat_buf), "%d / %d Students", c->student_count, c->capacity);
        GtkWidget *stat_badge = gtk_label_new(stat_buf);
        gtk_widget_add_css_class(stat_badge, "badge-adm");
        gtk_box_append(GTK_BOX(card_header), stat_badge);

        /* Actions: + Add Section, Edit, Delete */
        GtkWidget *add_sec_btn = gtk_button_new_with_label("+ Section");
        gtk_widget_add_css_class(add_sec_btn, "btn-secondary");
        g_object_set_data(G_OBJECT(add_sec_btn), "cvw", cvw);
        g_signal_connect(add_sec_btn, "clicked", G_CALLBACK(on_add_section_for_class_clicked), GINT_TO_POINTER(c->id));
        gtk_box_append(GTK_BOX(card_header), add_sec_btn);

        GtkWidget *edit_btn = gtk_button_new_with_label("Edit");
        gtk_widget_add_css_class(edit_btn, "btn-secondary");
        g_object_set_data(G_OBJECT(edit_btn), "cvw", cvw);
        g_signal_connect(edit_btn, "clicked", G_CALLBACK(on_edit_class_clicked), GINT_TO_POINTER(c->id));
        gtk_box_append(GTK_BOX(card_header), edit_btn);

        GtkWidget *del_btn = gtk_button_new_with_label("Delete");
        gtk_widget_add_css_class(del_btn, "btn-danger");
        g_object_set_data(G_OBJECT(del_btn), "cvw", cvw);
        g_object_set_data_full(G_OBJECT(del_btn), "class_name", g_strdup(c->name), g_free);
        g_signal_connect(del_btn, "clicked", G_CALLBACK(on_delete_class_clicked), GINT_TO_POINTER(c->id));
        gtk_box_append(GTK_BOX(card_header), del_btn);

        gtk_box_append(GTK_BOX(card), card_header);

        /* Divider */
        GtkWidget *sep = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
        gtk_box_append(GTK_BOX(card), sep);

        /* Sections List under this class */
        ClassSection sections[MAX_SECTIONS];
        int sec_count = 0;
        section_service_get_by_class(c->id, sections, MAX_SECTIONS, &sec_count);

        if (sec_count == 0) {
            GtkWidget *no_sec_lbl = gtk_label_new("No sections created yet for this class.");
            gtk_widget_add_css_class(no_sec_lbl, "dim-label");
            gtk_label_set_xalign(GTK_LABEL(no_sec_lbl), 0.0f);
            gtk_box_append(GTK_BOX(card), no_sec_lbl);
        } else {
            GtkWidget *sec_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
            for (int s = 0; s < sec_count; s++) {
                const ClassSection *sec = &sections[s];
                GtkWidget *sec_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
                gtk_widget_add_css_class(sec_row, "table-data-row");

                /* Section Name */
                char sname_buf[64];
                snprintf(sname_buf, sizeof(sname_buf), "Section %s", sec->name);
                GtkWidget *sname_lbl = gtk_label_new(sname_buf);
                gtk_widget_add_css_class(sname_lbl, "table-cell-bold");
                gtk_label_set_xalign(GTK_LABEL(sname_lbl), 0.0f);
                gtk_widget_set_size_request(sname_lbl, 120, -1);
                gtk_box_append(GTK_BOX(sec_row), sname_lbl);

                /* Class Teacher */
                char ct_buf[160];
                snprintf(ct_buf, sizeof(ct_buf), "Class Teacher: %s", sec->class_teacher_name);
                GtkWidget *ct_lbl = gtk_label_new(ct_buf);
                gtk_label_set_xalign(GTK_LABEL(ct_lbl), 0.0f);
                gtk_widget_set_hexpand(ct_lbl, TRUE);
                gtk_box_append(GTK_BOX(sec_row), ct_lbl);

                /* Enrolled students */
                char sc_buf[64];
                snprintf(sc_buf, sizeof(sc_buf), "%d Students", sec->student_count);
                GtkWidget *sc_lbl = gtk_label_new(sc_buf);
                gtk_widget_add_css_class(sc_lbl, "badge-adm");
                gtk_box_append(GTK_BOX(sec_row), sc_lbl);

                /* Edit & Delete Section Buttons */
                GtkWidget *sec_edit = gtk_button_new_with_label("Edit");
                gtk_widget_add_css_class(sec_edit, "btn-secondary");
                g_object_set_data(G_OBJECT(sec_edit), "cvw", cvw);
                g_signal_connect(sec_edit, "clicked", G_CALLBACK(on_edit_section_clicked), GINT_TO_POINTER(sec->id));
                gtk_box_append(GTK_BOX(sec_row), sec_edit);

                GtkWidget *sec_del = gtk_button_new_with_label("Delete");
                gtk_widget_add_css_class(sec_del, "btn-danger");
                g_object_set_data(G_OBJECT(sec_del), "cvw", cvw);
                g_object_set_data_full(G_OBJECT(sec_del), "sec_name", g_strdup(sec->name), g_free);
                g_signal_connect(sec_del, "clicked", G_CALLBACK(on_delete_section_clicked), GINT_TO_POINTER(sec->id));
                gtk_box_append(GTK_BOX(sec_row), sec_del);

                gtk_box_append(GTK_BOX(sec_box), sec_row);
            }
            gtk_box_append(GTK_BOX(card), sec_box);
        }

        gtk_box_append(GTK_BOX(cvw->content_container), card);
    }
}

static void render_subjects_tab(ClassesViewWidgets *cvw)
{
    Subject subjects[MAX_SUBJECTS];
    int sub_count = 0;
    subject_service_get_all(subjects, MAX_SUBJECTS, &sub_count);

    /* Table Container */
    GtkWidget *table_card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_add_css_class(table_card, "card");
    gtk_box_append(GTK_BOX(cvw->content_container), table_card);

    /* Table Header */
    GtkWidget *header_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_add_css_class(header_row, "table-header-row");

    const char *cols[] = { "Subject Code", "Subject Name", "Class", "Assigned Teacher", "Actions" };
    int widths[] = { 140, 220, 150, 220, 140 };

    for (int i = 0; i < 5; i++) {
        GtkWidget *th = gtk_label_new(cols[i]);
        gtk_widget_add_css_class(th, "table-th");
        gtk_label_set_xalign(GTK_LABEL(th), 0.0f);
        gtk_widget_set_size_request(th, widths[i], -1);
        if (i == 1) gtk_widget_set_hexpand(th, TRUE);
        gtk_box_append(GTK_BOX(header_row), th);
    }
    gtk_box_append(GTK_BOX(table_card), header_row);

    if (sub_count == 0) {
        GtkWidget *empty_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
        gtk_widget_set_margin_top(empty_box, 30);
        gtk_widget_set_margin_bottom(empty_box, 30);
        gtk_widget_set_halign(empty_box, GTK_ALIGN_CENTER);

        GtkWidget *lbl = gtk_label_new("No subjects defined. Click '+ Add Subject' to configure subjects.");
        gtk_widget_add_css_class(lbl, "dim-label");
        gtk_box_append(GTK_BOX(empty_box), lbl);

        gtk_box_append(GTK_BOX(table_card), empty_box);
        return;
    }

    for (int i = 0; i < sub_count; i++) {
        const Subject *sub = &subjects[i];
        GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
        gtk_widget_add_css_class(row, "table-data-row");

        /* Code */
        GtkWidget *code_lbl = gtk_label_new(sub->code);
        gtk_widget_add_css_class(code_lbl, "badge-adm");
        gtk_widget_set_size_request(code_lbl, widths[0], -1);
        gtk_label_set_xalign(GTK_LABEL(code_lbl), 0.0f);
        gtk_box_append(GTK_BOX(row), code_lbl);

        /* Name */
        GtkWidget *name_lbl = gtk_label_new(sub->name);
        gtk_widget_add_css_class(name_lbl, "table-cell-bold");
        gtk_label_set_xalign(GTK_LABEL(name_lbl), 0.0f);
        gtk_widget_set_hexpand(name_lbl, TRUE);
        gtk_box_append(GTK_BOX(row), name_lbl);

        /* Class */
        GtkWidget *cls_lbl = gtk_label_new(sub->class_name);
        gtk_widget_set_size_request(cls_lbl, widths[2], -1);
        gtk_label_set_xalign(GTK_LABEL(cls_lbl), 0.0f);
        gtk_box_append(GTK_BOX(row), cls_lbl);

        /* Teacher */
        GtkWidget *tch_lbl = gtk_label_new(sub->teacher_name);
        gtk_widget_set_size_request(tch_lbl, widths[3], -1);
        gtk_label_set_xalign(GTK_LABEL(tch_lbl), 0.0f);
        gtk_box_append(GTK_BOX(row), tch_lbl);

        /* Actions */
        GtkWidget *act_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
        gtk_widget_set_size_request(act_box, widths[4], -1);

        GtkWidget *edit_btn = gtk_button_new_with_label("Edit");
        gtk_widget_add_css_class(edit_btn, "btn-secondary");
        g_object_set_data(G_OBJECT(edit_btn), "cvw", cvw);
        g_signal_connect(edit_btn, "clicked", G_CALLBACK(on_edit_subject_clicked), GINT_TO_POINTER(sub->id));
        gtk_box_append(GTK_BOX(act_box), edit_btn);

        GtkWidget *del_btn = gtk_button_new_with_label("Delete");
        gtk_widget_add_css_class(del_btn, "btn-danger");
        g_object_set_data(G_OBJECT(del_btn), "cvw", cvw);
        g_object_set_data_full(G_OBJECT(del_btn), "sub_name", g_strdup(sub->name), g_free);
        g_signal_connect(del_btn, "clicked", G_CALLBACK(on_delete_subject_clicked), GINT_TO_POINTER(sub->id));
        gtk_box_append(GTK_BOX(act_box), del_btn);

        gtk_box_append(GTK_BOX(row), act_box);
        gtk_box_append(GTK_BOX(table_card), row);
    }
}

static void render_ay_tab(ClassesViewWidgets *cvw)
{
    AcademicYear years[MAX_ACADEMIC_YEARS];
    int ay_count = 0;
    academic_year_get_all(years, MAX_ACADEMIC_YEARS, &ay_count);

    GtkWidget *table_card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_add_css_class(table_card, "card");
    gtk_box_append(GTK_BOX(cvw->content_container), table_card);

    GtkWidget *header_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_add_css_class(header_row, "table-header-row");

    const char *cols[] = { "Academic Year", "Start Date", "End Date", "Status", "Action" };
    int widths[] = { 180, 150, 150, 140, 160 };

    for (int i = 0; i < 5; i++) {
        GtkWidget *th = gtk_label_new(cols[i]);
        gtk_widget_add_css_class(th, "table-th");
        gtk_label_set_xalign(GTK_LABEL(th), 0.0f);
        gtk_widget_set_size_request(th, widths[i], -1);
        if (i == 0) gtk_widget_set_hexpand(th, TRUE);
        gtk_box_append(GTK_BOX(header_row), th);
    }
    gtk_box_append(GTK_BOX(table_card), header_row);

    if (ay_count == 0) {
        GtkWidget *empty_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
        gtk_widget_set_margin_top(empty_box, 30);
        gtk_widget_set_margin_bottom(empty_box, 30);
        gtk_widget_set_halign(empty_box, GTK_ALIGN_CENTER);

        GtkWidget *lbl = gtk_label_new("No academic years configured.");
        gtk_widget_add_css_class(lbl, "dim-label");
        gtk_box_append(GTK_BOX(empty_box), lbl);

        gtk_box_append(GTK_BOX(table_card), empty_box);
        return;
    }

    for (int i = 0; i < ay_count; i++) {
        const AcademicYear *ay = &years[i];
        GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
        gtk_widget_add_css_class(row, "table-data-row");

        /* Name */
        GtkWidget *name_lbl = gtk_label_new(ay->name);
        gtk_widget_add_css_class(name_lbl, "table-cell-bold");
        gtk_label_set_xalign(GTK_LABEL(name_lbl), 0.0f);
        gtk_widget_set_hexpand(name_lbl, TRUE);
        gtk_box_append(GTK_BOX(row), name_lbl);

        /* Start Date */
        GtkWidget *s_lbl = gtk_label_new(ay->start_date);
        gtk_widget_set_size_request(s_lbl, widths[1], -1);
        gtk_label_set_xalign(GTK_LABEL(s_lbl), 0.0f);
        gtk_box_append(GTK_BOX(row), s_lbl);

        /* End Date */
        GtkWidget *e_lbl = gtk_label_new(ay->end_date);
        gtk_widget_set_size_request(e_lbl, widths[2], -1);
        gtk_label_set_xalign(GTK_LABEL(e_lbl), 0.0f);
        gtk_box_append(GTK_BOX(row), e_lbl);

        /* Status Badge */
        GtkWidget *status_badge = gtk_label_new(ay->is_current ? "CURRENT" : "INACTIVE");
        if (ay->is_current) {
            gtk_widget_add_css_class(status_badge, "badge-status-active");
        } else {
            gtk_widget_add_css_class(status_badge, "badge-status-inactive");
        }
        gtk_widget_set_size_request(status_badge, widths[3], -1);
        gtk_box_append(GTK_BOX(row), status_badge);

        /* Action: Set as Current */
        GtkWidget *act_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
        gtk_widget_set_size_request(act_box, widths[4], -1);

        if (!ay->is_current) {
            GtkWidget *set_btn = gtk_button_new_with_label("Set as Current");
            gtk_widget_add_css_class(set_btn, "btn-secondary");
            g_object_set_data(G_OBJECT(set_btn), "cvw", cvw);
            g_signal_connect(set_btn, "clicked", G_CALLBACK(on_set_current_ay_clicked), GINT_TO_POINTER(ay->id));
            gtk_box_append(GTK_BOX(act_box), set_btn);
        } else {
            GtkWidget *active_lbl = gtk_label_new("Active Year");
            gtk_widget_add_css_class(active_lbl, "dim-label");
            gtk_box_append(GTK_BOX(act_box), active_lbl);
        }

        gtk_box_append(GTK_BOX(row), act_box);
        gtk_box_append(GTK_BOX(table_card), row);
    }
}

static void refresh_view(ClassesViewWidgets *cvw)
{
    /* Clear container */
    GtkWidget *child = gtk_widget_get_first_child(cvw->content_container);
    while (child) {
        GtkWidget *next = gtk_widget_get_next_sibling(child);
        gtk_box_remove(GTK_BOX(cvw->content_container), child);
        child = next;
    }

    if (cvw->current_tab == TAB_CLASSES) {
        render_classes_tab(cvw);
    } else if (cvw->current_tab == TAB_SUBJECTS) {
        render_subjects_tab(cvw);
    } else {
        render_ay_tab(cvw);
    }
}

/* ── Modal Dialogs ────────────────────────────────────────────────── */

/* 1. Add / Edit Class Dialog */
typedef struct {
    ClassesViewWidgets *cvw;
    int class_id;
    GtkWidget *dialog;
    GtkWidget *entry_name;
    GtkWidget *entry_numeric;
    GtkWidget *entry_capacity;
    GtkWidget *combo_ay;
    int ay_ids[MAX_ACADEMIC_YEARS];
    int ay_count;
} ClassDialogData;

static void on_save_class_dialog(GtkButton *btn, gpointer user_data)
{
    (void)btn;
    ClassDialogData *d = (ClassDialogData *)user_data;

    const char *name = gtk_editable_get_text(GTK_EDITABLE(d->entry_name));
    const char *num_str = gtk_editable_get_text(GTK_EDITABLE(d->entry_numeric));
    const char *cap_str = gtk_editable_get_text(GTK_EDITABLE(d->entry_capacity));

    if (!name || strlen(name) == 0) return;

    SchoolClass cls;
    memset(&cls, 0, sizeof(SchoolClass));
    cls.id = d->class_id;
    strncpy(cls.name, name, sizeof(cls.name) - 1);
    cls.numeric_value = num_str ? atoi(num_str) : 1;
    cls.capacity = cap_str ? atoi(cap_str) : 40;

    guint sel_ay = gtk_drop_down_get_selected(GTK_DROP_DOWN(d->combo_ay));
    if ((int)sel_ay < d->ay_count) {
        cls.academic_year_id = d->ay_ids[sel_ay];
    }

    if (d->class_id > 0) {
        class_service_update(&cls);
    } else {
        int new_id = 0;
        class_service_create(&cls, &new_id);
    }

    refresh_view(d->cvw);
    gtk_window_destroy(GTK_WINDOW(d->dialog));
    g_free(d);
}

static void on_cancel_dialog(GtkButton *btn, gpointer user_data)
{
    (void)btn;
    GtkWidget *dialog = GTK_WIDGET(user_data);
    gtk_window_destroy(GTK_WINDOW(dialog));
}

static void show_add_edit_class_dialog(ClassesViewWidgets *cvw, int class_id)
{
    ClassDialogData *d = g_new0(ClassDialogData, 1);
    d->cvw = cvw;
    d->class_id = class_id;

    SchoolClass existing;
    bool is_edit = (class_id > 0 && class_service_get_by_id(class_id, &existing));

    GtkWidget *win = gtk_window_new();
    d->dialog = win;
    gtk_window_set_title(GTK_WINDOW(win), is_edit ? "Edit Class" : "Add New Class");
    gtk_window_set_modal(GTK_WINDOW(win), TRUE);
    gtk_window_set_default_size(GTK_WINDOW(win), 460, 400);

    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 16);
    gtk_widget_set_margin_start(main_box, 24);
    gtk_widget_set_margin_end(main_box, 24);
    gtk_widget_set_margin_top(main_box, 24);
    gtk_widget_set_margin_bottom(main_box, 24);
    gtk_window_set_child(GTK_WINDOW(win), main_box);

    GtkWidget *title_lbl = gtk_label_new(is_edit ? "Edit Class Details" : "Create New Class");
    gtk_widget_add_css_class(title_lbl, "dashboard-card-title");
    gtk_label_set_xalign(GTK_LABEL(title_lbl), 0.0f);
    gtk_box_append(GTK_BOX(main_box), title_lbl);

    /* Form Fields */
    GtkWidget *form_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_box_append(GTK_BOX(main_box), form_box);

    /* Name */
    GtkWidget *lbl_name = gtk_label_new("Class Name (e.g. Class 10):");
    gtk_label_set_xalign(GTK_LABEL(lbl_name), 0.0f);
    gtk_box_append(GTK_BOX(form_box), lbl_name);
    d->entry_name = gtk_entry_new();
    if (is_edit) gtk_editable_set_text(GTK_EDITABLE(d->entry_name), existing.name);
    gtk_box_append(GTK_BOX(form_box), d->entry_name);

    /* Numeric Value */
    GtkWidget *lbl_num = gtk_label_new("Numeric Standard (e.g. 10):");
    gtk_label_set_xalign(GTK_LABEL(lbl_num), 0.0f);
    gtk_box_append(GTK_BOX(form_box), lbl_num);
    d->entry_numeric = gtk_entry_new();
    char num_buf[16] = "1";
    if (is_edit) snprintf(num_buf, sizeof(num_buf), "%d", existing.numeric_value);
    gtk_editable_set_text(GTK_EDITABLE(d->entry_numeric), num_buf);
    gtk_box_append(GTK_BOX(form_box), d->entry_numeric);

    /* Student Capacity */
    GtkWidget *lbl_cap = gtk_label_new("Student Capacity:");
    gtk_label_set_xalign(GTK_LABEL(lbl_cap), 0.0f);
    gtk_box_append(GTK_BOX(form_box), lbl_cap);
    d->entry_capacity = gtk_entry_new();
    char cap_buf[16] = "40";
    if (is_edit) snprintf(cap_buf, sizeof(cap_buf), "%d", existing.capacity);
    gtk_editable_set_text(GTK_EDITABLE(d->entry_capacity), cap_buf);
    gtk_box_append(GTK_BOX(form_box), d->entry_capacity);

    /* Academic Year Dropdown */
    GtkWidget *lbl_ay = gtk_label_new("Academic Year:");
    gtk_label_set_xalign(GTK_LABEL(lbl_ay), 0.0f);
    gtk_box_append(GTK_BOX(form_box), lbl_ay);

    AcademicYear years[MAX_ACADEMIC_YEARS];
    int ay_count = 0;
    academic_year_get_all(years, MAX_ACADEMIC_YEARS, &ay_count);
    d->ay_count = ay_count;

    const char *ay_names[MAX_ACADEMIC_YEARS + 1];
    int default_ay_idx = 0;
    for (int i = 0; i < ay_count; i++) {
        ay_names[i] = years[i].name;
        d->ay_ids[i] = years[i].id;
        if (is_edit && years[i].id == existing.academic_year_id) {
            default_ay_idx = i;
        } else if (!is_edit && years[i].is_current) {
            default_ay_idx = i;
        }
    }
    ay_names[ay_count] = NULL;

    d->combo_ay = gtk_drop_down_new_from_strings(ay_names);
    if (ay_count > 0) {
        gtk_drop_down_set_selected(GTK_DROP_DOWN(d->combo_ay), default_ay_idx);
    }
    gtk_box_append(GTK_BOX(form_box), d->combo_ay);

    /* Dialog Buttons */
    GtkWidget *btn_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_set_halign(btn_box, GTK_ALIGN_END);
    gtk_widget_set_margin_top(btn_box, 16);

    GtkWidget *cancel_btn = gtk_button_new_with_label("Cancel");
    gtk_widget_add_css_class(cancel_btn, "btn-secondary");
    g_signal_connect(cancel_btn, "clicked", G_CALLBACK(on_cancel_dialog), win);
    gtk_box_append(GTK_BOX(btn_box), cancel_btn);

    GtkWidget *save_btn = gtk_button_new_with_label(is_edit ? "Save Changes" : "Create Class");
    gtk_widget_add_css_class(save_btn, "btn-primary");
    g_signal_connect(save_btn, "clicked", G_CALLBACK(on_save_class_dialog), d);
    gtk_box_append(GTK_BOX(btn_box), save_btn);

    gtk_box_append(GTK_BOX(main_box), btn_box);
    gtk_window_present(GTK_WINDOW(win));
}

/* 2. Add / Edit Section Dialog */
typedef struct {
    ClassesViewWidgets *cvw;
    int section_id;
    GtkWidget *dialog;
    GtkWidget *entry_name;
    GtkWidget *combo_class;
    GtkWidget *combo_teacher;
    int class_ids[MAX_CLASSES];
    int class_count;
    int teacher_ids[TEACHER_MAX_LIST + 1];
    int teacher_count;
} SectionDialogData;

static void on_save_section_dialog(GtkButton *btn, gpointer user_data)
{
    (void)btn;
    SectionDialogData *d = (SectionDialogData *)user_data;

    const char *name = gtk_editable_get_text(GTK_EDITABLE(d->entry_name));
    if (!name || strlen(name) == 0) return;

    ClassSection sec;
    memset(&sec, 0, sizeof(ClassSection));
    sec.id = d->section_id;
    strncpy(sec.name, name, sizeof(sec.name) - 1);

    guint sel_cls = gtk_drop_down_get_selected(GTK_DROP_DOWN(d->combo_class));
    if ((int)sel_cls < d->class_count) {
        sec.class_id = d->class_ids[sel_cls];
    }

    guint sel_tch = gtk_drop_down_get_selected(GTK_DROP_DOWN(d->combo_teacher));
    if ((int)sel_tch < d->teacher_count) {
        sec.class_teacher_id = d->teacher_ids[sel_tch];
    }

    if (d->section_id > 0) {
        section_service_update(&sec);
    } else {
        int new_id = 0;
        section_service_create(&sec, &new_id);
    }

    refresh_view(d->cvw);
    gtk_window_destroy(GTK_WINDOW(d->dialog));
    g_free(d);
}

static void show_add_edit_section_dialog(ClassesViewWidgets *cvw, int class_id, int section_id)
{
    SectionDialogData *d = g_new0(SectionDialogData, 1);
    d->cvw = cvw;
    d->section_id = section_id;

    ClassSection existing;
    memset(&existing, 0, sizeof(ClassSection));
    bool is_edit = false;

    if (section_id > 0) {
        ClassSection all_sec[MAX_SECTIONS];
        int count = 0;
        if (section_service_get_all(all_sec, MAX_SECTIONS, &count)) {
            for (int i = 0; i < count; i++) {
                if (all_sec[i].id == section_id) {
                    existing = all_sec[i];
                    is_edit = true;
                    break;
                }
            }
        }
    }

    GtkWidget *win = gtk_window_new();
    d->dialog = win;
    gtk_window_set_title(GTK_WINDOW(win), is_edit ? "Edit Section" : "Add Section");
    gtk_window_set_modal(GTK_WINDOW(win), TRUE);
    gtk_window_set_default_size(GTK_WINDOW(win), 460, 360);

    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 16);
    gtk_widget_set_margin_start(main_box, 24);
    gtk_widget_set_margin_end(main_box, 24);
    gtk_widget_set_margin_top(main_box, 24);
    gtk_widget_set_margin_bottom(main_box, 24);
    gtk_window_set_child(GTK_WINDOW(win), main_box);

    GtkWidget *title_lbl = gtk_label_new(is_edit ? "Edit Section Details" : "Create New Section");
    gtk_widget_add_css_class(title_lbl, "dashboard-card-title");
    gtk_label_set_xalign(GTK_LABEL(title_lbl), 0.0f);
    gtk_box_append(GTK_BOX(main_box), title_lbl);

    GtkWidget *form_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_box_append(GTK_BOX(main_box), form_box);

    /* Class Dropdown */
    GtkWidget *lbl_cls = gtk_label_new("Target Class:");
    gtk_label_set_xalign(GTK_LABEL(lbl_cls), 0.0f);
    gtk_box_append(GTK_BOX(form_box), lbl_cls);

    SchoolClass classes[MAX_CLASSES];
    int class_count = 0;
    class_service_get_all(classes, MAX_CLASSES, &class_count);
    d->class_count = class_count;

    const char *class_names[MAX_CLASSES + 1];
    int sel_cls_idx = 0;
    int target_class_id = is_edit ? existing.class_id : class_id;

    for (int i = 0; i < class_count; i++) {
        class_names[i] = classes[i].name;
        d->class_ids[i] = classes[i].id;
        if (classes[i].id == target_class_id) sel_cls_idx = i;
    }
    class_names[class_count] = NULL;

    d->combo_class = gtk_drop_down_new_from_strings(class_names);
    if (class_count > 0) {
        gtk_drop_down_set_selected(GTK_DROP_DOWN(d->combo_class), sel_cls_idx);
    }
    gtk_box_append(GTK_BOX(form_box), d->combo_class);

    /* Section Name */
    GtkWidget *lbl_name = gtk_label_new("Section Name (e.g. A, B, C):");
    gtk_label_set_xalign(GTK_LABEL(lbl_name), 0.0f);
    gtk_box_append(GTK_BOX(form_box), lbl_name);
    d->entry_name = gtk_entry_new();
    if (is_edit) gtk_editable_set_text(GTK_EDITABLE(d->entry_name), existing.name);
    gtk_box_append(GTK_BOX(form_box), d->entry_name);

    /* Class Teacher Dropdown */
    GtkWidget *lbl_tch = gtk_label_new("Assigned Class Teacher:");
    gtk_label_set_xalign(GTK_LABEL(lbl_tch), 0.0f);
    gtk_box_append(GTK_BOX(form_box), lbl_tch);

    Teacher teachers[TEACHER_MAX_LIST];
    int tch_count = 0;
    teacher_service_get_all(NULL, "ACTIVE", teachers, TEACHER_MAX_LIST, &tch_count);

    const char *tch_names[TEACHER_MAX_LIST + 2];
    tch_names[0] = "None / Unassigned";
    d->teacher_ids[0] = 0;
    int sel_tch_idx = 0;

    for (int i = 0; i < tch_count; i++) {
        tch_names[i + 1] = teachers[i].full_name;
        d->teacher_ids[i + 1] = teachers[i].id;
        if (is_edit && teachers[i].id == existing.class_teacher_id) {
            sel_tch_idx = i + 1;
        }
    }
    tch_names[tch_count + 1] = NULL;
    d->teacher_count = tch_count + 1;

    d->combo_teacher = gtk_drop_down_new_from_strings(tch_names);
    gtk_drop_down_set_selected(GTK_DROP_DOWN(d->combo_teacher), sel_tch_idx);
    gtk_box_append(GTK_BOX(form_box), d->combo_teacher);

    /* Dialog Buttons */
    GtkWidget *btn_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_set_halign(btn_box, GTK_ALIGN_END);
    gtk_widget_set_margin_top(btn_box, 16);

    GtkWidget *cancel_btn = gtk_button_new_with_label("Cancel");
    gtk_widget_add_css_class(cancel_btn, "btn-secondary");
    g_signal_connect(cancel_btn, "clicked", G_CALLBACK(on_cancel_dialog), win);
    gtk_box_append(GTK_BOX(btn_box), cancel_btn);

    GtkWidget *save_btn = gtk_button_new_with_label(is_edit ? "Save Changes" : "Create Section");
    gtk_widget_add_css_class(save_btn, "btn-primary");
    g_signal_connect(save_btn, "clicked", G_CALLBACK(on_save_section_dialog), d);
    gtk_box_append(GTK_BOX(btn_box), save_btn);

    gtk_box_append(GTK_BOX(main_box), btn_box);
    gtk_window_present(GTK_WINDOW(win));
}

/* 3. Add / Edit Subject Dialog */
typedef struct {
    ClassesViewWidgets *cvw;
    int subject_id;
    GtkWidget *dialog;
    GtkWidget *entry_code;
    GtkWidget *entry_name;
    GtkWidget *combo_class;
    GtkWidget *combo_teacher;
    int class_ids[MAX_CLASSES];
    int class_count;
    int teacher_ids[TEACHER_MAX_LIST + 1];
    int teacher_count;
} SubjectDialogData;

static void on_save_subject_dialog(GtkButton *btn, gpointer user_data)
{
    (void)btn;
    SubjectDialogData *d = (SubjectDialogData *)user_data;

    const char *code = gtk_editable_get_text(GTK_EDITABLE(d->entry_code));
    const char *name = gtk_editable_get_text(GTK_EDITABLE(d->entry_name));

    if (!code || strlen(code) == 0 || !name || strlen(name) == 0) return;

    Subject sub;
    memset(&sub, 0, sizeof(Subject));
    sub.id = d->subject_id;
    strncpy(sub.code, code, sizeof(sub.code) - 1);
    strncpy(sub.name, name, sizeof(sub.name) - 1);

    guint sel_cls = gtk_drop_down_get_selected(GTK_DROP_DOWN(d->combo_class));
    if ((int)sel_cls < d->class_count) {
        sub.class_id = d->class_ids[sel_cls];
    }

    guint sel_tch = gtk_drop_down_get_selected(GTK_DROP_DOWN(d->combo_teacher));
    if ((int)sel_tch < d->teacher_count) {
        sub.teacher_id = d->teacher_ids[sel_tch];
    }

    if (d->subject_id > 0) {
        subject_service_update(&sub);
    } else {
        int new_id = 0;
        subject_service_create(&sub, &new_id);
    }

    refresh_view(d->cvw);
    gtk_window_destroy(GTK_WINDOW(d->dialog));
    g_free(d);
}

static void show_add_edit_subject_dialog(ClassesViewWidgets *cvw, int subject_id)
{
    SubjectDialogData *d = g_new0(SubjectDialogData, 1);
    d->cvw = cvw;
    d->subject_id = subject_id;

    Subject existing;
    memset(&existing, 0, sizeof(Subject));
    bool is_edit = false;

    if (subject_id > 0) {
        Subject all_sub[MAX_SUBJECTS];
        int count = 0;
        if (subject_service_get_all(all_sub, MAX_SUBJECTS, &count)) {
            for (int i = 0; i < count; i++) {
                if (all_sub[i].id == subject_id) {
                    existing = all_sub[i];
                    is_edit = true;
                    break;
                }
            }
        }
    }

    GtkWidget *win = gtk_window_new();
    d->dialog = win;
    gtk_window_set_title(GTK_WINDOW(win), is_edit ? "Edit Subject" : "Add Subject");
    gtk_window_set_modal(GTK_WINDOW(win), TRUE);
    gtk_window_set_default_size(GTK_WINDOW(win), 460, 420);

    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 16);
    gtk_widget_set_margin_start(main_box, 24);
    gtk_widget_set_margin_end(main_box, 24);
    gtk_widget_set_margin_top(main_box, 24);
    gtk_widget_set_margin_bottom(main_box, 24);
    gtk_window_set_child(GTK_WINDOW(win), main_box);

    GtkWidget *title_lbl = gtk_label_new(is_edit ? "Edit Subject Details" : "Create New Subject");
    gtk_widget_add_css_class(title_lbl, "dashboard-card-title");
    gtk_label_set_xalign(GTK_LABEL(title_lbl), 0.0f);
    gtk_box_append(GTK_BOX(main_box), title_lbl);

    GtkWidget *form_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_box_append(GTK_BOX(main_box), form_box);

    /* Subject Code */
    GtkWidget *lbl_code = gtk_label_new("Subject Code (e.g. ENG-101):");
    gtk_label_set_xalign(GTK_LABEL(lbl_code), 0.0f);
    gtk_box_append(GTK_BOX(form_box), lbl_code);
    d->entry_code = gtk_entry_new();
    if (is_edit) gtk_editable_set_text(GTK_EDITABLE(d->entry_code), existing.code);
    gtk_box_append(GTK_BOX(form_box), d->entry_code);

    /* Subject Name */
    GtkWidget *lbl_name = gtk_label_new("Subject Name (e.g. English Literature):");
    gtk_label_set_xalign(GTK_LABEL(lbl_name), 0.0f);
    gtk_box_append(GTK_BOX(form_box), lbl_name);
    d->entry_name = gtk_entry_new();
    if (is_edit) gtk_editable_set_text(GTK_EDITABLE(d->entry_name), existing.name);
    gtk_box_append(GTK_BOX(form_box), d->entry_name);

    /* Class Dropdown */
    GtkWidget *lbl_cls = gtk_label_new("Class:");
    gtk_label_set_xalign(GTK_LABEL(lbl_cls), 0.0f);
    gtk_box_append(GTK_BOX(form_box), lbl_cls);

    SchoolClass classes[MAX_CLASSES];
    int class_count = 0;
    class_service_get_all(classes, MAX_CLASSES, &class_count);
    d->class_count = class_count;

    const char *class_names[MAX_CLASSES + 1];
    int sel_cls_idx = 0;
    for (int i = 0; i < class_count; i++) {
        class_names[i] = classes[i].name;
        d->class_ids[i] = classes[i].id;
        if (is_edit && classes[i].id == existing.class_id) sel_cls_idx = i;
    }
    class_names[class_count] = NULL;

    d->combo_class = gtk_drop_down_new_from_strings(class_names);
    if (class_count > 0) {
        gtk_drop_down_set_selected(GTK_DROP_DOWN(d->combo_class), sel_cls_idx);
    }
    gtk_box_append(GTK_BOX(form_box), d->combo_class);

    /* Teacher Dropdown */
    GtkWidget *lbl_tch = gtk_label_new("Assigned Teacher:");
    gtk_label_set_xalign(GTK_LABEL(lbl_tch), 0.0f);
    gtk_box_append(GTK_BOX(form_box), lbl_tch);

    Teacher teachers[TEACHER_MAX_LIST];
    int tch_count = 0;
    teacher_service_get_all(NULL, "ACTIVE", teachers, TEACHER_MAX_LIST, &tch_count);

    const char *tch_names[TEACHER_MAX_LIST + 2];
    tch_names[0] = "None / Unassigned";
    d->teacher_ids[0] = 0;
    int sel_tch_idx = 0;

    for (int i = 0; i < tch_count; i++) {
        tch_names[i + 1] = teachers[i].full_name;
        d->teacher_ids[i + 1] = teachers[i].id;
        if (is_edit && teachers[i].id == existing.teacher_id) {
            sel_tch_idx = i + 1;
        }
    }
    tch_names[tch_count + 1] = NULL;
    d->teacher_count = tch_count + 1;

    d->combo_teacher = gtk_drop_down_new_from_strings(tch_names);
    gtk_drop_down_set_selected(GTK_DROP_DOWN(d->combo_teacher), sel_tch_idx);
    gtk_box_append(GTK_BOX(form_box), d->combo_teacher);

    /* Dialog Buttons */
    GtkWidget *btn_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_set_halign(btn_box, GTK_ALIGN_END);
    gtk_widget_set_margin_top(btn_box, 16);

    GtkWidget *cancel_btn = gtk_button_new_with_label("Cancel");
    gtk_widget_add_css_class(cancel_btn, "btn-secondary");
    g_signal_connect(cancel_btn, "clicked", G_CALLBACK(on_cancel_dialog), win);
    gtk_box_append(GTK_BOX(btn_box), cancel_btn);

    GtkWidget *save_btn = gtk_button_new_with_label(is_edit ? "Save Changes" : "Create Subject");
    gtk_widget_add_css_class(save_btn, "btn-primary");
    g_signal_connect(save_btn, "clicked", G_CALLBACK(on_save_subject_dialog), d);
    gtk_box_append(GTK_BOX(btn_box), save_btn);

    gtk_box_append(GTK_BOX(main_box), btn_box);
    gtk_window_present(GTK_WINDOW(win));
}

/* 4. Add Academic Year Dialog */
typedef struct {
    ClassesViewWidgets *cvw;
    GtkWidget *dialog;
    GtkWidget *entry_name;
    GtkWidget *entry_start;
    GtkWidget *entry_end;
    GtkWidget *chk_current;
} AcademicYearDialogData;

static void on_save_ay_dialog(GtkButton *btn, gpointer user_data)
{
    (void)btn;
    AcademicYearDialogData *d = (AcademicYearDialogData *)user_data;

    const char *name = gtk_editable_get_text(GTK_EDITABLE(d->entry_name));
    const char *start_dt = gtk_editable_get_text(GTK_EDITABLE(d->entry_start));
    const char *end_dt = gtk_editable_get_text(GTK_EDITABLE(d->entry_end));

    if (!name || strlen(name) == 0) return;

    AcademicYear ay;
    memset(&ay, 0, sizeof(AcademicYear));
    strncpy(ay.name, name, sizeof(ay.name) - 1);
    if (start_dt) strncpy(ay.start_date, start_dt, sizeof(ay.start_date) - 1);
    if (end_dt) strncpy(ay.end_date, end_dt, sizeof(ay.end_date) - 1);
    ay.is_current = gtk_check_button_get_active(GTK_CHECK_BUTTON(d->chk_current));

    int new_id = 0;
    academic_year_create(&ay, &new_id);

    refresh_view(d->cvw);
    gtk_window_destroy(GTK_WINDOW(d->dialog));
    g_free(d);
}

static void show_add_academic_year_dialog(ClassesViewWidgets *cvw)
{
    AcademicYearDialogData *d = g_new0(AcademicYearDialogData, 1);
    d->cvw = cvw;

    GtkWidget *win = gtk_window_new();
    d->dialog = win;
    gtk_window_set_title(GTK_WINDOW(win), "Add Academic Year");
    gtk_window_set_modal(GTK_WINDOW(win), TRUE);
    gtk_window_set_default_size(GTK_WINDOW(win), 440, 360);

    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 16);
    gtk_widget_set_margin_start(main_box, 24);
    gtk_widget_set_margin_end(main_box, 24);
    gtk_widget_set_margin_top(main_box, 24);
    gtk_widget_set_margin_bottom(main_box, 24);
    gtk_window_set_child(GTK_WINDOW(win), main_box);

    GtkWidget *title_lbl = gtk_label_new("New Academic Year");
    gtk_widget_add_css_class(title_lbl, "dashboard-card-title");
    gtk_label_set_xalign(GTK_LABEL(title_lbl), 0.0f);
    gtk_box_append(GTK_BOX(main_box), title_lbl);

    GtkWidget *form_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_box_append(GTK_BOX(main_box), form_box);

    /* Year Name */
    GtkWidget *lbl_name = gtk_label_new("Academic Year Name (e.g. 2027-2028):");
    gtk_label_set_xalign(GTK_LABEL(lbl_name), 0.0f);
    gtk_box_append(GTK_BOX(form_box), lbl_name);
    d->entry_name = gtk_entry_new();
    gtk_box_append(GTK_BOX(form_box), d->entry_name);

    /* Start Date */
    GtkWidget *lbl_start = gtk_label_new("Start Date (YYYY-MM-DD):");
    gtk_label_set_xalign(GTK_LABEL(lbl_start), 0.0f);
    gtk_box_append(GTK_BOX(form_box), lbl_start);
    d->entry_start = gtk_entry_new();
    gtk_editable_set_text(GTK_EDITABLE(d->entry_start), "2027-04-01");
    gtk_box_append(GTK_BOX(form_box), d->entry_start);

    /* End Date */
    GtkWidget *lbl_end = gtk_label_new("End Date (YYYY-MM-DD):");
    gtk_label_set_xalign(GTK_LABEL(lbl_end), 0.0f);
    gtk_box_append(GTK_BOX(form_box), lbl_end);
    d->entry_end = gtk_entry_new();
    gtk_editable_set_text(GTK_EDITABLE(d->entry_end), "2028-03-31");
    gtk_box_append(GTK_BOX(form_box), d->entry_end);

    /* Checkbox: Is Current */
    d->chk_current = gtk_check_button_new_with_label("Set as Active / Current Academic Year");
    gtk_box_append(GTK_BOX(form_box), d->chk_current);

    /* Buttons */
    GtkWidget *btn_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_set_halign(btn_box, GTK_ALIGN_END);
    gtk_widget_set_margin_top(btn_box, 16);

    GtkWidget *cancel_btn = gtk_button_new_with_label("Cancel");
    gtk_widget_add_css_class(cancel_btn, "btn-secondary");
    g_signal_connect(cancel_btn, "clicked", G_CALLBACK(on_cancel_dialog), win);
    gtk_box_append(GTK_BOX(btn_box), cancel_btn);

    GtkWidget *save_btn = gtk_button_new_with_label("Save Year");
    gtk_widget_add_css_class(save_btn, "btn-primary");
    g_signal_connect(save_btn, "clicked", G_CALLBACK(on_save_ay_dialog), d);
    gtk_box_append(GTK_BOX(btn_box), save_btn);

    gtk_box_append(GTK_BOX(main_box), btn_box);
    gtk_window_present(GTK_WINDOW(win));
}

/* 5. Delete Dialogs */
typedef struct {
    ClassesViewWidgets *cvw;
    int id;
    int type; /* 0: class, 1: section, 2: subject */
    GtkWidget *dialog;
} DeleteDialogData;

static void on_confirm_delete(GtkButton *btn, gpointer user_data)
{
    (void)btn;
    DeleteDialogData *d = (DeleteDialogData *)user_data;
    if (d->type == 0) {
        class_service_delete(d->id);
    } else if (d->type == 1) {
        section_service_delete(d->id);
    } else {
        subject_service_delete(d->id);
    }
    refresh_view(d->cvw);
    gtk_window_destroy(GTK_WINDOW(d->dialog));
    g_free(d);
}

static void show_delete_class_dialog(ClassesViewWidgets *cvw, int class_id, const char *name)
{
    DeleteDialogData *d = g_new0(DeleteDialogData, 1);
    d->cvw = cvw;
    d->id = class_id;
    d->type = 0;

    GtkWidget *win = gtk_window_new();
    d->dialog = win;
    gtk_window_set_title(GTK_WINDOW(win), "Confirm Class Deletion");
    gtk_window_set_modal(GTK_WINDOW(win), TRUE);
    gtk_window_set_default_size(GTK_WINDOW(win), 400, 200);

    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 16);
    gtk_widget_set_margin_start(box, 24);
    gtk_widget_set_margin_end(box, 24);
    gtk_widget_set_margin_top(box, 24);
    gtk_widget_set_margin_bottom(box, 24);
    gtk_window_set_child(GTK_WINDOW(win), box);

    char msg[256];
    snprintf(msg, sizeof(msg), "Are you sure you want to delete '%s'?\nAll associated sections and subjects may be affected.", name);
    GtkWidget *lbl = gtk_label_new(msg);
    gtk_label_set_wrap(GTK_LABEL(lbl), TRUE);
    gtk_box_append(GTK_BOX(box), lbl);

    GtkWidget *btn_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_set_halign(btn_box, GTK_ALIGN_END);

    GtkWidget *cancel = gtk_button_new_with_label("Cancel");
    gtk_widget_add_css_class(cancel, "btn-secondary");
    g_signal_connect(cancel, "clicked", G_CALLBACK(on_cancel_dialog), win);
    gtk_box_append(GTK_BOX(btn_box), cancel);

    GtkWidget *confirm = gtk_button_new_with_label("Delete Class");
    gtk_widget_add_css_class(confirm, "btn-danger");
    g_signal_connect(confirm, "clicked", G_CALLBACK(on_confirm_delete), d);
    gtk_box_append(GTK_BOX(btn_box), confirm);

    gtk_box_append(GTK_BOX(box), btn_box);
    gtk_window_present(GTK_WINDOW(win));
}

static void show_delete_section_dialog(ClassesViewWidgets *cvw, int section_id, const char *name)
{
    DeleteDialogData *d = g_new0(DeleteDialogData, 1);
    d->cvw = cvw;
    d->id = section_id;
    d->type = 1;

    GtkWidget *win = gtk_window_new();
    d->dialog = win;
    gtk_window_set_title(GTK_WINDOW(win), "Confirm Section Deletion");
    gtk_window_set_modal(GTK_WINDOW(win), TRUE);
    gtk_window_set_default_size(GTK_WINDOW(win), 400, 180);

    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 16);
    gtk_widget_set_margin_start(box, 24);
    gtk_widget_set_margin_end(box, 24);
    gtk_widget_set_margin_top(box, 24);
    gtk_widget_set_margin_bottom(box, 24);
    gtk_window_set_child(GTK_WINDOW(win), box);

    char msg[256];
    snprintf(msg, sizeof(msg), "Are you sure you want to delete section '%s'?", name);
    GtkWidget *lbl = gtk_label_new(msg);
    gtk_label_set_wrap(GTK_LABEL(lbl), TRUE);
    gtk_box_append(GTK_BOX(box), lbl);

    GtkWidget *btn_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_set_halign(btn_box, GTK_ALIGN_END);

    GtkWidget *cancel = gtk_button_new_with_label("Cancel");
    gtk_widget_add_css_class(cancel, "btn-secondary");
    g_signal_connect(cancel, "clicked", G_CALLBACK(on_cancel_dialog), win);
    gtk_box_append(GTK_BOX(btn_box), cancel);

    GtkWidget *confirm = gtk_button_new_with_label("Delete Section");
    gtk_widget_add_css_class(confirm, "btn-danger");
    g_signal_connect(confirm, "clicked", G_CALLBACK(on_confirm_delete), d);
    gtk_box_append(GTK_BOX(btn_box), confirm);

    gtk_box_append(GTK_BOX(box), btn_box);
    gtk_window_present(GTK_WINDOW(win));
}

static void show_delete_subject_dialog(ClassesViewWidgets *cvw, int subject_id, const char *name)
{
    DeleteDialogData *d = g_new0(DeleteDialogData, 1);
    d->cvw = cvw;
    d->id = subject_id;
    d->type = 2;

    GtkWidget *win = gtk_window_new();
    d->dialog = win;
    gtk_window_set_title(GTK_WINDOW(win), "Confirm Subject Deletion");
    gtk_window_set_modal(GTK_WINDOW(win), TRUE);
    gtk_window_set_default_size(GTK_WINDOW(win), 400, 180);

    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 16);
    gtk_widget_set_margin_start(box, 24);
    gtk_widget_set_margin_end(box, 24);
    gtk_widget_set_margin_top(box, 24);
    gtk_widget_set_margin_bottom(box, 24);
    gtk_window_set_child(GTK_WINDOW(win), box);

    char msg[256];
    snprintf(msg, sizeof(msg), "Are you sure you want to delete subject '%s'?", name);
    GtkWidget *lbl = gtk_label_new(msg);
    gtk_label_set_wrap(GTK_LABEL(lbl), TRUE);
    gtk_box_append(GTK_BOX(box), lbl);

    GtkWidget *btn_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_set_halign(btn_box, GTK_ALIGN_END);

    GtkWidget *cancel = gtk_button_new_with_label("Cancel");
    gtk_widget_add_css_class(cancel, "btn-secondary");
    g_signal_connect(cancel, "clicked", G_CALLBACK(on_cancel_dialog), win);
    gtk_box_append(GTK_BOX(btn_box), cancel);

    GtkWidget *confirm = gtk_button_new_with_label("Delete Subject");
    gtk_widget_add_css_class(confirm, "btn-danger");
    g_signal_connect(confirm, "clicked", G_CALLBACK(on_confirm_delete), d);
    gtk_box_append(GTK_BOX(btn_box), confirm);

    gtk_box_append(GTK_BOX(box), btn_box);
    gtk_window_present(GTK_WINDOW(win));
}

/* ── Main View Constructor ────────────────────────────────────────── */

GtkWidget *school_classes_view_new(void)
{
    ClassesViewWidgets *cvw = g_new0(ClassesViewWidgets, 1);
    cvw->current_tab = TAB_CLASSES;

    /* Main Scrollable View */
    GtkWidget *scrolled = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
                                  GTK_POLICY_AUTOMATIC,
                                  GTK_POLICY_AUTOMATIC);

    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 24);
    gtk_widget_add_css_class(box, "page-container");
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled), box);
    cvw->root_box = box;
    g_object_set_data_full(G_OBJECT(scrolled), "cvw", cvw, g_free);

    /* ── Top Header Row ───────────────────────────────────────────── */
    GtkWidget *top_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 16);
    gtk_widget_set_valign(top_row, GTK_ALIGN_CENTER);

    GtkWidget *title_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    GtkWidget *title = gtk_label_new("Classes & Curriculum");
    gtk_widget_add_css_class(title, "page-title");
    gtk_label_set_xalign(GTK_LABEL(title), 0.0f);
    gtk_box_append(GTK_BOX(title_box), title);

    GtkWidget *sub = gtk_label_new("Configure classes, sections, assigned class teachers, subjects, and academic years");
    gtk_widget_add_css_class(sub, "page-subtitle");
    gtk_label_set_xalign(GTK_LABEL(sub), 0.0f);
    gtk_box_append(GTK_BOX(title_box), sub);

    gtk_widget_set_hexpand(title_box, TRUE);
    gtk_box_append(GTK_BOX(top_row), title_box);

    /* Primary Action Button */
    cvw->add_btn = gtk_button_new_with_label("+ Add Class");
    gtk_widget_add_css_class(cvw->add_btn, "btn-primary");
    g_signal_connect(cvw->add_btn, "clicked", G_CALLBACK(on_add_primary_clicked), cvw);
    gtk_box_append(GTK_BOX(top_row), cvw->add_btn);

    gtk_box_append(GTK_BOX(box), top_row);

    /* ── Tab Bar ─────────────────────────────────────────────────── */
    GtkWidget *tab_bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_margin_bottom(tab_bar, 8);

    cvw->tab_classes_btn = gtk_button_new_with_label("Classes & Sections");
    gtk_widget_add_css_class(cvw->tab_classes_btn, "tab-btn");
    gtk_widget_add_css_class(cvw->tab_classes_btn, "tab-btn-active");
    g_signal_connect(cvw->tab_classes_btn, "clicked", G_CALLBACK(on_tab_classes_clicked), cvw);
    gtk_box_append(GTK_BOX(tab_bar), cvw->tab_classes_btn);

    cvw->tab_subjects_btn = gtk_button_new_with_label("Subjects & Faculty");
    gtk_widget_add_css_class(cvw->tab_subjects_btn, "tab-btn");
    g_signal_connect(cvw->tab_subjects_btn, "clicked", G_CALLBACK(on_tab_subjects_clicked), cvw);
    gtk_box_append(GTK_BOX(tab_bar), cvw->tab_subjects_btn);

    cvw->tab_ay_btn = gtk_button_new_with_label("Academic Years");
    gtk_widget_add_css_class(cvw->tab_ay_btn, "tab-btn");
    g_signal_connect(cvw->tab_ay_btn, "clicked", G_CALLBACK(on_tab_ay_clicked), cvw);
    gtk_box_append(GTK_BOX(tab_bar), cvw->tab_ay_btn);

    gtk_box_append(GTK_BOX(box), tab_bar);

    /* ── Content Container ───────────────────────────────────────── */
    cvw->content_container = gtk_box_new(GTK_ORIENTATION_VERTICAL, 16);
    gtk_box_append(GTK_BOX(box), cvw->content_container);

    refresh_view(cvw);

    return scrolled;
}

void school_classes_view_reload(GtkWidget *view)
{
    if (!view) return;
    ClassesViewWidgets *cvw = (ClassesViewWidgets *)g_object_get_data(G_OBJECT(view), "cvw");
    if (cvw) refresh_view(cvw);
}
