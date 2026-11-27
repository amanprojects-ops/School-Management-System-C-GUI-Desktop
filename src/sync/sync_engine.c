/**
 * @file sync_engine.c
 * @brief In-process Windows WinHTTP delta sync transport + GLib background loop.
 *
 * Zero external daemon processes:
 *  - Connects to cPanel HTTPS PHP APIs (/api/health.php, /api/push.php, /api/pull.php)
 *  - Hooks SQLite inserts into sync_outbox for ~1s event-driven push debounce
 *  - Polls remote deltas adaptively every 20 seconds when idle
 *  - Computes optional HMAC-SHA256 request signatures using OpenSSL EVP_Q_mac
 *  - Notifies GTK UI on main thread via g_idle_add() when remote rows arrive
 */

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include "sync/sync_engine.h"
#include "database.h"

#include <windows.h>
#include <winhttp.h>
#include <openssl/evp.h>
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define SYNC_PULL_INTERVAL_SEC  20
#define SYNC_PUSH_DEBOUNCE_MS   900
#define SYNC_MAX_HTTP_BYTES     (8 * 1024 * 1024)

/* ── Thread & UI Callback State ───────────────────────────────────── */

static GThread *s_worker_thread = NULL;
static GMutex   s_wake_mutex;
static GCond    s_wake_cond;
static bool     s_stop_requested = false;
static bool     s_dirty_push = false;
static bool     s_force_sync = false;

static GMutex   s_exec_mutex;

static GMutex   s_state_mutex;
static SyncEngineStatus s_cached_status = {0};

static SyncRemoteAppliedCallback s_remote_cb = NULL;
static gpointer                  s_remote_ud = NULL;
static SyncStatusChangedCallback s_status_cb = NULL;
static gpointer                  s_status_ud = NULL;

/* ── UI Idle Notification Bridges ─────────────────────────────────── */

typedef struct {
    SyncRemoteAppliedCallback cb;
    gpointer ud;
    int applied_count;
} IdleRemotePayload;

static gboolean idle_dispatch_remote(gpointer data)
{
    IdleRemotePayload *p = (IdleRemotePayload *)data;
    g_mutex_lock(&s_state_mutex);
    SyncRemoteAppliedCallback cur_cb = s_remote_cb;
    gpointer cur_ud = s_remote_ud;
    g_mutex_unlock(&s_state_mutex);

    if (cur_cb && cur_cb == p->cb) {
        cur_cb(p->applied_count, cur_ud);
    }
    g_free(p);
    return G_SOURCE_REMOVE;
}

typedef struct {
    SyncStatusChangedCallback cb;
    gpointer ud;
    SyncEngineStatus status;
} IdleStatusPayload;

static gboolean idle_dispatch_status(gpointer data)
{
    IdleStatusPayload *p = (IdleStatusPayload *)data;
    g_mutex_lock(&s_state_mutex);
    SyncStatusChangedCallback cur_cb = s_status_cb;
    gpointer cur_ud = s_status_ud;
    g_mutex_unlock(&s_state_mutex);

    if (cur_cb && cur_cb == p->cb) {
        cur_cb(&p->status, cur_ud);
    }
    g_free(p);
    return G_SOURCE_REMOVE;
}

static void publish_status(SyncStatusCode code,
                           int pushed_delta,
                           int pulled_delta,
                           const char *msg)
{
    SyncEngineStatus snap;
    memset(&snap, 0, sizeof(snap));

    char seq_buf[32] = "0";
    char url_buf[256] = {0};
    char tok_buf[256] = {0};
    char hmac_buf[256] = {0};
    bool enabled = false;

    db_lock();
    sync_engine_get_config(url_buf, sizeof(url_buf), tok_buf, sizeof(tok_buf),
                           hmac_buf, sizeof(hmac_buf), &enabled);
    sync_core_get_device_id(snap.device_id, sizeof(snap.device_id));
    sync_core_get_device_prefix(snap.device_prefix, sizeof(snap.device_prefix));
    sync_core_get_state("last_server_seq", seq_buf, sizeof(seq_buf));
    sync_core_get_state("last_sync_at", snap.last_sync_at, sizeof(snap.last_sync_at));
    int pending = sync_core_get_pending_outbox_count();
    db_unlock();

    snap.code = code;
    snap.enabled = enabled;
    snprintf(snap.cpanel_url, sizeof(snap.cpanel_url), "%s", url_buf);
    snap.last_server_seq = strtoll(seq_buf, NULL, 10);
    snap.pending_outbox = pending >= 0 ? pending : 0;

    g_mutex_lock(&s_state_mutex);
    if (pushed_delta >= 0) {
        s_cached_status.last_pushed_count = pushed_delta;
    }
    if (pulled_delta >= 0) {
        s_cached_status.last_pulled_count = pulled_delta;
    }
    snap.last_pushed_count = s_cached_status.last_pushed_count;
    snap.last_pulled_count = s_cached_status.last_pulled_count;
    snprintf(snap.message, sizeof(snap.message), "%s", msg ? msg : "");
    s_cached_status = snap;

    SyncStatusChangedCallback cb = s_status_cb;
    gpointer ud = s_status_ud;
    g_mutex_unlock(&s_state_mutex);

    if (cb) {
        IdleStatusPayload *ip = g_new0(IdleStatusPayload, 1);
        ip->cb = cb;
        ip->ud = ud;
        ip->status = snap;
        g_idle_add(idle_dispatch_status, ip);
    }
}

static void notify_remote_applied(int applied_count)
{
    if (applied_count <= 0) return;

    g_mutex_lock(&s_state_mutex);
    SyncRemoteAppliedCallback cb = s_remote_cb;
    gpointer ud = s_remote_ud;
    g_mutex_unlock(&s_state_mutex);

    if (cb) {
        IdleRemotePayload *ip = g_new0(IdleRemotePayload, 1);
        ip->cb = cb;
        ip->ud = ud;
        ip->applied_count = applied_count;
        g_idle_add(idle_dispatch_remote, ip);
    }
}

