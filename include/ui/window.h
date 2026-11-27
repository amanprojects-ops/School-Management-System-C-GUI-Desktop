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

/** Minimum window dimensions (allows snapping to 50% split on 1280/1366 screens). */
#define WINDOW_MIN_WIDTH  640
#define WINDOW_MIN_HEIGHT 450

/** Default baseline window dimensions. */
#define WINDOW_DEFAULT_WIDTH  1280
#define WINDOW_DEFAULT_HEIGHT 720

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
 * @brief Return the RBAC module identifier for a navigation page, or NULL for Dashboard.
 * @param page  Target NavPage.
 * @return Static module string (e.g. "STUDENTS") or NULL.
 */
const char *school_window_page_module(NavPage page);

/**
 * @brief Get the main content GtkStack.
 * @return The GtkStack used for page switching, or NULL before window creation.
 */
GtkStack *school_window_get_content_stack(void);

/**
 * @brief Transition window to display the login screen.
 */
void school_window_show_login(void);

/**
 * @brief Transition window to display the main application dashboard.
 */
void school_window_show_app(void);

/**
 * @brief Refresh the currently visible content page after background updates.
 */
void school_window_refresh_active_page(void);

#endif /* SCHOOLCMS_UI_WINDOW_H */
