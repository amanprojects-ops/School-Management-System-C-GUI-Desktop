/**
 * @file header.h
 * @brief Top header bar with search, notifications, and user profile.
 */

#ifndef SCHOOLCMS_UI_HEADER_H
#define SCHOOLCMS_UI_HEADER_H

#include <gtk/gtk.h>

/**
 * @brief Create the top header bar widget.
 * @return A GtkWidget* representing the header bar.
 */
GtkWidget *school_header_new(void);

/**
 * @brief Update the breadcrumb text shown in the header.
 * @param text  The breadcrumb string (e.g. "Dashboard > Overview").
 */
void school_header_set_breadcrumb(const char *text);

/**
 * @brief Update the displayed user name and role badge.
 * @param full_name User's display name.
 * @param role_name Role name (e.g. "ADMIN", "TEACHER").
 */
void school_header_update_user(const char *full_name, const char *role_name);

#endif /* SCHOOLCMS_UI_HEADER_H */
