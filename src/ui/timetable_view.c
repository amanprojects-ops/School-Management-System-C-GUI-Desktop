/**
 * @file timetable_view.c
 * @brief Timetable GUI module implementation: weekly schedule grid and conflict-free period assignment.
 */

#include "ui/timetable_view.h"
#include "timetable.h"
#include "classes.h"
#include "teacher.h"
#include "auth/auth.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum {
    TAB_CLASS_SCHEDULE = 0,
    TAB_TEACHER_SCHEDULE
} TimetableTab;

typedef struct {
    GtkWidget *root_box;
    GtkWidget *tab_class_btn;
    GtkWidget *tab_teacher_btn;
    GtkWidget *content_container;
    GtkWidget *add_slot_btn;

    TimetableTab current_tab;

    /* Class timetable filter state */
    GtkWidget *combo_class;
    GtkWidget *combo_section;
    GtkWidget *combo_day;

    SchoolClass classes[MAX_CLASSES];
    int class_count;
    ClassSection sections[MAX_SECTIONS];
    int section_count;

    int selected_class_id;
    int selected_section_id;
    char selected_day[16];

    /* Teacher timetable filter state */
    GtkWidget *combo_teacher;
    Teacher teachers[TEACHER_MAX_LIST];
    int teacher_count;
    int selected_teacher_id;
    char teacher_selected_day[16];
} TimetableWidgets;

static void refresh_view(TimetableWidgets *tw);
static void update_sections_dropdown(TimetableWidgets *tw);
static void show_add_edit_slot_dialog(TimetableWidgets *tw, int slot_id);
static void show_delete_slot_dialog(TimetableWidgets *tw, int slot_id);

/* ── Tab Switch Handlers ─────────────────────────────────────────── */

static void on_tab_class_clicked(GtkButton *btn, gpointer user_data)
{
    (void)btn;
    TimetableWidgets *tw = (TimetableWidgets *)user_data;
    tw->current_tab = TAB_CLASS_SCHEDULE;
    gtk_widget_add_css_class(tw->tab_class_btn, "tab-btn-active");
    gtk_widget_remove_css_class(tw->tab_teacher_btn, "tab-btn-active");
    refresh_view(tw);
}

static void on_tab_teacher_clicked(GtkButton *btn, gpointer user_data)
{
    (void)btn;
    TimetableWidgets *tw = (TimetableWidgets *)user_data;
    tw->current_tab = TAB_TEACHER_SCHEDULE;
    gtk_widget_remove_css_class(tw->tab_class_btn, "tab-btn-active");
    gtk_widget_add_css_class(tw->tab_teacher_btn, "tab-btn-active");
    refresh_view(tw);
}

static void on_add_slot_clicked(GtkButton *btn, gpointer user_data)
{
    (void)btn;
    TimetableWidgets *tw = (TimetableWidgets *)user_data;
    show_add_edit_slot_dialog(tw, 0);
}

/* ── Filter Callbacks ────────────────────────────────────────────── */

static void on_class_changed(GtkDropDown *dropdown, GParamSpec *pspec, gpointer user_data)
{
    (void)pspec;
    TimetableWidgets *tw = (TimetableWidgets *)user_data;
    guint sel = gtk_drop_down_get_selected(dropdown);
    if ((int)sel < tw->class_count) {
        tw->selected_class_id = tw->classes[sel].id;
    } else {
        tw->selected_class_id = 0;
    }
    update_sections_dropdown(tw);
    refresh_view(tw);
}

static void on_section_changed(GtkDropDown *dropdown, GParamSpec *pspec, gpointer user_data)
{
    (void)pspec;
    TimetableWidgets *tw = (TimetableWidgets *)user_data;
    guint sel = gtk_drop_down_get_selected(dropdown);
    if (sel == 0) {
        tw->selected_section_id = 0;
    } else if ((int)sel - 1 < tw->section_count) {
        tw->selected_section_id = tw->sections[sel - 1].id;
    }
    refresh_view(tw);
}

static void on_day_changed(GtkDropDown *dropdown, GParamSpec *pspec, gpointer user_data)
{
    (void)pspec;
    TimetableWidgets *tw = (TimetableWidgets *)user_data;
    guint sel = gtk_drop_down_get_selected(dropdown);
    const char *days[] = { "", DAY_MONDAY, DAY_TUESDAY, DAY_WEDNESDAY, DAY_THURSDAY, DAY_FRIDAY, DAY_SATURDAY };
    if (sel <= 6) strncpy(tw->selected_day, days[sel], sizeof(tw->selected_day) - 1);
    else tw->selected_day[0] = '\0';
    refresh_view(tw);
}

static void on_teacher_changed(GtkDropDown *dropdown, GParamSpec *pspec, gpointer user_data)
{
    (void)pspec;
    TimetableWidgets *tw = (TimetableWidgets *)user_data;
    guint sel = gtk_drop_down_get_selected(dropdown);
    if ((int)sel < tw->teacher_count) {
        tw->selected_teacher_id = tw->teachers[sel].id;
    } else {
        tw->selected_teacher_id = 0;
    }
    refresh_view(tw);
}

