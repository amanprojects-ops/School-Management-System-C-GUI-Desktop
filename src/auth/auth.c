/**
 * @file auth.c
 * @brief Authentication, authorization, session management, and cryptographic password hashing implementation.
 */

#include "auth/auth.h"
#include "database.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <openssl/evp.h>
#include <openssl/rand.h>

static UserSession s_current_session = {0};

/* ── Crypto Utilities ─────────────────────────────────────────────── */

static void bytes_to_hex(const unsigned char *bytes, size_t len, char *out_hex)
{
    for (size_t i = 0; i < len; i++) {
        sprintf(out_hex + (i * 2), "%02x", bytes[i]);
    }
    out_hex[len * 2] = '\0';
}

static bool hex_to_bytes(const char *hex, unsigned char *out_bytes, size_t max_len)
{
    size_t hex_len = strlen(hex);
    if (hex_len % 2 != 0 || (hex_len / 2) > max_len) return false;

    for (size_t i = 0; i < hex_len / 2; i++) {
        unsigned int byte_val = 0;
        if (sscanf(hex + (i * 2), "%02x", &byte_val) != 1) {
            return false;
        }
        out_bytes[i] = (unsigned char)byte_val;
    }
    return true;
}

bool auth_generate_salt(char *out_salt_hex, size_t out_size)
{
    if (out_salt_hex == NULL || out_size < (SALT_BYTE_LEN * 2 + 1)) return false;

    unsigned char salt_bytes[SALT_BYTE_LEN];
    if (RAND_bytes(salt_bytes, sizeof(salt_bytes)) != 1) {
        return false;
    }

    bytes_to_hex(salt_bytes, sizeof(salt_bytes), out_salt_hex);
    return true;
}

bool auth_hash_password(const char *password, const char *salt_hex, char *out_hash_hex, size_t out_size)
{
    if (!password || !salt_hex || !out_hash_hex || out_size < (HASH_BYTE_LEN * 2 + 1)) {
        return false;
    }

    unsigned char salt_bytes[SALT_BYTE_LEN];
    if (!hex_to_bytes(salt_hex, salt_bytes, sizeof(salt_bytes))) {
        return false;
    }

    unsigned char hash_bytes[HASH_BYTE_LEN];
    int ok = PKCS5_PBKDF2_HMAC(password, (int)strlen(password),
                               salt_bytes, sizeof(salt_bytes),
                               PBKDF2_ITERATIONS,
                               EVP_sha256(),
                               sizeof(hash_bytes), hash_bytes);
    if (!ok) {
        return false;
    }

    bytes_to_hex(hash_bytes, sizeof(hash_bytes), out_hash_hex);
    return true;
}

/* ── Auth Lifecycle & Administration ──────────────────────────────── */

AuthResult auth_init(void)
{
    sqlite3 *db = db_get_handle();
    if (!db) return AUTH_ERR_DATABASE;

    /* Ensure default administrator account has a valid PBKDF2 hash (default: admin / admin123) */
    sqlite3_stmt *stmt = NULL;
    const char *check_sql = "SELECT id, salt FROM users WHERE username = 'admin';";
    int rc = sqlite3_prepare_v2(db, check_sql, -1, &stmt, NULL);
    if (rc == SQLITE_OK && sqlite3_step(stmt) == SQLITE_ROW) {
        const unsigned char *salt = sqlite3_column_text(stmt, 1);
        if (salt == NULL || strlen((const char *)salt) != (SALT_BYTE_LEN * 2)) {
            /* Generate proper cryptographic salt and PBKDF2 hash for admin */
            char new_salt[SALT_BYTE_LEN * 2 + 1];
            char new_hash[HASH_BYTE_LEN * 2 + 1];
            auth_generate_salt(new_salt, sizeof(new_salt));
            auth_hash_password("admin123", new_salt, new_hash, sizeof(new_hash));

            sqlite3_finalize(stmt);
            stmt = NULL;

            const char *upd_sql = "UPDATE users SET password_hash = ?, salt = ? WHERE username = 'admin';";
            sqlite3_prepare_v2(db, upd_sql, -1, &stmt, NULL);
            sqlite3_bind_text(stmt, 1, new_hash, -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 2, new_salt, -1, SQLITE_STATIC);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
            return AUTH_SUCCESS;
        }
        sqlite3_finalize(stmt);
        return AUTH_SUCCESS;
    }

    if (stmt) sqlite3_finalize(stmt);

    /* If admin user does not exist, create it */
    return auth_create_user("admin", "admin123", ROLE_ADMIN,
                            "School Administrator", "admin@schoolcms.local", "+91 99999 00000");
}

