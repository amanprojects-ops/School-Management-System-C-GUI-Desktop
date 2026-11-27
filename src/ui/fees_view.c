/**
 * @file fees_view.c
 * @brief Fee management GUI module: payment entry, receipts, structures, and dues ledger.
 */

#include "ui/fees_view.h"
#include "ui/dialog.h"
#include "utils/print_pdf.h"
#include "settings.h"
#include "fees.h"
#include "classes.h"
#include "student.h"
#include "auth/auth.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAX_FEE_STUDENTS 100

typedef enum {
    TAB_COLLECT = 0,
    TAB_TRANSACTIONS,
    TAB_STRUCTURES,
    TAB_PENDING_DUES
} FeesTab;

typedef struct {
    GtkWidget *root_box;
    GtkWidget *tab_collect_btn;
    GtkWidget *tab_transactions_btn;
    GtkWidget *tab_structures_btn;
    GtkWidget *tab_dues_btn;
    GtkWidget *content_container;
    GtkWidget *primary_action_btn;
    GtkWidget *submit_btn;
    GtkWidget *status_banner;

    FeesTab current_tab;

    /* Collect fee form state */
    GtkWidget *combo_student;
    GtkWidget *combo_structure;
    GtkWidget *entry_amount;
    GtkWidget *entry_discount;
    GtkWidget *entry_late_fine;
    GtkWidget *combo_method;
    GtkWidget *entry_tx_ref;
    GtkWidget *entry_date;
    GtkWidget *entry_remarks;
    GtkWidget *lbl_due_summary;

    int student_ids[MAX_FEE_STUDENTS];
    int student_count;
    int structure_ids[MAX_FEE_STRUCTURES];
    int structure_count;

    /* Transactions filter state */
    char tx_search[64];
    char tx_method[32];
    GtkWidget *tx_search_entry;
    GtkWidget *tx_method_combo;

    /* Dues filter state */
    int dues_class_id;
    GtkWidget *dues_class_combo;
    SchoolClass classes[MAX_CLASSES];
    int class_count;
} FeesViewWidgets;

static void refresh_view(FeesViewWidgets *fvw);
static void show_receipt_dialog(const FeePayment *p);
static void show_add_edit_structure_dialog(FeesViewWidgets *fvw, int structure_id);

static void show_status_message(FeesViewWidgets *fvw, const char *msg, bool is_success)
{
    if (!fvw || !fvw->status_banner) return;
    gtk_label_set_text(GTK_LABEL(fvw->status_banner), msg);
    if (is_success) {
        gtk_widget_remove_css_class(fvw->status_banner, "card-kpi-red");
        gtk_widget_add_css_class(fvw->status_banner, "card-kpi-green");
    } else {
        gtk_widget_remove_css_class(fvw->status_banner, "card-kpi-green");
        gtk_widget_add_css_class(fvw->status_banner, "card-kpi-red");
    }
    gtk_widget_set_visible(fvw->status_banner, TRUE);
}

/* ── Tab Switch Handlers ─────────────────────────────────────────── */

static void on_tab_collect_clicked(GtkButton *btn, gpointer user_data)
{
    (void)btn;
    FeesViewWidgets *fvw = (FeesViewWidgets *)user_data;
    fvw->current_tab = TAB_COLLECT;
    if (fvw->status_banner) gtk_widget_set_visible(fvw->status_banner, FALSE);
    gtk_widget_add_css_class(fvw->tab_collect_btn, "tab-btn-active");
    gtk_widget_remove_css_class(fvw->tab_transactions_btn, "tab-btn-active");
    gtk_widget_remove_css_class(fvw->tab_structures_btn, "tab-btn-active");
    gtk_widget_remove_css_class(fvw->tab_dues_btn, "tab-btn-active");
    gtk_widget_set_visible(fvw->primary_action_btn, FALSE);
    refresh_view(fvw);
}

static void on_tab_transactions_clicked(GtkButton *btn, gpointer user_data)
{
    (void)btn;
    FeesViewWidgets *fvw = (FeesViewWidgets *)user_data;
    fvw->current_tab = TAB_TRANSACTIONS;
    if (fvw->status_banner) gtk_widget_set_visible(fvw->status_banner, FALSE);
    gtk_widget_remove_css_class(fvw->tab_collect_btn, "tab-btn-active");
    gtk_widget_add_css_class(fvw->tab_transactions_btn, "tab-btn-active");
    gtk_widget_remove_css_class(fvw->tab_structures_btn, "tab-btn-active");
    gtk_widget_remove_css_class(fvw->tab_dues_btn, "tab-btn-active");
    gtk_widget_set_visible(fvw->primary_action_btn, FALSE);
    refresh_view(fvw);
}

static void on_tab_structures_clicked(GtkButton *btn, gpointer user_data)
{
    (void)btn;
    FeesViewWidgets *fvw = (FeesViewWidgets *)user_data;
    fvw->current_tab = TAB_STRUCTURES;
    if (fvw->status_banner) gtk_widget_set_visible(fvw->status_banner, FALSE);
    gtk_widget_remove_css_class(fvw->tab_collect_btn, "tab-btn-active");
    gtk_widget_remove_css_class(fvw->tab_transactions_btn, "tab-btn-active");
    gtk_widget_add_css_class(fvw->tab_structures_btn, "tab-btn-active");
    gtk_widget_remove_css_class(fvw->tab_dues_btn, "tab-btn-active");
    gtk_widget_set_visible(fvw->primary_action_btn, auth_current_user_can_access("FEES", true));
    gtk_button_set_label(GTK_BUTTON(fvw->primary_action_btn), "+ Add Fee Structure");
    refresh_view(fvw);
}

static void on_tab_dues_clicked(GtkButton *btn, gpointer user_data)
{
    (void)btn;
    FeesViewWidgets *fvw = (FeesViewWidgets *)user_data;
    fvw->current_tab = TAB_PENDING_DUES;
    if (fvw->status_banner) gtk_widget_set_visible(fvw->status_banner, FALSE);
    gtk_widget_remove_css_class(fvw->tab_collect_btn, "tab-btn-active");
    gtk_widget_remove_css_class(fvw->tab_transactions_btn, "tab-btn-active");
    gtk_widget_remove_css_class(fvw->tab_structures_btn, "tab-btn-active");
    gtk_widget_add_css_class(fvw->tab_dues_btn, "tab-btn-active");
    gtk_widget_set_visible(fvw->primary_action_btn, FALSE);
    refresh_view(fvw);
}

static void on_primary_action_clicked(GtkButton *btn, gpointer user_data)
{
    (void)btn;
    FeesViewWidgets *fvw = (FeesViewWidgets *)user_data;
    if (!auth_current_user_can_access("FEES", true)) {
        show_status_message(fvw, "Read-only access: your role cannot create fee structures.", false);
        return;
    }
    if (fvw->current_tab == TAB_STRUCTURES) {
        show_add_edit_structure_dialog(fvw, 0);
    }
}

/* ── 1. Tab: Collect Fee Form ─────────────────────────────────────── */

static void update_collect_student_dues(FeesViewWidgets *fvw)
{
    guint sel = gtk_drop_down_get_selected(GTK_DROP_DOWN(fvw->combo_student));
    if ((int)sel >= fvw->student_count) return;

    int student_id = fvw->student_ids[sel];
    StudentFeeSummary summary;
    if (fee_service_get_student_ledger(student_id, &summary, NULL, 0, NULL)) {
        char buf[512];
        snprintf(buf, sizeof(buf),
                 "Student: %s (%s) — Class: %s %s\n"
                 "Assigned Fees: ₹%.2f | Paid to Date: ₹%.2f | Outstanding Due: ₹%.2f",
                 summary.student_name, summary.admission_no, summary.class_name, summary.section_name,
                 summary.total_assigned, summary.total_paid, summary.balance_due);
        gtk_label_set_text(GTK_LABEL(fvw->lbl_due_summary), buf);
    }
}

static void on_collect_student_changed(GtkDropDown *dropdown, GParamSpec *pspec, gpointer user_data)
{
    (void)dropdown;
    (void)pspec;
    FeesViewWidgets *fvw = (FeesViewWidgets *)user_data;
    update_collect_student_dues(fvw);
}

static void on_collect_structure_changed(GtkDropDown *dropdown, GParamSpec *pspec, gpointer user_data)
{
    (void)pspec;
    FeesViewWidgets *fvw = (FeesViewWidgets *)user_data;
    guint sel = gtk_drop_down_get_selected(dropdown);
    if ((int)sel < fvw->structure_count) {
        FeeStructure st;
        if (fee_structure_get_by_id(fvw->structure_ids[sel], &st)) {
            char amt_buf[32];
            snprintf(amt_buf, sizeof(amt_buf), "%.2f", st.amount);
            gtk_editable_set_text(GTK_EDITABLE(fvw->entry_amount), amt_buf);
        }
    }
}

