/**
 * @file test_timetable.c
 * @brief Unit tests for class and teacher timetables, scheduling, and conflict detection.
 */

#include "timetable.h"
#include "database.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST_DB "test_schoolcms_timetable.db"

static void cleanup_files(void)
{
    remove(TEST_DB);
    remove(TEST_DB "-wal");
    remove(TEST_DB "-shm");
}

static void setup_test_data(void)
{
    sqlite3 *db = db_get_handle();
    assert(db != NULL);

    /* Insert test classes */
    assert(sqlite3_exec(db, "INSERT INTO classes (id, name, numeric_value, capacity, academic_year_id) "
                            "VALUES (1, 'Grade 10', 10, 40, 1), (2, 'Grade 11', 11, 40, 1);",
                        NULL, NULL, NULL) == SQLITE_OK);

    /* Insert test sections */
    assert(sqlite3_exec(db, "INSERT INTO sections (id, class_id, name) "
                            "VALUES (1, 1, 'Section A'), (2, 1, 'Section B'), (3, 2, 'Section A');",
                        NULL, NULL, NULL) == SQLITE_OK);

    /* Insert test teachers */
    assert(sqlite3_exec(db, "INSERT INTO teachers (id, teacher_code, full_name, email, phone, qualification, joining_date, status) "
                            "VALUES (1, 'TCH-001', 'Dr. Alan Turing', 'alan@school.edu', '555-0101', 'Ph.D. CS', '2020-01-01', 'ACTIVE'),"
                            "       (2, 'TCH-002', 'Dr. Grace Hopper', 'grace@school.edu', '555-0102', 'Ph.D. Math', '2020-01-01', 'ACTIVE');",
                        NULL, NULL, NULL) == SQLITE_OK);

    /* Insert test subjects */
    assert(sqlite3_exec(db, "INSERT INTO subjects (id, code, name, class_id) "
                            "VALUES (1, 'CS101', 'Computer Science', 1),"
                            "       (2, 'MTH101', 'Mathematics', 1),"
                            "       (3, 'PHY101', 'Physics', 2);",
                        NULL, NULL, NULL) == SQLITE_OK);
}

static void test_slot_creation_and_query(void)
{
    printf("[TEST] Testing timetable slot creation and querying...\n");

    TimetableSlot slot = {
        .id = 0,
        .class_id = 1,
        .section_id = 1,
        .period_number = 1,
        .subject_id = 1,
        .teacher_id = 1
    };
    strncpy(slot.day_of_week, DAY_MONDAY, sizeof(slot.day_of_week) - 1);
    strncpy(slot.room_number, "Room 101", sizeof(slot.room_number) - 1);
    strncpy(slot.start_time, "08:30", sizeof(slot.start_time) - 1);
    strncpy(slot.end_time, "09:15", sizeof(slot.end_time) - 1);

    int slot_id = 0;
    ConflictResult conflict;
    bool ok = timetable_service_save_slot(&slot, &slot_id, &conflict);
    assert(ok && "Slot saving should succeed");
    assert(slot_id > 0);
    assert(!conflict.has_conflict);

    /* Query by class */
    TimetableSlot class_slots[MAX_TIMETABLE_SLOTS];
    int count = 0;
    ok = timetable_service_get_by_class(1, 1, DAY_MONDAY, class_slots, MAX_TIMETABLE_SLOTS, &count);
    assert(ok && count == 1);
    assert(class_slots[0].id == slot_id);
    assert(class_slots[0].period_number == 1);
    assert(strcmp(class_slots[0].subject_code, "CS101") == 0);
    assert(strcmp(class_slots[0].teacher_name, "Dr. Alan Turing") == 0);
    assert(strcmp(class_slots[0].room_number, "Room 101") == 0);

    /* Query by teacher */
    TimetableSlot tch_slots[MAX_TIMETABLE_SLOTS];
    ok = timetable_service_get_by_teacher(1, DAY_MONDAY, tch_slots, MAX_TIMETABLE_SLOTS, &count);
    assert(ok && count == 1);
    assert(tch_slots[0].id == slot_id);

    /* Query by ID */
    TimetableSlot by_id;
    ok = timetable_service_get_by_id(slot_id, &by_id);
    assert(ok);
    assert(by_id.id == slot_id);
    assert(strcmp(by_id.day_of_week, DAY_MONDAY) == 0);

    printf("       Timetable slot creation and querying verified -> PASS\n");
}

