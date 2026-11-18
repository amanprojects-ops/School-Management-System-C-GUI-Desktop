/**
 * @file fees_view.h
 * @brief Fee collection, structures, receipt preview, and pending dues GUI module.
 */

#ifndef SCHOOLCMS_UI_FEES_VIEW_H
#define SCHOOLCMS_UI_FEES_VIEW_H

#include <gtk/gtk.h>

/**
 * @brief Create the fee management view widget.
 * @return GtkWidget* representing the fee management page.
 */
GtkWidget *school_fees_view_new(void);

/**
 * @brief Reload fee data, structures, and collection registers in the view.
 * @param view The widget returned by school_fees_view_new.
 */
void school_fees_view_reload(GtkWidget *view);

#endif /* SCHOOLCMS_UI_FEES_VIEW_H */