static void on_submit_payment_clicked(GtkButton *btn, gpointer user_data)
{
    (void)btn;
    FeesViewWidgets *fvw = (FeesViewWidgets *)user_data;
    if (!auth_current_user_can_access("FEES", true)) {
        if (fvw->status_banner) {
            gtk_label_set_text(GTK_LABEL(fvw->status_banner), "Read-only access: your role cannot collect fee payments.");
            gtk_widget_set_visible(fvw->status_banner, TRUE);
        }
        return;
    }

    guint sel_stu = gtk_drop_down_get_selected(GTK_DROP_DOWN(fvw->combo_student));
    if ((int)sel_stu >= fvw->student_count) return;

    guint sel_str = gtk_drop_down_get_selected(GTK_DROP_DOWN(fvw->combo_structure));
    if ((int)sel_str >= fvw->structure_count) return;

    const char *amt_str = gtk_editable_get_text(GTK_EDITABLE(fvw->entry_amount));
    const char *disc_str = gtk_editable_get_text(GTK_EDITABLE(fvw->entry_discount));
    const char *fine_str = gtk_editable_get_text(GTK_EDITABLE(fvw->entry_late_fine));
    const char *date_str = gtk_editable_get_text(GTK_EDITABLE(fvw->entry_date));
    const char *tx_ref = gtk_editable_get_text(GTK_EDITABLE(fvw->entry_tx_ref));
    const char *remarks = gtk_editable_get_text(GTK_EDITABLE(fvw->entry_remarks));

    double amount = amt_str ? atof(amt_str) : 0.0;
    if (amount <= 0.0) return;

    FeePayment p;
    memset(&p, 0, sizeof(FeePayment));
    p.student_id = fvw->student_ids[sel_stu];
    p.fee_structure_id = fvw->structure_ids[sel_str];
    p.amount_paid = amount;
    p.discount = disc_str ? atof(disc_str) : 0.0;
    p.late_fine = fine_str ? atof(fine_str) : 0.0;

    const char *methods[] = { PAYMENT_METHOD_CASH, PAYMENT_METHOD_UPI,
                              PAYMENT_METHOD_BANK, PAYMENT_METHOD_CHEQUE };
    guint sel_meth = gtk_drop_down_get_selected(GTK_DROP_DOWN(fvw->combo_method));
    if (sel_meth > 3) sel_meth = 0;
    strncpy(p.payment_method, methods[sel_meth], sizeof(p.payment_method) - 1);

    if (tx_ref) strncpy(p.transaction_ref, tx_ref, sizeof(p.transaction_ref) - 1);
    if (date_str && strlen(date_str) >= 10) strncpy(p.payment_date, date_str, sizeof(p.payment_date) - 1);
    if (remarks) strncpy(p.remarks, remarks, sizeof(p.remarks) - 1);

    const UserSession *sess = auth_get_session();
    p.collected_by = sess ? sess->user_id : 1;

    int new_id = 0;
    if (fee_payment_create(&p, &new_id)) {
        FeePayment full_p;
        if (fee_payment_get_by_id(new_id, &full_p)) {
            show_receipt_dialog(&full_p);
        }
        show_status_message(fvw, "Fee payment collected successfully. Official receipt generated.", true);
        update_collect_student_dues(fvw);
        /* Reset form fields */
        gtk_editable_set_text(GTK_EDITABLE(fvw->entry_amount), "0.00");
        gtk_editable_set_text(GTK_EDITABLE(fvw->entry_discount), "0.00");
        gtk_editable_set_text(GTK_EDITABLE(fvw->entry_late_fine), "0.00");
        gtk_editable_set_text(GTK_EDITABLE(fvw->entry_tx_ref), "");
        gtk_editable_set_text(GTK_EDITABLE(fvw->entry_remarks), "");
    } else {
        show_status_message(fvw, "Failed to record fee payment.", false);
    }
}

static void render_collect_fee_tab(FeesViewWidgets *fvw)
{
    GtkWidget *card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 16);
    gtk_widget_add_css_class(card, "card");
    gtk_box_append(GTK_BOX(fvw->content_container), card);

    GtkWidget *card_title = gtk_label_new("Fee Collection & Payment Receipt");
    gtk_widget_add_css_class(card_title, "dashboard-card-title");
    gtk_label_set_xalign(GTK_LABEL(card_title), 0.0f);
    gtk_box_append(GTK_BOX(card), card_title);

    /* Ledger Summary Box */
    fvw->lbl_due_summary = gtk_label_new("Select a student to load outstanding ledger details.");
    gtk_widget_add_css_class(fvw->lbl_due_summary, "badge-adm");
    gtk_label_set_xalign(GTK_LABEL(fvw->lbl_due_summary), 0.0f);
    gtk_box_append(GTK_BOX(card), fvw->lbl_due_summary);

    GtkWidget *form_grid = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_box_append(GTK_BOX(card), form_grid);

    /* 1. Student Selector */
    GtkWidget *row1 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 16);
    GtkWidget *b_stu = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_hexpand(b_stu, TRUE);
    GtkWidget *lbl_stu = gtk_label_new("Student:");
    gtk_label_set_xalign(GTK_LABEL(lbl_stu), 0.0f);
    gtk_box_append(GTK_BOX(b_stu), lbl_stu);

    StudentFilter sf = { .status = "ACTIVE" };
    StudentPage sp;
    student_service_get_page(&sf, &sp);
    fvw->student_count = sp.count;

    const char *stu_names[MAX_FEE_STUDENTS + 1];
    for (int i = 0; i < sp.count; i++) {
        char buf[256];
        snprintf(buf, sizeof(buf), "%s (%s) - Class %s",
                 sp.items[i].full_name, sp.items[i].admission_no, sp.items[i].class_name);
        stu_names[i] = g_strdup(buf);
        fvw->student_ids[i] = sp.items[i].id;
    }
    stu_names[sp.count] = NULL;

    fvw->combo_student = gtk_drop_down_new_from_strings(stu_names);
    g_signal_connect(fvw->combo_student, "notify::selected", G_CALLBACK(on_collect_student_changed), fvw);
    gtk_box_append(GTK_BOX(b_stu), fvw->combo_student);
    gtk_box_append(GTK_BOX(row1), b_stu);

    /* 2. Fee Structure Selector */
    GtkWidget *b_str = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_hexpand(b_str, TRUE);
    GtkWidget *lbl_str = gtk_label_new("Fee Structure / Category:");
    gtk_label_set_xalign(GTK_LABEL(lbl_str), 0.0f);
    gtk_box_append(GTK_BOX(b_str), lbl_str);

    FeeStructure structures[MAX_FEE_STRUCTURES];
    int str_count = 0;
    fee_structure_get_all(0, 0, structures, MAX_FEE_STRUCTURES, &str_count);
    fvw->structure_count = str_count;

    const char *str_names[MAX_FEE_STRUCTURES + 1];
    for (int i = 0; i < str_count; i++) {
        char buf[256];
        snprintf(buf, sizeof(buf), "%s (₹%.2f) - %s",
                 structures[i].name, structures[i].amount, structures[i].class_name);
        str_names[i] = g_strdup(buf);
        fvw->structure_ids[i] = structures[i].id;
    }
    str_names[str_count] = NULL;

    fvw->combo_structure = gtk_drop_down_new_from_strings(str_names);
    g_signal_connect(fvw->combo_structure, "notify::selected", G_CALLBACK(on_collect_structure_changed), fvw);
    gtk_box_append(GTK_BOX(b_str), fvw->combo_structure);
    gtk_box_append(GTK_BOX(row1), b_str);

    gtk_box_append(GTK_BOX(form_grid), row1);

    /* Row 2: Amount, Discount, Late Fine */
    GtkWidget *row2 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 16);

    GtkWidget *b_amt = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_hexpand(b_amt, TRUE);
    GtkWidget *lbl_amt = gtk_label_new("Amount to Pay (₹):");
    gtk_label_set_xalign(GTK_LABEL(lbl_amt), 0.0f);
    gtk_box_append(GTK_BOX(b_amt), lbl_amt);
    fvw->entry_amount = gtk_entry_new();
    gtk_editable_set_text(GTK_EDITABLE(fvw->entry_amount), "0.00");
    gtk_box_append(GTK_BOX(b_amt), fvw->entry_amount);
    gtk_box_append(GTK_BOX(row2), b_amt);

    GtkWidget *b_disc = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_hexpand(b_disc, TRUE);
    GtkWidget *lbl_disc = gtk_label_new("Concession / Discount (₹):");
    gtk_label_set_xalign(GTK_LABEL(lbl_disc), 0.0f);
    gtk_box_append(GTK_BOX(b_disc), lbl_disc);
    fvw->entry_discount = gtk_entry_new();
    gtk_editable_set_text(GTK_EDITABLE(fvw->entry_discount), "0.00");
    gtk_box_append(GTK_BOX(b_disc), fvw->entry_discount);
    gtk_box_append(GTK_BOX(row2), b_disc);

    GtkWidget *b_fine = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_hexpand(b_fine, TRUE);
    GtkWidget *lbl_fine = gtk_label_new("Late Fine (₹):");
    gtk_label_set_xalign(GTK_LABEL(lbl_fine), 0.0f);
    gtk_box_append(GTK_BOX(b_fine), lbl_fine);
    fvw->entry_late_fine = gtk_entry_new();
    gtk_editable_set_text(GTK_EDITABLE(fvw->entry_late_fine), "0.00");
    gtk_box_append(GTK_BOX(b_fine), fvw->entry_late_fine);
    gtk_box_append(GTK_BOX(row2), b_fine);

    gtk_box_append(GTK_BOX(form_grid), row2);

    /* Row 3: Method, Ref, Date */
    GtkWidget *row3 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 16);

    GtkWidget *b_mth = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_hexpand(b_mth, TRUE);
    GtkWidget *lbl_mth = gtk_label_new("Payment Mode:");
    gtk_label_set_xalign(GTK_LABEL(lbl_mth), 0.0f);
    gtk_box_append(GTK_BOX(b_mth), lbl_mth);
    const char *m_names[] = { "Cash", "UPI", "Bank Transfer", "Cheque", NULL };
    fvw->combo_method = gtk_drop_down_new_from_strings(m_names);
    gtk_box_append(GTK_BOX(b_mth), fvw->combo_method);
    gtk_box_append(GTK_BOX(row3), b_mth);

    GtkWidget *b_ref = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_hexpand(b_ref, TRUE);
    GtkWidget *lbl_ref = gtk_label_new("Transaction Ref / Cheque No:");
    gtk_label_set_xalign(GTK_LABEL(lbl_ref), 0.0f);
    gtk_box_append(GTK_BOX(b_ref), lbl_ref);
    fvw->entry_tx_ref = gtk_entry_new();
    gtk_box_append(GTK_BOX(b_ref), fvw->entry_tx_ref);
    gtk_box_append(GTK_BOX(row3), b_ref);

    GtkWidget *b_dt = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_hexpand(b_dt, TRUE);
    GtkWidget *lbl_dt = gtk_label_new("Payment Date (YYYY-MM-DD):");
    gtk_label_set_xalign(GTK_LABEL(lbl_dt), 0.0f);
    gtk_box_append(GTK_BOX(b_dt), lbl_dt);
    fvw->entry_date = gtk_entry_new();

    time_t t = time(NULL);
    struct tm tm = *localtime(&t);
    char today[32];
    snprintf(today, sizeof(today), "%04d-%02d-%02d", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday);
    gtk_editable_set_text(GTK_EDITABLE(fvw->entry_date), today);
    gtk_box_append(GTK_BOX(b_dt), fvw->entry_date);
    gtk_box_append(GTK_BOX(row3), b_dt);

    gtk_box_append(GTK_BOX(form_grid), row3);

    /* Row 4: Remarks & Submit */
    GtkWidget *row4 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 16);
    gtk_widget_set_valign(row4, GTK_ALIGN_END);

    GtkWidget *b_rmk = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_hexpand(b_rmk, TRUE);
    GtkWidget *lbl_rmk = gtk_label_new("Remarks / Notes:");
    gtk_label_set_xalign(GTK_LABEL(lbl_rmk), 0.0f);
    gtk_box_append(GTK_BOX(b_rmk), lbl_rmk);
    fvw->entry_remarks = gtk_entry_new();
    gtk_box_append(GTK_BOX(b_rmk), fvw->entry_remarks);
    gtk_box_append(GTK_BOX(row4), b_rmk);

    GtkWidget *submit_btn = gtk_button_new_with_label("Collect & Print Receipt");
    fvw->submit_btn = submit_btn;
    gtk_widget_add_css_class(submit_btn, "btn-primary");
    g_signal_connect(submit_btn, "clicked", G_CALLBACK(on_submit_payment_clicked), fvw);
    gtk_widget_set_visible(submit_btn, auth_current_user_can_access("FEES", true));
    gtk_box_append(GTK_BOX(row4), submit_btn);

    gtk_box_append(GTK_BOX(form_grid), row4);

    if (fvw->student_count > 0) {
        update_collect_student_dues(fvw);
    }
}

