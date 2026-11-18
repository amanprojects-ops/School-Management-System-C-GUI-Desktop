/**
 * @file dashboard.h
 * @brief Dashboard page with summary cards, quick actions,
 *        and placeholder sections for charts and tables.
 */

#ifndef SCHOOLCMS_UI_DASHBOARD_H
#define SCHOOLCMS_UI_DASHBOARD_H

#include <gtk/gtk.h>

/**
 * @brief Create the dashboard page widget.
 *
 * Returns a scrollable container with stat cards, quick-action
 * buttons, and placeholder areas for charts and recent data.
 * @return A GtkWidget* for the dashboard page.
 */
GtkWidget *school_dashboard_new(void);

#endif /* SCHOOLCMS_UI_DASHBOARD_H */