static void update_sections_dropdown(TimetableWidgets *tw)
{
    if (tw->selected_class_id <= 0) {
        const char *none[] = { "All Sections", NULL };
        GtkWidget *new_dd = gtk_drop_down_new_from_strings(none);
        gtk_box_remove(GTK_BOX(gtk_widget_get_parent(tw->combo_section)), tw->combo_section);
        tw->combo_section = new_dd;
        return;
    }

    tw->section_count = 0;
    section_service_get_by_class(tw->selected_class_id, tw->sections, MAX_SECTIONS, &tw->section_count);

    const char *sec_names[MAX_SECTIONS + 2];
    sec_names[0] = "All Sections";
    for (int i = 0; i < tw->section_count; i++) {
        sec_names[i + 1] = tw->sections[i].name;
    }
    sec_names[tw->section_count + 1] = NULL;

    GtkWidget *parent = gtk_widget_get_parent(tw->combo_section);
    GtkWidget *new_dd = gtk_drop_down_new_from_strings(sec_names);
    g_signal_connect(new_dd, "notify::selected", G_CALLBACK(on_section_changed), tw);

    gtk_box_remove(GTK_BOX(parent), tw->combo_section);
    gtk_box_append(GTK_BOX(parent), new_dd);
    tw->combo_section = new_dd;
    tw->selected_section_id = 0;
}

/* ── 1. View: Class Timetable ─────────────────────────────────────── */

static void on_edit_slot_btn(GtkButton *btn, gpointer user_data)
{
    TimetableWidgets *tw = (TimetableWidgets *)g_object_get_data(G_OBJECT(btn), "tw");
    int id = GPOINTER_TO_INT(user_data);
    show_add_edit_slot_dialog(tw, id);
}

static void on_delete_slot_btn(GtkButton *btn, gpointer user_data)
{
    TimetableWidgets *tw = (TimetableWidgets *)g_object_get_data(G_OBJECT(btn), "tw");
    int id = GPOINTER_TO_INT(user_data);
    show_delete_slot_dialog(tw, id);
}

