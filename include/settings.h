/**
 * @file settings.h
 * @brief Institutional configuration, theme switching, and SQLite backup/restore services.
 */

#ifndef SCHOOLCMS_SETTINGS_H
#define SCHOOLCMS_SETTINGS_H

#include <stdbool.h>
#include <stddef.h>

/**
 * @brief Institutional School Settings Model
 */
typedef struct {
    char school_name[128];
    char school_address[256];
    char school_phone[32];
    char school_email[128];
    char school_website[128];
    char principal_name[128];
    char currency_symbol[16];
    char theme_mode[16];        /* "light" or "dark" */
    int current_academic_year_id;
    char current_academic_year_name[32];
} SchoolSettings;

/**
 * @brief Activity Audit Log Item
 */
typedef struct {
    int id;
    int user_id;
    char action[64];
    char module[64];
    char details[256];
    char ip_address[48];
    char created_at[32];
} AuditLogEntry;

/**
 * @brief Backup File Metadata Item
 */
typedef struct {
    char filename[128];
    char filepath[256];
    char created_at[32];        /* YYYY-MM-DD HH:MM:SS */
    long file_size_bytes;       /* Size in bytes */
} BackupInfo;

/**
 * @brief Outgoing Email SMTP Configuration Model
 */
typedef struct {
    char smtp_host[128];
    int smtp_port;
    char smtp_username[128];
    char smtp_password[128];
    char smtp_encryption[16];   /* "STARTTLS", "SSL", "NONE" */
    char smtp_sender_email[128];
    char smtp_sender_name[128];
} SmtpConfig;

/* ── Service Functions ────────────────────────────────────────────── */

/**
 * @brief Load full institutional settings into structure.
 */
bool settings_service_get(SchoolSettings *out_settings);

/**
 * @brief Save institutional settings.
 */
bool settings_service_save(const SchoolSettings *settings);

/**
 * @brief Retrieve a single setting string by key.
 */
bool settings_service_get_value(const char *key, char *out_val, size_t val_size);

/**
 * @brief Save a single key-value setting.
 */
bool settings_service_set_value(const char *key, const char *value);

/**
 * @brief Trigger an online database backup to destination path.
 */
bool settings_service_create_backup(const char *dest_path);

/**
 * @brief Restore active database from backup file.
 */
bool settings_service_restore_backup(const char *src_path);

/**
 * @brief List all available database backup files from the backups directory.
 * @param out_backups Array to receive backup metadata items.
 * @param max_backups Maximum items that can fit into out_backups.
 * @param out_count Pointer to receive number of backups found.
 * @return true on success.
 */
bool settings_service_list_backups(BackupInfo *out_backups, int max_backups, int *out_count);

/**
 * @brief Delete a specific backup file.
 * @param filepath Full path to the backup file inside backups/
 * @return true on success.
 */
bool settings_service_delete_backup(const char *filepath);

/**
 * @brief Retrieve recent audit activity logs.
 */
bool settings_service_get_recent_logs(AuditLogEntry *out_logs, int max_items, int *out_count);

/**
 * @brief Load SMTP configuration from database.
 */
bool settings_service_get_smtp(SmtpConfig *out_cfg);

/**
 * @brief Save SMTP configuration to database.
 */
bool settings_service_save_smtp(const SmtpConfig *cfg);

/**
 * @brief Test live connectivity to configured SMTP server.
 */
bool settings_service_test_smtp(const SmtpConfig *cfg, char *out_msg, size_t msg_size);

#endif /* SCHOOLCMS_SETTINGS_H */
