/**
 * @file reports.h
 * @brief Report generation and CSV data export services for SchoolCMS.
 */

#ifndef SCHOOLCMS_REPORTS_H
#define SCHOOLCMS_REPORTS_H

#include <stdbool.h>
#include <stddef.h>

#define MAX_REPORT_COLUMNS 16
#define MAX_REPORT_ROWS 1000
#define MAX_REPORT_CELL_LEN 128

/**
 * @brief Supported Report Categories
 */
typedef enum {
    REPORT_STUDENT_LIST = 0,
    REPORT_ATTENDANCE_SUMMARY,
    REPORT_FEE_COLLECTION,
    REPORT_PENDING_DUES,
    REPORT_TEACHER_LIST,
    REPORT_EXAM_RESULTS,
    REPORT_TYPE_COUNT
} ReportType;

/**
 * @brief Structured In-Memory Report Dataset
 */
typedef struct {
    ReportType type;
    char title[128];
    char header[MAX_REPORT_COLUMNS][64];
    int column_count;
    char rows[MAX_REPORT_ROWS][MAX_REPORT_COLUMNS][MAX_REPORT_CELL_LEN];
    int row_count;
    char summary_text[256];
    double total_amount;
} ReportData;

/**
 * @brief Filter Parameters for Report Generation
 */
typedef struct {
    ReportType type;
    int class_id;            /* 0 = all classes */
    int section_id;          /* 0 = all sections */
    char date_from[16];      /* YYYY-MM-DD or empty */
    char date_to[16];        /* YYYY-MM-DD or empty */
    char month_str[16];      /* YYYY-MM or empty */
    int exam_id;             /* 0 = all / not applicable */
    char status_filter[32];  /* e.g. "ACTIVE" or empty */
} ReportFilter;

/* ── Service Functions ────────────────────────────────────────────── */

/**
 * @brief Generate report dataset in memory according to filters.
 * @param filter Pointer to ReportFilter configuring criteria.
 * @param out_data Pointer to ReportData to receive formatted rows.
 * @return true on success, false on database or parameter error.
 */
bool report_service_generate(const ReportFilter *filter, ReportData *out_data);

/**
 * @brief Export report data directly to a CSV file (RFC 4180 with UTF-8 BOM).
 * @param data Pre-generated report dataset.
 * @param filepath Absolute or relative path to output .csv file.
 * @return true on success, false on I/O error.
 */
bool report_service_export_csv(const ReportData *data, const char *filepath);

/**
 * @brief Convenience function: generate report and export directly to CSV.
 * @param filter Report filter criteria.
 * @param filepath Output CSV file path.
 * @param out_rows Pointer to receive total rows written.
 * @return true on success, false otherwise.
 */
bool report_service_generate_and_export_csv(const ReportFilter *filter, const char *filepath, int *out_rows);

/**
 * @brief Helper to escape a string field according to RFC 4180 CSV standard.
 * @param src Input string.
 * @param dst Output buffer for escaped string.
 * @param dst_size Size of destination buffer.
 */
void report_escape_csv_field(const char *src, char *dst, size_t dst_size);

#endif /* SCHOOLCMS_REPORTS_H */