/* ── SQLite Outbox Insert Hook (~1s Event-Driven Wakeup) ───────────── */

static void on_sqlite_update_hook(void *user_data,
                                  int operation,
                                  const char *db_name,
                                  const char *table_name,
                                  sqlite3_int64 rowid)
{
    (void)user_data;
    (void)db_name;
    (void)rowid;

    if (operation == SQLITE_INSERT && table_name && strcmp(table_name, "sync_outbox") == 0) {
        g_mutex_lock(&s_wake_mutex);
        s_dirty_push = true;
        g_cond_signal(&s_wake_cond);
        g_mutex_unlock(&s_wake_mutex);
    }
}

/* ── String, HMAC & WinHTTP Helpers ───────────────────────────────── */

static wchar_t *utf8_to_wide(const char *str)
{
    if (!str) return NULL;
    int needed = MultiByteToWideChar(CP_UTF8, 0, str, -1, NULL, 0);
    if (needed <= 0) return NULL;
    wchar_t *w = (wchar_t *)malloc((size_t)needed * sizeof(wchar_t));
    if (!w) return NULL;
    MultiByteToWideChar(CP_UTF8, 0, str, -1, w, needed);
    return w;
}

static bool compute_hmac_sha256_hex(const char *secret,
                                    const char *ts_str,
                                    const char *payload_or_query,
                                    char *out_hex,
                                    size_t hex_size)
{
    if (!secret || !secret[0] || !ts_str || !out_hex || hex_size < 65) return false;

    size_t p_len = payload_or_query ? strlen(payload_or_query) : 0;
    size_t msg_len = strlen(ts_str) + 1 + p_len;
    unsigned char *msg = (unsigned char *)malloc(msg_len + 1);
    if (!msg) return false;

    memcpy(msg, ts_str, strlen(ts_str));
    msg[strlen(ts_str)] = '\n';
    if (p_len > 0) {
        memcpy(msg + strlen(ts_str) + 1, payload_or_query, p_len);
    }
    msg[msg_len] = '\0';

    unsigned char digest[64];
    size_t out_len = 0;
    unsigned char *res = EVP_Q_mac(NULL, "HMAC", NULL, "SHA256", NULL,
                                   secret, strlen(secret),
                                   msg, msg_len,
                                   digest, sizeof(digest), &out_len);
    free(msg);

    if (!res || out_len != 32) return false;

    for (size_t i = 0; i < out_len; i++) {
        snprintf(out_hex + (i * 2), 3, "%02x", digest[i]);
    }
    out_hex[64] = '\0';
    return true;
}

static void format_utc_timestamp(char *out_buf, size_t size)
{
    time_t now = time(NULL);
    struct tm tm_buf;
    struct tm *lt = localtime(&now);
    if (lt) {
        tm_buf = *lt;
        strftime(out_buf, size, "%Y-%m-%d %H:%M:%S", &tm_buf);
    } else {
        snprintf(out_buf, size, "%lld", (long long)now);
    }
}

static void normalize_base_url(const char *raw_url, char *out_base, size_t size)
{
    if (!raw_url || !out_base || size == 0) return;
    snprintf(out_base, size, "%s", raw_url);

    /* Trim whitespace and trailing slashes */
    size_t len = strlen(out_base);
    while (len > 0 && (out_base[len - 1] == ' ' || out_base[len - 1] == '\t' ||
                       out_base[len - 1] == '\r' || out_base[len - 1] == '\n' ||
                       out_base[len - 1] == '/')) {
        out_base[--len] = '\0';
    }

    /* Be forgiving if user pasted .../api cleanly */
    if (len >= 4 && strcmp(out_base + len - 4, "/api") == 0) {
        out_base[len - 4] = '\0';
    }
}

typedef struct {
    DWORD status_code;
    char *body;
    size_t body_len;
    long long header_last_seq;
    bool header_has_more;
    DWORD win32_err;
} HttpResponse;

static void http_response_free(HttpResponse *resp)
{
    if (resp && resp->body) {
        free(resp->body);
        resp->body = NULL;
        resp->body_len = 0;
    }
}

