@file:OptIn(ExperimentalMaterial3Api::class)

package com.apcoaching.schoolcms.ui.screens

import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Add
import androidx.compose.material.icons.filled.Delete
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import com.apcoaching.schoolcms.nativebridge.NativeBridge
import com.apcoaching.schoolcms.sync.SyncManager
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import org.json.JSONArray
import org.json.JSONObject
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

private fun parseArr(raw: String): List<JSONObject> {
    val arr = JSONArray(raw)
    return (0 until arr.length()).map { arr.getJSONObject(it) }
}

@Composable
fun ClassesScreen(canEdit: Boolean, refreshSignal: Int) {
    val context = LocalContext.current
    val scope = rememberCoroutineScope()
    var tab by remember { mutableStateOf(0) } // 0=Classes, 1=Sections, 2=Subjects
    var classes by remember { mutableStateOf<List<JSONObject>>(emptyList()) }
    var sections by remember { mutableStateOf<List<JSONObject>>(emptyList()) }
    var subjects by remember { mutableStateOf<List<JSONObject>>(emptyList()) }
    var teachers by remember { mutableStateOf<List<JSONObject>>(emptyList()) }
    var showCreate by remember { mutableStateOf(false) }

    fun reload() {
        scope.launch {
            withContext(Dispatchers.IO) {
                classes = parseArr(NativeBridge.classListJson())
                sections = parseArr(NativeBridge.sectionListJson(0))
                subjects = parseArr(NativeBridge.subjectListJson(0))
                teachers = parseArr(NativeBridge.teacherListJson(""))
            }
        }
    }

    LaunchedEffect(refreshSignal) { reload() }

    Scaffold(
        floatingActionButton = {
            if (canEdit) {
                FloatingActionButton(onClick = { showCreate = true }) { Icon(Icons.Default.Add, "Add") }
            }
        }
    ) { pad ->
        Column(Modifier.fillMaxSize().padding(pad).padding(16.dp)) {
            TabRow(selectedTabIndex = tab) {
                Tab(selected = tab == 0, onClick = { tab = 0 }, text = { Text("Classes (${classes.size})") })
                Tab(selected = tab == 1, onClick = { tab = 1 }, text = { Text("Sections (${sections.size})") })
                Tab(selected = tab == 2, onClick = { tab = 2 }, text = { Text("Subjects (${subjects.size})") })
            }
            Spacer(Modifier.height(10.dp))
            LazyColumn(verticalArrangement = Arrangement.spacedBy(8.dp)) {
                val list = when (tab) { 0 -> classes; 1 -> sections; else -> subjects }
                items(list) { row ->
                    Card(Modifier.fillMaxWidth()) {
                        Row(Modifier.padding(12.dp), verticalAlignment = Alignment.CenterVertically) {
                            Column(Modifier.weight(1f)) {
                                when (tab) {
                                    0 -> {
                                        Text(row.optString("name"), fontWeight = FontWeight.Bold)
                                        Text(
                                            "Capacity: ${row.optInt("capacity")} • Students: ${row.optInt("studentCount")} • Sections: ${row.optInt("sectionCount")}",
                                            style = MaterialTheme.typography.bodySmall
                                        )
                                    }
                                    1 -> {
                                        Text("${row.optString("className")} — Section ${row.optString("name")}", fontWeight = FontWeight.Bold)
                                        Text("Students: ${row.optInt("studentCount")}", style = MaterialTheme.typography.bodySmall)
                                    }
                                    else -> {
                                        Text("${row.optString("code")}: ${row.optString("name")}", fontWeight = FontWeight.Bold)
                                        Text("${row.optString("className")} • Teacher: ${row.optString("teacherName").ifBlank { "-" }}", style = MaterialTheme.typography.bodySmall)
                                    }
                                }
                            }
                            if (canEdit) {
                                IconButton(onClick = {
                                    scope.launch {
                                        withContext(Dispatchers.IO) {
                                            when (tab) {
                                                0 -> NativeBridge.classDelete(row.optInt("id"))
                                                1 -> NativeBridge.sectionDelete(row.optInt("id"))
                                                else -> NativeBridge.subjectDelete(row.optInt("id"))
                                            }
                                        }
                                        SyncManager.notifyLocalMutation(context.applicationContext)
                                        reload()
                                    }
                                }) { Icon(Icons.Default.Delete, "Delete") }
                            }
                        }
                    }
                }
            }
        }
    }

    if (showCreate) {
        var name by remember { mutableStateOf("") }
        var code by remember { mutableStateOf("") }
        var numVal by remember { mutableStateOf("10") }
        var selClassId by remember { mutableStateOf(classes.firstOrNull()?.optInt("id") ?: 0) }
        var selTeacherId by remember { mutableStateOf(teachers.firstOrNull()?.optInt("id") ?: 0) }
        var err by remember { mutableStateOf<String?>(null) }

        AlertDialog(
            onDismissRequest = { showCreate = false },
            title = { Text(when (tab) { 0 -> "New Class"; 1 -> "New Section"; else -> "New Subject" }) },
            text = {
                Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
                    if (tab == 0) {
                        OutlinedTextField(value = name, onValueChange = { name = it }, label = { Text("Class Name (e.g. Class 10)*") })
                        OutlinedTextField(value = numVal, onValueChange = { numVal = it }, label = { Text("Numeric Level") })
                    } else {
                        if (classes.isNotEmpty()) {
                            Text("Target Class:", style = MaterialTheme.typography.labelMedium)
                            Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                                classes.take(4).forEach { c ->
                                    val cid = c.optInt("id")
                                    FilterChip(selected = selClassId == cid, onClick = { selClassId = cid }, label = { Text(c.optString("name")) })
                                }
                            }
                        }
                        if (tab == 2) {
                            OutlinedTextField(value = code, onValueChange = { code = it }, label = { Text("Subject Code (e.g. MATH-101)*") })
                        }
                        OutlinedTextField(value = name, onValueChange = { name = it }, label = { Text(if (tab == 1) "Section Name (e.g. A)*" else "Subject Name*") })
                    }
                    if (err != null) Text(err!!, color = MaterialTheme.colorScheme.error)
                }
            },
            confirmButton = {
                Button(onClick = {
                    val obj = JSONObject().apply {
                        put("name", name.trim())
                        put("numericValue", numVal.toIntOrNull() ?: 1)
                        put("capacity", 45)
                        put("academicYearId", 1)
                        put("classId", selClassId)
                        put("code", code.trim())
                        put("teacherId", selTeacherId)
                    }
                    scope.launch {
                        val res = JSONObject(withContext(Dispatchers.IO) {
                            when (tab) {
                                0 -> NativeBridge.classCreateJson(obj.toString())
                                1 -> NativeBridge.sectionCreateJson(obj.toString())
                                else -> NativeBridge.subjectCreateJson(obj.toString())
                            }
                        })
                        if (res.optBoolean("ok")) {
                            SyncManager.notifyLocalMutation(context.applicationContext)
                            showCreate = false
                            reload()
                        } else err = res.optString("error", "Create failed")
                    }
                }) { Text("Save") }
            },
            dismissButton = { TextButton(onClick = { showCreate = false }) { Text("Cancel") } }
        )
    }
}

