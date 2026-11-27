/**
 * @file database.h
 * @brief SQLite3 Database management and migration system for SchoolCMS.
 *
 * Provides schema management, automatic migrations, transactions,
 * backup/restore, audit logging, and seed data initialization.
 */

#ifndef SCHOOLCMS_DATABASE_H
#define SCHOOLCMS_DATABASE_H

#include <sqlite3.h>
#include <stdbool.h>
#include <stddef.h>

/** Current target schema version */
#define DB_CURRENT_VERSION 3

/** Default database filename */
#define DB_DEFAULT_FILE "schoolcms.db"

/**
 * @brief Error codes returned by database operations.
 */
typedef enum {
    DB_SUCCESS = 0,
    DB_ERROR_OPEN,
    DB_ERROR_EXEC,
    DB_ERROR_PREPARE,
    DB_ERROR_STEP,
    DB_ERROR_MIGRATION,
    DB_ERROR_BACKUP,
    DB_ERROR_RESTORE,
    DB_ERROR_PARAM
} DbResult;

/**
 * @brief Initialize database connection and run pending migrations.
 *
 * Enables foreign keys and WAL mode for high performance and durability.
 * If the database is empty, creates all tables and inserts seed data.
 *
 * @param db_path Path to the SQLite file, or NULL to use DB_DEFAULT_FILE.
 * @return DB_SUCCESS on success, error code otherwise.
 */
DbResult db_init(const char *db_path);

/**
 * @brief Close the active database connection.
 */
void db_close(void);

/**
 * @brief Get the active SQLite database handle.
 * @return sqlite3* pointer or NULL if uninitialized.
 */
sqlite3 *db_get_handle(void);

/**
 * @brief Run database migrations up to DB_CURRENT_VERSION.
 * @return DB_SUCCESS on success, error code otherwise.
 */
DbResult db_run_migrations(void);

/**
 * @brief Get current schema version stored in PRAGMA user_version.
 * @return Integer version, or -1 on error.
 */
int db_get_version(void);

/**
 * @brief Begin a transaction (BEGIN IMMEDIATE).
 * @return DB_SUCCESS on success.
 */
DbResult db_begin_transaction(void);

/**
 * @brief Acquire recursive process-wide DB synchronization mutex.
 */
void db_lock(void);

/**
 * @brief Release recursive process-wide DB synchronization mutex.
 */
void db_unlock(void);

/**
 * @brief Commit active transaction.
 * @return DB_SUCCESS on success.
 */
DbResult db_commit(void);

/**
 * @brief Rollback active transaction.
 * @return DB_SUCCESS on success.
 */
DbResult db_rollback(void);

/**
 * @brief Perform an online backup of the database to destination path.
 *
 * Uses the SQLite Online Backup API.
 * @param dest_path Path to destination .db file.
 * @return DB_SUCCESS on success.
 */
DbResult db_backup(const char *dest_path);

/**
 * @brief Restore the active database from a backup file.
 *
 * Restores content into the current database using SQLite Backup API.
 * @param src_path Path to source backup file.
 * @return DB_SUCCESS on success.
 */
DbResult db_restore(const char *src_path);

/**
 * @brief Record an event into the activity_logs table.
 *
 * @param user_id User ID who performed action (or 0 for system).
 * @param action Short action verb (e.g., "LOGIN", "CREATE_STUDENT").
 * @param module Module name (e.g., "AUTH", "STUDENTS", "SETTINGS").
 * @param details Freeform human-readable description.
 * @return DB_SUCCESS on success.
 */
DbResult db_log_activity(int user_id, const char *action, const char *module, const char *details);

/* ── Multi-Device Sync State & Collision-Free Code Helpers ─────────── */

/**
 * @brief Read a sync configuration/state value by key from sync_state table.
 */
bool db_sync_get_state(const char *key, char *out_val, size_t val_size);

/**
 * @brief Store or update a sync configuration/state key-value pair.
 */
bool db_sync_set_state(const char *key, const char *value);

/**
 * @brief Get the unique persistent local device identifier (e.g. "DEV-a1b2c3d4e5f6").
 */
bool db_sync_get_device_id(char *out_id, size_t size);

/**
 * @brief Get the local short device prefix used for offline sequential codes ("D" Desktop / "M" Mobile).
 */
bool db_sync_get_device_prefix(char *out_prefix, size_t size);

/**
 * @brief Generate a unique RFC 4122 UUID v4 string via SQLite CSPRNG.
 */
bool db_sync_generate_uuid(char *out_uuid, size_t size);

/**
 * @brief Generate a collision-free device-prefixed sequential code (e.g. "ADM-2026-D0001", "RCP-2026-D0001").
 */
bool db_sync_generate_prefixed_code(const char *table_name,
                                    const char *col_name,
                                    const char *code_prefix,
                                    char *out_buf,
                                    size_t buf_size);

#endif /* SCHOOLCMS_DATABASE_H */
