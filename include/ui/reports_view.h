/**
 * @file reports_view.h
 * @brief GUI module for report generation, table preview, and CSV data export.
 */

#ifndef SCHOOLCMS_UI_REPORTS_VIEW_H
#define SCHOOLCMS_UI_REPORTS_VIEW_H

#include <gtk/gtk.h>

/**
 * @brief Create the reports view widget.
 * @return GtkWidget* representing the reports view.
 */
GtkWidget *school_reports_view_new(void);

/**
 * @brief Reload datasets and filter options in the reports view.
 * @param view The widget returned by school_reports_view_new.
 */
void school_reports_view_reload(GtkWidget *view);

#endif /* SCHOOLCMS_UI_REPORTS_VIEW_H */
