/**
 * @file exams.h
 * @brief Examination schedules, subject marks, grade calculation, and report cards.
 */

#ifndef SCHOOLCMS_EXAMS_H
#define SCHOOLCMS_EXAMS_H

#include <stdbool.h>
#include <stddef.h>

#define MAX_EXAMINATIONS 50
#define MAX_EXAM_SUBJECTS 50
#define MAX_REPORT_CARD_SUBJECTS 30
#define MAX_CLASS_RANKS 200

/**
 * @brief Examination Schedule / Event Entity
 */
typedef struct {
    int id;
    char name[128];              /* e.g. "Mid-Term Examination 2026" */
    int academic_year_id;
    char academic_year_name[32];
    char start_date[16];         /* YYYY-MM-DD */
    char end_date[16];           /* YYYY-MM-DD */
    char grading_system[32];     /* PERCENTAGE, GPA */
    char status[32];             /* PLANNED, ONGOING, COMPLETED */
} Examination;

/**
 * @brief Subject scheduled in an Examination
 */
typedef struct {
    int id;
    int exam_id;
    char exam_name[128];
    int subject_id;
    char subject_code[32];
    char subject_name[128];
    int class_id;
    char class_name[64];
    char exam_date[16];          /* YYYY-MM-DD */
    double max_marks;            /* e.g. 100.0 */
    double passing_marks;        /* e.g. 35.0 */
} ExamSubject;

/**
 * @brief Marks obtained in a subject by a student
 */
typedef struct {
    int id;
    int exam_subject_id;
    int exam_id;
    char exam_name[128];
    int student_id;
    char student_name[128];
    char admission_no[32];
    int roll_number;
    int class_id;
    char class_name[64];
    char section_name[32];
    char subject_code[32];
    char subject_name[128];
    double marks_obtained;
    double max_marks;
    double passing_marks;
    char grade[8];               /* A+, A, B, C, D, F */
    bool is_pass;
    char remarks[128];
} ExamResult;

/**
 * @brief Single subject score entry on a report card
 */
typedef struct {
    char code[32];
    char subject_name[128];
    double max_marks;
    double passing_marks;
    double marks_obtained;
    char grade[8];
    bool is_pass;
    char remarks[64];
} SubjectScore;

/**
 * @brief Comprehensive Student Report Card
 */
typedef struct {
    int student_id;
    char student_name[128];
    char admission_no[32];
    int roll_number;
    char class_name[64];
    char section_name[32];
    char exam_name[128];
    char academic_year[32];
    SubjectScore scores[MAX_REPORT_CARD_SUBJECTS];
    int score_count;
    double total_marks_obtained;
    double total_max_marks;
    double percentage;
    char overall_grade[8];
    bool is_pass;
    int class_rank;
} StudentReportCard;

/**
 * @brief Class Merit Rank Item
 */
typedef struct {
    int student_id;
    char student_name[128];
    char admission_no[32];
    int roll_number;
    double total_obtained;
    double total_max;
    double percentage;
    char overall_grade[8];
    bool is_pass;
    int rank;
} ClassExamRank;

/* ── Service Functions ────────────────────────────────────────────── */

/**
 * @brief Calculate letter grade from percentage (configurable scale).
 */
const char *exam_service_calculate_grade(double percentage);

/* Examination CRUD */
bool exam_service_get_all(Examination *out_list, int max_items, int *out_count);
bool exam_service_get_by_id(int id, Examination *out_exam);
bool exam_service_create(const Examination *exam, int *out_id);
bool exam_service_update(const Examination *exam);
bool exam_service_delete(int id);

/* Exam Subject Scheduling */
bool exam_subject_create(const ExamSubject *es, int *out_id);
bool exam_subject_get_by_exam(int exam_id, ExamSubject *out_list, int max_items, int *out_count);
bool exam_subject_delete(int id);

/* Marks Entry & Results */
bool exam_result_record(ExamResult *res, int *out_id);
bool exam_result_get_class_sheet(int exam_subject_id, ExamResult *out_list, int max_items, int *out_count);

/* Report Card & Class Ranking */
bool exam_service_generate_report_card(int exam_id, int student_id, StudentReportCard *out_card);
bool exam_service_get_class_ranks(int exam_id, int class_id,
                                  ClassExamRank *out_ranks, int max_ranks, int *out_count);

#endif /* SCHOOLCMS_EXAMS_H */