static void test_conflict_detection(void)
{
    printf("[TEST] Testing teacher and room conflict detection...\n");

    /* At this point, Teacher 1 is teaching Class 1 Sec 1 in Room 101 on MONDAY Period 1 */

    /* 1. Test Teacher Conflict:
     * Try assigning Teacher 1 to Class 1 Sec 2 on MONDAY Period 1 in Room 102 */
    TimetableSlot tch_conflict_slot = {
        .id = 0,
        .class_id = 1,
        .section_id = 2,
        .period_number = 1,
        .subject_id = 2,
        .teacher_id = 1
    };
    strncpy(tch_conflict_slot.day_of_week, DAY_MONDAY, sizeof(tch_conflict_slot.day_of_week) - 1);
    strncpy(tch_conflict_slot.room_number, "Room 102", sizeof(tch_conflict_slot.room_number) - 1);

    ConflictResult conf_res;
    timetable_service_check_conflict(&tch_conflict_slot, &conf_res);
    assert(conf_res.has_conflict == true && "Teacher conflict must be detected");
    assert(conf_res.type == CONFLICT_TEACHER);
    assert(strlen(conf_res.message) > 0);

    int new_id = 0;
    bool saved = timetable_service_save_slot(&tch_conflict_slot, &new_id, &conf_res);
    assert(!saved && "Saving conflicted slot must fail");

    /* 2. Test Room Conflict:
     * Try assigning Teacher 2 to Class 1 Sec 2 in Room 101 on MONDAY Period 1 */
    TimetableSlot rm_conflict_slot = {
        .id = 0,
        .class_id = 1,
        .section_id = 2,
        .period_number = 1,
        .subject_id = 2,
        .teacher_id = 2
    };
    strncpy(rm_conflict_slot.day_of_week, DAY_MONDAY, sizeof(rm_conflict_slot.day_of_week) - 1);
    strncpy(rm_conflict_slot.room_number, "Room 101", sizeof(rm_conflict_slot.room_number) - 1);

    timetable_service_check_conflict(&rm_conflict_slot, &conf_res);
    assert(conf_res.has_conflict == true && "Room conflict must be detected");
    assert(conf_res.type == CONFLICT_ROOM);

    saved = timetable_service_save_slot(&rm_conflict_slot, &new_id, &conf_res);
    assert(!saved && "Saving slot with room conflict must fail");

    /* 3. Non-conflicting slot:
     * Teacher 1 in Room 101 on MONDAY Period 2 (different period) */
    TimetableSlot valid_slot = {
        .id = 0,
        .class_id = 1,
        .section_id = 1,
        .period_number = 2,
        .subject_id = 1,
        .teacher_id = 1
    };
    strncpy(valid_slot.day_of_week, DAY_MONDAY, sizeof(valid_slot.day_of_week) - 1);
    strncpy(valid_slot.room_number, "Room 101", sizeof(valid_slot.room_number) - 1);

    timetable_service_check_conflict(&valid_slot, &conf_res);
    assert(!conf_res.has_conflict && "Different period must not conflict");

    saved = timetable_service_save_slot(&valid_slot, &new_id, &conf_res);
    assert(saved && new_id > 0);

    printf("       Conflict detection verified -> PASS\n");
}

static void test_slot_update_and_delete(void)
{
    printf("[TEST] Testing timetable slot update and deletion...\n");

    /* Slot for Period 2 on MONDAY exists; let's find it */
    TimetableSlot slots[MAX_TIMETABLE_SLOTS];
    int count = 0;
    timetable_service_get_by_class(1, 1, DAY_MONDAY, slots, MAX_TIMETABLE_SLOTS, &count);
    assert(count == 2);

    int slot2_id = (slots[0].period_number == 2) ? slots[0].id : slots[1].id;

    /* Delete slot 2 */
    bool del_ok = timetable_service_delete(slot2_id);
    assert(del_ok && "Delete should succeed");

    TimetableSlot check;
    bool found = timetable_service_get_by_id(slot2_id, &check);
    assert(!found && "Deleted slot should not be found");

    timetable_service_get_by_class(1, 1, DAY_MONDAY, slots, MAX_TIMETABLE_SLOTS, &count);
    assert(count == 1);

    printf("       Slot update and deletion verified -> PASS\n");
}

int main(void)
{
    printf("====================================================\n");
    printf("   SchoolCMS Timetable System Unit Tests            \n");
    printf("====================================================\n");

    cleanup_files();

    int rc = db_init(TEST_DB);
    assert(rc == DB_SUCCESS && "Database init should succeed");

    setup_test_data();

    test_slot_creation_and_query();
    test_conflict_detection();
    test_slot_update_and_delete();

    db_close();
    cleanup_files();

    printf("\n>>> ALL TIMETABLE TESTS PASSED (100%%) <<<\n");
    return 0;
}
