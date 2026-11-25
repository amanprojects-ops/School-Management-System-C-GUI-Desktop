/**
 * @file dialog.c
 * @brief In-app centered modal dialog overlay engine implementation.
 */

#include "ui/dialog.h"
#include <stdbool.h>

typedef struct {
    GtkWidget *backdrop;
    GtkWidget *card;
    GtkWidget *header_box;
    GtkWidget *close_btn;
    GtkWidget *body_scroll;
    int pref_width;
    int pref_height;
    int last_target_w;
    int last_avail_w;
    int last_avail_h;
    bool presented;
    bool closed;
} SchoolDialogData;

/* ── Host State ───────────────────────────────────────────────────── */

static GtkWindow *host_window = NULL;
static GtkOverlay *host_overlay = NULL;
static GtkWidget *host_base_widget = NULL;
static GList *active_dialogs = NULL;
static bool surface_hooked = false;

/* ── Helpers ──────────────────────────────────────────────────────── */

static void sync_dialog_bounds(SchoolDialogData *sd, int overlay_w, int overlay_h)
{
    if (!sd || !sd->body_scroll || sd->closed) return;

    if (overlay_w <= 0 || overlay_h <= 0) {
        if (host_window) {
            gtk_window_get_default_size(host_window, &overlay_w, &overlay_h);
        }
        if (overlay_w <= 0) overlay_w = 1000;
        if (overlay_h <= 0) overlay_h = 650;
    }

    /* Keep comfortable margin from window edges and reserve space for dialog header */
    int avail_w = overlay_w - 48;
    int avail_h = overlay_h - 108;
    if (avail_w < 260) avail_w = 260;
    if (avail_h < 180) avail_h = 180;

    int target_w = (sd->pref_width > 0) ? MIN(sd->pref_width, avail_w) : MIN(460, avail_w);

    if (sd->last_target_w == target_w &&
        sd->last_avail_w == avail_w &&
        sd->last_avail_h == avail_h) {
        return;
    }

    sd->last_target_w = target_w;
    sd->last_avail_w = avail_w;
    sd->last_avail_h = avail_h;

    gtk_scrolled_window_set_min_content_width(GTK_SCROLLED_WINDOW(sd->body_scroll), target_w);
    gtk_scrolled_window_set_max_content_width(GTK_SCROLLED_WINDOW(sd->body_scroll), avail_w);

    /* Large dialogs/sheets with expanding inner lists request up to clamped height;
     * compact form & confirmation dialogs hug content cleanly and scroll if squeezed. */
    int min_h = (sd->pref_height >= 500) ? MIN(sd->pref_height, avail_h) : -1;
    gtk_scrolled_window_set_min_content_height(GTK_SCROLLED_WINDOW(sd->body_scroll), min_h);
    gtk_scrolled_window_set_max_content_height(GTK_SCROLLED_WINDOW(sd->body_scroll), avail_h);
}

static void on_surface_layout(GdkSurface *surface, int width, int height, gpointer user_data)
{
    (void)surface;
    (void)user_data;

    if (!host_overlay || !active_dialogs) return;

    int ow = gtk_widget_get_width(GTK_WIDGET(host_overlay));
    int oh = gtk_widget_get_height(GTK_WIDGET(host_overlay));
    if (ow <= 0) ow = width;
    if (oh <= 0) oh = height;

    for (GList *l = active_dialogs; l != NULL; l = l->next) {
        SchoolDialogData *sd = (SchoolDialogData *)l->data;
        sync_dialog_bounds(sd, ow, oh);
    }
}

static void attach_surface_listener(void)
{
    if (surface_hooked || !host_window) return;
    GtkNative *native = gtk_widget_get_native(GTK_WIDGET(host_window));
    if (!native) return;
    GdkSurface *surface = gtk_native_get_surface(native);
    if (!surface) return;

    g_signal_connect_after(surface, "layout", G_CALLBACK(on_surface_layout), NULL);
    surface_hooked = true;
}

static void on_host_window_realize(GtkWidget *widget, gpointer user_data)
{
    (void)widget;
    (void)user_data;
    attach_surface_listener();
}

