/**
 * @file test_sync.c
 * @brief Unit & integration tests for Schema V3 sync infrastructure, prefix code
 *        generation, outbox serialization, remote FK UUID resolution, LWW,
 *        append-only fee protection, tombstones, and anti-echo suppression.
 */

#include "sync/sync.h"
#include "database.h"
#include "classes.h"
#include "student.h"
#include "teacher.h"
#include "fees.h"

#include <assert.h>
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST_DB "test_sync.db"

static int g_pass = 0;

static void check(bool cond, const char *label)
{
    if (!cond) {
        fprintf(stderr, "[FAIL] %s\n", label);
        db_close();
        exit(1);
    }
    printf("  [PASS] %s\n", label);
    g_pass++;
}

static void get_single_text(const char *sql, const char *param1, char *out_buf, size_t size)
{
    sqlite3 *db = db_get_handle();
    assert(db != NULL);
    sqlite3_stmt *stmt = NULL;
    assert(sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK);
    if (param1) sqlite3_bind_text(stmt, 1, param1, -1, SQLITE_STATIC);
    out_buf[0] = '\0';
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const char *txt = (const char *)sqlite3_column_text(stmt, 0);
        if (txt) snprintf(out_buf, size, "%s", txt);
    }
    sqlite3_finalize(stmt);
}

static long long get_single_int64(const char *sql, const char *param1)
{
    sqlite3 *db = db_get_handle();
    assert(db != NULL);
    sqlite3_stmt *stmt = NULL;
    assert(sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK);
    if (param1) sqlite3_bind_text(stmt, 1, param1, -1, SQLITE_STATIC);
    long long val = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        val = sqlite3_column_int64(stmt, 0);
    }
    sqlite3_finalize(stmt);
    return val;
}

static bool has_table_column(const char *table_name, const char *col_name)
{
    char sql[128];
    snprintf(sql, sizeof(sql), "PRAGMA table_info(%s);", table_name);
    sqlite3 *db = db_get_handle();
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return false;
    bool found = false;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char *c = (const char *)sqlite3_column_text(stmt, 1);
        if (c && strcmp(c, col_name) == 0) {
            found = true;
            break;
        }
    }
    sqlite3_finalize(stmt);
    return found;
}

static void test_schema_v3_and_identity(void)
{
    printf("\n1. Testing Schema V3 Upgrade, Backfill & Device Identity...\n");

    check(db_get_version() == 3, "Database version upgraded to 3 (DB_CURRENT_VERSION)");
    check(get_single_int64("SELECT COUNT(*) FROM sqlite_master WHERE type='table' AND name='sync_state';", NULL) == 1,
          "Table sync_state exists");
    check(get_single_int64("SELECT COUNT(*) FROM sqlite_master WHERE type='table' AND name='sync_outbox';", NULL) == 1,
          "Table sync_outbox exists");
    check(get_single_int64("SELECT COUNT(*) FROM sqlite_master WHERE type='table' AND name='sync_tombstones';", NULL) == 1,
          "Table sync_tombstones exists");

    const char *sample_tables[] = { "roles", "users", "students", "teachers", "staff", "fee_payments", "settings", "role_permissions" };
    for (size_t i = 0; i < sizeof(sample_tables) / sizeof(sample_tables[0]); i++) {
        char msg[128];
        snprintf(msg, sizeof(msg), "Table '%s' has sync_uuid + updated_at_ms + deleted_at_ms columns", sample_tables[i]);
        check(has_table_column(sample_tables[i], "sync_uuid")
              && has_table_column(sample_tables[i], "origin_device_id")
              && has_table_column(sample_tables[i], "updated_at_ms")
              && has_table_column(sample_tables[i], "deleted_at_ms"),
              msg);
    }

    check(get_single_int64("SELECT COUNT(*) FROM roles WHERE sync_uuid IS NULL OR length(sync_uuid) < 36;", NULL) == 0,
          "All seeded roles rows backfilled with valid sync_uuid");
    check(get_single_int64("SELECT COUNT(*) FROM settings WHERE sync_uuid IS NULL OR length(sync_uuid) < 36;", NULL) == 0,
          "All seeded settings rows backfilled with valid sync_uuid");
    check(sync_core_get_pending_outbox_count() == 0,
          "Initial migration/backfill leaves zero spurious outbox items");

    char dev_id[SYNC_DEVICE_ID_LEN] = {0};
    char dev_prefix[8] = {0};
    check(sync_core_get_device_id(dev_id, sizeof(dev_id)) && strncmp(dev_id, "DEV-", 4) == 0 && strlen(dev_id) >= 16,
          "Generated persistent DEV-* device_id");
    check(sync_core_get_device_prefix(dev_prefix, sizeof(dev_prefix)) && strcmp(dev_prefix, "D") == 0,
          "Default desktop device_prefix is 'D'");

    char uuid1[SYNC_UUID_LEN] = {0};
    char uuid2[SYNC_UUID_LEN] = {0};
    check(sync_core_generate_uuid(uuid1, sizeof(uuid1)) && strlen(uuid1) == 36
          && uuid1[14] == '4' && (uuid1[19] == '8' || uuid1[19] == '9' || uuid1[19] == 'a' || uuid1[19] == 'b'),
          "sync_core_generate_uuid produces compliant RFC 4122 UUID v4");
    check(sync_core_generate_uuid(uuid2, sizeof(uuid2)) && strcmp(uuid1, uuid2) != 0,
          "sync_core_generate_uuid produces unique values");
}