AuthResult auth_create_user(const char *username, const char *password, int role_id,
                            const char *full_name, const char *email, const char *phone)
{
    if (!username || !password || !full_name) return AUTH_ERR_PARAM;

    sqlite3 *db = db_get_handle();
    if (!db) return AUTH_ERR_DATABASE;

    char salt[SALT_BYTE_LEN * 2 + 1];
    char hash[HASH_BYTE_LEN * 2 + 1];
    if (!auth_generate_salt(salt, sizeof(salt))) {
        return AUTH_ERR_DATABASE;
    }
    if (!auth_hash_password(password, salt, hash, sizeof(hash))) {
        return AUTH_ERR_DATABASE;
    }

    const char *sql =
        "INSERT INTO users (username, password_hash, salt, role_id, full_name, email, phone, status) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, 'ACTIVE');";

    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) return AUTH_ERR_DATABASE;

    sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, hash, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, salt, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 4, role_id);
    sqlite3_bind_text(stmt, 5, full_name, -1, SQLITE_STATIC);
    if (email) sqlite3_bind_text(stmt, 6, email, -1, SQLITE_STATIC);
    else sqlite3_bind_null(stmt, 6);
    if (phone) sqlite3_bind_text(stmt, 7, phone, -1, SQLITE_STATIC);
    else sqlite3_bind_null(stmt, 7);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) return AUTH_ERR_DATABASE;

    db_log_activity(s_current_session.user_id, "CREATE_USER", "AUTH", username);
    return AUTH_SUCCESS;
}

/* ── Login & Session Management ───────────────────────────────────── */

