package com.apcoaching.schoolcms

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.compose.foundation.layout.*
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.unit.dp
import com.apcoaching.schoolcms.nativebridge.NativeBridge
import com.apcoaching.schoolcms.sync.SyncManager
import com.apcoaching.schoolcms.ui.screens.*
import com.apcoaching.schoolcms.ui.theme.SchoolCmsTheme
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext

data class NavModuleItem(
    val key: String,
    val title: String,
    val rbacModule: String?,
    val icon: ImageVector
)

private val ALL_MODULES = listOf(
    NavModuleItem("DASHBOARD", "Dashboard", null, Icons.Default.Dashboard),
    NavModuleItem("STUDENTS", "Students", "STUDENTS", Icons.Default.People),
    NavModuleItem("TEACHERS", "Teachers & Staff", "TEACHERS", Icons.Default.Person),
    NavModuleItem("CLASSES", "Classes & Subjects", "CLASSES", Icons.Default.Class),
    NavModuleItem("ATTENDANCE", "Attendance", "ATTENDANCE", Icons.Default.CheckCircle),
    NavModuleItem("FEES", "Fees & Receipts", "FEES", Icons.Default.Payments),
    NavModuleItem("EXAMS", "Exams & Marksheets", "EXAMS", Icons.Default.School),
    NavModuleItem("TIMETABLE", "Timetable", "TIMETABLE", Icons.Default.Schedule),
    NavModuleItem("REPORTS", "Reports & PDF", "REPORTS", Icons.Default.Assessment),
    NavModuleItem("SETTINGS", "Settings & Sync", "SETTINGS", Icons.Default.Settings)
)

class MainActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContent {
            SchoolCmsTheme {
                Surface(modifier = Modifier.fillMaxSize(), color = MaterialTheme.colorScheme.background) {
                    SchoolCmsRoot()
                }
            }
        }
    }
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun SchoolCmsRoot() {
    var currentUser by remember { mutableStateOf<LoggedInUser?>(null) }
    var activePage by remember { mutableStateOf("DASHBOARD") }
    var refreshTrigger by remember { mutableStateOf(0) }
    var allowedKeys by remember { mutableStateOf(setOf("DASHBOARD")) }
    var editRights by remember { mutableStateOf<Map<String, Boolean>>(emptyMap()) }

    val drawerState = rememberDrawerState(DrawerValue.Closed)
    val scope = rememberCoroutineScope()

    suspend fun reloadPermissions() {
        val allowed = mutableSetOf("DASHBOARD")
        val edits = mutableMapOf<String, Boolean>()
        withContext(Dispatchers.IO) {
            ALL_MODULES.forEach { m ->
                if (m.rbacModule == null) {
                    allowed.add(m.key)
                    edits[m.key] = true
                } else {
                    val canView = NativeBridge.authCanAccess(m.rbacModule, false)
                    val canEdit = NativeBridge.authCanAccess(m.rbacModule, true)
                    if (canView) allowed.add(m.key)
                    edits[m.key] = canEdit
                }
            }
        }
        allowedKeys = allowed
        editRights = edits
        if (!allowed.contains(activePage)) activePage = "DASHBOARD"
    }

    LaunchedEffect(Unit) {
        SyncManager.remoteUpdates.collect { appliedCount ->
            if (appliedCount > 0 && currentUser != null) {
                reloadPermissions()
                refreshTrigger++
            }
        }
    }

    LaunchedEffect(currentUser) {
        if (currentUser != null) {
            reloadPermissions()
            refreshTrigger++
        }
    }

    val user = currentUser
    if (user == null) {
        LoginScreen { loggedIn ->
            currentUser = loggedIn
            activePage = "DASHBOARD"
        }
        return
    }

    val visibleModules = ALL_MODULES.filter { allowedKeys.contains(it.key) }

    ModalNavigationDrawer(
        drawerState = drawerState,
        drawerContent = {
            ModalDrawerSheet {
                Column(Modifier.padding(16.dp)) {
                    Text("APCOACHING", style = MaterialTheme.typography.titleLarge)
                    Text("${user.fullName} (${user.roleName})", style = MaterialTheme.typography.bodySmall)
                }
                Divider()
                visibleModules.forEach { mod ->
                    NavigationDrawerItem(
                        icon = { Icon(mod.icon, null) },
                        label = { Text(mod.title) },
                        selected = activePage == mod.key,
                        onClick = {
                            activePage = mod.key
                            scope.launch { drawerState.close() }
                        },
                        modifier = Modifier.padding(NavigationDrawerItemDefaults.ItemPadding)
                    )
                }
                Spacer(Modifier.weight(1f))
                NavigationDrawerItem(
                    icon = { Icon(Icons.Default.Logout, null) },
                    label = { Text("Logout") },
                    selected = false,
                    onClick = {
                        scope.launch {
                            withContext(Dispatchers.IO) { NativeBridge.authLogout() }
                            currentUser = null
                        }
                    },
                    modifier = Modifier.padding(NavigationDrawerItemDefaults.ItemPadding)
                )
            }
        }
    ) {
        val currentMod = ALL_MODULES.firstOrNull { it.key == activePage } ?: ALL_MODULES.first()
        val canEditCurrent = editRights[activePage] == true

        Scaffold(
            topBar = {
                TopAppBar(
                    title = { Text(currentMod.title) },
                    navigationIcon = {
                        IconButton(onClick = { scope.launch { drawerState.open() } }) {
                            Icon(Icons.Default.Menu, "Menu")
                        }
                    },
                    actions = {
                        IconButton(onClick = { scope.launch { SyncManager.runSyncCycle(false); refreshTrigger++ } }) {
                            Icon(Icons.Default.Sync, "Sync Now")
                        }
                    }
                )
            },
            bottomBar = {
                NavigationBar {
                    visibleModules.take(4).forEach { mod ->
                        NavigationBarItem(
                            icon = { Icon(mod.icon, null) },
                            label = { Text(mod.title.substringBefore(" ")) },
                            selected = activePage == mod.key,
                            onClick = { activePage = mod.key }
                        )
                    }
                }
            }
        ) { pad ->
            Box(Modifier.fillMaxSize().padding(pad)) {
                when (activePage) {
                    "DASHBOARD" -> DashboardScreen(refreshTrigger) { target -> if (allowedKeys.contains(target)) activePage = target }
                    "STUDENTS" -> StudentsScreen(canEditCurrent, refreshTrigger)
                    "TEACHERS" -> TeachersStaffScreen(canEditCurrent, refreshTrigger)
                    "CLASSES" -> ClassesScreen(canEditCurrent, refreshTrigger)
                    "ATTENDANCE" -> AttendanceScreen(user.userId, canEditCurrent, refreshTrigger)
                    "FEES" -> FeesScreen(user.userId, canEditCurrent, refreshTrigger)
                    "EXAMS" -> ExamsScreen(canEditCurrent, refreshTrigger)
                    "TIMETABLE" -> TimetableScreen(canEditCurrent, refreshTrigger)
                    "REPORTS" -> ReportsScreen(refreshTrigger)
                    "SETTINGS" -> SettingsScreen(user, canEditCurrent, refreshTrigger)
                }
            }
        }
    }
}