static void test_prefixed_code_generation(void)
{
    printf("\n2. Testing Device-Prefixed Collision-Free Code Generation...\n");

    char adm[32] = {0}, tch[32] = {0}, stf[32] = {0}, rcp[32] = {0};
    check(student_service_generate_admission_no(adm, sizeof(adm))
          && strncmp(adm, "ADM-", 4) == 0 && strstr(adm, "-D0001") != NULL,
          "Admission number includes desktop '-D0001' prefix");
    check(teacher_service_generate_code(tch, sizeof(tch))
          && strncmp(tch, "TCH-", 4) == 0 && strstr(tch, "-D0001") != NULL,
          "Teacher code includes desktop '-D0001' prefix");
    check(staff_service_generate_code(stf, sizeof(stf))
          && strncmp(stf, "STF-", 4) == 0 && strstr(stf, "-D0001") != NULL,
          "Staff code includes desktop '-D0001' prefix");
    check(fee_service_generate_receipt_no(rcp, sizeof(rcp))
          && strncmp(rcp, "RCP-", 4) == 0 && strstr(rcp, "-D0001") != NULL,
          "Receipt number includes desktop '-D0001' prefix");

    check(sync_core_set_state("device_prefix", "m"), "Switch device_prefix state to 'm' (normalized to 'M')");
    check(student_service_generate_admission_no(adm, sizeof(adm)) && strstr(adm, "-M0001") != NULL,
          "Admission number reflects active mobile '-M0001' prefix");
    check(sync_core_set_state("device_prefix", "D"), "Restore device_prefix back to 'D'");
}

