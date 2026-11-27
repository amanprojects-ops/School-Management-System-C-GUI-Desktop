/**
 * @file sync.h
 * @brief Multi-device offline-first delta sync engine interfaces (schema, outbox, UUID resolution, LWW merge).
 */

#ifndef SCHOOLCMS_SYNC_H
#define SCHOOLCMS_SYNC_H

#include <stdbool.h>
#include <stddef.h>

#define SYNC_UUID_LEN 40
#define SYNC_DEVICE_ID_LEN 32
#define SYNC_MAX_BATCH 100

/**
 * @brief Local queued outbox mutation awaiting push to cPanel sync backend.
 */
typedef struct {
    long long id;
    char table_name[48];
    char record_uuid[SYNC_UUID_LEN];
    char operation[16];          /* "UPSERT" or "DELETE" */
    char *payload_json;          /* Heap-allocated JSON string (freed via sync_core_free_outbox_items) */
    long long updated_at_ms;
    int synced;
} SyncOutboxItem;

/**
 * @brief Incoming remote mutation pulled from cPanel sync backend.
 */
typedef struct {
    long long server_seq;
    char table_name[48];
    char record_uuid[SYNC_UUID_LEN];
    char operation[16];          /* "UPSERT" or "DELETE" */
    char origin_device_id[SYNC_DEVICE_ID_LEN];
    long long updated_at_ms;
    const char *payload_json;    /* Borrowed JSON object string */
} SyncRemoteChange;

/**
 * @brief Result counters for applying remote changes.
 */
typedef struct {
    int applied_count;
    int skipped_conflict_count;
    int error_count;
    long long last_server_seq;
} SyncApplyResult;

/* ── Identity, Time & Config Helpers ──────────────────────────────── */

bool sync_core_generate_uuid(char *out_uuid, size_t size);
long long sync_core_now_ms(void);
bool sync_core_get_device_id(char *out_id, size_t size);
bool sync_core_get_device_prefix(char *out_prefix, size_t size);
bool sync_core_get_state(const char *key, char *out_val, size_t size);
bool sync_core_set_state(const char *key, const char *value);

/* ── Outbox Query & Push Payload Serialization ────────────────────── */

int sync_core_get_pending_outbox_count(void);

bool sync_core_fetch_pending_outbox(SyncOutboxItem *out_items,
                                    int max_items,
                                    int *out_count);

void sync_core_free_outbox_items(SyncOutboxItem *items, int count);

/**
 * @brief Serialize live database row (resolving FK parent IDs to parent sync_uuids) into JSON.
 * Caller frees *out_json using free().
 */
bool sync_core_serialize_row_json(const char *table_name,
                                  const char *record_uuid,
                                  char **out_json);

/**
 * @brief Build batched push JSON document:
 *        {"device_id":"...","device_prefix":"D","changes":[...]}
 * Caller frees *out_json using free().
 */
bool sync_core_build_push_payload_json(const SyncOutboxItem *items,
                                       int count,
                                       char **out_json);

bool sync_core_mark_outbox_synced(long long max_outbox_id);

/* ── Remote Delta Applier (LWW + Tombstones + Anti-Echo) ──────────── */

/**
 * @brief Apply a single remote change under sync_applying_remote='1'.
 * @param out_applied Optional: true if inserted/updated/deleted, false if skipped due to newer local timestamp.
 */
bool sync_core_apply_remote_change(const SyncRemoteChange *change, bool *out_applied);

/**
 * @brief Atomically apply a batch of remote changes in one SQLite transaction and update last_server_seq.
 */
bool sync_core_apply_remote_batch(const SyncRemoteChange *changes,
                                  int count,
                                  SyncApplyResult *out_result);

#endif /* SCHOOLCMS_SYNC_H */
