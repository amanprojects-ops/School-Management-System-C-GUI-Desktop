/**
 * @file test_classes.c
 * @brief Unit tests for Class, Section, Subject, and Academic Year management.
 */

#include "classes.h"
#include "database.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_academic_years(void)
{
    printf("Testing Academic Year Services...\n");

    AcademicYear cur;
    bool ok = academic_year_get_current(&cur);
    assert(ok && "Current academic year must be retrievable from seed data");
    assert(strcmp(cur.name, "2026-2027") == 0);
    assert(cur.is_current == true);

    AcademicYear new_year = {
        .id = 0,
        .name = "2027-2028",
        .start_date = "2027-04-01",
        .end_date = "2028-03-31",
        .is_current = false
    };
    int ay_id = 0;
    ok = academic_year_create(&new_year, &ay_id);
    assert(ok && ay_id > 0 && "Must create new academic year");

    ok = academic_year_set_current(ay_id);
    assert(ok && "Must set new academic year as current");

    AcademicYear updated_cur;
    ok = academic_year_get_current(&updated_cur);
    assert(ok && updated_cur.id == ay_id && strcmp(updated_cur.name, "2027-2028") == 0);

    /* Reset back to 1 for remainder of tests */
    academic_year_set_current(1);

    AcademicYear list[MAX_ACADEMIC_YEARS];
    int count = 0;
    ok = academic_year_get_all(list, MAX_ACADEMIC_YEARS, &count);
    assert(ok && count >= 2 && "Must have at least 2 academic years");

    printf(" Academic Year tests passed.\n");
}

static void test_classes(void)
{
    printf("Testing Class Services...\n");

    /* Create sample classes */
    SchoolClass c1 = { .name = "Class 9", .numeric_value = 9, .capacity = 40, .academic_year_id = 1 };
    SchoolClass c2 = { .name = "Class 10", .numeric_value = 10, .capacity = 40, .academic_year_id = 1 };
    SchoolClass c3 = { .name = "Class 11", .numeric_value = 11, .capacity = 40, .academic_year_id = 1 };

    int id1 = 0, id2 = 0, id3 = 0;
    assert(class_service_create(&c1, &id1) && id1 > 0);
    assert(class_service_create(&c2, &id2) && id2 > 0);
    assert(class_service_create(&c3, &id3) && id3 > 0);

    SchoolClass list[MAX_CLASSES];
    int count = 0;
    bool ok = class_service_get_all(list, MAX_CLASSES, &count);
    assert(ok && count >= 3 && "Classes must exist");

    /* Create new class */
    SchoolClass cls = {
        .id = 0,
        .name = "Class 12 - Science",
        .numeric_value = 12,
        .capacity = 45,
        .academic_year_id = 1
    };
    int new_id = 0;
    ok = class_service_create(&cls, &new_id);
    assert(ok && new_id > 0 && "Must create class");

    /* Read by ID */
    SchoolClass fetched;
    ok = class_service_get_by_id(new_id, &fetched);
    assert(ok && fetched.id == new_id);
    assert(strcmp(fetched.name, "Class 12 - Science") == 0);
    assert(fetched.numeric_value == 12);
    assert(fetched.capacity == 45);

    /* Update class */
    strncpy(fetched.name, "Class 12 - Advanced", sizeof(fetched.name) - 1);
    fetched.capacity = 50;
    ok = class_service_update(&fetched);
    assert(ok && "Must update class");

    SchoolClass updated;
    class_service_get_by_id(new_id, &updated);
    assert(strcmp(updated.name, "Class 12 - Advanced") == 0);
    assert(updated.capacity == 50);

    /* Delete class */
    ok = class_service_delete(new_id);
    assert(ok && "Must delete class");
    assert(!class_service_get_by_id(new_id, &fetched));

    printf(" Class tests passed.\n");
}