static void render_class_schedule_tab(TimetableWidgets *tw)
{
    /* Controls Bar */
    GtkWidget *ctrl_card = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 16);
    gtk_widget_add_css_class(ctrl_card, "card");
    gtk_widget_set_valign(ctrl_card, GTK_ALIGN_CENTER);

    /* Class */
    GtkWidget *b_c = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    GtkWidget *lbl_c = gtk_label_new("Class:");
    gtk_label_set_xalign(GTK_LABEL(lbl_c), 0.0f);
    gtk_box_append(GTK_BOX(b_c), lbl_c);

    tw->class_count = 0;
    class_service_get_all(tw->classes, MAX_CLASSES, &tw->class_count);

    const char *c_names[MAX_CLASSES + 1];
    for (int i = 0; i < tw->class_count; i++) {
        c_names[i] = tw->classes[i].name;
    }
    c_names[tw->class_count] = NULL;

    tw->combo_class = gtk_drop_down_new_from_strings(c_names);
    if (tw->class_count > 0 && tw->selected_class_id == 0) {
        tw->selected_class_id = tw->classes[0].id;
        gtk_drop_down_set_selected(GTK_DROP_DOWN(tw->combo_class), 0);
    }
    g_signal_connect(tw->combo_class, "notify::selected", G_CALLBACK(on_class_changed), tw);
    gtk_box_append(GTK_BOX(b_c), tw->combo_class);
    gtk_box_append(GTK_BOX(ctrl_card), b_c);

    /* Section */
    GtkWidget *b_s = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    GtkWidget *lbl_s = gtk_label_new("Section:");
    gtk_label_set_xalign(GTK_LABEL(lbl_s), 0.0f);
    gtk_box_append(GTK_BOX(b_s), lbl_s);

    const char *init_sec[] = { "All Sections", NULL };
    tw->combo_section = gtk_drop_down_new_from_strings(init_sec);
    gtk_box_append(GTK_BOX(b_s), tw->combo_section);
    gtk_box_append(GTK_BOX(ctrl_card), b_s);

    if (tw->selected_class_id > 0) {
        update_sections_dropdown(tw);
    }

    /* Day of Week */
    GtkWidget *b_d = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    GtkWidget *lbl_d = gtk_label_new("Day of Week:");
    gtk_label_set_xalign(GTK_LABEL(lbl_d), 0.0f);
    gtk_box_append(GTK_BOX(b_d), lbl_d);

    const char *d_names[] = { "All Days (Monday - Saturday)", "Monday", "Tuesday",
                              "Wednesday", "Thursday", "Friday", "Saturday", NULL };
    tw->combo_day = gtk_drop_down_new_from_strings(d_names);
    g_signal_connect(tw->combo_day, "notify::selected", G_CALLBACK(on_day_changed), tw);
    gtk_box_append(GTK_BOX(b_d), tw->combo_day);
    gtk_box_append(GTK_BOX(ctrl_card), b_d);

    gtk_box_append(GTK_BOX(tw->content_container), ctrl_card);

    if (tw->selected_class_id <= 0) {
        GtkWidget *msg = gtk_label_new("Please select a class to view its timetable.");
        gtk_widget_add_css_class(msg, "dim-label");
        gtk_box_append(GTK_BOX(tw->content_container), msg);
        return;
    }

    /* Fetch scheduled slots */
    TimetableSlot slots[MAX_TIMETABLE_SLOTS];
    int slot_count = 0;
    timetable_service_get_by_class(tw->selected_class_id, tw->selected_section_id,
                                   tw->selected_day, slots, MAX_TIMETABLE_SLOTS, &slot_count);

    const char *all_days[] = { DAY_MONDAY, DAY_TUESDAY, DAY_WEDNESDAY, DAY_THURSDAY, DAY_FRIDAY, DAY_SATURDAY };
    int num_days = 6;
    if (strlen(tw->selected_day) > 0) {
        all_days[0] = tw->selected_day;
        num_days = 1;
    }

    for (int d = 0; d < num_days; d++) {
        const char *cur_day = all_days[d];

        GtkWidget *day_card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
        gtk_widget_add_css_class(day_card, "card");
        gtk_widget_set_margin_bottom(day_card, 16);

        /* Day Header */
        GtkWidget *dh = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
        GtkWidget *day_title = gtk_label_new(cur_day);
        gtk_widget_add_css_class(day_title, "dashboard-card-title");
        gtk_label_set_xalign(GTK_LABEL(day_title), 0.0f);
        gtk_widget_set_hexpand(day_title, TRUE);
        gtk_box_append(GTK_BOX(dh), day_title);
        gtk_box_append(GTK_BOX(day_card), dh);

        GtkWidget *sep = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
        gtk_box_append(GTK_BOX(day_card), sep);

        /* Slots matching this day */
        int day_slots = 0;
        GtkWidget *slots_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);

        for (int s = 0; s < slot_count; s++) {
            if (strcmp(slots[s].day_of_week, cur_day) == 0) {
                const TimetableSlot *slot = &slots[s];
                GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
                gtk_widget_add_css_class(row, "table-data-row");

                /* Period Badge */
                char p_buf[32];
                snprintf(p_buf, sizeof(p_buf), "Period %d", slot->period_number);
                GtkWidget *p_lbl = gtk_label_new(p_buf);
                gtk_widget_add_css_class(p_lbl, "badge-adm");
                gtk_widget_set_size_request(p_lbl, 90, -1);
                gtk_box_append(GTK_BOX(row), p_lbl);

                /* Timing */
                char tm_buf[64];
                snprintf(tm_buf, sizeof(tm_buf), "%s - %s", slot->start_time, slot->end_time);
                GtkWidget *tm_lbl = gtk_label_new(tm_buf);
                gtk_widget_set_size_request(tm_lbl, 110, -1);
                gtk_box_append(GTK_BOX(row), tm_lbl);

                /* Subject */
                char sub_buf[256];
                snprintf(sub_buf, sizeof(sub_buf), "%s (%s)", slot->subject_name, slot->subject_code);
                GtkWidget *sub_lbl = gtk_label_new(sub_buf);
                gtk_widget_add_css_class(sub_lbl, "table-cell-bold");
                gtk_label_set_xalign(GTK_LABEL(sub_lbl), 0.0f);
                gtk_widget_set_hexpand(sub_lbl, TRUE);
                gtk_box_append(GTK_BOX(row), sub_lbl);

                /* Teacher */
                GtkWidget *tch_lbl = gtk_label_new(slot->teacher_name);
                gtk_widget_set_size_request(tch_lbl, 160, -1);
                gtk_label_set_xalign(GTK_LABEL(tch_lbl), 0.0f);
                gtk_box_append(GTK_BOX(row), tch_lbl);

                /* Room */
                GtkWidget *rm_lbl = gtk_label_new(strlen(slot->room_number) > 0 ? slot->room_number : "Standard");
                gtk_widget_add_css_class(rm_lbl, "badge-role");
                gtk_widget_set_size_request(rm_lbl, 100, -1);
                gtk_box_append(GTK_BOX(row), rm_lbl);

                /* Actions */
                GtkWidget *act_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);

                GtkWidget *edit_btn = gtk_button_new_with_label("Edit");
                gtk_widget_add_css_class(edit_btn, "btn-secondary");
                g_object_set_data(G_OBJECT(edit_btn), "tw", tw);
                g_signal_connect(edit_btn, "clicked", G_CALLBACK(on_edit_slot_btn), GINT_TO_POINTER(slot->id));
                gtk_box_append(GTK_BOX(act_box), edit_btn);

                GtkWidget *del_btn = gtk_button_new_with_label("Delete");
                gtk_widget_add_css_class(del_btn, "btn-danger");
                g_object_set_data(G_OBJECT(del_btn), "tw", tw);
                g_signal_connect(del_btn, "clicked", G_CALLBACK(on_delete_slot_btn), GINT_TO_POINTER(slot->id));
                gtk_box_append(GTK_BOX(act_box), del_btn);

                gtk_box_append(GTK_BOX(row), act_box);

                gtk_box_append(GTK_BOX(slots_box), row);
                day_slots++;
            }
        }

        if (day_slots == 0) {
            GtkWidget *empty_lbl = gtk_label_new("No periods scheduled for this day.");
            gtk_widget_add_css_class(empty_lbl, "dim-label");
            gtk_label_set_xalign(GTK_LABEL(empty_lbl), 0.0f);
            gtk_box_append(GTK_BOX(day_card), empty_lbl);
        } else {
            gtk_box_append(GTK_BOX(day_card), slots_box);
        }

        gtk_box_append(GTK_BOX(tw->content_container), day_card);
    }
}

/* ── 2. View: Teacher Schedule ────────────────────────────────────── */

