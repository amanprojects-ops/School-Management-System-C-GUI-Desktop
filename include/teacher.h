/**
 * @file teacher.h
 * @brief Teacher and staff data models, database CRUD services, and filtering.
 */

#ifndef SCHOOLCMS_TEACHER_H
#define SCHOOLCMS_TEACHER_H

#include <stdbool.h>
#include <stddef.h>

#define TEACHER_MAX_LIST 100
#define STAFF_MAX_LIST 100

/**
 * @brief Teacher Entity Record
 */
typedef struct {
    int id;
    char teacher_code[32];
    int user_id;
    char full_name[128];
    char phone[32];
    char email[128];
    char address[256];
    char qualification[128];
    char subject[128];
    char joining_date[16];       /* YYYY-MM-DD */
    double salary;
    char status[16];             /* ACTIVE, INACTIVE, RESIGNED */
    char created_at[32];
} Teacher;

/**
 * @brief Support Staff Entity Record
 */
typedef struct {
    int id;
    char staff_code[32];
    int user_id;
    char full_name[128];
    char role_title[64];         /* e.g. Librarian, Accountant, Admin Clerk */
    char phone[32];
    char email[128];
    char address[256];
    char joining_date[16];       /* YYYY-MM-DD */
    double salary;
    char status[16];             /* ACTIVE, INACTIVE, RESIGNED */
    char created_at[32];
} Staff;

/* ── Teacher Services ─────────────────────────────────────────────── */

bool teacher_service_get_all(const char *search, const char *status,
                             Teacher *out_list, int max_items, int *out_count);

bool teacher_service_get_by_id(int id, Teacher *out_teacher);

bool teacher_service_create(const Teacher *teacher, int *out_new_id);

bool teacher_service_update(const Teacher *teacher);

bool teacher_service_delete(int id);

bool teacher_service_generate_code(char *out_code, size_t size);

/* ── Staff Services ───────────────────────────────────────────────── */

bool staff_service_get_all(const char *search, const char *status,
                           Staff *out_list, int max_items, int *out_count);

bool staff_service_get_by_id(int id, Staff *out_staff);

bool staff_service_create(const Staff *staff, int *out_new_id);

bool staff_service_update(const Staff *staff);

bool staff_service_delete(int id);

bool staff_service_generate_code(char *out_code, size_t size);

#endif /* SCHOOLCMS_TEACHER_H */
