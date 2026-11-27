/**
 * @file exams_view.c
 * @brief Examination GUI module: schedule, marks entry, report cards, and rank merit boards.
 */

#include "ui/exams_view.h"
#include "ui/dialog.h"
#include "utils/print_pdf.h"
#include "settings.h"
#include "exams.h"
#include "classes.h"
#include "student.h"
#include "auth/auth.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_EXAM_STUDENTS 100

typedef enum {
    TAB_EXAMS = 0,
    TAB_MARKS,
    TAB_REPORTS,
    TAB_RANKS
} ExamsTab;

typedef struct {
    int student_id;
    GtkWidget *entry_marks;
    GtkWidget *lbl_grade;
    GtkWidget *entry_remarks;
} MarksRowWidgets;

typedef struct {
    GtkWidget *root_box;
    GtkWidget *tab_exams_btn;
    GtkWidget *tab_marks_btn;
    GtkWidget *tab_reports_btn;
    GtkWidget *tab_ranks_btn;
    GtkWidget *content_container;
    GtkWidget *primary_action_btn;
    GtkWidget *status_banner;

    ExamsTab current_tab;

    /* Marks tab state */
    GtkWidget *marks_combo_exam;
    GtkWidget *marks_combo_subject;
    GtkWidget *marks_roster_box;
    GtkWidget *btn_save_marks;

    Examination exams[MAX_EXAMINATIONS];
    int exam_count;
    ExamSubject exam_subjects[MAX_EXAM_SUBJECTS];
    int exam_subject_count;

    ExamResult current_sheet[MAX_EXAM_STUDENTS];
    int sheet_count;
    MarksRowWidgets row_widgets[MAX_EXAM_STUDENTS];

    int selected_exam_id;
    int selected_exam_subject_id;

    /* Report Card tab state */
    GtkWidget *report_combo_exam;
    GtkWidget *report_combo_student;
    int report_student_ids[MAX_EXAM_STUDENTS];
    int report_student_count;

    /* Ranks tab state */
    GtkWidget *ranks_combo_exam;
    GtkWidget *ranks_combo_class;
    SchoolClass classes[MAX_CLASSES];
    int class_count;
    int ranks_class_id;
} ExamsViewWidgets;

static void refresh_view(ExamsViewWidgets *evw);
static void show_add_exam_dialog(ExamsViewWidgets *evw);
static void show_schedule_subject_dialog(ExamsViewWidgets *evw, int exam_id);
static void show_marksheet_dialog(const StudentReportCard *card);

static void show_status_message(ExamsViewWidgets *evw, const char *msg, bool is_success)
{
    if (!evw || !evw->status_banner) return;
    gtk_label_set_text(GTK_LABEL(evw->status_banner), msg);
    if (is_success) {
        gtk_widget_remove_css_class(evw->status_banner, "card-kpi-red");
        gtk_widget_add_css_class(evw->status_banner, "card-kpi-green");
    } else {
        gtk_widget_remove_css_class(evw->status_banner, "card-kpi-green");
        gtk_widget_add_css_class(evw->status_banner, "card-kpi-red");
    }
    gtk_widget_set_visible(evw->status_banner, TRUE);
}

/* ── Tab Switch Handlers ─────────────────────────────────────────── */

static void on_tab_exams_clicked(GtkButton *btn, gpointer user_data)
{
    (void)btn;
    ExamsViewWidgets *evw = (ExamsViewWidgets *)user_data;
    evw->current_tab = TAB_EXAMS;
    if (evw->status_banner) gtk_widget_set_visible(evw->status_banner, FALSE);
    gtk_widget_add_css_class(evw->tab_exams_btn, "tab-btn-active");
    gtk_widget_remove_css_class(evw->tab_marks_btn, "tab-btn-active");
    gtk_widget_remove_css_class(evw->tab_reports_btn, "tab-btn-active");
    gtk_widget_remove_css_class(evw->tab_ranks_btn, "tab-btn-active");
    gtk_widget_set_visible(evw->primary_action_btn, auth_current_user_can_access("EXAMS", true));
    gtk_button_set_label(GTK_BUTTON(evw->primary_action_btn), "+ Create Exam");
    refresh_view(evw);
}

static void on_tab_marks_clicked(GtkButton *btn, gpointer user_data)
{
    (void)btn;
    ExamsViewWidgets *evw = (ExamsViewWidgets *)user_data;
    evw->current_tab = TAB_MARKS;
    if (evw->status_banner) gtk_widget_set_visible(evw->status_banner, FALSE);
    gtk_widget_remove_css_class(evw->tab_exams_btn, "tab-btn-active");
    gtk_widget_add_css_class(evw->tab_marks_btn, "tab-btn-active");
    gtk_widget_remove_css_class(evw->tab_reports_btn, "tab-btn-active");
    gtk_widget_remove_css_class(evw->tab_ranks_btn, "tab-btn-active");
    gtk_widget_set_visible(evw->primary_action_btn, FALSE);
    refresh_view(evw);
}

static void on_tab_reports_clicked(GtkButton *btn, gpointer user_data)
{
    (void)btn;
    ExamsViewWidgets *evw = (ExamsViewWidgets *)user_data;
    evw->current_tab = TAB_REPORTS;
    if (evw->status_banner) gtk_widget_set_visible(evw->status_banner, FALSE);
    gtk_widget_remove_css_class(evw->tab_exams_btn, "tab-btn-active");
    gtk_widget_remove_css_class(evw->tab_marks_btn, "tab-btn-active");
    gtk_widget_add_css_class(evw->tab_reports_btn, "tab-btn-active");
    gtk_widget_remove_css_class(evw->tab_ranks_btn, "tab-btn-active");
    gtk_widget_set_visible(evw->primary_action_btn, FALSE);
    refresh_view(evw);
}

static void on_tab_ranks_clicked(GtkButton *btn, gpointer user_data)
{
    (void)btn;
    ExamsViewWidgets *evw = (ExamsViewWidgets *)user_data;
    evw->current_tab = TAB_RANKS;
    if (evw->status_banner) gtk_widget_set_visible(evw->status_banner, FALSE);
    gtk_widget_remove_css_class(evw->tab_exams_btn, "tab-btn-active");
    gtk_widget_remove_css_class(evw->tab_marks_btn, "tab-btn-active");
    gtk_widget_remove_css_class(evw->tab_reports_btn, "tab-btn-active");
    gtk_widget_add_css_class(evw->tab_ranks_btn, "tab-btn-active");
    gtk_widget_set_visible(evw->primary_action_btn, FALSE);
    refresh_view(evw);
}

static void on_primary_action_clicked(GtkButton *btn, gpointer user_data)
{
    (void)btn;
    ExamsViewWidgets *evw = (ExamsViewWidgets *)user_data;
    if (!auth_current_user_can_access("EXAMS", true)) {
        show_status_message(evw, "Read-only access: your role cannot create or schedule exams.", false);
        return;
    }
    if (evw->current_tab == TAB_EXAMS) {
        show_add_exam_dialog(evw);
    }
}

/* ── 1. Tab: Exams & Schedules ────────────────────────────────────── */

static void on_schedule_subject_clicked(GtkButton *btn, gpointer user_data)
{
    ExamsViewWidgets *evw = (ExamsViewWidgets *)g_object_get_data(G_OBJECT(btn), "evw");
    if (!auth_current_user_can_access("EXAMS", true)) {
        show_status_message(evw, "Read-only access: your role cannot schedule exam subjects.", false);
        return;
    }
    int exam_id = GPOINTER_TO_INT(user_data);
    show_schedule_subject_dialog(evw, exam_id);
}

typedef struct {
    ExamsViewWidgets *evw;
    int exam_id;
    GtkWidget *dialog;
} DeleteExamData;

static void on_confirm_delete_exam(GtkButton *btn, gpointer user_data)
{
    (void)btn;
    DeleteExamData *d = (DeleteExamData *)user_data;
    if (exam_service_delete(d->exam_id)) {
        show_status_message(d->evw, "Examination deleted successfully.", true);
    } else {
        show_status_message(d->evw, "Failed to delete examination.", false);
    }
    refresh_view(d->evw);
    school_dialog_close(d->dialog);
}

static void show_delete_exam_dialog(ExamsViewWidgets *evw, int exam_id, const char *exam_name)
{
    DeleteExamData *d = g_new0(DeleteExamData, 1);
    d->evw = evw;
    d->exam_id = exam_id;

    GtkWidget *win = school_dialog_new("Confirm Exam Deletion", 420, 200);
    d->dialog = win;

    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 16);
    gtk_widget_set_margin_start(box, 24);
    gtk_widget_set_margin_end(box, 24);
    gtk_widget_set_margin_top(box, 20);
    gtk_widget_set_margin_bottom(box, 20);
    school_dialog_set_child(win, box);

    char msg[256];
    snprintf(msg, sizeof(msg), "Are you sure you want to delete examination '%s'?\nAll scheduled subjects, marks, and rankings will be permanently removed.",
             exam_name ? exam_name : "this exam");
    GtkWidget *lbl = gtk_label_new(msg);
    gtk_label_set_wrap(GTK_LABEL(lbl), TRUE);
    gtk_box_append(GTK_BOX(box), lbl);

    GtkWidget *btn_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_set_halign(btn_box, GTK_ALIGN_END);

    GtkWidget *cancel = gtk_button_new_with_label("Cancel");
    gtk_widget_add_css_class(cancel, "btn-secondary");
    g_signal_connect_swapped(cancel, "clicked", G_CALLBACK(school_dialog_close), win);
    gtk_box_append(GTK_BOX(btn_box), cancel);

    GtkWidget *del = gtk_button_new_with_label("Delete Exam");
    gtk_widget_add_css_class(del, "btn-danger");
    g_signal_connect(del, "clicked", G_CALLBACK(on_confirm_delete_exam), d);
    gtk_box_append(GTK_BOX(btn_box), del);

    gtk_box_append(GTK_BOX(box), btn_box);
    g_object_set_data_full(G_OBJECT(win), "d", d, g_free);
    school_dialog_present(win);
}

