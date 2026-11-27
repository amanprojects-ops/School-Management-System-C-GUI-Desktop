/**
 * @file sync_engine.h
 * @brief In-process Windows background WinHTTP sync engine & lifecycle control.
 *
 * Runs inside APCOACHING.exe via GLib GThread + Windows WinHTTP (zero external
 * background processes), using event-driven ~1s debounced push on local DB
 * mutations and 20s adaptive background pull, dispatching GTK UI refresh
 * notifications via g_idle_add().
 */

#ifndef SCHOOLCMS_SYNC_ENGINE_H
#define SCHOOLCMS_SYNC_ENGINE_H

#include "sync/sync.h"
#include <glib.h>
#include <stdbool.h>
#include <stddef.h>

typedef enum {
    SYNC_STATUS_DISABLED = 0,
    SYNC_STATUS_IDLE,
    SYNC_STATUS_SYNCING,
    SYNC_STATUS_OK,
    SYNC_STATUS_OFFLINE,
    SYNC_STATUS_AUTH_ERROR,
    SYNC_STATUS_ERROR
} SyncStatusCode;

typedef struct {
    SyncStatusCode code;
    bool enabled;
    char cpanel_url[256];
    char device_id[SYNC_DEVICE_ID_LEN];
    char device_prefix[8];
    long long last_server_seq;
    int pending_outbox;
    int last_pushed_count;
    int last_pulled_count;
    char last_sync_at[64];
    char message[256];
} SyncEngineStatus;

/** Invoked on GTK main thread via g_idle_add() after remote changes are applied locally. */
typedef void (*SyncRemoteAppliedCallback)(int applied_count, gpointer user_data);

/** Invoked on GTK main thread via g_idle_add() whenever live sync status changes. */
typedef void (*SyncStatusChangedCallback)(const SyncEngineStatus *status, gpointer user_data);

/* ── Configuration Persistence (sync_state) ───────────────────────── */

bool sync_engine_get_config(char *out_url, size_t url_sz,
                            char *out_token, size_t tok_sz,
                            char *out_hmac, size_t hmac_sz,
                            bool *out_enabled);

bool sync_engine_save_config(const char *cpanel_url,
                             const char *device_token,
                             const char *hmac_secret,
                             bool enabled);

void sync_engine_get_status(SyncEngineStatus *out_status);

/* ── Callback & Background Thread Lifecycle ───────────────────────── */

void sync_engine_set_remote_callback(SyncRemoteAppliedCallback cb, gpointer user_data);
void sync_engine_set_status_callback(SyncStatusChangedCallback cb, gpointer user_data);

void sync_engine_start(void);
void sync_engine_stop(void);
void sync_engine_trigger_now(void);
bool sync_engine_is_running(void);

/* ── Direct Blocking Operations (Safe for Async Worker or Tests) ─── */

bool sync_engine_test_connection(const char *cpanel_url,
                                 const char *device_token,
                                 const char *hmac_secret,
                                 char *out_msg,
                                 size_t msg_sz);

bool sync_engine_sync_once_blocking(char *out_msg, size_t msg_sz);

#endif /* SCHOOLCMS_SYNC_ENGINE_H */
