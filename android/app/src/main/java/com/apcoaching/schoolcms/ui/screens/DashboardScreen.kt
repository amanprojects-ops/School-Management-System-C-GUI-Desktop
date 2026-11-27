package com.apcoaching.schoolcms.ui.screens

import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import com.apcoaching.schoolcms.nativebridge.NativeBridge
import com.apcoaching.schoolcms.sync.SyncManager
import com.apcoaching.schoolcms.ui.theme.BrandBlue
import com.apcoaching.schoolcms.ui.theme.BrandTeal
import com.apcoaching.schoolcms.ui.theme.StatusAmber
import com.apcoaching.schoolcms.ui.theme.StatusGreen
import com.apcoaching.schoolcms.ui.theme.StatusRed
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import org.json.JSONArray
import org.json.JSONObject

@Composable
fun DashboardScreen(
    refreshSignal: Int,
    onQuickNavigate: (String) -> Unit
) {
    var data by remember { mutableStateOf(JSONObject()) }
    var currency by remember { mutableStateOf("₹") }
    val syncState by SyncManager.status.collectAsState()

    LaunchedEffect(refreshSignal) {
        withContext(Dispatchers.IO) {
            data = JSONObject(NativeBridge.getDashboardJson())
            currency = JSONObject(NativeBridge.settingsGetJson()).optString("currencySymbol", "₹")
        }
    }

    LazyColumn(
        modifier = Modifier.fillMaxSize().padding(16.dp),
        verticalArrangement = Arrangement.spacedBy(12.dp)
    ) {
        item {
            Card(
                colors = CardDefaults.cardColors(
                    containerColor = if (syncState.isError) Color(0xFFFEF2F2)
                    else if (syncState.enabled) Color(0xFFF0FDF4)
                    else MaterialTheme.colorScheme.surfaceVariant
                )
            ) {
                Row(
                    modifier = Modifier.fillMaxWidth().padding(12.dp),
                    verticalAlignment = Alignment.CenterVertically
                ) {
                    Icon(
                        Icons.Default.CloudSync,
                        contentDescription = null,
                        tint = if (syncState.isError) StatusRed else if (syncState.enabled) StatusGreen else BrandBlue
                    )
                    Spacer(Modifier.width(10.dp))
                    Column(Modifier.weight(1f)) {
                        Text("Cloud Sync (${syncState.devicePrefix})", fontWeight = FontWeight.SemiBold)
                        Text(syncState.message, style = MaterialTheme.typography.bodySmall)
                    }
                }
            }
        }

        item {
            Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(10.dp)) {
                KpiCard("Students", "${data.optInt("totalStudents")}", Icons.Default.People, BrandBlue, Modifier.weight(1f))
                KpiCard("Teachers", "${data.optInt("totalTeachers")}", Icons.Default.Person, BrandTeal, Modifier.weight(1f))
            }
        }
        item {
            Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(10.dp)) {
                KpiCard("Classes", "${data.optInt("totalClasses")}", Icons.Default.Class, Color(0xFF7C3AED), Modifier.weight(1f))
                KpiCard(
                    "Attendance Today",
                    "${"%.1f".format(data.optDouble("todayAttendancePct"))}%",
                    Icons.Default.CheckCircle,
                    StatusGreen,
                    Modifier.weight(1f)
                )
            }
        }
        item {
            Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(10.dp)) {
                KpiCard(
                    "Fees (Month)",
                    "$currency${"%.0f".format(data.optDouble("feeCollectionThisMonth"))}",
                    Icons.Default.Payments,
                    StatusGreen,
                    Modifier.weight(1f)
                )
                KpiCard(
                    "Pending Dues",
                    "$currency${"%.0f".format(data.optDouble("feePendingTotal"))}",
                    Icons.Default.Warning,
                    StatusAmber,
                    Modifier.weight(1f)
                )
            }
        }

        item {
            Text("Quick Actions", style = MaterialTheme.typography.titleMedium, fontWeight = FontWeight.Bold)
            Spacer(Modifier.height(6.dp))
            Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                FilledTonalButton(onClick = { onQuickNavigate("STUDENTS") }, modifier = Modifier.weight(1f)) {
                    Text("Students")
                }
                FilledTonalButton(onClick = { onQuickNavigate("ATTENDANCE") }, modifier = Modifier.weight(1f)) {
                    Text("Attendance")
                }
                FilledTonalButton(onClick = { onQuickNavigate("FEES") }, modifier = Modifier.weight(1f)) {
                    Text("Fees")
                }
            }
        }

        val recentArr = data.optJSONArray("recentAdmissions") ?: JSONArray()
        val recentList = (0 until recentArr.length()).map { recentArr.getJSONObject(it) }
        if (recentList.isNotEmpty()) {
            item {
                Text("Recent Admissions", style = MaterialTheme.typography.titleMedium, fontWeight = FontWeight.Bold)
            }
            items(recentList) { r ->
                Card(Modifier.fillMaxWidth()) {
                    Row(Modifier.padding(12.dp), horizontalArrangement = Arrangement.SpaceBetween) {
                        Column(Modifier.weight(1f)) {
                            Text(r.optString("fullName"), fontWeight = FontWeight.SemiBold)
                            Text("${r.optString("admissionNo")} • ${r.optString("className")}", style = MaterialTheme.typography.bodySmall)
                        }
                        Text(r.optString("admissionDate"), style = MaterialTheme.typography.labelSmall)
                    }
                }
            }
        }

        val examsArr = data.optJSONArray("upcomingExams") ?: JSONArray()
        val examsList = (0 until examsArr.length()).map { examsArr.getJSONObject(it) }
        if (examsList.isNotEmpty()) {
            item {
                Text("Upcoming Examinations", style = MaterialTheme.typography.titleMedium, fontWeight = FontWeight.Bold)
            }
            items(examsList) { e ->
                Card(Modifier.fillMaxWidth()) {
                    Row(Modifier.padding(12.dp), horizontalArrangement = Arrangement.SpaceBetween) {
                        Column(Modifier.weight(1f)) {
                            Text(e.optString("name"), fontWeight = FontWeight.SemiBold)
                            Text("Starts: ${e.optString("startDate")}", style = MaterialTheme.typography.bodySmall)
                        }
                        AssistChip(onClick = {}, label = { Text(e.optString("status")) })
                    }
                }
            }
        }
    }
}

@Composable
private fun KpiCard(title: String, value: String, icon: ImageVector, tint: Color, modifier: Modifier) {
    Card(modifier = modifier, elevation = CardDefaults.cardElevation(defaultElevation = 2.dp)) {
        Column(Modifier.padding(14.dp)) {
            Row(verticalAlignment = Alignment.CenterVertically) {
                Icon(icon, null, tint = tint, modifier = Modifier.size(20.dp))
                Spacer(Modifier.width(6.dp))
                Text(title, style = MaterialTheme.typography.labelMedium, color = MaterialTheme.colorScheme.onSurfaceVariant)
            }
            Spacer(Modifier.height(8.dp))
            Text(value, style = MaterialTheme.typography.headlineSmall, fontWeight = FontWeight.Bold, color = tint)
        }
    }
}
