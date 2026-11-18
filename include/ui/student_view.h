/**
 * @file student_view.h
 * @brief Student management GUI module with search, filters, pagination, and CRUD dialogs.
 */

#ifndef SCHOOLCMS_UI_STUDENT_VIEW_H
#define SCHOOLCMS_UI_STUDENT_VIEW_H

#include <gtk/gtk.h>

/**
 * @brief Create the student management page widget.
 * @return GtkWidget* representing the student view.
 */
GtkWidget *school_student_view_new(void);

/**
 * @brief Reload the student list matching the current search & filter state.
 * @param student_view The widget returned by school_student_view_new.
 */
void school_student_view_reload(GtkWidget *student_view);

#endif /* SCHOOLCMS_UI_STUDENT_VIEW_H */
