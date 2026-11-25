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

/* ── Password Reset & Administration Services ─────────────────────── */

AuthResult auth_change_password(int user_id, const char *old_password, const char *new_password)
{
    if (user_id <= 0 || !old_password || !new_password) return AUTH_ERR_PARAM;
    if (strlen(new_password) < 6) return AUTH_ERR_PARAM;

    sqlite3 *db = db_get_handle();
    if (!db) return AUTH_ERR_DATABASE;

    const char *sel_sql = "SELECT username, password_hash, salt FROM users WHERE id = ?;";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sel_sql, -1, &stmt, NULL) != SQLITE_OK) return AUTH_ERR_DATABASE;

    sqlite3_bind_int(stmt, 1, user_id);
    if (sqlite3_step(stmt) != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        return AUTH_ERR_NOT_FOUND;
    }

    char username[64];
    const char *u = (const char *)sqlite3_column_text(stmt, 0);
    strncpy(username, u ? u : "user", sizeof(username) - 1);
    username[sizeof(username) - 1] = '\0';

    const char *stored_hash = (const char *)sqlite3_column_text(stmt, 1);
    const char *stored_salt = (const char *)sqlite3_column_text(stmt, 2);

    char computed_hash[HASH_BYTE_LEN * 2 + 1];
    if (!auth_hash_password(old_password, stored_salt, computed_hash, sizeof(computed_hash))) {
        sqlite3_finalize(stmt);
        return AUTH_ERR_DATABASE;
    }

    if (strcmp(stored_hash, computed_hash) != 0) {
        sqlite3_finalize(stmt);
        db_log_activity(user_id, "PWD_CHANGE_FAILED", "AUTH", "Old password verification failed");
        return AUTH_ERR_INVALID_CREDENTIALS;
    }
    sqlite3_finalize(stmt);

    char new_salt[SALT_BYTE_LEN * 2 + 1];
    char new_hash[HASH_BYTE_LEN * 2 + 1];
    if (!auth_generate_salt(new_salt, sizeof(new_salt)) ||
        !auth_hash_password(new_password, new_salt, new_hash, sizeof(new_hash))) {
        return AUTH_ERR_DATABASE;
    }

    const char *upd_sql =
        "UPDATE users SET password_hash = ?, salt = ?, failed_attempts = 0, "
        "                 locked_until = NULL, updated_at = CURRENT_TIMESTAMP "
        "WHERE id = ?;";
    if (sqlite3_prepare_v2(db, upd_sql, -1, &stmt, NULL) != SQLITE_OK) return AUTH_ERR_DATABASE;

    sqlite3_bind_text(stmt, 1, new_hash, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, new_salt, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 3, user_id);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) return AUTH_ERR_DATABASE;

    db_log_activity(user_id, "PWD_CHANGE_SUCCESS", "AUTH", username);
    return AUTH_SUCCESS;
}

AuthResult auth_admin_reset_password(int target_user_id, const char *new_password)
{
    if (target_user_id <= 0 || !new_password) return AUTH_ERR_PARAM;
    if (strlen(new_password) < 6) return AUTH_ERR_PARAM;

    sqlite3 *db = db_get_handle();
    if (!db) return AUTH_ERR_DATABASE;

    const char *sel_sql = "SELECT username FROM users WHERE id = ?;";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sel_sql, -1, &stmt, NULL) != SQLITE_OK) return AUTH_ERR_DATABASE;

    sqlite3_bind_int(stmt, 1, target_user_id);
    if (sqlite3_step(stmt) != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        return AUTH_ERR_NOT_FOUND;
    }

    char username[64];
    const char *u = (const char *)sqlite3_column_text(stmt, 0);
    strncpy(username, u ? u : "user", sizeof(username) - 1);
    username[sizeof(username) - 1] = '\0';
    sqlite3_finalize(stmt);

    char new_salt[SALT_BYTE_LEN * 2 + 1];
    char new_hash[HASH_BYTE_LEN * 2 + 1];
    if (!auth_generate_salt(new_salt, sizeof(new_salt)) ||
        !auth_hash_password(new_password, new_salt, new_hash, sizeof(new_hash))) {
        return AUTH_ERR_DATABASE;
    }

    const char *upd_sql =
        "UPDATE users SET password_hash = ?, salt = ?, failed_attempts = 0, "
        "                 locked_until = NULL, updated_at = CURRENT_TIMESTAMP "
        "WHERE id = ?;";
    if (sqlite3_prepare_v2(db, upd_sql, -1, &stmt, NULL) != SQLITE_OK) return AUTH_ERR_DATABASE;

    sqlite3_bind_text(stmt, 1, new_hash, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, new_salt, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 3, target_user_id);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) return AUTH_ERR_DATABASE;

    char details[128];
    snprintf(details, sizeof(details), "Admin reset password for user: %s", username);
    db_log_activity(s_current_session.user_id, "ADMIN_RESET_PWD", "AUTH", details);
    return AUTH_SUCCESS;
}