static void render_teacher_schedule_tab(TimetableWidgets *tw)
{
    GtkWidget *bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 16);
    gtk_widget_add_css_class(bar, "card");
    gtk_widget_set_valign(bar, GTK_ALIGN_CENTER);

    GtkWidget *b_t = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    GtkWidget *lbl_t = gtk_label_new("Select Faculty / Teacher:");
    gtk_label_set_xalign(GTK_LABEL(lbl_t), 0.0f);
    gtk_box_append(GTK_BOX(b_t), lbl_t);

    tw->teacher_count = 0;
    teacher_service_get_all(NULL, "ACTIVE", tw->teachers, TEACHER_MAX_LIST, &tw->teacher_count);

    const char *t_names[TEACHER_MAX_LIST + 1];
    for (int i = 0; i < tw->teacher_count; i++) {
        char buf[384];
        snprintf(buf, sizeof(buf), "%s (%s)", tw->teachers[i].full_name, tw->teachers[i].subject);
        t_names[i] = g_strdup(buf);
    }
    t_names[tw->teacher_count] = NULL;

    tw->combo_teacher = gtk_drop_down_new_from_strings(t_names);
    if (tw->teacher_count > 0 && tw->selected_teacher_id == 0) {
        tw->selected_teacher_id = tw->teachers[0].id;
        gtk_drop_down_set_selected(GTK_DROP_DOWN(tw->combo_teacher), 0);
    }
    g_signal_connect(tw->combo_teacher, "notify::selected", G_CALLBACK(on_teacher_changed), tw);
    gtk_box_append(GTK_BOX(b_t), tw->combo_teacher);
    gtk_box_append(GTK_BOX(bar), b_t);

    gtk_box_append(GTK_BOX(tw->content_container), bar);

    if (tw->selected_teacher_id <= 0) {
        GtkWidget *msg = gtk_label_new("Please select a teacher to view their teaching schedule.");
        gtk_widget_add_css_class(msg, "dim-label");
        gtk_box_append(GTK_BOX(tw->content_container), msg);
        return;
    }

    TimetableSlot slots[MAX_TIMETABLE_SLOTS];
    int slot_count = 0;
    timetable_service_get_by_teacher(tw->selected_teacher_id, NULL, slots, MAX_TIMETABLE_SLOTS, &slot_count);

    GtkWidget *card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_add_css_class(card, "card");
    gtk_box_append(GTK_BOX(tw->content_container), card);

    GtkWidget *h_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_add_css_class(h_row, "table-header-row");

    const char *cols[] = { "Day", "Period", "Time", "Class & Section", "Subject", "Room" };
    int widths[] = { 110, 80, 110, 150, 200, 100 };

    for (int i = 0; i < 6; i++) {
        GtkWidget *th = gtk_label_new(cols[i]);
        gtk_widget_add_css_class(th, "table-th");
        gtk_label_set_xalign(GTK_LABEL(th), 0.0f);
        gtk_widget_set_size_request(th, widths[i], -1);
        if (i == 4) gtk_widget_set_hexpand(th, TRUE);
        gtk_box_append(GTK_BOX(h_row), th);
    }
    gtk_box_append(GTK_BOX(card), h_row);

    if (slot_count == 0) {
        GtkWidget *empty_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
        gtk_widget_set_margin_top(empty_box, 30);
        GtkWidget *lbl = gtk_label_new("No periods scheduled for this faculty member.");
        gtk_widget_add_css_class(lbl, "dim-label");
        gtk_box_append(GTK_BOX(empty_box), lbl);
        gtk_box_append(GTK_BOX(card), empty_box);
        return;
    }

    for (int i = 0; i < slot_count; i++) {
        const TimetableSlot *s = &slots[i];
        GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
        gtk_widget_add_css_class(row, "table-data-row");

        /* Day */
        GtkWidget *day_lbl = gtk_label_new(s->day_of_week);
        gtk_widget_add_css_class(day_lbl, "badge-adm");
        gtk_widget_set_size_request(day_lbl, widths[0], -1);
        gtk_box_append(GTK_BOX(row), day_lbl);

        /* Period */
        char p_buf[16];
        snprintf(p_buf, sizeof(p_buf), "P %d", s->period_number);
        GtkWidget *p_lbl = gtk_label_new(p_buf);
        gtk_widget_set_size_request(p_lbl, widths[1], -1);
        gtk_box_append(GTK_BOX(row), p_lbl);

        /* Time */
        char tm_buf[32];
        snprintf(tm_buf, sizeof(tm_buf), "%s-%s", s->start_time, s->end_time);
        GtkWidget *tm_lbl = gtk_label_new(tm_buf);
        gtk_widget_set_size_request(tm_lbl, widths[2], -1);
        gtk_box_append(GTK_BOX(row), tm_lbl);

        /* Class & Section */
        char cls_buf[128];
        snprintf(cls_buf, sizeof(cls_buf), "%s (%s)", s->class_name, s->section_name);
        GtkWidget *cls_lbl = gtk_label_new(cls_buf);
        gtk_widget_set_size_request(cls_lbl, widths[3], -1);
        gtk_label_set_xalign(GTK_LABEL(cls_lbl), 0.0f);
        gtk_box_append(GTK_BOX(row), cls_lbl);

        /* Subject */
        GtkWidget *sub_lbl = gtk_label_new(s->subject_name);
        gtk_widget_add_css_class(sub_lbl, "table-cell-bold");
        gtk_label_set_xalign(GTK_LABEL(sub_lbl), 0.0f);
        gtk_widget_set_hexpand(sub_lbl, TRUE);
        gtk_box_append(GTK_BOX(row), sub_lbl);

        /* Room */
        GtkWidget *rm_lbl = gtk_label_new(strlen(s->room_number) > 0 ? s->room_number : "Standard");
        gtk_widget_add_css_class(rm_lbl, "badge-role");
        gtk_widget_set_size_request(rm_lbl, widths[5], -1);
        gtk_box_append(GTK_BOX(row), rm_lbl);

        gtk_box_append(GTK_BOX(card), row);
    }
}

