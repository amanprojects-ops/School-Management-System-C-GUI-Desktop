package com.apcoaching.schoolcms.sync

import android.content.Context
import androidx.work.CoroutineWorker
import androidx.work.WorkerParameters

/**
 * Periodic Android WorkManager worker keeping local SQLite synced with cPanel
 * even when the UI is closed or backgrounded.
 */
class SyncWorker(
    appContext: Context,
    params: WorkerParameters
) : CoroutineWorker(appContext, params) {

    override suspend fun doWork(): Result {
        val outcome = SyncManager.runSyncCycle(requireEnabled = true)
        return if (outcome.ok || !outcome.message.contains("network", ignoreCase = true)) {
            Result.success()
        } else {
            Result.retry()
        }
    }
}
