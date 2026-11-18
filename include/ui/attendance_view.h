/**
 * @file attendance_view.h
 * @brief Attendance management and register GUI view module for SchoolCMS.
 */

#ifndef SCHOOLCMS_UI_ATTENDANCE_VIEW_H
#define SCHOOLCMS_UI_ATTENDANCE_VIEW_H

#include <gtk/gtk.h>

/**
 * @brief Create the attendance management view widget.
 * @return GtkWidget* representing the attendance view.
 */
GtkWidget *school_attendance_view_new(void);

/**
 * @brief Reload attendance data in the view.
 * @param view The widget returned by school_attendance_view_new.
 */
void school_attendance_view_reload(GtkWidget *view);

#endif /* SCHOOLCMS_UI_ATTENDANCE_VIEW_H */
