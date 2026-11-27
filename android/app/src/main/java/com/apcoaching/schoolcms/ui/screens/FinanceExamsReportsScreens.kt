@file:OptIn(ExperimentalMaterial3Api::class)

package com.apcoaching.schoolcms.ui.screens

import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Add
import androidx.compose.material.icons.filled.PictureAsPdf
import androidx.compose.material.icons.filled.Print
import androidx.compose.material.icons.filled.Share
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import com.apcoaching.schoolcms.nativebridge.NativeBridge
import com.apcoaching.schoolcms.pdf.PdfPrintHelper
import com.apcoaching.schoolcms.sync.SyncManager
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import org.json.JSONArray
import org.json.JSONObject
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

private fun jsonList(raw: String): List<JSONObject> {
    val arr = JSONArray(raw)
    return (0 until arr.length()).map { arr.getJSONObject(it) }
}

@Composable
fun FeesScreen(userId: Int, canEdit: Boolean, refreshSignal: Int) {
    val context = LocalContext.current
    val scope = rememberCoroutineScope()
    var tab by remember { mutableStateOf(0) } // 0=Payments, 1=Dues, 2=Structures
    var payments by remember { mutableStateOf<List<JSONObject>>(emptyList()) }
    var dues by remember { mutableStateOf<List<JSONObject>>(emptyList()) }
    var structures by remember { mutableStateOf<List<JSONObject>>(emptyList()) }
    var students by remember { mutableStateOf<List<JSONObject>>(emptyList()) }
    var settings by remember { mutableStateOf(JSONObject()) }
    var showCollect by remember { mutableStateOf(false) }

    fun reload() {
        scope.launch {
            withContext(Dispatchers.IO) {
                payments = jsonList(NativeBridge.feePaymentsJson("", 0))
                dues = jsonList(NativeBridge.feePendingDuesJson(0))
                structures = jsonList(NativeBridge.feeStructuresJson(0))
                students = jsonList(NativeBridge.studentListJson("", 0, "ACTIVE"))
                settings = JSONObject(NativeBridge.settingsGetJson())
            }
        }
    }

    LaunchedEffect(refreshSignal) { reload() }

    val currency = settings.optString("currencySymbol", "₹")

    Scaffold(
        floatingActionButton = {
            if (canEdit) {
                FloatingActionButton(onClick = { showCollect = true }) {
                    Icon(Icons.Default.Add, "Collect Fee")
                }
            }
        }
    ) { pad ->
        Column(Modifier.fillMaxSize().padding(pad).padding(16.dp)) {
            TabRow(selectedTabIndex = tab) {
                Tab(selected = tab == 0, onClick = { tab = 0 }, text = { Text("Receipts (${payments.size})") })
                Tab(selected = tab == 1, onClick = { tab = 1 }, text = { Text("Dues (${dues.size})") })
                Tab(selected = tab == 2, onClick = { tab = 2 }, text = { Text("Structures") })
            }
            Spacer(Modifier.height(10.dp))
            LazyColumn(verticalArrangement = Arrangement.spacedBy(8.dp)) {
                when (tab) {
                    0 -> items(payments) { p ->
                        Card(Modifier.fillMaxWidth()) {
                            Column(Modifier.padding(12.dp)) {
                                Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) {
                                    Text(p.optString("receiptNo"), fontWeight = FontWeight.Bold)
                                    Text("$currency${"%.2f".format(p.optDouble("totalAmount"))}", fontWeight = FontWeight.Bold)
                                }
                                Text("${p.optString("studentName")} (${p.optString("className")}) • ${p.optString("paymentMethod")}", style = MaterialTheme.typography.bodySmall)
                                Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                                    TextButton(onClick = {
                                        val pdf = PdfPrintHelper.generateFeeReceiptPdf(context, settings, p)
                                        PdfPrintHelper.printPdfFile(context, pdf, "Receipt ${p.optString("receiptNo")}")
                                    }) {
                                        Icon(Icons.Default.Print, null, Modifier.size(16.dp))
                                        Spacer(Modifier.width(4.dp))
                                        Text("Print / PDF")
                                    }
                                    TextButton(onClick = {
                                        val pdf = PdfPrintHelper.generateFeeReceiptPdf(context, settings, p)
                                        PdfPrintHelper.sharePdfFile(context, pdf, "Fee Receipt ${p.optString("receiptNo")}")
                                    }) {
                                        Icon(Icons.Default.Share, null, Modifier.size(16.dp))
                                        Spacer(Modifier.width(4.dp))
                                        Text("Share PDF")
                                    }
                                }
                            }
                        }
                    }
                    1 -> items(dues) { d ->
                        Card(Modifier.fillMaxWidth()) {
                            Row(Modifier.padding(12.dp), horizontalArrangement = Arrangement.SpaceBetween) {
                                Column(Modifier.weight(1f)) {
                                    Text(d.optString("studentName"), fontWeight = FontWeight.Bold)
                                    Text("${d.optString("admissionNo")} • ${d.optString("className")}", style = MaterialTheme.typography.bodySmall)
                                }
                                Text("Due: $currency${"%.2f".format(d.optDouble("balanceDue"))}", fontWeight = FontWeight.Bold, color = MaterialTheme.colorScheme.error)
                            }
                        }
                    }
                    else -> items(structures) { fs ->
                        Card(Modifier.fillMaxWidth()) {
                            Row(Modifier.padding(12.dp), horizontalArrangement = Arrangement.SpaceBetween) {
                                Column {
                                    Text(fs.optString("name"), fontWeight = FontWeight.Bold)
                                    Text("${fs.optString("className")} • ${fs.optString("feeType")}", style = MaterialTheme.typography.bodySmall)
                                }
                                Text("$currency${"%.2f".format(fs.optDouble("amount"))}", fontWeight = FontWeight.Bold)
                            }
                        }
                    }
                }
            }
        }
    }

    if (showCollect) {
        var rcpNo by remember { mutableStateOf("") }
        var selStuId by remember { mutableStateOf(students.firstOrNull()?.optInt("id") ?: 0) }
        var selFsId by remember { mutableStateOf(structures.firstOrNull()?.optInt("id") ?: 0) }
        var amount by remember { mutableStateOf((structures.firstOrNull()?.optDouble("amount") ?: 1000.0).toString()) }
        var discount by remember { mutableStateOf("0") }
        var fine by remember { mutableStateOf("0") }
        var method by remember { mutableStateOf("UPI") }
        var err by remember { mutableStateOf<String?>(null) }

        LaunchedEffect(Unit) {
            rcpNo = withContext(Dispatchers.IO) { NativeBridge.feeGenerateReceiptNo() }
        }

        AlertDialog(
            onDismissRequest = { showCollect = false },
            title = { Text("Collect Fee Payment") },
            text = {
                Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
                    OutlinedTextField(value = rcpNo, onValueChange = { rcpNo = it }, label = { Text("Receipt No (Device-M Prefixed)") })
                    if (students.isNotEmpty()) {
                        Text("Student:", style = MaterialTheme.typography.labelMedium)
                        Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                            students.take(3).forEach { s ->
                                val sid = s.optInt("id")
                                FilterChip(selected = selStuId == sid, onClick = { selStuId = sid }, label = { Text(s.optString("fullName").take(12)) })
                            }
                        }
                    }
                    OutlinedTextField(value = amount, onValueChange = { amount = it }, label = { Text("Amount Paid ($currency)") })
                    Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                        listOf("CASH", "UPI", "BANK_TRANSFER").forEach { m ->
                            FilterChip(selected = method == m, onClick = { method = m }, label = { Text(m.take(4)) })
                        }
                    }
                    if (err != null) Text(err!!, color = MaterialTheme.colorScheme.error)
                }
            },
            confirmButton = {
                Button(onClick = {
                    val today = SimpleDateFormat("yyyy-MM-dd", Locale.US).format(Date())
                    val obj = JSONObject().apply {
                        put("receiptNo", rcpNo.trim())
                        put("studentId", selStuId)
                        put("feeStructureId", selFsId)
                        put("amountPaid", amount.toDoubleOrNull() ?: 0.0)
                        put("discount", discount.toDoubleOrNull() ?: 0.0)
                        put("lateFine", fine.toDoubleOrNull() ?: 0.0)
                        put("paymentMethod", method)
                        put("paymentDate", today)
                        put("collectedBy", userId)
                    }
                    scope.launch {
                        val res = JSONObject(withContext(Dispatchers.IO) { NativeBridge.feePaymentCreateJson(obj.toString()) })
                        if (res.optBoolean("ok")) {
                            SyncManager.notifyLocalMutation(context.applicationContext)
                            showCollect = false
                            reload()
                        } else err = res.optString("error", "Failed collecting fee")
                    }
                }) { Text("Confirm Payment") }
            },
            dismissButton = { TextButton(onClick = { showCollect = false }) { Text("Cancel") } }
        )
    }
}

