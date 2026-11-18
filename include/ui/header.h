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

#endif /* SCHOOLCMS_UI_HEADER_H */