static void on_delete_exam_clicked(GtkButton *btn, gpointer user_data)
{
    ExamsViewWidgets *evw = (ExamsViewWidgets *)g_object_get_data(G_OBJECT(btn), "evw");
    int exam_id = GPOINTER_TO_INT(user_data);
    const char *name = (const char *)g_object_get_data(G_OBJECT(btn), "exam_name");
    show_delete_exam_dialog(evw, exam_id, name ? name : "Examination");
}

static void render_exams_tab(ExamsViewWidgets *evw)
{
    evw->exam_count = 0;
    exam_service_get_all(evw->exams, MAX_EXAMINATIONS, &evw->exam_count);

    if (evw->exam_count == 0) {
        GtkWidget *empty_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
        gtk_widget_set_margin_top(empty_box, 30);
        gtk_widget_set_margin_bottom(empty_box, 30);
        GtkWidget *lbl = gtk_label_new("No examinations planned. Click '+ Create Exam' to configure an examination.");
        gtk_widget_add_css_class(lbl, "dim-label");
        gtk_box_append(GTK_BOX(empty_box), lbl);
        gtk_box_append(GTK_BOX(evw->content_container), empty_box);
        return;
    }

    for (int i = 0; i < evw->exam_count; i++) {
        const Examination *ex = &evw->exams[i];

        GtkWidget *card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
        gtk_widget_add_css_class(card, "card");
        gtk_widget_set_margin_bottom(card, 16);

        /* Card Header */
        GtkWidget *header_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
        gtk_widget_set_valign(header_row, GTK_ALIGN_CENTER);

        GtkWidget *name_lbl = gtk_label_new(ex->name);
        gtk_widget_add_css_class(name_lbl, "dashboard-card-title");
        gtk_label_set_xalign(GTK_LABEL(name_lbl), 0.0f);
        gtk_widget_set_hexpand(name_lbl, TRUE);
        gtk_box_append(GTK_BOX(header_row), name_lbl);

        /* Academic Year Badge */
        GtkWidget *ay_lbl = gtk_label_new(ex->academic_year_name);
        gtk_widget_add_css_class(ay_lbl, "badge-role");
        gtk_box_append(GTK_BOX(header_row), ay_lbl);

        /* Dates */
        char dt_buf[64];
        snprintf(dt_buf, sizeof(dt_buf), "%s to %s", ex->start_date, ex->end_date);
        GtkWidget *dt_lbl = gtk_label_new(dt_buf);
        gtk_widget_add_css_class(dt_lbl, "badge-adm");
        gtk_box_append(GTK_BOX(header_row), dt_lbl);

        /* Status */
        GtkWidget *st_lbl = gtk_label_new(ex->status);
        if (strcmp(ex->status, "COMPLETED") == 0) gtk_widget_add_css_class(st_lbl, "badge-status-active");
        else gtk_widget_add_css_class(st_lbl, "badge-status-inactive");
        gtk_box_append(GTK_BOX(header_row), st_lbl);

        /* Actions: Schedule Subject, Delete */
        gboolean can_edit = auth_current_user_can_access("EXAMS", true);

        GtkWidget *sched_btn = gtk_button_new_with_label("+ Schedule Subject");
        gtk_widget_add_css_class(sched_btn, "btn-secondary");
        g_object_set_data(G_OBJECT(sched_btn), "evw", evw);
        g_signal_connect(sched_btn, "clicked", G_CALLBACK(on_schedule_subject_clicked), GINT_TO_POINTER(ex->id));
        gtk_widget_set_visible(sched_btn, can_edit);
        gtk_box_append(GTK_BOX(header_row), sched_btn);

        GtkWidget *del_btn = gtk_button_new_with_label("Delete");
        gtk_widget_add_css_class(del_btn, "btn-danger");
        g_object_set_data(G_OBJECT(del_btn), "evw", evw);
        g_object_set_data_full(G_OBJECT(del_btn), "exam_name", g_strdup(ex->name), g_free);
        g_signal_connect(del_btn, "clicked", G_CALLBACK(on_delete_exam_clicked), GINT_TO_POINTER(ex->id));
        gtk_widget_set_visible(del_btn, can_edit);
        gtk_box_append(GTK_BOX(header_row), del_btn);

        gtk_box_append(GTK_BOX(card), header_row);

        /* Separator */
        GtkWidget *sep = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
        gtk_box_append(GTK_BOX(card), sep);

        /* Scheduled Subjects */
        ExamSubject subjects[MAX_EXAM_SUBJECTS];
        int sub_count = 0;
        exam_subject_get_by_exam(ex->id, subjects, MAX_EXAM_SUBJECTS, &sub_count);

        if (sub_count == 0) {
            GtkWidget *no_sub = gtk_label_new("No subjects scheduled yet for this examination.");
            gtk_widget_add_css_class(no_sub, "dim-label");
            gtk_label_set_xalign(GTK_LABEL(no_sub), 0.0f);
            gtk_box_append(GTK_BOX(card), no_sub);
        } else {
            GtkWidget *sub_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
            for (int s = 0; s < sub_count; s++) {
                const ExamSubject *es = &subjects[s];
                GtkWidget *s_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
                gtk_widget_add_css_class(s_row, "table-data-row");

                GtkWidget *code_lbl = gtk_label_new(es->subject_code);
                gtk_widget_add_css_class(code_lbl, "badge-adm");
                gtk_widget_set_size_request(code_lbl, 110, -1);
                gtk_label_set_xalign(GTK_LABEL(code_lbl), 0.0f);
                gtk_box_append(GTK_BOX(s_row), code_lbl);

                GtkWidget *sname_lbl = gtk_label_new(es->subject_name);
                gtk_widget_add_css_class(sname_lbl, "table-cell-bold");
                gtk_label_set_xalign(GTK_LABEL(sname_lbl), 0.0f);
                gtk_widget_set_hexpand(sname_lbl, TRUE);
                gtk_box_append(GTK_BOX(s_row), sname_lbl);

                GtkWidget *c_lbl = gtk_label_new(es->class_name);
                gtk_widget_set_size_request(c_lbl, 120, -1);
                gtk_box_append(GTK_BOX(s_row), c_lbl);

                char dt_str[32];
                snprintf(dt_str, sizeof(dt_str), "Date: %s", es->exam_date);
                GtkWidget *d_lbl = gtk_label_new(dt_str);
                gtk_widget_set_size_request(d_lbl, 130, -1);
                gtk_box_append(GTK_BOX(s_row), d_lbl);

                char marks_str[64];
                snprintf(marks_str, sizeof(marks_str), "Max: %.0f (Pass: %.0f)", es->max_marks, es->passing_marks);
                GtkWidget *m_lbl = gtk_label_new(marks_str);
                gtk_box_append(GTK_BOX(s_row), m_lbl);

                gtk_box_append(GTK_BOX(sub_box), s_row);
            }
            gtk_box_append(GTK_BOX(card), sub_box);
        }

        gtk_box_append(GTK_BOX(evw->content_container), card);
    }
}

/* ── 2. Tab: Enter Marks ──────────────────────────────────────────── */

static void render_marks_roster(ExamsViewWidgets *evw);

static void on_marks_exam_changed(GtkDropDown *dropdown, GParamSpec *pspec, gpointer user_data)
{
    (void)pspec;
    ExamsViewWidgets *evw = (ExamsViewWidgets *)user_data;
    guint sel = gtk_drop_down_get_selected(dropdown);
    if ((int)sel < evw->exam_count) {
        evw->selected_exam_id = evw->exams[sel].id;
    } else {
        evw->selected_exam_id = 0;
    }

    /* Refresh subjects dropdown */
    evw->exam_subject_count = 0;
    if (evw->selected_exam_id > 0) {
        exam_subject_get_by_exam(evw->selected_exam_id, evw->exam_subjects, MAX_EXAM_SUBJECTS, &evw->exam_subject_count);
    }

    const char *sub_names[MAX_EXAM_SUBJECTS + 1];
    for (int i = 0; i < evw->exam_subject_count; i++) {
        char buf[256];
        snprintf(buf, sizeof(buf), "%s - %s (%s)",
                 evw->exam_subjects[i].subject_code,
                 evw->exam_subjects[i].subject_name,
                 evw->exam_subjects[i].class_name);
        sub_names[i] = g_strdup(buf);
    }
    sub_names[evw->exam_subject_count] = NULL;

    GtkWidget *parent = gtk_widget_get_parent(evw->marks_combo_subject);
    GtkWidget *new_sub_dd = gtk_drop_down_new_from_strings(sub_names);
    if (evw->exam_subject_count > 0) {
        evw->selected_exam_subject_id = evw->exam_subjects[0].id;
        gtk_drop_down_set_selected(GTK_DROP_DOWN(new_sub_dd), 0);
    } else {
        evw->selected_exam_subject_id = 0;
    }

    gtk_box_remove(GTK_BOX(parent), evw->marks_combo_subject);
    gtk_box_append(GTK_BOX(parent), new_sub_dd);
    evw->marks_combo_subject = new_sub_dd;

    render_marks_roster(evw);
}

