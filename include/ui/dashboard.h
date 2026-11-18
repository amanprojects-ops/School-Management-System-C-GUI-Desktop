/**
 * @file dashboard.h
 * @brief Dashboard page with summary cards, quick actions,
 *        and placeholder sections for charts and tables.
 */

#ifndef SCHOOLCMS_UI_DASHBOARD_H
#define SCHOOLCMS_UI_DASHBOARD_H

#include <gtk/gtk.h>

/**
 * @brief Create the dashboard page widget with dynamic cards, Cairo charts, and tables.
 * @return GtkWidget* for the dashboard page.
 */
GtkWidget *school_dashboard_new(void);

/**
 * @brief Refresh dashboard statistics and redraw charts.
 * @param dashboard_widget The widget returned by school_dashboard_new.
 */
void school_dashboard_refresh(GtkWidget *dashboard_widget);

#endif /* SCHOOLCMS_UI_DASHBOARD_H */