static gboolean on_host_key_pressed(GtkEventControllerKey *controller,
                                    guint keyval,
                                    guint keycode,
                                    GdkModifierType state,
                                    gpointer user_data)
{
    (void)controller;
    (void)keycode;
    (void)state;
    (void)user_data;

    if (keyval == GDK_KEY_Escape && active_dialogs != NULL) {
        GList *last = g_list_last(active_dialogs);
        if (last && last->data) {
            SchoolDialogData *top = (SchoolDialogData *)last->data;
            school_dialog_close(top->backdrop);
            return TRUE;
        }
    }
    return FALSE;
}

static void on_close_btn_clicked(GtkButton *btn, gpointer user_data)
{
    (void)btn;
    GtkWidget *dialog = GTK_WIDGET(user_data);
    school_dialog_close(dialog);
}

/* ── Public API ───────────────────────────────────────────────────── */

void school_dialog_init(GtkWindow *window, GtkOverlay *overlay, GtkWidget *base_widget)
{
    host_window = window;
    host_overlay = overlay;
    host_base_widget = base_widget;
    active_dialogs = NULL;
    surface_hooked = false;

    if (host_window) {
        GtkEventController *key_ctrl = gtk_event_controller_key_new();
        gtk_event_controller_set_propagation_phase(key_ctrl, GTK_PHASE_BUBBLE);
        g_signal_connect(key_ctrl, "key-pressed", G_CALLBACK(on_host_key_pressed), NULL);
        gtk_widget_add_controller(GTK_WIDGET(host_window), key_ctrl);

        if (gtk_widget_get_realized(GTK_WIDGET(host_window))) {
            attach_surface_listener();
        } else {
            g_signal_connect(host_window, "realize", G_CALLBACK(on_host_window_realize), NULL);
        }
    }
}

GtkWidget *school_dialog_new(const char *title, int preferred_width, int preferred_height)
{
    SchoolDialogData *sd = g_new0(SchoolDialogData, 1);
    sd->pref_width = preferred_width;
    sd->pref_height = preferred_height;
    sd->last_target_w = -1;
    sd->last_avail_w = -1;
    sd->last_avail_h = -1;

    /* Modal Backdrop covering the entire app window */
    GtkWidget *backdrop = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_add_css_class(backdrop, "dialog-backdrop");
    gtk_widget_set_hexpand(backdrop, TRUE);
    gtk_widget_set_vexpand(backdrop, TRUE);
    gtk_widget_set_halign(backdrop, GTK_ALIGN_FILL);
    gtk_widget_set_valign(backdrop, GTK_ALIGN_FILL);
    gtk_widget_set_can_target(backdrop, TRUE);
    sd->backdrop = backdrop;

    /* Centered Modal Card bounded within the app window */
    GtkWidget *card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_add_css_class(card, "dialog-card");
    gtk_widget_set_overflow(card, GTK_OVERFLOW_HIDDEN);
    gtk_widget_set_halign(card, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(card, GTK_ALIGN_CENTER);
    gtk_widget_set_hexpand(card, TRUE);
    gtk_widget_set_vexpand(card, TRUE);
    gtk_widget_set_margin_start(card, 16);
    gtk_widget_set_margin_end(card, 16);
    gtk_widget_set_margin_top(card, 16);
    gtk_widget_set_margin_bottom(card, 16);
    gtk_box_append(GTK_BOX(backdrop), card);
    sd->card = card;

    /* Header Bar with Title and Close Button */
    GtkWidget *hdr = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_add_css_class(hdr, "dialog-header");
    gtk_widget_set_valign(hdr, GTK_ALIGN_CENTER);
    sd->header_box = hdr;

    GtkWidget *title_lbl = gtk_label_new(title ? title : "");
    gtk_widget_add_css_class(title_lbl, "dialog-title");
    gtk_label_set_xalign(GTK_LABEL(title_lbl), 0.0f);
    gtk_label_set_ellipsize(GTK_LABEL(title_lbl), PANGO_ELLIPSIZE_END);
    gtk_widget_set_hexpand(title_lbl, TRUE);
    gtk_box_append(GTK_BOX(hdr), title_lbl);

    GtkWidget *close_btn = gtk_button_new_from_icon_name("window-close-symbolic");
    gtk_widget_add_css_class(close_btn, "btn-dialog-close");
    gtk_widget_set_tooltip_text(close_btn, "Close (Esc)");
    g_signal_connect(close_btn, "clicked", G_CALLBACK(on_close_btn_clicked), backdrop);
    gtk_box_append(GTK_BOX(hdr), close_btn);
    sd->close_btn = close_btn;

    gtk_box_append(GTK_BOX(card), hdr);

    GtkWidget *sep = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_append(GTK_BOX(card), sep);

    /* Scrollable Body so content never overflows outside the dialog or app */
    GtkWidget *scroll = gtk_scrolled_window_new();
    gtk_widget_add_css_class(scroll, "dialog-body-scroll");
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
                                  GTK_POLICY_AUTOMATIC,
                                  GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_propagate_natural_width(GTK_SCROLLED_WINDOW(scroll), TRUE);
    gtk_scrolled_window_set_propagate_natural_height(GTK_SCROLLED_WINDOW(scroll), TRUE);
    gtk_widget_set_hexpand(scroll, TRUE);
    gtk_widget_set_vexpand(scroll, TRUE);
    gtk_box_append(GTK_BOX(card), scroll);
    sd->body_scroll = scroll;

    g_object_set_data_full(G_OBJECT(backdrop), "sd", sd, g_free);
    return backdrop;
}

void school_dialog_set_child(GtkWidget *dialog, GtkWidget *child)
{
    if (!dialog) return;
    SchoolDialogData *sd = (SchoolDialogData *)g_object_get_data(G_OBJECT(dialog), "sd");
    if (!sd || !sd->body_scroll) return;
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(sd->body_scroll), child);
}