static bool winhttp_request_json(const char *base_url,
                                 const char *endpoint_path_and_query,
                                 const char *method,
                                 const char *device_id,
                                 const char *bearer_token,
                                 const char *hmac_secret,
                                 const char *post_body,
                                 HttpResponse *out_resp)
{
    memset(out_resp, 0, sizeof(*out_resp));
    out_resp->header_last_seq = -1;

    char clean_base[256];
    normalize_base_url(base_url, clean_base, sizeof(clean_base));
    if (clean_base[0] == '\0') {
        out_resp->win32_err = ERROR_INVALID_PARAMETER;
        return false;
    }

    char full_url[1024];
    snprintf(full_url, sizeof(full_url), "%s%s", clean_base, endpoint_path_and_query);

    wchar_t *w_full_url = utf8_to_wide(full_url);
    if (!w_full_url) return false;

    wchar_t scheme_buf[16] = {0};
    wchar_t host_buf[256] = {0};
    wchar_t path_buf[1024] = {0};

    URL_COMPONENTSW uc;
    memset(&uc, 0, sizeof(uc));
    uc.dwStructSize = sizeof(uc);
    uc.lpszScheme = scheme_buf;
    uc.dwSchemeLength = (DWORD)(sizeof(scheme_buf) / sizeof(wchar_t));
    uc.lpszHostName = host_buf;
    uc.dwHostNameLength = (DWORD)(sizeof(host_buf) / sizeof(wchar_t));
    uc.lpszUrlPath = path_buf;
    uc.dwUrlPathLength = (DWORD)(sizeof(path_buf) / sizeof(wchar_t));
    uc.lpszExtraInfo = NULL;
    uc.dwExtraInfoLength = 0;

    if (!WinHttpCrackUrl(w_full_url, 0, 0, &uc)) {
        out_resp->win32_err = GetLastError();
        free(w_full_url);
        return false;
    }
    free(w_full_url);

    HINTERNET hSession = WinHttpOpen(L"SchoolCMS-APCOACHING/2.0",
                                     WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                     WINHTTP_NO_PROXY_NAME,
                                     WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) {
        out_resp->win32_err = GetLastError();
        return false;
    }

    WinHttpSetTimeouts(hSession, 5000, 8000, 10000, 15000);

    HINTERNET hConnect = WinHttpConnect(hSession, host_buf, uc.nPort, 0);
    if (!hConnect) {
        out_resp->win32_err = GetLastError();
        WinHttpCloseHandle(hSession);
        return false;
    }

    wchar_t w_method[16];
    MultiByteToWideChar(CP_UTF8, 0, method, -1, w_method, 16);

    DWORD req_flags = (uc.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, w_method, path_buf,
                                            NULL, WINHTTP_NO_REFERER,
                                            WINHTTP_DEFAULT_ACCEPT_TYPES,
                                            req_flags);
    if (!hRequest) {
        out_resp->win32_err = GetLastError();
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    /* Build HTTP Headers cleanly */
    char hdr_buf[2048];
    int hlen = snprintf(hdr_buf, sizeof(hdr_buf),
                        "Accept: application/json\r\n"
                        "Authorization: Bearer %s\r\n"
                        "X-Device-Id: %s\r\n",
                        bearer_token ? bearer_token : "",
                        device_id ? device_id : "");

    if (post_body != NULL && hlen > 0 && (size_t)hlen < sizeof(hdr_buf)) {
        hlen += snprintf(hdr_buf + hlen, sizeof(hdr_buf) - (size_t)hlen,
                         "Content-Type: application/json; charset=utf-8\r\n");
    }

    if (hmac_secret && hmac_secret[0] != '\0' && hlen > 0 && (size_t)hlen < sizeof(hdr_buf)) {
        const char *q = strchr(endpoint_path_and_query, '?');
        const char *signed_part = (strcmp(method, "POST") == 0)
                                    ? (post_body ? post_body : "")
                                    : (q ? (q + 1) : "");
        char ts_str[32];
        snprintf(ts_str, sizeof(ts_str), "%lld", (long long)time(NULL));

        char sig_hex[68] = {0};
        if (compute_hmac_sha256_hex(hmac_secret, ts_str, signed_part, sig_hex, sizeof(sig_hex))) {
            snprintf(hdr_buf + hlen, sizeof(hdr_buf) - (size_t)hlen,
                     "X-Sync-Timestamp: %s\r\nX-Sync-Signature: %s\r\n",
                     ts_str, sig_hex);
        }
    }

    wchar_t *w_headers = utf8_to_wide(hdr_buf);
    DWORD body_len = post_body ? (DWORD)strlen(post_body) : 0;

    BOOL sent = WinHttpSendRequest(hRequest,
                                   w_headers ? w_headers : WINHTTP_NO_ADDITIONAL_HEADERS,
                                   w_headers ? (DWORD)-1L : 0,
                                   post_body ? (LPVOID)post_body : WINHTTP_NO_REQUEST_DATA,
                                   body_len, body_len, 0);
    if (w_headers) free(w_headers);

    if (!sent || !WinHttpReceiveResponse(hRequest, NULL)) {
        out_resp->win32_err = GetLastError();
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    DWORD status = 0;
    DWORD status_sz = sizeof(status);
    WinHttpQueryHeaders(hRequest,
                        WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                        WINHTTP_HEADER_NAME_BY_INDEX,
                        &status, &status_sz, WINHTTP_NO_HEADER_INDEX);
    out_resp->status_code = status;

    wchar_t w_hdr_val[64] = {0};
    DWORD w_hdr_sz = sizeof(w_hdr_val);
    if (WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_CUSTOM, L"X-Sync-Last-Seq",
                            w_hdr_val, &w_hdr_sz, WINHTTP_NO_HEADER_INDEX)) {
        char seq_tmp[64] = {0};
        WideCharToMultiByte(CP_UTF8, 0, w_hdr_val, -1, seq_tmp, sizeof(seq_tmp), NULL, NULL);
        if (seq_tmp[0] != '\0') {
            out_resp->header_last_seq = strtoll(seq_tmp, NULL, 10);
        }
    }

    w_hdr_sz = sizeof(w_hdr_val);
    memset(w_hdr_val, 0, sizeof(w_hdr_val));
    if (WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_CUSTOM, L"X-Sync-Has-More",
                            w_hdr_val, &w_hdr_sz, WINHTTP_NO_HEADER_INDEX)) {
        out_resp->header_has_more = (w_hdr_val[0] == L'1');
    }

    if (status != 204) {
        size_t cap = 4096;
        char *buf = (char *)malloc(cap);
        size_t total = 0;
        if (!buf) {
            WinHttpCloseHandle(hRequest);
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return false;
        }

        for (;;) {
            DWORD avail = 0;
            if (!WinHttpQueryDataAvailable(hRequest, &avail) || avail == 0) break;
            if (total + avail + 1 > SYNC_MAX_HTTP_BYTES) break;

            if (total + avail + 1 > cap) {
                size_t new_cap = cap * 2;
                while (new_cap < total + avail + 1) new_cap *= 2;
                char *resized = (char *)realloc(buf, new_cap);
                if (!resized) break;
                buf = resized;
                cap = new_cap;
            }

            DWORD read_now = 0;
            if (!WinHttpReadData(hRequest, buf + total, avail, &read_now) || read_now == 0) break;
            total += read_now;
        }
        buf[total] = '\0';
        out_resp->body = buf;
        out_resp->body_len = total;
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return true;
}

static bool is_winhttp_network_err(DWORD err)
{
    return err == ERROR_WINHTTP_CANNOT_CONNECT ||
           err == ERROR_WINHTTP_NAME_NOT_RESOLVED ||
           err == ERROR_WINHTTP_TIMEOUT ||
           err == ERROR_WINHTTP_CONNECTION_ERROR;
}

/* ── Push & Pull JSON SQLite Helpers ──────────────────────────────── */

static bool mark_acknowledged_ids_in_db(const char *response_json, long long fallback_max_id)
{
    sqlite3 *db = db_get_handle();
    if (!db) return false;

    bool marked_specific = false;
    if (response_json && response_json[0] != '\0') {
        const char *ack_sql =
            "UPDATE sync_outbox SET synced = 1 "
            "WHERE synced = 0 AND id IN ("
            "  SELECT CAST(value AS INTEGER) FROM json_each(json_extract(?1, '$.acknowledged_ids'))"
            ");";
        sqlite3_stmt *stmt = NULL;
        if (sqlite3_prepare_v2(db, ack_sql, -1, &stmt, NULL) == SQLITE_OK) {
            sqlite3_bind_text(stmt, 1, response_json, -1, SQLITE_STATIC);
            if (sqlite3_step(stmt) == SQLITE_DONE && sqlite3_changes(db) > 0) {
                marked_specific = true;
            }
            sqlite3_finalize(stmt);
        }
    }

    if (!marked_specific && fallback_max_id > 0) {
        return sync_core_mark_outbox_synced(fallback_max_id);
    }
    return true;
}

static bool sync_push_pending_batches(const char *base_url,
                                      const char *device_id,
                                      const char *token,
                                      const char *hmac_secret,
                                      int *out_pushed,
                                      SyncStatusCode *out_err_code,
                                      char *out_err_msg,
                                      size_t err_sz)
{
    *out_pushed = 0;

    for (;;) {
        SyncOutboxItem *items = (SyncOutboxItem *)calloc(SYNC_MAX_BATCH, sizeof(SyncOutboxItem));
        if (!items) return false;

        int count = 0;
        char *push_json = NULL;

        db_lock();
        bool ok_fetch = sync_core_fetch_pending_outbox(items, SYNC_MAX_BATCH, &count);
        bool ok_build = (ok_fetch && count > 0)
                            ? sync_core_build_push_payload_json(items, count, &push_json)
                            : false;
        db_unlock();

        if (!ok_fetch) {
            free(items);
            snprintf(out_err_msg, err_sz, "Failed reading local outbox.");
            *out_err_code = SYNC_STATUS_ERROR;
            return false;
        }

        if (count == 0) {
            free(items);
            return true;
        }

        long long batch_max_id = 0;
        for (int i = 0; i < count; i++) {
            if (items[i].id > batch_max_id) batch_max_id = items[i].id;
        }
        sync_core_free_outbox_items(items, count);
        free(items);

        if (!ok_build || !push_json) {
            snprintf(out_err_msg, err_sz, "Failed building push JSON payload.");
            *out_err_code = SYNC_STATUS_ERROR;
            return false;
        }

        HttpResponse resp;
        bool http_ok = winhttp_request_json(base_url, "/api/push.php", "POST",
                                            device_id, token, hmac_secret,
                                            push_json, &resp);
        free(push_json);

        if (!http_ok) {
            *out_err_code = is_winhttp_network_err(resp.win32_err) ? SYNC_STATUS_OFFLINE : SYNC_STATUS_ERROR;
            snprintf(out_err_msg, err_sz, "Offline or unreachable during push (WinHTTP %lu).",
                     (unsigned long)resp.win32_err);
            http_response_free(&resp);
            return false;
        }

        if (resp.status_code == 401 || resp.status_code == 403) {
            *out_err_code = SYNC_STATUS_AUTH_ERROR;
            snprintf(out_err_msg, err_sz, "Authentication failed (HTTP %lu): check Device Token / HMAC.",
                     (unsigned long)resp.status_code);
            http_response_free(&resp);
            return false;
        }

        if (resp.status_code != 200 || !resp.body) {
            *out_err_code = SYNC_STATUS_ERROR;
            snprintf(out_err_msg, err_sz, "Server rejected push (HTTP %lu).", (unsigned long)resp.status_code);
            http_response_free(&resp);
            return false;
        }

        db_lock();
        sqlite3 *db = db_get_handle();
        int api_ok = 0;
        int accepted = 0;
        int skipped = 0;
        int err_cnt = 0;
        char srv_err[192] = {0};

        if (db) {
            sqlite3_stmt *stmt = NULL;
            const char *psql =
                "SELECT COALESCE(json_extract(?1, '$.ok'), 0),"
                " COALESCE(json_extract(?1, '$.accepted_count'), 0),"
                " COALESCE(json_extract(?1, '$.skipped_count'), 0),"
                " COALESCE(json_extract(?1, '$.error_count'), 0),"
                " COALESCE(json_extract(?1, '$.error'), '') "
                "WHERE json_valid(?1);";
            if (sqlite3_prepare_v2(db, psql, -1, &stmt, NULL) == SQLITE_OK) {
                sqlite3_bind_text(stmt, 1, resp.body, -1, SQLITE_STATIC);
                if (sqlite3_step(stmt) == SQLITE_ROW) {
                    api_ok   = sqlite3_column_int(stmt, 0);
                    accepted = sqlite3_column_int(stmt, 1);
                    skipped  = sqlite3_column_int(stmt, 2);
                    err_cnt  = sqlite3_column_int(stmt, 3);
                    const char *em = (const char *)sqlite3_column_text(stmt, 4);
                    if (em) snprintf(srv_err, sizeof(srv_err), "%s", em);
                }
                sqlite3_finalize(stmt);
            }
        }

        if (api_ok == 1) {
            mark_acknowledged_ids_in_db(resp.body, err_cnt == 0 ? batch_max_id : 0);
        }
        db_unlock();
        http_response_free(&resp);

        if (api_ok != 1) {
            *out_err_code = SYNC_STATUS_ERROR;
            snprintf(out_err_msg, err_sz, "Server push error: %s", srv_err[0] ? srv_err : "invalid JSON");
            return false;
        }

        *out_pushed += (accepted + skipped);
        if (err_cnt > 0 || count < SYNC_MAX_BATCH) {
            break;
        }
    }

    return true;
}

static bool sync_pull_remote_batches(const char *base_url,
                                     const char *device_id,
                                     const char *token,
                                     const char *hmac_secret,
                                     int *out_pulled,
                                     SyncStatusCode *out_err_code,
                                     char *out_err_msg,
                                     size_t err_sz)
{
    *out_pulled = 0;

    for (int page = 0; page < 20; page++) {
        char seq_buf[32] = "0";
        db_lock();
        sync_core_get_state("last_server_seq", seq_buf, sizeof(seq_buf));
        db_unlock();
        long long since = strtoll(seq_buf, NULL, 10);
        if (since < 0) since = 0;

        char endpoint[256];
        snprintf(endpoint, sizeof(endpoint),
                 "/api/pull.php?since=%lld&device_id=%s&limit=%d",
                 since, device_id, SYNC_MAX_BATCH);

        HttpResponse resp;
        bool http_ok = winhttp_request_json(base_url, endpoint, "GET",
                                            device_id, token, hmac_secret,
                                            NULL, &resp);
        if (!http_ok) {
            *out_err_code = is_winhttp_network_err(resp.win32_err) ? SYNC_STATUS_OFFLINE : SYNC_STATUS_ERROR;
            snprintf(out_err_msg, err_sz, "Offline or unreachable during pull (WinHTTP %lu).",
                     (unsigned long)resp.win32_err);
            http_response_free(&resp);
            return false;
        }

        if (resp.status_code == 401 || resp.status_code == 403) {
            *out_err_code = SYNC_STATUS_AUTH_ERROR;
            snprintf(out_err_msg, err_sz, "Authentication failed (HTTP %lu): check Device Token / HMAC.",
                     (unsigned long)resp.status_code);
            http_response_free(&resp);
            return false;
        }

        if (resp.status_code == 204) {
            if (resp.header_last_seq > since) {
                char new_seq[32];
                snprintf(new_seq, sizeof(new_seq), "%lld", resp.header_last_seq);
                db_lock();
                sync_core_set_state("last_server_seq", new_seq);
                db_unlock();
            }
            bool more = resp.header_has_more;
            http_response_free(&resp);
            if (!more) break;
            continue;
        }

        if (resp.status_code != 200 || !resp.body) {
            *out_err_code = SYNC_STATUS_ERROR;
            snprintf(out_err_msg, err_sz, "Server pull error (HTTP %lu).", (unsigned long)resp.status_code);
            http_response_free(&resp);
            return false;
        }

        db_lock();
        sqlite3 *db = db_get_handle();
        int api_ok = 0;
        long long next_since = since;
        int has_more = 0;
        int item_count = 0;

        SyncRemoteChange *changes = (SyncRemoteChange *)calloc(SYNC_MAX_BATCH, sizeof(SyncRemoteChange));
        char **owned_payloads = (char **)calloc(SYNC_MAX_BATCH, sizeof(char *));

        if (db && changes && owned_payloads) {
            sqlite3_stmt *meta_stmt = NULL;
            const char *msql =
                "SELECT COALESCE(json_extract(?1, '$.ok'), 0),"
                " COALESCE(json_extract(?1, '$.next_since'), -1),"
                " COALESCE(json_extract(?1, '$.has_more'), 0) "
                "WHERE json_valid(?1);";
            if (sqlite3_prepare_v2(db, msql, -1, &meta_stmt, NULL) == SQLITE_OK) {
                sqlite3_bind_text(meta_stmt, 1, resp.body, -1, SQLITE_STATIC);
                if (sqlite3_step(meta_stmt) == SQLITE_ROW) {
                    api_ok     = sqlite3_column_int(meta_stmt, 0);
                    long long ns = sqlite3_column_int64(meta_stmt, 1);
                    if (ns >= 0) next_since = ns;
                    has_more   = sqlite3_column_int(meta_stmt, 2);
                }
                sqlite3_finalize(meta_stmt);
            }

            if (api_ok == 1) {
                const char *rows_sql =
                    "SELECT"
                    " COALESCE(CAST(json_extract(value, '$.server_seq') AS INTEGER), 0),"
                    " COALESCE(CAST(json_extract(value, '$.table_name') AS TEXT), ''),"
                    " COALESCE(CAST(json_extract(value, '$.record_uuid') AS TEXT), ''),"
                    " COALESCE(CAST(json_extract(value, '$.operation') AS TEXT), 'UPSERT'),"
                    " COALESCE(CAST(json_extract(value, '$.origin_device_id') AS TEXT), ''),"
                    " COALESCE(CAST(json_extract(value, '$.updated_at_ms') AS INTEGER), 0),"
                    " COALESCE(json_extract(value, '$.payload'), '{}') "
                    "FROM json_each(json_extract(?1, '$.changes')) "
                    "ORDER BY key ASC;";
                sqlite3_stmt *rstmt = NULL;
                if (sqlite3_prepare_v2(db, rows_sql, -1, &rstmt, NULL) == SQLITE_OK) {
                    sqlite3_bind_text(rstmt, 1, resp.body, -1, SQLITE_STATIC);
                    while (item_count < SYNC_MAX_BATCH && sqlite3_step(rstmt) == SQLITE_ROW) {
                        SyncRemoteChange *rc = &changes[item_count];
                        rc->server_seq = sqlite3_column_int64(rstmt, 0);

                        const char *tbl  = (const char *)sqlite3_column_text(rstmt, 1);
                        const char *uuid = (const char *)sqlite3_column_text(rstmt, 2);
                        const char *op   = (const char *)sqlite3_column_text(rstmt, 3);
                        const char *dev  = (const char *)sqlite3_column_text(rstmt, 4);
                        rc->updated_at_ms = sqlite3_column_int64(rstmt, 5);
                        const char *pl   = (const char *)sqlite3_column_text(rstmt, 6);

                        snprintf(rc->table_name, sizeof(rc->table_name), "%s", tbl ? tbl : "");
                        snprintf(rc->record_uuid, sizeof(rc->record_uuid), "%s", uuid ? uuid : "");
                        snprintf(rc->operation, sizeof(rc->operation), "%s", op ? op : "UPSERT");
                        snprintf(rc->origin_device_id, sizeof(rc->origin_device_id), "%s", dev ? dev : "");

                        if (!pl) pl = "{}";
                        size_t plen = strlen(pl);
                        owned_payloads[item_count] = (char *)malloc(plen + 1);
                        if (owned_payloads[item_count]) {
                            memcpy(owned_payloads[item_count], pl, plen + 1);
                            rc->payload_json = owned_payloads[item_count];
                        } else {
                            rc->payload_json = "{}";
                        }
                        item_count++;
                    }
                    sqlite3_finalize(rstmt);
                }
            }
        }

        int batch_applied = 0;
        if (api_ok == 1 && item_count > 0) {
            SyncApplyResult apply_res;
            memset(&apply_res, 0, sizeof(apply_res));
            sync_core_apply_remote_batch(changes, item_count, &apply_res);
            batch_applied = apply_res.applied_count;
            if (apply_res.error_count == 0 && next_since > apply_res.last_server_seq) {
                char seq_upd[32];
                snprintf(seq_upd, sizeof(seq_upd), "%lld", next_since);
                sync_core_set_state("last_server_seq", seq_upd);
            }
        } else if (api_ok == 1 && next_since > since) {
            char seq_upd[32];
            snprintf(seq_upd, sizeof(seq_upd), "%lld", next_since);
            sync_core_set_state("last_server_seq", seq_upd);
        }

        db_unlock();

        if (owned_payloads) {
            for (int i = 0; i < item_count; i++) free(owned_payloads[i]);
            free(owned_payloads);
        }
        if (changes) free(changes);
        http_response_free(&resp);

        if (api_ok != 1) {
            *out_err_code = SYNC_STATUS_ERROR;
            snprintf(out_err_msg, err_sz, "Malformed response JSON from pull endpoint.");
            return false;
        }

        *out_pulled += batch_applied;
        if (!has_more) break;
    }

    return true;
}

/* ── Configuration & Status Public API ────────────────────────────── */

bool sync_engine_get_config(char *out_url, size_t url_sz,
                            char *out_token, size_t tok_sz,
                            char *out_hmac, size_t hmac_sz,
                            bool *out_enabled)
{
    if (out_url && url_sz > 0) out_url[0] = '\0';
    if (out_token && tok_sz > 0) out_token[0] = '\0';
    if (out_hmac && hmac_sz > 0) out_hmac[0] = '\0';
    if (out_enabled) *out_enabled = false;

    if (!db_get_handle()) return false;

    char enabled_buf[16] = "0";
    db_lock();
    if (out_url && url_sz > 0) sync_core_get_state("cpanel_url", out_url, url_sz);
    if (out_token && tok_sz > 0) sync_core_get_state("device_token", out_token, tok_sz);
    if (out_hmac && hmac_sz > 0) sync_core_get_state("hmac_secret", out_hmac, hmac_sz);
    sync_core_get_state("sync_enabled", enabled_buf, sizeof(enabled_buf));
    db_unlock();

    if (out_enabled) {
        *out_enabled = (strcmp(enabled_buf, "1") == 0);
    }
    return true;
}

bool sync_engine_save_config(const char *cpanel_url,
                             const char *device_token,
                             const char *hmac_secret,
                             bool enabled)
{
    if (!db_get_handle()) return false;

    char clean_url[256] = {0};
    if (cpanel_url) normalize_base_url(cpanel_url, clean_url, sizeof(clean_url));

    db_lock();
    bool ok = sync_core_set_state("cpanel_url", clean_url) &&
              sync_core_set_state("device_token", device_token ? device_token : "") &&
              sync_core_set_state("hmac_secret", hmac_secret ? hmac_secret : "") &&
              sync_core_set_state("sync_enabled", enabled ? "1" : "0");
    db_unlock();

    if (ok) {
        publish_status(enabled ? SYNC_STATUS_IDLE : SYNC_STATUS_DISABLED, -1, -1,
                       enabled ? "Sync configuration saved." : "Cloud Sync disabled.");
        if (enabled) sync_engine_trigger_now();
    }
    return ok;
}

void sync_engine_get_status(SyncEngineStatus *out_status)
{
    if (!out_status) return;
    g_mutex_lock(&s_state_mutex);
    *out_status = s_cached_status;
    g_mutex_unlock(&s_state_mutex);

    if (out_status->device_id[0] == '\0' && db_get_handle()) {
        char url[256] = {0}, tok[256] = {0}, hmac[256] = {0}, seq_buf[32] = "0";
        bool en = false;
        db_lock();
        sync_engine_get_config(url, sizeof(url), tok, sizeof(tok), hmac, sizeof(hmac), &en);
        sync_core_get_device_id(out_status->device_id, sizeof(out_status->device_id));
        sync_core_get_device_prefix(out_status->device_prefix, sizeof(out_status->device_prefix));
        sync_core_get_state("last_server_seq", seq_buf, sizeof(seq_buf));
        sync_core_get_state("last_sync_at", out_status->last_sync_at, sizeof(out_status->last_sync_at));
        int pending = sync_core_get_pending_outbox_count();
        db_unlock();

        out_status->enabled = en;
        out_status->code = en ? SYNC_STATUS_IDLE : SYNC_STATUS_DISABLED;
        snprintf(out_status->cpanel_url, sizeof(out_status->cpanel_url), "%s", url);
        out_status->last_server_seq = strtoll(seq_buf, NULL, 10);
        out_status->pending_outbox = pending >= 0 ? pending : 0;
        snprintf(out_status->message, sizeof(out_status->message), "%s",
                 en ? "Ready to sync." : "Disabled (configure cPanel URL & Token to enable).");
    }
}

void sync_engine_set_remote_callback(SyncRemoteAppliedCallback cb, gpointer user_data)
{
    g_mutex_lock(&s_state_mutex);
    s_remote_cb = cb;
    s_remote_ud = user_data;
    g_mutex_unlock(&s_state_mutex);
}

void sync_engine_set_status_callback(SyncStatusChangedCallback cb, gpointer user_data)
{
    g_mutex_lock(&s_state_mutex);
    s_status_cb = cb;
    s_status_ud = user_data;
    g_mutex_unlock(&s_state_mutex);
}

/* ── Blocking Probe & Delta Cycle Execution ───────────────────────── */

bool sync_engine_test_connection(const char *cpanel_url,
                                 const char *device_token,
                                 const char *hmac_secret,
                                 char *out_msg,
                                 size_t msg_sz)
{
    if (!cpanel_url || !cpanel_url[0] || !device_token || !device_token[0]) {
        if (out_msg && msg_sz > 0) {
            snprintf(out_msg, msg_sz, "Enter both cPanel API URL and Device Token first.");
        }
        return false;
    }

    char dev_id[SYNC_DEVICE_ID_LEN] = "DEV-desktop";
    if (db_get_handle()) {
        db_lock();
        sync_core_get_device_id(dev_id, sizeof(dev_id));
        db_unlock();
    }

    HttpResponse resp;
    if (!winhttp_request_json(cpanel_url, "/api/health.php", "GET",
                              dev_id, device_token, hmac_secret, NULL, &resp)) {
        if (out_msg && msg_sz > 0) {
            snprintf(out_msg, msg_sz,
                     "Connection failed (WinHTTP %lu). Verify URL, HTTPS certificate, or internet.",
                     (unsigned long)resp.win32_err);
        }
        http_response_free(&resp);
        return false;
    }

    if (resp.status_code != 200 || !resp.body) {
        if (out_msg && msg_sz > 0) {
            snprintf(out_msg, msg_sz, "Server returned HTTP %lu. Verify Device Token / HMAC Secret.",
                     (unsigned long)resp.status_code);
        }
        http_response_free(&resp);
        return false;
    }

    int ok_flag = 0;
    int schema_ver = 0;
    long long latest_seq = 0;
    char driver[32] = {0};

    db_lock();
    sqlite3 *db = db_get_handle();
    if (db) {
        sqlite3_stmt *stmt = NULL;
        const char *sql =
            "SELECT COALESCE(json_extract(?1, '$.ok'), 0),"
            " COALESCE(json_extract(?1, '$.schema_ver'), 0),"
            " COALESCE(json_extract(?1, '$.latest_seq'), 0),"
            " COALESCE(CAST(json_extract(?1, '$.db_driver') AS TEXT), '') "
            "WHERE json_valid(?1);";
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
            sqlite3_bind_text(stmt, 1, resp.body, -1, SQLITE_STATIC);
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                ok_flag    = sqlite3_column_int(stmt, 0);
                schema_ver = sqlite3_column_int(stmt, 1);
                latest_seq = sqlite3_column_int64(stmt, 2);
                const char *drv = (const char *)sqlite3_column_text(stmt, 3);
                if (drv) snprintf(driver, sizeof(driver), "%s", drv);
            }
            sqlite3_finalize(stmt);
        }
    }
    db_unlock();
    http_response_free(&resp);

    if (ok_flag == 1) {
        if (out_msg && msg_sz > 0) {
            snprintf(out_msg, msg_sz,
                     "Connected to cPanel Sync API (%s, Schema V%d, Latest Server Seq #%lld).",
                     driver[0] ? driver : "db", schema_ver, latest_seq);
        }
        return true;
    }

    if (out_msg && msg_sz > 0) {
        snprintf(out_msg, msg_sz, "Health check returned invalid schema response.");
    }
    return false;
}

