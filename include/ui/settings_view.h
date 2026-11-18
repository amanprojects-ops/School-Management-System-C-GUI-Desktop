/**
 * @file settings_view.h
 * @brief Institutional configuration, theme switching, and backup/restore GUI view.
 */

#ifndef SCHOOLCMS_UI_SETTINGS_VIEW_H
#define SCHOOLCMS_UI_SETTINGS_VIEW_H

#include <gtk/gtk.h>

/**
 * @brief Create the settings view widget.
 * @return GtkWidget* representing the settings view.
 */
GtkWidget *school_settings_view_new(void);

/**
 * @brief Reload settings values from the database into the view.
 * @param view The widget returned by school_settings_view_new.
 */
void school_settings_view_reload(GtkWidget *view);

#endif /* SCHOOLCMS_UI_SETTINGS_VIEW_H */