static void on_marks_subject_changed(GtkDropDown *dropdown, GParamSpec *pspec, gpointer user_data)
{
    (void)pspec;
    ExamsViewWidgets *evw = (ExamsViewWidgets *)user_data;
    guint sel = gtk_drop_down_get_selected(dropdown);
    if ((int)sel < evw->exam_subject_count) {
        evw->selected_exam_subject_id = evw->exam_subjects[sel].id;
    } else {
        evw->selected_exam_subject_id = 0;
    }
    render_marks_roster(evw);
}

static void on_save_marks_clicked(GtkButton *btn, gpointer user_data)
{
    (void)btn;
    ExamsViewWidgets *evw = (ExamsViewWidgets *)user_data;
    if (!auth_current_user_can_access("EXAMS", true)) {
        show_status_message(evw, "Read-only access: your role cannot modify exam marks.", false);
        return;
    }
    if (evw->sheet_count == 0 || evw->selected_exam_subject_id <= 0) return;

    for (int i = 0; i < evw->sheet_count; i++) {
        const char *txt = gtk_editable_get_text(GTK_EDITABLE(evw->row_widgets[i].entry_marks));
        double marks = txt ? atof(txt) : 0.0;

        ExamResult res;
        memset(&res, 0, sizeof(ExamResult));
        res.exam_subject_id = evw->selected_exam_subject_id;
        res.student_id = evw->row_widgets[i].student_id;
        res.marks_obtained = marks;

        const char *rm = gtk_editable_get_text(GTK_EDITABLE(evw->row_widgets[i].entry_remarks));
        if (rm) strncpy(res.remarks, rm, sizeof(res.remarks) - 1);

        int new_id = 0;
        exam_result_record(&res, &new_id);
    }

    show_status_message(evw, "Marks recorded successfully for the subject.", true);
    render_marks_roster(evw);
}

static void render_marks_roster(ExamsViewWidgets *evw)
{
    /* Clear roster container */
    GtkWidget *c = gtk_widget_get_first_child(evw->marks_roster_box);
    while (c) {
        GtkWidget *next = gtk_widget_get_next_sibling(c);
        gtk_box_remove(GTK_BOX(evw->marks_roster_box), c);
        c = next;
    }

    if (evw->selected_exam_subject_id <= 0) {
        GtkWidget *msg = gtk_label_new("Please select an exam and scheduled subject to enter student marks.");
        gtk_widget_add_css_class(msg, "dim-label");
        gtk_box_append(GTK_BOX(evw->marks_roster_box), msg);
        return;
    }

    evw->sheet_count = 0;
    exam_result_get_class_sheet(evw->selected_exam_subject_id, evw->current_sheet, MAX_EXAM_STUDENTS, &evw->sheet_count);

    if (evw->sheet_count == 0) {
        GtkWidget *empty_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
        gtk_widget_set_margin_top(empty_box, 30);
        GtkWidget *lbl = gtk_label_new("No active students found enrolled for this subject/class.");
        gtk_widget_add_css_class(lbl, "dim-label");
        gtk_box_append(GTK_BOX(empty_box), lbl);
        gtk_box_append(GTK_BOX(evw->marks_roster_box), empty_box);
        return;
    }

    GtkWidget *card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_add_css_class(card, "card");
    gtk_box_append(GTK_BOX(evw->marks_roster_box), card);

    /* Table Header */
    GtkWidget *h_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_add_css_class(h_row, "table-header-row");

    const char *cols[] = { "Roll", "Admission No", "Student Name", "Marks Obtained", "Grade", "Status", "Remarks" };
    int widths[] = { 60, 130, 220, 130, 80, 100, 180 };

    for (int i = 0; i < 7; i++) {
        GtkWidget *th = gtk_label_new(cols[i]);
        gtk_widget_add_css_class(th, "table-th");
        gtk_label_set_xalign(GTK_LABEL(th), 0.0f);
        gtk_widget_set_size_request(th, widths[i], -1);
        if (i == 2) gtk_widget_set_hexpand(th, TRUE);
        gtk_box_append(GTK_BOX(h_row), th);
    }
    gtk_box_append(GTK_BOX(card), h_row);

    for (int i = 0; i < evw->sheet_count; i++) {
        const ExamResult *r = &evw->current_sheet[i];
        GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
        gtk_widget_add_css_class(row, "table-data-row");

        /* Roll */
        char r_buf[16];
        snprintf(r_buf, sizeof(r_buf), "%d", r->roll_number);
        GtkWidget *roll_lbl = gtk_label_new(r_buf);
        gtk_widget_set_size_request(roll_lbl, widths[0], -1);
        gtk_box_append(GTK_BOX(row), roll_lbl);

        /* Admission No */
        GtkWidget *adm_lbl = gtk_label_new(r->admission_no);
        gtk_widget_add_css_class(adm_lbl, "badge-adm");
        gtk_widget_set_size_request(adm_lbl, widths[1], -1);
        gtk_label_set_xalign(GTK_LABEL(adm_lbl), 0.0f);
        gtk_box_append(GTK_BOX(row), adm_lbl);

        /* Student Name */
        GtkWidget *name_lbl = gtk_label_new(r->student_name);
        gtk_widget_add_css_class(name_lbl, "table-cell-bold");
        gtk_label_set_xalign(GTK_LABEL(name_lbl), 0.0f);
        gtk_widget_set_hexpand(name_lbl, TRUE);
        gtk_box_append(GTK_BOX(row), name_lbl);

        /* Marks Input */
        gboolean can_edit = auth_current_user_can_access("EXAMS", true);
        GtkWidget *entry_m = gtk_entry_new();
        gtk_widget_set_size_request(entry_m, widths[3], -1);
        char m_buf[32];
        snprintf(m_buf, sizeof(m_buf), "%.1f", r->marks_obtained);
        gtk_editable_set_text(GTK_EDITABLE(entry_m), m_buf);
        gtk_editable_set_editable(GTK_EDITABLE(entry_m), can_edit);
        gtk_widget_set_sensitive(entry_m, can_edit);
        gtk_box_append(GTK_BOX(row), entry_m);

        /* Grade */
        GtkWidget *grd_lbl = gtk_label_new(r->grade);
        gtk_widget_add_css_class(grd_lbl, "badge-adm");
        gtk_widget_set_size_request(grd_lbl, widths[4], -1);
        gtk_box_append(GTK_BOX(row), grd_lbl);

        /* Status (Pass/Fail) */
        GtkWidget *st_lbl = gtk_label_new(r->is_pass ? "PASS" : "FAIL");
        if (r->is_pass) gtk_widget_add_css_class(st_lbl, "badge-status-active");
        else gtk_widget_add_css_class(st_lbl, "badge-status-inactive");
        gtk_widget_set_size_request(st_lbl, widths[5], -1);
        gtk_box_append(GTK_BOX(row), st_lbl);

        /* Remarks Input */
        GtkWidget *entry_rm = gtk_entry_new();
        gtk_widget_set_size_request(entry_rm, widths[6], -1);
        gtk_editable_set_text(GTK_EDITABLE(entry_rm), r->remarks);
        gtk_editable_set_editable(GTK_EDITABLE(entry_rm), can_edit);
        gtk_widget_set_sensitive(entry_rm, can_edit);
        gtk_box_append(GTK_BOX(row), entry_rm);

        /* Bind to row widgets for save action */
        evw->row_widgets[i].student_id = r->student_id;
        evw->row_widgets[i].entry_marks = entry_m;
        evw->row_widgets[i].lbl_grade = grd_lbl;
        evw->row_widgets[i].entry_remarks = entry_rm;

        gtk_box_append(GTK_BOX(card), row);
    }
}

