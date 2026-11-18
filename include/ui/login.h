/**
 * @file login.h
 * @brief Modern login view widget for user authentication.
 */

#ifndef SCHOOLCMS_UI_LOGIN_H
#define SCHOOLCMS_UI_LOGIN_H

#include <gtk/gtk.h>

/**
 * @brief Callback invoked when user successfully logs in.
 * @param user_data User data passed to school_login_view_new.
 */
typedef void (*LoginSuccessCallback)(gpointer user_data);

/**
 * @brief Create a new modern login screen widget.
 *
 * @param on_success Callback invoked upon successful authentication.
 * @param user_data Context data passed to the callback.
 * @return GtkWidget* representing the login screen.
 */
GtkWidget *school_login_view_new(LoginSuccessCallback on_success, gpointer user_data);

/**
 * @brief Clear input fields and reset error status on the login view.
 * @param view Login widget.
 */
void school_login_view_reset(GtkWidget *view);

#endif /* SCHOOLCMS_UI_LOGIN_H */