void school_dialog_present(GtkWidget *dialog)
{
    if (!dialog || !host_overlay) return;
    SchoolDialogData *sd = (SchoolDialogData *)g_object_get_data(G_OBJECT(dialog), "sd");
    if (!sd || sd->closed || sd->presented) return;

    attach_surface_listener();

    /* Block keyboard/pointer access to whatever sits below this modal dialog */
    if (active_dialogs == NULL) {
        if (host_base_widget) {
            gtk_widget_set_sensitive(host_base_widget, FALSE);
        }
    } else {
        GList *prev_last = g_list_last(active_dialogs);
        if (prev_last && prev_last->data) {
            SchoolDialogData *prev_sd = (SchoolDialogData *)prev_last->data;
            gtk_widget_set_sensitive(prev_sd->backdrop, FALSE);
        }
    }

    int ow = gtk_widget_get_width(GTK_WIDGET(host_overlay));
    int oh = gtk_widget_get_height(GTK_WIDGET(host_overlay));
    sync_dialog_bounds(sd, ow, oh);

    sd->presented = true;
    active_dialogs = g_list_append(active_dialogs, sd);
    gtk_overlay_add_overlay(host_overlay, sd->backdrop);

    if (!gtk_widget_child_focus(sd->body_scroll, GTK_DIR_TAB_FORWARD)) {
        gtk_widget_grab_focus(sd->close_btn);
    }
}

void school_dialog_close(GtkWidget *dialog)
{
    if (!dialog) return;
    SchoolDialogData *sd = (SchoolDialogData *)g_object_get_data(G_OBJECT(dialog), "sd");
    if (!sd || sd->closed) return;

    sd->closed = true;
    active_dialogs = g_list_remove(active_dialogs, sd);

    /* Unblock the next layer down */
    if (active_dialogs == NULL) {
        if (host_base_widget) {
            gtk_widget_set_sensitive(host_base_widget, TRUE);
        }
    } else {
        GList *new_last = g_list_last(active_dialogs);
        if (new_last && new_last->data) {
            SchoolDialogData *top = (SchoolDialogData *)new_last->data;
            gtk_widget_set_sensitive(top->backdrop, TRUE);
        }
    }

    if (sd->presented && host_overlay) {
        gtk_overlay_remove_overlay(host_overlay, dialog);
    } else {
        g_object_ref_sink(dialog);
        g_object_unref(dialog);
    }

    /* Restore focus to remaining modal or underlying window */
    if (active_dialogs != NULL) {
        GList *new_last = g_list_last(active_dialogs);
        if (new_last && new_last->data) {
            SchoolDialogData *top = (SchoolDialogData *)new_last->data;
            if (!gtk_widget_child_focus(top->body_scroll, GTK_DIR_TAB_FORWARD)) {
                gtk_widget_grab_focus(top->close_btn);
            }
        }
    }
}

void school_dialog_close_all(void)
{
    while (active_dialogs != NULL) {
        GList *last = g_list_last(active_dialogs);
        SchoolDialogData *sd = (SchoolDialogData *)last->data;
        school_dialog_close(sd->backdrop);
    }
}
