/**
 * @file timetable_view.h
 * @brief Class and teacher timetable grid GUI module for SchoolCMS.
 */

#ifndef SCHOOLCMS_UI_TIMETABLE_VIEW_H
#define SCHOOLCMS_UI_TIMETABLE_VIEW_H

#include <gtk/gtk.h>

/**
 * @brief Create the timetable management view widget.
 * @return GtkWidget* representing the timetable view.
 */
GtkWidget *school_timetable_view_new(void);

/**
 * @brief Reload timetable slots, classes, and teachers in the view.
 * @param view The widget returned by school_timetable_view_new.
 */
void school_timetable_view_reload(GtkWidget *view);

#endif /* SCHOOLCMS_UI_TIMETABLE_VIEW_H */
