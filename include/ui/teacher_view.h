/**
 * @file teacher_view.h
 * @brief Teacher and staff management GUI module with search, filter, and CRUD dialogs.
 */

#ifndef SCHOOLCMS_UI_TEACHER_VIEW_H
#define SCHOOLCMS_UI_TEACHER_VIEW_H

#include <gtk/gtk.h>

/**
 * @brief Create the teacher and staff management page widget.
 * @return GtkWidget* representing the teacher view.
 */
GtkWidget *school_teacher_view_new(void);

/**
 * @brief Reload the active list matching search & filter criteria.
 * @param teacher_view The widget returned by school_teacher_view_new.
 */
void school_teacher_view_reload(GtkWidget *teacher_view);

#endif /* SCHOOLCMS_UI_TEACHER_VIEW_H */