/* ── 2. Tab: Payment Transactions & Receipt Viewer ────────────────── */

static void on_view_receipt_clicked(GtkButton *btn, gpointer user_data)
{
    (void)btn;
    int payment_id = GPOINTER_TO_INT(user_data);
    FeePayment p;
    if (fee_payment_get_by_id(payment_id, &p)) {
        show_receipt_dialog(&p);
    }
}

static void on_tx_search_changed(GtkSearchEntry *entry, gpointer user_data)
{
    FeesViewWidgets *fvw = (FeesViewWidgets *)user_data;
    const char *text = gtk_editable_get_text(GTK_EDITABLE(entry));
    if (text) strncpy(fvw->tx_search, text, sizeof(fvw->tx_search) - 1);
    else fvw->tx_search[0] = '\0';
    refresh_view(fvw);
}

static void on_tx_method_changed(GtkDropDown *dropdown, GParamSpec *pspec, gpointer user_data)
{
    (void)pspec;
    FeesViewWidgets *fvw = (FeesViewWidgets *)user_data;
    guint sel = gtk_drop_down_get_selected(dropdown);
    const char *methods[] = { "", PAYMENT_METHOD_CASH, PAYMENT_METHOD_UPI,
                              PAYMENT_METHOD_BANK, PAYMENT_METHOD_CHEQUE };
    if (sel <= 4) strncpy(fvw->tx_method, methods[sel], sizeof(fvw->tx_method) - 1);
    else fvw->tx_method[0] = '\0';
    refresh_view(fvw);
}