static void render_marks_tab(ExamsViewWidgets *evw)
{
    /* Controls Bar */
    GtkWidget *ctrl_bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 16);
    gtk_widget_add_css_class(ctrl_bar, "card");
    gtk_widget_set_valign(ctrl_bar, GTK_ALIGN_CENTER);

    /* Exam Dropdown */
    GtkWidget *b_ex = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    GtkWidget *lbl_ex = gtk_label_new("Examination:");
    gtk_label_set_xalign(GTK_LABEL(lbl_ex), 0.0f);
    gtk_box_append(GTK_BOX(b_ex), lbl_ex);

    evw->exam_count = 0;
    exam_service_get_all(evw->exams, MAX_EXAMINATIONS, &evw->exam_count);

    const char *ex_names[MAX_EXAMINATIONS + 1];
    for (int i = 0; i < evw->exam_count; i++) {
        ex_names[i] = evw->exams[i].name;
    }
    ex_names[evw->exam_count] = NULL;

    evw->marks_combo_exam = gtk_drop_down_new_from_strings(ex_names);
    if (evw->exam_count > 0) {
        evw->selected_exam_id = evw->exams[0].id;
        gtk_drop_down_set_selected(GTK_DROP_DOWN(evw->marks_combo_exam), 0);
    }
    g_signal_connect(evw->marks_combo_exam, "notify::selected", G_CALLBACK(on_marks_exam_changed), evw);
    gtk_box_append(GTK_BOX(b_ex), evw->marks_combo_exam);
    gtk_box_append(GTK_BOX(ctrl_bar), b_ex);

    /* Scheduled Subject Dropdown */
    GtkWidget *b_sub = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    GtkWidget *lbl_sub = gtk_label_new("Scheduled Subject:");
    gtk_label_set_xalign(GTK_LABEL(lbl_sub), 0.0f);
    gtk_box_append(GTK_BOX(b_sub), lbl_sub);

    evw->exam_subject_count = 0;
    if (evw->selected_exam_id > 0) {
        exam_subject_get_by_exam(evw->selected_exam_id, evw->exam_subjects, MAX_EXAM_SUBJECTS, &evw->exam_subject_count);
    }

    const char *sub_names[MAX_EXAM_SUBJECTS + 1];
    for (int i = 0; i < evw->exam_subject_count; i++) {
        char buf[256];
        snprintf(buf, sizeof(buf), "%s - %s (%s)",
                 evw->exam_subjects[i].subject_code,
                 evw->exam_subjects[i].subject_name,
                 evw->exam_subjects[i].class_name);
        sub_names[i] = g_strdup(buf);
    }
    sub_names[evw->exam_subject_count] = NULL;

    evw->marks_combo_subject = gtk_drop_down_new_from_strings(sub_names);
    if (evw->exam_subject_count > 0) {
        evw->selected_exam_subject_id = evw->exam_subjects[0].id;
        gtk_drop_down_set_selected(GTK_DROP_DOWN(evw->marks_combo_subject), 0);
    }
    g_signal_connect(evw->marks_combo_subject, "notify::selected", G_CALLBACK(on_marks_subject_changed), evw);
    gtk_box_append(GTK_BOX(b_sub), evw->marks_combo_subject);
    gtk_box_append(GTK_BOX(ctrl_bar), b_sub);

    /* Save Button */
    evw->btn_save_marks = gtk_button_new_with_label("Save Marks");
    gtk_widget_add_css_class(evw->btn_save_marks, "btn-primary");
    gtk_widget_set_valign(evw->btn_save_marks, GTK_ALIGN_END);
    g_signal_connect(evw->btn_save_marks, "clicked", G_CALLBACK(on_save_marks_clicked), evw);
    gtk_widget_set_visible(evw->btn_save_marks, auth_current_user_can_access("EXAMS", true));
    gtk_box_append(GTK_BOX(ctrl_bar), evw->btn_save_marks);

    gtk_box_append(GTK_BOX(evw->content_container), ctrl_bar);

    /* Roster Container */
    evw->marks_roster_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 16);
    gtk_box_append(GTK_BOX(evw->content_container), evw->marks_roster_box);

    render_marks_roster(evw);
}

/* ── 3. Tab: Report Cards & Marksheets ────────────────────────────── */

static void on_view_report_card_clicked(GtkButton *btn, gpointer user_data)
{
    (void)btn;
    ExamsViewWidgets *evw = (ExamsViewWidgets *)user_data;

    guint sel_ex = gtk_drop_down_get_selected(GTK_DROP_DOWN(evw->report_combo_exam));
    if ((int)sel_ex >= evw->exam_count) return;

    guint sel_stu = gtk_drop_down_get_selected(GTK_DROP_DOWN(evw->report_combo_student));
    if ((int)sel_stu >= evw->report_student_count) return;

    int exam_id = evw->exams[sel_ex].id;
    int student_id = evw->report_student_ids[sel_stu];

    StudentReportCard card;
    if (exam_service_generate_report_card(exam_id, student_id, &card)) {
        show_marksheet_dialog(&card);
    }
}

static void render_reports_tab(ExamsViewWidgets *evw)
{
    GtkWidget *card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 16);
    gtk_widget_add_css_class(card, "card");
    gtk_box_append(GTK_BOX(evw->content_container), card);

    GtkWidget *title_lbl = gtk_label_new("Generate Student Report Card / Marksheet");
    gtk_widget_add_css_class(title_lbl, "dashboard-card-title");
    gtk_label_set_xalign(GTK_LABEL(title_lbl), 0.0f);
    gtk_box_append(GTK_BOX(card), title_lbl);

    GtkWidget *grid = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 16);
    gtk_widget_set_valign(grid, GTK_ALIGN_CENTER);

    /* Exam Dropdown */
    GtkWidget *b_ex = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_hexpand(b_ex, TRUE);
    GtkWidget *lbl_ex = gtk_label_new("Select Examination:");
    gtk_label_set_xalign(GTK_LABEL(lbl_ex), 0.0f);
    gtk_box_append(GTK_BOX(b_ex), lbl_ex);

    evw->exam_count = 0;
    exam_service_get_all(evw->exams, MAX_EXAMINATIONS, &evw->exam_count);

    const char *ex_names[MAX_EXAMINATIONS + 1];
    for (int i = 0; i < evw->exam_count; i++) {
        ex_names[i] = evw->exams[i].name;
    }
    ex_names[evw->exam_count] = NULL;

    evw->report_combo_exam = gtk_drop_down_new_from_strings(ex_names);
    gtk_box_append(GTK_BOX(b_ex), evw->report_combo_exam);
    gtk_box_append(GTK_BOX(grid), b_ex);

    /* Student Dropdown */
    GtkWidget *b_stu = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_hexpand(b_stu, TRUE);
    GtkWidget *lbl_stu = gtk_label_new("Select Student:");
    gtk_label_set_xalign(GTK_LABEL(lbl_stu), 0.0f);
    gtk_box_append(GTK_BOX(b_stu), lbl_stu);

    StudentFilter sf = { .status = "ACTIVE" };
    StudentPage sp;
    student_service_get_page(&sf, &sp);
    evw->report_student_count = sp.count;

    const char *stu_names[MAX_EXAM_STUDENTS + 1];
    for (int i = 0; i < sp.count; i++) {
        char buf[256];
        snprintf(buf, sizeof(buf), "%s (%s) - Class %s",
                 sp.items[i].full_name, sp.items[i].admission_no, sp.items[i].class_name);
        stu_names[i] = g_strdup(buf);
        evw->report_student_ids[i] = sp.items[i].id;
    }
    stu_names[sp.count] = NULL;

    evw->report_combo_student = gtk_drop_down_new_from_strings(stu_names);
    gtk_box_append(GTK_BOX(b_stu), evw->report_combo_student);
    gtk_box_append(GTK_BOX(grid), b_stu);

    /* Generate Button */
    GtkWidget *btn = gtk_button_new_with_label("View Marksheet");
    gtk_widget_add_css_class(btn, "btn-primary");
    gtk_widget_set_valign(btn, GTK_ALIGN_END);
    g_signal_connect(btn, "clicked", G_CALLBACK(on_view_report_card_clicked), evw);
    gtk_box_append(GTK_BOX(grid), btn);

    gtk_box_append(GTK_BOX(card), grid);
}

/* ── 4. Tab: Class Merit & Ranks ──────────────────────────────────── */

static void render_ranks_table(ExamsViewWidgets *evw, GtkWidget *container);

static void on_ranks_filter_changed(GtkDropDown *dropdown, GParamSpec *pspec, gpointer user_data)
{
    (void)dropdown;
    (void)pspec;
    ExamsViewWidgets *evw = (ExamsViewWidgets *)user_data;
    refresh_view(evw);
}

static void render_ranks_tab(ExamsViewWidgets *evw)
{
    GtkWidget *bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 16);
    gtk_widget_add_css_class(bar, "card");
    gtk_widget_set_valign(bar, GTK_ALIGN_CENTER);

    /* Exam Dropdown */
    GtkWidget *b_ex = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    GtkWidget *lbl_ex = gtk_label_new("Examination:");
    gtk_label_set_xalign(GTK_LABEL(lbl_ex), 0.0f);
    gtk_box_append(GTK_BOX(b_ex), lbl_ex);

    evw->exam_count = 0;
    exam_service_get_all(evw->exams, MAX_EXAMINATIONS, &evw->exam_count);

    const char *ex_names[MAX_EXAMINATIONS + 1];
    for (int i = 0; i < evw->exam_count; i++) {
        ex_names[i] = evw->exams[i].name;
    }
    ex_names[evw->exam_count] = NULL;

    evw->ranks_combo_exam = gtk_drop_down_new_from_strings(ex_names);
    g_signal_connect(evw->ranks_combo_exam, "notify::selected", G_CALLBACK(on_ranks_filter_changed), evw);
    gtk_box_append(GTK_BOX(b_ex), evw->ranks_combo_exam);
    gtk_box_append(GTK_BOX(bar), b_ex);

    /* Class Dropdown */
    GtkWidget *b_c = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    GtkWidget *lbl_c = gtk_label_new("Class:");
    gtk_label_set_xalign(GTK_LABEL(lbl_c), 0.0f);
    gtk_box_append(GTK_BOX(b_c), lbl_c);

    evw->class_count = 0;
    class_service_get_all(evw->classes, MAX_CLASSES, &evw->class_count);

    const char *c_names[MAX_CLASSES + 1];
    for (int i = 0; i < evw->class_count; i++) {
        c_names[i] = evw->classes[i].name;
    }
    c_names[evw->class_count] = NULL;

    evw->ranks_combo_class = gtk_drop_down_new_from_strings(c_names);
    g_signal_connect(evw->ranks_combo_class, "notify::selected", G_CALLBACK(on_ranks_filter_changed), evw);
    gtk_box_append(GTK_BOX(b_c), evw->ranks_combo_class);
    gtk_box_append(GTK_BOX(bar), b_c);

    gtk_box_append(GTK_BOX(evw->content_container), bar);

    GtkWidget *table_container = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_append(GTK_BOX(evw->content_container), table_container);

    render_ranks_table(evw, table_container);
}

