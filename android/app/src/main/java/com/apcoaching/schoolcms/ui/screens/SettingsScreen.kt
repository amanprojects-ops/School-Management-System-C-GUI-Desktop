@file:OptIn(ExperimentalMaterial3Api::class)

package com.apcoaching.schoolcms.ui.screens

import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.input.PasswordVisualTransformation
import androidx.compose.ui.unit.dp
import com.apcoaching.schoolcms.nativebridge.NativeBridge
import com.apcoaching.schoolcms.sync.CpanelSyncClient
import com.apcoaching.schoolcms.sync.SyncManager
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import org.json.JSONArray
import org.json.JSONObject

@Composable
fun SettingsScreen(user: LoggedInUser, canEdit: Boolean, refreshSignal: Int) {
    val context = LocalContext.current
    val scope = rememberCoroutineScope()
    var tab by remember { mutableStateOf(0) } // 0=Cloud Sync, 1=Profile, 2=Security, 3=RBAC
    val syncStatus by SyncManager.status.collectAsState()

    var cpanelUrl by remember { mutableStateOf(syncStatus.cpanelUrl) }
    var deviceToken by remember { mutableStateOf(syncStatus.deviceToken) }
    var hmacSecret by remember { mutableStateOf(syncStatus.hmacSecret) }
    var syncEnabled by remember { mutableStateOf(syncStatus.enabled) }
    var syncBanner by remember { mutableStateOf<String?>(null) }
    var syncBusy by remember { mutableStateOf(false) }

    var schoolName by remember { mutableStateOf("") }
    var schoolAddress by remember { mutableStateOf("") }
    var schoolPhone by remember { mutableStateOf("") }
    var principalName by remember { mutableStateOf("") }
    var currencySymbol by remember { mutableStateOf("₹") }
    var profileBanner by remember { mutableStateOf<String?>(null) }

    var oldPwd by remember { mutableStateOf("") }
    var newPwd by remember { mutableStateOf("") }
    var pwdBanner by remember { mutableStateOf<String?>(null) }

    var selectedRoleId by remember { mutableStateOf(2) }
    var rolePerms by remember { mutableStateOf<List<JSONObject>>(emptyList()) }

    LaunchedEffect(refreshSignal, selectedRoleId) {
        withContext(Dispatchers.IO) {
            val s = JSONObject(NativeBridge.settingsGetJson())
            schoolName = s.optString("schoolName")
            schoolAddress = s.optString("schoolAddress")
            schoolPhone = s.optString("schoolPhone")
            principalName = s.optString("principalName")
            currencySymbol = s.optString("currencySymbol", "₹")

            val pArr = JSONArray(NativeBridge.authGetRolePermissionsJson(selectedRoleId))
            rolePerms = (0 until pArr.length()).map { pArr.getJSONObject(it) }
        }
        SyncManager.refreshLocalStatus()
    }

    Column(Modifier.fillMaxSize().padding(16.dp)) {
        TabRow(selectedTabIndex = tab) {
            Tab(selected = tab == 0, onClick = { tab = 0 }, text = { Text("Cloud Sync") })
            Tab(selected = tab == 1, onClick = { tab = 1 }, text = { Text("Profile") })
            Tab(selected = tab == 2, onClick = { tab = 2 }, text = { Text("Security") })
            Tab(selected = tab == 3, onClick = { tab = 3 }, text = { Text("RBAC") })
        }
        Spacer(Modifier.height(12.dp))

        Column(Modifier.verticalScroll(rememberScrollState()), verticalArrangement = Arrangement.spacedBy(10.dp)) {
            when (tab) {
                0 -> {
                    Card(Modifier.fillMaxWidth()) {
                        Column(Modifier.padding(14.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
                            Text("Serverless cPanel Sync", style = MaterialTheme.typography.titleMedium, fontWeight = FontWeight.Bold)
                            Text("Device: ${syncStatus.deviceId} (${syncStatus.devicePrefix}) • Server Seq #${syncStatus.lastServerSeq} • Outbox: ${syncStatus.pendingOutbox}", style = MaterialTheme.typography.bodySmall)
                            Text("Status: ${syncStatus.message}", style = MaterialTheme.typography.bodySmall, color = if (syncStatus.isError) MaterialTheme.colorScheme.error else MaterialTheme.colorScheme.primary)
                        }
                    }

                    Row(verticalAlignment = Alignment.CenterVertically) {
                        Checkbox(checked = syncEnabled, onCheckedChange = { if (canEdit) syncEnabled = it })
                        Text("Enable Automatic Background Delta Sync")
                    }
                    OutlinedTextField(value = cpanelUrl, onValueChange = { cpanelUrl = it }, label = { Text("cPanel Sync API URL") }, modifier = Modifier.fillMaxWidth())
                    OutlinedTextField(value = deviceToken, onValueChange = { deviceToken = it }, label = { Text("Device Bearer Token") }, visualTransformation = PasswordVisualTransformation(), modifier = Modifier.fillMaxWidth())
                    OutlinedTextField(value = hmacSecret, onValueChange = { hmacSecret = it }, label = { Text("HMAC Secret (Optional)") }, visualTransformation = PasswordVisualTransformation(), modifier = Modifier.fillMaxWidth())

                    if (syncBanner != null) Text(syncBanner!!, style = MaterialTheme.typography.bodySmall)

                    if (canEdit) {
                        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                            Button(enabled = !syncBusy, onClick = {
                                scope.launch {
                                    withContext(Dispatchers.IO) { NativeBridge.syncSaveConfig(cpanelUrl, deviceToken, hmacSecret, syncEnabled) }
                                    SyncManager.refreshLocalStatus()
                                    if (syncEnabled) SyncManager.startForegroundLoop(context.applicationContext)
                                    syncBanner = "Sync configuration saved."
                                }
                            }) { Text("Save") }

                            OutlinedButton(enabled = !syncBusy, onClick = {
                                syncBusy = true
                                scope.launch {
                                    val r = withContext(Dispatchers.IO) { CpanelSyncClient.testHealth(cpanelUrl, deviceToken, hmacSecret, syncStatus.deviceId) }
                                    syncBusy = false
                                    syncBanner = r.message
                                }
                            }) { Text("Test") }

                            FilledTonalButton(enabled = !syncBusy, onClick = {
                                syncBusy = true
                                scope.launch {
                                    withContext(Dispatchers.IO) { NativeBridge.syncSaveConfig(cpanelUrl, deviceToken, hmacSecret, syncEnabled) }
                                    val out = SyncManager.runSyncCycle(requireEnabled = false)
                                    syncBusy = false
                                    syncBanner = out.message
                                }
                            }) { Text("Sync Now") }
                        }
                    }
                }
                1 -> {
                    OutlinedTextField(value = schoolName, onValueChange = { schoolName = it }, label = { Text("School Name") }, modifier = Modifier.fillMaxWidth())
                    OutlinedTextField(value = schoolAddress, onValueChange = { schoolAddress = it }, label = { Text("Address") }, modifier = Modifier.fillMaxWidth())
                    OutlinedTextField(value = schoolPhone, onValueChange = { schoolPhone = it }, label = { Text("Phone") }, modifier = Modifier.fillMaxWidth())
                    OutlinedTextField(value = principalName, onValueChange = { principalName = it }, label = { Text("Principal Name") }, modifier = Modifier.fillMaxWidth())
                    OutlinedTextField(value = currencySymbol, onValueChange = { currencySymbol = it }, label = { Text("Currency Symbol") }, modifier = Modifier.fillMaxWidth())
                    if (profileBanner != null) Text(profileBanner!!)
                    if (canEdit) {
                        Button(onClick = {
                            val obj = JSONObject().apply {
                                put("schoolName", schoolName)
                                put("schoolAddress", schoolAddress)
                                put("schoolPhone", schoolPhone)
                                put("principalName", principalName)
                                put("currencySymbol", currencySymbol)
                            }
                            scope.launch {
                                val ok = withContext(Dispatchers.IO) { NativeBridge.settingsSaveJson(obj.toString()) }
                                if (ok) SyncManager.notifyLocalMutation(context.applicationContext)
                                profileBanner = if (ok) "Institutional settings saved." else "Failed updating settings."
                            }
                        }) { Text("Save Profile") }
                    }
                }
                2 -> {
                    OutlinedTextField(value = oldPwd, onValueChange = { oldPwd = it }, label = { Text("Current Password") }, visualTransformation = PasswordVisualTransformation(), modifier = Modifier.fillMaxWidth())
                    OutlinedTextField(value = newPwd, onValueChange = { newPwd = it }, label = { Text("New Password") }, visualTransformation = PasswordVisualTransformation(), modifier = Modifier.fillMaxWidth())
                    if (pwdBanner != null) Text(pwdBanner!!)
                    Button(onClick = {
                        scope.launch {
                            val res = JSONObject(withContext(Dispatchers.IO) { NativeBridge.authChangePassword(user.userId, oldPwd, newPwd) })
                            if (res.optBoolean("ok")) {
                                SyncManager.notifyLocalMutation(context.applicationContext)
                                pwdBanner = "Password updated & queued for sync."
                                oldPwd = ""; newPwd = ""
                            } else pwdBanner = res.optString("error")
                        }
                    }) { Text("Update Password") }
                }
                else -> {
                    Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                        listOf(2 to "Teacher", 3 to "Accountant", 4 to "Staff").forEach { (rid, rlabel) ->
                            FilterChip(selected = selectedRoleId == rid, onClick = { selectedRoleId = rid }, label = { Text(rlabel) })
                        }
                    }
                    rolePerms.forEach { p ->
                        val mod = p.optString("moduleName")
                        val v = p.optBoolean("canView")
                        val e = p.optBoolean("canEdit")
                        Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
                            Text(mod, Modifier.weight(1f), fontWeight = FontWeight.Medium)
                            Text("View")
                            Checkbox(checked = v, onCheckedChange = { nv ->
                                if (!canEdit) return@Checkbox
                                scope.launch {
                                    withContext(Dispatchers.IO) { NativeBridge.authSetRolePermission(selectedRoleId, mod, nv, e) }
                                    SyncManager.notifyLocalMutation(context.applicationContext)
                                    val pArr = JSONArray(withContext(Dispatchers.IO) { NativeBridge.authGetRolePermissionsJson(selectedRoleId) })
                                    rolePerms = (0 until pArr.length()).map { pArr.getJSONObject(it) }
                                }
                            })
                            Text("Edit")
                            Checkbox(checked = e, onCheckedChange = { ne ->
                                if (!canEdit) return@Checkbox
                                scope.launch {
                                    withContext(Dispatchers.IO) { NativeBridge.authSetRolePermission(selectedRoleId, mod, v, ne) }
                                    SyncManager.notifyLocalMutation(context.applicationContext)
                                    val pArr = JSONArray(withContext(Dispatchers.IO) { NativeBridge.authGetRolePermissionsJson(selectedRoleId) })
                                    rolePerms = (0 until pArr.length()).map { pArr.getJSONObject(it) }
                                }
                            })
                        }
                    }
                }
            }
        }
    }
}
