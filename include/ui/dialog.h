/**
 * @file dialog.h
 * @brief In-app centered modal dialog overlay bounded within the main window.
 *
 * Replaces unparented external GtkWindow popups with an in-window modal overlay
 * card that remains centered and clamped within the application window bounds,
 * scrolling automatically when the window is resized.
 */

#ifndef SCHOOLCMS_UI_DIALOG_H
#define SCHOOLCMS_UI_DIALOG_H

#include <gtk/gtk.h>

/**
 * @brief Initialize the in-app modal dialog overlay host on the main window.
 * @param window       Top-level application GtkWindow.
 * @param overlay      Root GtkOverlay containing the main app content.
 * @param base_widget  Underlying main UI widget (root_stack) to block while modal is active.
 */
void school_dialog_init(GtkWindow *window, GtkOverlay *overlay, GtkWidget *base_widget);

/**
 * @brief Create a new modal dialog bounded inside the main application window.
 * @param title             Header title displayed at the top of the dialog card.
 * @param preferred_width   Ideal width in logical pixels (clamped to window width).
 * @param preferred_height  Ideal body height in logical pixels (clamped to window height).
 * @return GtkWidget* handle representing the dialog backdrop container.
 */
GtkWidget *school_dialog_new(const char *title, int preferred_width, int preferred_height);

/**
 * @brief Set the content widget inside the dialog's scrollable body.
 * @param dialog  Handle returned by school_dialog_new().
 * @param child   Content widget tree to display inside the dialog.
 */
void school_dialog_set_child(GtkWidget *dialog, GtkWidget *child);

/**
 * @brief Present the modal dialog centered inside the application window.
 * @param dialog  Handle returned by school_dialog_new().
 */
void school_dialog_present(GtkWidget *dialog);

/**
 * @brief Close and destroy an open modal dialog, restoring underlying focus.
 *        Safe to connect directly via g_signal_connect_swapped(..., school_dialog_close, dlg).
 * @param dialog  Handle returned by school_dialog_new().
 */
void school_dialog_close(GtkWidget *dialog);

/**
 * @brief Close all currently open in-app modal dialogs (e.g. on logout).
 */
void school_dialog_close_all(void);

#endif /* SCHOOLCMS_UI_DIALOG_H */
