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
    AUTH_ERR_PARAM,
    AUTH_ERR_NOT_FOUND,
    AUTH_ERR_LAST_ADMIN
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
 * @brief Managed User Information for Administrative Directory
 */
typedef struct {
    int id;
    char username[64];
    char full_name[128];
    int role_id;
    char role_name[32];
    char email[128];
    char phone[32];
    char status[16];          /* "ACTIVE", "INACTIVE" */
    int failed_attempts;
    char created_at[32];
} ManagedUser;

/**
 * @brief Role Permission Entry
 */
typedef struct {
    int id;
    int role_id;
    char module_name[32];
    bool can_view;
    bool can_edit;
} RolePermission;

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

/**
 * @brief Change user's own password with old password verification.
 */
AuthResult auth_change_password(int user_id, const char *old_password, const char *new_password);

/**
 * @brief Administrative direct password reset for any user.
 */
AuthResult auth_admin_reset_password(int target_user_id, const char *new_password);

/**
 * @brief Retrieve all registered system users.
 */
bool auth_get_all_users(ManagedUser *out_users, int max_users, int *out_count);

/**
 * @brief Update user profile, role, or status.
 */
AuthResult auth_update_user(int user_id, const char *full_name, const char *email,
                            const char *phone, int role_id, const char *status);

/**
 * @brief Toggle user active/inactive status safely.
 */
AuthResult auth_toggle_user_status(int user_id);

/**
 * @brief Retrieve role permissions for a specified role ID.
 */
bool auth_get_role_permissions(int role_id, RolePermission *out_perms, int max_perms, int *out_count);

/**
 * @brief Update role permission for a given module.
 */
bool auth_set_role_permission(int role_id, const char *module_name, bool can_view, bool can_edit);

/**
 * @brief Check if a user has view/edit permission for a module.
 */
bool auth_user_can_access(int user_id, const char *module_name, bool require_edit);

/**
 * @brief Check if the currently authenticated session user has view/edit permission for a module.
 */
bool auth_current_user_can_access(const char *module_name, bool require_edit);

#endif /* SCHOOLCMS_AUTH_H */
