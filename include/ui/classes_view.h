/**
 * @file classes_view.h
 * @brief Class, section, and subject management GUI module for SchoolCMS.
 */

#ifndef SCHOOLCMS_UI_CLASSES_VIEW_H
#define SCHOOLCMS_UI_CLASSES_VIEW_H

#include <gtk/gtk.h>

/**
 * @brief Create the class & section management page widget.
 * @return GtkWidget* representing the classes view.
 */
GtkWidget *school_classes_view_new(void);

/**
 * @brief Reload class, section, and subject data in the view.
 * @param view The widget returned by school_classes_view_new.
 */
void school_classes_view_reload(GtkWidget *view);

#endif /* SCHOOLCMS_UI_CLASSES_VIEW_H */