@Composable
fun ExamsScreen(canEdit: Boolean, refreshSignal: Int) {
    val context = LocalContext.current
    val scope = rememberCoroutineScope()
    var exams by remember { mutableStateOf<List<JSONObject>>(emptyList()) }
    var students by remember { mutableStateOf<List<JSONObject>>(emptyList()) }
    var settings by remember { mutableStateOf(JSONObject()) }
    var selectedExamId by remember { mutableStateOf(0) }
    var reportCard by remember { mutableStateOf<JSONObject?>(null) }

    LaunchedEffect(refreshSignal) {
        withContext(Dispatchers.IO) {
            exams = jsonList(NativeBridge.examsListJson())
            students = jsonList(NativeBridge.studentListJson("", 0, "ACTIVE"))
            settings = JSONObject(NativeBridge.settingsGetJson())
            if (selectedExamId == 0 && exams.isNotEmpty()) selectedExamId = exams.first().optInt("id")
        }
    }

    Column(Modifier.fillMaxSize().padding(16.dp), verticalArrangement = Arrangement.spacedBy(10.dp)) {
        Text("Examinations & Report Cards", style = MaterialTheme.typography.titleLarge, fontWeight = FontWeight.Bold)
        LazyColumn(verticalArrangement = Arrangement.spacedBy(8.dp), modifier = Modifier.weight(1f)) {
            items(exams) { ex ->
                val eid = ex.optInt("id")
                Card(Modifier.fillMaxWidth()) {
                    Column(Modifier.padding(12.dp)) {
                        Text(ex.optString("name"), fontWeight = FontWeight.Bold)
                        Text("${ex.optString("startDate")} to ${ex.optString("endDate")} • ${ex.optString("status")}", style = MaterialTheme.typography.bodySmall)
                        if (students.isNotEmpty()) {
                            Spacer(Modifier.height(6.dp))
                            TextButton(onClick = {
                                scope.launch {
                                    val sid = students.first().optInt("id")
                                    val cObj = JSONObject(withContext(Dispatchers.IO) { NativeBridge.examReportCardJson(eid, sid) })
                                    if (cObj.optBoolean("ok")) reportCard = cObj
                                }
                            }) {
                                Icon(Icons.Default.PictureAsPdf, null, Modifier.size(16.dp))
                                Spacer(Modifier.width(4.dp))
                                Text("Preview Sample Report Card PDF")
                            }
                        }
                    }
                }
            }
        }
    }

    if (reportCard != null) {
        val rc = reportCard!!
        AlertDialog(
            onDismissRequest = { reportCard = null },
            title = { Text("${rc.optString("studentName")} — Report Card") },
            text = {
                Column(verticalArrangement = Arrangement.spacedBy(4.dp)) {
                    Text("Exam: ${rc.optString("examName")}")
                    Text("Percentage: ${"%.2f".format(rc.optDouble("percentage"))}% • Grade: ${rc.optString("overallGrade")} • Rank #${rc.optInt("classRank")}")
                }
            },
            confirmButton = {
                Button(onClick = {
                    val pdf = PdfPrintHelper.generateReportCardPdf(context, settings, rc)
                    PdfPrintHelper.printPdfFile(context, pdf, "Report Card")
                    reportCard = null
                }) { Text("Print / PDF") }
            },
            dismissButton = {
                TextButton(onClick = {
                    val pdf = PdfPrintHelper.generateReportCardPdf(context, settings, rc)
                    PdfPrintHelper.sharePdfFile(context, pdf, "Report Card PDF")
                    reportCard = null
                }) { Text("Share PDF") }
            }
        )
    }
}

