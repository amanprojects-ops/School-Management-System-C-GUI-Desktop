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
#include <winsock2.h>
#include <ws2tcpip.h>
#include <direct.h>
#define MKDIR(d) _mkdir(d)
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <sys/stat.h>
#define MKDIR(d) mkdir(d, 0755)
typedef int SOCKET;
#define INVALID_SOCKET (-1)
#define closesocket close
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


/* ── Outgoing Email SMTP Gateway Services ─────────────────────────── */

bool settings_service_get_smtp(SmtpConfig *out_cfg)
{
    if (!out_cfg) return false;
    memset(out_cfg, 0, sizeof(SmtpConfig));

    settings_service_get_value("smtp_host", out_cfg->smtp_host, sizeof(out_cfg->smtp_host));
    char port_str[32] = {0};
    settings_service_get_value("smtp_port", port_str, sizeof(port_str));
    out_cfg->smtp_port = atoi(port_str);
    if (out_cfg->smtp_port <= 0) out_cfg->smtp_port = 587;

    settings_service_get_value("smtp_username", out_cfg->smtp_username, sizeof(out_cfg->smtp_username));
    settings_service_get_value("smtp_password", out_cfg->smtp_password, sizeof(out_cfg->smtp_password));
    settings_service_get_value("smtp_encryption", out_cfg->smtp_encryption, sizeof(out_cfg->smtp_encryption));
    if (strlen(out_cfg->smtp_encryption) == 0) strncpy(out_cfg->smtp_encryption, "STARTTLS", sizeof(out_cfg->smtp_encryption) - 1);

    settings_service_get_value("smtp_sender_name", out_cfg->smtp_sender_name, sizeof(out_cfg->smtp_sender_name));
    settings_service_get_value("smtp_sender_email", out_cfg->smtp_sender_email, sizeof(out_cfg->smtp_sender_email));

    return true;
}

bool settings_service_save_smtp(const SmtpConfig *cfg)
{
    if (!cfg) return false;

    settings_service_set_value("smtp_host", cfg->smtp_host);
    char port_str[32];
    snprintf(port_str, sizeof(port_str), "%d", cfg->smtp_port > 0 ? cfg->smtp_port : 587);
    settings_service_set_value("smtp_port", port_str);
    settings_service_set_value("smtp_username", cfg->smtp_username);
    settings_service_set_value("smtp_password", cfg->smtp_password);
    settings_service_set_value("smtp_encryption", cfg->smtp_encryption);
    settings_service_set_value("smtp_sender_name", cfg->smtp_sender_name);
    settings_service_set_value("smtp_sender_email", cfg->smtp_sender_email);

    db_log_activity(0, "UPDATE_SMTP", "SETTINGS", "Updated outgoing email SMTP configuration");
    return true;
}

bool settings_service_test_smtp(const SmtpConfig *cfg, char *out_msg, size_t msg_size)
{
    if (!out_msg || msg_size == 0) return false;
    out_msg[0] = '\0';

    if (!cfg || strlen(cfg->smtp_host) == 0) {
        snprintf(out_msg, msg_size, "Validation Error: SMTP host cannot be empty.");
        return false;
    }

    int port = cfg->smtp_port > 0 ? cfg->smtp_port : 587;
    if (port <= 0 || port > 65535) {
        snprintf(out_msg, msg_size, "Validation Error: Invalid port %d (must be between 1 and 65535).", port);
        return false;
    }

    if (strlen(cfg->smtp_sender_email) > 0 && !strchr(cfg->smtp_sender_email, '@')) {
        snprintf(out_msg, msg_size, "Validation Error: Sender email '%s' is missing '@'.", cfg->smtp_sender_email);
        return false;
    }

#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        snprintf(out_msg, msg_size, "Network Error: Failed to initialize Winsock subsystem.");
        return false;
    }
#endif

    char port_str[16];
    snprintf(port_str, sizeof(port_str), "%d", port);

    struct addrinfo hints, *res = NULL;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    int rc = getaddrinfo(cfg->smtp_host, port_str, &hints, &res);
    if (rc != 0 || !res) {
#ifdef _WIN32
        WSACleanup();
#endif
        snprintf(out_msg, msg_size,
                 "DNS Resolution Failed: Unable to resolve SMTP host '%s'. Please verify host name and internet connection.",
                 cfg->smtp_host);
        return false;
    }

    SOCKET sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
#ifdef _WIN32
    if (sock == INVALID_SOCKET) {
        freeaddrinfo(res);
        WSACleanup();
        snprintf(out_msg, msg_size, "Socket Error: Could not create TCP socket.");
        return false;
    }
    DWORD timeout_ms = 3500;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char *)&timeout_ms, sizeof(timeout_ms));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char *)&timeout_ms, sizeof(timeout_ms));
#else
    if (sock < 0) {
        freeaddrinfo(res);
        snprintf(out_msg, msg_size, "Socket Error: Could not create TCP socket.");
        return false;
    }
#endif

    int conn_res = connect(sock, res->ai_addr, (int)res->ai_addrlen);
    freeaddrinfo(res);

    if (conn_res != 0) {
#ifdef _WIN32
        closesocket(sock);
        WSACleanup();
#else
        close(sock);
#endif
        snprintf(out_msg, msg_size,
                 "Connection Failed: Could not connect to '%s:%d' (Connection timed out or refused). Verify port and firewall.",
                 cfg->smtp_host, port);
        return false;
    }

    char banner[256] = {0};
    int bytes = recv(sock, banner, sizeof(banner) - 1, 0);
#ifdef _WIN32
    closesocket(sock);
    WSACleanup();
#else
    close(sock);
#endif

    if (bytes > 0) {
        banner[bytes] = '\0';
        char *nl = strpbrk(banner, "\r\n");
        if (nl) *nl = '\0';
        snprintf(out_msg, msg_size,
                 "SMTP Handshake Succeeded: Connected to '%s:%d' (%s). Server replied: '%s'. Gateway is online.",
                 cfg->smtp_host, port, cfg->smtp_encryption, banner);
    } else {
        snprintf(out_msg, msg_size,
                 "SMTP Connection Succeeded: Connected to '%s:%d' (%s). TCP port is open and accepting traffic.",
                 cfg->smtp_host, port, cfg->smtp_encryption);
    }

    db_log_activity(0, "TEST_SMTP", "SETTINGS", out_msg);
    return true;
}
