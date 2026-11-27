package com.apcoaching.schoolcms.sync

import org.json.JSONArray
import org.json.JSONObject
import java.io.BufferedReader
import java.io.InputStream
import java.io.InputStreamReader
import java.io.OutputStreamWriter
import java.net.HttpURLConnection
import java.net.URL
import javax.crypto.Mac
import javax.crypto.spec.SecretKeySpec

data class SyncProbeResult(
    val ok: Boolean,
    val statusCode: Int,
    val message: String,
    val latestSeq: Long = 0L,
    val schemaVer: Int = 0
)

data class SyncPushHttpResult(
    val ok: Boolean,
    val statusCode: Int,
    val acceptedCount: Int = 0,
    val skippedCount: Int = 0,
    val errorCount: Int = 0,
    val latestSeq: Long = 0L,
    val acknowledgedIdsJson: String = "[]",
    val errorMessage: String = ""
)

data class SyncPullHttpResult(
    val ok: Boolean,
    val statusCode: Int,
    val nextSince: Long = 0L,
    val hasMore: Boolean = false,
    val changesArrayJson: String = "[]",
    val changeCount: Int = 0,
    val errorMessage: String = ""
)

/**
 * Pure JVM HTTPS client talking directly to SchoolCMS cpanel_backend/ PHP REST APIs.
 * Compatible with both Bearer Token and optional HMAC-SHA256 dual-factor authentication.
 */
object CpanelSyncClient {

    fun normalizeBaseUrl(rawUrl: String): String {
        var s = rawUrl.trim()
        while (s.endsWith("/")) {
            s = s.dropLast(1)
        }
        if (s.endsWith("/api")) {
            s = s.dropLast(4)
        }
        return s
    }

    fun computeHmacSha256(secret: String, timestampSec: Long, payloadOrQuery: String): String {
        val mac = Mac.getInstance("HmacSHA256")
        mac.init(SecretKeySpec(secret.toByteArray(Charsets.UTF_8), "HmacSHA256"))
        val signed = "$timestampSec\n$payloadOrQuery".toByteArray(Charsets.UTF_8)
        return mac.doFinal(signed).joinToString("") { "%02x".format(it) }
    }

    private fun readStreamText(stream: InputStream?): String {
        if (stream == null) return ""
        return BufferedReader(InputStreamReader(stream, Charsets.UTF_8)).use { it.readText() }
    }

    private fun openRequest(
        fullUrl: String,
        method: String,
        token: String,
        deviceId: String,
        hmacSecret: String,
        signedData: String
    ): HttpURLConnection {
        val conn = (URL(fullUrl).openConnection() as HttpURLConnection).apply {
            requestMethod = method
            connectTimeout = 8_000
            readTimeout = 15_000
            useCaches = false
            setRequestProperty("Accept", "application/json")
            setRequestProperty("Authorization", "Bearer $token")
            if (deviceId.isNotBlank()) {
                setRequestProperty("X-Device-Id", deviceId)
            }
            if (hmacSecret.isNotBlank()) {
                val ts = System.currentTimeMillis() / 1000L
                val sig = computeHmacSha256(hmacSecret, ts, signedData)
                setRequestProperty("X-Sync-Timestamp", ts.toString())
                setRequestProperty("X-Sync-Signature", sig)
            }
            if (method == "POST") {
                doOutput = true
                setRequestProperty("Content-Type", "application/json; charset=utf-8")
            }
        }
        return conn
    }

    fun testHealth(
        rawBaseUrl: String,
        token: String,
        hmacSecret: String = "",
        deviceId: String = "DEV-android"
    ): SyncProbeResult {
        val base = normalizeBaseUrl(rawBaseUrl)
        if (base.isBlank() || token.isBlank()) {
            return SyncProbeResult(false, 0, "Enter both cPanel API URL and Device Token.")
        }

        return try {
            val conn = openRequest("$base/api/health.php", "GET", token, deviceId, hmacSecret, "")
            val status = conn.responseCode
            val body = readStreamText(if (status in 200..299) conn.inputStream else conn.errorStream)
            conn.disconnect()

            if (status != 200 || body.isBlank()) {
                return SyncProbeResult(false, status, "Server returned HTTP $status. Check URL / Token / HMAC.")
            }

            val obj = JSONObject(body)
            val ok = obj.optBoolean("ok", false)
            val ver = obj.optInt("schema_ver", 0)
            val seq = obj.optLong("latest_seq", 0L)
            val driver = obj.optString("db_driver", "mysql")
            if (ok) {
                SyncProbeResult(true, 200, "Connected ($driver, Schema V$ver, Seq #$seq)", seq, ver)
            } else {
                SyncProbeResult(false, status, obj.optString("error", "Invalid server response"))
            }
        } catch (e: Exception) {
            SyncProbeResult(false, 0, "Network error: ${e.message ?: "Unreachable"}")
        }
    }