static void render_ranks_table(ExamsViewWidgets *evw, GtkWidget *container)
{
    guint sel_ex = gtk_drop_down_get_selected(GTK_DROP_DOWN(evw->ranks_combo_exam));
    guint sel_cls = gtk_drop_down_get_selected(GTK_DROP_DOWN(evw->ranks_combo_class));

    if ((int)sel_ex >= evw->exam_count || (int)sel_cls >= evw->class_count) return;

    int exam_id = evw->exams[sel_ex].id;
    int class_id = evw->classes[sel_cls].id;

    ClassExamRank ranks[MAX_CLASS_RANKS];
    int count = 0;
    exam_service_get_class_ranks(exam_id, class_id, ranks, MAX_CLASS_RANKS, &count);

    GtkWidget *card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_add_css_class(card, "card");
    gtk_box_append(GTK_BOX(container), card);

    /* Table Header */
    GtkWidget *h_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_add_css_class(h_row, "table-header-row");

    const char *cols[] = { "Rank", "Roll", "Admission No", "Student Name", "Total Marks", "Percentage", "Grade", "Status" };
    int widths[] = { 60, 50, 130, 220, 120, 110, 80, 100 };

    for (int i = 0; i < 8; i++) {
        GtkWidget *th = gtk_label_new(cols[i]);
        gtk_widget_add_css_class(th, "table-th");
        gtk_label_set_xalign(GTK_LABEL(th), 0.0f);
        gtk_widget_set_size_request(th, widths[i], -1);
        if (i == 3) gtk_widget_set_hexpand(th, TRUE);
        gtk_box_append(GTK_BOX(h_row), th);
    }
    gtk_box_append(GTK_BOX(card), h_row);

    if (count == 0) {
        GtkWidget *empty_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
        gtk_widget_set_margin_top(empty_box, 30);
        GtkWidget *lbl = gtk_label_new("No results or students found for this class and examination.");
        gtk_widget_add_css_class(lbl, "dim-label");
        gtk_box_append(GTK_BOX(empty_box), lbl);
        gtk_box_append(GTK_BOX(card), empty_box);
        return;
    }

    for (int i = 0; i < count; i++) {
        const ClassExamRank *r = &ranks[i];
        GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
        gtk_widget_add_css_class(row, "table-data-row");

        /* Rank Badge */
        char rk_buf[16];
        snprintf(rk_buf, sizeof(rk_buf), "#%d", r->rank);
        GtkWidget *rk_lbl = gtk_label_new(rk_buf);
        if (r->rank == 1) gtk_widget_add_css_class(rk_lbl, "badge-status-active");
        else if (r->rank <= 3) gtk_widget_add_css_class(rk_lbl, "badge-role");
        else gtk_widget_add_css_class(rk_lbl, "badge-adm");
        gtk_widget_set_size_request(rk_lbl, widths[0], -1);
        gtk_box_append(GTK_BOX(row), rk_lbl);

        /* Roll */
        char roll_buf[16];
        snprintf(roll_buf, sizeof(roll_buf), "%d", r->roll_number);
        GtkWidget *roll_lbl = gtk_label_new(roll_buf);
        gtk_widget_set_size_request(roll_lbl, widths[1], -1);
        gtk_box_append(GTK_BOX(row), roll_lbl);

        /* Admission No */
        GtkWidget *adm_lbl = gtk_label_new(r->admission_no);
        gtk_widget_set_size_request(adm_lbl, widths[2], -1);
        gtk_label_set_xalign(GTK_LABEL(adm_lbl), 0.0f);
        gtk_box_append(GTK_BOX(row), adm_lbl);

        /* Name */
        GtkWidget *name_lbl = gtk_label_new(r->student_name);
        gtk_widget_add_css_class(name_lbl, "table-cell-bold");
        gtk_label_set_xalign(GTK_LABEL(name_lbl), 0.0f);
        gtk_widget_set_hexpand(name_lbl, TRUE);
        gtk_box_append(GTK_BOX(row), name_lbl);

        /* Total Marks */
        char tm_buf[32];
        snprintf(tm_buf, sizeof(tm_buf), "%.1f / %.0f", r->total_obtained, r->total_max);
        GtkWidget *tm_lbl = gtk_label_new(tm_buf);
        gtk_widget_set_size_request(tm_lbl, widths[4], -1);
        gtk_box_append(GTK_BOX(row), tm_lbl);

        /* Percentage */
        char pct_buf[32];
        snprintf(pct_buf, sizeof(pct_buf), "%.1f%%", r->percentage);
        GtkWidget *pct_lbl = gtk_label_new(pct_buf);
        gtk_widget_set_size_request(pct_lbl, widths[5], -1);
        gtk_box_append(GTK_BOX(row), pct_lbl);

        /* Grade */
        GtkWidget *grd_lbl = gtk_label_new(r->overall_grade);
        gtk_widget_add_css_class(grd_lbl, "badge-adm");
        gtk_widget_set_size_request(grd_lbl, widths[6], -1);
        gtk_box_append(GTK_BOX(row), grd_lbl);

        /* Status */
        GtkWidget *st_lbl = gtk_label_new(r->is_pass ? "PASSED" : "FAILED");
        if (r->is_pass) gtk_widget_add_css_class(st_lbl, "badge-status-active");
        else gtk_widget_add_css_class(st_lbl, "badge-status-inactive");
        gtk_widget_set_size_request(st_lbl, widths[7], -1);
        gtk_box_append(GTK_BOX(row), st_lbl);

        gtk_box_append(GTK_BOX(card), row);
    }
}

static void refresh_view(ExamsViewWidgets *evw)
{
    gboolean can_edit = auth_current_user_can_access("EXAMS", true);
    if (evw->primary_action_btn) {
        gtk_widget_set_visible(evw->primary_action_btn, can_edit && evw->current_tab == TAB_EXAMS);
    }

    /* Clear content container */
    GtkWidget *c = gtk_widget_get_first_child(evw->content_container);
    while (c) {
        GtkWidget *next = gtk_widget_get_next_sibling(c);
        gtk_box_remove(GTK_BOX(evw->content_container), c);
        c = next;
    }

    if (evw->current_tab == TAB_EXAMS) {
        render_exams_tab(evw);
    } else if (evw->current_tab == TAB_MARKS) {
        render_marks_tab(evw);
    } else if (evw->current_tab == TAB_REPORTS) {
        render_reports_tab(evw);
    } else {
        render_ranks_tab(evw);
    }
}

/* ── Modal Dialogs ────────────────────────────────────────────────── */

/* 1. Add Exam Dialog */
typedef struct {
    ExamsViewWidgets *evw;
    GtkWidget *dialog;
    GtkWidget *entry_name;
    GtkWidget *entry_start;
    GtkWidget *entry_end;
    GtkWidget *combo_grading;
    GtkWidget *combo_status;
} ExamDialogData;

static void on_cancel_dialog(GtkButton *btn, gpointer user_data)
{
    (void)btn;
    GtkWidget *w = GTK_WIDGET(user_data);
    school_dialog_close(w);
}

static void on_save_exam_dialog(GtkButton *btn, gpointer user_data)
{
    (void)btn;
    ExamDialogData *d = (ExamDialogData *)user_data;

    const char *name = gtk_editable_get_text(GTK_EDITABLE(d->entry_name));
    const char *start_dt = gtk_editable_get_text(GTK_EDITABLE(d->entry_start));
    const char *end_dt = gtk_editable_get_text(GTK_EDITABLE(d->entry_end));

    if (!name || strlen(name) == 0) return;

    Examination ex;
    memset(&ex, 0, sizeof(Examination));
    strncpy(ex.name, name, sizeof(ex.name) - 1);
    if (start_dt) strncpy(ex.start_date, start_dt, sizeof(ex.start_date) - 1);
    if (end_dt) strncpy(ex.end_date, end_dt, sizeof(ex.end_date) - 1);

    guint sel_g = gtk_drop_down_get_selected(GTK_DROP_DOWN(d->combo_grading));
    strncpy(ex.grading_system, sel_g == 1 ? "GPA" : "PERCENTAGE", sizeof(ex.grading_system) - 1);

    guint sel_s = gtk_drop_down_get_selected(GTK_DROP_DOWN(d->combo_status));
    const char *st_opts[] = { "PLANNED", "ONGOING", "COMPLETED" };
    if (sel_s <= 2) strncpy(ex.status, st_opts[sel_s], sizeof(ex.status) - 1);

    int new_id = 0;
    if (exam_service_create(&ex, &new_id)) {
        show_status_message(d->evw, "Examination created successfully.", true);
    } else {
        show_status_message(d->evw, "Failed to create examination.", false);
    }

    refresh_view(d->evw);
    school_dialog_close(d->dialog);
}