AuthResult auth_login(const char *username, const char *password, UserSession *out_session)
{
    if (!username || !password) return AUTH_ERR_PARAM;

    sqlite3 *db = db_get_handle();
    if (!db) return AUTH_ERR_DATABASE;

    const char *sql =
        "SELECT u.id, u.username, u.password_hash, u.salt, u.role_id, r.name, u.full_name, "
        "       u.email, u.status, u.failed_attempts, "
        "       CASE WHEN u.locked_until > CURRENT_TIMESTAMP THEN 1 ELSE 0 END AS is_locked "
        "FROM users u "
        "JOIN roles r ON u.role_id = r.id "
        "WHERE u.username = ?;";

    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) return AUTH_ERR_DATABASE;

    sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);
    rc = sqlite3_step(stmt);

    if (rc != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        db_log_activity(0, "LOGIN_FAILED_NOUSER", "AUTH", username);
        return AUTH_ERR_INVALID_CREDENTIALS;
    }

    int user_id = sqlite3_column_int(stmt, 0);
    const char *stored_hash = (const char *)sqlite3_column_text(stmt, 2);
    const char *stored_salt = (const char *)sqlite3_column_text(stmt, 3);
    int role_id = sqlite3_column_int(stmt, 4);
    const char *role_name = (const char *)sqlite3_column_text(stmt, 5);
    const char *full_name = (const char *)sqlite3_column_text(stmt, 6);
    const char *email = (const char *)sqlite3_column_text(stmt, 7);
    const char *status = (const char *)sqlite3_column_text(stmt, 8);
    int failed_attempts = sqlite3_column_int(stmt, 9);
    int is_locked = sqlite3_column_int(stmt, 10);

    /* 1. Check account active */
    if (status && strcmp(status, "ACTIVE") != 0) {
        sqlite3_finalize(stmt);
        db_log_activity(user_id, "LOGIN_INACTIVE", "AUTH", username);
        return AUTH_ERR_INACTIVE;
    }

    /* 2. Check account lockout */
    if (is_locked) {
        sqlite3_finalize(stmt);
        db_log_activity(user_id, "LOGIN_LOCKED_ATTEMPT", "AUTH", username);
        return AUTH_ERR_ACCOUNT_LOCKED;
    }

    /* 3. Compute hash and compare */
    char computed_hash[HASH_BYTE_LEN * 2 + 1];
    if (!auth_hash_password(password, stored_salt, computed_hash, sizeof(computed_hash))) {
        sqlite3_finalize(stmt);
        return AUTH_ERR_DATABASE;
    }

    bool password_match = (strcmp(stored_hash, computed_hash) == 0);

    if (!password_match) {
        failed_attempts++;
        sqlite3_finalize(stmt);

        /* Update failed attempts and lock if threshold reached */
        sqlite3_stmt *upd = NULL;
        if (failed_attempts >= MAX_FAILED_ATTEMPTS) {
            const char *lock_sql =
                "UPDATE users SET failed_attempts = ?, "
                "                 locked_until = datetime('now', '+15 minutes'), "
                "                 updated_at = CURRENT_TIMESTAMP "
                "WHERE id = ?;";
            sqlite3_prepare_v2(db, lock_sql, -1, &upd, NULL);
            sqlite3_bind_int(upd, 1, failed_attempts);
            sqlite3_bind_int(upd, 2, user_id);
            sqlite3_step(upd);
            sqlite3_finalize(upd);

            db_log_activity(user_id, "ACCOUNT_LOCKED", "AUTH", "Max login attempts exceeded");
            return AUTH_ERR_ACCOUNT_LOCKED;
        } else {
            const char *fail_sql =
                "UPDATE users SET failed_attempts = ?, updated_at = CURRENT_TIMESTAMP WHERE id = ?;";
            sqlite3_prepare_v2(db, fail_sql, -1, &upd, NULL);
            sqlite3_bind_int(upd, 1, failed_attempts);
            sqlite3_bind_int(upd, 2, user_id);
            sqlite3_step(upd);
            sqlite3_finalize(upd);

            db_log_activity(user_id, "LOGIN_FAILED_BADPWD", "AUTH", username);
            return AUTH_ERR_INVALID_CREDENTIALS;
        }
    }

    /* Successful login: reset failed attempts & lockout */
    sqlite3_stmt *reset_stmt = NULL;
    const char *reset_sql =
        "UPDATE users SET failed_attempts = 0, locked_until = NULL, updated_at = CURRENT_TIMESTAMP WHERE id = ?;";
    sqlite3_prepare_v2(db, reset_sql, -1, &reset_stmt, NULL);
    sqlite3_bind_int(reset_stmt, 1, user_id);
    sqlite3_step(reset_stmt);
    sqlite3_finalize(reset_stmt);

    /* Populate session */
    s_current_session.user_id = user_id;
    strncpy(s_current_session.username, username, sizeof(s_current_session.username) - 1);
    if (full_name) strncpy(s_current_session.full_name, full_name, sizeof(s_current_session.full_name) - 1);
    s_current_session.role_id = role_id;
    if (role_name) strncpy(s_current_session.role_name, role_name, sizeof(s_current_session.role_name) - 1);
    if (email) strncpy(s_current_session.email, email, sizeof(s_current_session.email) - 1);
    s_current_session.is_authenticated = true;

    if (out_session) {
        *out_session = s_current_session;
    }

    sqlite3_finalize(stmt);
    db_log_activity(user_id, "LOGIN_SUCCESS", "AUTH", username);
    return AUTH_SUCCESS;
}

void auth_logout(void)
{
    if (s_current_session.is_authenticated) {
        db_log_activity(s_current_session.user_id, "LOGOUT", "AUTH", s_current_session.username);
    }
    memset(&s_current_session, 0, sizeof(s_current_session));
}

const UserSession *auth_get_session(void)
{
    return &s_current_session;
}

bool auth_is_logged_in(void)
{
    return s_current_session.is_authenticated;
}

bool auth_has_role(UserRole role)
{
    return (s_current_session.is_authenticated && s_current_session.role_id == (int)role);
}
