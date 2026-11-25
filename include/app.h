/**
 * @file app.h
 * @brief Application initialization and lifecycle management.
 *
 * Provides the main GtkApplication wrapper, CSS loading,
 * and application-level state.
 */

#ifndef SCHOOLCMS_APP_H
#define SCHOOLCMS_APP_H

#include <gtk/gtk.h>

#define APP_NAME "APCOACHING"
#define APP_VERSION "1.1.5"

/** Opaque application state. */
typedef struct _SchoolApp SchoolApp;

/**
 * @brief Create and return a new GtkApplication configured for SchoolCMS.
 *
 * Connects the "activate" signal to build the main window.
 * @return A new GtkApplication instance. The caller owns the reference.
 */
GtkApplication *school_app_new(void);

/**
 * @brief Load the application CSS stylesheet.
 *
 * Searches for assets/styles/main.css relative to the executable
 * and applies it to the default GDK display.
 */
void school_app_load_css(void);

#endif /* SCHOOLCMS_APP_H */
