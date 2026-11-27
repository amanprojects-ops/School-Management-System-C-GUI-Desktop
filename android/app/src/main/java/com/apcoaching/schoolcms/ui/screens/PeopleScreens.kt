@file:OptIn(ExperimentalMaterial3Api::class)

package com.apcoaching.schoolcms.ui.screens

import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Add
import androidx.compose.material.icons.filled.Delete
import androidx.compose.material.icons.filled.Edit
import androidx.compose.material.icons.filled.Search
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

private fun jsonArrayToList(raw: String): List<JSONObject> {
    val arr = JSONArray(raw)
    return (0 until arr.length()).map { arr.getJSONObject(it) }
}

@Composable
fun StudentsScreen(canEdit: Boolean, refreshSignal: Int) {
    val context = LocalContext.current
    val scope = rememberCoroutineScope()
    var search by remember { mutableStateOf("") }
    var students by remember { mutableStateOf<List<JSONObject>>(emptyList()) }
    var classes by remember { mutableStateOf<List<JSONObject>>(emptyList()) }
    var sections by remember { mutableStateOf<List<JSONObject>>(emptyList()) }
    var editingStudent by remember { mutableStateOf<JSONObject?>(null) }
    var showForm by remember { mutableStateOf(false) }

    fun reload() {
        scope.launch {
            withContext(Dispatchers.IO) {
                students = jsonArrayToList(NativeBridge.studentListJson(search, 0, ""))
                classes = jsonArrayToList(NativeBridge.classListJson())
                sections = jsonArrayToList(NativeBridge.sectionListJson(0))
            }
        }
    }

    LaunchedEffect(refreshSignal, search) { reload() }

    Scaffold(
        floatingActionButton = {
            if (canEdit) {
                FloatingActionButton(onClick = {
                    editingStudent = null
                    showForm = true
                }) { Icon(Icons.Default.Add, "Add Student") }
            }
        }
    ) { pad ->
        Column(Modifier.fillMaxSize().padding(pad).padding(16.dp)) {
            OutlinedTextField(
                value = search,
                onValueChange = { search = it },
                placeholder = { Text("Search students by name or ADM-...") },
                leadingIcon = { Icon(Icons.Default.Search, null) },
                singleLine = true,
                modifier = Modifier.fillMaxWidth()
            )
            Spacer(Modifier.height(10.dp))
            LazyColumn(verticalArrangement = Arrangement.spacedBy(8.dp)) {
                items(students) { st ->
                    Card(Modifier.fillMaxWidth()) {
                        Row(Modifier.padding(12.dp), verticalAlignment = Alignment.CenterVertically) {
                            Column(Modifier.weight(1f)) {
                                Text(st.optString("fullName"), fontWeight = FontWeight.Bold)
                                Text(
                                    "${st.optString("admissionNo")} • ${st.optString("className")} (${st.optString("sectionName")}) • Roll #${st.optInt("rollNumber")}",
                                    style = MaterialTheme.typography.bodySmall
                                )
                                Text(
                                    "Phone: ${st.optString("phone").ifBlank { "-" }} • ${st.optString("status")}",
                                    style = MaterialTheme.typography.labelSmall
                                )
                            }
                            if (canEdit) {
                                IconButton(onClick = { editingStudent = st; showForm = true }) {
                                    Icon(Icons.Default.Edit, "Edit")
                                }
                                IconButton(onClick = {
                                    scope.launch {
                                        withContext(Dispatchers.IO) { NativeBridge.studentDelete(st.optInt("id")) }
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

    if (showForm) {
        val initClsId = editingStudent?.optInt("classId") ?: (classes.firstOrNull()?.optInt("id") ?: 0)
        var admNo by remember { mutableStateOf(editingStudent?.optString("admissionNo") ?: "") }
        var fullName by remember { mutableStateOf(editingStudent?.optString("fullName") ?: "") }
        var gender by remember { mutableStateOf(editingStudent?.optString("gender")?.ifBlank { "MALE" } ?: "MALE") }
        var phone by remember { mutableStateOf(editingStudent?.optString("phone") ?: "") }
        var fatherName by remember { mutableStateOf(editingStudent?.optString("fatherName") ?: "") }
        var selectedClassId by remember { mutableStateOf(initClsId) }
        var selectedSectionId by remember {
            mutableStateOf(
                editingStudent?.optInt("sectionId")
                    ?: (sections.firstOrNull { it.optInt("classId") == initClsId }?.optInt("id") ?: 0)
            )
        }
        var rollNo by remember { mutableStateOf((editingStudent?.optInt("rollNumber") ?: 1).toString()) }
        var err by remember { mutableStateOf<String?>(null) }

        LaunchedEffect(Unit) {
            if (admNo.isBlank()) {
                admNo = withContext(Dispatchers.IO) { NativeBridge.studentGenerateAdmissionNo() }
            }
        }

        val availSections = sections.filter { it.optInt("classId") == selectedClassId }
        if (selectedSectionId == 0 && availSections.isNotEmpty()) {
            selectedSectionId = availSections.first().optInt("id")
        }

        AlertDialog(
            onDismissRequest = { showForm = false },
            title = { Text(if (editingStudent == null) "Enroll Student" else "Edit Student") },
            text = {
                Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
                    OutlinedTextField(value = admNo, onValueChange = { admNo = it }, label = { Text("Admission No") }, singleLine = true)
                    OutlinedTextField(value = fullName, onValueChange = { fullName = it }, label = { Text("Full Name*") }, singleLine = true)
                    OutlinedTextField(value = fatherName, onValueChange = { fatherName = it }, label = { Text("Father Name") }, singleLine = true)
                    OutlinedTextField(value = phone, onValueChange = { phone = it }, label = { Text("Phone") }, singleLine = true)
                    OutlinedTextField(value = rollNo, onValueChange = { rollNo = it }, label = { Text("Roll Number") }, singleLine = true)

                    Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                        listOf("MALE", "FEMALE", "OTHER").forEach { g ->
                            FilterChip(selected = gender == g, onClick = { gender = g }, label = { Text(g) })
                        }
                    }

                    if (classes.isNotEmpty()) {
                        Text("Class:", style = MaterialTheme.typography.labelMedium)
                        Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                            classes.take(4).forEach { c ->
                                val cid = c.optInt("id")
                                FilterChip(
                                    selected = selectedClassId == cid,
                                    onClick = {
                                        selectedClassId = cid
                                        selectedSectionId = sections.firstOrNull { it.optInt("classId") == cid }?.optInt("id") ?: 0
                                    },
                                    label = { Text(c.optString("name")) }
                                )
                            }
                        }
                    }
                    if (availSections.isNotEmpty()) {
                        Text("Section:", style = MaterialTheme.typography.labelMedium)
                        Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                            availSections.take(4).forEach { s ->
                                val sid = s.optInt("id")
                                FilterChip(selected = selectedSectionId == sid, onClick = { selectedSectionId = sid }, label = { Text(s.optString("name")) })
                            }
                        }
                    }
                    if (err != null) Text(err!!, color = MaterialTheme.colorScheme.error, style = MaterialTheme.typography.bodySmall)
                }
            },
            confirmButton = {
                Button(onClick = {
                    val today = SimpleDateFormat("yyyy-MM-dd", Locale.US).format(Date())
                    val obj = JSONObject().apply {
                        put("id", editingStudent?.optInt("id") ?: 0)
                        put("admissionNo", admNo.trim())
                        put("fullName", fullName.trim())
                        put("fatherName", fatherName.trim())
                        put("gender", gender)
                        put("phone", phone.trim())
                        put("admissionDate", editingStudent?.optString("admissionDate")?.ifBlank { today } ?: today)
                        put("classId", selectedClassId)
                        put("sectionId", selectedSectionId)
                        put("rollNumber", rollNo.toIntOrNull() ?: 1)
                        put("status", "ACTIVE")
                    }
                    scope.launch {
                        val res = JSONObject(withContext(Dispatchers.IO) { NativeBridge.studentSaveJson(obj.toString()) })
                        if (res.optBoolean("ok")) {
                            SyncManager.notifyLocalMutation(context.applicationContext)
                            showForm = false
                            reload()
                        } else {
                            err = res.optString("error", "Save failed")
                        }
                    }
                }) { Text("Save") }
            },
            dismissButton = { TextButton(onClick = { showForm = false }) { Text("Cancel") } }
        )
    }
}

@Composable
fun TeachersStaffScreen(canEdit: Boolean, refreshSignal: Int) {
    val context = LocalContext.current
    val scope = rememberCoroutineScope()
    var tab by remember { mutableStateOf(0) } // 0=Teachers, 1=Staff
    var search by remember { mutableStateOf("") }
    var teachers by remember { mutableStateOf<List<JSONObject>>(emptyList()) }
    var staffList by remember { mutableStateOf<List<JSONObject>>(emptyList()) }
    var showAdd by remember { mutableStateOf(false) }

    fun reload() {
        scope.launch {
            withContext(Dispatchers.IO) {
                teachers = jsonArrayToList(NativeBridge.teacherListJson(search))
                staffList = jsonArrayToList(NativeBridge.staffListJson(search))
            }
        }
    }

    LaunchedEffect(refreshSignal, search) { reload() }

    Scaffold(
        floatingActionButton = {
            if (canEdit) {
                FloatingActionButton(onClick = { showAdd = true }) { Icon(Icons.Default.Add, "Add") }
            }
        }
    ) { pad ->
        Column(Modifier.fillMaxSize().padding(pad).padding(16.dp)) {
            TabRow(selectedTabIndex = tab) {
                Tab(selected = tab == 0, onClick = { tab = 0 }, text = { Text("Teachers (${teachers.size})") })
                Tab(selected = tab == 1, onClick = { tab = 1 }, text = { Text("Support Staff (${staffList.size})") })
            }
            Spacer(Modifier.height(10.dp))
            OutlinedTextField(
                value = search,
                onValueChange = { search = it },
                placeholder = { Text("Search name, subject, or code...") },
                leadingIcon = { Icon(Icons.Default.Search, null) },
                singleLine = true,
                modifier = Modifier.fillMaxWidth()
            )
            Spacer(Modifier.height(10.dp))
            val itemsList = if (tab == 0) teachers else staffList
            LazyColumn(verticalArrangement = Arrangement.spacedBy(8.dp)) {
                items(itemsList) { item ->
                    Card(Modifier.fillMaxWidth()) {
                        Row(Modifier.padding(12.dp), verticalAlignment = Alignment.CenterVertically) {
                            Column(Modifier.weight(1f)) {
                                Text(item.optString("fullName"), fontWeight = FontWeight.Bold)
                                if (tab == 0) {
                                    Text("${item.optString("teacherCode")} • ${item.optString("subject")}", style = MaterialTheme.typography.bodySmall)
                                } else {
                                    Text("${item.optString("staffCode")} • ${item.optString("roleTitle")}", style = MaterialTheme.typography.bodySmall)
                                }
                                Text("Phone: ${item.optString("phone")}", style = MaterialTheme.typography.labelSmall)
                            }
                            if (canEdit) {
                                IconButton(onClick = {
                                    scope.launch {
                                        withContext(Dispatchers.IO) {
                                            if (tab == 0) NativeBridge.teacherDelete(item.optInt("id"))
                                            else NativeBridge.staffDelete(item.optInt("id"))
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

    if (showAdd) {
        var code by remember { mutableStateOf("") }
        var fullName by remember { mutableStateOf("") }
        var extra by remember { mutableStateOf("") }
        var phone by remember { mutableStateOf("") }
        var salary by remember { mutableStateOf("25000") }
        var err by remember { mutableStateOf<String?>(null) }

        LaunchedEffect(tab) {
            code = withContext(Dispatchers.IO) {
                if (tab == 0) NativeBridge.teacherGenerateCode() else NativeBridge.staffGenerateCode()
            }
        }

        AlertDialog(
            onDismissRequest = { showAdd = false },
            title = { Text(if (tab == 0) "Add Faculty Teacher" else "Add Support Staff") },
            text = {
                Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
                    OutlinedTextField(value = code, onValueChange = { code = it }, label = { Text("Code") }, singleLine = true)
                    OutlinedTextField(value = fullName, onValueChange = { fullName = it }, label = { Text("Full Name*") }, singleLine = true)
                    OutlinedTextField(
                        value = extra,
                        onValueChange = { extra = it },
                        label = { Text(if (tab == 0) "Primary Subject*" else "Role Title*") },
                        singleLine = true
                    )
                    OutlinedTextField(value = phone, onValueChange = { phone = it }, label = { Text("Phone*") }, singleLine = true)
                    OutlinedTextField(value = salary, onValueChange = { salary = it }, label = { Text("Salary") }, singleLine = true)
                    if (err != null) Text(err!!, color = MaterialTheme.colorScheme.error)
                }
            },
            confirmButton = {
                Button(onClick = {
                    val today = SimpleDateFormat("yyyy-MM-dd", Locale.US).format(Date())
                    val obj = JSONObject().apply {
                        put(if (tab == 0) "teacherCode" else "staffCode", code.trim())
                        put("fullName", fullName.trim())
                        put(if (tab == 0) "subject" else "roleTitle", extra.trim())
                        put("phone", phone.trim())
                        put("joiningDate", today)
                        put("salary", salary.toDoubleOrNull() ?: 0.0)
                        put("status", "ACTIVE")
                    }
                    scope.launch {
                        val res = JSONObject(withContext(Dispatchers.IO) {
                            if (tab == 0) NativeBridge.teacherSaveJson(obj.toString())
                            else NativeBridge.staffSaveJson(obj.toString())
                        })
                        if (res.optBoolean("ok")) {
                            SyncManager.notifyLocalMutation(context.applicationContext)
                            showAdd = false
                            reload()
                        } else {
                            err = res.optString("error", "Save failed")
                        }
                    }
                }) { Text("Save") }
            },
            dismissButton = { TextButton(onClick = { showAdd = false }) { Text("Cancel") } }
        )
    }
}
