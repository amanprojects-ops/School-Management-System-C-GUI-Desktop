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

/** Current target schema version */
#define DB_CURRENT_VERSION 1

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

#endif /* SCHOOLCMS_DATABASE_H */
