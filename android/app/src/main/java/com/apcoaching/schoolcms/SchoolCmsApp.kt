package com.apcoaching.schoolcms

import android.app.Application
import com.apcoaching.schoolcms.nativebridge.NativeBridge
import com.apcoaching.schoolcms.sync.SyncManager
import java.io.File

class SchoolCmsApp : Application() {
    override fun onCreate() {
        super.onCreate()
        val dbFile = File(filesDir, "schoolcms.db")
        val backupsDir = File(filesDir, "backups")
        if (!backupsDir.exists()) backupsDir.mkdirs()

        NativeBridge.initDatabase(dbFile.absolutePath, filesDir.absolutePath)
        SyncManager.schedulePeriodicWork(this)
        SyncManager.refreshLocalStatus()
    }
}
