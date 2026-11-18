/**
 * @file auth.h
 * @brief Authentication, authorization, session management, and cryptographic password hashing.
 */

#ifndef SCHOOLCMS_AUTH_H
#define SCHOOLCMS_AUTH_H

#include <stdbool.h>
#include <stddef.h>

#define MAX_FAILED_ATTEMPTS 5
#define LOCKOUT_DURATION_MINUTES 15
#define PBKDF2_ITERATIONS 10000
#define SALT_BYTE_LEN 16
#define HASH_BYTE_LEN 32

/**
 * @brief System Roles
 */
typedef enum {
    ROLE_ADMIN = 1,
    ROLE_TEACHER = 2,
    ROLE_ACCOUNTANT = 3,
    ROLE_STAFF = 4
} UserRole;

/**
 * @brief Authentication Result Status
 */
typedef enum {
    AUTH_SUCCESS = 0,
    AUTH_ERR_INVALID_CREDENTIALS,
    AUTH_ERR_ACCOUNT_LOCKED,
    AUTH_ERR_INACTIVE,
    AUTH_ERR_DATABASE,
    AUTH_ERR_PARAM
} AuthResult;

/**
 * @brief Authenticated User Session
 */
typedef struct {
    int user_id;
    char username[64];
    char full_name[128];
    int role_id;
    char role_name[32];
    char email[128];
    bool is_authenticated;
} UserSession;

/**
 * @brief Initialize authentication subsystem and ensure default admin credentials.
 * @return AUTH_SUCCESS on success.
 */
AuthResult auth_init(void);

/**
 * @brief Cryptographically hash a password with a given salt using PBKDF2-HMAC-SHA256.
 *
 * @param password Plaintext password.
 * @param salt_hex Hexadecimal representation of salt.
 * @param out_hash_hex Output buffer for hex hash (at least 65 bytes).
 * @param out_size Size of out_hash_hex buffer.
 * @return true on success, false on failure.
 */
bool auth_hash_password(const char *password, const char *salt_hex, char *out_hash_hex, size_t out_size);

/**
 * @brief Generate a cryptographically secure random salt in hex string format.
 *
 * @param out_salt_hex Output buffer (at least 33 bytes for 16-byte salt).
 * @param out_size Size of out_salt_hex buffer.
 * @return true on success, false on failure.
 */
bool auth_generate_salt(char *out_salt_hex, size_t out_size);

/**
 * @brief Attempt to authenticate user credentials.
 *
 * Handles failed attempts count and automatic account lockout.
 *
 * @param username Entered username.
 * @param password Entered plaintext password.
 * @param out_session Optional pointer to receive session info on success.
 * @return AuthResult status code.
 */
AuthResult auth_login(const char *username, const char *password, UserSession *out_session);

/**
 * @brief Log out the current user session and write audit log.
 */
void auth_logout(void);

/**
 * @brief Get the active user session.
 * @return Pointer to internal static session struct.
 */
const UserSession *auth_get_session(void);

/**
 * @brief Check if a user is currently logged in.
 */
bool auth_is_logged_in(void);

/**
 * @brief Check if current user has the specified role.
 */
bool auth_has_role(UserRole role);

/**
 * @brief Create a new user with hashed password and salt.
 *
 * @param username Unique username.
 * @param password Plaintext password to hash.
 * @param role_id System role (1-4).
 * @param full_name User's display name.
 * @param email Unique email address.
 * @param phone Contact phone number.
 * @return AuthResult status.
 */
AuthResult auth_create_user(const char *username, const char *password, int role_id,
                            const char *full_name, const char *email, const char *phone);

#endif /* SCHOOLCMS_AUTH_H */