static bool sync_cycle_internal(bool require_enabled, char *out_msg, size_t msg_sz)
{
    g_mutex_lock(&s_exec_mutex);

    char url[256] = {0}, token[256] = {0}, hmac[256] = {0}, dev_id[SYNC_DEVICE_ID_LEN] = {0};
    bool enabled = false;

    if (!db_get_handle()) {
        g_mutex_unlock(&s_exec_mutex);
        if (out_msg && msg_sz > 0) snprintf(out_msg, msg_sz, "Database not initialized.");
        return false;
    }

    db_lock();
    sync_engine_get_config(url, sizeof(url), token, sizeof(token), hmac, sizeof(hmac), &enabled);
    sync_core_get_device_id(dev_id, sizeof(dev_id));
    db_unlock();

    if ((require_enabled && !enabled) || url[0] == '\0' || token[0] == '\0') {
        g_mutex_unlock(&s_exec_mutex);
        SyncStatusCode st = enabled ? SYNC_STATUS_IDLE : SYNC_STATUS_DISABLED;
        const char *m = enabled ? "Configure cPanel URL & Token to sync." : "Cloud Sync disabled.";
        publish_status(st, -1, -1, m);
        if (out_msg && msg_sz > 0) snprintf(out_msg, msg_sz, "%s", m);
        return false;
    }

    publish_status(SYNC_STATUS_SYNCING, -1, -1, "Synchronizing deltas with cPanel...");

    int pushed = 0;
    int pulled = 0;
    SyncStatusCode err_code = SYNC_STATUS_OK;
    char err_msg[256] = {0};

    bool ok = sync_push_pending_batches(url, dev_id, token, hmac,
                                        &pushed, &err_code, err_msg, sizeof(err_msg));
    if (ok) {
        ok = sync_pull_remote_batches(url, dev_id, token, hmac,
                                      &pulled, &err_code, err_msg, sizeof(err_msg));
    }

    if (ok) {
        char ts[64];
        format_utc_timestamp(ts, sizeof(ts));
        db_lock();
        sync_core_set_state("last_sync_at", ts);
        db_unlock();

        char ok_msg[256];
        snprintf(ok_msg, sizeof(ok_msg),
                 "Synced (%d pushed, %d pulled at %s).", pushed, pulled, ts);
        publish_status(SYNC_STATUS_OK, pushed, pulled, ok_msg);
        if (pulled > 0) {
            notify_remote_applied(pulled);
        }
        if (out_msg && msg_sz > 0) snprintf(out_msg, msg_sz, "%s", ok_msg);
        g_mutex_unlock(&s_exec_mutex);
        return true;
    }

    publish_status(err_code, -1, -1, err_msg);
    if (out_msg && msg_sz > 0) snprintf(out_msg, msg_sz, "%s", err_msg);
    g_mutex_unlock(&s_exec_mutex);
    return false;
}

