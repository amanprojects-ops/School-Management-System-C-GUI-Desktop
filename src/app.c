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
static void init_runtime_environment(void);
static void setup_icon_theme(void);

/* ── Public API ───────────────────────────────────────────────────── */

GtkApplication *school_app_new(void)
{
    init_runtime_environment();

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
    char *pkg_dir = g_win32_get_package_installation_directory_of_module(NULL);

    /*
     * Try several locations for main.css:
     *   1. <pkg_dir>/assets/styles/main.css
     *   2. <pkg_dir>/../assets/styles/main.css
     *   3. relative paths for development runs
     */
    char *path1 = pkg_dir ? g_build_filename(pkg_dir, "assets", "styles", "main.css", NULL) : NULL;
    char *path2 = pkg_dir ? g_build_filename(pkg_dir, "..", "assets", "styles", "main.css", NULL) : NULL;

    const char *paths[] = {
        path1,
        path2,
        "assets/styles/main.css",
        "../assets/styles/main.css",
        "../../assets/styles/main.css",
        NULL
    };

    gboolean loaded = FALSE;
    for (int i = 0; paths[i] != NULL; i++) {
        if (!paths[i]) continue;
        GFile *file = g_file_new_for_path(paths[i]);
        if (g_file_query_exists(file, NULL)) {
            gtk_css_provider_load_from_file(provider, file);
            gtk_style_context_add_provider_for_display(
                gdk_display_get_default(),
                GTK_STYLE_PROVIDER(provider),
                GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
            );
            g_object_unref(file);
            loaded = TRUE;
            break;
        }
        g_object_unref(file);
    }

    if (path1) g_free(path1);
    if (path2) g_free(path2);
    if (pkg_dir) g_free(pkg_dir);

    if (!loaded) {
        g_printerr("Warning: Could not find main.css stylesheet.\n");
        g_object_unref(provider);
    }
}

/* ── Private ──────────────────────────────────────────────────────── */

static void init_runtime_environment(void)
{
    char *pkg_dir = g_win32_get_package_installation_directory_of_module(NULL);

    /* 1. Ensure GDK_PIXBUF_MODULE_FILE is located for SVG/PNG loaders */
    char *pkg_pixbuf = pkg_dir ? g_build_filename(pkg_dir, "lib", "gdk-pixbuf-2.0", "2.10.0", "loaders.cache", NULL) : NULL;
    char *pkg_pixbuf_parent = pkg_dir ? g_build_filename(pkg_dir, "..", "lib", "gdk-pixbuf-2.0", "2.10.0", "loaders.cache", NULL) : NULL;

    const char *pixbuf_paths[] = {
        pkg_pixbuf,
        pkg_pixbuf_parent,
        "lib/gdk-pixbuf-2.0/2.10.0/loaders.cache",
        "../lib/gdk-pixbuf-2.0/2.10.0/loaders.cache",
        "C:/msys64/ucrt64/lib/gdk-pixbuf-2.0/2.10.0/loaders.cache",
        NULL
    };

    for (int i = 0; pixbuf_paths[i]; i++) {
        if (pixbuf_paths[i] && g_file_test(pixbuf_paths[i], G_FILE_TEST_EXISTS)) {
            g_setenv("GDK_PIXBUF_MODULE_FILE", pixbuf_paths[i], FALSE);
            break;
        }
    }

    if (pkg_pixbuf) g_free(pkg_pixbuf);
    if (pkg_pixbuf_parent) g_free(pkg_pixbuf_parent);

    /* 2. Configure XDG_DATA_DIRS and GSETTINGS_SCHEMA_DIR */
    if (pkg_dir) {
        char *share_dir = g_build_filename(pkg_dir, "share", NULL);
        if (g_file_test(share_dir, G_FILE_TEST_IS_DIR)) {
            const char *cur_xdg = g_getenv("XDG_DATA_DIRS");
            if (cur_xdg) {
                char *new_xdg = g_strdup_printf("%s;%s", share_dir, cur_xdg);
                g_setenv("XDG_DATA_DIRS", new_xdg, TRUE);
                g_free(new_xdg);
            } else {
                g_setenv("XDG_DATA_DIRS", share_dir, TRUE);
            }
        }
        g_free(share_dir);

        char *schema_dir = g_build_filename(pkg_dir, "share", "glib-2.0", "schemas", NULL);
        if (g_file_test(schema_dir, G_FILE_TEST_IS_DIR)) {
            g_setenv("GSETTINGS_SCHEMA_DIR", schema_dir, FALSE);
        }
        g_free(schema_dir);

        g_free(pkg_dir);
    }
}

static void setup_icon_theme(void)
{
    /* Set global icon theme name via GtkSettings */
    GtkSettings *settings = gtk_settings_get_default();
    if (settings) {
        g_object_set(settings, "gtk-icon-theme-name", "Adwaita", NULL);
    }

    /* Configure search paths on display icon theme */
    GdkDisplay *display = gdk_display_get_default();
    if (display) {
        GtkIconTheme *theme = gtk_icon_theme_get_for_display(display);
        if (theme) {
            char *pkg_dir = g_win32_get_package_installation_directory_of_module(NULL);
            if (pkg_dir) {
                char *pkg_icons = g_build_filename(pkg_dir, "share", "icons", NULL);
                char *pkg_icons_p = g_build_filename(pkg_dir, "..", "share", "icons", NULL);
                char *pkg_assets = g_build_filename(pkg_dir, "assets", "icons", NULL);
                char *pkg_assets_p = g_build_filename(pkg_dir, "..", "assets", "icons", NULL);

                gtk_icon_theme_add_search_path(theme, pkg_icons);
                gtk_icon_theme_add_search_path(theme, pkg_icons_p);
                gtk_icon_theme_add_search_path(theme, pkg_assets);
                gtk_icon_theme_add_search_path(theme, pkg_assets_p);

                g_free(pkg_icons);
                g_free(pkg_icons_p);
                g_free(pkg_assets);
                g_free(pkg_assets_p);
                g_free(pkg_dir);
            }

            gtk_icon_theme_add_search_path(theme, "share/icons");
            gtk_icon_theme_add_search_path(theme, "../share/icons");
            gtk_icon_theme_add_search_path(theme, "assets/icons");
            gtk_icon_theme_add_search_path(theme, "../assets/icons");
            gtk_icon_theme_add_search_path(theme, "C:/msys64/ucrt64/share/icons");
        }
    }
}

static void on_activate(GtkApplication *app, gpointer user_data)
{
    (void)user_data;

    /* Initialize SQLite database & migrations */
    if (db_init(NULL) != DB_SUCCESS) {
        g_printerr("Failed to initialize database!\n");
    }

    /* Initialize authentication subsystem */
    auth_init();

    setup_icon_theme();
    school_app_load_css();
    school_window_new(app);
}