static void render_transactions_tab(FeesViewWidgets *fvw)
{
    /* Filter Bar */
    GtkWidget *filter_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_set_margin_bottom(filter_box, 8);

    fvw->tx_search_entry = gtk_search_entry_new();
    gtk_widget_set_size_request(fvw->tx_search_entry, 240, -1);
    gtk_editable_set_text(GTK_EDITABLE(fvw->tx_search_entry), fvw->tx_search);
    g_signal_connect(fvw->tx_search_entry, "search-changed", G_CALLBACK(on_tx_search_changed), fvw);
    gtk_box_append(GTK_BOX(filter_box), fvw->tx_search_entry);

    const char *m_opts[] = { "All Payment Modes", "Cash", "UPI", "Bank Transfer", "Cheque", NULL };
    fvw->tx_method_combo = gtk_drop_down_new_from_strings(m_opts);
    g_signal_connect(fvw->tx_method_combo, "notify::selected", G_CALLBACK(on_tx_method_changed), fvw);
    gtk_box_append(GTK_BOX(filter_box), fvw->tx_method_combo);

    gtk_box_append(GTK_BOX(fvw->content_container), filter_box);

    FeeFilter filter = {0};
    if (strlen(fvw->tx_search) > 0) strncpy(filter.search, fvw->tx_search, sizeof(filter.search) - 1);
    if (strlen(fvw->tx_method) > 0) strncpy(filter.payment_method, fvw->tx_method, sizeof(filter.payment_method) - 1);

    FeePayment payments[MAX_FEE_PAYMENTS];
    int count = 0;
    fee_payment_get_all(&filter, payments, MAX_FEE_PAYMENTS, &count);

    GtkWidget *card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_add_css_class(card, "card");
    gtk_box_append(GTK_BOX(fvw->content_container), card);

    /* Header */
    GtkWidget *h_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_add_css_class(h_row, "table-header-row");

    const char *cols[] = { "Receipt No", "Date", "Student Name", "Fee Category", "Amount Paid", "Mode", "Action" };
    int widths[] = { 140, 110, 200, 180, 120, 100, 110 };

    for (int i = 0; i < 7; i++) {
        GtkWidget *th = gtk_label_new(cols[i]);
        gtk_widget_add_css_class(th, "table-th");
        gtk_label_set_xalign(GTK_LABEL(th), 0.0f);
        gtk_widget_set_size_request(th, widths[i], -1);
        if (i == 2) gtk_widget_set_hexpand(th, TRUE);
        gtk_box_append(GTK_BOX(h_row), th);
    }
    gtk_box_append(GTK_BOX(card), h_row);

    if (count == 0) {
        GtkWidget *empty_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
        gtk_widget_set_margin_top(empty_box, 30);
        gtk_widget_set_margin_bottom(empty_box, 30);
        GtkWidget *lbl = gtk_label_new("No fee payment transactions found.");
        gtk_widget_add_css_class(lbl, "dim-label");
        gtk_box_append(GTK_BOX(empty_box), lbl);
        gtk_box_append(GTK_BOX(card), empty_box);
        return;
    }

    for (int i = 0; i < count; i++) {
        const FeePayment *p = &payments[i];
        GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
        gtk_widget_add_css_class(row, "table-data-row");

        /* Receipt No */
        GtkWidget *rcp_lbl = gtk_label_new(p->receipt_no);
        gtk_widget_add_css_class(rcp_lbl, "badge-adm");
        gtk_widget_set_size_request(rcp_lbl, widths[0], -1);
        gtk_label_set_xalign(GTK_LABEL(rcp_lbl), 0.0f);
        gtk_box_append(GTK_BOX(row), rcp_lbl);

        /* Date */
        GtkWidget *dt_lbl = gtk_label_new(p->payment_date);
        gtk_widget_set_size_request(dt_lbl, widths[1], -1);
        gtk_label_set_xalign(GTK_LABEL(dt_lbl), 0.0f);
        gtk_box_append(GTK_BOX(row), dt_lbl);

        /* Student Name */
        GtkWidget *name_lbl = gtk_label_new(p->student_name);
        gtk_widget_add_css_class(name_lbl, "table-cell-bold");
        gtk_label_set_xalign(GTK_LABEL(name_lbl), 0.0f);
        gtk_widget_set_hexpand(name_lbl, TRUE);
        gtk_box_append(GTK_BOX(row), name_lbl);

        /* Fee Structure */
        GtkWidget *fs_lbl = gtk_label_new(p->fee_structure_name);
        gtk_widget_set_size_request(fs_lbl, widths[3], -1);
        gtk_label_set_xalign(GTK_LABEL(fs_lbl), 0.0f);
        gtk_box_append(GTK_BOX(row), fs_lbl);

        /* Amount Paid */
        char amt_str[32];
        snprintf(amt_str, sizeof(amt_str), "₹%.2f", p->amount_paid);
        GtkWidget *amt_lbl = gtk_label_new(amt_str);
        gtk_widget_add_css_class(amt_lbl, "badge-status-active");
        gtk_widget_set_size_request(amt_lbl, widths[4], -1);
        gtk_box_append(GTK_BOX(row), amt_lbl);

        /* Method */
        GtkWidget *m_lbl = gtk_label_new(p->payment_method);
        gtk_widget_set_size_request(m_lbl, widths[5], -1);
        gtk_box_append(GTK_BOX(row), m_lbl);

        /* Action: View Receipt */
        GtkWidget *btn = gtk_button_new_with_label("Receipt");
        gtk_widget_add_css_class(btn, "btn-secondary");
        gtk_widget_set_size_request(btn, widths[6], -1);
        g_signal_connect(btn, "clicked", G_CALLBACK(on_view_receipt_clicked), GINT_TO_POINTER(p->id));
        gtk_box_append(GTK_BOX(row), btn);

        gtk_box_append(GTK_BOX(card), row);
    }
}

/* ── 3. Tab: Fee Structures ───────────────────────────────────────── */

static void on_edit_structure_clicked(GtkButton *btn, gpointer user_data)
{
    FeesViewWidgets *fvw = (FeesViewWidgets *)g_object_get_data(G_OBJECT(btn), "fvw");
    int id = GPOINTER_TO_INT(user_data);
    show_add_edit_structure_dialog(fvw, id);
}

typedef struct {
    FeesViewWidgets *fvw;
    int id;
    GtkWidget *dialog;
} DeleteFeeStructData;

static void on_confirm_delete_structure(GtkButton *btn, gpointer user_data)
{
    (void)btn;
    DeleteFeeStructData *d = (DeleteFeeStructData *)user_data;
    if (fee_structure_delete(d->id)) {
        show_status_message(d->fvw, "Fee structure deleted successfully.", true);
    } else {
        show_status_message(d->fvw, "Failed to delete fee structure.", false);
    }
    refresh_view(d->fvw);
    school_dialog_close(d->dialog);
}

static void show_delete_structure_dialog(FeesViewWidgets *fvw, int id, const char *name)
{
    DeleteFeeStructData *d = g_new0(DeleteFeeStructData, 1);
    d->fvw = fvw;
    d->id = id;

    GtkWidget *win = school_dialog_new("Confirm Fee Structure Deletion", 420, 200);
    d->dialog = win;

    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 16);
    gtk_widget_set_margin_start(box, 24);
    gtk_widget_set_margin_end(box, 24);
    gtk_widget_set_margin_top(box, 20);
    gtk_widget_set_margin_bottom(box, 20);
    school_dialog_set_child(win, box);

    char msg[256];
    snprintf(msg, sizeof(msg), "Are you sure you want to delete fee structure '%s'?\nExisting payment records will remain in audit history.",
             name ? name : "this structure");
    GtkWidget *lbl = gtk_label_new(msg);
    gtk_label_set_wrap(GTK_LABEL(lbl), TRUE);
    gtk_box_append(GTK_BOX(box), lbl);

    GtkWidget *btn_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_set_halign(btn_box, GTK_ALIGN_END);

    GtkWidget *cancel = gtk_button_new_with_label("Cancel");
    gtk_widget_add_css_class(cancel, "btn-secondary");
    g_signal_connect_swapped(cancel, "clicked", G_CALLBACK(school_dialog_close), win);
    gtk_box_append(GTK_BOX(btn_box), cancel);

    GtkWidget *del = gtk_button_new_with_label("Delete Structure");
    gtk_widget_add_css_class(del, "btn-danger");
    g_signal_connect(del, "clicked", G_CALLBACK(on_confirm_delete_structure), d);
    gtk_box_append(GTK_BOX(btn_box), del);

    gtk_box_append(GTK_BOX(box), btn_box);
    g_object_set_data_full(G_OBJECT(win), "d", d, g_free);
    school_dialog_present(win);
}

static void on_delete_structure_clicked(GtkButton *btn, gpointer user_data)
{
    FeesViewWidgets *fvw = (FeesViewWidgets *)g_object_get_data(G_OBJECT(btn), "fvw");
    int id = GPOINTER_TO_INT(user_data);
    const char *name = (const char *)g_object_get_data(G_OBJECT(btn), "struct_name");
    show_delete_structure_dialog(fvw, id, name ? name : "Fee Structure");
}