static void refresh_view(TimetableWidgets *tw)
{
    /* Clear content container */
    GtkWidget *c = gtk_widget_get_first_child(tw->content_container);
    while (c) {
        GtkWidget *next = gtk_widget_get_next_sibling(c);
        gtk_box_remove(GTK_BOX(tw->content_container), c);
        c = next;
    }

    if (tw->current_tab == TAB_CLASS_SCHEDULE) {
        render_class_schedule_tab(tw);
    } else {
        render_teacher_schedule_tab(tw);
    }
}

/* ── Modal Dialogs ────────────────────────────────────────────────── */

/* 1. Add / Edit Slot Modal */
typedef struct {
    TimetableWidgets *tw;
    int slot_id;
    GtkWidget *dialog;
    GtkWidget *combo_class;
    GtkWidget *combo_section;
    GtkWidget *combo_day;
    GtkWidget *combo_period;
    GtkWidget *combo_subject;
    GtkWidget *combo_teacher;
    GtkWidget *entry_room;
    GtkWidget *lbl_conflict_error;

    int class_ids[MAX_CLASSES];
    int class_count;
    int section_ids[MAX_SECTIONS];
    int section_count;
    int subject_ids[MAX_SUBJECTS];
    int subject_count;
    int teacher_ids[TEACHER_MAX_LIST];
    int teacher_count;
} SlotDialogData;

static void on_cancel_dialog(GtkButton *btn, gpointer user_data)
{
    (void)btn;
    GtkWidget *w = GTK_WIDGET(user_data);
    gtk_window_destroy(GTK_WINDOW(w));
}

static void on_save_slot_dialog(GtkButton *btn, gpointer user_data)
{
    (void)btn;
    SlotDialogData *d = (SlotDialogData *)user_data;

    guint sel_cls = gtk_drop_down_get_selected(GTK_DROP_DOWN(d->combo_class));
    if ((int)sel_cls >= d->class_count) return;

    guint sel_sec = gtk_drop_down_get_selected(GTK_DROP_DOWN(d->combo_section));
    if ((int)sel_sec >= d->section_count) return;

    guint sel_sub = gtk_drop_down_get_selected(GTK_DROP_DOWN(d->combo_subject));
    if ((int)sel_sub >= d->subject_count) return;

    guint sel_tch = gtk_drop_down_get_selected(GTK_DROP_DOWN(d->combo_teacher));
    if ((int)sel_tch >= d->teacher_count) return;

    const char *days[] = { DAY_MONDAY, DAY_TUESDAY, DAY_WEDNESDAY, DAY_THURSDAY, DAY_FRIDAY, DAY_SATURDAY };
    guint sel_day = gtk_drop_down_get_selected(GTK_DROP_DOWN(d->combo_day));
    if (sel_day > 5) sel_day = 0;

    int period = (int)gtk_drop_down_get_selected(GTK_DROP_DOWN(d->combo_period)) + 1;
    const char *room = gtk_editable_get_text(GTK_EDITABLE(d->entry_room));

    TimetableSlot slot;
    memset(&slot, 0, sizeof(TimetableSlot));
    slot.id = d->slot_id;
    slot.class_id = d->class_ids[sel_cls];
    slot.section_id = d->section_ids[sel_sec];
    strncpy(slot.day_of_week, days[sel_day], sizeof(slot.day_of_week) - 1);
    slot.period_number = period;
    slot.subject_id = d->subject_ids[sel_sub];
    slot.teacher_id = d->teacher_ids[sel_tch];
    if (room) strncpy(slot.room_number, room, sizeof(slot.room_number) - 1);

    ConflictResult conf;
    int new_id = 0;
    if (!timetable_service_save_slot(&slot, &new_id, &conf)) {
        if (conf.has_conflict) {
            gtk_label_set_text(GTK_LABEL(d->lbl_conflict_error), conf.message);
            gtk_widget_set_visible(d->lbl_conflict_error, TRUE);
            return;
        }
    }

    refresh_view(d->tw);
    gtk_window_destroy(GTK_WINDOW(d->dialog));
    g_free(d);
}

