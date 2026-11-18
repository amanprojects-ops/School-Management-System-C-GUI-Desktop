/**
 * @file main.c
 * @brief Entry point for the SchoolCMS application.
 *
 * Creates the GtkApplication and runs the main event loop.
 */

#include "app.h"

int main(int argc, char *argv[])
{
    GtkApplication *app = school_app_new();
    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    return status;
}
