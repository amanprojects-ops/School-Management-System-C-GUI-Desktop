/**
 * @file test_auth.c
 * @brief Unit tests for authentication, PBKDF2 hashing, session handling, and lockout protection.
 */

#include "auth/auth.h"
#include "database.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define TEST_DB "test_schoolcms_auth.db"

static void cleanup_files(void)
{
    remove(TEST_DB);
    remove(TEST_DB "-wal");
    remove(TEST_DB "-shm");
}

static void test_salt_and_hash(void)
{
    printf("[TEST] Testing cryptographic salt generation and PBKDF2 hashing...\n");

    char salt1[SALT_BYTE_LEN * 2 + 1];
    char salt2[SALT_BYTE_LEN * 2 + 1];
    assert(auth_generate_salt(salt1, sizeof(salt1)));
    assert(auth_generate_salt(salt2, sizeof(salt2)));
    assert(strcmp(salt1, salt2) != 0);
    printf("       Random salts generated uniquely -> PASS\n");

    char hash1[HASH_BYTE_LEN * 2 + 1];
    char hash2[HASH_BYTE_LEN * 2 + 1];
    assert(auth_hash_password("SuperSecret123", salt1, hash1, sizeof(hash1)));
    assert(auth_hash_password("SuperSecret123", salt1, hash2, sizeof(hash2)));
    assert(strcmp(hash1, hash2) == 0);
    printf("       Deterministic PBKDF2 hash verification -> PASS\n");

    /* Different passwords produce different hashes */
    char hash3[HASH_BYTE_LEN * 2 + 1];
    assert(auth_hash_password("DifferentPassword", salt1, hash3, sizeof(hash3)));
    assert(strcmp(hash1, hash3) != 0);
    printf("       Different passwords produce distinct hashes -> PASS\n");
}

static void test_admin_login(void)
{
    printf("[TEST] Testing default administrator login & session...\n");

    assert(auth_init() == AUTH_SUCCESS);

    UserSession session;
    AuthResult res = auth_login("admin", "admin123", &session);
    assert(res == AUTH_SUCCESS);
    assert(session.is_authenticated == true);
    assert(session.role_id == ROLE_ADMIN);
    assert(strcmp(session.username, "admin") == 0);
    printf("       Admin logged in successfully with default credentials -> PASS\n");

    assert(auth_is_logged_in() == true);
    assert(auth_has_role(ROLE_ADMIN) == true);
    assert(auth_has_role(ROLE_TEACHER) == false);

    auth_logout();
    assert(auth_is_logged_in() == false);
    printf("       Logout cleared session state -> PASS\n");
}

static void test_failed_attempts_and_lockout(void)
{
    printf("[TEST] Testing failed login attempts and lockout threshold...\n");

    /* Attempt 1 to 4 should return invalid credentials */
    for (int i = 1; i < MAX_FAILED_ATTEMPTS; i++) {
        AuthResult res = auth_login("admin", "wrong_password", NULL);
        assert(res == AUTH_ERR_INVALID_CREDENTIALS);
    }
    printf("       Failed attempts 1..4 tracked properly -> PASS\n");

    /* 5th attempt triggers account lockout */
    AuthResult res5 = auth_login("admin", "wrong_password", NULL);
    assert(res5 == AUTH_ERR_ACCOUNT_LOCKED);
    printf("       Account locked on 5th consecutive failure -> PASS\n");

    /* Subsequent attempt even with correct password is locked */
    AuthResult res_locked = auth_login("admin", "admin123", NULL);
    assert(res_locked == AUTH_ERR_ACCOUNT_LOCKED);
    printf("       Account lockout enforced even with correct password -> PASS\n");
}

static void test_create_user_and_login(void)
{
    printf("[TEST] Testing new user creation and role authorization...\n");

    AuthResult res = auth_create_user("teacher_ramesh", "TeacherPass@2026", ROLE_TEACHER,
                                      "Ramesh Sharma", "ramesh@school.local", "+91 98765 11111");
    assert(res == AUTH_SUCCESS);

    UserSession t_sess;
    res = auth_login("teacher_ramesh", "TeacherPass@2026", &t_sess);
    assert(res == AUTH_SUCCESS);
    assert(t_sess.role_id == ROLE_TEACHER);
    assert(auth_has_role(ROLE_TEACHER) == true);
    assert(auth_has_role(ROLE_ADMIN) == false);
    printf("       New teacher created, authenticated, and role verified -> PASS\n");

    auth_logout();
}

int main(void)
{
    printf("=========================================\n");
    printf("    SchoolCMS Auth Module Unit Tests     \n");
    printf("=========================================\n\n");

    cleanup_files();
    DbResult db_res = db_init(TEST_DB);
    assert(db_res == DB_SUCCESS);

    test_salt_and_hash();
    test_admin_login();
    test_failed_attempts_and_lockout();
    test_create_user_and_login();

    db_close();
    cleanup_files();

    printf("\nAll authentication unit tests passed successfully!\n");
    return 0;
}
