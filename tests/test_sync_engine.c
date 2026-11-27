/**
 * @file test_sync_engine.c
 * @brief Unit & live integration test runner for Phase 4 WinHTTP sync_engine.
 *
 * Modes:
 *  - No CLI arguments: Standalone config persistence, status cache, and GThread
 *    start/stop lifecycle smoke test (runs inside run_tests.sh without server).
 *  - With CLI arguments (<cpanel_url> <token> <hmac_secret> [verify_remote_adm]):
 *    Runs live WinHTTP test_connection, GThread debounced auto-push after creating
 *    a local student, and pulls remote changes pushed from simulated Android device.
 */

#include "sync/sync_engine.h"
#include "sync/sync.h"
#include "database.h"
#include "classes.h"
#include "student.h"

#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST_DB "test_sync_engine.db"

static int g_pass = 0;

static void check(bool cond, const char *label)
{
    if (!cond) {
        fprintf(stderr, "[FAIL] %s\n", label);
        sync_engine_stop();
        db_close();
        exit(1);
    }
    printf("  [PASS] %s\n", label);
    g_pass++;
}

static int g_remote_callback_hits = 0;

static void on_remote_hook(int applied_count, gpointer ud)
{
    (void)ud;
    g_remote_callback_hits += applied_count;
}

static int run_standalone_tests(void)
{
    printf("============================================================\n");
    printf("   SchoolCMS Sync Engine Standalone Lifecycle Test Suite\n");
    printf("============================================================\n");

    check(sync_engine_save_config("https://sync.schoolcms.local/api/", "tok-12345", "hmac-xyz", false),
          "Save disabled Cloud Sync config (normalizes trailing /api/)");

    char url[256] = {0}, tok[256] = {0}, hmac[256] = {0};
    bool enabled = true;
    check(sync_engine_get_config(url, sizeof(url), tok, sizeof(tok), hmac, sizeof(hmac), &enabled)
          && strcmp(url, "https://sync.schoolcms.local") == 0
          && strcmp(tok, "tok-12345") == 0
          && strcmp(hmac, "hmac-xyz") == 0
          && enabled == false,
          "Reloaded Cloud Sync config preserves normalized URL, token, HMAC, and disabled state");

    SyncEngineStatus st;
    sync_engine_get_status(&st);
    check(st.code == SYNC_STATUS_DISABLED && strncmp(st.device_id, "DEV-", 4) == 0 && strcmp(st.device_prefix, "D") == 0,
          "SyncEngineStatus reports DISABLED with valid DEV-* identity");

    char msg[256] = {0};
    check(!sync_engine_test_connection("", "", "", msg, sizeof(msg)),
          "sync_engine_test_connection rejects missing URL/token cleanly");

    sync_engine_set_remote_callback(on_remote_hook, NULL);
    sync_engine_start();
    check(sync_engine_is_running(), "Background sync GThread started cleanly");
    g_usleep(200 * 1000);
    sync_engine_trigger_now();
    g_usleep(200 * 1000);
    sync_engine_stop();
    check(!sync_engine_is_running(), "Background sync GThread joined cleanly on sync_engine_stop");

    printf("\nALL %d SYNC ENGINE STANDALONE CHECKS PASSED!\n", g_pass);
    return 0;
}