bool auth_get_all_users(ManagedUser *out_users, int max_users, int *out_count)
{
    if (!out_users || max_users <= 0 || !out_count) return false;
    *out_count = 0;

    sqlite3 *db = db_get_handle();
    if (!db) return false;

    const char *sql =
        "SELECT u.id, u.username, u.full_name, u.role_id, r.name, "
        "       COALESCE(u.email, ''), COALESCE(u.phone, ''), u.status, "
        "       u.failed_attempts, u.created_at "
        "FROM users u "
        "JOIN roles r ON u.role_id = r.id "
        "ORDER BY u.id ASC;";

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return false;

    int idx = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW && idx < max_users) {
        out_users[idx].id = sqlite3_column_int(stmt, 0);

        const char *txt = (const char *)sqlite3_column_text(stmt, 1);
        strncpy(out_users[idx].username, txt ? txt : "", sizeof(out_users[idx].username) - 1);

        txt = (const char *)sqlite3_column_text(stmt, 2);
        strncpy(out_users[idx].full_name, txt ? txt : "", sizeof(out_users[idx].full_name) - 1);

        out_users[idx].role_id = sqlite3_column_int(stmt, 3);

        txt = (const char *)sqlite3_column_text(stmt, 4);
        strncpy(out_users[idx].role_name, txt ? txt : "", sizeof(out_users[idx].role_name) - 1);

        txt = (const char *)sqlite3_column_text(stmt, 5);
        strncpy(out_users[idx].email, txt ? txt : "", sizeof(out_users[idx].email) - 1);

        txt = (const char *)sqlite3_column_text(stmt, 6);
        strncpy(out_users[idx].phone, txt ? txt : "", sizeof(out_users[idx].phone) - 1);

        txt = (const char *)sqlite3_column_text(stmt, 7);
        strncpy(out_users[idx].status, txt ? txt : "ACTIVE", sizeof(out_users[idx].status) - 1);

        out_users[idx].failed_attempts = sqlite3_column_int(stmt, 8);

        txt = (const char *)sqlite3_column_text(stmt, 9);
        strncpy(out_users[idx].created_at, txt ? txt : "", sizeof(out_users[idx].created_at) - 1);

        idx++;
    }
    sqlite3_finalize(stmt);
    *out_count = idx;
    return true;
}