static void render_structures_tab(FeesViewWidgets *fvw)
{
    FeeStructure structures[MAX_FEE_STRUCTURES];
    int count = 0;
    fee_structure_get_all(0, 0, structures, MAX_FEE_STRUCTURES, &count);

    GtkWidget *card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_add_css_class(card, "card");
    gtk_box_append(GTK_BOX(fvw->content_container), card);

    GtkWidget *h_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_add_css_class(h_row, "table-header-row");

    const char *cols[] = { "Structure Name", "Class", "Category", "Amount", "Due Date", "Actions" };
    int widths[] = { 220, 130, 130, 110, 110, 140 };

    for (int i = 0; i < 6; i++) {
        GtkWidget *th = gtk_label_new(cols[i]);
        gtk_widget_add_css_class(th, "table-th");
        gtk_label_set_xalign(GTK_LABEL(th), 0.0f);
        gtk_widget_set_size_request(th, widths[i], -1);
        if (i == 0) gtk_widget_set_hexpand(th, TRUE);
        gtk_box_append(GTK_BOX(h_row), th);
    }
    gtk_box_append(GTK_BOX(card), h_row);

    if (count == 0) {
        GtkWidget *empty_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
        gtk_widget_set_margin_top(empty_box, 30);
        GtkWidget *lbl = gtk_label_new("No fee structures configured. Click '+ Add Fee Structure' to create one.");
        gtk_widget_add_css_class(lbl, "dim-label");
        gtk_box_append(GTK_BOX(empty_box), lbl);
        gtk_box_append(GTK_BOX(card), empty_box);
        return;
    }

    for (int i = 0; i < count; i++) {
        const FeeStructure *st = &structures[i];
        GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
        gtk_widget_add_css_class(row, "table-data-row");

        /* Name */
        GtkWidget *name_lbl = gtk_label_new(st->name);
        gtk_widget_add_css_class(name_lbl, "table-cell-bold");
        gtk_label_set_xalign(GTK_LABEL(name_lbl), 0.0f);
        gtk_widget_set_hexpand(name_lbl, TRUE);
        gtk_box_append(GTK_BOX(row), name_lbl);

        /* Class */
        GtkWidget *cls_lbl = gtk_label_new(st->class_name);
        gtk_widget_set_size_request(cls_lbl, widths[1], -1);
        gtk_label_set_xalign(GTK_LABEL(cls_lbl), 0.0f);
        gtk_box_append(GTK_BOX(row), cls_lbl);

        /* Type */
        GtkWidget *type_lbl = gtk_label_new(st->fee_type);
        gtk_widget_add_css_class(type_lbl, "badge-role");
        gtk_widget_set_size_request(type_lbl, widths[2], -1);
        gtk_box_append(GTK_BOX(row), type_lbl);

        /* Amount */
        char amt_str[32];
        snprintf(amt_str, sizeof(amt_str), "₹%.2f", st->amount);
        GtkWidget *amt_lbl = gtk_label_new(amt_str);
        gtk_widget_set_size_request(amt_lbl, widths[3], -1);
        gtk_label_set_xalign(GTK_LABEL(amt_lbl), 0.0f);
        gtk_box_append(GTK_BOX(row), amt_lbl);

        /* Due Date */
        GtkWidget *due_lbl = gtk_label_new(strlen(st->due_date) > 0 ? st->due_date : "-");
        gtk_widget_set_size_request(due_lbl, widths[4], -1);
        gtk_label_set_xalign(GTK_LABEL(due_lbl), 0.0f);
        gtk_box_append(GTK_BOX(row), due_lbl);

        /* Actions */
        gboolean can_edit = auth_current_user_can_access("FEES", true);
        GtkWidget *act_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
        gtk_widget_set_size_request(act_box, widths[5], -1);

        GtkWidget *edit_btn = gtk_button_new_with_label("Edit");
        gtk_widget_add_css_class(edit_btn, "btn-secondary");
        g_object_set_data(G_OBJECT(edit_btn), "fvw", fvw);
        g_signal_connect(edit_btn, "clicked", G_CALLBACK(on_edit_structure_clicked), GINT_TO_POINTER(st->id));
        gtk_widget_set_visible(edit_btn, can_edit);
        gtk_box_append(GTK_BOX(act_box), edit_btn);

        GtkWidget *del_btn = gtk_button_new_with_label("Delete");
        gtk_widget_add_css_class(del_btn, "btn-danger");
        g_object_set_data(G_OBJECT(del_btn), "fvw", fvw);
        g_object_set_data_full(G_OBJECT(del_btn), "struct_name", g_strdup(st->name), g_free);
        g_signal_connect(del_btn, "clicked", G_CALLBACK(on_delete_structure_clicked), GINT_TO_POINTER(st->id));
        gtk_widget_set_visible(del_btn, can_edit);
        gtk_box_append(GTK_BOX(act_box), del_btn);

        gtk_box_append(GTK_BOX(row), act_box);
        gtk_box_append(GTK_BOX(card), row);
    }
}

/* ── 4. Tab: Pending Dues Ledger ──────────────────────────────────── */

static void on_dues_class_changed(GtkDropDown *dropdown, GParamSpec *pspec, gpointer user_data)
{
    (void)pspec;
    FeesViewWidgets *fvw = (FeesViewWidgets *)user_data;
    guint sel = gtk_drop_down_get_selected(dropdown);
    if (sel == 0) {
        fvw->dues_class_id = 0;
    } else if ((int)sel - 1 < fvw->class_count) {
        fvw->dues_class_id = fvw->classes[sel - 1].id;
    }
    refresh_view(fvw);
}

static void on_collect_due_for_student(GtkButton *btn, gpointer user_data)
{
    FeesViewWidgets *fvw = (FeesViewWidgets *)g_object_get_data(G_OBJECT(btn), "fvw");
    if (!auth_current_user_can_access("FEES", true)) {
        show_status_message(fvw, "Read-only access: your role cannot collect fee payments.", false);
        return;
    }
    int student_id = GPOINTER_TO_INT(user_data);

    /* Switch to collect tab and select this student */
    fvw->current_tab = TAB_COLLECT;
    gtk_widget_add_css_class(fvw->tab_collect_btn, "tab-btn-active");
    gtk_widget_remove_css_class(fvw->tab_dues_btn, "tab-btn-active");
    refresh_view(fvw);

    for (int i = 0; i < fvw->student_count; i++) {
        if (fvw->student_ids[i] == student_id) {
            gtk_drop_down_set_selected(GTK_DROP_DOWN(fvw->combo_student), i);
            break;
        }
    }
}

static void render_pending_dues_tab(FeesViewWidgets *fvw)
{
    /* Filter Bar */
    GtkWidget *filter_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_set_margin_bottom(filter_box, 8);

    const char *cls_names[MAX_CLASSES + 2];
    cls_names[0] = "All Classes";
    for (int i = 0; i < fvw->class_count; i++) {
        cls_names[i + 1] = fvw->classes[i].name;
    }
    cls_names[fvw->class_count + 1] = NULL;

    fvw->dues_class_combo = gtk_drop_down_new_from_strings(cls_names);
    g_signal_connect(fvw->dues_class_combo, "notify::selected", G_CALLBACK(on_dues_class_changed), fvw);
    gtk_box_append(GTK_BOX(filter_box), fvw->dues_class_combo);

    gtk_box_append(GTK_BOX(fvw->content_container), filter_box);

    StudentFeeSummary dues[200];
    int count = 0;
    fee_service_get_pending_dues(fvw->dues_class_id, dues, 200, &count);

    GtkWidget *card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_add_css_class(card, "card");
    gtk_box_append(GTK_BOX(fvw->content_container), card);

    GtkWidget *h_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_add_css_class(h_row, "table-header-row");

    const char *cols[] = { "Admission No", "Roll", "Student Name", "Class", "Assigned Fee", "Paid", "Balance Due", "Action" };
    int widths[] = { 130, 50, 200, 110, 110, 110, 120, 110 };

    for (int i = 0; i < 8; i++) {
        GtkWidget *th = gtk_label_new(cols[i]);
        gtk_widget_add_css_class(th, "table-th");
        gtk_label_set_xalign(GTK_LABEL(th), 0.0f);
        gtk_widget_set_size_request(th, widths[i], -1);
        if (i == 2) gtk_widget_set_hexpand(th, TRUE);
        gtk_box_append(GTK_BOX(h_row), th);
    }
    gtk_box_append(GTK_BOX(card), h_row);

    if (count == 0) {
        GtkWidget *empty_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
        gtk_widget_set_margin_top(empty_box, 30);
        GtkWidget *lbl = gtk_label_new("No outstanding pending dues found.");
        gtk_widget_add_css_class(lbl, "dim-label");
        gtk_box_append(GTK_BOX(empty_box), lbl);
        gtk_box_append(GTK_BOX(card), empty_box);
        return;
    }

    for (int i = 0; i < count; i++) {
        const StudentFeeSummary *s = &dues[i];
        GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
        gtk_widget_add_css_class(row, "table-data-row");

        GtkWidget *adm_lbl = gtk_label_new(s->admission_no);
        gtk_widget_add_css_class(adm_lbl, "badge-adm");
        gtk_widget_set_size_request(adm_lbl, widths[0], -1);
        gtk_label_set_xalign(GTK_LABEL(adm_lbl), 0.0f);
        gtk_box_append(GTK_BOX(row), adm_lbl);

        char r_buf[16];
        snprintf(r_buf, sizeof(r_buf), "%d", s->roll_number);
        GtkWidget *r_lbl = gtk_label_new(r_buf);
        gtk_widget_set_size_request(r_lbl, widths[1], -1);
        gtk_box_append(GTK_BOX(row), r_lbl);

        GtkWidget *n_lbl = gtk_label_new(s->student_name);
        gtk_widget_add_css_class(n_lbl, "table-cell-bold");
        gtk_label_set_xalign(GTK_LABEL(n_lbl), 0.0f);
        gtk_widget_set_hexpand(n_lbl, TRUE);
        gtk_box_append(GTK_BOX(row), n_lbl);

        GtkWidget *c_lbl = gtk_label_new(s->class_name);
        gtk_widget_set_size_request(c_lbl, widths[3], -1);
        gtk_label_set_xalign(GTK_LABEL(c_lbl), 0.0f);
        gtk_box_append(GTK_BOX(row), c_lbl);

        char as_str[32], pd_str[32], due_str[32];
        snprintf(as_str, sizeof(as_str), "₹%.2f", s->total_assigned);
        snprintf(pd_str, sizeof(pd_str), "₹%.2f", s->total_paid);
        snprintf(due_str, sizeof(due_str), "₹%.2f", s->balance_due);

        GtkWidget *as_lbl = gtk_label_new(as_str);
        gtk_widget_set_size_request(as_lbl, widths[4], -1);
        gtk_label_set_xalign(GTK_LABEL(as_lbl), 0.0f);
        gtk_box_append(GTK_BOX(row), as_lbl);

        GtkWidget *pd_lbl = gtk_label_new(pd_str);
        gtk_widget_set_size_request(pd_lbl, widths[5], -1);
        gtk_label_set_xalign(GTK_LABEL(pd_lbl), 0.0f);
        gtk_box_append(GTK_BOX(row), pd_lbl);

        GtkWidget *due_lbl = gtk_label_new(due_str);
        gtk_widget_add_css_class(due_lbl, "badge-status-inactive");
        gtk_widget_set_size_request(due_lbl, widths[6], -1);
        gtk_box_append(GTK_BOX(row), due_lbl);

        GtkWidget *act_btn = gtk_button_new_with_label("Collect");
        gtk_widget_add_css_class(act_btn, "btn-primary");
        gtk_widget_set_size_request(act_btn, widths[7], -1);
        g_object_set_data(G_OBJECT(act_btn), "fvw", fvw);
        g_signal_connect(act_btn, "clicked", G_CALLBACK(on_collect_due_for_student), GINT_TO_POINTER(s->student_id));
        gtk_widget_set_visible(act_btn, auth_current_user_can_access("FEES", true));
        gtk_box_append(GTK_BOX(row), act_btn);

        gtk_box_append(GTK_BOX(card), row);
    }
}

