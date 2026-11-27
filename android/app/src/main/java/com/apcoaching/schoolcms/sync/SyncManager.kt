package com.apcoaching.schoolcms.sync

import android.content.Context
import androidx.work.Constraints
import androidx.work.ExistingPeriodicWorkPolicy
import androidx.work.ExistingWorkPolicy
import androidx.work.NetworkType
import androidx.work.OneTimeWorkRequestBuilder
import androidx.work.PeriodicWorkRequestBuilder
import androidx.work.WorkManager
import com.apcoaching.schoolcms.nativebridge.NativeBridge
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.MutableSharedFlow
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.SharedFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asSharedFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch
import kotlinx.coroutines.sync.Mutex
import kotlinx.coroutines.sync.withLock
import org.json.JSONObject
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale
import java.util.concurrent.TimeUnit

data class SyncUiState(
    val enabled: Boolean = false,
    val isSyncing: Boolean = false,
    val isError: Boolean = false,
    val deviceId: String = "",
    val devicePrefix: String = "M",
    val cpanelUrl: String = "",
    val deviceToken: String = "",
    val hmacSecret: String = "",
    val lastServerSeq: Long = 0L,
    val pendingOutbox: Int = 0,
    val lastSyncAt: String = "",
    val message: String = "Configure cPanel Sync in Settings"
)

data class SyncCycleOutcome(
    val ok: Boolean,
    val pushed: Int,
    val pulled: Int,
    val message: String
)

/**
 * Coordinates automatic ~1s debounced push, 20s in-app periodic pull, and Android WorkManager
 * background delta sync without spawning extra OS daemons.
 */
object SyncManager {
    private const val PERIODIC_WORK_NAME = "schoolcms_cpanel_periodic_sync"
    private const val ONESHOT_WORK_NAME = "schoolcms_cpanel_immediate_sync"

    private val scope = CoroutineScope(SupervisorJob() + Dispatchers.IO)
    private val syncMutex = Mutex()
    private var foregroundLoopJob: Job? = null
    private var debouncePushJob: Job? = null

    private val _status = MutableStateFlow(SyncUiState())
    val status: StateFlow<SyncUiState> = _status.asStateFlow()

    private val _remoteUpdates = MutableSharedFlow<Int>(extraBufferCapacity = 8)
    val remoteUpdates: SharedFlow<Int> = _remoteUpdates.asSharedFlow()

    fun refreshLocalStatus(overrideMessage: String? = null, isError: Boolean = false, isSyncing: Boolean = false) {
        try {
            val obj = JSONObject(NativeBridge.syncGetStatusJson())
            val enabled = obj.optBoolean("syncEnabled", false)
            val lastSync = obj.optString("lastSyncAt", "")
            val pending = obj.optInt("pendingOutbox", 0)
            val seq = obj.optLong("lastServerSeq", 0L)
            val defaultMsg = when {
                !enabled -> "Disabled (configure cPanel URL & Token)"
                lastSync.isNotBlank() -> "Synced ($lastSync | Seq #$seq | Outbox: $pending)"
                else -> "Sync enabled (Ready)"
            }
            _status.value = SyncUiState(
                enabled = enabled,
                isSyncing = isSyncing,
                isError = isError,
                deviceId = obj.optString("deviceId", "DEV-android"),
                devicePrefix = obj.optString("devicePrefix", "M"),
                cpanelUrl = obj.optString("cpanelUrl", ""),
                deviceToken = obj.optString("deviceToken", ""),
                hmacSecret = obj.optString("hmacSecret", ""),
                lastServerSeq = seq,
                pendingOutbox = pending,
                lastSyncAt = lastSync,
                message = overrideMessage ?: defaultMsg
            )
        } catch (_: Throwable) {
        }
    }

    fun startForegroundLoop(appContext: Context) {
        schedulePeriodicWork(appContext)
        refreshLocalStatus()
        if (foregroundLoopJob?.isActive == true) return
        foregroundLoopJob = scope.launch {
            delay(1_200L)
            while (isActive) {
                val cfg = _status.value
                if (cfg.enabled && cfg.cpanelUrl.isNotBlank() && cfg.deviceToken.isNotBlank()) {
                    runSyncCycle(requireEnabled = true)
                }
                delay(20_000L)
            }
        }
    }

    fun notifyLocalMutation(appContext: Context? = null) {
        refreshLocalStatus()
        debouncePushJob?.cancel()
        debouncePushJob = scope.launch {
            delay(900L)
            val st = _status.value
            if (st.enabled && st.cpanelUrl.isNotBlank() && st.deviceToken.isNotBlank()) {
                val out = runSyncCycle(requireEnabled = true)
                if (!out.ok && appContext != null) {
                    enqueueOneShotSync(appContext)
                }
            }
        }
    }