@Composable
fun AttendanceScreen(userId: Int, canEdit: Boolean, refreshSignal: Int) {
    val context = LocalContext.current
    val scope = rememberCoroutineScope()
    var classes by remember { mutableStateOf<List<JSONObject>>(emptyList()) }
    var sections by remember { mutableStateOf<List<JSONObject>>(emptyList()) }
    var selClassId by remember { mutableStateOf(0) }
    var selSectionId by remember { mutableStateOf(0) }
    var dateStr by remember { mutableStateOf(SimpleDateFormat("yyyy-MM-dd", Locale.US).format(Date())) }
    val statusMap = remember { mutableStateMapOf<Int, String>() }
    var roster by remember { mutableStateOf<List<JSONObject>>(emptyList()) }
    var banner by remember { mutableStateOf<String?>(null) }

    fun loadRoster() {
        if (selClassId <= 0 || selSectionId <= 0) return
        scope.launch {
            val list = withContext(Dispatchers.IO) {
                parseArr(NativeBridge.attendanceRosterJson(selClassId, selSectionId, dateStr))
            }
            roster = list
            statusMap.clear()
            list.forEach { r ->
                val sid = r.optInt("studentId")
                val cur = r.optString("status").ifBlank { "PRESENT" }
                statusMap[sid] = if (cur == "UNMARKED") "PRESENT" else cur
            }
        }
    }

    LaunchedEffect(refreshSignal) {
        withContext(Dispatchers.IO) {
            classes = parseArr(NativeBridge.classListJson())
            sections = parseArr(NativeBridge.sectionListJson(0))
        }
        if (selClassId == 0 && classes.isNotEmpty()) {
            selClassId = classes.first().optInt("id")
        }
        if (selSectionId == 0 && selClassId > 0) {
            selSectionId = sections.firstOrNull { it.optInt("classId") == selClassId }?.optInt("id") ?: 0
        }
        loadRoster()
    }

    Column(Modifier.fillMaxSize().padding(16.dp), verticalArrangement = Arrangement.spacedBy(10.dp)) {
        if (classes.isNotEmpty()) {
            Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                classes.take(4).forEach { c ->
                    val cid = c.optInt("id")
                    FilterChip(
                        selected = selClassId == cid,
                        onClick = {
                            selClassId = cid
                            selSectionId = sections.firstOrNull { it.optInt("classId") == cid }?.optInt("id") ?: 0
                            loadRoster()
                        },
                        label = { Text(c.optString("name")) }
                    )
                }
            }
        }
        Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            OutlinedTextField(
                value = dateStr,
                onValueChange = { dateStr = it },
                label = { Text("Date (YYYY-MM-DD)") },
                singleLine = true,
                modifier = Modifier.weight(1f)
            )
            Button(onClick = { loadRoster() }) { Text("Load") }
        }

        if (canEdit && roster.isNotEmpty()) {
            Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                OutlinedButton(onClick = { roster.forEach { statusMap[it.optInt("studentId")] = "PRESENT" } }) {
                    Text("Mark All Present")
                }
                Button(onClick = {
                    val arr = JSONArray()
                    roster.forEach { r ->
                        val sid = r.optInt("studentId")
                        arr.put(JSONObject().apply {
                            put("studentId", sid)
                            put("status", statusMap[sid] ?: "PRESENT")
                            put("remarks", "")
                        })
                    }
                    scope.launch {
                        val ok = withContext(Dispatchers.IO) {
                            NativeBridge.attendanceMarkBulkJson(selClassId, selSectionId, dateStr, arr.toString(), userId)
                        }
                        if (ok) {
                            banner = "Saved attendance for ${roster.size} students."
                            SyncManager.notifyLocalMutation(context.applicationContext)
                        } else banner = "Failed saving attendance."
                    }
                }) { Text("Save Attendance") }
            }
        }

        if (banner != null) Text(banner!!, color = MaterialTheme.colorScheme.primary)

        LazyColumn(verticalArrangement = Arrangement.spacedBy(8.dp)) {
            items(roster) { r ->
                val sid = r.optInt("studentId")
                val cur = statusMap[sid] ?: "PRESENT"
                Card(Modifier.fillMaxWidth()) {
                    Column(Modifier.padding(10.dp)) {
                        Text("${r.optString("studentName")} (${r.optString("admissionNo")})", fontWeight = FontWeight.Bold)
                        Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                            listOf("PRESENT", "ABSENT", "LATE").forEach { st ->
                                FilterChip(
                                    selected = cur == st,
                                    onClick = { if (canEdit) statusMap[sid] = st },
                                    label = { Text(st) }
                                )
                            }
                        }
                    }
                }
            }
        }
    }
}

