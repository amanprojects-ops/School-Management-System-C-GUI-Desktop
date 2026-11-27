package com.apcoaching.schoolcms.ui.screens

import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.CloudSync
import androidx.compose.material.icons.filled.Lock
import androidx.compose.material.icons.filled.Person
import androidx.compose.material.icons.filled.School
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.text.input.PasswordVisualTransformation
import androidx.compose.ui.unit.dp
import com.apcoaching.schoolcms.nativebridge.NativeBridge
import com.apcoaching.schoolcms.sync.CpanelSyncClient
import com.apcoaching.schoolcms.sync.SyncManager
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import org.json.JSONObject

data class LoggedInUser(
    val userId: Int,
    val username: String,
    val fullName: String,
    val roleId: Int,
    val roleName: String
)

@Composable
fun LoginScreen(onLoginSuccess: (LoggedInUser) -> Unit) {
    val context = LocalContext.current
    val scope = rememberCoroutineScope()

    var username by remember { mutableStateOf("") }
    var password by remember { mutableStateOf("") }
    var errorText by remember { mutableStateOf<String?>(null) }
    var loading by remember { mutableStateOf(false) }

    var showSyncDialog by remember { mutableStateOf(false) }
    val syncStatus by SyncManager.status.collectAsState()

    Column(
        modifier = Modifier
            .fillMaxSize()
            .verticalScroll(rememberScrollState())
            .padding(24.dp),
        horizontalAlignment = Alignment.CenterHorizontally,
        verticalArrangement = Arrangement.Center
    ) {
        Icon(
            imageVector = Icons.Default.School,
            contentDescription = null,
            tint = MaterialTheme.colorScheme.primary,
            modifier = Modifier.size(64.dp)
        )
        Spacer(Modifier.height(12.dp))
        Text(
            text = "APCOACHING",
            style = MaterialTheme.typography.headlineMedium,
            fontWeight = FontWeight.Bold
        )
        Text(
            text = "School Management System — Android Companion",
            style = MaterialTheme.typography.bodyMedium,
            color = MaterialTheme.colorScheme.onSurfaceVariant
        )
        Spacer(Modifier.height(24.dp))

        Card(
            modifier = Modifier.fillMaxWidth().widthIn(max = 460.dp),
            elevation = CardDefaults.cardElevation(defaultElevation = 4.dp)
        ) {
            Column(modifier = Modifier.padding(20.dp), verticalArrangement = Arrangement.spacedBy(12.dp)) {
                Text("Sign In", style = MaterialTheme.typography.titleLarge, fontWeight = FontWeight.SemiBold)

                OutlinedTextField(
                    value = username,
                    onValueChange = { username = it; errorText = null },
                    label = { Text("Username") },
                    leadingIcon = { Icon(Icons.Default.Person, null) },
                    singleLine = true,
                    modifier = Modifier.fillMaxWidth()
                )

                OutlinedTextField(
                    value = password,
                    onValueChange = { password = it; errorText = null },
                    label = { Text("Password") },
                    leadingIcon = { Icon(Icons.Default.Lock, null) },
                    singleLine = true,
                    visualTransformation = PasswordVisualTransformation(),
                    keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Password),
                    modifier = Modifier.fillMaxWidth()
                )

                if (errorText != null) {
                    Text(
                        text = errorText!!,
                        color = MaterialTheme.colorScheme.error,
                        style = MaterialTheme.typography.bodySmall
                    )
                }

                Button(
                    onClick = {
                        if (username.isBlank() || password.isBlank()) {
                            errorText = "Enter both username and password."
                            return@Button
                        }
                        loading = true
                        scope.launch {
                            val resJson = withContext(Dispatchers.IO) {
                                NativeBridge.authLogin(username.trim(), password)
                            }
                            loading = false
                            val obj = JSONObject(resJson)
                            if (obj.optBoolean("ok", false)) {
                                SyncManager.startForegroundLoop(context.applicationContext)
                                onLoginSuccess(
                                    LoggedInUser(
                                        userId = obj.optInt("userId"),
                                        username = obj.optString("username"),
                                        fullName = obj.optString("fullName"),
                                        roleId = obj.optInt("roleId"),
                                        roleName = obj.optString("roleName")
                                    )
                                )
                            } else {
                                errorText = obj.optString("error", "Login failed")
                            }
                        }
                    },
                    enabled = !loading,
                    modifier = Modifier.fillMaxWidth().height(48.dp)
                ) {
                    Text(if (loading) "Authenticating..." else "Login to Dashboard")
                }

                OutlinedButton(
                    onClick = { showSyncDialog = true },
                    modifier = Modifier.fillMaxWidth()
                ) {
                    Icon(Icons.Default.CloudSync, contentDescription = null, modifier = Modifier.size(18.dp))
                    Spacer(Modifier.width(8.dp))
                    Text("Configure cPanel Cloud Sync")
                }
            }
        }
    }

    if (showSyncDialog) {
        var url by remember { mutableStateOf(syncStatus.cpanelUrl) }
        var token by remember { mutableStateOf(syncStatus.deviceToken) }
        var hmac by remember { mutableStateOf(syncStatus.hmacSecret) }
        var msg by remember { mutableStateOf<String?>(null) }
        var busy by remember { mutableStateOf(false) }

        AlertDialog(
            onDismissRequest = { if (!busy) showSyncDialog = false },
            title = { Text("Pre-Login Cloud Sync Setup") },
            text = {
                Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
                    OutlinedTextField(value = url, onValueChange = { url = it }, label = { Text("cPanel API URL") }, singleLine = true)
                    OutlinedTextField(value = token, onValueChange = { token = it }, label = { Text("Device Token") }, singleLine = true)
                    OutlinedTextField(value = hmac, onValueChange = { hmac = it }, label = { Text("HMAC Secret (Optional)") }, singleLine = true)
                    if (msg != null) Text(msg!!, style = MaterialTheme.typography.bodySmall)
                }
            },
            confirmButton = {
                Button(
                    enabled = !busy,
                    onClick = {
                        busy = true
                        scope.launch {
                            val probe = withContext(Dispatchers.IO) {
                                NativeBridge.syncSaveConfig(url, token, hmac, true)
                                val p = CpanelSyncClient.testHealth(url, token, hmac, syncStatus.deviceId)
                                if (p.ok) SyncManager.runSyncCycle(requireEnabled = false)
                                p
                            }
                            busy = false
                            msg = if (probe.ok) "Synced! ${probe.message}" else probe.message
                        }
                    }
                ) { Text(if (busy) "Syncing..." else "Save & Pull Now") }
            },
            dismissButton = {
                TextButton(enabled = !busy, onClick = { showSyncDialog = false }) { Text("Close") }
            }
        )
    }
}
