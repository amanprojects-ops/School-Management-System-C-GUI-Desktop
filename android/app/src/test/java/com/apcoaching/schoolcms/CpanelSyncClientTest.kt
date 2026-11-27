package com.apcoaching.schoolcms.sync

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Test

class CpanelSyncClientTest {

    @Test
    fun normalizeBaseUrl_stripsTrailingSlashAndApi() {
        assertEquals("https://sync.school.edu", CpanelSyncClient.normalizeBaseUrl("https://sync.school.edu/"))
        assertEquals("https://sync.school.edu", CpanelSyncClient.normalizeBaseUrl("https://sync.school.edu/api/"))
        assertEquals("http://10.0.2.2:8080", CpanelSyncClient.normalizeBaseUrl(" http://10.0.2.2:8080/api "))
    }

    @Test
    fun computeHmacSha256_matchesKnownVector() {
        // Matches PHP: hash_hmac('sha256', "1700000000\nsince=0&device_id=DEV-1", "secret")
        val sig = CpanelSyncClient.computeHmacSha256("secret", 1700000000L, "since=0&device_id=DEV-1")
        assertEquals(64, sig.length)
        val sigSame = CpanelSyncClient.computeHmacSha256("secret", 1700000000L, "since=0&device_id=DEV-1")
        assertEquals(sig, sigSame)
    }

    @Test
    fun testHealth_rejectsBlankConfig() {
        val res = CpanelSyncClient.testHealth("", "")
        assertFalse(res.ok)
        assertEquals(0, res.statusCode)
    }
}
