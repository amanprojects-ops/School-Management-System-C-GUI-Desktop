/**
 * @file timetable.h
 * @brief Class and teacher timetables, period allocations, and double-booking conflict detection.
 */

#ifndef SCHOOLCMS_TIMETABLE_H
#define SCHOOLCMS_TIMETABLE_H

#include <stdbool.h>
#include <stddef.h>

#define MAX_TIMETABLE_PERIODS 8
#define MAX_TIMETABLE_DAYS 6
#define MAX_TIMETABLE_SLOTS 100

/**
 * @brief Days of week constants
 */
#define DAY_MONDAY    "MONDAY"
#define DAY_TUESDAY   "TUESDAY"
#define DAY_WEDNESDAY "WEDNESDAY"
#define DAY_THURSDAY  "THURSDAY"
#define DAY_FRIDAY    "FRIDAY"
#define DAY_SATURDAY  "SATURDAY"

/**
 * @brief Timetable Slot Entity
 */
typedef struct {
    int id;
    int class_id;
    char class_name[64];
    int section_id;
    char section_name[32];
    char day_of_week[16];        /* MONDAY..SATURDAY */
    int period_number;           /* 1..8 */
    char start_time[16];         /* HH:MM */
    char end_time[16];           /* HH:MM */
    int subject_id;
    char subject_code[32];
    char subject_name[128];
    int teacher_id;
    char teacher_name[128];
    char room_number[32];        /* e.g. "Room 101" or "Physics Lab" */
    char created_at[32];
} TimetableSlot;

/**
 * @brief Conflict Check Status
 */
typedef enum {
    CONFLICT_NONE = 0,
    CONFLICT_TEACHER,
    CONFLICT_ROOM,
    CONFLICT_CLASS_SLOT
} ConflictType;

typedef struct {
    bool has_conflict;
    ConflictType type;
    char message[256];
} ConflictResult;

/* ── Service Functions ────────────────────────────────────────────── */

/**
 * @brief Check if a proposed slot causes teacher, room, or class conflicts.
 */
bool timetable_service_check_conflict(const TimetableSlot *slot, ConflictResult *out_result);

/**
 * @brief Save or update a timetable slot with conflict detection.
 * If conflict occurs, returns false and fills out_conflict.
 */
bool timetable_service_save_slot(const TimetableSlot *slot, int *out_id, ConflictResult *out_conflict);

/**
 * @brief Retrieve all scheduled slots for a class and section (optionally filtered by day).
 */
bool timetable_service_get_by_class(int class_id, int section_id, const char *day_of_week,
                                    TimetableSlot *out_list, int max_items, int *out_count);

/**
 * @brief Retrieve all teaching slots for a teacher (optionally filtered by day).
 */
bool timetable_service_get_by_teacher(int teacher_id, const char *day_of_week,
                                      TimetableSlot *out_list, int max_items, int *out_count);

/**
 * @brief Get a single slot by ID.
 */
bool timetable_service_get_by_id(int id, TimetableSlot *out_slot);

/**
 * @brief Delete a timetable slot by ID.
 */
bool timetable_service_delete(int id);

#endif /* SCHOOLCMS_TIMETABLE_H */
