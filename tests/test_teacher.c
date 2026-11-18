/**
 * @file test_teacher.c
 * @brief Unit tests for teacher and staff CRUD operations, code generation, and search.
 */

#include "teacher.h"
#include "database.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define TEST_DB "test_schoolcms_teacher.db"

static void cleanup_files(void)
{
    remove(TEST_DB);
    remove(TEST_DB "-wal");
    remove(TEST_DB "-shm");
}

static void test_code_generators(void)
{
    printf("[TEST] Testing teacher and staff code generators...\n");

    char code[32];
    assert(teacher_service_generate_code(code, sizeof(code)));
    assert(strncmp(code, "TCH-", 4) == 0);
    printf("       Teacher code generated: %s -> PASS\n", code);

    assert(staff_service_generate_code(code, sizeof(code)));
    assert(strncmp(code, "STF-", 4) == 0);
    printf("       Staff code generated: %s -> PASS\n", code);
}

static void test_teacher_crud(void)
{
    printf("[TEST] Testing teacher CRUD operations...\n");

    Teacher t = {0};
    strncpy(t.full_name, "Dr. Sanjay Roy", sizeof(t.full_name) - 1);
    strncpy(t.phone, "9876500001", sizeof(t.phone) - 1);
    strncpy(t.email, "sanjay.roy@school.local", sizeof(t.email) - 1);
    strncpy(t.address, "12 Faculty Row", sizeof(t.address) - 1);
    strncpy(t.qualification, "Ph.D. in Physics", sizeof(t.qualification) - 1);
    strncpy(t.subject, "Physics", sizeof(t.subject) - 1);
    strncpy(t.joining_date, "2026-01-10", sizeof(t.joining_date) - 1);
    t.salary = 65000.00;
    strcpy(t.status, "ACTIVE");

    /* 1. Create */
    int new_id = 0;
    assert(teacher_service_create(&t, &new_id));
    assert(new_id > 0);
    printf("       Created teacher with ID %d -> PASS\n", new_id);

    /* 2. Read */
    Teacher fetched = {0};
    assert(teacher_service_get_by_id(new_id, &fetched));
    assert(strcmp(fetched.full_name, "Dr. Sanjay Roy") == 0);
    assert(strcmp(fetched.subject, "Physics") == 0);
    assert(fetched.salary >= 64999.0);
    printf("       Fetched teacher by ID -> PASS\n");

    /* 3. Search */
    Teacher list[10];
    int count = 0;
    assert(teacher_service_get_all("Physics", "ACTIVE", list, 10, &count));
    assert(count == 1);
    assert(list[0].id == new_id);
    printf("       Search teacher by subject 'Physics' -> PASS\n");

    /* 4. Update */
    strncpy(fetched.qualification, "Ph.D. (Physics) & M.Ed.", sizeof(fetched.qualification) - 1);
    fetched.salary = 70000.00;
    assert(teacher_service_update(&fetched));

    Teacher updated = {0};
    assert(teacher_service_get_by_id(new_id, &updated));
    assert(updated.salary >= 69999.0);
    assert(strcmp(updated.qualification, "Ph.D. (Physics) & M.Ed.") == 0);
    printf("       Updated teacher qualification and salary -> PASS\n");

    /* 5. Delete */
    assert(teacher_service_delete(new_id));
    Teacher deleted = {0};
    assert(!teacher_service_get_by_id(new_id, &deleted));
    printf("       Deleted teacher verified -> PASS\n");
}

static void test_staff_crud(void)
{
    printf("[TEST] Testing support staff CRUD operations...\n");

    Staff s = {0};
    strncpy(s.full_name, "Mohan Lal", sizeof(s.full_name) - 1);
    strncpy(s.role_title, "Chief Librarian", sizeof(s.role_title) - 1);
    strncpy(s.phone, "9876500002", sizeof(s.phone) - 1);
    strncpy(s.email, "mohan.lal@school.local", sizeof(s.email) - 1);
    strncpy(s.address, "5 Staff Enclave", sizeof(s.address) - 1);
    strncpy(s.joining_date, "2026-02-01", sizeof(s.joining_date) - 1);
    s.salary = 38000.00;
    strcpy(s.status, "ACTIVE");

    /* 1. Create */
    int new_id = 0;
    assert(staff_service_create(&s, &new_id));
    assert(new_id > 0);
    printf("       Created staff member with ID %d -> PASS\n", new_id);

    /* 2. Read */
    Staff fetched = {0};
    assert(staff_service_get_by_id(new_id, &fetched));
    assert(strcmp(fetched.full_name, "Mohan Lal") == 0);
    assert(strcmp(fetched.role_title, "Chief Librarian") == 0);
    printf("       Fetched staff by ID -> PASS\n");

    /* 3. Search */
    Staff list[10];
    int count = 0;
    assert(staff_service_get_all("Librarian", "ACTIVE", list, 10, &count));
    assert(count == 1);
    assert(list[0].id == new_id);
    printf("       Search staff by role title 'Librarian' -> PASS\n");

    /* 4. Update */
    strncpy(fetched.phone, "9990001112", sizeof(fetched.phone) - 1);
    fetched.salary = 40000.00;
    assert(staff_service_update(&fetched));

    Staff updated = {0};
    assert(staff_service_get_by_id(new_id, &updated));
    assert(strcmp(updated.phone, "9990001112") == 0);
    assert(updated.salary >= 39999.0);
    printf("       Updated staff phone and salary -> PASS\n");

    /* 5. Delete */
    assert(staff_service_delete(new_id));
    Staff deleted = {0};
    assert(!staff_service_get_by_id(new_id, &deleted));
    printf("       Deleted staff verified -> PASS\n");
}

int main(void)
{
    printf("=========================================\n");
    printf("   SchoolCMS Teacher Module Unit Tests   \n");
    printf("=========================================\n\n");

    cleanup_files();
    assert(db_init(TEST_DB) == DB_SUCCESS);

    test_code_generators();
    test_teacher_crud();
    test_staff_crud();

    db_close();
    cleanup_files();

    printf("\nAll teacher and staff unit tests passed successfully!\n");
    return 0;
}