    fun pushBatch(
        rawBaseUrl: String,
        token: String,
        hmacSecret: String,
        deviceId: String,
        pushBodyJson: String
    ): SyncPushHttpResult {
        val base = normalizeBaseUrl(rawBaseUrl)
        if (base.isBlank() || token.isBlank()) {
            return SyncPushHttpResult(false, 0, errorMessage = "Sync URL or Token missing")
        }

        return try {
            val conn = openRequest("$base/api/push.php", "POST", token, deviceId, hmacSecret, pushBodyJson)
            OutputStreamWriter(conn.outputStream, Charsets.UTF_8).use { it.write(pushBodyJson) }
            val status = conn.responseCode
            val body = readStreamText(if (status in 200..299) conn.inputStream else conn.errorStream)
            conn.disconnect()

            if (status != 200 || body.isBlank()) {
                return SyncPushHttpResult(false, status, errorMessage = "Push failed (HTTP $status)")
            }

            val obj = JSONObject(body)
            val ok = obj.optBoolean("ok", false)
            if (!ok) {
                return SyncPushHttpResult(false, status, errorMessage = obj.optString("error", "Push rejected"))
            }

            val ackArr = obj.optJSONArray("acknowledged_ids") ?: JSONArray()
            SyncPushHttpResult(
                ok = true,
                statusCode = 200,
                acceptedCount = obj.optInt("accepted_count", 0),
                skippedCount = obj.optInt("skipped_count", 0),
                errorCount = obj.optInt("error_count", 0),
                latestSeq = obj.optLong("latest_seq", 0L),
                acknowledgedIdsJson = ackArr.toString()
            )
        } catch (e: Exception) {
            SyncPushHttpResult(false, 0, errorMessage = "Push network error: ${e.message ?: "Offline"}")
        }
    }

    fun pullBatch(
        rawBaseUrl: String,
        token: String,
        hmacSecret: String,
        deviceId: String,
        sinceSeq: Long,
        limit: Int = 100
    ): SyncPullHttpResult {
        val base = normalizeBaseUrl(rawBaseUrl)
        if (base.isBlank() || token.isBlank()) {
            return SyncPullHttpResult(false, 0, errorMessage = "Sync URL or Token missing")
        }

        val query = "since=$sinceSeq&device_id=$deviceId&limit=$limit"
        return try {
            val conn = openRequest("$base/api/pull.php?$query", "GET", token, deviceId, hmacSecret, query)
            val status = conn.responseCode
            val hdrLastSeq = conn.getHeaderField("X-Sync-Last-Seq")?.toLongOrNull() ?: sinceSeq
            val hdrHasMore = conn.getHeaderField("X-Sync-Has-More") == "1"

            if (status == 204) {
                conn.disconnect()
                return SyncPullHttpResult(
                    ok = true,
                    statusCode = 204,
                    nextSince = maxOf(sinceSeq, hdrLastSeq),
                    hasMore = hdrHasMore,
                    changesArrayJson = "[]",
                    changeCount = 0
                )
            }

            val body = readStreamText(if (status in 200..299) conn.inputStream else conn.errorStream)
            conn.disconnect()

            if (status != 200 || body.isBlank()) {
                return SyncPullHttpResult(false, status, errorMessage = "Pull failed (HTTP $status)")
            }

            val obj = JSONObject(body)
            if (!obj.optBoolean("ok", false)) {
                return SyncPullHttpResult(false, status, errorMessage = obj.optString("error", "Pull rejected"))
            }

            val arr = obj.optJSONArray("changes") ?: JSONArray()
            SyncPullHttpResult(
                ok = true,
                statusCode = 200,
                nextSince = obj.optLong("next_since", sinceSeq),
                hasMore = obj.optBoolean("has_more", false),
                changesArrayJson = arr.toString(),
                changeCount = arr.length()
            )
        } catch (e: Exception) {
            SyncPullHttpResult(false, 0, errorMessage = "Pull network error: ${e.message ?: "Offline"}")
        }
    }
}