static void show_add_edit_slot_dialog(TimetableWidgets *tw, int slot_id)
{
    SlotDialogData *d = g_new0(SlotDialogData, 1);
    d->tw = tw;
    d->slot_id = slot_id;

    TimetableSlot existing;
    bool is_edit = (slot_id > 0 && timetable_service_get_by_id(slot_id, &existing));

    GtkWidget *win = gtk_window_new();
    d->dialog = win;
    gtk_window_set_title(GTK_WINDOW(win), is_edit ? "Edit Timetable Period Slot" : "Assign Timetable Period Slot");
    gtk_window_set_modal(GTK_WINDOW(win), TRUE);
    gtk_window_set_default_size(GTK_WINDOW(win), 480, 520);

    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 16);
    gtk_widget_set_margin_start(main_box, 24);
    gtk_widget_set_margin_end(main_box, 24);
    gtk_widget_set_margin_top(main_box, 24);
    gtk_widget_set_margin_bottom(main_box, 24);
    gtk_window_set_child(GTK_WINDOW(win), main_box);

    GtkWidget *title_lbl = gtk_label_new(is_edit ? "Edit Scheduled Slot" : "Assign Period Slot");
    gtk_widget_add_css_class(title_lbl, "dashboard-card-title");
    gtk_label_set_xalign(GTK_LABEL(title_lbl), 0.0f);
    gtk_box_append(GTK_BOX(main_box), title_lbl);

    /* Conflict Error Alert Label */
    d->lbl_conflict_error = gtk_label_new("");
    gtk_widget_add_css_class(d->lbl_conflict_error, "badge-status-inactive");
    gtk_widget_set_visible(d->lbl_conflict_error, FALSE);
    gtk_label_set_wrap(GTK_LABEL(d->lbl_conflict_error), TRUE);
    gtk_box_append(GTK_BOX(main_box), d->lbl_conflict_error);

    GtkWidget *form_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_box_append(GTK_BOX(main_box), form_box);

    /* Class & Section row */
    GtkWidget *r_cs = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 16);

    GtkWidget *b_c = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_hexpand(b_c, TRUE);
    GtkWidget *lbl_c = gtk_label_new("Class:");
    gtk_label_set_xalign(GTK_LABEL(lbl_c), 0.0f);
    gtk_box_append(GTK_BOX(b_c), lbl_c);

    SchoolClass classes[MAX_CLASSES];
    int count = 0;
    class_service_get_all(classes, MAX_CLASSES, &count);
    d->class_count = count;

    const char *c_names[MAX_CLASSES + 1];
    int sel_c_idx = 0;
    for (int i = 0; i < count; i++) {
        c_names[i] = classes[i].name;
        d->class_ids[i] = classes[i].id;
        if (is_edit && classes[i].id == existing.class_id) sel_c_idx = i;
        else if (!is_edit && classes[i].id == tw->selected_class_id) sel_c_idx = i;
    }
    c_names[count] = NULL;
    d->combo_class = gtk_drop_down_new_from_strings(c_names);
    gtk_drop_down_set_selected(GTK_DROP_DOWN(d->combo_class), sel_c_idx);
    gtk_box_append(GTK_BOX(b_c), d->combo_class);
    gtk_box_append(GTK_BOX(r_cs), b_c);

    /* Section */
    GtkWidget *b_s = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_hexpand(b_s, TRUE);
    GtkWidget *lbl_s = gtk_label_new("Section:");
    gtk_label_set_xalign(GTK_LABEL(lbl_s), 0.0f);
    gtk_box_append(GTK_BOX(b_s), lbl_s);

    ClassSection sec_list[MAX_SECTIONS];
    int s_count = 0;
    int target_class = is_edit ? existing.class_id : (count > 0 ? d->class_ids[sel_c_idx] : 1);
    section_service_get_by_class(target_class, sec_list, MAX_SECTIONS, &s_count);
    d->section_count = s_count;

    const char *s_names[MAX_SECTIONS + 1];
    int sel_s_idx = 0;
    for (int i = 0; i < s_count; i++) {
        s_names[i] = sec_list[i].name;
        d->section_ids[i] = sec_list[i].id;
        if (is_edit && sec_list[i].id == existing.section_id) sel_s_idx = i;
    }
    s_names[s_count] = NULL;
    d->combo_section = gtk_drop_down_new_from_strings(s_names);
    if (s_count > 0) gtk_drop_down_set_selected(GTK_DROP_DOWN(d->combo_section), sel_s_idx);
    gtk_box_append(GTK_BOX(b_s), d->combo_section);
    gtk_box_append(GTK_BOX(r_cs), b_s);

    gtk_box_append(GTK_BOX(form_box), r_cs);

    /* Day of week & Period */
    GtkWidget *r_dp = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 16);

    GtkWidget *b_day = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_hexpand(b_day, TRUE);
    GtkWidget *lbl_day = gtk_label_new("Day of Week:");
    gtk_label_set_xalign(GTK_LABEL(lbl_day), 0.0f);
    gtk_box_append(GTK_BOX(b_day), lbl_day);

    const char *days[] = { "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", NULL };
    d->combo_day = gtk_drop_down_new_from_strings(days);
    gtk_box_append(GTK_BOX(b_day), d->combo_day);
    gtk_box_append(GTK_BOX(r_dp), b_day);

    GtkWidget *b_p = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_hexpand(b_p, TRUE);
    GtkWidget *lbl_p = gtk_label_new("Period Number:");
    gtk_label_set_xalign(GTK_LABEL(lbl_p), 0.0f);
    gtk_box_append(GTK_BOX(b_p), lbl_p);

    const char *periods[] = { "Period 1 (08:30-09:15)", "Period 2 (09:15-10:00)", "Period 3 (10:00-10:45)",
                              "Period 4 (11:00-11:45)", "Period 5 (11:45-12:30)", "Period 6 (13:00-13:45)",
                              "Period 7 (13:45-14:30)", "Period 8 (14:30-15:15)", NULL };
    d->combo_period = gtk_drop_down_new_from_strings(periods);
    if (is_edit && existing.period_number >= 1 && existing.period_number <= 8) {
        gtk_drop_down_set_selected(GTK_DROP_DOWN(d->combo_period), existing.period_number - 1);
    }
    gtk_box_append(GTK_BOX(b_p), d->combo_period);
    gtk_box_append(GTK_BOX(r_dp), b_p);

    gtk_box_append(GTK_BOX(form_box), r_dp);

    /* Subject */
    GtkWidget *b_sub = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    GtkWidget *lbl_sub = gtk_label_new("Subject:");
    gtk_label_set_xalign(GTK_LABEL(lbl_sub), 0.0f);
    gtk_box_append(GTK_BOX(b_sub), lbl_sub);

    Subject subs[MAX_SUBJECTS];
    int sub_count = 0;
    subject_service_get_all(subs, MAX_SUBJECTS, &sub_count);
    d->subject_count = sub_count;

    const char *sub_names[MAX_SUBJECTS + 1];
    int sel_sub_idx = 0;
    for (int i = 0; i < sub_count; i++) {
        char buf[256];
        snprintf(buf, sizeof(buf), "%s - %s (%s)", subs[i].code, subs[i].name, subs[i].class_name);
        sub_names[i] = g_strdup(buf);
        d->subject_ids[i] = subs[i].id;
        if (is_edit && subs[i].id == existing.subject_id) sel_sub_idx = i;
    }
    sub_names[sub_count] = NULL;
    d->combo_subject = gtk_drop_down_new_from_strings(sub_names);
    if (sub_count > 0) gtk_drop_down_set_selected(GTK_DROP_DOWN(d->combo_subject), sel_sub_idx);
    gtk_box_append(GTK_BOX(b_sub), d->combo_subject);
    gtk_box_append(GTK_BOX(form_box), b_sub);

    /* Faculty / Teacher */
    GtkWidget *b_tch = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    GtkWidget *lbl_tch = gtk_label_new("Assigned Faculty / Teacher:");
    gtk_label_set_xalign(GTK_LABEL(lbl_tch), 0.0f);
    gtk_box_append(GTK_BOX(b_tch), lbl_tch);

    Teacher tchs[TEACHER_MAX_LIST];
    int tch_count = 0;
    teacher_service_get_all(NULL, "ACTIVE", tchs, TEACHER_MAX_LIST, &tch_count);
    d->teacher_count = tch_count;

    const char *tch_names[TEACHER_MAX_LIST + 1];
    int sel_tch_idx = 0;
    for (int i = 0; i < tch_count; i++) {
        char buf[384];
        snprintf(buf, sizeof(buf), "%s (%s)", tchs[i].full_name, tchs[i].subject);
        tch_names[i] = g_strdup(buf);
        d->teacher_ids[i] = tchs[i].id;
        if (is_edit && tchs[i].id == existing.teacher_id) sel_tch_idx = i;
    }
    tch_names[tch_count] = NULL;
    d->combo_teacher = gtk_drop_down_new_from_strings(tch_names);
    if (tch_count > 0) gtk_drop_down_set_selected(GTK_DROP_DOWN(d->combo_teacher), sel_tch_idx);
    gtk_box_append(GTK_BOX(b_tch), d->combo_teacher);
    gtk_box_append(GTK_BOX(form_box), b_tch);

    /* Room Number */
    GtkWidget *b_rm = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    GtkWidget *lbl_rm = gtk_label_new("Room / Laboratory Number:");
    gtk_label_set_xalign(GTK_LABEL(lbl_rm), 0.0f);
    gtk_box_append(GTK_BOX(b_rm), lbl_rm);
    d->entry_room = gtk_entry_new();
    if (is_edit) gtk_editable_set_text(GTK_EDITABLE(d->entry_room), existing.room_number);
    else gtk_editable_set_text(GTK_EDITABLE(d->entry_room), "Room 101");
    gtk_box_append(GTK_BOX(b_rm), d->entry_room);
    gtk_box_append(GTK_BOX(form_box), b_rm);

    /* Buttons */
    GtkWidget *btn_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_set_halign(btn_box, GTK_ALIGN_END);
    gtk_widget_set_margin_top(btn_box, 16);

    GtkWidget *cancel_btn = gtk_button_new_with_label("Cancel");
    gtk_widget_add_css_class(cancel_btn, "btn-secondary");
    g_signal_connect(cancel_btn, "clicked", G_CALLBACK(on_cancel_dialog), win);
    gtk_box_append(GTK_BOX(btn_box), cancel_btn);

    GtkWidget *save_btn = gtk_button_new_with_label("Save Slot");
    gtk_widget_add_css_class(save_btn, "btn-primary");
    g_signal_connect(save_btn, "clicked", G_CALLBACK(on_save_slot_dialog), d);
    gtk_box_append(GTK_BOX(btn_box), save_btn);

    gtk_box_append(GTK_BOX(main_box), btn_box);
    gtk_window_present(GTK_WINDOW(win));
}

