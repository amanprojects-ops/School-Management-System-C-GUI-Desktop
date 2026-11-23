"""Create a GitHub PR for the icon, settings, and window icon fixes."""
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
url = f'https://api.github.com/repos/{REPO}/pulls'
HDR = {
    'Authorization': f'token {token}',
    'Content-Type': 'application/json',
    'Accept': 'application/vnd.github.v3+json'
}

body_lines = [
    '## Summary of Changes',
    '',
    'Fixes three UI/branding issues reported in #6, #7, and #8.',
    '',
    '---',
    '',
    '### Fix 1 - App Icon (closes #6)',
    '**Files:** `src/ui/sidebar.c`, `src/ui/login.c`',
    '- Replaced `accessories-dictionary-symbolic` (generic dictionary icon) with '
    '`applications-education-symbolic` in both the sidebar brand logo and the login '
    'screen hero icon.',
    '',
    '### Fix 2 - Settings Form Placeholders (closes #7)',
    '**File:** `src/ui/settings_view.c`',
    '- Added `gtk_entry_set_placeholder_text()` to all 7 institutional profile entry fields:',
    '  - School Name, Principal Name, Address, Phone, Email, Website, Currency Symbol.',
    '- Fresh-install users now see contextual hints instead of empty blank fields.',
    '',
    '### Fix 3 - OS Window Icon (closes #8)',
    '**File:** `src/ui/window.c`',
    '- Added `gtk_window_set_icon_name(GTK_WINDOW(window), "applications-education-symbolic")`',
    '  so the OS taskbar, window title bar, and Alt+Tab switcher show a proper school icon.',
    '',
    '---',
    '',
    '### Verification',
    '- Build: `ninja` -- 0 warnings, 0 errors',
    '- Tests: 13/13 PASS (`bash run_tests.sh`)',
    '- Commits: 4 commits (1 file per commit as per project convention)',
]
body = '\n'.join(body_lines)

data = json.dumps({
    'title': 'fix(ui): app icon, settings placeholders, and OS window icon (fixes #6 #7 #8)',
    'body': body,
    'head': 'main',
    'base': 'v1.0.0-release',
    'draft': False
}).encode()

req = urllib.request.Request(url, data=data, headers=HDR, method='POST')
try:
    with urllib.request.urlopen(req, context=ctx) as r:
        resp = json.loads(r.read())
        print(f'PR #{resp["number"]}: {resp["html_url"]}')
except urllib.error.HTTPError as e:
    body_resp = e.read()
    print(f'HTTPError {e.code}: {body_resp}')