static void show_add_exam_dialog(ExamsViewWidgets *evw)
{
    ExamDialogData *d = g_new0(ExamDialogData, 1);
    d->evw = evw;

    GtkWidget *win = school_dialog_new("Create Examination", 460, 420);
    d->dialog = win;

    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 16);
    gtk_widget_set_margin_start(main_box, 24);
    gtk_widget_set_margin_end(main_box, 24);
    gtk_widget_set_margin_top(main_box, 20);
    gtk_widget_set_margin_bottom(main_box, 20);
    school_dialog_set_child(win, main_box);

    GtkWidget *title_lbl = gtk_label_new("Configure New Examination");
    gtk_widget_add_css_class(title_lbl, "dashboard-card-title");
    gtk_label_set_xalign(GTK_LABEL(title_lbl), 0.0f);
    gtk_box_append(GTK_BOX(main_box), title_lbl);

    GtkWidget *form_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_box_append(GTK_BOX(main_box), form_box);

    /* Name */
    GtkWidget *lbl_name = gtk_label_new("Examination Name (e.g. Mid-Term Exam 2026):");
    gtk_label_set_xalign(GTK_LABEL(lbl_name), 0.0f);
    gtk_box_append(GTK_BOX(form_box), lbl_name);
    d->entry_name = gtk_entry_new();
    gtk_box_append(GTK_BOX(form_box), d->entry_name);

    /* Start Date */
    GtkWidget *lbl_s = gtk_label_new("Start Date (YYYY-MM-DD):");
    gtk_label_set_xalign(GTK_LABEL(lbl_s), 0.0f);
    gtk_box_append(GTK_BOX(form_box), lbl_s);
    d->entry_start = gtk_entry_new();
    gtk_editable_set_text(GTK_EDITABLE(d->entry_start), "2026-09-15");
    gtk_box_append(GTK_BOX(form_box), d->entry_start);

    /* End Date */
    GtkWidget *lbl_e = gtk_label_new("End Date (YYYY-MM-DD):");
    gtk_label_set_xalign(GTK_LABEL(lbl_e), 0.0f);
    gtk_box_append(GTK_BOX(form_box), lbl_e);
    d->entry_end = gtk_entry_new();
    gtk_editable_set_text(GTK_EDITABLE(d->entry_end), "2026-09-25");
    gtk_box_append(GTK_BOX(form_box), d->entry_end);

    /* Grading System */
    GtkWidget *lbl_g = gtk_label_new("Grading Scale:");
    gtk_label_set_xalign(GTK_LABEL(lbl_g), 0.0f);
    gtk_box_append(GTK_BOX(form_box), lbl_g);
    const char *g_opts[] = { "Percentage Scale (A+, A, B, C, D, F)", "GPA Scale", NULL };
    d->combo_grading = gtk_drop_down_new_from_strings(g_opts);
    gtk_box_append(GTK_BOX(form_box), d->combo_grading);

    /* Status */
    GtkWidget *lbl_st = gtk_label_new("Status:");
    gtk_label_set_xalign(GTK_LABEL(lbl_st), 0.0f);
    gtk_box_append(GTK_BOX(form_box), lbl_st);
    const char *st_opts[] = { "PLANNED", "ONGOING", "COMPLETED", NULL };
    d->combo_status = gtk_drop_down_new_from_strings(st_opts);
    gtk_box_append(GTK_BOX(form_box), d->combo_status);

    /* Buttons */
    GtkWidget *btn_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_set_halign(btn_box, GTK_ALIGN_END);
    gtk_widget_set_margin_top(btn_box, 16);

    GtkWidget *cancel_btn = gtk_button_new_with_label("Cancel");
    gtk_widget_add_css_class(cancel_btn, "btn-secondary");
    g_signal_connect(cancel_btn, "clicked", G_CALLBACK(on_cancel_dialog), win);
    gtk_box_append(GTK_BOX(btn_box), cancel_btn);

    GtkWidget *save_btn = gtk_button_new_with_label("Create Examination");
    gtk_widget_add_css_class(save_btn, "btn-primary");
    g_signal_connect(save_btn, "clicked", G_CALLBACK(on_save_exam_dialog), d);
    gtk_box_append(GTK_BOX(btn_box), save_btn);

    gtk_box_append(GTK_BOX(main_box), btn_box);
    g_object_set_data_full(G_OBJECT(win), "d", d, g_free);
    school_dialog_present(win);
}

/* 2. Schedule Subject to Exam Dialog */
typedef struct {
    ExamsViewWidgets *evw;
    int exam_id;
    GtkWidget *dialog;
    GtkWidget *combo_subject;
    GtkWidget *entry_date;
    GtkWidget *entry_max;
    GtkWidget *entry_pass;
    int subject_ids[MAX_SUBJECTS];
    int subject_count;
} SchedDialogData;

static void on_save_sched_dialog(GtkButton *btn, gpointer user_data)
{
    (void)btn;
    SchedDialogData *d = (SchedDialogData *)user_data;

    guint sel_sub = gtk_drop_down_get_selected(GTK_DROP_DOWN(d->combo_subject));
    if ((int)sel_sub >= d->subject_count) return;

    const char *dt_str = gtk_editable_get_text(GTK_EDITABLE(d->entry_date));
    const char *max_str = gtk_editable_get_text(GTK_EDITABLE(d->entry_max));
    const char *pass_str = gtk_editable_get_text(GTK_EDITABLE(d->entry_pass));

    ExamSubject es;
    memset(&es, 0, sizeof(ExamSubject));
    es.exam_id = d->exam_id;
    es.subject_id = d->subject_ids[sel_sub];
    if (dt_str) strncpy(es.exam_date, dt_str, sizeof(es.exam_date) - 1);
    es.max_marks = max_str ? atof(max_str) : 100.0;
    es.passing_marks = pass_str ? atof(pass_str) : 35.0;

    int new_id = 0;
    if (exam_subject_create(&es, &new_id)) {
        show_status_message(d->evw, "Subject scheduled to examination successfully.", true);
    } else {
        show_status_message(d->evw, "Failed to schedule subject.", false);
    }

    refresh_view(d->evw);
    school_dialog_close(d->dialog);
}

static void show_schedule_subject_dialog(ExamsViewWidgets *evw, int exam_id)
{
    SchedDialogData *d = g_new0(SchedDialogData, 1);
    d->evw = evw;
    d->exam_id = exam_id;

    GtkWidget *win = school_dialog_new("Schedule Subject to Exam", 460, 380);
    d->dialog = win;

    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 16);
    gtk_widget_set_margin_start(main_box, 24);
    gtk_widget_set_margin_end(main_box, 24);
    gtk_widget_set_margin_top(main_box, 20);
    gtk_widget_set_margin_bottom(main_box, 20);
    school_dialog_set_child(win, main_box);

    GtkWidget *title_lbl = gtk_label_new("Schedule Subject");
    gtk_widget_add_css_class(title_lbl, "dashboard-card-title");
    gtk_label_set_xalign(GTK_LABEL(title_lbl), 0.0f);
    gtk_box_append(GTK_BOX(main_box), title_lbl);

    GtkWidget *form_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_box_append(GTK_BOX(main_box), form_box);

    /* Subject Selector */
    GtkWidget *lbl_sub = gtk_label_new("Select Subject:");
    gtk_label_set_xalign(GTK_LABEL(lbl_sub), 0.0f);
    gtk_box_append(GTK_BOX(form_box), lbl_sub);

    Subject subs[MAX_SUBJECTS];
    int sub_count = 0;
    subject_service_get_all(subs, MAX_SUBJECTS, &sub_count);
    d->subject_count = sub_count;

    const char *s_names[MAX_SUBJECTS + 1];
    for (int i = 0; i < sub_count; i++) {
        char buf[256];
        snprintf(buf, sizeof(buf), "%s - %s (%s)", subs[i].code, subs[i].name, subs[i].class_name);
        s_names[i] = g_strdup(buf);
        d->subject_ids[i] = subs[i].id;
    }
    s_names[sub_count] = NULL;

    d->combo_subject = gtk_drop_down_new_from_strings(s_names);
    gtk_box_append(GTK_BOX(form_box), d->combo_subject);

    /* Exam Date */
    GtkWidget *lbl_dt = gtk_label_new("Exam Date (YYYY-MM-DD):");
    gtk_label_set_xalign(GTK_LABEL(lbl_dt), 0.0f);
    gtk_box_append(GTK_BOX(form_box), lbl_dt);
    d->entry_date = gtk_entry_new();
    gtk_editable_set_text(GTK_EDITABLE(d->entry_date), "2026-09-18");
    gtk_box_append(GTK_BOX(form_box), d->entry_date);

    /* Max Marks */
    GtkWidget *lbl_max = gtk_label_new("Maximum Marks:");
    gtk_label_set_xalign(GTK_LABEL(lbl_max), 0.0f);
    gtk_box_append(GTK_BOX(form_box), lbl_max);
    d->entry_max = gtk_entry_new();
    gtk_editable_set_text(GTK_EDITABLE(d->entry_max), "100.0");
    gtk_box_append(GTK_BOX(form_box), d->entry_max);

    /* Passing Marks */
    GtkWidget *lbl_pass = gtk_label_new("Passing Marks:");
    gtk_label_set_xalign(GTK_LABEL(lbl_pass), 0.0f);
    gtk_box_append(GTK_BOX(form_box), lbl_pass);
    d->entry_pass = gtk_entry_new();
    gtk_editable_set_text(GTK_EDITABLE(d->entry_pass), "35.0");
    gtk_box_append(GTK_BOX(form_box), d->entry_pass);

    /* Buttons */
    GtkWidget *btn_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_set_halign(btn_box, GTK_ALIGN_END);
    gtk_widget_set_margin_top(btn_box, 16);

    GtkWidget *cancel_btn = gtk_button_new_with_label("Cancel");
    gtk_widget_add_css_class(cancel_btn, "btn-secondary");
    g_signal_connect(cancel_btn, "clicked", G_CALLBACK(on_cancel_dialog), win);
    gtk_box_append(GTK_BOX(btn_box), cancel_btn);

    GtkWidget *save_btn = gtk_button_new_with_label("Schedule Subject");
    gtk_widget_add_css_class(save_btn, "btn-primary");
    g_signal_connect(save_btn, "clicked", G_CALLBACK(on_save_sched_dialog), d);
    gtk_box_append(GTK_BOX(btn_box), save_btn);

    gtk_box_append(GTK_BOX(main_box), btn_box);
    g_object_set_data_full(G_OBJECT(win), "d", d, g_free);
    school_dialog_present(win);
}

