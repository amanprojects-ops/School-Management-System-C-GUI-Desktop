/**
 * @file sidebar.h
 * @brief Left sidebar navigation panel.
 *
 * Creates a vertical navigation bar with icons and labels
 * for each module. Emits navigation requests to the main window.
 */

#ifndef SCHOOLCMS_UI_SIDEBAR_H
#define SCHOOLCMS_UI_SIDEBAR_H

#include <gtk/gtk.h>

/** Width of the sidebar in pixels. */
#define SIDEBAR_WIDTH 240

/**
 * @brief Create the sidebar widget.
 * @return A GtkWidget* representing the sidebar. Caller does NOT own a ref.
 */
GtkWidget *school_sidebar_new(void);

/**
 * @brief Highlight the sidebar item for the given page index.
 * @param page_index  Zero-based index matching NavPage enum.
 */
void school_sidebar_set_active(int page_index);

#endif /* SCHOOLCMS_UI_SIDEBAR_H */