bool sync_engine_sync_once_blocking(char *out_msg, size_t msg_sz)
{
    return sync_cycle_internal(false, out_msg, msg_sz);
}

/* ── Background Worker Thread ─────────────────────────────────────── */

static gpointer sync_worker_thread_fn(gpointer user_data)
{
    (void)user_data;

    /* Brief startup settle delay before first background check */
    g_mutex_lock(&s_wake_mutex);
    if (!s_stop_requested && !s_force_sync) {
        gint64 init_deadline = g_get_monotonic_time() + (1500 * 1000LL);
        g_cond_wait_until(&s_wake_cond, &s_wake_mutex, init_deadline);
    }

    gint64 next_pull_at = g_get_monotonic_time();

    while (!s_stop_requested) {
        bool do_push_only = s_dirty_push && !s_force_sync;
        bool do_full = s_force_sync || (g_get_monotonic_time() >= next_pull_at);

        if (do_push_only && !do_full) {
            /* Wait ~900ms debounce so multi-row commits coalesce cleanly */
            s_dirty_push = false;
            gint64 debounce_until = g_get_monotonic_time() + (SYNC_PUSH_DEBOUNCE_MS * 1000LL);
            while (!s_stop_requested && !s_force_sync) {
                if (!g_cond_wait_until(&s_wake_cond, &s_wake_mutex, debounce_until)) {
                    break;
                }
            }
            if (s_stop_requested) break;
        }

        s_dirty_push = false;
        s_force_sync = false;
        g_mutex_unlock(&s_wake_mutex);

        sync_cycle_internal(true, NULL, 0);

        g_mutex_lock(&s_wake_mutex);
        if (do_full) {
            next_pull_at = g_get_monotonic_time() + (SYNC_PULL_INTERVAL_SEC * G_USEC_PER_SEC);
        }

        while (!s_stop_requested && !s_dirty_push && !s_force_sync) {
            gint64 now = g_get_monotonic_time();
            if (now >= next_pull_at) break;
            g_cond_wait_until(&s_wake_cond, &s_wake_mutex, next_pull_at);
        }
    }

    g_mutex_unlock(&s_wake_mutex);
    return NULL;
}