static void test_triggers_outbox_serialization_and_push_bundle(void)
{
    printf("\n3. Testing Triggers (INS/UPD/DEL), Outbox Fetch & Push JSON Bundling...\n");

    SchoolClass cls = {0};
    snprintf(cls.name, sizeof(cls.name), "Class 10");
    cls.numeric_value = 10;
    cls.capacity = 40;
    cls.academic_year_id = 1;
    int cls_id = 0;
    check(class_service_create(&cls, &cls_id) && cls_id > 0, "Create local Class 10");

    ClassSection sec = {0};
    sec.class_id = cls_id;
    snprintf(sec.name, sizeof(sec.name), "A");
    int sec_id = 0;
    check(section_service_create(&sec, &sec_id) && sec_id > 0, "Create local Section A");

    ClassSection temp_sec = {0};
    temp_sec.class_id = cls_id;
    snprintf(temp_sec.name, sizeof(temp_sec.name), "Temp");
    int temp_sec_id = 0;
    check(section_service_create(&temp_sec, &temp_sec_id) && temp_sec_id > 0, "Create temporary Section Temp");

    Student st = {0};
    student_service_generate_admission_no(st.admission_no, sizeof(st.admission_no));
    snprintf(st.full_name, sizeof(st.full_name), "Aarav Sharma");
    snprintf(st.gender, sizeof(st.gender), "MALE");
    st.class_id = cls_id;
    st.section_id = sec_id;
    st.roll_number = 1;
    snprintf(st.status, sizeof(st.status), "ACTIVE");
    int st_id = 0;
    check(student_service_create(&st, &st_id) && st_id > 0, "Create local Student Aarav Sharma");

    long long ts_after_insert = get_single_int64("SELECT updated_at_ms FROM students WHERE admission_no = ?1;", st.admission_no);
    check(ts_after_insert > 0, "Insert trigger populated updated_at_ms on student");

    /* Update student -> verify outbox captures UPSERT and updated_at_ms increments */
    st.id = st_id;
    snprintf(st.full_name, sizeof(st.full_name), "Aarav Kumar Sharma");
    check(student_service_update(&st), "Update student full_name");
    long long ts_after_update = get_single_int64("SELECT updated_at_ms FROM students WHERE admission_no = ?1;", st.admission_no);
    check(ts_after_update > ts_after_insert, "Update trigger monotonically increased updated_at_ms");

    /* Delete temporary section -> verify tombstone + DELETE outbox entry */
    char temp_sec_uuid[SYNC_UUID_LEN] = {0};
    get_single_text("SELECT sync_uuid FROM sections WHERE name = ?1;", "Temp", temp_sec_uuid, sizeof(temp_sec_uuid));
    check(section_service_delete(temp_sec_id), "Delete temporary Section Temp");
    check(get_single_int64("SELECT COUNT(*) FROM sync_tombstones WHERE record_uuid = ?1;", temp_sec_uuid) == 1,
          "Delete trigger wrote tombstone into sync_tombstones");

    int pending = sync_core_get_pending_outbox_count();
    check(pending >= 6, "Outbox captured class + 2 sections + student insert + student update + section delete");

    SyncOutboxItem *items = (SyncOutboxItem *)calloc((size_t)pending, sizeof(SyncOutboxItem));
    assert(items != NULL);
    int fetched = 0;
    check(sync_core_fetch_pending_outbox(items, pending, &fetched) && fetched == pending,
          "sync_core_fetch_pending_outbox returns all pending items");

    /* Locate student UPSERT payload and verify FK UUIDs + JSON validity */
    bool found_stu_json = false;
    bool has_class_uuid = false;
    long long max_outbox_id = 0;
    for (int i = 0; i < fetched; i++) {
        if (items[i].id > max_outbox_id) max_outbox_id = items[i].id;
        if (strcmp(items[i].table_name, "students") == 0 && strcmp(items[i].operation, "UPSERT") == 0) {
            found_stu_json = (items[i].payload_json != NULL);
            has_class_uuid = (items[i].payload_json != NULL && strstr(items[i].payload_json, "\"class_uuid\":\"") != NULL);
        }
    }
    check(found_stu_json && has_class_uuid, "Serialized student JSON resolves parent class_id into class_uuid");

    char *push_json = NULL;
    check(sync_core_build_push_payload_json(items, fetched, &push_json) && push_json != NULL,
          "sync_core_build_push_payload_json built push envelope");
    check(get_single_int64("SELECT json_valid(?1) AND json_array_length(json_extract(?1, '$.changes')) > 0;", push_json) == 1,
          "Push payload JSON is RFC 8259 valid and contains non-empty $.changes array");
    check(strstr(push_json, "\"device_prefix\":\"D\"") != NULL,
          "Push payload JSON contains device_prefix metadata");

    free(push_json);
    sync_core_free_outbox_items(items, fetched);
    free(items);

    check(sync_core_mark_outbox_synced(max_outbox_id), "Mark outbox batch as synced");
    check(sync_core_get_pending_outbox_count() == 0, "Pending outbox count is 0 after ack");
}

