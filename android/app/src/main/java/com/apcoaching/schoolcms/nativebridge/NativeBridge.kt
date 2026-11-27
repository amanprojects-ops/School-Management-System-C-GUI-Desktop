package com.apcoaching.schoolcms.nativebridge

/**
 * Thread-safe JNI bridge to the shared C11 library (libschoolcms_core.so).
 * All CRUD, RBAC permissions, reporting, fee ledger math, and SQLite Schema V3
 * delta sync operations execute inside the exact same C11 core used on Windows Desktop.
 */
object NativeBridge {
    init {
        System.loadLibrary("schoolcms_core")
    }

    external fun initDatabase(dbPath: String, appFilesDir: String): Boolean
    external fun closeDatabase()
    external fun getDbVersion(): Int

    external fun authLogin(username: String, password: String): String
    external fun authLogout()
    external fun authCanAccess(moduleName: String, requireEdit: Boolean): Boolean
    external fun authChangePassword(userId: Int, oldPassword: String, newPassword: String): String
    external fun authGetUsersJson(): String
    external fun authCreateUserJson(json: String): String
    external fun authToggleUserStatus(userId: Int): Boolean
    external fun authGetRolePermissionsJson(roleId: Int): String
    external fun authSetRolePermission(roleId: Int, moduleName: String, canView: Boolean, canEdit: Boolean): Boolean

    external fun getDashboardJson(): String

    external fun studentGenerateAdmissionNo(): String
    external fun studentListJson(query: String, classId: Int, status: String): String
    external fun studentSaveJson(json: String): String
    external fun studentDelete(id: Int): Boolean

    external fun teacherGenerateCode(): String
    external fun teacherListJson(query: String): String
    external fun teacherSaveJson(json: String): String
    external fun teacherDelete(id: Int): Boolean

    external fun staffGenerateCode(): String
    external fun staffListJson(query: String): String
    external fun staffSaveJson(json: String): String
    external fun staffDelete(id: Int): Boolean

    external fun academicYearsJson(): String
    external fun classListJson(): String
    external fun classCreateJson(json: String): String
    external fun classDelete(id: Int): Boolean
    external fun sectionListJson(classId: Int): String
    external fun sectionCreateJson(json: String): String
    external fun sectionDelete(id: Int): Boolean
    external fun subjectListJson(classId: Int): String
    external fun subjectCreateJson(json: String): String
    external fun subjectDelete(id: Int): Boolean

    external fun attendanceRosterJson(classId: Int, sectionId: Int, date: String): String
    external fun attendanceMarkBulkJson(
        classId: Int,
        sectionId: Int,
        date: String,
        entriesJson: String,
        recordedBy: Int
    ): Boolean

    external fun feeGenerateReceiptNo(): String
    external fun feeStructuresJson(classId: Int): String
    external fun feeStructureCreateJson(json: String): String
    external fun feeStructureDelete(id: Int): Boolean
    external fun feePaymentsJson(search: String, studentId: Int): String
    external fun feePaymentCreateJson(json: String): String
    external fun feePendingDuesJson(classId: Int): String

    external fun examsListJson(): String
    external fun examCreateJson(json: String): String
    external fun examDelete(id: Int): Boolean
    external fun examSubjectsJson(examId: Int): String
    external fun examSubjectCreateJson(json: String): String
    external fun examResultsSheetJson(examSubjectId: Int): String
    external fun examResultRecordJson(json: String): Boolean
    external fun examClassRanksJson(examId: Int, classId: Int): String
    external fun examReportCardJson(examId: Int, studentId: Int): String

    external fun timetableByClassJson(classId: Int, sectionId: Int, dayOfWeek: String): String
    external fun timetableSaveSlotJson(json: String): String
    external fun timetableDelete(id: Int): Boolean

    external fun reportGenerateJson(typeIdx: Int, classId: Int, dateFrom: String, dateTo: String): String

    external fun settingsGetJson(): String
    external fun settingsSaveJson(json: String): Boolean
    external fun settingsLogsJson(limit: Int): String

    external fun syncGetStatusJson(): String
    external fun syncSaveConfig(cpanelUrl: String, deviceToken: String, hmacSecret: String, enabled: Boolean): Boolean
    external fun syncGetPendingCount(): Int
    external fun syncFetchPushBundleJson(limit: Int): String
    external fun syncAckOutboxIdsJson(idsJson: String, fallbackMaxId: Long): Boolean
    external fun syncApplyRemoteBatchJson(changesArrayJson: String, serverLatestSeq: Long): String
    external fun syncSetLastServerSeq(seq: Long): Boolean
    external fun syncTouchLastSyncAt(ts: String): Boolean
}
