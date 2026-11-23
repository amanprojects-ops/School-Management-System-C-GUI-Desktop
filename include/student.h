/**
 * @file student.h
 * @brief Student data models, database CRUD services, filtering, and pagination.
 */

#ifndef SCHOOLCMS_STUDENT_H
#define SCHOOLCMS_STUDENT_H

#include <stdbool.h>
#include <stddef.h>

#define STUDENT_PAGE_SIZE_DEFAULT 10
#define STUDENT_MAX_PER_PAGE 50

/**
 * @brief Complete Student Data Record
 */
typedef struct {
    int id;
    char admission_no[32];
    char full_name[128];
    char father_name[128];
    char mother_name[128];
    char dob[16];                /* YYYY-MM-DD */
    char gender[16];             /* MALE, FEMALE, OTHER */
    char category[32];           /* GENERAL, OBC, SC, ST, etc. */
    char phone[32];
    char email[128];
    char address[256];
    char admission_date[16];     /* YYYY-MM-DD */
    int class_id;
    char class_name[64];         /* Joined for display */
    int section_id;
    char section_name[32];       /* Joined for display */
    int roll_number;
    char previous_school[128];
    char emergency_contact[32];
    char blood_group[8];         /* A+, O-, etc. */
    char photo_path[256];
    char status[16];             /* ACTIVE, INACTIVE, TRANSFERRED */
    char created_at[32];
    char updated_at[32];
} Student;

/**
 * @brief Search & Filtering Parameters
 */
typedef struct {
    char search_query[128];      /* Matches name or admission_no */
    int class_id;                /* 0 for all classes */
    int section_id;              /* 0 for all sections */
    char gender[16];             /* Empty for all genders */
    char status[16];             /* Empty for all statuses */
    int admission_year;          /* 0 for all years */
    int page;                    /* 1-based index */
    int page_size;               /* Records per page */
} StudentFilter;

/**
 * @brief Paginated Result Set
 */
typedef struct {
    Student items[STUDENT_MAX_PER_PAGE];
    int count;
    int total_records;
    int total_pages;
    int current_page;
    int page_size;
} StudentPage;

/* ── Service API ──────────────────────────────────────────────────── */

/**
 * @brief Fetch a paginated, filtered list of students.
 */
bool student_service_get_page(const StudentFilter *filter, StudentPage *out_page);

/**
 * @brief Get a single student by primary key ID.
 */
bool student_service_get_by_id(int id, Student *out_student);

/**
 * @brief Get a single student by admission number.
 */
bool student_service_get_by_admission_no(const char *admission_no, Student *out_student);

/**
 * @brief Insert a new student record into the database.
 * @param student Struct containing student fields.
 * @param out_new_id Optional pointer to receive generated primary key ID.
 * @return true on success.
 */
bool student_service_create(const Student *student, int *out_new_id);

/**
 * @brief Update an existing student record.
 */
bool student_service_update(const Student *student);

/**
 * @brief Delete a student record by ID.
 */
bool student_service_delete(int id);

/**
 * @brief Generate next sequential admission number (e.g. ADM-2026-0001).
 */
bool student_service_generate_admission_no(char *out_admission_no, size_t size);

/**
 * @brief Fetch students for ID card generation filtered by class, section, and status.
 * @param class_id Target class ID (0 for all classes).
 * @param section_id Target section ID (0 for all sections).
 * @param status Target status string (empty or NULL for all).
 * @param out_students Array to receive student records.
 * @param max_students Maximum capacity of out_students.
 * @param out_count Pointer to receive number of students returned.
 * @return true on success.
 */
bool student_service_get_for_id_cards(int class_id, int section_id, const char *status,
                                     Student *out_students, int max_students, int *out_count);

#endif /* SCHOOLCMS_STUDENT_H */