/* 2. Delete Slot Confirmation */
typedef struct {
    TimetableWidgets *tw;
    int slot_id;
    GtkWidget *dialog;
} DeleteSlotData;

static void on_confirm_delete_slot(GtkButton *btn, gpointer user_data)
{
    (void)btn;
    DeleteSlotData *d = (DeleteSlotData *)user_data;
    timetable_service_delete(d->slot_id);
    refresh_view(d->tw);
    gtk_window_destroy(GTK_WINDOW(d->dialog));
    g_free(d);
}

static void show_delete_slot_dialog(TimetableWidgets *tw, int slot_id)
{
    DeleteSlotData *d = g_new0(DeleteSlotData, 1);
    d->tw = tw;
    d->slot_id = slot_id;

    GtkWidget *win = gtk_window_new();
    d->dialog = win;
    gtk_window_set_title(GTK_WINDOW(win), "Confirm Slot Deletion");
    gtk_window_set_modal(GTK_WINDOW(win), TRUE);
    gtk_window_set_default_size(GTK_WINDOW(win), 400, 180);

    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 16);
    gtk_widget_set_margin_start(box, 24);
    gtk_widget_set_margin_end(box, 24);
    gtk_widget_set_margin_top(box, 24);
    gtk_widget_set_margin_bottom(box, 24);
    gtk_window_set_child(GTK_WINDOW(win), box);

    GtkWidget *lbl = gtk_label_new("Are you sure you want to remove this timetable period slot?");
    gtk_box_append(GTK_BOX(box), lbl);

    GtkWidget *btn_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_set_halign(btn_box, GTK_ALIGN_END);

    GtkWidget *cancel = gtk_button_new_with_label("Cancel");
    gtk_widget_add_css_class(cancel, "btn-secondary");
    g_signal_connect(cancel, "clicked", G_CALLBACK(on_cancel_dialog), win);
    gtk_box_append(GTK_BOX(btn_box), cancel);

    GtkWidget *del = gtk_button_new_with_label("Delete Slot");
    gtk_widget_add_css_class(del, "btn-danger");
    g_signal_connect(del, "clicked", G_CALLBACK(on_confirm_delete_slot), d);
    gtk_box_append(GTK_BOX(btn_box), del);

    gtk_box_append(GTK_BOX(box), btn_box);
    gtk_window_present(GTK_WINDOW(win));
}

