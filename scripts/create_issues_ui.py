"""Create GitHub issues for UI icon, school name, and window icon bugs."""
import urllib.request
import json
import subprocess
import ssl

ctx = ssl.create_default_context()
proc = subprocess.run(
    ['git', 'credential', 'fill'],
    input='protocol=https\nhost=github.com\n',
    capture_output=True,
    text=True
)
token = ''
for line in proc.stdout.splitlines():
    if line.startswith('password='):
        token = line.split('=', 1)[1].strip()
        break

REPO = 'amanprojects-ops/School-Management-System-C-GUI-Desktop'
BASE = f'https://api.github.com/repos/{REPO}/issues'
HDR = {
    'Authorization': f'token {token}',
    'Content-Type': 'application/json',
    'Accept': 'application/vnd.github.v3+json'
}

ISSUE_ICON = {
    'title': 'BUG: App uses wrong icon (accessories-dictionary-symbolic) in sidebar and login screen',
    'body': (
        '## Description\n'
        'Both the sidebar brand area and the login screen logo use `accessories-dictionary-symbolic`'
        ' -- a generic dictionary/thesaurus icon unrelated to a school management app.\n\n'
        '## Affected Files\n'
        '- `src/ui/sidebar.c` line 58 -- sidebar brand logo\n'
        '- `src/ui/login.c` line 106 -- login screen hero icon\n\n'
        '## Expected Behavior\n'
        'Use `applications-education-symbolic` as the app icon everywhere.\n\n'
        '## Steps to Reproduce\n'
        '1. Launch the app -- the login screen shows a dictionary icon\n'
        '2. Log in -- the sidebar brand shows the same dictionary icon\n\n'
        '## Severity\nMedium -- Cosmetic but affects professional appearance and branding.'
    ),
    'labels': ['bug']
}

ISSUE_SETTINGS = {
    'title': 'BUG: Settings form fields have no placeholder text; school name blank on first launch',
    'body': (
        '## Description\n'
        'In `settings_view.c`, the form fields for school name, address, phone, email, website,'
        ' principal name, and currency have no `gtk_entry_set_placeholder_text()` calls.\n'
        'On a fresh install all fields are completely blank with no guidance for the user.\n\n'
        '## Affected File\n'
        '- `src/ui/settings_view.c`\n\n'
        '## Expected Behavior\n'
        'Each entry field should show a helpful placeholder, e.g.:\n'
        '- School Name: "e.g. St. Mary Higher Secondary School"\n'
        '- Contact Phone: "e.g. +91 9876543210"\n'
        '- Currency Symbol: "e.g. Rs. or INR"\n\n'
        '## Steps to Reproduce\n'
        '1. Fresh database install\n'
        '2. Navigate to Settings -- all fields blank with no guidance\n\n'
        '## Severity\nMedium -- Affects usability on first setup.'
    ),
    'labels': ['bug']
}

ISSUE_WINDOW_ICON = {
    'title': 'BUG: Main window taskbar has no application icon (OS-level icon missing)',
    'body': (
        '## Description\n'
        '`window.c` never calls `gtk_window_set_icon_name()`. The OS taskbar, window title bar,'
        ' and Alt+Tab switcher all show a generic blank icon.\n\n'
        '## Affected File\n'
        '- `src/ui/window.c`\n\n'
        '## Expected Behavior\n'
        'Call `gtk_window_set_icon_name(GTK_WINDOW(window), "applications-education-symbolic")`'
        ' after window creation so the OS shows an appropriate icon.\n\n'
        '## Steps to Reproduce\n'
        '1. Launch SchoolCMS\n'
        '2. Check the Windows taskbar -- icon is generic/blank\n\n'
        '## Severity\nLow -- Cosmetic but affects OS integration quality.'
    ),
    'labels': ['bug']
}

for iss in [ISSUE_ICON, ISSUE_SETTINGS, ISSUE_WINDOW_ICON]:
    data = json.dumps({
        'title': iss['title'],
        'body': iss['body'],
        'labels': iss['labels']
    }).encode()
    req = urllib.request.Request(BASE, data=data, headers=HDR, method='POST')
    with urllib.request.urlopen(req, context=ctx) as r:
        resp = json.loads(r.read())
        print(f'Created #{resp["number"]}: {resp["html_url"]}')