static void test_sections(void)
{
    printf("Testing Section Services...\n");

    /* Create a section for Class ID 1 (Class 10) */
    ClassSection sec = {
        .id = 0,
        .class_id = 1,
        .name = "Z",
        .class_teacher_id = 1
    };
    int sec_id = 0;
    bool ok = section_service_create(&sec, &sec_id);
    assert(ok && sec_id > 0 && "Must create section");

    /* Query by class */
    ClassSection sec_list[MAX_SECTIONS];
    int sec_count = 0;
    ok = section_service_get_by_class(1, sec_list, MAX_SECTIONS, &sec_count);
    assert(ok && sec_count >= 1);

    bool found = false;
    for (int i = 0; i < sec_count; i++) {
        if (sec_list[i].id == sec_id) {
            assert(strcmp(sec_list[i].name, "Z") == 0);
            assert(sec_list[i].class_teacher_id == 1);
            found = true;
            break;
        }
    }
    assert(found && "Created section must be found in class section list");

    /* Update section */
    sec.id = sec_id;
    strncpy(sec.name, "Omega", sizeof(sec.name) - 1);
    sec.class_teacher_id = 0; /* unassign */
    ok = section_service_update(&sec);
    assert(ok && "Must update section");

    /* Delete section */
    ok = section_service_delete(sec_id);
    assert(ok && "Must delete section");

    printf(" Section tests passed.\n");
}

static void test_subjects(void)
{
    printf("Testing Subject Services...\n");

    Subject sub = {
        .id = 0,
        .code = "TEST-999",
        .name = "Advanced Robotics",
        .class_id = 1,
        .teacher_id = 1
    };
    int sub_id = 0;
    bool ok = subject_service_create(&sub, &sub_id);
    assert(ok && sub_id > 0 && "Must create subject");

    /* Query by class */
    Subject sub_list[MAX_SUBJECTS];
    int sub_count = 0;
    ok = subject_service_get_by_class(1, sub_list, MAX_SUBJECTS, &sub_count);
    assert(ok && sub_count >= 1);

    bool found = false;
    for (int i = 0; i < sub_count; i++) {
        if (sub_list[i].id == sub_id) {
            assert(strcmp(sub_list[i].code, "TEST-999") == 0);
            assert(strcmp(sub_list[i].name, "Advanced Robotics") == 0);
            assert(sub_list[i].teacher_id == 1);
            found = true;
            break;
        }
    }
    assert(found && "Created subject must be found in subject list");

    /* Update subject */
    sub.id = sub_id;
    strncpy(sub.name, "Intro to Robotics", sizeof(sub.name) - 1);
    ok = subject_service_update(&sub);
    assert(ok && "Must update subject");

    /* Delete subject */
    ok = subject_service_delete(sub_id);
    assert(ok && "Must delete subject");

    printf(" Subject tests passed.\n");
}

#define TEST_DB "test_schoolcms_classes.db"

static void cleanup_files(void)
{
    remove(TEST_DB);
    remove(TEST_DB "-wal");
    remove(TEST_DB "-shm");
}

int main(void)
{
    printf("=========================================\n");
    printf(" SchoolCMS Class & Section Tests\n");
    printf("=========================================\n");

    cleanup_files();
    assert(db_init(TEST_DB) == DB_SUCCESS && "Database initialization failed");

    test_academic_years();
    test_classes();

    /* Insert sample teacher for class teacher & subject allocation tests */
    sqlite3 *db = db_get_handle();
    assert(db != NULL);
    sqlite3_exec(db, "INSERT INTO teachers (id, teacher_code, full_name, phone, joining_date) "
                     "VALUES (1, 'TCH-0001', 'Dr. Alan Turing', '9876543210', '2026-01-01');",
                 NULL, NULL, NULL);

    test_sections();
    test_subjects();

    db_close();
    cleanup_files();

    printf("\nAll Class & Section management tests passed successfully! 100%% OK.\n");
    return 0;
}