static int run_live_winhttp_test(const char *url, const char *token, const char *hmac)
{
    printf("============================================================\n");
    printf("   SchoolCMS Live WinHTTP <-> cPanel PHP Sync Test\n");
    printf("============================================================\n");

    char msg[256] = {0};
    check(sync_engine_test_connection(url, token, hmac, msg, sizeof(msg)),
          msg[0] ? msg : "WinHTTP /api/health.php probe succeeded");

    /* Start background GThread BEFORE creating local entities so SQLite update hook wakes debounced push */
    sync_engine_set_remote_callback(on_remote_hook, NULL);
    sync_engine_start();
    check(sync_engine_save_config(url, token, hmac, true),
          "Enabled Cloud Sync with live cPanel endpoint");

    SchoolClass cls = {0};
    snprintf(cls.name, sizeof(cls.name), "Class 11");
    cls.numeric_value = 11;
    cls.capacity = 45;
    cls.academic_year_id = 1;
    int cls_id = 0;
    check(class_service_create(&cls, &cls_id) && cls_id > 0, "Create local Class 11");

    ClassSection sec = {0};
    sec.class_id = cls_id;
    snprintf(sec.name, sizeof(sec.name), "A");
    int sec_id = 0;
    check(section_service_create(&sec, &sec_id) && sec_id > 0, "Create local Section A");

    Student st = {0};
    student_service_generate_admission_no(st.admission_no, sizeof(st.admission_no));
    snprintf(st.full_name, sizeof(st.full_name), "Kavya Singh Desktop");
    snprintf(st.gender, sizeof(st.gender), "FEMALE");
    st.class_id = cls_id;
    st.section_id = sec_id;
    st.roll_number = 3;
    snprintf(st.status, sizeof(st.status), "ACTIVE");
    int st_id = 0;
    check(student_service_create(&st, &st_id) && st_id > 0, "Create local Student triggering debounced outbox push");

    /* Wait up to 4 seconds for the background GThread (~900ms debounce) to empty sync_outbox cleanly */
    int pending = -1;
    for (int i = 0; i < 40; i++) {
        pending = sync_core_get_pending_outbox_count();
        if (pending == 0) break;
        g_usleep(100 * 1000);
    }
    check(pending == 0, "Background GThread debounced push automatically flushed local outbox (pending == 0)");

    /* Stop worker thread gracefully before external step pushes simulated Android student */
    sync_engine_stop();
    check(sync_core_get_pending_outbox_count() == 0, "Zero pending outbox after initial push");

    printf("PUSH_PHASE_DONE class_uuid_adm=%s\n", st.admission_no);
    return 0;
}

static int run_live_pull_phase(void)
{
    char msg[256] = {0};
    check(sync_engine_sync_once_blocking(msg, sizeof(msg)),
          msg[0] ? msg : "sync_engine_sync_once_blocking pulled Android delta");

    Student pulled_st = {0};
    check(student_service_get_by_admission_no("ADM-2026-M0099", &pulled_st)
          && strcmp(pulled_st.full_name, "Kabir Android Synced") == 0
          && strcmp(pulled_st.class_name, "Class 11") == 0
          && strcmp(pulled_st.section_name, "A") == 0,
          "Desktop SQLite materialized remote Android student with exact local Class 11 & Section A links");

    check(sync_core_get_pending_outbox_count() == 0,
          "Anti-echo verified end-to-end: pulled Android student queued ZERO outbox echo rows");

    SyncEngineStatus st;
    sync_engine_get_status(&st);
    check(st.code == SYNC_STATUS_OK && st.last_server_seq >= 4,
          "SyncEngineStatus updated cleanly to SYNC_STATUS_OK with latest server_seq");

    printf("\nALL %d LIVE WINHTTP SYNC CHECKS PASSED!\n", g_pass);
    return 0;
}

int main(int argc, char *argv[])
{
    int rc = 0;
    if (argc >= 2 && strcmp(argv[1], "pull_verify") == 0) {
        if (db_init(TEST_DB) != DB_SUCCESS) return 1;
        rc = run_live_pull_phase();
        db_close();
        remove(TEST_DB);
        return rc;
    }

    remove(TEST_DB);
    if (db_init(TEST_DB) != DB_SUCCESS) {
        fprintf(stderr, "Failed to open %s\n", TEST_DB);
        return 1;
    }

    if (argc >= 4) {
        rc = run_live_winhttp_test(argv[1], argv[2], argv[3]);
        db_close();
        /* Keep TEST_DB intact for pull_verify phase triggered by test_desktop_to_cpanel.py */
    } else {
        rc = run_standalone_tests();
        db_close();
        remove(TEST_DB);
    }
    return rc;
}
