/**
 * @file exams_view.h
 * @brief Examination schedules, marks entry, and printable marksheet GUI module.
 */

#ifndef SCHOOLCMS_UI_EXAMS_VIEW_H
#define SCHOOLCMS_UI_EXAMS_VIEW_H

#include <gtk/gtk.h>

/**
 * @brief Create the examinations view widget.
 * @return GtkWidget* representing the examination management page.
 */
GtkWidget *school_exams_view_new(void);

/**
 * @brief Reload exams, subjects, marks, and rank registers in the view.
 * @param view The widget returned by school_exams_view_new.
 */
void school_exams_view_reload(GtkWidget *view);

#endif /* SCHOOLCMS_UI_EXAMS_VIEW_H */