void sync_engine_start(void)
{
    g_mutex_lock(&s_wake_mutex);
    if (s_worker_thread != NULL) {
        g_mutex_unlock(&s_wake_mutex);
        return;
    }
    s_stop_requested = false;
    s_dirty_push = false;
    s_force_sync = false;
    g_mutex_unlock(&s_wake_mutex);

    sqlite3 *db = db_get_handle();
    if (db) {
        db_lock();
        sqlite3_update_hook(db, on_sqlite_update_hook, NULL);
        db_unlock();
    }

    s_worker_thread = g_thread_new("schoolcms-sync", sync_worker_thread_fn, NULL);
}

void sync_engine_stop(void)
{
    sqlite3 *db = db_get_handle();
    if (db) {
        db_lock();
        sqlite3_update_hook(db, NULL, NULL);
        db_unlock();
    }

    g_mutex_lock(&s_wake_mutex);
    s_stop_requested = true;
    g_cond_signal(&s_wake_cond);
    g_mutex_unlock(&s_wake_mutex);

    if (s_worker_thread != NULL) {
        g_thread_join(s_worker_thread);
        s_worker_thread = NULL;
    }
}

void sync_engine_trigger_now(void)
{
    g_mutex_lock(&s_wake_mutex);
    s_force_sync = true;
    g_cond_signal(&s_wake_cond);
    g_mutex_unlock(&s_wake_mutex);
}

bool sync_engine_is_running(void)
{
    return (s_worker_thread != NULL);
}
