/**
 * @file app.c
 * @brief GtkApplication creation, CSS loading, and activate handler.
 */

#include "app.h"
#include "database.h"
#include "auth/auth.h"
#include "ui/window.h"
#include <stdio.h>

/* ── Forward declarations ─────────────────────────────────────────── */

static void on_activate(GtkApplication *app, gpointer user_data);

/* ── Public API ───────────────────────────────────────────────────── */

GtkApplication *school_app_new(void)
{
    GtkApplication *app = gtk_application_new(
        "com.schoolcms.app",
        G_APPLICATION_DEFAULT_FLAGS
    );
    g_signal_connect(app, "activate", G_CALLBACK(on_activate), NULL);
    return app;
}

void school_app_load_css(void)
{
    GtkCssProvider *provider = gtk_css_provider_new();

    /*
     * Try several locations for main.css:
     *   1. assets/styles/main.css  (relative — works in dev)
     *   2. <exe_dir>/assets/styles/main.css  (installed / post-build copy)
     */
    const char *paths[] = {
        "assets/styles/main.css",
        NULL  /* sentinel */
    };

    for (int i = 0; paths[i] != NULL; i++) {
        GFile *file = g_file_new_for_path(paths[i]);
        if (g_file_query_exists(file, NULL)) {
            gtk_css_provider_load_from_file(provider, file);
            gtk_style_context_add_provider_for_display(
                gdk_display_get_default(),
                GTK_STYLE_PROVIDER(provider),
                GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
            );
            g_object_unref(file);
            g_print("CSS loaded from: %s\n", paths[i]);
            return;
        }
        g_object_unref(file);
    }

    g_printerr("Warning: Could not find main.css stylesheet.\n");
    g_object_unref(provider);
}

/* ── Private ──────────────────────────────────────────────────────── */

static void on_activate(GtkApplication *app, gpointer user_data)
{
    (void)user_data;

    /* Initialize SQLite database & migrations */
    if (db_init(NULL) != DB_SUCCESS) {
        g_printerr("Failed to initialize database!\n");
    }

    /* Initialize authentication subsystem */
    auth_init();

    school_app_load_css();
    school_window_new(app);
}