/* 3. Printable Marksheet Dialog */
static void on_marksheet_pdf_done(PrintPdfStatus status, const char *path_or_err, gpointer user_data)
{
    GtkWidget *lbl = GTK_WIDGET(user_data);
    if (!lbl || status == PRINT_PDF_CANCELLED) return;

    if (status == PRINT_PDF_OK) {
        char msg[512];
        snprintf(msg, sizeof(msg), "Report Card PDF saved: %s",
                 path_or_err ? path_or_err : "exports/*.pdf");
        gtk_label_set_text(GTK_LABEL(lbl), msg);
        gtk_widget_remove_css_class(lbl, "card-kpi-red");
        gtk_widget_add_css_class(lbl, "card-kpi-green");
        gtk_widget_set_visible(lbl, TRUE);
    } else {
        gtk_label_set_text(GTK_LABEL(lbl), path_or_err ? path_or_err : "Failed to save marksheet PDF.");
        gtk_widget_remove_css_class(lbl, "card-kpi-green");
        gtk_widget_add_css_class(lbl, "card-kpi-red");
        gtk_widget_set_visible(lbl, TRUE);
    }
}

static void on_print_marksheet_btn_clicked(GtkButton *btn, gpointer user_data)
{
    GtkWidget *win = GTK_WIDGET(user_data);
    const StudentReportCard *card = (const StudentReportCard *)g_object_get_data(G_OBJECT(win), "marksheet_data");
    GtkWidget *lbl = GTK_WIDGET(g_object_get_data(G_OBJECT(win), "marksheet_status_lbl"));
    char err_msg[256] = {0};

    PrintPdfStatus st = print_pdf_print_marksheet(GTK_WIDGET(btn), card, err_msg, sizeof(err_msg));
    if (!lbl || st == PRINT_PDF_CANCELLED) return;

    if (st == PRINT_PDF_OK) {
        gtk_label_set_text(GTK_LABEL(lbl), "Report Card sent to printer successfully.");
        gtk_widget_remove_css_class(lbl, "card-kpi-red");
        gtk_widget_add_css_class(lbl, "card-kpi-green");
        gtk_widget_set_visible(lbl, TRUE);
    } else {
        gtk_label_set_text(GTK_LABEL(lbl), err_msg[0] ? err_msg : "Printing failed.");
        gtk_widget_remove_css_class(lbl, "card-kpi-green");
        gtk_widget_add_css_class(lbl, "card-kpi-red");
        gtk_widget_set_visible(lbl, TRUE);
    }
}

static void on_download_marksheet_pdf_clicked(GtkButton *btn, gpointer user_data)
{
    GtkWidget *win = GTK_WIDGET(user_data);
    const StudentReportCard *card = (const StudentReportCard *)g_object_get_data(G_OBJECT(win), "marksheet_data");
    GtkWidget *lbl = GTK_WIDGET(g_object_get_data(G_OBJECT(win), "marksheet_status_lbl"));
    print_pdf_prompt_export_marksheet(GTK_WIDGET(btn), card, on_marksheet_pdf_done, lbl);
}

static void show_marksheet_dialog(const StudentReportCard *card)
{
    if (!card) return;

    SchoolSettings settings = {0};
    settings_service_get(&settings);
    const char *sch_name = settings.school_name[0] ? settings.school_name : "APCOACHING Institution";

    GtkWidget *win = school_dialog_new("Official Student Report Card", 580, 620);
    StudentReportCard *card_copy = g_new(StudentReportCard, 1);
    *card_copy = *card;
    g_object_set_data_full(G_OBJECT(win), "marksheet_data", card_copy, g_free);

    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 16);
    gtk_widget_set_margin_start(box, 28);
    gtk_widget_set_margin_end(box, 28);
    gtk_widget_set_margin_top(box, 20);
    gtk_widget_set_margin_bottom(box, 20);
    school_dialog_set_child(win, box);

    /* Institutional Header */
    GtkWidget *inst_title = gtk_label_new(sch_name);
    gtk_widget_add_css_class(inst_title, "dashboard-card-title");
    gtk_label_set_xalign(GTK_LABEL(inst_title), 0.5f);
    gtk_box_append(GTK_BOX(box), inst_title);

    char sub_buf[256];
    snprintf(sub_buf, sizeof(sub_buf), "%s — Academic Year %s\nSTUDENT ACADEMIC PERFORMANCE REPORT CARD",
             card->exam_name, card->academic_year);
    GtkWidget *inst_sub = gtk_label_new(sub_buf);
    gtk_widget_add_css_class(inst_sub, "dim-label");
    gtk_label_set_justify(GTK_LABEL(inst_sub), GTK_JUSTIFY_CENTER);
    gtk_box_append(GTK_BOX(box), inst_sub);

    GtkWidget *sep1 = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_append(GTK_BOX(box), sep1);

    /* Student Info Box */
    GtkWidget *stu_card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_widget_add_css_class(stu_card, "card");

    char l1[256], l2[256];
    snprintf(l1, sizeof(l1), "Student Name: %s   |   Admission No: %s", card->student_name, card->admission_no);
    snprintf(l2, sizeof(l2), "Class & Section: %s %s   |   Roll Number: %d   |   Class Rank: #%d",
             card->class_name, card->section_name, card->roll_number, card->class_rank);

    GtkWidget *lbl1 = gtk_label_new(l1);
    gtk_label_set_xalign(GTK_LABEL(lbl1), 0.0f);
    gtk_box_append(GTK_BOX(stu_card), lbl1);

    GtkWidget *lbl2 = gtk_label_new(l2);
    gtk_label_set_xalign(GTK_LABEL(lbl2), 0.0f);
    gtk_box_append(GTK_BOX(stu_card), lbl2);

    gtk_box_append(GTK_BOX(box), stu_card);

    /* Subjects Marks Table */
    GtkWidget *marks_card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_add_css_class(marks_card, "card");

    GtkWidget *h_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_add_css_class(h_row, "table-header-row");

    const char *cols[] = { "Subject Code", "Subject Name", "Max", "Pass", "Obtained", "Grade", "Status" };
    int widths[] = { 110, 180, 50, 50, 70, 50, 60 };

    for (int i = 0; i < 7; i++) {
        GtkWidget *th = gtk_label_new(cols[i]);
        gtk_widget_add_css_class(th, "table-th");
        gtk_label_set_xalign(GTK_LABEL(th), 0.0f);
        gtk_widget_set_size_request(th, widths[i], -1);
        if (i == 1) gtk_widget_set_hexpand(th, TRUE);
        gtk_box_append(GTK_BOX(h_row), th);
    }
    gtk_box_append(GTK_BOX(marks_card), h_row);

    for (int i = 0; i < card->score_count; i++) {
        const SubjectScore *sc = &card->scores[i];
        GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
        gtk_widget_add_css_class(row, "table-data-row");

        GtkWidget *c_lbl = gtk_label_new(sc->code);
        gtk_widget_set_size_request(c_lbl, widths[0], -1);
        gtk_label_set_xalign(GTK_LABEL(c_lbl), 0.0f);
        gtk_box_append(GTK_BOX(row), c_lbl);

        GtkWidget *n_lbl = gtk_label_new(sc->subject_name);
        gtk_widget_add_css_class(n_lbl, "table-cell-bold");
        gtk_label_set_xalign(GTK_LABEL(n_lbl), 0.0f);
        gtk_widget_set_hexpand(n_lbl, TRUE);
        gtk_box_append(GTK_BOX(row), n_lbl);

        char mx[16], ps[16], ob[16];
        snprintf(mx, sizeof(mx), "%.0f", sc->max_marks);
        snprintf(ps, sizeof(ps), "%.0f", sc->passing_marks);
        snprintf(ob, sizeof(ob), "%.1f", sc->marks_obtained);

        GtkWidget *mx_lbl = gtk_label_new(mx);
        gtk_widget_set_size_request(mx_lbl, widths[2], -1);
        gtk_box_append(GTK_BOX(row), mx_lbl);

        GtkWidget *ps_lbl = gtk_label_new(ps);
        gtk_widget_set_size_request(ps_lbl, widths[3], -1);
        gtk_box_append(GTK_BOX(row), ps_lbl);

        GtkWidget *ob_lbl = gtk_label_new(ob);
        gtk_widget_add_css_class(ob_lbl, "table-cell-bold");
        gtk_widget_set_size_request(ob_lbl, widths[4], -1);
        gtk_box_append(GTK_BOX(row), ob_lbl);

        GtkWidget *g_lbl = gtk_label_new(sc->grade);
        gtk_widget_add_css_class(g_lbl, "badge-adm");
        gtk_widget_set_size_request(g_lbl, widths[5], -1);
        gtk_box_append(GTK_BOX(row), g_lbl);

        GtkWidget *st_lbl = gtk_label_new(sc->is_pass ? "P" : "F");
        if (sc->is_pass) gtk_widget_add_css_class(st_lbl, "badge-status-active");
        else gtk_widget_add_css_class(st_lbl, "badge-status-inactive");
        gtk_widget_set_size_request(st_lbl, widths[6], -1);
        gtk_box_append(GTK_BOX(row), st_lbl);

        gtk_box_append(GTK_BOX(marks_card), row);
    }

    gtk_box_append(GTK_BOX(box), marks_card);

    /* Summary Card */
    GtkWidget *sum_card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_add_css_class(sum_card, "card");

    char sum_txt[256];
    snprintf(sum_txt, sizeof(sum_txt),
             "Grand Total: %.1f / %.0f   |   Percentage: %.1f%%   |   Overall Grade: %s\n"
             "Result: %s   |   Class Rank: #%d",
             card->total_marks_obtained, card->total_max_marks, card->percentage, card->overall_grade,
             card->is_pass ? "PASSED & PROMOTED" : "NEEDS IMPROVEMENT", card->class_rank);

    GtkWidget *sum_lbl = gtk_label_new(sum_txt);
    gtk_widget_add_css_class(sum_lbl, "dashboard-card-title");
    gtk_label_set_xalign(GTK_LABEL(sum_lbl), 0.0f);
    gtk_box_append(GTK_BOX(sum_card), sum_lbl);

    gtk_box_append(GTK_BOX(box), sum_card);

    /* Signatures */
    GtkWidget *sig_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 24);
    gtk_widget_set_margin_top(sig_box, 16);

    GtkWidget *s1 = gtk_label_new("____________________\nClass Teacher Signature");
    gtk_label_set_justify(GTK_LABEL(s1), GTK_JUSTIFY_CENTER);
    gtk_widget_set_hexpand(s1, TRUE);
    gtk_box_append(GTK_BOX(sig_box), s1);

    GtkWidget *s2 = gtk_label_new("____________________\nPrincipal Signature & Seal");
    gtk_label_set_justify(GTK_LABEL(s2), GTK_JUSTIFY_CENTER);
    gtk_widget_set_hexpand(s2, TRUE);
    gtk_box_append(GTK_BOX(sig_box), s2);

    gtk_box_append(GTK_BOX(box), sig_box);

    /* Inline Print/PDF Status Feedback */
    GtkWidget *lbl_status = gtk_label_new("");
    gtk_widget_add_css_class(lbl_status, "card");
    gtk_label_set_wrap(GTK_LABEL(lbl_status), TRUE);
    gtk_widget_set_visible(lbl_status, FALSE);
    g_object_set_data(G_OBJECT(win), "marksheet_status_lbl", lbl_status);
    gtk_box_append(GTK_BOX(box), lbl_status);

    /* Action Buttons: Print, Download PDF, Close */
    GtkWidget *btn_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_set_halign(btn_box, GTK_ALIGN_END);
    gtk_widget_set_margin_top(btn_box, 12);

    GtkWidget *print_btn = gtk_button_new_with_label("Print Report Card");
    gtk_widget_add_css_class(print_btn, "btn-secondary");
    g_signal_connect(print_btn, "clicked", G_CALLBACK(on_print_marksheet_btn_clicked), win);
    gtk_box_append(GTK_BOX(btn_box), print_btn);

    GtkWidget *pdf_btn = gtk_button_new_with_label("Download PDF");
    gtk_widget_add_css_class(pdf_btn, "btn-secondary");
    g_signal_connect(pdf_btn, "clicked", G_CALLBACK(on_download_marksheet_pdf_clicked), win);
    gtk_box_append(GTK_BOX(btn_box), pdf_btn);

    GtkWidget *close_btn = gtk_button_new_with_label("Close Marksheet");
    gtk_widget_add_css_class(close_btn, "btn-primary");
    g_signal_connect(close_btn, "clicked", G_CALLBACK(on_cancel_dialog), win);
    gtk_box_append(GTK_BOX(btn_box), close_btn);

    gtk_box_append(GTK_BOX(box), btn_box);
    school_dialog_present(win);
}