    fun schedulePeriodicWork(context: Context) {
        val constraints = Constraints.Builder()
            .setRequiredNetworkType(NetworkType.CONNECTED)
            .build()
        val req = PeriodicWorkRequestBuilder<SyncWorker>(15, TimeUnit.MINUTES)
            .setConstraints(constraints)
            .build()
        WorkManager.getInstance(context).enqueueUniquePeriodicWork(
            PERIODIC_WORK_NAME,
            ExistingPeriodicWorkPolicy.KEEP,
            req
        )
    }

    private fun enqueueOneShotSync(context: Context) {
        val constraints = Constraints.Builder()
            .setRequiredNetworkType(NetworkType.CONNECTED)
            .build()
        val req = OneTimeWorkRequestBuilder<SyncWorker>()
            .setConstraints(constraints)
            .build()
        WorkManager.getInstance(context).enqueueUniqueWork(
            ONESHOT_WORK_NAME,
            ExistingWorkPolicy.REPLACE,
            req
        )
    }

    suspend fun runSyncCycle(requireEnabled: Boolean = false): SyncCycleOutcome = syncMutex.withLock {
        val stObj = JSONObject(NativeBridge.syncGetStatusJson())
        val enabled = stObj.optBoolean("syncEnabled", false)
        val url = stObj.optString("cpanelUrl", "")
        val token = stObj.optString("deviceToken", "")
        val hmac = stObj.optString("hmacSecret", "")
        val deviceId = stObj.optString("deviceId", "DEV-android")

        if ((requireEnabled && !enabled) || url.isBlank() || token.isBlank()) {
            val msg = if (!enabled) "Cloud Sync is disabled." else "Configure cPanel API URL and Token."
            refreshLocalStatus(msg, isError = false, isSyncing = false)
            return@withLock SyncCycleOutcome(false, 0, 0, msg)
        }

        refreshLocalStatus("Syncing deltas with cPanel...", isError = false, isSyncing = true)

        var totalPushed = 0
        var totalPulled = 0

        // 1. Push pending outbox batches
        while (true) {
            val bundle = JSONObject(NativeBridge.syncFetchPushBundleJson(100))
            val cnt = bundle.optInt("count", 0)
            val maxId = bundle.optLong("maxOutboxId", 0L)
            if (cnt <= 0 || bundle.isNull("pushJson")) break

            val pushJsonStr = bundle.getJSONObject("pushJson").toString()
            val pushRes = CpanelSyncClient.pushBatch(url, token, hmac, deviceId, pushJsonStr)
            if (!pushRes.ok) {
                refreshLocalStatus(pushRes.errorMessage, isError = true, isSyncing = false)
                return@withLock SyncCycleOutcome(false, totalPushed, totalPulled, pushRes.errorMessage)
            }

            NativeBridge.syncAckOutboxIdsJson(pushRes.acknowledgedIdsJson, if (pushRes.errorCount == 0) maxId else 0L)
            totalPushed += pushRes.acceptedCount + pushRes.skippedCount
            if (pushRes.errorCount > 0 || cnt < 100) break
        }

        // 2. Pull remote deltas from cPanel
        var pages = 0
        while (pages < 20) {
            pages++
            val currentSeq = JSONObject(NativeBridge.syncGetStatusJson()).optLong("lastServerSeq", 0L)
            val pullRes = CpanelSyncClient.pullBatch(url, token, hmac, deviceId, currentSeq, 100)
            if (!pullRes.ok) {
                refreshLocalStatus(pullRes.errorMessage, isError = true, isSyncing = false)
                return@withLock SyncCycleOutcome(false, totalPushed, totalPulled, pullRes.errorMessage)
            }

            if (pullRes.statusCode == 204 || pullRes.changeCount == 0) {
                if (pullRes.nextSince > currentSeq) {
                    NativeBridge.syncSetLastServerSeq(pullRes.nextSince)
                }
                if (!pullRes.hasMore) break
                continue
            }

            val applyObj = JSONObject(NativeBridge.syncApplyRemoteBatchJson(pullRes.changesArrayJson, pullRes.nextSince))
            val applied = applyObj.optInt("applied", 0)
            totalPulled += applied
            if (!pullRes.hasMore) break
        }

        val nowStr = SimpleDateFormat("yyyy-MM-dd HH:mm:ss", Locale.US).format(Date())
        NativeBridge.syncTouchLastSyncAt(nowStr)
        val okMsg = "Synced ($totalPushed pushed, $totalPulled pulled at $nowStr)"
        refreshLocalStatus(okMsg, isError = false, isSyncing = false)

        if (totalPulled > 0) {
            _remoteUpdates.tryEmit(totalPulled)
        }
        return@withLock SyncCycleOutcome(true, totalPushed, totalPulled, okMsg)
    }
}