@Composable
fun ReportsScreen(refreshSignal: Int) {
    val context = LocalContext.current
    val scope = rememberCoroutineScope()
    val categories = listOf(
        "Student Directory",
        "Attendance Summary",
        "Fee Collection",
        "Pending Dues",
        "Teacher List",
        "Exam Results"
    )
    var selType by remember { mutableStateOf(0) }
    var reportObj by remember { mutableStateOf<JSONObject?>(null) }
    var settings by remember { mutableStateOf(JSONObject()) }

    fun generate(type: Int) {
        scope.launch {
            withContext(Dispatchers.IO) {
                reportObj = JSONObject(NativeBridge.reportGenerateJson(type, 0, "", ""))
                settings = JSONObject(NativeBridge.settingsGetJson())
            }
        }
    }

    LaunchedEffect(refreshSignal, selType) { generate(selType) }

    Column(Modifier.fillMaxSize().padding(16.dp), verticalArrangement = Arrangement.spacedBy(10.dp)) {
        Text("Analytics Reports & PDF Export", style = MaterialTheme.typography.titleLarge, fontWeight = FontWeight.Bold)
        Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
            categories.take(3).forEachIndexed { i, lbl ->
                FilterChip(selected = selType == i, onClick = { selType = i }, label = { Text(lbl.substringBefore(" ")) })
            }
        }
        Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
            categories.drop(3).forEachIndexed { idx, lbl ->
                val i = idx + 3
                FilterChip(selected = selType == i, onClick = { selType = i }, label = { Text(lbl.substringBefore(" ")) })
            }
        }

        if (reportObj != null && reportObj!!.optBoolean("ok")) {
            val r = reportObj!!
            Text(r.optString("title"), fontWeight = FontWeight.Bold)
            Text(r.optString("summaryText"), style = MaterialTheme.typography.bodySmall)
            Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                Button(onClick = {
                    val pdf = PdfPrintHelper.generateReportPdf(context, settings, r)
                    PdfPrintHelper.printPdfFile(context, pdf, r.optString("title"))
                }) {
                    Icon(Icons.Default.Print, null, Modifier.size(18.dp))
                    Spacer(Modifier.width(6.dp))
                    Text("Print / PDF")
                }
                OutlinedButton(onClick = {
                    val pdf = PdfPrintHelper.generateReportPdf(context, settings, r)
                    PdfPrintHelper.sharePdfFile(context, pdf, r.optString("title"))
                }) {
                    Icon(Icons.Default.Share, null, Modifier.size(18.dp))
                    Spacer(Modifier.width(6.dp))
                    Text("Share PDF")
                }
            }
            val rows = r.optJSONArray("rows") ?: JSONArray()
            LazyColumn(verticalArrangement = Arrangement.spacedBy(6.dp)) {
                items((0 until rows.length()).toList()) { i ->
                    val cols = rows.getJSONArray(i)
                    Card(Modifier.fillMaxWidth()) {
                        Text(
                            (0 until minOf(cols.length(), 4)).joinToString(" • ") { cols.optString(it) },
                            modifier = Modifier.padding(10.dp),
                            style = MaterialTheme.typography.bodySmall
                        )
                    }
                }
            }
        }
    }
}
