/**
 * @file test_settings.c
 * @brief Unit tests for institutional settings, key-value configuration, and database backup/restore.
 */

#include "settings.h"
#include "database.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST_DB "test_schoolcms_settings.db"
#define TEST_BACKUP "test_backup_settings.db"

static void cleanup_files(void)
{
    remove(TEST_DB);
    remove(TEST_DB "-wal");
    remove(TEST_DB "-shm");
    remove(TEST_BACKUP);
    remove(TEST_BACKUP "-wal");
    remove(TEST_BACKUP "-shm");
}

static void test_key_value_settings(void)
{
    printf("[TEST] Testing key-value settings retrieval and mutation...\n");

    char val[128];
    bool ok = settings_service_get_value("school_name", val, sizeof(val));
    assert(ok && "Seed school_name must exist");
    assert(strcmp(val, "St. Xavier International School") == 0);

    /* Set custom key */
    ok = settings_service_set_value("system_timezone", "Asia/Kolkata");
    assert(ok && "Must set custom key");

    char tz[128];
    ok = settings_service_get_value("system_timezone", tz, sizeof(tz));
    assert(ok && strcmp(tz, "Asia/Kolkata") == 0);

    printf("       Key-value settings verified -> PASS\n");
}

static void test_full_settings_profile(void)
{
    printf("[TEST] Testing full institutional profile get and save...\n");

    SchoolSettings s;
    bool ok = settings_service_get(&s);
    assert(ok);
    assert(strlen(s.school_name) > 0);

    /* Update profile */
    strncpy(s.school_name, "Greenwood High Academy", sizeof(s.school_name) - 1);
    strncpy(s.principal_name, "Dr. Jane Roe", sizeof(s.principal_name) - 1);
    strncpy(s.theme_mode, "dark", sizeof(s.theme_mode) - 1);
    strncpy(s.currency_symbol, "₹", sizeof(s.currency_symbol) - 1);

    ok = settings_service_save(&s);
    assert(ok && "Settings save should succeed");

    SchoolSettings updated;
    ok = settings_service_get(&updated);
    assert(ok);
    assert(strcmp(updated.school_name, "Greenwood High Academy") == 0);
    assert(strcmp(updated.principal_name, "Dr. Jane Roe") == 0);
    assert(strcmp(updated.theme_mode, "dark") == 0);
    assert(strcmp(updated.currency_symbol, "₹") == 0);

    printf("       Full settings profile verified -> PASS\n");
}

static void test_backup_and_restore(void)
{
    printf("[TEST] Testing database online backup and atomic restore...\n");

    /* Create backup */
    bool ok = settings_service_create_backup(TEST_BACKUP);
    assert(ok && "Backup creation must succeed");

    /* Verify backup file exists */
    FILE *fp = fopen(TEST_BACKUP, "rb");
    assert(fp != NULL && "Backup file must be accessible");
    fclose(fp);

    /* Mutate database state */
    ok = settings_service_set_value("school_name", "Mutated Temporary Name");
    assert(ok);

    char mutated[128];
    settings_service_get_value("school_name", mutated, sizeof(mutated));
    assert(strcmp(mutated, "Mutated Temporary Name") == 0);

    /* Restore from backup */
    ok = settings_service_restore_backup(TEST_BACKUP);
    assert(ok && "Restore from backup must succeed");

    /* Verify restored state */
    char restored[128];
    ok = settings_service_get_value("school_name", restored, sizeof(restored));
    assert(ok);
    assert(strcmp(restored, "Greenwood High Academy") == 0 && "Restored value must match backup state");

    printf("       Backup and restore verified -> PASS\n");
}

static void test_backup_list_and_delete(void)
{
    printf("[TEST] Testing backup inventory listing and deletion...\n");

    /* Create test backup inside backups/ directory */
    const char *test_bk = "backups/test_schoolcms_inventory.db";
    bool ok = settings_service_create_backup(test_bk);
    assert(ok && "Backup creation in backups/ must succeed");

    BackupInfo backups[20];
    int count = 0;
    ok = settings_service_list_backups(backups, 20, &count);
    assert(ok && "Listing backups must succeed");
    assert(count >= 1 && "At least one backup must be found");

    bool found = false;
    for (int i = 0; i < count; i++) {
        if (strcmp(backups[i].filename, "test_schoolcms_inventory.db") == 0) {
            found = true;
            assert(backups[i].file_size_bytes > 0 && "Backup file size must be non-zero");
            assert(strlen(backups[i].created_at) > 0 && "Backup timestamp must be populated");
            break;
        }
    }
    assert(found && "Created backup must be in listing");

    /* Test backup deletion */
    ok = settings_service_delete_backup(test_bk);
    assert(ok && "Backup deletion must succeed");

    /* Verify no longer in list */
    count = 0;
    settings_service_list_backups(backups, 20, &count);
    found = false;
    for (int i = 0; i < count; i++) {
        if (strcmp(backups[i].filename, "test_schoolcms_inventory.db") == 0) {
            found = true;
            break;
        }
    }
    assert(!found && "Deleted backup must not be present in list");

    printf("       Backup inventory listing and deletion verified -> PASS\n");
}

static void test_audit_logs(void)
{
    printf("[TEST] Testing audit activity logging...\n");

    AuditLogEntry logs[20];
    int count = 0;
    bool ok = settings_service_get_recent_logs(logs, 20, &count);
    assert(ok);
    assert(count >= 2 && "Should have logged settings updates and backup/restore actions");

    printf("       Audit activity logs verified (%d events) -> PASS\n", count);
}

int main(void)
{
    printf("====================================================\n");
    printf("   SchoolCMS Settings & Backup Unit Tests           \n");
    printf("====================================================\n");

    cleanup_files();

    int rc = db_init(TEST_DB);
    assert(rc == DB_SUCCESS && "Database init should succeed");

    test_key_value_settings();
    test_full_settings_profile();
    test_backup_and_restore();
    test_backup_list_and_delete();
    test_audit_logs();

    db_close();
    cleanup_files();

    printf("\n>>> ALL SETTINGS TESTS PASSED (100%%) <<<\n");
    return 0;
}
