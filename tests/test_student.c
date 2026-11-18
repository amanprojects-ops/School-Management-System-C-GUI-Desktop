/**
 * @file test_student.c
 * @brief Unit tests for student CRUD operations, filtering, and pagination.
 */

#include "student.h"
#include "database.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define TEST_DB "test_schoolcms_student.db"

static void cleanup_files(void)
{
    remove(TEST_DB);
    remove(TEST_DB "-wal");
    remove(TEST_DB "-shm");
}

static void test_admission_no_generation(void)
{
    printf("[TEST] Testing admission number generation...\n");

    char adm_no[32];
    assert(student_service_generate_admission_no(adm_no, sizeof(adm_no)));
    assert(strncmp(adm_no, "ADM-", 4) == 0);
    printf("       Generated admission number: %s -> PASS\n", adm_no);
}

static void test_student_crud(void)
{
    printf("[TEST] Testing student create, read, update, delete...\n");

    /* Create sample class */
    sqlite3 *db = db_get_handle();
    assert(db != NULL);
    sqlite3_exec(db, "INSERT INTO classes (id, name, numeric_value) VALUES (1, 'Class 10-A', 10);", NULL, NULL, NULL);
    sqlite3_exec(db, "INSERT INTO sections (id, class_id, name) VALUES (1, 1, 'A');", NULL, NULL, NULL);

    /* 1. Create Student */
    Student s = {0};
    strncpy(s.full_name, "Aditya Roy", sizeof(s.full_name) - 1);
    strncpy(s.father_name, "Deepak Roy", sizeof(s.father_name) - 1);
    strncpy(s.mother_name, "Sunita Roy", sizeof(s.mother_name) - 1);
    strncpy(s.dob, "2010-05-14", sizeof(s.dob) - 1);
    strcpy(s.gender, "MALE");
    strcpy(s.category, "GENERAL");
    strncpy(s.phone, "9876543210", sizeof(s.phone) - 1);
    strncpy(s.email, "aditya@test.local", sizeof(s.email) - 1);
    strncpy(s.address, "74 Park Avenue, City", sizeof(s.address) - 1);
    strncpy(s.admission_date, "2026-04-05", sizeof(s.admission_date) - 1);
    s.class_id = 1;
    s.section_id = 1;
    s.roll_number = 15;
    strcpy(s.blood_group, "O+");
    strcpy(s.status, "ACTIVE");

    int new_id = 0;
    assert(student_service_create(&s, &new_id));
    assert(new_id > 0);
    printf("       Created student with ID %d -> PASS\n", new_id);

    /* 2. Read Student */
    Student fetched = {0};
    assert(student_service_get_by_id(new_id, &fetched));
    assert(strcmp(fetched.full_name, "Aditya Roy") == 0);
    assert(strcmp(fetched.father_name, "Deepak Roy") == 0);
    assert(fetched.roll_number == 15);
    assert(strcmp(fetched.class_name, "Class 10-A") == 0);
    assert(strcmp(fetched.section_name, "A") == 0);
    printf("       Fetched student with joined class and section -> PASS\n");

    /* 3. Read by Admission No */
    Student fetched_adm = {0};
    assert(student_service_get_by_admission_no(fetched.admission_no, &fetched_adm));
    assert(fetched_adm.id == new_id);
    printf("       Fetched student by admission number %s -> PASS\n", fetched.admission_no);

    /* 4. Update Student */
    strncpy(fetched.phone, "9998887776", sizeof(fetched.phone) - 1);
    strncpy(fetched.address, "100 New Civil Lines", sizeof(fetched.address) - 1);
    fetched.roll_number = 18;
    assert(student_service_update(&fetched));

    Student updated = {0};
    assert(student_service_get_by_id(new_id, &updated));
    assert(strcmp(updated.phone, "9998887776") == 0);
    assert(strcmp(updated.address, "100 New Civil Lines") == 0);
    assert(updated.roll_number == 18);
    printf("       Updated student phone, address, and roll number -> PASS\n");

    /* 5. Delete Student */
    assert(student_service_delete(new_id));
    Student deleted = {0};
    assert(!student_service_get_by_id(new_id, &deleted));
    printf("       Deleted student verified -> PASS\n");
}

static void test_student_filtering_and_pagination(void)
{
    printf("[TEST] Testing student filtering, search, and pagination...\n");

    /* Insert 15 students */
    for (int i = 1; i <= 15; i++) {
        Student s = {0};
        snprintf(s.full_name, sizeof(s.full_name), "Student %02d", i);
        s.class_id = 1;
        s.section_id = 1;
        s.roll_number = i;
        strcpy(s.gender, (i % 2 == 0) ? "FEMALE" : "MALE");
        strcpy(s.status, (i > 13) ? "INACTIVE" : "ACTIVE");
        strncpy(s.admission_date, "2026-04-01", sizeof(s.admission_date) - 1);
        assert(student_service_create(&s, NULL));
    }

    /* Test Pagination: Page 1 with size 5 */
    StudentFilter f = {0};
    f.page = 1;
    f.page_size = 5;

    StudentPage p;
    assert(student_service_get_page(&f, &p));
    assert(p.count == 5);
    assert(p.total_records == 15);
    assert(p.total_pages == 3);
    printf("       Pagination: 15 records across 3 pages of 5 items -> PASS\n");

    /* Test Search Filter */
    memset(&f, 0, sizeof(f));
    strcpy(f.search_query, "Student 05");
    f.page = 1;
    f.page_size = 10;
    assert(student_service_get_page(&f, &p));
    assert(p.total_records == 1);
    assert(strcmp(p.items[0].full_name, "Student 05") == 0);
    printf("       Search filter by name 'Student 05' returned exact match -> PASS\n");

    /* Test Gender Filter */
    memset(&f, 0, sizeof(f));
    strcpy(f.gender, "FEMALE");
    assert(student_service_get_page(&f, &p));
    assert(p.total_records == 7); /* 2,4,6,8,10,12,14 */
    printf("       Gender filter returned 7 female students -> PASS\n");

    /* Test Status Filter */
    memset(&f, 0, sizeof(f));
    strcpy(f.status, "INACTIVE");
    assert(student_service_get_page(&f, &p));
    assert(p.total_records == 2); /* 14, 15 */
    printf("       Status filter returned 2 inactive students -> PASS\n");
}

int main(void)
{
    printf("=========================================\n");
    printf("   SchoolCMS Student Module Unit Tests   \n");
    printf("=========================================\n\n");

    cleanup_files();
    assert(db_init(TEST_DB) == DB_SUCCESS);

    test_admission_no_generation();
    test_student_crud();
    test_student_filtering_and_pagination();

    db_close();
    cleanup_files();

    printf("\nAll student unit tests passed successfully!\n");
    return 0;
}
