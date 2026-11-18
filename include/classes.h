/**
 * @file classes.h
 * @brief Class, section, subject, and academic year data models and CRUD services.
 */

#ifndef SCHOOLCMS_CLASSES_H
#define SCHOOLCMS_CLASSES_H

#include <stdbool.h>
#include <stddef.h>

#define MAX_CLASSES 50
#define MAX_SECTIONS 100
#define MAX_SUBJECTS 100
#define MAX_ACADEMIC_YEARS 20

/**
 * @brief Academic Year Entity
 */
typedef struct {
    int id;
    char name[32];               /* e.g. "2026-2027" */
    char start_date[16];         /* YYYY-MM-DD */
    char end_date[16];           /* YYYY-MM-DD */
    bool is_current;
} AcademicYear;

/**
 * @brief Class Entity with aggregations
 */
typedef struct {
    int id;
    char name[64];               /* e.g. "Class 10" */
    int numeric_value;           /* e.g. 10 */
    int capacity;                /* max students */
    int academic_year_id;
    char academic_year_name[32];
    int section_count;
    int student_count;
} SchoolClass;

/**
 * @brief Section Entity with assigned class teacher
 */
typedef struct {
    int id;
    int class_id;
    char class_name[64];
    char name[32];               /* e.g. "A" */
    int class_teacher_id;
    char class_teacher_name[128];
    int student_count;
} ClassSection;

/**
 * @brief Subject Entity with assigned teacher
 */
typedef struct {
    int id;
    char code[32];               /* e.g. "ENG-101" */
    char name[128];              /* e.g. "English Literature" */
    int class_id;
    char class_name[64];
    int teacher_id;
    char teacher_name[128];
} Subject;

/* ── Academic Year Services ───────────────────────────────────────── */

bool academic_year_get_all(AcademicYear *out_list, int max_items, int *out_count);
bool academic_year_get_current(AcademicYear *out_year);
bool academic_year_create(const AcademicYear *year, int *out_id);
bool academic_year_set_current(int id);

/* ── Class Services ───────────────────────────────────────────────── */

bool class_service_get_all(SchoolClass *out_list, int max_items, int *out_count);
bool class_service_get_by_id(int id, SchoolClass *out_class);
bool class_service_create(const SchoolClass *cls, int *out_new_id);
bool class_service_update(const SchoolClass *cls);
bool class_service_delete(int id);

/* ── Section Services ─────────────────────────────────────────────── */

bool section_service_get_by_class(int class_id, ClassSection *out_list, int max_items, int *out_count);
bool section_service_get_all(ClassSection *out_list, int max_items, int *out_count);
bool section_service_create(const ClassSection *section, int *out_new_id);
bool section_service_update(const ClassSection *section);
bool section_service_delete(int id);

/* ── Subject Services ─────────────────────────────────────────────── */

bool subject_service_get_by_class(int class_id, Subject *out_list, int max_items, int *out_count);
bool subject_service_get_all(Subject *out_list, int max_items, int *out_count);
bool subject_service_create(const Subject *subject, int *out_new_id);
bool subject_service_update(const Subject *subject);
bool subject_service_delete(int id);

#endif /* SCHOOLCMS_CLASSES_H */