/* ── Main View Constructor ────────────────────────────────────────── */

GtkWidget *school_timetable_view_new(void)
{
    TimetableWidgets *tw = g_new0(TimetableWidgets, 1);
    tw->current_tab = TAB_CLASS_SCHEDULE;

    GtkWidget *scrolled = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
                                  GTK_POLICY_NEVER,
                                  GTK_POLICY_AUTOMATIC);

    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 20);
    gtk_widget_add_css_class(box, "page-container");
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled), box);
    tw->root_box = box;
    g_object_set_data_full(G_OBJECT(scrolled), "tw", tw, g_free);

    /* Header */
    GtkWidget *top_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 16);
    gtk_widget_set_valign(top_row, GTK_ALIGN_CENTER);

    GtkWidget *title_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    GtkWidget *title = gtk_label_new("Timetable & Schedules");
    gtk_widget_add_css_class(title, "page-title");
    gtk_label_set_xalign(GTK_LABEL(title), 0.0f);
    gtk_box_append(GTK_BOX(title_box), title);

    GtkWidget *sub = gtk_label_new("Class & faculty timetables, period allocations, and double-booking conflict detection");
    gtk_widget_add_css_class(sub, "page-subtitle");
    gtk_label_set_xalign(GTK_LABEL(sub), 0.0f);
    gtk_box_append(GTK_BOX(title_box), sub);

    gtk_widget_set_hexpand(title_box, TRUE);
    gtk_box_append(GTK_BOX(top_row), title_box);

    /* Add Slot Button */
    tw->add_slot_btn = gtk_button_new_with_label("+ Assign Slot");
    gtk_widget_add_css_class(tw->add_slot_btn, "btn-primary");
    g_signal_connect(tw->add_slot_btn, "clicked", G_CALLBACK(on_add_slot_clicked), tw);
    gtk_box_append(GTK_BOX(top_row), tw->add_slot_btn);

    gtk_box_append(GTK_BOX(box), top_row);

    /* Tabs */
    GtkWidget *tab_bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);

    tw->tab_class_btn = gtk_button_new_with_label("Class Timetable");
    gtk_widget_add_css_class(tw->tab_class_btn, "tab-btn");
    gtk_widget_add_css_class(tw->tab_class_btn, "tab-btn-active");
    g_signal_connect(tw->tab_class_btn, "clicked", G_CALLBACK(on_tab_class_clicked), tw);
    gtk_box_append(GTK_BOX(tab_bar), tw->tab_class_btn);

    tw->tab_teacher_btn = gtk_button_new_with_label("Teacher Weekly Schedule");
    gtk_widget_add_css_class(tw->tab_teacher_btn, "tab-btn");
    g_signal_connect(tw->tab_teacher_btn, "clicked", G_CALLBACK(on_tab_teacher_clicked), tw);
    gtk_box_append(GTK_BOX(tab_bar), tw->tab_teacher_btn);

    gtk_box_append(GTK_BOX(box), tab_bar);

    /* Content Container */
    tw->content_container = gtk_box_new(GTK_ORIENTATION_VERTICAL, 16);
    gtk_box_append(GTK_BOX(box), tw->content_container);

    refresh_view(tw);

    return scrolled;
}

void school_timetable_view_reload(GtkWidget *view)
{
    if (!view) return;
    TimetableWidgets *tw = (TimetableWidgets *)g_object_get_data(G_OBJECT(view), "tw");
    if (tw) refresh_view(tw);
}