/* ── Main View Constructor ────────────────────────────────────────── */

GtkWidget *school_exams_view_new(void)
{
    ExamsViewWidgets *evw = g_new0(ExamsViewWidgets, 1);
    evw->current_tab = TAB_EXAMS;

    GtkWidget *scrolled = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
                                  GTK_POLICY_AUTOMATIC,
                                  GTK_POLICY_AUTOMATIC);

    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 20);
    gtk_widget_add_css_class(box, "page-container");
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled), box);
    evw->root_box = box;
    g_object_set_data_full(G_OBJECT(scrolled), "evw", evw, g_free);

    /* Header */
    GtkWidget *top_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 16);
    gtk_widget_set_valign(top_row, GTK_ALIGN_CENTER);

    GtkWidget *title_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    GtkWidget *title = gtk_label_new("Examinations & Results");
    gtk_widget_add_css_class(title, "page-title");
    gtk_label_set_xalign(GTK_LABEL(title), 0.0f);
    gtk_box_append(GTK_BOX(title_box), title);

    GtkWidget *sub = gtk_label_new("Schedule exams, enter subject marks, generate student report cards, and view class ranks");
    gtk_widget_add_css_class(sub, "page-subtitle");
    gtk_label_set_xalign(GTK_LABEL(sub), 0.0f);
    gtk_box_append(GTK_BOX(title_box), sub);

    gtk_widget_set_hexpand(title_box, TRUE);
    gtk_box_append(GTK_BOX(top_row), title_box);

    /* Primary Action Button */
    evw->primary_action_btn = gtk_button_new_with_label("+ Create Exam");
    gtk_widget_add_css_class(evw->primary_action_btn, "btn-primary");
    g_signal_connect(evw->primary_action_btn, "clicked", G_CALLBACK(on_primary_action_clicked), evw);
    gtk_box_append(GTK_BOX(top_row), evw->primary_action_btn);

    gtk_box_append(GTK_BOX(box), top_row);

    /* ── Status Banner ───────────────────────────────────────────── */
    evw->status_banner = gtk_label_new("");
    gtk_widget_add_css_class(evw->status_banner, "card");
    gtk_widget_set_visible(evw->status_banner, FALSE);
    gtk_box_append(GTK_BOX(box), evw->status_banner);

    /* Tabs */
    GtkWidget *tab_bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);

    evw->tab_exams_btn = gtk_button_new_with_label("Exams & Schedules");
    gtk_widget_add_css_class(evw->tab_exams_btn, "tab-btn");
    gtk_widget_add_css_class(evw->tab_exams_btn, "tab-btn-active");
    g_signal_connect(evw->tab_exams_btn, "clicked", G_CALLBACK(on_tab_exams_clicked), evw);
    gtk_box_append(GTK_BOX(tab_bar), evw->tab_exams_btn);

    evw->tab_marks_btn = gtk_button_new_with_label("Enter Marks");
    gtk_widget_add_css_class(evw->tab_marks_btn, "tab-btn");
    g_signal_connect(evw->tab_marks_btn, "clicked", G_CALLBACK(on_tab_marks_clicked), evw);
    gtk_box_append(GTK_BOX(tab_bar), evw->tab_marks_btn);

    evw->tab_reports_btn = gtk_button_new_with_label("Report Cards & Marksheets");
    gtk_widget_add_css_class(evw->tab_reports_btn, "tab-btn");
    g_signal_connect(evw->tab_reports_btn, "clicked", G_CALLBACK(on_tab_reports_clicked), evw);
    gtk_box_append(GTK_BOX(tab_bar), evw->tab_reports_btn);

    evw->tab_ranks_btn = gtk_button_new_with_label("Class Merit & Ranks");
    gtk_widget_add_css_class(evw->tab_ranks_btn, "tab-btn");
    g_signal_connect(evw->tab_ranks_btn, "clicked", G_CALLBACK(on_tab_ranks_clicked), evw);
    gtk_box_append(GTK_BOX(tab_bar), evw->tab_ranks_btn);

    gtk_box_append(GTK_BOX(box), tab_bar);

    /* Content Container */
    evw->content_container = gtk_box_new(GTK_ORIENTATION_VERTICAL, 16);
    gtk_box_append(GTK_BOX(box), evw->content_container);

    refresh_view(evw);

    return scrolled;
}

void school_exams_view_reload(GtkWidget *view)
{
    if (!view) return;
    ExamsViewWidgets *evw = (ExamsViewWidgets *)g_object_get_data(G_OBJECT(view), "evw");
    if (evw) refresh_view(evw);
}