static void refresh_view(FeesViewWidgets *fvw)
{
    gboolean can_edit = auth_current_user_can_access("FEES", true);
    if (fvw->tab_collect_btn) {
        gtk_widget_set_visible(fvw->tab_collect_btn, can_edit);
    }
    if (!can_edit && fvw->current_tab == TAB_COLLECT) {
        fvw->current_tab = TAB_TRANSACTIONS;
        gtk_widget_remove_css_class(fvw->tab_collect_btn, "tab-btn-active");
        gtk_widget_add_css_class(fvw->tab_transactions_btn, "tab-btn-active");
    }
    if (fvw->primary_action_btn) {
        gtk_widget_set_visible(fvw->primary_action_btn, can_edit && fvw->current_tab == TAB_STRUCTURES);
    }
    if (fvw->submit_btn) {
        gtk_widget_set_visible(fvw->submit_btn, can_edit);
    }

    /* Clear content container */
    GtkWidget *c = gtk_widget_get_first_child(fvw->content_container);
    while (c) {
        GtkWidget *next = gtk_widget_get_next_sibling(c);
        gtk_box_remove(GTK_BOX(fvw->content_container), c);
        c = next;
    }

    if (fvw->current_tab == TAB_COLLECT) {
        render_collect_fee_tab(fvw);
    } else if (fvw->current_tab == TAB_TRANSACTIONS) {
        render_transactions_tab(fvw);
    } else if (fvw->current_tab == TAB_STRUCTURES) {
        render_structures_tab(fvw);
    } else {
        render_pending_dues_tab(fvw);
    }
}

/* ── Modal Dialogs ────────────────────────────────────────────────── */

/* 1. Add / Edit Fee Structure Dialog */
typedef struct {
    FeesViewWidgets *fvw;
    int structure_id;
    GtkWidget *dialog;
    GtkWidget *entry_name;
    GtkWidget *combo_class;
    GtkWidget *combo_type;
    GtkWidget *entry_amount;
    GtkWidget *entry_due;
    int class_ids[MAX_CLASSES];
    int class_count;
} StructureDialogData;

static void on_save_structure_dialog(GtkButton *btn, gpointer user_data)
{
    (void)btn;
    StructureDialogData *d = (StructureDialogData *)user_data;

    const char *name = gtk_editable_get_text(GTK_EDITABLE(d->entry_name));
    const char *amt_str = gtk_editable_get_text(GTK_EDITABLE(d->entry_amount));
    const char *due_str = gtk_editable_get_text(GTK_EDITABLE(d->entry_due));

    if (!name || strlen(name) == 0 || !amt_str) return;

    FeeStructure st;
    memset(&st, 0, sizeof(FeeStructure));
    st.id = d->structure_id;
    strncpy(st.name, name, sizeof(st.name) - 1);
    st.amount = atof(amt_str);
    if (due_str) strncpy(st.due_date, due_str, sizeof(st.due_date) - 1);

    guint sel_cls = gtk_drop_down_get_selected(GTK_DROP_DOWN(d->combo_class));
    if ((int)sel_cls < d->class_count) {
        st.class_id = d->class_ids[sel_cls];
    }

    const char *types[] = { FEE_TYPE_MONTHLY, FEE_TYPE_ANNUAL, FEE_TYPE_ADMISSION,
                            FEE_TYPE_TRANSPORT, FEE_TYPE_OTHER };
    guint sel_tp = gtk_drop_down_get_selected(GTK_DROP_DOWN(d->combo_type));
    if (sel_tp <= 4) strncpy(st.fee_type, types[sel_tp], sizeof(st.fee_type) - 1);

    if (d->structure_id > 0) {
        if (fee_structure_update(&st)) {
            show_status_message(d->fvw, "Fee structure updated successfully.", true);
        } else {
            show_status_message(d->fvw, "Failed to update fee structure.", false);
        }
    } else {
        int new_id = 0;
        if (fee_structure_create(&st, &new_id)) {
            show_status_message(d->fvw, "Fee structure created successfully.", true);
        } else {
            show_status_message(d->fvw, "Failed to create fee structure.", false);
        }
    }

    refresh_view(d->fvw);
    school_dialog_close(d->dialog);
}

static void on_cancel_dialog(GtkButton *btn, gpointer user_data)
{
    (void)btn;
    GtkWidget *w = GTK_WIDGET(user_data);
    school_dialog_close(w);
}

static void show_add_edit_structure_dialog(FeesViewWidgets *fvw, int structure_id)
{
    StructureDialogData *d = g_new0(StructureDialogData, 1);
    d->fvw = fvw;
    d->structure_id = structure_id;

    FeeStructure existing;
    bool is_edit = (structure_id > 0 && fee_structure_get_by_id(structure_id, &existing));

    GtkWidget *win = school_dialog_new(is_edit ? "Edit Fee Structure" : "Add Fee Structure", 460, 420);
    d->dialog = win;

    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 16);
    gtk_widget_set_margin_start(main_box, 24);
    gtk_widget_set_margin_end(main_box, 24);
    gtk_widget_set_margin_top(main_box, 20);
    gtk_widget_set_margin_bottom(main_box, 20);
    school_dialog_set_child(win, main_box);

    GtkWidget *title_lbl = gtk_label_new(is_edit ? "Edit Fee Structure Details" : "Create New Fee Structure");
    gtk_widget_add_css_class(title_lbl, "dashboard-card-title");
    gtk_label_set_xalign(GTK_LABEL(title_lbl), 0.0f);
    gtk_box_append(GTK_BOX(main_box), title_lbl);

    GtkWidget *form_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_box_append(GTK_BOX(main_box), form_box);

    /* Name */
    GtkWidget *lbl_name = gtk_label_new("Fee Structure Name (e.g. Monthly Tuition Fee):");
    gtk_label_set_xalign(GTK_LABEL(lbl_name), 0.0f);
    gtk_box_append(GTK_BOX(form_box), lbl_name);
    d->entry_name = gtk_entry_new();
    if (is_edit) gtk_editable_set_text(GTK_EDITABLE(d->entry_name), existing.name);
    gtk_box_append(GTK_BOX(form_box), d->entry_name);

    /* Target Class */
    GtkWidget *lbl_cls = gtk_label_new("Applicable Class:");
    gtk_label_set_xalign(GTK_LABEL(lbl_cls), 0.0f);
    gtk_box_append(GTK_BOX(form_box), lbl_cls);

    SchoolClass classes[MAX_CLASSES];
    int count = 0;
    class_service_get_all(classes, MAX_CLASSES, &count);
    d->class_count = count;

    const char *c_names[MAX_CLASSES + 1];
    int sel_idx = 0;
    for (int i = 0; i < count; i++) {
        c_names[i] = classes[i].name;
        d->class_ids[i] = classes[i].id;
        if (is_edit && classes[i].id == existing.class_id) sel_idx = i;
    }
    c_names[count] = NULL;

    d->combo_class = gtk_drop_down_new_from_strings(c_names);
    if (count > 0) gtk_drop_down_set_selected(GTK_DROP_DOWN(d->combo_class), sel_idx);
    gtk_box_append(GTK_BOX(form_box), d->combo_class);

    /* Fee Type */
    GtkWidget *lbl_tp = gtk_label_new("Fee Category:");
    gtk_label_set_xalign(GTK_LABEL(lbl_tp), 0.0f);
    gtk_box_append(GTK_BOX(form_box), lbl_tp);
    const char *types[] = { "Monthly Tuition", "Annual Development", "Admission / Registration",
                            "Transport / Bus", "Other Charges", NULL };
    d->combo_type = gtk_drop_down_new_from_strings(types);
    gtk_box_append(GTK_BOX(form_box), d->combo_type);

    /* Amount */
    GtkWidget *lbl_amt = gtk_label_new("Standard Amount (₹):");
    gtk_label_set_xalign(GTK_LABEL(lbl_amt), 0.0f);
    gtk_box_append(GTK_BOX(form_box), lbl_amt);
    d->entry_amount = gtk_entry_new();
    char a_buf[32] = "1500.00";
    if (is_edit) snprintf(a_buf, sizeof(a_buf), "%.2f", existing.amount);
    gtk_editable_set_text(GTK_EDITABLE(d->entry_amount), a_buf);
    gtk_box_append(GTK_BOX(form_box), d->entry_amount);

    /* Due Date */
    GtkWidget *lbl_due = gtk_label_new("Due Date (YYYY-MM-DD):");
    gtk_label_set_xalign(GTK_LABEL(lbl_due), 0.0f);
    gtk_box_append(GTK_BOX(form_box), lbl_due);
    d->entry_due = gtk_entry_new();
    if (is_edit) gtk_editable_set_text(GTK_EDITABLE(d->entry_due), existing.due_date);
    gtk_box_append(GTK_BOX(form_box), d->entry_due);

    /* Buttons */
    GtkWidget *btn_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_set_halign(btn_box, GTK_ALIGN_END);
    gtk_widget_set_margin_top(btn_box, 16);

    GtkWidget *cancel_btn = gtk_button_new_with_label("Cancel");
    gtk_widget_add_css_class(cancel_btn, "btn-secondary");
    g_signal_connect(cancel_btn, "clicked", G_CALLBACK(on_cancel_dialog), win);
    gtk_box_append(GTK_BOX(btn_box), cancel_btn);

    GtkWidget *save_btn = gtk_button_new_with_label(is_edit ? "Save Changes" : "Create Structure");
    gtk_widget_add_css_class(save_btn, "btn-primary");
    g_signal_connect(save_btn, "clicked", G_CALLBACK(on_save_structure_dialog), d);
    gtk_box_append(GTK_BOX(btn_box), save_btn);

    gtk_box_append(GTK_BOX(main_box), btn_box);
    g_object_set_data_full(G_OBJECT(win), "d", d, g_free);
    school_dialog_present(win);
}

