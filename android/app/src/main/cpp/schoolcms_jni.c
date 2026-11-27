/**
 * @file schoolcms_jni.c
 * @brief Thread-safe JNI bridge exposing shared C11 persistence, business logic,
 *        and Schema V3 delta sync APIs to the APCOACHING Android Kotlin app.
 *
 * Uses SQLite's native json_object() / json_each() helpers for zero-copy UTF-8
 * JSON serialization and heap-allocates large C structs safely for Android
 * worker thread stacks.
 */

#include <jni.h>
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "database.h"
#include "auth/auth.h"
#include "dashboard.h"
#include "student.h"
#include "teacher.h"
#include "classes.h"
#include "attendance.h"
#include "fees.h"
#include "exams.h"
#include "timetable.h"
#include "reports.h"
#include "settings.h"
#include "sync/sync.h"

/* ── Portable JSON & String Builders ──────────────────────────────── */

typedef struct {
    char *data;
    size_t len;
    size_t cap;
} JniStr;

static bool jstr_init(JniStr *b, size_t cap)
{
    b->cap = cap > 0 ? cap : 512;
    b->data = (char *)malloc(b->cap);
    if (!b->data) return false;
    b->data[0] = '\0';
    b->len = 0;
    return true;
}

static bool jstr_append(JniStr *b, const char *s, size_t n)
{
    if (!b || !s) return false;
    if (b->len + n + 1 > b->cap) {
        size_t nc = b->cap * 2;
        while (nc < b->len + n + 1) nc *= 2;
        char *r = (char *)realloc(b->data, nc);
        if (!r) return false;
        b->data = r;
        b->cap = nc;
    }
    memcpy(b->data + b->len, s, n);
    b->len += n;
    b->data[b->len] = '\0';
    return true;
}

static bool jstr_append_raw_json_str(JniStr *b, const char *utf8)
{
    if (!utf8) utf8 = "";
    if (!jstr_append(b, "\"", 1)) return false;
    for (const unsigned char *p = (const unsigned char *)utf8; *p; p++) {
        unsigned char c = *p;
        if (c == '"') {
            if (!jstr_append(b, "\\\"", 2)) return false;
        } else if (c == '\\') {
            if (!jstr_append(b, "\\\\", 2)) return false;
        } else if (c == '\b') {
            if (!jstr_append(b, "\\b", 2)) return false;
        } else if (c == '\f') {
            if (!jstr_append(b, "\\f", 2)) return false;
        } else if (c == '\n') {
            if (!jstr_append(b, "\\n", 2)) return false;
        } else if (c == '\r') {
            if (!jstr_append(b, "\\r", 2)) return false;
        } else if (c == '\t') {
            if (!jstr_append(b, "\\t", 2)) return false;
        } else if (c < 0x20) {
            char esc[8];
            snprintf(esc, sizeof(esc), "\\u%04x", (unsigned int)c);
            if (!jstr_append(b, esc, strlen(esc))) return false;
        } else {
            char ch = (char)c;
            if (!jstr_append(b, &ch, 1)) return false;
        }
    }
    return jstr_append(b, "\"", 1);
}

static jstring to_jstring_and_free(JNIEnv *env, JniStr *b, const char *fallback)
{
    jstring out = (*env)->NewStringUTF(env, (b && b->data) ? b->data : fallback);
    if (b && b->data) {
        free(b->data);
        b->data = NULL;
    }
    return out;
}

static void copy_jstr(JNIEnv *env, jstring js, char *out, size_t sz)
{
    if (!out || sz == 0) return;
    out[0] = '\0';
    if (!js) return;
    const char *utf = (*env)->GetStringUTFChars(env, js, NULL);
    if (utf) {
        snprintf(out, sz, "%s", utf);
        (*env)->ReleaseStringUTFChars(env, js, utf);
    }
}

static void json_get_str(sqlite3 *db, const char *json, const char *key, char *out, size_t sz)
{
    if (!out || sz == 0) return;
    out[0] = '\0';
    if (!db || !json || !key) return;
    char path[96];
    snprintf(path, sizeof(path), "$.%s", key);
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, "SELECT CAST(json_extract(?1, ?2) AS TEXT);", -1, &stmt, NULL) != SQLITE_OK) return;
    sqlite3_bind_text(stmt, 1, json, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, path, -1, SQLITE_STATIC);
    if (sqlite3_step(stmt) == SQLITE_ROW && sqlite3_column_type(stmt, 0) != SQLITE_NULL) {
        const char *txt = (const char *)sqlite3_column_text(stmt, 0);
        if (txt) snprintf(out, sz, "%s", txt);
    }
    sqlite3_finalize(stmt);
}

static long long json_get_int(sqlite3 *db, const char *json, const char *key, long long def_val)
{
    if (!db || !json || !key) return def_val;
    char path[96];
    snprintf(path, sizeof(path), "$.%s", key);
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, "SELECT json_extract(?1, ?2);", -1, &stmt, NULL) != SQLITE_OK) return def_val;
    sqlite3_bind_text(stmt, 1, json, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, path, -1, SQLITE_STATIC);
    long long res = def_val;
    if (sqlite3_step(stmt) == SQLITE_ROW && sqlite3_column_type(stmt, 0) != SQLITE_NULL) {
        res = sqlite3_column_int64(stmt, 0);
    }
    sqlite3_finalize(stmt);
    return res;
}

static double json_get_double(sqlite3 *db, const char *json, const char *key, double def_val)
{
    if (!db || !json || !key) return def_val;
    char path[96];
    snprintf(path, sizeof(path), "$.%s", key);
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, "SELECT json_extract(?1, ?2);", -1, &stmt, NULL) != SQLITE_OK) return def_val;
    sqlite3_bind_text(stmt, 1, json, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, path, -1, SQLITE_STATIC);
    double res = def_val;
    if (sqlite3_step(stmt) == SQLITE_ROW && sqlite3_column_type(stmt, 0) != SQLITE_NULL) {
        res = sqlite3_column_double(stmt, 0);
    }
    sqlite3_finalize(stmt);
    return res;
}

static jstring make_ok_id_json(JNIEnv *env, bool ok, int id, const char *err)
{
    char buf[384];
    JniStr b;
    jstr_init(&b, 384);
    snprintf(buf, sizeof(buf), "{\"ok\":%s,\"id\":%d,\"error\":", ok ? "true" : "false", id);
    jstr_append(&b, buf, strlen(buf));
    jstr_append_raw_json_str(&b, err ? err : "");
    jstr_append(&b, "}", 1);
    return to_jstring_and_free(env, &b, "{\"ok\":false}");
}

/* ── DB Lifecycle & Version ───────────────────────────────────────── */