@Composable
fun TimetableScreen(canEdit: Boolean, refreshSignal: Int) {
    val context = LocalContext.current
    val scope = rememberCoroutineScope()
    var classes by remember { mutableStateOf<List<JSONObject>>(emptyList()) }
    var sections by remember { mutableStateOf<List<JSONObject>>(emptyList()) }
    var subjects by remember { mutableStateOf<List<JSONObject>>(emptyList()) }
    var teachers by remember { mutableStateOf<List<JSONObject>>(emptyList()) }
    var selClassId by remember { mutableStateOf(0) }
    var selSectionId by remember { mutableStateOf(0) }
    var selDay by remember { mutableStateOf("MONDAY") }
    var slots by remember { mutableStateOf<List<JSONObject>>(emptyList()) }
    var showAdd by remember { mutableStateOf(false) }

    fun loadSlots() {
        if (selClassId <= 0 || selSectionId <= 0) return
        scope.launch {
            slots = withContext(Dispatchers.IO) {
                parseArr(NativeBridge.timetableByClassJson(selClassId, selSectionId, selDay))
            }
        }
    }

    LaunchedEffect(refreshSignal) {
        withContext(Dispatchers.IO) {
            classes = parseArr(NativeBridge.classListJson())
            sections = parseArr(NativeBridge.sectionListJson(0))
            subjects = parseArr(NativeBridge.subjectListJson(0))
            teachers = parseArr(NativeBridge.teacherListJson(""))
        }
        if (selClassId == 0 && classes.isNotEmpty()) selClassId = classes.first().optInt("id")
        if (selSectionId == 0 && selClassId > 0) selSectionId = sections.firstOrNull { it.optInt("classId") == selClassId }?.optInt("id") ?: 0
        loadSlots()
    }

    Scaffold(
        floatingActionButton = {
            if (canEdit && selClassId > 0 && selSectionId > 0) {
                FloatingActionButton(onClick = { showAdd = true }) { Icon(Icons.Default.Add, "Add Slot") }
            }
        }
    ) { pad ->
        Column(Modifier.fillMaxSize().padding(pad).padding(16.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
            Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                listOf("MONDAY", "TUESDAY", "WEDNESDAY", "THURSDAY", "FRIDAY", "SATURDAY").take(4).forEach { d ->
                    FilterChip(selected = selDay == d, onClick = { selDay = d; loadSlots() }, label = { Text(d.take(3)) })
                }
            }
            LazyColumn(verticalArrangement = Arrangement.spacedBy(8.dp)) {
                items(slots) { sl ->
                    Card(Modifier.fillMaxWidth()) {
                        Row(Modifier.padding(12.dp), verticalAlignment = Alignment.CenterVertically) {
                            Column(Modifier.weight(1f)) {
                                Text("Period ${sl.optInt("periodNumber")} (${sl.optString("startTime")}-${sl.optString("endTime")})", fontWeight = FontWeight.Bold)
                                Text("${sl.optString("subjectName")} • ${sl.optString("teacherName")}", style = MaterialTheme.typography.bodySmall)
                                Text("Room: ${sl.optString("roomNumber").ifBlank { "-" }}", style = MaterialTheme.typography.labelSmall)
                            }
                            if (canEdit) {
                                IconButton(onClick = {
                                    scope.launch {
                                        withContext(Dispatchers.IO) { NativeBridge.timetableDelete(sl.optInt("id")) }
                                        SyncManager.notifyLocalMutation(context.applicationContext)
                                        loadSlots()
                                    }
                                }) { Icon(Icons.Default.Delete, "Delete") }
                            }
                        }
                    }
                }
            }
        }
    }

    if (showAdd) {
        var period by remember { mutableStateOf("1") }
        var room by remember { mutableStateOf("Room 101") }
        var err by remember { mutableStateOf<String?>(null) }
        val clsSubs = subjects.filter { it.optInt("classId") == selClassId }
        val subId = clsSubs.firstOrNull()?.optInt("id") ?: (subjects.firstOrNull()?.optInt("id") ?: 0)
        val tchId = teachers.firstOrNull()?.optInt("id") ?: 0

        AlertDialog(
            onDismissRequest = { showAdd = false },
            title = { Text("Assign Period ($selDay)") },
            text = {
                Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
                    OutlinedTextField(value = period, onValueChange = { period = it }, label = { Text("Period # (1-8)") })
                    OutlinedTextField(value = room, onValueChange = { room = it }, label = { Text("Room Number") })
                    if (err != null) Text(err!!, color = MaterialTheme.colorScheme.error)
                }
            },
            confirmButton = {
                Button(onClick = {
                    val obj = JSONObject().apply {
                        put("classId", selClassId)
                        put("sectionId", selSectionId)
                        put("dayOfWeek", selDay)
                        put("periodNumber", period.toIntOrNull() ?: 1)
                        put("startTime", "09:00")
                        put("endTime", "09:45")
                        put("subjectId", subId)
                        put("teacherId", tchId)
                        put("roomNumber", room.trim())
                    }
                    scope.launch {
                        val res = JSONObject(withContext(Dispatchers.IO) { NativeBridge.timetableSaveSlotJson(obj.toString()) })
                        if (res.optBoolean("ok")) {
                            SyncManager.notifyLocalMutation(context.applicationContext)
                            showAdd = false
                            loadSlots()
                        } else err = res.optString("error", "Conflict or invalid data")
                    }
                }) { Text("Save") }
            },
            dismissButton = { TextButton(onClick = { showAdd = false }) { Text("Cancel") } }
        )
    }
}