AuthResult auth_update_user(int user_id, const char *full_name, const char *email,
                            const char *phone, int role_id, const char *status)
{
    if (user_id <= 0 || !full_name || role_id <= 0 || !status) return AUTH_ERR_PARAM;

    sqlite3 *db = db_get_handle();
    if (!db) return AUTH_ERR_DATABASE;

    /* Verify if user is last active admin and attempting to demote or deactivate */
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, "SELECT role_id, status FROM users WHERE id = ?;", -1, &stmt, NULL) != SQLITE_OK) {
        return AUTH_ERR_DATABASE;
    }
    sqlite3_bind_int(stmt, 1, user_id);
    if (sqlite3_step(stmt) != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        return AUTH_ERR_NOT_FOUND;
    }

    int curr_role = sqlite3_column_int(stmt, 0);
    const char *curr_status = (const char *)sqlite3_column_text(stmt, 1);
    bool was_active_admin = (curr_role == ROLE_ADMIN && curr_status && strcmp(curr_status, "ACTIVE") == 0);
    sqlite3_finalize(stmt);

    if (was_active_admin && (role_id != ROLE_ADMIN || strcmp(status, "ACTIVE") != 0)) {
        /* Check if other active admins exist */
        int admin_count = 0;
        if (sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM users WHERE role_id = 1 AND status = 'ACTIVE';", -1, &stmt, NULL) == SQLITE_OK) {
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                admin_count = sqlite3_column_int(stmt, 0);
            }
            sqlite3_finalize(stmt);
        }
        if (admin_count <= 1) {
            return AUTH_ERR_LAST_ADMIN;
        }
    }

    const char *sql =
        "UPDATE users SET full_name = ?, email = ?, phone = ?, role_id = ?, status = ?, updated_at = CURRENT_TIMESTAMP "
        "WHERE id = ?;";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return AUTH_ERR_DATABASE;

    sqlite3_bind_text(stmt, 1, full_name, -1, SQLITE_STATIC);
    if (email && strlen(email) > 0) sqlite3_bind_text(stmt, 2, email, -1, SQLITE_STATIC);
    else sqlite3_bind_null(stmt, 2);
    if (phone && strlen(phone) > 0) sqlite3_bind_text(stmt, 3, phone, -1, SQLITE_STATIC);
    else sqlite3_bind_null(stmt, 3);
    sqlite3_bind_int(stmt, 4, role_id);
    sqlite3_bind_text(stmt, 5, status, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 6, user_id);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) return AUTH_ERR_DATABASE;

    char details[128];
    snprintf(details, sizeof(details), "Updated user ID %d profile and status: %s", user_id, status);
    db_log_activity(s_current_session.user_id, "UPDATE_USER", "AUTH", details);
    return AUTH_SUCCESS;
}

AuthResult auth_toggle_user_status(int user_id)
{
    if (user_id <= 0) return AUTH_ERR_PARAM;
    if (s_current_session.is_authenticated && s_current_session.user_id == user_id) {
        return AUTH_ERR_PARAM; /* Cannot deactivate logged-in user */
    }

    sqlite3 *db = db_get_handle();
    if (!db) return AUTH_ERR_DATABASE;

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, "SELECT status, role_id, username FROM users WHERE id = ?;", -1, &stmt, NULL) != SQLITE_OK) {
        return AUTH_ERR_DATABASE;
    }
    sqlite3_bind_int(stmt, 1, user_id);
    if (sqlite3_step(stmt) != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        return AUTH_ERR_NOT_FOUND;
    }

    const char *curr_status = (const char *)sqlite3_column_text(stmt, 0);
    int role_id = sqlite3_column_int(stmt, 1);
    char uname[64];
    const char *u = (const char *)sqlite3_column_text(stmt, 2);
    strncpy(uname, u ? u : "user", sizeof(uname) - 1);
    uname[sizeof(uname) - 1] = '\0';
    bool is_active = (curr_status && strcmp(curr_status, "ACTIVE") == 0);
    sqlite3_finalize(stmt);

    if (is_active && role_id == ROLE_ADMIN) {
        int admin_count = 0;
        if (sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM users WHERE role_id = 1 AND status = 'ACTIVE';", -1, &stmt, NULL) == SQLITE_OK) {
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                admin_count = sqlite3_column_int(stmt, 0);
            }
            sqlite3_finalize(stmt);
        }
        if (admin_count <= 1) {
            return AUTH_ERR_LAST_ADMIN;
        }
    }

    const char *new_status = is_active ? "INACTIVE" : "ACTIVE";
    const char *upd = "UPDATE users SET status = ?, updated_at = CURRENT_TIMESTAMP WHERE id = ?;";
    if (sqlite3_prepare_v2(db, upd, -1, &stmt, NULL) != SQLITE_OK) return AUTH_ERR_DATABASE;

    sqlite3_bind_text(stmt, 1, new_status, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 2, user_id);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) return AUTH_ERR_DATABASE;

    char details[128];
    snprintf(details, sizeof(details), "User %s status changed to %s", uname, new_status);
    db_log_activity(s_current_session.user_id, "TOGGLE_USER_STATUS", "AUTH", details);
    return AUTH_SUCCESS;
}