JNIEXPORT jboolean JNICALL
Java_com_apcoaching_schoolcms_nativebridge_NativeBridge_initDatabase(
    JNIEnv *env, jobject thiz, jstring j_db_path, jstring j_files_dir)
{
    (void)thiz;
    char db_path[512], files_dir[512];
    copy_jstr(env, j_db_path, db_path, sizeof(db_path));
    copy_jstr(env, j_files_dir, files_dir, sizeof(files_dir));

    if (files_dir[0] != '\0') {
        chdir(files_dir);
    }

    db_lock();
    DbResult rc = db_init(db_path);
    if (rc == DB_SUCCESS) {
        auth_init();
        db_sync_set_state("device_prefix", "M");
    }
    db_unlock();
    return (rc == DB_SUCCESS) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL
Java_com_apcoaching_schoolcms_nativebridge_NativeBridge_closeDatabase(JNIEnv *env, jobject thiz)
{
    (void)env; (void)thiz;
    db_lock();
    db_close();
    db_unlock();
}

JNIEXPORT jint JNICALL
Java_com_apcoaching_schoolcms_nativebridge_NativeBridge_getDbVersion(JNIEnv *env, jobject thiz)
{
    (void)env; (void)thiz;
    db_lock();
    int v = db_get_version();
    db_unlock();
    return v;
}

/* ── Authentication & RBAC ────────────────────────────────────────── */

JNIEXPORT jstring JNICALL
Java_com_apcoaching_schoolcms_nativebridge_NativeBridge_authLogin(
    JNIEnv *env, jobject thiz, jstring j_user, jstring j_pass)
{
    (void)thiz;
    char user[64], pass[128];
    copy_jstr(env, j_user, user, sizeof(user));
    copy_jstr(env, j_pass, pass, sizeof(pass));

    UserSession sess;
    memset(&sess, 0, sizeof(sess));

    db_lock();
    AuthResult res = auth_login(user, pass, &sess);
    db_unlock();

    JniStr b;
    jstr_init(&b, 512);
    char head[128];
    snprintf(head, sizeof(head), "{\"ok\":%s,\"code\":%d,\"userId\":%d,\"roleId\":%d,",
             res == AUTH_SUCCESS ? "true" : "false", (int)res, sess.user_id, sess.role_id);
    jstr_append(&b, head, strlen(head));
    jstr_append(&b, "\"username\":", 11); jstr_append_raw_json_str(&b, sess.username);
    jstr_append(&b, ",\"fullName\":", 12); jstr_append_raw_json_str(&b, sess.full_name);
    jstr_append(&b, ",\"roleName\":", 12); jstr_append_raw_json_str(&b, sess.role_name);
    jstr_append(&b, ",\"email\":", 9); jstr_append_raw_json_str(&b, sess.email);

    const char *msg = "";
    if (res == AUTH_ERR_INVALID_CREDENTIALS) msg = "Invalid username or password.";
    else if (res == AUTH_ERR_ACCOUNT_LOCKED) msg = "Account temporarily locked due to failed attempts.";
    else if (res == AUTH_ERR_INACTIVE) msg = "Account is inactive. Contact Administrator.";
    else if (res != AUTH_SUCCESS) msg = "Authentication database error.";
    jstr_append(&b, ",\"error\":", 9); jstr_append_raw_json_str(&b, msg);
    jstr_append(&b, "}", 1);
    return to_jstring_and_free(env, &b, "{\"ok\":false}");
}

JNIEXPORT void JNICALL
Java_com_apcoaching_schoolcms_nativebridge_NativeBridge_authLogout(JNIEnv *env, jobject thiz)
{
    (void)env; (void)thiz;
    db_lock();
    auth_logout();
    db_unlock();
}

JNIEXPORT jboolean JNICALL
Java_com_apcoaching_schoolcms_nativebridge_NativeBridge_authCanAccess(
    JNIEnv *env, jobject thiz, jstring j_mod, jboolean j_edit)
{
    (void)thiz;
    char mod[32];
    copy_jstr(env, j_mod, mod, sizeof(mod));
    db_lock();
    bool ok = auth_current_user_can_access(mod, j_edit == JNI_TRUE);
    db_unlock();
    return ok ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jstring JNICALL
Java_com_apcoaching_schoolcms_nativebridge_NativeBridge_authChangePassword(
    JNIEnv *env, jobject thiz, jint user_id, jstring j_old, jstring j_new)
{
    (void)thiz;
    char oldp[128], newp[128];
    copy_jstr(env, j_old, oldp, sizeof(oldp));
    copy_jstr(env, j_new, newp, sizeof(newp));
    db_lock();
    AuthResult res = auth_change_password(user_id, oldp, newp);
    db_unlock();
    return make_ok_id_json(env, res == AUTH_SUCCESS, user_id,
                           res == AUTH_SUCCESS ? "" : "Current password incorrect or invalid input");
}

JNIEXPORT jstring JNICALL
Java_com_apcoaching_schoolcms_nativebridge_NativeBridge_authGetUsersJson(JNIEnv *env, jobject thiz)
{
    (void)thiz;
    ManagedUser *users = (ManagedUser *)calloc(100, sizeof(ManagedUser));
    if (!users) return (*env)->NewStringUTF(env, "[]");
    int count = 0;
    db_lock();
    auth_get_all_users(users, 100, &count);
    db_unlock();

    JniStr b;
    jstr_init(&b, 2048);
    jstr_append(&b, "[", 1);
    for (int i = 0; i < count; i++) {
        if (i > 0) jstr_append(&b, ",", 1);
        char num[128];
        snprintf(num, sizeof(num), "{\"id\":%d,\"roleId\":%d,\"failedAttempts\":%d,",
                 users[i].id, users[i].role_id, users[i].failed_attempts);
        jstr_append(&b, num, strlen(num));
        jstr_append(&b, "\"username\":", 11); jstr_append_raw_json_str(&b, users[i].username);
        jstr_append(&b, ",\"fullName\":", 12); jstr_append_raw_json_str(&b, users[i].full_name);
        jstr_append(&b, ",\"roleName\":", 12); jstr_append_raw_json_str(&b, users[i].role_name);
        jstr_append(&b, ",\"email\":", 9); jstr_append_raw_json_str(&b, users[i].email);
        jstr_append(&b, ",\"phone\":", 9); jstr_append_raw_json_str(&b, users[i].phone);
        jstr_append(&b, ",\"status\":", 10); jstr_append_raw_json_str(&b, users[i].status);
        jstr_append(&b, "}", 1);
    }
    jstr_append(&b, "]", 1);
    free(users);
    return to_jstring_and_free(env, &b, "[]");
}

JNIEXPORT jstring JNICALL
Java_com_apcoaching_schoolcms_nativebridge_NativeBridge_authCreateUserJson(
    JNIEnv *env, jobject thiz, jstring j_json)
{
    (void)thiz;
    const char *raw = (*env)->GetStringUTFChars(env, j_json, NULL);
    if (!raw) return make_ok_id_json(env, false, 0, "Invalid string");

    char username[64], pass[128], full_name[128], email[128], phone[32];
    db_lock();
    sqlite3 *db = db_get_handle();
    json_get_str(db, raw, "username", username, sizeof(username));
    json_get_str(db, raw, "password", pass, sizeof(pass));
    json_get_str(db, raw, "fullName", full_name, sizeof(full_name));
    json_get_str(db, raw, "email", email, sizeof(email));
    json_get_str(db, raw, "phone", phone, sizeof(phone));
    int role_id = (int)json_get_int(db, raw, "roleId", ROLE_STAFF);
    AuthResult res = auth_create_user(username, pass, role_id, full_name, email, phone);
    db_unlock();
    (*env)->ReleaseStringUTFChars(env, j_json, raw);
    return make_ok_id_json(env, res == AUTH_SUCCESS, 0,
                           res == AUTH_SUCCESS ? "" : "Could not create user (duplicate username/email)");
}

JNIEXPORT jboolean JNICALL
Java_com_apcoaching_schoolcms_nativebridge_NativeBridge_authToggleUserStatus(
    JNIEnv *env, jobject thiz, jint user_id)
{
    (void)env; (void)thiz;
    db_lock();
    AuthResult res = auth_toggle_user_status(user_id);
    db_unlock();
    return (res == AUTH_SUCCESS) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jstring JNICALL
Java_com_apcoaching_schoolcms_nativebridge_NativeBridge_authGetRolePermissionsJson(
    JNIEnv *env, jobject thiz, jint role_id)
{
    (void)thiz;
    RolePermission *perms = (RolePermission *)calloc(16, sizeof(RolePermission));
    if (!perms) return (*env)->NewStringUTF(env, "[]");
    int count = 0;
    db_lock();
    auth_get_role_permissions(role_id, perms, 16, &count);
    db_unlock();

    JniStr b;
    jstr_init(&b, 1024);
    jstr_append(&b, "[", 1);
    for (int i = 0; i < count; i++) {
        if (i > 0) jstr_append(&b, ",", 1);
        char buf[128];
        snprintf(buf, sizeof(buf), "{\"id\":%d,\"roleId\":%d,\"canView\":%s,\"canEdit\":%s,\"moduleName\":",
                 perms[i].id, perms[i].role_id,
                 perms[i].can_view ? "true" : "false",
                 perms[i].can_edit ? "true" : "false");
        jstr_append(&b, buf, strlen(buf));
        jstr_append_raw_json_str(&b, perms[i].module_name);
        jstr_append(&b, "}", 1);
    }
    jstr_append(&b, "]", 1);
    free(perms);
    return to_jstring_and_free(env, &b, "[]");
}

JNIEXPORT jboolean JNICALL
Java_com_apcoaching_schoolcms_nativebridge_NativeBridge_authSetRolePermission(
    JNIEnv *env, jobject thiz, jint role_id, jstring j_mod, jboolean j_view, jboolean j_edit)
{
    (void)thiz;
    char mod[32];
    copy_jstr(env, j_mod, mod, sizeof(mod));
    db_lock();
    bool ok = auth_set_role_permission(role_id, mod, j_view == JNI_TRUE, j_edit == JNI_TRUE);
    db_unlock();
    return ok ? JNI_TRUE : JNI_FALSE;
}

/* ── Dashboard Metrics ────────────────────────────────────────────── */

JNIEXPORT jstring JNICALL
Java_com_apcoaching_schoolcms_nativebridge_NativeBridge_getDashboardJson(JNIEnv *env, jobject thiz)
{
    (void)thiz;
    DashboardStats stats = {0};
    RecentAdmission rec_adm[DASHBOARD_MAX_RECENT];
    UpcomingExam up_ex[DASHBOARD_MAX_RECENT];
    int adm_cnt = 0, ex_cnt = 0;

    memset(rec_adm, 0, sizeof(rec_adm));
    memset(up_ex, 0, sizeof(up_ex));

    db_lock();
    dashboard_service_get_stats(&stats);
    dashboard_service_get_recent_admissions(rec_adm, DASHBOARD_MAX_RECENT, &adm_cnt);
    dashboard_service_get_upcoming_exams(up_ex, DASHBOARD_MAX_RECENT, &ex_cnt);
    db_unlock();

    JniStr b;
    jstr_init(&b, 2048);
    char hdr[512];
    snprintf(hdr, sizeof(hdr),
             "{\"totalStudents\":%d,\"totalTeachers\":%d,\"totalStaff\":%d,\"totalClasses\":%d,"
             "\"todayAttendancePresent\":%d,\"todayAttendanceTotal\":%d,\"todayAttendancePct\":%.1f,"
             "\"feeCollectionThisMonth\":%.2f,\"feePendingTotal\":%.2f,\"newAdmissionsCount\":%d,"
             "\"recentAdmissions\":[",
             stats.total_students, stats.total_teachers, stats.total_staff, stats.total_classes,
             stats.today_attendance_present, stats.today_attendance_total, stats.today_attendance_pct,
             stats.fee_collection_this_month, stats.fee_pending_total, stats.new_admissions_count);
    jstr_append(&b, hdr, strlen(hdr));

    for (int i = 0; i < adm_cnt; i++) {
        if (i > 0) jstr_append(&b, ",", 1);
        jstr_append(&b, "{\"admissionNo\":", 15); jstr_append_raw_json_str(&b, rec_adm[i].admission_no);
        jstr_append(&b, ",\"fullName\":", 12); jstr_append_raw_json_str(&b, rec_adm[i].full_name);
        jstr_append(&b, ",\"className\":", 13); jstr_append_raw_json_str(&b, rec_adm[i].class_name);
        jstr_append(&b, ",\"admissionDate\":", 17); jstr_append_raw_json_str(&b, rec_adm[i].admission_date);
        jstr_append(&b, "}", 1);
    }
    jstr_append(&b, "],\"upcomingExams\":[", 19);
    for (int i = 0; i < ex_cnt; i++) {
        if (i > 0) jstr_append(&b, ",", 1);
        jstr_append(&b, "{\"name\":", 8); jstr_append_raw_json_str(&b, up_ex[i].name);
        jstr_append(&b, ",\"startDate\":", 13); jstr_append_raw_json_str(&b, up_ex[i].start_date);
        jstr_append(&b, ",\"status\":", 10); jstr_append_raw_json_str(&b, up_ex[i].status);
        jstr_append(&b, "}", 1);
    }
    jstr_append(&b, "]}", 2);
    return to_jstring_and_free(env, &b, "{}");
}

/* ── Students CRUD ────────────────────────────────────────────────── */

JNIEXPORT jstring JNICALL
Java_com_apcoaching_schoolcms_nativebridge_NativeBridge_studentGenerateAdmissionNo(
    JNIEnv *env, jobject thiz)
{
    (void)thiz;
    char buf[32] = {0};
    db_lock();
    student_service_generate_admission_no(buf, sizeof(buf));
    db_unlock();
    return (*env)->NewStringUTF(env, buf);
}

JNIEXPORT jstring JNICALL
Java_com_apcoaching_schoolcms_nativebridge_NativeBridge_studentListJson(
    JNIEnv *env, jobject thiz, jstring j_query, jint class_id, jstring j_status)
{
    (void)thiz;
    StudentFilter f;
    memset(&f, 0, sizeof(f));
    copy_jstr(env, j_query, f.search_query, sizeof(f.search_query));
    f.class_id = class_id;
    copy_jstr(env, j_status, f.status, sizeof(f.status));
    f.page = 1;
    f.page_size = STUDENT_MAX_PER_PAGE;

    StudentPage *page = (StudentPage *)calloc(1, sizeof(StudentPage));
    if (!page) return (*env)->NewStringUTF(env, "[]");

    db_lock();
    student_service_get_page(&f, page);
    db_unlock();

    JniStr b;
    jstr_init(&b, 4096);
    jstr_append(&b, "[", 1);
    for (int i = 0; i < page->count; i++) {
        const Student *s = &page->items[i];
        if (i > 0) jstr_append(&b, ",", 1);
        char num[128];
        snprintf(num, sizeof(num), "{\"id\":%d,\"classId\":%d,\"sectionId\":%d,\"rollNumber\":%d,",
                 s->id, s->class_id, s->section_id, s->roll_number);
        jstr_append(&b, num, strlen(num));
        jstr_append(&b, "\"admissionNo\":", 14); jstr_append_raw_json_str(&b, s->admission_no);
        jstr_append(&b, ",\"fullName\":", 12); jstr_append_raw_json_str(&b, s->full_name);
        jstr_append(&b, ",\"fatherName\":", 14); jstr_append_raw_json_str(&b, s->father_name);
        jstr_append(&b, ",\"motherName\":", 14); jstr_append_raw_json_str(&b, s->mother_name);
        jstr_append(&b, ",\"dob\":", 7); jstr_append_raw_json_str(&b, s->dob);
        jstr_append(&b, ",\"gender\":", 10); jstr_append_raw_json_str(&b, s->gender);
        jstr_append(&b, ",\"phone\":", 9); jstr_append_raw_json_str(&b, s->phone);
        jstr_append(&b, ",\"email\":", 9); jstr_append_raw_json_str(&b, s->email);
        jstr_append(&b, ",\"address\":", 11); jstr_append_raw_json_str(&b, s->address);
        jstr_append(&b, ",\"admissionDate\":", 17); jstr_append_raw_json_str(&b, s->admission_date);
        jstr_append(&b, ",\"className\":", 13); jstr_append_raw_json_str(&b, s->class_name);
        jstr_append(&b, ",\"sectionName\":", 15); jstr_append_raw_json_str(&b, s->section_name);
        jstr_append(&b, ",\"status\":", 10); jstr_append_raw_json_str(&b, s->status);
        jstr_append(&b, "}", 1);
    }
    jstr_append(&b, "]", 1);
    free(page);
    return to_jstring_and_free(env, &b, "[]");
}

JNIEXPORT jstring JNICALL
Java_com_apcoaching_schoolcms_nativebridge_NativeBridge_studentSaveJson(
    JNIEnv *env, jobject thiz, jstring j_json)
{
    (void)thiz;
    const char *raw = (*env)->GetStringUTFChars(env, j_json, NULL);
    if (!raw) return make_ok_id_json(env, false, 0, "Invalid JSON");

    Student *st = (Student *)calloc(1, sizeof(Student));
    if (!st) {
        (*env)->ReleaseStringUTFChars(env, j_json, raw);
        return make_ok_id_json(env, false, 0, "Out of memory");
    }

    db_lock();
    sqlite3 *db = db_get_handle();
    st->id = (int)json_get_int(db, raw, "id", 0);
    json_get_str(db, raw, "admissionNo", st->admission_no, sizeof(st->admission_no));
    json_get_str(db, raw, "fullName", st->full_name, sizeof(st->full_name));
    json_get_str(db, raw, "fatherName", st->father_name, sizeof(st->father_name));
    json_get_str(db, raw, "motherName", st->mother_name, sizeof(st->mother_name));
    json_get_str(db, raw, "dob", st->dob, sizeof(st->dob));
    json_get_str(db, raw, "gender", st->gender, sizeof(st->gender));
    json_get_str(db, raw, "category", st->category, sizeof(st->category));
    json_get_str(db, raw, "phone", st->phone, sizeof(st->phone));
    json_get_str(db, raw, "email", st->email, sizeof(st->email));
    json_get_str(db, raw, "address", st->address, sizeof(st->address));
    json_get_str(db, raw, "admissionDate", st->admission_date, sizeof(st->admission_date));
    st->class_id = (int)json_get_int(db, raw, "classId", 0);
    st->section_id = (int)json_get_int(db, raw, "sectionId", 0);
    st->roll_number = (int)json_get_int(db, raw, "rollNumber", 0);
    json_get_str(db, raw, "status", st->status, sizeof(st->status));
    if (st->status[0] == '\0') snprintf(st->status, sizeof(st->status), "ACTIVE");
    if (st->gender[0] == '\0') snprintf(st->gender, sizeof(st->gender), "MALE");

    bool ok = false;
    int out_id = st->id;
    if (st->id > 0) {
        ok = student_service_update(st);
    } else {
        if (st->admission_no[0] == '\0') {
            student_service_generate_admission_no(st->admission_no, sizeof(st->admission_no));
        }
        ok = student_service_create(st, &out_id);
    }
    db_unlock();
    (*env)->ReleaseStringUTFChars(env, j_json, raw);
    free(st);
    return make_ok_id_json(env, ok, out_id, ok ? "" : "Failed saving student (verify fields & admission no)");
}

JNIEXPORT jboolean JNICALL
Java_com_apcoaching_schoolcms_nativebridge_NativeBridge_studentDelete(
    JNIEnv *env, jobject thiz, jint id)
{
    (void)env; (void)thiz;
    db_lock();
    bool ok = student_service_delete(id);
    db_unlock();
    return ok ? JNI_TRUE : JNI_FALSE;
}

/* ── Teachers & Staff CRUD ────────────────────────────────────────── */

JNIEXPORT jstring JNICALL
Java_com_apcoaching_schoolcms_nativebridge_NativeBridge_teacherGenerateCode(JNIEnv *env, jobject thiz)
{
    (void)thiz;
    char buf[32] = {0};
    db_lock();
    teacher_service_generate_code(buf, sizeof(buf));
    db_unlock();
    return (*env)->NewStringUTF(env, buf);
}

JNIEXPORT jstring JNICALL
Java_com_apcoaching_schoolcms_nativebridge_NativeBridge_teacherListJson(
    JNIEnv *env, jobject thiz, jstring j_query)
{
    (void)thiz;
    char q[128];
    copy_jstr(env, j_query, q, sizeof(q));
    Teacher *list = (Teacher *)calloc(TEACHER_MAX_LIST, sizeof(Teacher));
    if (!list) return (*env)->NewStringUTF(env, "[]");
    int count = 0;
    db_lock();
    teacher_service_get_all(q, "", list, TEACHER_MAX_LIST, &count);
    db_unlock();

    JniStr b;
    jstr_init(&b, 2048);
    jstr_append(&b, "[", 1);
    for (int i = 0; i < count; i++) {
        if (i > 0) jstr_append(&b, ",", 1);
        char num[128];
        snprintf(num, sizeof(num), "{\"id\":%d,\"salary\":%.2f,", list[i].id, list[i].salary);
        jstr_append(&b, num, strlen(num));
        jstr_append(&b, "\"teacherCode\":", 14); jstr_append_raw_json_str(&b, list[i].teacher_code);
        jstr_append(&b, ",\"fullName\":", 12); jstr_append_raw_json_str(&b, list[i].full_name);
        jstr_append(&b, ",\"phone\":", 9); jstr_append_raw_json_str(&b, list[i].phone);
        jstr_append(&b, ",\"email\":", 9); jstr_append_raw_json_str(&b, list[i].email);
        jstr_append(&b, ",\"qualification\":", 17); jstr_append_raw_json_str(&b, list[i].qualification);
        jstr_append(&b, ",\"subject\":", 11); jstr_append_raw_json_str(&b, list[i].subject);
        jstr_append(&b, ",\"joiningDate\":", 15); jstr_append_raw_json_str(&b, list[i].joining_date);
        jstr_append(&b, ",\"status\":", 10); jstr_append_raw_json_str(&b, list[i].status);
        jstr_append(&b, "}", 1);
    }
    jstr_append(&b, "]", 1);
    free(list);
    return to_jstring_and_free(env, &b, "[]");
}

JNIEXPORT jstring JNICALL
Java_com_apcoaching_schoolcms_nativebridge_NativeBridge_teacherSaveJson(
    JNIEnv *env, jobject thiz, jstring j_json)
{
    (void)thiz;
    const char *raw = (*env)->GetStringUTFChars(env, j_json, NULL);
    if (!raw) return make_ok_id_json(env, false, 0, "Invalid JSON");
    Teacher *t = (Teacher *)calloc(1, sizeof(Teacher));
    if (!t) {
        (*env)->ReleaseStringUTFChars(env, j_json, raw);
        return make_ok_id_json(env, false, 0, "OOM");
    }

    db_lock();
    sqlite3 *db = db_get_handle();
    t->id = (int)json_get_int(db, raw, "id", 0);
    json_get_str(db, raw, "teacherCode", t->teacher_code, sizeof(t->teacher_code));
    json_get_str(db, raw, "fullName", t->full_name, sizeof(t->full_name));
    json_get_str(db, raw, "phone", t->phone, sizeof(t->phone));
    json_get_str(db, raw, "email", t->email, sizeof(t->email));
    json_get_str(db, raw, "address", t->address, sizeof(t->address));
    json_get_str(db, raw, "qualification", t->qualification, sizeof(t->qualification));
    json_get_str(db, raw, "subject", t->subject, sizeof(t->subject));
    json_get_str(db, raw, "joiningDate", t->joining_date, sizeof(t->joining_date));
    t->salary = json_get_double(db, raw, "salary", 0.0);
    json_get_str(db, raw, "status", t->status, sizeof(t->status));
    if (t->status[0] == '\0') snprintf(t->status, sizeof(t->status), "ACTIVE");

    bool ok = false;
    int out_id = t->id;
    if (t->id > 0) {
        ok = teacher_service_update(t);
    } else {
        if (t->teacher_code[0] == '\0') teacher_service_generate_code(t->teacher_code, sizeof(t->teacher_code));
        ok = teacher_service_create(t, &out_id);
    }
    db_unlock();
    (*env)->ReleaseStringUTFChars(env, j_json, raw);
    free(t);
    return make_ok_id_json(env, ok, out_id, ok ? "" : "Failed saving teacher record");
}

JNIEXPORT jboolean JNICALL
Java_com_apcoaching_schoolcms_nativebridge_NativeBridge_teacherDelete(
    JNIEnv *env, jobject thiz, jint id)
{
    (void)env; (void)thiz;
    db_lock();
    bool ok = teacher_service_delete(id);
    db_unlock();
    return ok ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jstring JNICALL
Java_com_apcoaching_schoolcms_nativebridge_NativeBridge_staffGenerateCode(JNIEnv *env, jobject thiz)
{
    (void)thiz;
    char buf[32] = {0};
    db_lock();
    staff_service_generate_code(buf, sizeof(buf));
    db_unlock();
    return (*env)->NewStringUTF(env, buf);
}

JNIEXPORT jstring JNICALL
Java_com_apcoaching_schoolcms_nativebridge_NativeBridge_staffListJson(
    JNIEnv *env, jobject thiz, jstring j_query)
{
    (void)thiz;
    char q[128];
    copy_jstr(env, j_query, q, sizeof(q));
    Staff *list = (Staff *)calloc(STAFF_MAX_LIST, sizeof(Staff));
    if (!list) return (*env)->NewStringUTF(env, "[]");
    int count = 0;
    db_lock();
    staff_service_get_all(q, "", list, STAFF_MAX_LIST, &count);
    db_unlock();

    JniStr b;
    jstr_init(&b, 2048);
    jstr_append(&b, "[", 1);
    for (int i = 0; i < count; i++) {
        if (i > 0) jstr_append(&b, ",", 1);
        char num[128];
        snprintf(num, sizeof(num), "{\"id\":%d,\"salary\":%.2f,", list[i].id, list[i].salary);
        jstr_append(&b, num, strlen(num));
        jstr_append(&b, "\"staffCode\":", 12); jstr_append_raw_json_str(&b, list[i].staff_code);
        jstr_append(&b, ",\"fullName\":", 12); jstr_append_raw_json_str(&b, list[i].full_name);
        jstr_append(&b, ",\"roleTitle\":", 13); jstr_append_raw_json_str(&b, list[i].role_title);
        jstr_append(&b, ",\"phone\":", 9); jstr_append_raw_json_str(&b, list[i].phone);
        jstr_append(&b, ",\"joiningDate\":", 15); jstr_append_raw_json_str(&b, list[i].joining_date);
        jstr_append(&b, ",\"status\":", 10); jstr_append_raw_json_str(&b, list[i].status);
        jstr_append(&b, "}", 1);
    }
    jstr_append(&b, "]", 1);
    free(list);
    return to_jstring_and_free(env, &b, "[]");
}

JNIEXPORT jstring JNICALL
Java_com_apcoaching_schoolcms_nativebridge_NativeBridge_staffSaveJson(
    JNIEnv *env, jobject thiz, jstring j_json)
{
    (void)thiz;
    const char *raw = (*env)->GetStringUTFChars(env, j_json, NULL);
    if (!raw) return make_ok_id_json(env, false, 0, "Invalid JSON");
    Staff *s = (Staff *)calloc(1, sizeof(Staff));
    if (!s) {
        (*env)->ReleaseStringUTFChars(env, j_json, raw);
        return make_ok_id_json(env, false, 0, "OOM");
    }

    db_lock();
    sqlite3 *db = db_get_handle();
    s->id = (int)json_get_int(db, raw, "id", 0);
    json_get_str(db, raw, "staffCode", s->staff_code, sizeof(s->staff_code));
    json_get_str(db, raw, "fullName", s->full_name, sizeof(s->full_name));
    json_get_str(db, raw, "roleTitle", s->role_title, sizeof(s->role_title));
    json_get_str(db, raw, "phone", s->phone, sizeof(s->phone));
    json_get_str(db, raw, "email", s->email, sizeof(s->email));
    json_get_str(db, raw, "joiningDate", s->joining_date, sizeof(s->joining_date));
    s->salary = json_get_double(db, raw, "salary", 0.0);
    json_get_str(db, raw, "status", s->status, sizeof(s->status));
    if (s->status[0] == '\0') snprintf(s->status, sizeof(s->status), "ACTIVE");
    if (s->role_title[0] == '\0') snprintf(s->role_title, sizeof(s->role_title), "Staff");

    bool ok = false;
    int out_id = s->id;
    if (s->id > 0) {
        ok = staff_service_update(s);
    } else {
        if (s->staff_code[0] == '\0') staff_service_generate_code(s->staff_code, sizeof(s->staff_code));
        ok = staff_service_create(s, &out_id);
    }
    db_unlock();
    (*env)->ReleaseStringUTFChars(env, j_json, raw);
    free(s);
    return make_ok_id_json(env, ok, out_id, ok ? "" : "Failed saving staff record");
}

JNIEXPORT jboolean JNICALL
Java_com_apcoaching_schoolcms_nativebridge_NativeBridge_staffDelete(
    JNIEnv *env, jobject thiz, jint id)
{
    (void)env; (void)thiz;
    db_lock();
    bool ok = staff_service_delete(id);
    db_unlock();
    return ok ? JNI_TRUE : JNI_FALSE;
}

/* ── Classes, Sections, Subjects & Academic Years ─────────────────── */

JNIEXPORT jstring JNICALL
Java_com_apcoaching_schoolcms_nativebridge_NativeBridge_academicYearsJson(JNIEnv *env, jobject thiz)
{
    (void)thiz;
    AcademicYear ays[MAX_ACADEMIC_YEARS];
    memset(ays, 0, sizeof(ays));
    int count = 0;
    db_lock();
    academic_year_get_all(ays, MAX_ACADEMIC_YEARS, &count);
    db_unlock();

    JniStr b;
    jstr_init(&b, 1024);
    jstr_append(&b, "[", 1);
    for (int i = 0; i < count; i++) {
        if (i > 0) jstr_append(&b, ",", 1);
        char num[128];
        snprintf(num, sizeof(num), "{\"id\":%d,\"isCurrent\":%s,",
                 ays[i].id, ays[i].is_current ? "true" : "false");
        jstr_append(&b, num, strlen(num));
        jstr_append(&b, "\"name\":", 7); jstr_append_raw_json_str(&b, ays[i].name);
        jstr_append(&b, ",\"startDate\":", 13); jstr_append_raw_json_str(&b, ays[i].start_date);
        jstr_append(&b, ",\"endDate\":", 11); jstr_append_raw_json_str(&b, ays[i].end_date);
        jstr_append(&b, "}", 1);
    }
    jstr_append(&b, "]", 1);
    return to_jstring_and_free(env, &b, "[]");
}

JNIEXPORT jstring JNICALL
Java_com_apcoaching_schoolcms_nativebridge_NativeBridge_classListJson(JNIEnv *env, jobject thiz)
{
    (void)thiz;
    SchoolClass *cls = (SchoolClass *)calloc(MAX_CLASSES, sizeof(SchoolClass));
    if (!cls) return (*env)->NewStringUTF(env, "[]");
    int count = 0;
    db_lock();
    class_service_get_all(cls, MAX_CLASSES, &count);
    db_unlock();

    JniStr b;
    jstr_init(&b, 2048);
    jstr_append(&b, "[", 1);
    for (int i = 0; i < count; i++) {
        if (i > 0) jstr_append(&b, ",", 1);
        char num[192];
        snprintf(num, sizeof(num),
                 "{\"id\":%d,\"numericValue\":%d,\"capacity\":%d,\"academicYearId\":%d,"
                 "\"sectionCount\":%d,\"studentCount\":%d,",
                 cls[i].id, cls[i].numeric_value, cls[i].capacity, cls[i].academic_year_id,
                 cls[i].section_count, cls[i].student_count);
        jstr_append(&b, num, strlen(num));
        jstr_append(&b, "\"name\":", 7); jstr_append_raw_json_str(&b, cls[i].name);
        jstr_append(&b, ",\"academicYearName\":", 20); jstr_append_raw_json_str(&b, cls[i].academic_year_name);
        jstr_append(&b, "}", 1);
    }
    jstr_append(&b, "]", 1);
    free(cls);
    return to_jstring_and_free(env, &b, "[]");
}

JNIEXPORT jstring JNICALL
Java_com_apcoaching_schoolcms_nativebridge_NativeBridge_classCreateJson(
    JNIEnv *env, jobject thiz, jstring j_json)
{
    (void)thiz;
    const char *raw = (*env)->GetStringUTFChars(env, j_json, NULL);
    if (!raw) return make_ok_id_json(env, false, 0, "Invalid JSON");
    SchoolClass c;
    memset(&c, 0, sizeof(c));

    db_lock();
    sqlite3 *db = db_get_handle();
    json_get_str(db, raw, "name", c.name, sizeof(c.name));
    c.numeric_value = (int)json_get_int(db, raw, "numericValue", 1);
    c.capacity = (int)json_get_int(db, raw, "capacity", 40);
    c.academic_year_id = (int)json_get_int(db, raw, "academicYearId", 1);
    int out_id = 0;
    bool ok = class_service_create(&c, &out_id);
    db_unlock();
    (*env)->ReleaseStringUTFChars(env, j_json, raw);
    return make_ok_id_json(env, ok, out_id, ok ? "" : "Failed creating class");
}

JNIEXPORT jboolean JNICALL
Java_com_apcoaching_schoolcms_nativebridge_NativeBridge_classDelete(JNIEnv *env, jobject thiz, jint id)
{
    (void)env; (void)thiz;
    db_lock();
    bool ok = class_service_delete(id);
    db_unlock();
    return ok ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jstring JNICALL
Java_com_apcoaching_schoolcms_nativebridge_NativeBridge_sectionListJson(
    JNIEnv *env, jobject thiz, jint class_id)
{
    (void)thiz;
    ClassSection *secs = (ClassSection *)calloc(MAX_SECTIONS, sizeof(ClassSection));
    if (!secs) return (*env)->NewStringUTF(env, "[]");
    int count = 0;
    db_lock();
    if (class_id > 0) section_service_get_by_class(class_id, secs, MAX_SECTIONS, &count);
    else section_service_get_all(secs, MAX_SECTIONS, &count);
    db_unlock();

    JniStr b;
    jstr_init(&b, 2048);
    jstr_append(&b, "[", 1);
    for (int i = 0; i < count; i++) {
        if (i > 0) jstr_append(&b, ",", 1);
        char num[128];
        snprintf(num, sizeof(num), "{\"id\":%d,\"classId\":%d,\"studentCount\":%d,",
                 secs[i].id, secs[i].class_id, secs[i].student_count);
        jstr_append(&b, num, strlen(num));
        jstr_append(&b, "\"name\":", 7); jstr_append_raw_json_str(&b, secs[i].name);
        jstr_append(&b, ",\"className\":", 13); jstr_append_raw_json_str(&b, secs[i].class_name);
        jstr_append(&b, ",\"classTeacherName\":", 20); jstr_append_raw_json_str(&b, secs[i].class_teacher_name);
        jstr_append(&b, "}", 1);
    }
    jstr_append(&b, "]", 1);
    free(secs);
    return to_jstring_and_free(env, &b, "[]");
}

JNIEXPORT jstring JNICALL
Java_com_apcoaching_schoolcms_nativebridge_NativeBridge_sectionCreateJson(
    JNIEnv *env, jobject thiz, jstring j_json)
{
    (void)thiz;
    const char *raw = (*env)->GetStringUTFChars(env, j_json, NULL);
    if (!raw) return make_ok_id_json(env, false, 0, "Invalid JSON");
    ClassSection s;
    memset(&s, 0, sizeof(s));
    db_lock();
    sqlite3 *db = db_get_handle();
    s.class_id = (int)json_get_int(db, raw, "classId", 0);
    json_get_str(db, raw, "name", s.name, sizeof(s.name));
    s.class_teacher_id = (int)json_get_int(db, raw, "classTeacherId", 0);
    int out_id = 0;
    bool ok = section_service_create(&s, &out_id);
    db_unlock();
    (*env)->ReleaseStringUTFChars(env, j_json, raw);
    return make_ok_id_json(env, ok, out_id, ok ? "" : "Failed creating section");
}

JNIEXPORT jboolean JNICALL
Java_com_apcoaching_schoolcms_nativebridge_NativeBridge_sectionDelete(JNIEnv *env, jobject thiz, jint id)
{
    (void)env; (void)thiz;
    db_lock();
    bool ok = section_service_delete(id);
    db_unlock();
    return ok ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jstring JNICALL
Java_com_apcoaching_schoolcms_nativebridge_NativeBridge_subjectListJson(
    JNIEnv *env, jobject thiz, jint class_id)
{
    (void)thiz;
    Subject *subs = (Subject *)calloc(MAX_SUBJECTS, sizeof(Subject));
    if (!subs) return (*env)->NewStringUTF(env, "[]");
    int count = 0;
    db_lock();
    if (class_id > 0) subject_service_get_by_class(class_id, subs, MAX_SUBJECTS, &count);
    else subject_service_get_all(subs, MAX_SUBJECTS, &count);
    db_unlock();

    JniStr b;
    jstr_init(&b, 2048);
    jstr_append(&b, "[", 1);
    for (int i = 0; i < count; i++) {
        if (i > 0) jstr_append(&b, ",", 1);
        char num[128];
        snprintf(num, sizeof(num), "{\"id\":%d,\"classId\":%d,\"teacherId\":%d,",
                 subs[i].id, subs[i].class_id, subs[i].teacher_id);
        jstr_append(&b, num, strlen(num));
        jstr_append(&b, "\"code\":", 7); jstr_append_raw_json_str(&b, subs[i].code);
        jstr_append(&b, ",\"name\":", 8); jstr_append_raw_json_str(&b, subs[i].name);
        jstr_append(&b, ",\"className\":", 13); jstr_append_raw_json_str(&b, subs[i].class_name);
        jstr_append(&b, ",\"teacherName\":", 15); jstr_append_raw_json_str(&b, subs[i].teacher_name);
        jstr_append(&b, "}", 1);
    }
    jstr_append(&b, "]", 1);
    free(subs);
    return to_jstring_and_free(env, &b, "[]");
}

JNIEXPORT jstring JNICALL
Java_com_apcoaching_schoolcms_nativebridge_NativeBridge_subjectCreateJson(
    JNIEnv *env, jobject thiz, jstring j_json)
{
    (void)thiz;
    const char *raw = (*env)->GetStringUTFChars(env, j_json, NULL);
    if (!raw) return make_ok_id_json(env, false, 0, "Invalid JSON");
    Subject sb;
    memset(&sb, 0, sizeof(sb));
    db_lock();
    sqlite3 *db = db_get_handle();
    json_get_str(db, raw, "code", sb.code, sizeof(sb.code));
    json_get_str(db, raw, "name", sb.name, sizeof(sb.name));
    sb.class_id = (int)json_get_int(db, raw, "classId", 0);
    sb.teacher_id = (int)json_get_int(db, raw, "teacherId", 0);
    int out_id = 0;
    bool ok = subject_service_create(&sb, &out_id);
    db_unlock();
    (*env)->ReleaseStringUTFChars(env, j_json, raw);
    return make_ok_id_json(env, ok, out_id, ok ? "" : "Failed creating subject");
}

JNIEXPORT jboolean JNICALL
Java_com_apcoaching_schoolcms_nativebridge_NativeBridge_subjectDelete(JNIEnv *env, jobject thiz, jint id)
{
    (void)env; (void)thiz;
    db_lock();
    bool ok = subject_service_delete(id);
    db_unlock();
    return ok ? JNI_TRUE : JNI_FALSE;
}

/* ── Attendance Services ──────────────────────────────────────────── */

JNIEXPORT jstring JNICALL
Java_com_apcoaching_schoolcms_nativebridge_NativeBridge_attendanceRosterJson(
    JNIEnv *env, jobject thiz, jint class_id, jint section_id, jstring j_date)
{
    (void)thiz;
    char date[16];
    copy_jstr(env, j_date, date, sizeof(date));
    AttendanceRecord *recs = (AttendanceRecord *)calloc(ATTENDANCE_MAX_RECORDS, sizeof(AttendanceRecord));
    if (!recs) return (*env)->NewStringUTF(env, "[]");
    int count = 0;
    db_lock();
    attendance_service_get_roster(class_id, section_id, date, recs, ATTENDANCE_MAX_RECORDS, &count);
    db_unlock();

    JniStr b;
    jstr_init(&b, 4096);
    jstr_append(&b, "[", 1);
    for (int i = 0; i < count; i++) {
        if (i > 0) jstr_append(&b, ",", 1);
        char num[128];
        snprintf(num, sizeof(num), "{\"id\":%d,\"studentId\":%d,\"rollNumber\":%d,",
                 recs[i].id, recs[i].student_id, recs[i].roll_number);
        jstr_append(&b, num, strlen(num));
        jstr_append(&b, "\"admissionNo\":", 14); jstr_append_raw_json_str(&b, recs[i].admission_no);
        jstr_append(&b, ",\"studentName\":", 15); jstr_append_raw_json_str(&b, recs[i].student_name);
        jstr_append(&b, ",\"date\":", 8); jstr_append_raw_json_str(&b, recs[i].date);
        jstr_append(&b, ",\"status\":", 10); jstr_append_raw_json_str(&b, recs[i].status);
        jstr_append(&b, ",\"remarks\":", 11); jstr_append_raw_json_str(&b, recs[i].remarks);
        jstr_append(&b, "}", 1);
    }
    jstr_append(&b, "]", 1);
    free(recs);
    return to_jstring_and_free(env, &b, "[]");
}

JNIEXPORT jboolean JNICALL
Java_com_apcoaching_schoolcms_nativebridge_NativeBridge_attendanceMarkBulkJson(
    JNIEnv *env, jobject thiz, jint class_id, jint section_id, jstring j_date,
    jstring j_entries_json, jint recorded_by)
{
    (void)thiz;
    char date[16];
    copy_jstr(env, j_date, date, sizeof(date));
    const char *raw = (*env)->GetStringUTFChars(env, j_entries_json, NULL);
    if (!raw) return JNI_FALSE;

    AttendanceEntry *entries = (AttendanceEntry *)calloc(ATTENDANCE_MAX_RECORDS, sizeof(AttendanceEntry));
    if (!entries) {
        (*env)->ReleaseStringUTFChars(env, j_entries_json, raw);
        return JNI_FALSE;
    }

    int count = 0;
    db_lock();
    sqlite3 *db = db_get_handle();
    if (db) {
        const char *q =
            "SELECT CAST(json_extract(value, '$.studentId') AS INTEGER),"
            " COALESCE(CAST(json_extract(value, '$.status') AS TEXT), 'PRESENT'),"
            " COALESCE(CAST(json_extract(value, '$.remarks') AS TEXT), '') "
            "FROM json_each(?1) ORDER BY key ASC;";
        sqlite3_stmt *stmt = NULL;
        if (sqlite3_prepare_v2(db, q, -1, &stmt, NULL) == SQLITE_OK) {
            sqlite3_bind_text(stmt, 1, raw, -1, SQLITE_STATIC);
            while (count < ATTENDANCE_MAX_RECORDS && sqlite3_step(stmt) == SQLITE_ROW) {
                entries[count].student_id = sqlite3_column_int(stmt, 0);
                const char *st = (const char *)sqlite3_column_text(stmt, 1);
                const char *rm = (const char *)sqlite3_column_text(stmt, 2);
                snprintf(entries[count].status, sizeof(entries[count].status), "%s", st ? st : "PRESENT");
                snprintf(entries[count].remarks, sizeof(entries[count].remarks), "%s", rm ? rm : "");
                count++;
            }
            sqlite3_finalize(stmt);
        }
    }
    bool ok = (count > 0) ? attendance_service_mark_bulk(class_id, section_id, date, entries, count, recorded_by) : false;
    db_unlock();

    (*env)->ReleaseStringUTFChars(env, j_entries_json, raw);
    free(entries);
    return ok ? JNI_TRUE : JNI_FALSE;
}

/* ── Fees, Structures & Ledger ────────────────────────────────────── */

JNIEXPORT jstring JNICALL
Java_com_apcoaching_schoolcms_nativebridge_NativeBridge_feeGenerateReceiptNo(JNIEnv *env, jobject thiz)
{
    (void)thiz;
    char buf[32] = {0};
    db_lock();
    fee_service_generate_receipt_no(buf, sizeof(buf));
    db_unlock();
    return (*env)->NewStringUTF(env, buf);
}

JNIEXPORT jstring JNICALL
Java_com_apcoaching_schoolcms_nativebridge_NativeBridge_feeStructuresJson(
    JNIEnv *env, jobject thiz, jint class_id)
{
    (void)thiz;
    FeeStructure *fs = (FeeStructure *)calloc(MAX_FEE_STRUCTURES, sizeof(FeeStructure));
    if (!fs) return (*env)->NewStringUTF(env, "[]");
    int count = 0;
    db_lock();
    fee_structure_get_all(class_id, 0, fs, MAX_FEE_STRUCTURES, &count);
    db_unlock();

    JniStr b;
    jstr_init(&b, 2048);
    jstr_append(&b, "[", 1);
    for (int i = 0; i < count; i++) {
        if (i > 0) jstr_append(&b, ",", 1);
        char num[128];
        snprintf(num, sizeof(num), "{\"id\":%d,\"classId\":%d,\"academicYearId\":%d,\"amount\":%.2f,",
                 fs[i].id, fs[i].class_id, fs[i].academic_year_id, fs[i].amount);
        jstr_append(&b, num, strlen(num));
        jstr_append(&b, "\"name\":", 7); jstr_append_raw_json_str(&b, fs[i].name);
        jstr_append(&b, ",\"className\":", 13); jstr_append_raw_json_str(&b, fs[i].class_name);
        jstr_append(&b, ",\"feeType\":", 11); jstr_append_raw_json_str(&b, fs[i].fee_type);
        jstr_append(&b, ",\"dueDate\":", 11); jstr_append_raw_json_str(&b, fs[i].due_date);
        jstr_append(&b, "}", 1);
    }
    jstr_append(&b, "]", 1);
    free(fs);
    return to_jstring_and_free(env, &b, "[]");
}

JNIEXPORT jstring JNICALL
Java_com_apcoaching_schoolcms_nativebridge_NativeBridge_feeStructureCreateJson(
    JNIEnv *env, jobject thiz, jstring j_json)
{
    (void)thiz;
    const char *raw = (*env)->GetStringUTFChars(env, j_json, NULL);
    if (!raw) return make_ok_id_json(env, false, 0, "Invalid JSON");
    FeeStructure fs;
    memset(&fs, 0, sizeof(fs));
    db_lock();
    sqlite3 *db = db_get_handle();
    json_get_str(db, raw, "name", fs.name, sizeof(fs.name));
    fs.class_id = (int)json_get_int(db, raw, "classId", 0);
    fs.academic_year_id = (int)json_get_int(db, raw, "academicYearId", 1);
    json_get_str(db, raw, "feeType", fs.fee_type, sizeof(fs.fee_type));
    if (fs.fee_type[0] == '\0') snprintf(fs.fee_type, sizeof(fs.fee_type), "MONTHLY");
    fs.amount = json_get_double(db, raw, "amount", 0.0);
    json_get_str(db, raw, "dueDate", fs.due_date, sizeof(fs.due_date));
    int out_id = 0;
    bool ok = fee_structure_create(&fs, &out_id);
    db_unlock();
    (*env)->ReleaseStringUTFChars(env, j_json, raw);
    return make_ok_id_json(env, ok, out_id, ok ? "" : "Failed creating fee structure");
}

JNIEXPORT jboolean JNICALL
Java_com_apcoaching_schoolcms_nativebridge_NativeBridge_feeStructureDelete(JNIEnv *env, jobject thiz, jint id)
{
    (void)env; (void)thiz;
    db_lock();
    bool ok = fee_structure_delete(id);
    db_unlock();
    return ok ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jstring JNICALL
Java_com_apcoaching_schoolcms_nativebridge_NativeBridge_feePaymentsJson(
    JNIEnv *env, jobject thiz, jstring j_search, jint student_id)
{
    (void)thiz;
    FeeFilter flt;
    memset(&flt, 0, sizeof(flt));
    copy_jstr(env, j_search, flt.search, sizeof(flt.search));
    flt.student_id = student_id;

    FeePayment *pays = (FeePayment *)calloc(MAX_FEE_PAYMENTS, sizeof(FeePayment));
    if (!pays) return (*env)->NewStringUTF(env, "[]");
    int count = 0;
    db_lock();
    fee_payment_get_all(&flt, pays, MAX_FEE_PAYMENTS, &count);
    db_unlock();

    JniStr b;
    jstr_init(&b, 4096);
    jstr_append(&b, "[", 1);
    for (int i = 0; i < count; i++) {
        if (i > 0) jstr_append(&b, ",", 1);
        char num[256];
        snprintf(num, sizeof(num),
                 "{\"id\":%d,\"studentId\":%d,\"rollNumber\":%d,\"feeStructureId\":%d,"
                 "\"amountPaid\":%.2f,\"discount\":%.2f,\"lateFine\":%.2f,\"totalAmount\":%.2f,",
                 pays[i].id, pays[i].student_id, pays[i].roll_number, pays[i].fee_structure_id,
                 pays[i].amount_paid, pays[i].discount, pays[i].late_fine, pays[i].total_amount);
        jstr_append(&b, num, strlen(num));
        jstr_append(&b, "\"receiptNo\":", 12); jstr_append_raw_json_str(&b, pays[i].receipt_no);
        jstr_append(&b, ",\"admissionNo\":", 15); jstr_append_raw_json_str(&b, pays[i].admission_no);
        jstr_append(&b, ",\"studentName\":", 15); jstr_append_raw_json_str(&b, pays[i].student_name);
        jstr_append(&b, ",\"className\":", 13); jstr_append_raw_json_str(&b, pays[i].class_name);
        jstr_append(&b, ",\"sectionName\":", 15); jstr_append_raw_json_str(&b, pays[i].section_name);
        jstr_append(&b, ",\"feeStructureName\":", 20); jstr_append_raw_json_str(&b, pays[i].fee_structure_name);
        jstr_append(&b, ",\"paymentMethod\":", 17); jstr_append_raw_json_str(&b, pays[i].payment_method);
        jstr_append(&b, ",\"transactionRef\":", 18); jstr_append_raw_json_str(&b, pays[i].transaction_ref);
        jstr_append(&b, ",\"paymentDate\":", 15); jstr_append_raw_json_str(&b, pays[i].payment_date);
        jstr_append(&b, ",\"remarks\":", 11); jstr_append_raw_json_str(&b, pays[i].remarks);
        jstr_append(&b, ",\"collectedByName\":", 19); jstr_append_raw_json_str(&b, pays[i].collected_by_name);
        jstr_append(&b, "}", 1);
    }
    jstr_append(&b, "]", 1);
    free(pays);
    return to_jstring_and_free(env, &b, "[]");
}

JNIEXPORT jstring JNICALL
Java_com_apcoaching_schoolcms_nativebridge_NativeBridge_feePaymentCreateJson(
    JNIEnv *env, jobject thiz, jstring j_json)
{
    (void)thiz;
    const char *raw = (*env)->GetStringUTFChars(env, j_json, NULL);
    if (!raw) return make_ok_id_json(env, false, 0, "Invalid JSON");

    FeePayment *pay = (FeePayment *)calloc(1, sizeof(FeePayment));
    if (!pay) {
        (*env)->ReleaseStringUTFChars(env, j_json, raw);
        return make_ok_id_json(env, false, 0, "OOM");
    }

    db_lock();
    sqlite3 *db = db_get_handle();
    json_get_str(db, raw, "receiptNo", pay->receipt_no, sizeof(pay->receipt_no));
    pay->student_id = (int)json_get_int(db, raw, "studentId", 0);
    pay->fee_structure_id = (int)json_get_int(db, raw, "feeStructureId", 0);
    pay->amount_paid = json_get_double(db, raw, "amountPaid", 0.0);
    pay->discount = json_get_double(db, raw, "discount", 0.0);
    pay->late_fine = json_get_double(db, raw, "lateFine", 0.0);
    pay->total_amount = pay->amount_paid - pay->discount + pay->late_fine;
    json_get_str(db, raw, "paymentMethod", pay->payment_method, sizeof(pay->payment_method));
    if (pay->payment_method[0] == '\0') snprintf(pay->payment_method, sizeof(pay->payment_method), "CASH");
    json_get_str(db, raw, "transactionRef", pay->transaction_ref, sizeof(pay->transaction_ref));
    json_get_str(db, raw, "paymentDate", pay->payment_date, sizeof(pay->payment_date));
    json_get_str(db, raw, "remarks", pay->remarks, sizeof(pay->remarks));
    pay->collected_by = (int)json_get_int(db, raw, "collectedBy", 1);

    int out_id = 0;
    bool ok = fee_payment_create(pay, &out_id);
    char rcp[32];
    snprintf(rcp, sizeof(rcp), "%s", pay->receipt_no);
    db_unlock();

    (*env)->ReleaseStringUTFChars(env, j_json, raw);
    free(pay);

    JniStr b;
    jstr_init(&b, 256);
    char head[64];
    snprintf(head, sizeof(head), "{\"ok\":%s,\"id\":%d,\"receiptNo\":", ok ? "true" : "false", out_id);
    jstr_append(&b, head, strlen(head));
    jstr_append_raw_json_str(&b, rcp);
    jstr_append(&b, ",\"error\":", 9);
    jstr_append_raw_json_str(&b, ok ? "" : "Fee payment failed (verify student & structure)");
    jstr_append(&b, "}", 1);
    return to_jstring_and_free(env, &b, "{\"ok\":false}");
}

JNIEXPORT jstring JNICALL
Java_com_apcoaching_schoolcms_nativebridge_NativeBridge_feePendingDuesJson(
    JNIEnv *env, jobject thiz, jint class_id)
{
    (void)thiz;
    StudentFeeSummary *list = (StudentFeeSummary *)calloc(100, sizeof(StudentFeeSummary));
    if (!list) return (*env)->NewStringUTF(env, "[]");
    int count = 0;
    db_lock();
    fee_service_get_pending_dues(class_id, list, 100, &count);
    db_unlock();

    JniStr b;
    jstr_init(&b, 2048);
    jstr_append(&b, "[", 1);
    for (int i = 0; i < count; i++) {
        if (i > 0) jstr_append(&b, ",", 1);
        char num[256];
        snprintf(num, sizeof(num),
                 "{\"studentId\":%d,\"classId\":%d,\"rollNumber\":%d,\"totalAssigned\":%.2f,"
                 "\"totalPaid\":%.2f,\"balanceDue\":%.2f,\"paymentCount\":%d,",
                 list[i].student_id, list[i].class_id, list[i].roll_number,
                 list[i].total_assigned, list[i].total_paid, list[i].balance_due, list[i].payment_count);
        jstr_append(&b, num, strlen(num));
        jstr_append(&b, "\"admissionNo\":", 14); jstr_append_raw_json_str(&b, list[i].admission_no);
        jstr_append(&b, ",\"studentName\":", 15); jstr_append_raw_json_str(&b, list[i].student_name);
        jstr_append(&b, ",\"className\":", 13); jstr_append_raw_json_str(&b, list[i].class_name);
        jstr_append(&b, ",\"sectionName\":", 15); jstr_append_raw_json_str(&b, list[i].section_name);
        jstr_append(&b, "}", 1);
    }
    jstr_append(&b, "]", 1);
    free(list);
    return to_jstring_and_free(env, &b, "[]");
}

/* ── Examinations, Marks & Report Cards ───────────────────────────── */

JNIEXPORT jstring JNICALL
Java_com_apcoaching_schoolcms_nativebridge_NativeBridge_examsListJson(JNIEnv *env, jobject thiz)
{
    (void)thiz;
    Examination *list = (Examination *)calloc(MAX_EXAMINATIONS, sizeof(Examination));
    if (!list) return (*env)->NewStringUTF(env, "[]");
    int count = 0;
    db_lock();
    exam_service_get_all(list, MAX_EXAMINATIONS, &count);
    db_unlock();

    JniStr b;
    jstr_init(&b, 2048);
    jstr_append(&b, "[", 1);
    for (int i = 0; i < count; i++) {
        if (i > 0) jstr_append(&b, ",", 1);
        char num[96];
        snprintf(num, sizeof(num), "{\"id\":%d,\"academicYearId\":%d,", list[i].id, list[i].academic_year_id);
        jstr_append(&b, num, strlen(num));
        jstr_append(&b, "\"name\":", 7); jstr_append_raw_json_str(&b, list[i].name);
        jstr_append(&b, ",\"startDate\":", 13); jstr_append_raw_json_str(&b, list[i].start_date);
        jstr_append(&b, ",\"endDate\":", 11); jstr_append_raw_json_str(&b, list[i].end_date);
        jstr_append(&b, ",\"gradingSystem\":", 17); jstr_append_raw_json_str(&b, list[i].grading_system);
        jstr_append(&b, ",\"status\":", 10); jstr_append_raw_json_str(&b, list[i].status);
        jstr_append(&b, "}", 1);
    }
    jstr_append(&b, "]", 1);
    free(list);
    return to_jstring_and_free(env, &b, "[]");
}

JNIEXPORT jstring JNICALL
Java_com_apcoaching_schoolcms_nativebridge_NativeBridge_examCreateJson(
    JNIEnv *env, jobject thiz, jstring j_json)
{
    (void)thiz;
    const char *raw = (*env)->GetStringUTFChars(env, j_json, NULL);
    if (!raw) return make_ok_id_json(env, false, 0, "Invalid JSON");
    Examination ex;
    memset(&ex, 0, sizeof(ex));
    db_lock();
    sqlite3 *db = db_get_handle();
    json_get_str(db, raw, "name", ex.name, sizeof(ex.name));
    ex.academic_year_id = (int)json_get_int(db, raw, "academicYearId", 1);
    json_get_str(db, raw, "startDate", ex.start_date, sizeof(ex.start_date));
    json_get_str(db, raw, "endDate", ex.end_date, sizeof(ex.end_date));
    json_get_str(db, raw, "gradingSystem", ex.grading_system, sizeof(ex.grading_system));
    json_get_str(db, raw, "status", ex.status, sizeof(ex.status));
    if (ex.grading_system[0] == '\0') snprintf(ex.grading_system, sizeof(ex.grading_system), "PERCENTAGE");
    if (ex.status[0] == '\0') snprintf(ex.status, sizeof(ex.status), "PLANNED");
    int out_id = 0;
    bool ok = exam_service_create(&ex, &out_id);
    db_unlock();
    (*env)->ReleaseStringUTFChars(env, j_json, raw);
    return make_ok_id_json(env, ok, out_id, ok ? "" : "Failed creating exam");
}

JNIEXPORT jboolean JNICALL
Java_com_apcoaching_schoolcms_nativebridge_NativeBridge_examDelete(JNIEnv *env, jobject thiz, jint id)
{
    (void)env; (void)thiz;
    db_lock();
    bool ok = exam_service_delete(id);
    db_unlock();
    return ok ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jstring JNICALL
Java_com_apcoaching_schoolcms_nativebridge_NativeBridge_examSubjectsJson(
    JNIEnv *env, jobject thiz, jint exam_id)
{
    (void)thiz;
    ExamSubject *list = (ExamSubject *)calloc(MAX_EXAM_SUBJECTS, sizeof(ExamSubject));
    if (!list) return (*env)->NewStringUTF(env, "[]");
    int count = 0;
    db_lock();
    exam_subject_get_by_exam(exam_id, list, MAX_EXAM_SUBJECTS, &count);
    db_unlock();

    JniStr b;
    jstr_init(&b, 2048);
    jstr_append(&b, "[", 1);
    for (int i = 0; i < count; i++) {
        if (i > 0) jstr_append(&b, ",", 1);
        char num[160];
        snprintf(num, sizeof(num),
                 "{\"id\":%d,\"examId\":%d,\"subjectId\":%d,\"classId\":%d,"
                 "\"maxMarks\":%.1f,\"passingMarks\":%.1f,",
                 list[i].id, list[i].exam_id, list[i].subject_id, list[i].class_id,
                 list[i].max_marks, list[i].passing_marks);
        jstr_append(&b, num, strlen(num));
        jstr_append(&b, "\"subjectCode\":", 14); jstr_append_raw_json_str(&b, list[i].subject_code);
        jstr_append(&b, ",\"subjectName\":", 15); jstr_append_raw_json_str(&b, list[i].subject_name);
        jstr_append(&b, ",\"className\":", 13); jstr_append_raw_json_str(&b, list[i].class_name);
        jstr_append(&b, ",\"examDate\":", 12); jstr_append_raw_json_str(&b, list[i].exam_date);
        jstr_append(&b, "}", 1);
    }
    jstr_append(&b, "]", 1);
    free(list);
    return to_jstring_and_free(env, &b, "[]");
}

JNIEXPORT jstring JNICALL
Java_com_apcoaching_schoolcms_nativebridge_NativeBridge_examSubjectCreateJson(
    JNIEnv *env, jobject thiz, jstring j_json)
{
    (void)thiz;
    const char *raw = (*env)->GetStringUTFChars(env, j_json, NULL);
    if (!raw) return make_ok_id_json(env, false, 0, "Invalid JSON");
    ExamSubject es;
    memset(&es, 0, sizeof(es));
    db_lock();
    sqlite3 *db = db_get_handle();
    es.exam_id = (int)json_get_int(db, raw, "examId", 0);
    es.subject_id = (int)json_get_int(db, raw, "subjectId", 0);
    json_get_str(db, raw, "examDate", es.exam_date, sizeof(es.exam_date));
    es.max_marks = json_get_double(db, raw, "maxMarks", 100.0);
    es.passing_marks = json_get_double(db, raw, "passingMarks", 35.0);
    int out_id = 0;
    bool ok = exam_subject_create(&es, &out_id);
    db_unlock();
    (*env)->ReleaseStringUTFChars(env, j_json, raw);
    return make_ok_id_json(env, ok, out_id, ok ? "" : "Failed adding exam subject");
}

JNIEXPORT jstring JNICALL
Java_com_apcoaching_schoolcms_nativebridge_NativeBridge_examResultsSheetJson(
    JNIEnv *env, jobject thiz, jint exam_subject_id)
{
    (void)thiz;
    ExamResult *list = (ExamResult *)calloc(MAX_CLASS_RANKS, sizeof(ExamResult));
    if (!list) return (*env)->NewStringUTF(env, "[]");
    int count = 0;
    db_lock();
    exam_result_get_class_sheet(exam_subject_id, list, MAX_CLASS_RANKS, &count);
    db_unlock();

    JniStr b;
    jstr_init(&b, 4096);
    jstr_append(&b, "[", 1);
    for (int i = 0; i < count; i++) {
        if (i > 0) jstr_append(&b, ",", 1);
        char num[192];
        snprintf(num, sizeof(num),
                 "{\"id\":%d,\"examSubjectId\":%d,\"studentId\":%d,\"rollNumber\":%d,"
                 "\"marksObtained\":%.1f,\"maxMarks\":%.1f,\"passingMarks\":%.1f,\"isPass\":%s,",
                 list[i].id, list[i].exam_subject_id, list[i].student_id, list[i].roll_number,
                 list[i].marks_obtained, list[i].max_marks, list[i].passing_marks,
                 list[i].is_pass ? "true" : "false");
        jstr_append(&b, num, strlen(num));
        jstr_append(&b, "\"admissionNo\":", 14); jstr_append_raw_json_str(&b, list[i].admission_no);
        jstr_append(&b, ",\"studentName\":", 15); jstr_append_raw_json_str(&b, list[i].student_name);
        jstr_append(&b, ",\"grade\":", 9); jstr_append_raw_json_str(&b, list[i].grade);
        jstr_append(&b, ",\"remarks\":", 11); jstr_append_raw_json_str(&b, list[i].remarks);
        jstr_append(&b, "}", 1);
    }
    jstr_append(&b, "]", 1);
    free(list);
    return to_jstring_and_free(env, &b, "[]");
}

JNIEXPORT jboolean JNICALL
Java_com_apcoaching_schoolcms_nativebridge_NativeBridge_examResultRecordJson(
    JNIEnv *env, jobject thiz, jstring j_json)
{
    (void)thiz;
    const char *raw = (*env)->GetStringUTFChars(env, j_json, NULL);
    if (!raw) return JNI_FALSE;
    ExamResult r;
    memset(&r, 0, sizeof(r));
    db_lock();
    sqlite3 *db = db_get_handle();
    r.exam_subject_id = (int)json_get_int(db, raw, "examSubjectId", 0);
    r.student_id = (int)json_get_int(db, raw, "studentId", 0);
    r.marks_obtained = json_get_double(db, raw, "marksObtained", 0.0);
    json_get_str(db, raw, "remarks", r.remarks, sizeof(r.remarks));
    int out_id = 0;
    bool ok = exam_result_record(&r, &out_id);
    db_unlock();
    (*env)->ReleaseStringUTFChars(env, j_json, raw);
    return ok ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jstring JNICALL
Java_com_apcoaching_schoolcms_nativebridge_NativeBridge_examClassRanksJson(
    JNIEnv *env, jobject thiz, jint exam_id, jint class_id)
{
    (void)thiz;
    ClassExamRank *ranks = (ClassExamRank *)calloc(MAX_CLASS_RANKS, sizeof(ClassExamRank));
    if (!ranks) return (*env)->NewStringUTF(env, "[]");
    int count = 0;
    db_lock();
    exam_service_get_class_ranks(exam_id, class_id, ranks, MAX_CLASS_RANKS, &count);
    db_unlock();

    JniStr b;
    jstr_init(&b, 2048);
    jstr_append(&b, "[", 1);
    for (int i = 0; i < count; i++) {
        if (i > 0) jstr_append(&b, ",", 1);
        char num[192];
        snprintf(num, sizeof(num),
                 "{\"rank\":%d,\"studentId\":%d,\"rollNumber\":%d,\"totalObtained\":%.1f,"
                 "\"totalMax\":%.1f,\"percentage\":%.2f,\"isPass\":%s,",
                 ranks[i].rank, ranks[i].student_id, ranks[i].roll_number,
                 ranks[i].total_obtained, ranks[i].total_max, ranks[i].percentage,
                 ranks[i].is_pass ? "true" : "false");
        jstr_append(&b, num, strlen(num));
        jstr_append(&b, "\"admissionNo\":", 14); jstr_append_raw_json_str(&b, ranks[i].admission_no);
        jstr_append(&b, ",\"studentName\":", 15); jstr_append_raw_json_str(&b, ranks[i].student_name);
        jstr_append(&b, ",\"overallGrade\":", 16); jstr_append_raw_json_str(&b, ranks[i].overall_grade);
        jstr_append(&b, "}", 1);
    }
    jstr_append(&b, "]", 1);
    free(ranks);
    return to_jstring_and_free(env, &b, "[]");
}

JNIEXPORT jstring JNICALL
Java_com_apcoaching_schoolcms_nativebridge_NativeBridge_examReportCardJson(
    JNIEnv *env, jobject thiz, jint exam_id, jint student_id)
{
    (void)thiz;
    StudentReportCard *card = (StudentReportCard *)calloc(1, sizeof(StudentReportCard));
    if (!card) return (*env)->NewStringUTF(env, "{\"ok\":false}");

    db_lock();
    bool ok = exam_service_generate_report_card(exam_id, student_id, card);
    db_unlock();

    if (!ok) {
        free(card);
        return (*env)->NewStringUTF(env, "{\"ok\":false}");
    }

    JniStr b;
    jstr_init(&b, 2048);
    char hdr[256];
    snprintf(hdr, sizeof(hdr),
             "{\"ok\":true,\"studentId\":%d,\"rollNumber\":%d,\"totalObtained\":%.1f,"
             "\"totalMax\":%.1f,\"percentage\":%.2f,\"isPass\":%s,\"classRank\":%d,",
             card->student_id, card->roll_number, card->total_marks_obtained,
             card->total_max_marks, card->percentage, card->is_pass ? "true" : "false", card->class_rank);
    jstr_append(&b, hdr, strlen(hdr));
    jstr_append(&b, "\"studentName\":", 14); jstr_append_raw_json_str(&b, card->student_name);
    jstr_append(&b, ",\"admissionNo\":", 15); jstr_append_raw_json_str(&b, card->admission_no);
    jstr_append(&b, ",\"className\":", 13); jstr_append_raw_json_str(&b, card->class_name);
    jstr_append(&b, ",\"sectionName\":", 15); jstr_append_raw_json_str(&b, card->section_name);
    jstr_append(&b, ",\"examName\":", 12); jstr_append_raw_json_str(&b, card->exam_name);
    jstr_append(&b, ",\"overallGrade\":", 16); jstr_append_raw_json_str(&b, card->overall_grade);
    jstr_append(&b, ",\"scores\":[", 11);
    for (int i = 0; i < card->score_count; i++) {
        if (i > 0) jstr_append(&b, ",", 1);
        char snum[128];
        snprintf(snum, sizeof(snum),
                 "{\"maxMarks\":%.1f,\"passingMarks\":%.1f,\"marksObtained\":%.1f,\"isPass\":%s,",
                 card->scores[i].max_marks, card->scores[i].passing_marks,
                 card->scores[i].marks_obtained, card->scores[i].is_pass ? "true" : "false");
        jstr_append(&b, snum, strlen(snum));
        jstr_append(&b, "\"code\":", 7); jstr_append_raw_json_str(&b, card->scores[i].code);
        jstr_append(&b, ",\"subjectName\":", 15); jstr_append_raw_json_str(&b, card->scores[i].subject_name);
        jstr_append(&b, ",\"grade\":", 9); jstr_append_raw_json_str(&b, card->scores[i].grade);
        jstr_append(&b, "}", 1);
    }
    jstr_append(&b, "]}", 2);
    free(card);
    return to_jstring_and_free(env, &b, "{\"ok\":false}");
}

/* ── Timetable Services ───────────────────────────────────────────── */

JNIEXPORT jstring JNICALL
Java_com_apcoaching_schoolcms_nativebridge_NativeBridge_timetableByClassJson(
    JNIEnv *env, jobject thiz, jint class_id, jint section_id, jstring j_day)
{
    (void)thiz;
    char day[16];
    copy_jstr(env, j_day, day, sizeof(day));
    TimetableSlot *slots = (TimetableSlot *)calloc(MAX_TIMETABLE_SLOTS, sizeof(TimetableSlot));
    if (!slots) return (*env)->NewStringUTF(env, "[]");
    int count = 0;
    db_lock();
    timetable_service_get_by_class(class_id, section_id, day, slots, MAX_TIMETABLE_SLOTS, &count);
    db_unlock();

    JniStr b;
    jstr_init(&b, 2048);
    jstr_append(&b, "[", 1);
    for (int i = 0; i < count; i++) {
        if (i > 0) jstr_append(&b, ",", 1);
        char num[160];
        snprintf(num, sizeof(num),
                 "{\"id\":%d,\"classId\":%d,\"sectionId\":%d,\"periodNumber\":%d,\"subjectId\":%d,\"teacherId\":%d,",
                 slots[i].id, slots[i].class_id, slots[i].section_id,
                 slots[i].period_number, slots[i].subject_id, slots[i].teacher_id);
        jstr_append(&b, num, strlen(num));
        jstr_append(&b, "\"dayOfWeek\":", 12); jstr_append_raw_json_str(&b, slots[i].day_of_week);
        jstr_append(&b, ",\"startTime\":", 13); jstr_append_raw_json_str(&b, slots[i].start_time);
        jstr_append(&b, ",\"endTime\":", 11); jstr_append_raw_json_str(&b, slots[i].end_time);
        jstr_append(&b, ",\"subjectName\":", 15); jstr_append_raw_json_str(&b, slots[i].subject_name);
        jstr_append(&b, ",\"teacherName\":", 15); jstr_append_raw_json_str(&b, slots[i].teacher_name);
        jstr_append(&b, ",\"roomNumber\":", 14); jstr_append_raw_json_str(&b, slots[i].room_number);
        jstr_append(&b, "}", 1);
    }
    jstr_append(&b, "]", 1);
    free(slots);
    return to_jstring_and_free(env, &b, "[]");
}

JNIEXPORT jstring JNICALL
Java_com_apcoaching_schoolcms_nativebridge_NativeBridge_timetableSaveSlotJson(
    JNIEnv *env, jobject thiz, jstring j_json)
{
    (void)thiz;
    const char *raw = (*env)->GetStringUTFChars(env, j_json, NULL);
    if (!raw) return make_ok_id_json(env, false, 0, "Invalid JSON");
    TimetableSlot sl;
    memset(&sl, 0, sizeof(sl));
    ConflictResult conf;
    memset(&conf, 0, sizeof(conf));

    db_lock();
    sqlite3 *db = db_get_handle();
    sl.id = (int)json_get_int(db, raw, "id", 0);
    sl.class_id = (int)json_get_int(db, raw, "classId", 0);
    sl.section_id = (int)json_get_int(db, raw, "sectionId", 0);
    json_get_str(db, raw, "dayOfWeek", sl.day_of_week, sizeof(sl.day_of_week));
    sl.period_number = (int)json_get_int(db, raw, "periodNumber", 1);
    json_get_str(db, raw, "startTime", sl.start_time, sizeof(sl.start_time));
    json_get_str(db, raw, "endTime", sl.end_time, sizeof(sl.end_time));
    sl.subject_id = (int)json_get_int(db, raw, "subjectId", 0);
    sl.teacher_id = (int)json_get_int(db, raw, "teacherId", 0);
    json_get_str(db, raw, "roomNumber", sl.room_number, sizeof(sl.room_number));
    int out_id = sl.id;
    bool ok = timetable_service_save_slot(&sl, &out_id, &conf);
    db_unlock();
    (*env)->ReleaseStringUTFChars(env, j_json, raw);
    return make_ok_id_json(env, ok, out_id, ok ? "" : (conf.message[0] ? conf.message : "Slot conflict"));
}

JNIEXPORT jboolean JNICALL
Java_com_apcoaching_schoolcms_nativebridge_NativeBridge_timetableDelete(JNIEnv *env, jobject thiz, jint id)
{
    (void)env; (void)thiz;
    db_lock();
    bool ok = timetable_service_delete(id);
    db_unlock();
    return ok ? JNI_TRUE : JNI_FALSE;
}

/* ── Reports Service (Heap Allocated!) ────────────────────────────── */

JNIEXPORT jstring JNICALL
Java_com_apcoaching_schoolcms_nativebridge_NativeBridge_reportGenerateJson(
    JNIEnv *env, jobject thiz, jint type_idx, jint class_id, jstring j_from, jstring j_to)
{
    (void)thiz;
    ReportFilter flt;
    memset(&flt, 0, sizeof(flt));
    flt.type = (type_idx >= 0 && type_idx < REPORT_TYPE_COUNT) ? (ReportType)type_idx : REPORT_STUDENT_LIST;
    flt.class_id = class_id;
    copy_jstr(env, j_from, flt.date_from, sizeof(flt.date_from));
    copy_jstr(env, j_to, flt.date_to, sizeof(flt.date_to));

    ReportData *rep = (ReportData *)calloc(1, sizeof(ReportData));
    if (!rep) return (*env)->NewStringUTF(env, "{\"ok\":false}");

    db_lock();
    bool ok = report_service_generate(&flt, rep);
    db_unlock();

    if (!ok) {
        free(rep);
        return (*env)->NewStringUTF(env, "{\"ok\":false}");
    }

    JniStr b;
    jstr_init(&b, 8192);
    char hdr[96];
    snprintf(hdr, sizeof(hdr), "{\"ok\":true,\"columnCount\":%d,\"rowCount\":%d,\"totalAmount\":%.2f,",
             rep->column_count, rep->row_count, rep->total_amount);
    jstr_append(&b, hdr, strlen(hdr));
    jstr_append(&b, "\"title\":", 8); jstr_append_raw_json_str(&b, rep->title);
    jstr_append(&b, ",\"summaryText\":", 15); jstr_append_raw_json_str(&b, rep->summary_text);
    jstr_append(&b, ",\"headers\":[", 12);
    for (int c = 0; c < rep->column_count; c++) {
        if (c > 0) jstr_append(&b, ",", 1);
        jstr_append_raw_json_str(&b, rep->header[c]);
    }
    jstr_append(&b, "],\"rows\":[", 9);
    for (int r = 0; r < rep->row_count; r++) {
        if (r > 0) jstr_append(&b, ",", 1);
        jstr_append(&b, "[", 1);
        for (int c = 0; c < rep->column_count; c++) {
            if (c > 0) jstr_append(&b, ",", 1);
            jstr_append_raw_json_str(&b, rep->rows[r][c]);
        }
        jstr_append(&b, "]", 1);
    }
    jstr_append(&b, "]}", 2);
    free(rep);
    return to_jstring_and_free(env, &b, "{\"ok\":false}");
}

/* ── Settings & Activity Logs ─────────────────────────────────────── */

JNIEXPORT jstring JNICALL
Java_com_apcoaching_schoolcms_nativebridge_NativeBridge_settingsGetJson(JNIEnv *env, jobject thiz)
{
    (void)thiz;
    SchoolSettings s;
    memset(&s, 0, sizeof(s));
    db_lock();
    settings_service_get(&s);
    db_unlock();

    JniStr b;
    jstr_init(&b, 1024);
    jstr_append(&b, "{", 1);
    jstr_append(&b, "\"schoolName\":", 13); jstr_append_raw_json_str(&b, s.school_name);
    jstr_append(&b, ",\"schoolAddress\":", 17); jstr_append_raw_json_str(&b, s.school_address);
    jstr_append(&b, ",\"schoolPhone\":", 15); jstr_append_raw_json_str(&b, s.school_phone);
    jstr_append(&b, ",\"schoolEmail\":", 15); jstr_append_raw_json_str(&b, s.school_email);
    jstr_append(&b, ",\"schoolWebsite\":", 17); jstr_append_raw_json_str(&b, s.school_website);
    jstr_append(&b, ",\"principalName\":", 17); jstr_append_raw_json_str(&b, s.principal_name);
    jstr_append(&b, ",\"currencySymbol\":", 18); jstr_append_raw_json_str(&b, s.currency_symbol);
    jstr_append(&b, ",\"themeMode\":", 13); jstr_append_raw_json_str(&b, s.theme_mode);
    jstr_append(&b, ",\"academicYearName\":", 20); jstr_append_raw_json_str(&b, s.current_academic_year_name);
    jstr_append(&b, "}", 1);
    return to_jstring_and_free(env, &b, "{}");
}

JNIEXPORT jboolean JNICALL
Java_com_apcoaching_schoolcms_nativebridge_NativeBridge_settingsSaveJson(
    JNIEnv *env, jobject thiz, jstring j_json)
{
    (void)thiz;
    const char *raw = (*env)->GetStringUTFChars(env, j_json, NULL);
    if (!raw) return JNI_FALSE;
    SchoolSettings s;
    memset(&s, 0, sizeof(s));
    db_lock();
    settings_service_get(&s);
    sqlite3 *db = db_get_handle();
    json_get_str(db, raw, "schoolName", s.school_name, sizeof(s.school_name));
    json_get_str(db, raw, "schoolAddress", s.school_address, sizeof(s.school_address));
    json_get_str(db, raw, "schoolPhone", s.school_phone, sizeof(s.school_phone));
    json_get_str(db, raw, "schoolEmail", s.school_email, sizeof(s.school_email));
    json_get_str(db, raw, "schoolWebsite", s.school_website, sizeof(s.school_website));
    json_get_str(db, raw, "principalName", s.principal_name, sizeof(s.principal_name));
    json_get_str(db, raw, "currencySymbol", s.currency_symbol, sizeof(s.currency_symbol));
    bool ok = settings_service_save(&s);
    db_unlock();
    (*env)->ReleaseStringUTFChars(env, j_json, raw);
    return ok ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jstring JNICALL
Java_com_apcoaching_schoolcms_nativebridge_NativeBridge_settingsLogsJson(
    JNIEnv *env, jobject thiz, jint limit)
{
    (void)thiz;
    int max_it = (limit > 0 && limit <= 100) ? limit : 25;
    AuditLogEntry *logs = (AuditLogEntry *)calloc((size_t)max_it, sizeof(AuditLogEntry));
    if (!logs) return (*env)->NewStringUTF(env, "[]");
    int count = 0;
    db_lock();
    settings_service_get_recent_logs(logs, max_it, &count);
    db_unlock();

    JniStr b;
    jstr_init(&b, 2048);
    jstr_append(&b, "[", 1);
    for (int i = 0; i < count; i++) {
        if (i > 0) jstr_append(&b, ",", 1);
        jstr_append(&b, "{\"action\":", 10); jstr_append_raw_json_str(&b, logs[i].action);
        jstr_append(&b, ",\"module\":", 10); jstr_append_raw_json_str(&b, logs[i].module);
        jstr_append(&b, ",\"details\":", 11); jstr_append_raw_json_str(&b, logs[i].details);
        jstr_append(&b, ",\"createdAt\":", 13); jstr_append_raw_json_str(&b, logs[i].created_at);
        jstr_append(&b, "}", 1);
    }
    jstr_append(&b, "]", 1);
    free(logs);
    return to_jstring_and_free(env, &b, "[]");
}

/* ── Multi-Device Schema V3 Sync Bridges ──────────────────────────── */

JNIEXPORT jstring JNICALL
Java_com_apcoaching_schoolcms_nativebridge_NativeBridge_syncGetStatusJson(JNIEnv *env, jobject thiz)
{
    (void)thiz;
    char dev_id[SYNC_DEVICE_ID_LEN] = {0};
    char dev_prefix[8] = {0};
    char seq_buf[32] = "0";
    char url[256] = {0};
    char token[256] = {0};
    char hmac[256] = {0};
    char en_buf[16] = "0";
    char last_sync[64] = {0};

    db_lock();
    sync_core_get_device_id(dev_id, sizeof(dev_id));
    sync_core_get_device_prefix(dev_prefix, sizeof(dev_prefix));
    sync_core_get_state("last_server_seq", seq_buf, sizeof(seq_buf));
    sync_core_get_state("cpanel_url", url, sizeof(url));
    sync_core_get_state("device_token", token, sizeof(token));
    sync_core_get_state("hmac_secret", hmac, sizeof(hmac));
    sync_core_get_state("sync_enabled", en_buf, sizeof(en_buf));
    sync_core_get_state("last_sync_at", last_sync, sizeof(last_sync));
    int pending = sync_core_get_pending_outbox_count();
    db_unlock();

    long long seq = strtoll(seq_buf, NULL, 10);

    JniStr b;
    jstr_init(&b, 1024);
    char num[128];
    snprintf(num, sizeof(num), "{\"lastServerSeq\":%lld,\"pendingOutbox\":%d,\"syncEnabled\":%s,",
             seq, pending >= 0 ? pending : 0, strcmp(en_buf, "1") == 0 ? "true" : "false");
    jstr_append(&b, num, strlen(num));
    jstr_append(&b, "\"deviceId\":", 11); jstr_append_raw_json_str(&b, dev_id);
    jstr_append(&b, ",\"devicePrefix\":", 16); jstr_append_raw_json_str(&b, dev_prefix);
    jstr_append(&b, ",\"cpanelUrl\":", 13); jstr_append_raw_json_str(&b, url);
    jstr_append(&b, ",\"deviceToken\":", 15); jstr_append_raw_json_str(&b, token);
    jstr_append(&b, ",\"hmacSecret\":", 14); jstr_append_raw_json_str(&b, hmac);
    jstr_append(&b, ",\"lastSyncAt\":", 14); jstr_append_raw_json_str(&b, last_sync);
    jstr_append(&b, "}", 1);
    return to_jstring_and_free(env, &b, "{}");
}

JNIEXPORT jboolean JNICALL
Java_com_apcoaching_schoolcms_nativebridge_NativeBridge_syncSaveConfig(
    JNIEnv *env, jobject thiz, jstring j_url, jstring j_token, jstring j_hmac, jboolean j_enabled)
{
    (void)thiz;
    char url[256], tok[256], hmac[256];
    copy_jstr(env, j_url, url, sizeof(url));
    copy_jstr(env, j_token, tok, sizeof(tok));
    copy_jstr(env, j_hmac, hmac, sizeof(hmac));

    /* Trim trailing slashes cleanly */
    size_t n = strlen(url);
    while (n > 0 && url[n - 1] == '/') url[--n] = '\0';
    if (n >= 4 && strcmp(url + n - 4, "/api") == 0) url[n - 4] = '\0';

    db_lock();
    bool ok = sync_core_set_state("cpanel_url", url) &&
              sync_core_set_state("device_token", tok) &&
              sync_core_set_state("hmac_secret", hmac) &&
              sync_core_set_state("sync_enabled", j_enabled == JNI_TRUE ? "1" : "0");
    db_unlock();
    return ok ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jint JNICALL
Java_com_apcoaching_schoolcms_nativebridge_NativeBridge_syncGetPendingCount(JNIEnv *env, jobject thiz)
{
    (void)env; (void)thiz;
    db_lock();
    int cnt = sync_core_get_pending_outbox_count();
    db_unlock();
    return cnt;
}

JNIEXPORT jstring JNICALL
Java_com_apcoaching_schoolcms_nativebridge_NativeBridge_syncFetchPushBundleJson(
    JNIEnv *env, jobject thiz, jint limit)
{
    (void)thiz;
    int max_it = (limit > 0 && limit <= SYNC_MAX_BATCH) ? limit : SYNC_MAX_BATCH;
    SyncOutboxItem *items = (SyncOutboxItem *)calloc((size_t)max_it, sizeof(SyncOutboxItem));
    if (!items) return (*env)->NewStringUTF(env, "{\"count\":0,\"maxOutboxId\":0}");

    int count = 0;
    char *push_json = NULL;

    db_lock();
    bool ok_fetch = sync_core_fetch_pending_outbox(items, max_it, &count);
    long long max_id = 0;
    if (ok_fetch && count > 0) {
        for (int i = 0; i < count; i++) {
            if (items[i].id > max_id) max_id = items[i].id;
        }
        sync_core_build_push_payload_json(items, count, &push_json);
    }
    sync_core_free_outbox_items(items, count);
    db_unlock();
    free(items);

    JniStr b;
    jstr_init(&b, push_json ? (strlen(push_json) + 128) : 128);
    char num[96];
    snprintf(num, sizeof(num), "{\"count\":%d,\"maxOutboxId\":%lld,\"pushJson\":", count, max_id);
    jstr_append(&b, num, strlen(num));
    if (push_json) {
        jstr_append(&b, push_json, strlen(push_json));
        free(push_json);
    } else {
        jstr_append(&b, "null", 4);
    }
    jstr_append(&b, "}", 1);
    return to_jstring_and_free(env, &b, "{\"count\":0,\"maxOutboxId\":0}");
}

JNIEXPORT jboolean JNICALL
Java_com_apcoaching_schoolcms_nativebridge_NativeBridge_syncAckOutboxIdsJson(
    JNIEnv *env, jobject thiz, jstring j_ids_json, jlong fallback_max_id)
{
    (void)thiz;
    const char *raw = (*env)->GetStringUTFChars(env, j_ids_json, NULL);
    bool ok = false;

    db_lock();
    sqlite3 *db = db_get_handle();
    if (db && raw && raw[0] != '\0') {
        const char *sql =
            "UPDATE sync_outbox SET synced = 1 "
            "WHERE synced = 0 AND id IN (SELECT CAST(value AS INTEGER) FROM json_each(?1));";
        sqlite3_stmt *stmt = NULL;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
            sqlite3_bind_text(stmt, 1, raw, -1, SQLITE_STATIC);
            if (sqlite3_step(stmt) == SQLITE_DONE) {
                ok = true;
            }
            sqlite3_finalize(stmt);
        }
    }
    if (!ok && fallback_max_id > 0) {
        ok = sync_core_mark_outbox_synced((long long)fallback_max_id);
    }
    db_unlock();

    if (raw) (*env)->ReleaseStringUTFChars(env, j_ids_json, raw);
    return ok ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jstring JNICALL
Java_com_apcoaching_schoolcms_nativebridge_NativeBridge_syncApplyRemoteBatchJson(
    JNIEnv *env, jobject thiz, jstring j_changes_array_json, jlong server_latest_seq)
{
    (void)thiz;
    const char *raw = (*env)->GetStringUTFChars(env, j_changes_array_json, NULL);
    if (!raw) return (*env)->NewStringUTF(env, "{\"ok\":false,\"applied\":0}");

    SyncRemoteChange *changes = (SyncRemoteChange *)calloc(SYNC_MAX_BATCH, sizeof(SyncRemoteChange));
    char **owned_payloads = (char **)calloc(SYNC_MAX_BATCH, sizeof(char *));
    int item_count = 0;

    SyncApplyResult apply_res;
    memset(&apply_res, 0, sizeof(apply_res));

    db_lock();
    sqlite3 *db = db_get_handle();
    if (db && changes && owned_payloads) {
        const char *sql =
            "SELECT"
            " COALESCE(CAST(json_extract(value, '$.server_seq') AS INTEGER), 0),"
            " COALESCE(CAST(json_extract(value, '$.table_name') AS TEXT), ''),"
            " COALESCE(CAST(json_extract(value, '$.record_uuid') AS TEXT), ''),"
            " COALESCE(CAST(json_extract(value, '$.operation') AS TEXT), 'UPSERT'),"
            " COALESCE(CAST(json_extract(value, '$.origin_device_id') AS TEXT), ''),"
            " COALESCE(CAST(json_extract(value, '$.updated_at_ms') AS INTEGER), 0),"
            " COALESCE(json_extract(value, '$.payload'), '{}') "
            "FROM json_each(?1) ORDER BY key ASC;";
        sqlite3_stmt *stmt = NULL;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
            sqlite3_bind_text(stmt, 1, raw, -1, SQLITE_STATIC);
            while (item_count < SYNC_MAX_BATCH && sqlite3_step(stmt) == SQLITE_ROW) {
                SyncRemoteChange *rc = &changes[item_count];
                rc->server_seq = sqlite3_column_int64(stmt, 0);
                const char *tbl  = (const char *)sqlite3_column_text(stmt, 1);
                const char *uuid = (const char *)sqlite3_column_text(stmt, 2);
                const char *op   = (const char *)sqlite3_column_text(stmt, 3);
                const char *dev  = (const char *)sqlite3_column_text(stmt, 4);
                rc->updated_at_ms = sqlite3_column_int64(stmt, 5);
                const char *pl   = (const char *)sqlite3_column_text(stmt, 6);

                snprintf(rc->table_name, sizeof(rc->table_name), "%s", tbl ? tbl : "");
                snprintf(rc->record_uuid, sizeof(rc->record_uuid), "%s", uuid ? uuid : "");
                snprintf(rc->operation, sizeof(rc->operation), "%s", op ? op : "UPSERT");
                snprintf(rc->origin_device_id, sizeof(rc->origin_device_id), "%s", dev ? dev : "");

                if (!pl) pl = "{}";
                size_t plen = strlen(pl);
                owned_payloads[item_count] = (char *)malloc(plen + 1);
                if (owned_payloads[item_count]) {
                    memcpy(owned_payloads[item_count], pl, plen + 1);
                    rc->payload_json = owned_payloads[item_count];
                } else {
                    rc->payload_json = "{}";
                }
                item_count++;
            }
            sqlite3_finalize(stmt);
        }
    }

    bool ok = true;
    if (item_count > 0) {
        ok = sync_core_apply_remote_batch(changes, item_count, &apply_res);
    }
    if (apply_res.error_count == 0 && (long long)server_latest_seq > apply_res.last_server_seq) {
        char seq_str[32];
        snprintf(seq_str, sizeof(seq_str), "%lld", (long long)server_latest_seq);
        sync_core_set_state("last_server_seq", seq_str);
        apply_res.last_server_seq = (long long)server_latest_seq;
    }
    db_unlock();

    if (owned_payloads) {
        for (int i = 0; i < item_count; i++) free(owned_payloads[i]);
        free(owned_payloads);
    }
    if (changes) free(changes);
    (*env)->ReleaseStringUTFChars(env, j_changes_array_json, raw);

    char out[192];
    snprintf(out, sizeof(out),
             "{\"ok\":%s,\"applied\":%d,\"skipped\":%d,\"errors\":%d,\"lastServerSeq\":%lld}",
             ok ? "true" : "false",
             apply_res.applied_count,
             apply_res.skipped_conflict_count,
             apply_res.error_count,
             apply_res.last_server_seq);
    return (*env)->NewStringUTF(env, out);
}

JNIEXPORT jboolean JNICALL
Java_com_apcoaching_schoolcms_nativebridge_NativeBridge_syncSetLastServerSeq(
    JNIEnv *env, jobject thiz, jlong seq)
{
    (void)env; (void)thiz;
    char buf[32];
    snprintf(buf, sizeof(buf), "%lld", (long long)seq);
    db_lock();
    bool ok = sync_core_set_state("last_server_seq", buf);
    db_unlock();
    return ok ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_com_apcoaching_schoolcms_nativebridge_NativeBridge_syncTouchLastSyncAt(
    JNIEnv *env, jobject thiz, jstring j_ts)
{
    (void)thiz;
    char buf[64];
    copy_jstr(env, j_ts, buf, sizeof(buf));
    db_lock();
    bool ok = sync_core_set_state("last_sync_at", buf);
    db_unlock();
    return ok ? JNI_TRUE : JNI_FALSE;
}
