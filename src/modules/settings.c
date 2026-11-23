/**
 * @file settings.c
 * @brief Institutional settings management, theme config, and backup/restore service implementation.
 */

#include "settings.h"
#include "database.h"
#include "classes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <time.h>

#ifdef _WIN32
#include <direct.h>
#define MKDIR(d) _mkdir(d)
#else
#define MKDIR(d) mkdir(d, 0755)
#endif

bool settings_service_get_value(const char *key, char *out_val, size_t val_size)
{
    if (!key || !out_val || val_size == 0) return false;
    out_val[0] = '\0';

    sqlite3 *db = db_get_handle();
    if (!db) return false;

    const char *sql = "SELECT value FROM settings WHERE key = ?;";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return false;

    sqlite3_bind_text(stmt, 1, key, -1, SQLITE_STATIC);
    bool found = false;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const char *txt = (const char *)sqlite3_column_text(stmt, 0);
        if (txt) {
            strncpy(out_val, txt, val_size - 1);
            out_val[val_size - 1] = '\0';
            found = true;
        }
    }
    sqlite3_finalize(stmt);
    return found;
}

bool settings_service_set_value(const char *key, const char *value)
{
    if (!key || !value) return false;

    sqlite3 *db = db_get_handle();
    if (!db) return false;

    const char *sql =
        "INSERT INTO settings (key, value) VALUES (?, ?) "
        "ON CONFLICT(key) DO UPDATE SET value = excluded.value, updated_at = CURRENT_TIMESTAMP;";

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return false;

    sqlite3_bind_text(stmt, 1, key, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, value, -1, SQLITE_STATIC);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return (rc == SQLITE_DONE);
}

bool settings_service_get(SchoolSettings *s)
{
    if (!s) return false;
    memset(s, 0, sizeof(SchoolSettings));

    settings_service_get_value("school_name", s->school_name, sizeof(s->school_name));
    settings_service_get_value("school_address", s->school_address, sizeof(s->school_address));
    settings_service_get_value("school_phone", s->school_phone, sizeof(s->school_phone));
    settings_service_get_value("school_email", s->school_email, sizeof(s->school_email));
    settings_service_get_value("school_website", s->school_website, sizeof(s->school_website));
    settings_service_get_value("principal_name", s->principal_name, sizeof(s->principal_name));
    settings_service_get_value("currency_symbol", s->currency_symbol, sizeof(s->currency_symbol));
    settings_service_get_value("theme_mode", s->theme_mode, sizeof(s->theme_mode));

    if (strlen(s->currency_symbol) == 0) strncpy(s->currency_symbol, "INR", sizeof(s->currency_symbol) - 1);
    if (strlen(s->theme_mode) == 0) strncpy(s->theme_mode, "light", sizeof(s->theme_mode) - 1);

    /* Get current academic year */
    sqlite3 *db = db_get_handle();
    if (db) {
        sqlite3_stmt *stmt = NULL;
        if (sqlite3_prepare_v2(db, "SELECT id, name FROM academic_years WHERE is_current = 1 LIMIT 1;", -1, &stmt, NULL) == SQLITE_OK) {
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                s->current_academic_year_id = sqlite3_column_int(stmt, 0);
                const char *ay_name = (const char *)sqlite3_column_text(stmt, 1);
                if (ay_name) strncpy(s->current_academic_year_name, ay_name, sizeof(s->current_academic_year_name) - 1);
            }
            sqlite3_finalize(stmt);
        }
    }

    return true;
}

bool settings_service_save(const SchoolSettings *s)
{
    if (!s) return false;

    settings_service_set_value("school_name", s->school_name);
    settings_service_set_value("school_address", s->school_address);
    settings_service_set_value("school_phone", s->school_phone);
    settings_service_set_value("school_email", s->school_email);
    settings_service_set_value("school_website", s->school_website);
    settings_service_set_value("principal_name", s->principal_name);
    settings_service_set_value("currency_symbol", s->currency_symbol);
    settings_service_set_value("theme_mode", s->theme_mode);

    if (s->current_academic_year_id > 0) {
        sqlite3 *db = db_get_handle();
        if (db) {
            sqlite3_stmt *stmt = NULL;
            if (sqlite3_prepare_v2(db, "UPDATE academic_years SET is_current = CASE WHEN id = ? THEN 1 ELSE 0 END;", -1, &stmt, NULL) == SQLITE_OK) {
                sqlite3_bind_int(stmt, 1, s->current_academic_year_id);
                sqlite3_step(stmt);
                sqlite3_finalize(stmt);
            }
        }
    }

    db_log_activity(0, "UPDATE_SETTINGS", "SETTINGS", "Updated institutional settings and configuration");
    return true;
}