/* 2. Printable Institutional Receipt Dialog */
static void on_receipt_pdf_done(PrintPdfStatus status, const char *path_or_err, gpointer user_data)
{
    GtkWidget *lbl = GTK_WIDGET(user_data);
    if (!lbl || status == PRINT_PDF_CANCELLED) return;

    if (status == PRINT_PDF_OK) {
        char msg[512];
        snprintf(msg, sizeof(msg), "Receipt PDF downloaded: %s",
                 path_or_err ? path_or_err : "exports/*.pdf");
        gtk_label_set_text(GTK_LABEL(lbl), msg);
        gtk_widget_remove_css_class(lbl, "card-kpi-red");
        gtk_widget_add_css_class(lbl, "card-kpi-green");
        gtk_widget_set_visible(lbl, TRUE);
    } else {
        gtk_label_set_text(GTK_LABEL(lbl), path_or_err ? path_or_err : "Failed to save PDF receipt.");
        gtk_widget_remove_css_class(lbl, "card-kpi-green");
        gtk_widget_add_css_class(lbl, "card-kpi-red");
        gtk_widget_set_visible(lbl, TRUE);
    }
}

static void on_print_receipt_btn_clicked(GtkButton *btn, gpointer user_data)
{
    GtkWidget *win = GTK_WIDGET(user_data);
    const FeePayment *pay = (const FeePayment *)g_object_get_data(G_OBJECT(win), "receipt_data");
    GtkWidget *lbl = GTK_WIDGET(g_object_get_data(G_OBJECT(win), "receipt_status_lbl"));
    char err_msg[256] = {0};

    PrintPdfStatus st = print_pdf_print_receipt(GTK_WIDGET(btn), pay, err_msg, sizeof(err_msg));
    if (!lbl || st == PRINT_PDF_CANCELLED) return;

    if (st == PRINT_PDF_OK) {
        gtk_label_set_text(GTK_LABEL(lbl), "Receipt sent to printer successfully.");
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

static void on_download_receipt_pdf_clicked(GtkButton *btn, gpointer user_data)
{
    GtkWidget *win = GTK_WIDGET(user_data);
    const FeePayment *pay = (const FeePayment *)g_object_get_data(G_OBJECT(win), "receipt_data");
    GtkWidget *lbl = GTK_WIDGET(g_object_get_data(G_OBJECT(win), "receipt_status_lbl"));
    print_pdf_prompt_export_receipt(GTK_WIDGET(btn), pay, on_receipt_pdf_done, lbl);
}

static void show_receipt_dialog(const FeePayment *p)
{
    if (!p) return;

    SchoolSettings settings = {0};
    settings_service_get(&settings);
    const char *sch_name = settings.school_name[0] ? settings.school_name : "APCOACHING Institution";
    const char *curr = settings.currency_symbol[0] ? settings.currency_symbol : "₹";

    GtkWidget *win = school_dialog_new("Fee Payment Receipt", 520, 560);
    FeePayment *pay_copy = g_new(FeePayment, 1);
    *pay_copy = *p;
    g_object_set_data_full(G_OBJECT(win), "receipt_data", pay_copy, g_free);

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

    char sub_line[384];
    snprintf(sub_line, sizeof(sub_line), "%s%s%s\nOfficial Fee Acknowledgment Receipt",
             settings.school_address[0] ? settings.school_address : "Academic Campus",
             settings.school_phone[0] ? " • Tel: " : "",
             settings.school_phone);
    GtkWidget *inst_sub = gtk_label_new(sub_line);
    gtk_widget_add_css_class(inst_sub, "dim-label");
    gtk_label_set_justify(GTK_LABEL(inst_sub), GTK_JUSTIFY_CENTER);
    gtk_box_append(GTK_BOX(box), inst_sub);

    GtkWidget *sep1 = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_append(GTK_BOX(box), sep1);

    /* Receipt Meta */
    GtkWidget *meta_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 16);

    char rcp_str[64];
    snprintf(rcp_str, sizeof(rcp_str), "Receipt No: %s", p->receipt_no);
    GtkWidget *lbl_rcp = gtk_label_new(rcp_str);
    gtk_widget_add_css_class(lbl_rcp, "table-cell-bold");
    gtk_widget_set_hexpand(lbl_rcp, TRUE);
    gtk_label_set_xalign(GTK_LABEL(lbl_rcp), 0.0f);
    gtk_box_append(GTK_BOX(meta_box), lbl_rcp);

    char dt_str[64];
    snprintf(dt_str, sizeof(dt_str), "Date: %s", p->payment_date);
    GtkWidget *lbl_dt = gtk_label_new(dt_str);
    gtk_box_append(GTK_BOX(meta_box), lbl_dt);

    gtk_box_append(GTK_BOX(box), meta_box);

    /* Student Info Box */
    GtkWidget *stu_card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_widget_add_css_class(stu_card, "card");

    char line1[256], line2[256];
    snprintf(line1, sizeof(line1), "Student Name: %s   (Adm No: %s)", p->student_name, p->admission_no);
    snprintf(line2, sizeof(line2), "Class: %s %s   |   Roll Number: %d", p->class_name, p->section_name, p->roll_number);

    GtkWidget *l1 = gtk_label_new(line1);
    gtk_label_set_xalign(GTK_LABEL(l1), 0.0f);
    gtk_box_append(GTK_BOX(stu_card), l1);

    GtkWidget *l2 = gtk_label_new(line2);
    gtk_label_set_xalign(GTK_LABEL(l2), 0.0f);
    gtk_box_append(GTK_BOX(stu_card), l2);

    gtk_box_append(GTK_BOX(box), stu_card);

    /* Payment Particulars */
    GtkWidget *part_card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_add_css_class(part_card, "card");

    char p1[256], p2[128], p3[160], p4[128];
    snprintf(p1, sizeof(p1), "Fee Particulars: %s", p->fee_structure_name);
    snprintf(p2, sizeof(p2), "Payment Mode: %s %s", p->payment_method,
             strlen(p->transaction_ref) > 0 ? p->transaction_ref : "");
    snprintf(p3, sizeof(p3), "Base Amount: %s%.2f   |   Discount: %s%.2f   |   Fine: %s%.2f",
             curr, p->amount_paid, curr, p->discount, curr, p->late_fine);
    snprintf(p4, sizeof(p4), "TOTAL AMOUNT PAID: %s%.2f", curr, p->total_amount);

    GtkWidget *lp1 = gtk_label_new(p1);
    gtk_label_set_xalign(GTK_LABEL(lp1), 0.0f);
    gtk_box_append(GTK_BOX(part_card), lp1);

    GtkWidget *lp2 = gtk_label_new(p2);
    gtk_label_set_xalign(GTK_LABEL(lp2), 0.0f);
    gtk_box_append(GTK_BOX(part_card), lp2);

    GtkWidget *lp3 = gtk_label_new(p3);
    gtk_label_set_xalign(GTK_LABEL(lp3), 0.0f);
    gtk_box_append(GTK_BOX(part_card), lp3);

    GtkWidget *sep2 = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_append(GTK_BOX(part_card), sep2);

    GtkWidget *lp4 = gtk_label_new(p4);
    gtk_widget_add_css_class(lp4, "dashboard-card-title");
    gtk_label_set_xalign(GTK_LABEL(lp4), 0.0f);
    gtk_box_append(GTK_BOX(part_card), lp4);

    gtk_box_append(GTK_BOX(box), part_card);

    /* Sign-off */
    GtkWidget *sign_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 16);
    gtk_widget_set_margin_top(sign_box, 12);

    char cash_str[256];
    snprintf(cash_str, sizeof(cash_str), "Authorized By: %s", p->collected_by_name);
    GtkWidget *cash_lbl = gtk_label_new(cash_str);
    gtk_widget_add_css_class(cash_lbl, "dim-label");
    gtk_widget_set_hexpand(cash_lbl, TRUE);
    gtk_label_set_xalign(GTK_LABEL(cash_lbl), 0.0f);
    gtk_box_append(GTK_BOX(sign_box), cash_lbl);

    GtkWidget *seal_lbl = gtk_label_new("[ PAID & VERIFIED ]");
    gtk_widget_add_css_class(seal_lbl, "badge-status-active");
    gtk_box_append(GTK_BOX(sign_box), seal_lbl);

    gtk_box_append(GTK_BOX(box), sign_box);

    /* Inline Print/PDF Status Feedback */
    GtkWidget *lbl_status = gtk_label_new("");
    gtk_widget_add_css_class(lbl_status, "card");
    gtk_label_set_wrap(GTK_LABEL(lbl_status), TRUE);
    gtk_widget_set_visible(lbl_status, FALSE);
    g_object_set_data(G_OBJECT(win), "receipt_status_lbl", lbl_status);
    gtk_box_append(GTK_BOX(box), lbl_status);

    /* Action Buttons: Print, Download PDF, Close */
    GtkWidget *btn_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_set_halign(btn_box, GTK_ALIGN_END);
    gtk_widget_set_margin_top(btn_box, 12);

    GtkWidget *print_btn = gtk_button_new_with_label("Print Receipt");
    gtk_widget_add_css_class(print_btn, "btn-secondary");
    g_signal_connect(print_btn, "clicked", G_CALLBACK(on_print_receipt_btn_clicked), win);
    gtk_box_append(GTK_BOX(btn_box), print_btn);

    GtkWidget *pdf_btn = gtk_button_new_with_label("Download PDF");
    gtk_widget_add_css_class(pdf_btn, "btn-secondary");
    g_signal_connect(pdf_btn, "clicked", G_CALLBACK(on_download_receipt_pdf_clicked), win);
    gtk_box_append(GTK_BOX(btn_box), pdf_btn);

    GtkWidget *close_btn = gtk_button_new_with_label("Close Receipt");
    gtk_widget_add_css_class(close_btn, "btn-primary");
    g_signal_connect(close_btn, "clicked", G_CALLBACK(on_cancel_dialog), win);
    gtk_box_append(GTK_BOX(btn_box), close_btn);

    gtk_box_append(GTK_BOX(box), btn_box);
    school_dialog_present(win);
}

