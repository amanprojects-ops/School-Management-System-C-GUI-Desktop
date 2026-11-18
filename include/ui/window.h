/**
 * @file window.h
 * @brief Main application window construction.
 *
 * Assembles the top-level window with sidebar, header, and
 * content area (initially the dashboard).
 */

#ifndef SCHOOLCMS_UI_WINDOW_H
#define SCHOOLCMS_UI_WINDOW_H

#include <gtk/gtk.h>

/** Minimum window dimensions. */
#define WINDOW_MIN_WIDTH  1100
#define WINDOW_MIN_HEIGHT 700

/** Default window dimensions. */
#define WINDOW_DEFAULT_WIDTH  1366
#define WINDOW_DEFAULT_HEIGHT 768

/**
 * @brief Navigation page identifiers.
 *
 * Used by the sidebar to tell the content area which page to show.
 */
typedef enum {
    NAV_DASHBOARD = 0,
    NAV_STUDENTS,
    NAV_TEACHERS,
    NAV_CLASSES,
    NAV_ATTENDANCE,
    NAV_FEES,
    NAV_EXAMS,
    NAV_TIMETABLE,
    NAV_REPORTS,
    NAV_SETTINGS,
    NAV_PAGE_COUNT
} NavPage;

/**
 * @brief Build and present the main application window.
 * @param app  The owning GtkApplication.
 */
void school_window_new(GtkApplication *app);

/**
 * @brief Navigate the content area to the given page.
 * @param page  The target NavPage.
 */
void school_window_navigate(NavPage page);

/**
 * @brief Get the main content GtkStack.
 * @return The GtkStack used for page switching, or NULL before window creation.
 */
GtkStack *school_window_get_content_stack(void);

#endif /* SCHOOLCMS_UI_WINDOW_H */