bool settings_service_create_backup(const char *dest_path)
{
    if (!dest_path || strlen(dest_path) == 0) return false;

    /* Ensure parent directory exists if path contains a slash */
    const char *last_slash = strrchr(dest_path, '/');
    if (!last_slash) last_slash = strrchr(dest_path, '\\');
    if (last_slash) {
        char dir_buf[256];
        size_t len = (size_t)(last_slash - dest_path);
        if (len > 0 && len < sizeof(dir_buf)) {
            strncpy(dir_buf, dest_path, len);
            dir_buf[len] = '\0';
            MKDIR(dir_buf);
        }
    }

    DbResult rc = db_backup(dest_path);
    if (rc != DB_SUCCESS) return false;

    char details[256];
    snprintf(details, sizeof(details), "Created database backup: %s", dest_path);
    db_log_activity(0, "CREATE_BACKUP", "DATABASE", details);
    return true;
}

bool settings_service_restore_backup(const char *src_path)
{
    if (!src_path || strlen(src_path) == 0) return false;

    DbResult rc = db_restore(src_path);
    if (rc != DB_SUCCESS) return false;

    char details[256];
    snprintf(details, sizeof(details), "Restored database from backup: %s", src_path);
    db_log_activity(0, "RESTORE_BACKUP", "DATABASE", details);
    return true;
}

static int backup_compare_desc(const void *a, const void *b)
{
    const BackupInfo *ba = (const BackupInfo *)a;
    const BackupInfo *bb = (const BackupInfo *)b;
    return strcmp(bb->created_at, ba->created_at);
}

bool settings_service_list_backups(BackupInfo *out_backups, int max_backups, int *out_count)
{
    if (!out_backups || max_backups <= 0 || !out_count) return false;
    *out_count = 0;

    const char *dir_path = "backups";
    DIR *dir = opendir(dir_path);
    if (!dir) {
        MKDIR(dir_path);
        return true;
    }

    int count = 0;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL && count < max_backups) {
        const char *name = entry->d_name;
        size_t len = strlen(name);
        if (len < 4 || strcmp(name + len - 3, ".db") != 0) {
            continue;
        }

        BackupInfo *bi = &out_backups[count];
        memset(bi, 0, sizeof(BackupInfo));

        strncpy(bi->filename, name, sizeof(bi->filename) - 1);
        snprintf(bi->filepath, sizeof(bi->filepath), "%s/%.127s", dir_path, name);

        struct stat st;
        if (stat(bi->filepath, &st) == 0) {
            bi->file_size_bytes = (long)st.st_size;

            time_t mtime = st.st_mtime;
            struct tm *tm_info = localtime(&mtime);
            if (tm_info) {
                strftime(bi->created_at, sizeof(bi->created_at), "%Y-%m-%d %H:%M:%S", tm_info);
            } else {
                strncpy(bi->created_at, "Unknown", sizeof(bi->created_at) - 1);
            }
        }

        count++;
    }

    closedir(dir);

    if (count > 1) {
        qsort(out_backups, count, sizeof(BackupInfo), backup_compare_desc);
    }

    *out_count = count;
    return true;
}

bool settings_service_delete_backup(const char *filepath)
{
    if (!filepath || strlen(filepath) == 0) return false;

    size_t len = strlen(filepath);
    if (len < 4 || strcmp(filepath + len - 3, ".db") != 0) {
        return false;
    }
    if (!strstr(filepath, "backups")) {
        return false;
    }

    if (remove(filepath) != 0) {
        return false;
    }

    char details[256];
    snprintf(details, sizeof(details), "Deleted backup file: %s", filepath);
    db_log_activity(0, "DELETE_BACKUP", "DATABASE", details);
    return true;
}

bool settings_service_get_recent_logs(AuditLogEntry *out_logs, int max_items, int *out_count)
{
    if (!out_logs || max_items <= 0 || !out_count) return false;
    *out_count = 0;

    sqlite3 *db = db_get_handle();
    if (!db) return false;

    const char *sql =
        "SELECT id, user_id, action, module, COALESCE(details, ''), ip_address, created_at "
        "FROM activity_logs ORDER BY id DESC LIMIT ?;";

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return false;

    sqlite3_bind_int(stmt, 1, max_items);

    int count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW && count < max_items) {
        AuditLogEntry *e = &out_logs[count];
        memset(e, 0, sizeof(AuditLogEntry));

        e->id = sqlite3_column_int(stmt, 0);
        e->user_id = sqlite3_column_int(stmt, 1);
        const char *act = (const char *)sqlite3_column_text(stmt, 2);
        const char *mod = (const char *)sqlite3_column_text(stmt, 3);
        const char *det = (const char *)sqlite3_column_text(stmt, 4);
        const char *ip  = (const char *)sqlite3_column_text(stmt, 5);
        const char *cat = (const char *)sqlite3_column_text(stmt, 6);

        if (act) strncpy(e->action, act, sizeof(e->action) - 1);
        if (mod) strncpy(e->module, mod, sizeof(e->module) - 1);
        if (det) strncpy(e->details, det, sizeof(e->details) - 1);
        if (ip)  strncpy(e->ip_address, ip, sizeof(e->ip_address) - 1);
        if (cat) strncpy(e->created_at, cat, sizeof(e->created_at) - 1);

        count++;
    }
    sqlite3_finalize(stmt);
    *out_count = count;
    return true;
}