/* ── Main View Constructor ────────────────────────────────────────── */

GtkWidget *school_fees_view_new(void)
{
    FeesViewWidgets *fvw = g_new0(FeesViewWidgets, 1);
    fvw->current_tab = TAB_COLLECT;

    /* Load initial classes */
    fvw->class_count = 0;
    class_service_get_all(fvw->classes, MAX_CLASSES, &fvw->class_count);

    GtkWidget *scrolled = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
                                  GTK_POLICY_AUTOMATIC,
                                  GTK_POLICY_AUTOMATIC);

    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 20);
    gtk_widget_add_css_class(box, "page-container");
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled), box);
    fvw->root_box = box;
    g_object_set_data_full(G_OBJECT(scrolled), "fvw", fvw, g_free);

    /* ── Header Row ───────────────────────────────────────────────── */
    GtkWidget *top_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 16);
    gtk_widget_set_valign(top_row, GTK_ALIGN_CENTER);

    GtkWidget *title_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    GtkWidget *title = gtk_label_new("Fee Management");
    gtk_widget_add_css_class(title, "page-title");
    gtk_label_set_xalign(GTK_LABEL(title), 0.0f);
    gtk_box_append(GTK_BOX(title_box), title);

    GtkWidget *sub = gtk_label_new("Collect fees, manage structures, issue receipts, and track pending dues");
    gtk_widget_add_css_class(sub, "page-subtitle");
    gtk_label_set_xalign(GTK_LABEL(sub), 0.0f);
    gtk_box_append(GTK_BOX(title_box), sub);

    gtk_widget_set_hexpand(title_box, TRUE);
    gtk_box_append(GTK_BOX(top_row), title_box);

    /* Primary Action Button (used on Structures tab) */
    fvw->primary_action_btn = gtk_button_new_with_label("+ Add Fee Structure");
    gtk_widget_add_css_class(fvw->primary_action_btn, "btn-primary");
    gtk_widget_set_visible(fvw->primary_action_btn, FALSE);
    g_signal_connect(fvw->primary_action_btn, "clicked", G_CALLBACK(on_primary_action_clicked), fvw);
    gtk_box_append(GTK_BOX(top_row), fvw->primary_action_btn);

    gtk_box_append(GTK_BOX(box), top_row);

    /* ── Status Banner ───────────────────────────────────────────── */
    fvw->status_banner = gtk_label_new("");
    gtk_widget_add_css_class(fvw->status_banner, "card");
    gtk_widget_set_visible(fvw->status_banner, FALSE);
    gtk_box_append(GTK_BOX(box), fvw->status_banner);

    /* ── Tab Switcher ─────────────────────────────────────────────── */
    GtkWidget *tab_bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);

    fvw->tab_collect_btn = gtk_button_new_with_label("Collect Fee");
    gtk_widget_add_css_class(fvw->tab_collect_btn, "tab-btn");
    gtk_widget_add_css_class(fvw->tab_collect_btn, "tab-btn-active");
    g_signal_connect(fvw->tab_collect_btn, "clicked", G_CALLBACK(on_tab_collect_clicked), fvw);
    gtk_box_append(GTK_BOX(tab_bar), fvw->tab_collect_btn);

    fvw->tab_transactions_btn = gtk_button_new_with_label("Payment Transactions");
    gtk_widget_add_css_class(fvw->tab_transactions_btn, "tab-btn");
    g_signal_connect(fvw->tab_transactions_btn, "clicked", G_CALLBACK(on_tab_transactions_clicked), fvw);
    gtk_box_append(GTK_BOX(tab_bar), fvw->tab_transactions_btn);

    fvw->tab_structures_btn = gtk_button_new_with_label("Fee Structures");
    gtk_widget_add_css_class(fvw->tab_structures_btn, "tab-btn");
    g_signal_connect(fvw->tab_structures_btn, "clicked", G_CALLBACK(on_tab_structures_clicked), fvw);
    gtk_box_append(GTK_BOX(tab_bar), fvw->tab_structures_btn);

    fvw->tab_dues_btn = gtk_button_new_with_label("Pending Dues Ledger");
    gtk_widget_add_css_class(fvw->tab_dues_btn, "tab-btn");
    g_signal_connect(fvw->tab_dues_btn, "clicked", G_CALLBACK(on_tab_dues_clicked), fvw);
    gtk_box_append(GTK_BOX(tab_bar), fvw->tab_dues_btn);

    gtk_box_append(GTK_BOX(box), tab_bar);

    /* ── Content Container ───────────────────────────────────────── */
    fvw->content_container = gtk_box_new(GTK_ORIENTATION_VERTICAL, 16);
    gtk_box_append(GTK_BOX(box), fvw->content_container);

    refresh_view(fvw);

    return scrolled;
}

void school_fees_view_reload(GtkWidget *view)
{
    if (!view) return;
    FeesViewWidgets *fvw = (FeesViewWidgets *)g_object_get_data(G_OBJECT(view), "fvw");
    if (fvw) {
        fvw->class_count = 0;
        class_service_get_all(fvw->classes, MAX_CLASSES, &fvw->class_count);
        refresh_view(fvw);
    }
}
