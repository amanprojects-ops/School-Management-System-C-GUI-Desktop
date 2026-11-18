/**
 * @file test_database.c
 * @brief Unit tests for database initialization, migrations, constraints, and backup/restore.
 */

#include "database.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define TEST_DB "test_schoolcms.db"
#define BACKUP_DB "test_schoolcms_backup.db"

static void cleanup_files(void)
{
    remove(TEST_DB);
    remove(TEST_DB "-wal");
    remove(TEST_DB "-shm");
    remove(BACKUP_DB);
    remove(BACKUP_DB "-wal");
    remove(BACKUP_DB "-shm");
}

static void test_initialization_and_version(void)
{
    printf("[TEST] Testing database initialization and versioning...\n");
    cleanup_files();

    DbResult res = db_init(TEST_DB);
    assert(res == DB_SUCCESS);

    int version = db_get_version();
    assert(version == DB_CURRENT_VERSION);
    printf("       Schema version is %d (expected %d) -> PASS\n", version, DB_CURRENT_VERSION);
}

static void test_tables_exist(void)
{
    printf("[TEST] Testing all required tables exist...\n");
    const char *required_tables[] = {
        "roles", "users", "academic_years", "classes", "sections",
        "teachers", "staff", "students", "subjects", "attendance",
        "fee_structures", "fee_payments", "examinations", "exam_subjects",
        "exam_results", "timetables", "notifications", "activity_logs",
        "settings", NULL
    };

    sqlite3 *db = db_get_handle();
    assert(db != NULL);

    for (int i = 0; required_tables[i] != NULL; i++) {
        const char *tbl = required_tables[i];
        sqlite3_stmt *stmt = NULL;
        const char *sql = "SELECT name FROM sqlite_master WHERE type='table' AND name=?;";
        int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
        assert(rc == SQLITE_OK);
        sqlite3_bind_text(stmt, 1, tbl, -1, SQLITE_STATIC);

        rc = sqlite3_step(stmt);
        assert(rc == SQLITE_ROW);
        sqlite3_finalize(stmt);
        printf("       Table '%s' verified -> PASS\n", tbl);
    }
}

static void test_seed_data(void)
{
    printf("[TEST] Testing seed data integrity...\n");
    sqlite3 *db = db_get_handle();
    assert(db != NULL);

    /* Verify roles count */
    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM roles;", -1, &stmt, NULL);
    assert(rc == SQLITE_OK);
    assert(sqlite3_step(stmt) == SQLITE_ROW);
    int role_count = sqlite3_column_int(stmt, 0);
    assert(role_count == 4);
    sqlite3_finalize(stmt);
    printf("       Roles seeded: %d -> PASS\n", role_count);

    /* Verify default admin user */
    rc = sqlite3_prepare_v2(db, "SELECT username, role_id FROM users WHERE id=1;", -1, &stmt, NULL);
    assert(rc == SQLITE_OK);
    assert(sqlite3_step(stmt) == SQLITE_ROW);
    const unsigned char *uname = sqlite3_column_text(stmt, 0);
    assert(strcmp((const char *)uname, "admin") == 0);
    int role_id = sqlite3_column_int(stmt, 1);
    assert(role_id == 1);
    sqlite3_finalize(stmt);
    printf("       Default admin verified -> PASS\n");

    /* Verify school settings */
    rc = sqlite3_prepare_v2(db, "SELECT value FROM settings WHERE key='school_name';", -1, &stmt, NULL);
    assert(rc == SQLITE_OK);
    assert(sqlite3_step(stmt) == SQLITE_ROW);
    const unsigned char *school_name = sqlite3_column_text(stmt, 0);
    assert(strlen((const char *)school_name) > 0);
    printf("       School name setting: '%s' -> PASS\n", school_name);
    sqlite3_finalize(stmt);
}

static void test_backup_and_restore(void)
{
    printf("[TEST] Testing database backup and restore...\n");
    DbResult res = db_backup(BACKUP_DB);
    assert(res == DB_SUCCESS);
    printf("       Backup created successfully -> PASS\n");

    /* Insert test activity into current DB */
    res = db_log_activity(1, "TEST_ACTION", "UNIT_TEST", "Before restore point");
    assert(res == DB_SUCCESS);

    /* Restore from backup */
    res = db_restore(BACKUP_DB);
    assert(res == DB_SUCCESS);
    printf("       Database restored from backup -> PASS\n");
}

int main(void)
{
    printf("=========================================\n");
    printf("  SchoolCMS Database Module Unit Tests   \n");
    printf("=========================================\n\n");

    test_initialization_and_version();
    test_tables_exist();
    test_seed_data();
    test_backup_and_restore();

    db_close();
    cleanup_files();

    printf("\nAll database unit tests passed successfully!\n");
    return 0;
}