static void test_remote_batch_fk_lww_append_only_and_tombstones(void)
{
    printf("\n4. Testing Remote Delta Batch, FK Resolution, Anti-Echo, LWW, Append-Only Fees & Delete...\n");

    char r_cls_uuid[SYNC_UUID_LEN] = {0};
    char r_sec_uuid[SYNC_UUID_LEN] = {0};
    char r_stu_uuid[SYNC_UUID_LEN] = {0};
    char r_fs_uuid[SYNC_UUID_LEN] = {0};
    char r_fee_uuid[SYNC_UUID_LEN] = {0};
    char r_set_uuid[SYNC_UUID_LEN] = {0};
    assert(sync_core_generate_uuid(r_cls_uuid, sizeof(r_cls_uuid)));
    assert(sync_core_generate_uuid(r_sec_uuid, sizeof(r_sec_uuid)));
    assert(sync_core_generate_uuid(r_stu_uuid, sizeof(r_stu_uuid)));
    assert(sync_core_generate_uuid(r_fs_uuid, sizeof(r_fs_uuid)));
    assert(sync_core_generate_uuid(r_fee_uuid, sizeof(r_fee_uuid)));
    assert(sync_core_generate_uuid(r_set_uuid, sizeof(r_set_uuid)));

    long long base_ts = sync_core_now_ms() + 10000LL;

    char p_cls[512];
    snprintf(p_cls, sizeof(p_cls),
             "{\"name\":\"Class 12\",\"numeric_value\":12,\"capacity\":45,\"academic_year_name\":\"2026-2027\"}");

    char p_sec[512];
    snprintf(p_sec, sizeof(p_sec),
             "{\"class_uuid\":\"%s\",\"name\":\"Science\",\"class_teacher_uuid\":null}",
             r_cls_uuid);

    char p_stu[1024];
    snprintf(p_stu, sizeof(p_stu),
             "{\"admission_no\":\"ADM-2026-M0001\",\"full_name\":\"Priya Verma\","
             "\"gender\":\"FEMALE\",\"admission_date\":\"2026-11-27\","
             "\"class_uuid\":\"%s\",\"section_uuid\":\"%s\","
             "\"roll_number\":7,\"status\":\"ACTIVE\"}",
             r_cls_uuid, r_sec_uuid);

    char p_fs[512];
    snprintf(p_fs, sizeof(p_fs),
             "{\"name\":\"Class 12 Tuition\",\"class_uuid\":\"%s\","
             "\"academic_year_name\":\"2026-2027\",\"fee_type\":\"MONTHLY\","
             "\"amount\":2500.0,\"due_date\":\"2026-12-10\"}",
             r_cls_uuid);

    char p_set[512];
    snprintf(p_set, sizeof(p_set),
             "{\"key\":\"school_name\",\"value\":\"Synced Android Academy\",\"description\":\"Official name\"}");

    SyncRemoteChange batch[5];
    memset(batch, 0, sizeof(batch));

    batch[0].server_seq = 101;
    snprintf(batch[0].table_name, sizeof(batch[0].table_name), "classes");
    snprintf(batch[0].record_uuid, sizeof(batch[0].record_uuid), "%s", r_cls_uuid);
    snprintf(batch[0].operation, sizeof(batch[0].operation), "UPSERT");
    snprintf(batch[0].origin_device_id, sizeof(batch[0].origin_device_id), "DEV-android01");
    batch[0].updated_at_ms = base_ts;
    batch[0].payload_json = p_cls;

    batch[1].server_seq = 102;
    snprintf(batch[1].table_name, sizeof(batch[1].table_name), "sections");
    snprintf(batch[1].record_uuid, sizeof(batch[1].record_uuid), "%s", r_sec_uuid);
    snprintf(batch[1].operation, sizeof(batch[1].operation), "UPSERT");
    snprintf(batch[1].origin_device_id, sizeof(batch[1].origin_device_id), "DEV-android01");
    batch[1].updated_at_ms = base_ts + 10;
    batch[1].payload_json = p_sec;

    batch[2].server_seq = 103;
    snprintf(batch[2].table_name, sizeof(batch[2].table_name), "students");
    snprintf(batch[2].record_uuid, sizeof(batch[2].record_uuid), "%s", r_stu_uuid);
    snprintf(batch[2].operation, sizeof(batch[2].operation), "UPSERT");
    snprintf(batch[2].origin_device_id, sizeof(batch[2].origin_device_id), "DEV-android01");
    batch[2].updated_at_ms = base_ts + 20;
    batch[2].payload_json = p_stu;

    batch[3].server_seq = 104;
    snprintf(batch[3].table_name, sizeof(batch[3].table_name), "fee_structures");
    snprintf(batch[3].record_uuid, sizeof(batch[3].record_uuid), "%s", r_fs_uuid);
    snprintf(batch[3].operation, sizeof(batch[3].operation), "UPSERT");
    snprintf(batch[3].origin_device_id, sizeof(batch[3].origin_device_id), "DEV-android01");
    batch[3].updated_at_ms = base_ts + 25;
    batch[3].payload_json = p_fs;

    batch[4].server_seq = 105;
    snprintf(batch[4].table_name, sizeof(batch[4].table_name), "settings");
    snprintf(batch[4].record_uuid, sizeof(batch[4].record_uuid), "%s", r_set_uuid);
    snprintf(batch[4].operation, sizeof(batch[4].operation), "UPSERT");
    snprintf(batch[4].origin_device_id, sizeof(batch[4].origin_device_id), "DEV-android01");
    batch[4].updated_at_ms = base_ts + 30;
    batch[4].payload_json = p_set;

    SyncApplyResult res = {0};
    check(sync_core_apply_remote_batch(batch, 5, &res), "Apply remote batch (class + section + student + fee_structure + seeded settings merge)");
    check(res.applied_count == 5 && res.skipped_conflict_count == 0 && res.error_count == 0 && res.last_server_seq == 105,
          "Batch result counters & last_server_seq == 105 match");
    check(sync_core_get_pending_outbox_count() == 0,
          "Anti-echo guard: ZERO new outbox entries generated from remote batch apply");

    Student remote_st = {0};
    check(student_service_get_by_admission_no("ADM-2026-M0001", &remote_st)
          && strcmp(remote_st.full_name, "Priya Verma") == 0
          && strcmp(remote_st.class_name, "Class 12") == 0
          && strcmp(remote_st.section_name, "Science") == 0,
          "Remote student inserted with resolved local class_id ('Class 12') and section_id ('Science')");

    char school_val[128] = {0};
    get_single_text("SELECT value FROM settings WHERE key = ?1;", "school_name", school_val, sizeof(school_val));
    check(strcmp(school_val, "Synced Android Academy") == 0,
          "Natural-key merge updated seeded settings row without UNIQUE conflict");

    /* LWW: Stale remote update must be rejected */
    SyncRemoteChange stale_upd = {0};
    snprintf(stale_upd.table_name, sizeof(stale_upd.table_name), "students");
    snprintf(stale_upd.record_uuid, sizeof(stale_upd.record_uuid), "%s", r_stu_uuid);
    snprintf(stale_upd.operation, sizeof(stale_upd.operation), "UPSERT");
    snprintf(stale_upd.origin_device_id, sizeof(stale_upd.origin_device_id), "DEV-android02");
    stale_upd.updated_at_ms = base_ts - 5000LL;
    char p_stale[512];
    snprintf(p_stale, sizeof(p_stale),
             "{\"admission_no\":\"ADM-2026-M0001\",\"full_name\":\"Stale Name\",\"class_uuid\":\"%s\",\"section_uuid\":\"%s\"}",
             r_cls_uuid, r_sec_uuid);
    stale_upd.payload_json = p_stale;

    bool applied = true;
    check(sync_core_apply_remote_change(&stale_upd, &applied) && applied == false,
          "LWW rejects stale remote student update");
    check(student_service_get_by_admission_no("ADM-2026-M0001", &remote_st)
          && strcmp(remote_st.full_name, "Priya Verma") == 0,
          "Student name remains untouched after stale update rejection");

    /* LWW: Newer remote update wins */
    SyncRemoteChange newer_upd = stale_upd;
    newer_upd.updated_at_ms = base_ts + 5000LL;
    char p_newer[512];
    snprintf(p_newer, sizeof(p_newer),
             "{\"admission_no\":\"ADM-2026-M0001\",\"full_name\":\"Priya Verma Updated\",\"class_uuid\":\"%s\",\"section_uuid\":\"%s\"}",
             r_cls_uuid, r_sec_uuid);
    newer_upd.payload_json = p_newer;
    check(sync_core_apply_remote_change(&newer_upd, &applied) && applied == true,
          "LWW accepts newer remote student update");
    check(student_service_get_by_admission_no("ADM-2026-M0001", &remote_st)
          && strcmp(remote_st.full_name, "Priya Verma Updated") == 0,
          "Student name updated cleanly to newer remote value");

    /* Append-Only Fee Ledger Protection */
    char p_fee1[512];
    snprintf(p_fee1, sizeof(p_fee1),
             "{\"receipt_no\":\"RCP-2026-M0001\",\"student_uuid\":\"%s\",\"fee_structure_uuid\":\"%s\","
             "\"amount_paid\":2500.0,\"discount\":0.0,\"late_fine\":0.0,\"total_amount\":2500.0,"
             "\"payment_method\":\"UPI\",\"payment_date\":\"2026-11-27\",\"collected_by_username\":\"admin\"}",
             r_stu_uuid, r_fs_uuid);
    SyncRemoteChange fee_ins = {0};
    snprintf(fee_ins.table_name, sizeof(fee_ins.table_name), "fee_payments");
    snprintf(fee_ins.record_uuid, sizeof(fee_ins.record_uuid), "%s", r_fee_uuid);
    snprintf(fee_ins.operation, sizeof(fee_ins.operation), "UPSERT");
    snprintf(fee_ins.origin_device_id, sizeof(fee_ins.origin_device_id), "DEV-android01");
    fee_ins.updated_at_ms = base_ts + 6000LL;
    fee_ins.payload_json = p_fee1;
    check(sync_core_apply_remote_change(&fee_ins, &applied) && applied == true,
          "Remote append of new fee receipt RCP-2026-M0001 succeeds");

    char p_fee_tamper[512];
    snprintf(p_fee_tamper, sizeof(p_fee_tamper),
             "{\"receipt_no\":\"RCP-2026-M0001\",\"student_uuid\":\"%s\",\"fee_structure_uuid\":\"%s\","
             "\"amount_paid\":99999.0,\"total_amount\":99999.0,"
             "\"payment_method\":\"CASH\",\"payment_date\":\"2026-11-27\"}",
             r_stu_uuid, r_fs_uuid);
    fee_ins.updated_at_ms = base_ts + 9000LL;
    fee_ins.payload_json = p_fee_tamper;
    check(sync_core_apply_remote_change(&fee_ins, &applied) && applied == false,
          "Append-only guard rejects overwriting existing fee receipt");

    FeePayment pay = {0};
    check(fee_payment_get_by_receipt("RCP-2026-M0001", &pay) && pay.total_amount == 2500.0,
          "Original fee payment amount (2500.0) stays immutable");

    /* Delete fee receipt first, then test remote DELETE on student */
    SyncRemoteChange del_fee = {0};
    snprintf(del_fee.table_name, sizeof(del_fee.table_name), "fee_payments");
    snprintf(del_fee.record_uuid, sizeof(del_fee.record_uuid), "%s", r_fee_uuid);
    snprintf(del_fee.operation, sizeof(del_fee.operation), "DELETE");
    snprintf(del_fee.origin_device_id, sizeof(del_fee.origin_device_id), "DEV-android01");
    del_fee.updated_at_ms = base_ts + 10000LL;
    check(sync_core_apply_remote_change(&del_fee, &applied) && applied == true,
          "Remote DELETE on fee receipt succeeds before student delete");

    SyncRemoteChange del_stu = {0};
    snprintf(del_stu.table_name, sizeof(del_stu.table_name), "students");
    snprintf(del_stu.record_uuid, sizeof(del_stu.record_uuid), "%s", r_stu_uuid);
    snprintf(del_stu.operation, sizeof(del_stu.operation), "DELETE");
    snprintf(del_stu.origin_device_id, sizeof(del_stu.origin_device_id), "DEV-android01");
    del_stu.updated_at_ms = base_ts + 11000LL;
    check(sync_core_apply_remote_change(&del_stu, &applied) && applied == true,
          "Remote DELETE tombstone removes student locally");
    check(!student_service_get_by_admission_no("ADM-2026-M0001", &remote_st),
          "Deleted remote student no longer queryable");
    check(sync_core_get_pending_outbox_count() == 0,
          "Anti-echo guard still intact after LWW, fee, and DELETE operations");

    /* Verify dangling bogus FK fails cleanly & restores sync_applying_remote='0' */
    char bogus_uuid[SYNC_UUID_LEN] = {0};
    assert(sync_core_generate_uuid(bogus_uuid, sizeof(bogus_uuid)));
    SyncRemoteChange bad_fk = {0};
    snprintf(bad_fk.table_name, sizeof(bad_fk.table_name), "students");
    snprintf(bad_fk.record_uuid, sizeof(bad_fk.record_uuid), "%s", bogus_uuid);
    snprintf(bad_fk.operation, sizeof(bad_fk.operation), "UPSERT");
    snprintf(bad_fk.origin_device_id, sizeof(bad_fk.origin_device_id), "DEV-android01");
    bad_fk.updated_at_ms = base_ts + 12000LL;
    bad_fk.payload_json = "{\"admission_no\":\"ADM-BAD-1\",\"full_name\":\"Ghost\",\"class_uuid\":\"00000000-0000-4000-8000-000000000000\"}";
    check(sync_core_apply_remote_change(&bad_fk, &applied) == false,
          "Remote change with unresolvable parent UUID fails gracefully");

    char flag[16] = {0};
    sync_core_get_state("sync_applying_remote", flag, sizeof(flag));
    check(strcmp(flag, "0") == 0,
          "Anti-echo flag cleanly restored to '0' after failed remote change");
}

int main(void)
{
    remove(TEST_DB);

    printf("============================================================\n");
    printf("   SchoolCMS Sync Core Engine & Schema V3 Test Suite\n");
    printf("============================================================\n");

    if (db_init(TEST_DB) != DB_SUCCESS) {
        fprintf(stderr, "Failed to initialize test database\n");
        return 1;
    }

    test_schema_v3_and_identity();
    test_prefixed_code_generation();
    test_triggers_outbox_serialization_and_push_bundle();
    test_remote_batch_fk_lww_append_only_and_tombstones();

    db_close();
    remove(TEST_DB);

    printf("\n============================================================\n");
    printf("   ALL %d SYNC UNIT TEST ASSERTIONS PASSED!\n", g_pass);
    printf("============================================================\n");
    return 0;
}
