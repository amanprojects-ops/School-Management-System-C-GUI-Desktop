/**
 * @file dashboard.h
 * @brief Dashboard data models, dynamic database statistics queries, and UI page interface.
 */

#ifndef SCHOOLCMS_DASHBOARD_H
#define SCHOOLCMS_DASHBOARD_H

#include <gtk/gtk.h>
#include <stdbool.h>

#define DASHBOARD_MAX_RECENT 5
#define DASHBOARD_CHART_POINTS 7

/**
 * @brief Aggregated real-time metrics for dashboard cards.
 */
typedef struct {
    int total_students;
    int total_teachers;
    int total_staff;
    int total_classes;
    int today_attendance_present;
    int today_attendance_total;
    double today_attendance_pct;
    double fee_collection_this_month;
    double fee_pending_total;
    int new_admissions_count;
} DashboardStats;

/**
 * @brief Recent student admission item.
 */
typedef struct {
    char admission_no[32];
    char full_name[128];
    char class_name[64];
    char admission_date[32];
} RecentAdmission;

/**
 * @brief Recent audit activity log item.
 */
typedef struct {
    char action[64];
    char details[256];
    char created_at[32];
} RecentActivity;

/**
 * @brief Scheduled upcoming examination item.
 */
typedef struct {
    char name[128];
    char start_date[32];
    char status[32];
} UpcomingExam;

/**
 * @brief Weekly attendance data point for Cairo chart.
 */
typedef struct {
    char label[16];
    double percentage;
} AttendanceChartPoint;

/**
 * @brief Monthly fee collection data point for Cairo chart.
 */
typedef struct {
    char label[16];
    double amount_thousands;
} FeeChartPoint;

/* ── Service / Database Query API ─────────────────────────────────── */

/**
 * @brief Fetch live aggregated statistics from the SQLite database.
 * @param out_stats Pointer to stats struct to fill.
 * @return true on success.
 */
bool dashboard_service_get_stats(DashboardStats *out_stats);

/**
 * @brief Fetch the latest student admissions.
 */
bool dashboard_service_get_recent_admissions(RecentAdmission *out_items, int max_items, int *out_count);

/**
 * @brief Fetch the latest system activities from activity_logs.
 */
bool dashboard_service_get_recent_activities(RecentActivity *out_items, int max_items, int *out_count);

/**
 * @brief Fetch upcoming examinations.
 */
bool dashboard_service_get_upcoming_exams(UpcomingExam *out_items, int max_items, int *out_count);

/**
 * @brief Fetch weekly attendance trend points for chart rendering.
 */
bool dashboard_service_get_weekly_attendance(AttendanceChartPoint *out_points, int *out_count);

/**
 * @brief Fetch monthly fee collection trend points for chart rendering.
 */
bool dashboard_service_get_fee_trends(FeeChartPoint *out_points, int *out_count);

/* ── UI Page API ──────────────────────────────────────────────────── */

/**
 * @brief Create the dashboard page widget with dynamic cards, Cairo charts, and tables.
 * @return GtkWidget* for the dashboard page.
 */
GtkWidget *school_dashboard_new(void);

/**
 * @brief Refresh dashboard statistics and redraw charts.
 * @param dashboard_widget The widget returned by school_dashboard_new.
 */
void school_dashboard_refresh(GtkWidget *dashboard_widget);

#endif /* SCHOOLCMS_DASHBOARD_H */