bool auth_get_role_permissions(int role_id, RolePermission *out_perms, int max_perms, int *out_count)
{
    if (role_id <= 0 || !out_perms || max_perms <= 0 || !out_count) return false;
    *out_count = 0;

    sqlite3 *db = db_get_handle();
    if (!db) return false;

    const char *sql =
        "SELECT id, role_id, module_name, can_view, can_edit "
        "FROM role_permissions "
        "WHERE role_id = ? "
        "ORDER BY id ASC;";

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return false;

    sqlite3_bind_int(stmt, 1, role_id);

    int idx = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW && idx < max_perms) {
        out_perms[idx].id = sqlite3_column_int(stmt, 0);
        out_perms[idx].role_id = sqlite3_column_int(stmt, 1);

        const char *txt = (const char *)sqlite3_column_text(stmt, 2);
        strncpy(out_perms[idx].module_name, txt ? txt : "", sizeof(out_perms[idx].module_name) - 1);

        out_perms[idx].can_view = (sqlite3_column_int(stmt, 3) != 0);
        out_perms[idx].can_edit = (sqlite3_column_int(stmt, 4) != 0);

        idx++;
    }
    sqlite3_finalize(stmt);
    *out_count = idx;
    return true;
}

bool auth_set_role_permission(int role_id, const char *module_name, bool can_view, bool can_edit)
{
    if (role_id <= 0 || !module_name || strlen(module_name) == 0) return false;

    /* Safety rule: Administrator role always retains full view and edit privileges */
    if (role_id == ROLE_ADMIN) {
        can_view = true;
        can_edit = true;
    }

    sqlite3 *db = db_get_handle();
    if (!db) return false;

    const char *sql =
        "INSERT INTO role_permissions (role_id, module_name, can_view, can_edit) "
        "VALUES (?, ?, ?, ?) "
        "ON CONFLICT(role_id, module_name) DO UPDATE SET "
        "    can_view = excluded.can_view, "
        "    can_edit = excluded.can_edit;";

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return false;

    sqlite3_bind_int(stmt, 1, role_id);
    sqlite3_bind_text(stmt, 2, module_name, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 3, can_view ? 1 : 0);
    sqlite3_bind_int(stmt, 4, can_edit ? 1 : 0);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) return false;

    char details[128];
    snprintf(details, sizeof(details), "Updated permission role %d module %s (view:%d edit:%d)",
             role_id, module_name, can_view ? 1 : 0, can_edit ? 1 : 0);
    db_log_activity(s_current_session.user_id, "UPDATE_ROLE_PERM", "AUTH", details);
    return true;
}

bool auth_user_can_access(int user_id, const char *module_name, bool require_edit)
{
    if (user_id <= 0 || !module_name) return false;

    sqlite3 *db = db_get_handle();
    if (!db) return false;

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, "SELECT role_id FROM users WHERE id = ?;", -1, &stmt, NULL) != SQLITE_OK) {
        return false;
    }
    sqlite3_bind_int(stmt, 1, user_id);
    if (sqlite3_step(stmt) != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        return false;
    }
    int role_id = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);

    /* Admin role bypass: full access to everything */
    if (role_id == ROLE_ADMIN) {
        return true;
    }

    const char *sql = "SELECT can_view, can_edit FROM role_permissions WHERE role_id = ? AND module_name = ?;";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return false;

    sqlite3_bind_int(stmt, 1, role_id);
    sqlite3_bind_text(stmt, 2, module_name, -1, SQLITE_STATIC);

    bool allowed = false;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        bool cv = (sqlite3_column_int(stmt, 0) != 0);
        bool ce = (sqlite3_column_int(stmt, 1) != 0);
        allowed = require_edit ? ce : cv;
    }
    sqlite3_finalize(stmt);
    return allowed;
}

bool auth_current_user_can_access(const char *module_name, bool require_edit)
{
    if (!s_current_session.is_authenticated || s_current_session.user_id <= 0) {
        return false;
    }
    return auth_user_can_access(s_current_session.user_id, module_name, require_edit);
}
