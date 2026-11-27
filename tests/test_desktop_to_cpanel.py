#!/usr/bin/env python3
"""
True end-to-end Desktop C11 WinHTTP <-> cPanel PHP 8 REST API bidirectional test.

1. Starts local PHP 8 server running cpanel_backend/ against an isolated SQLite DB.
2. Runs build/test_sync_engine.exe against http://127.0.0.1:<port> with Bearer + HMAC auth:
   - Tests health check via WinHTTP
   - Creates local Class, Section, Student on Desktop C11 and asserts background GThread
     automatically pushes all rows within ~1s debounce to PHP /api/push.php
3. Simulates Android client pushing a new student (ADM-2026-M0099) referencing Desktop's
   synced class_uuid + section_uuid.
4. Runs build/test_sync_engine.exe pull_verify and asserts Desktop WinHTTP pulls Android's
   student into local SQLite with resolved FK links and zero outbox echo.
"""

import hashlib
import hmac
import json
import os
import re
import shutil
import socket
import sqlite3
import subprocess
import sys
import tempfile
import time
import urllib.request
from pathlib import Path

ROOT_DIR = Path(__file__).resolve().parent.parent
CPANEL_DIR = ROOT_DIR / "cpanel_backend"
SCHEMA_SQL = CPANEL_DIR / "schema.sql"
EXE_PATH = ROOT_DIR / "build" / "test_sync_engine.exe"

TEST_TOKEN = "aa11bb22cc33dd44ee55ff6600112233445566778899aabbccddeeff00112233"
TEST_HMAC = "ffeeddccbbaa99887766554433221100ffeeddccbbaa99887766554433221100"


def find_php_exe() -> str:
    for cand in [shutil.which("php"), r"C:\xampp\php\php.exe"]:
        if cand and Path(cand).is_file():
            return cand
    raise RuntimeError("PHP executable not found")


def convert_mysql_schema_to_sqlite(mysql_ddl: str) -> str:
    lines = []
    for raw in mysql_ddl.splitlines():
        s = raw.strip()
        if not s or s.startswith("--") or s.upper().startswith("SET "):
            continue
        if re.match(r"^KEY\s+`?\w+`?\s*\(", s, re.IGNORECASE):
            continue
        s = re.sub(r"UNIQUE\s+KEY\s+`?\w+`?\s*\(", "UNIQUE (", s, flags=re.IGNORECASE)
        s = re.sub(
            r"\bBIGINT\s+UNSIGNED\s+NOT\s+NULL\s+AUTO_INCREMENT\s+PRIMARY\s+KEY\b",
            "INTEGER PRIMARY KEY AUTOINCREMENT",
            s,
            flags=re.IGNORECASE,
        )
        s = re.sub(r"\bON\s+UPDATE\s+CURRENT_TIMESTAMP\b", "", s, flags=re.IGNORECASE)
        s = re.sub(r"\)\s*ENGINE\s*=\s*InnoDB[^;]*;", ");", s, flags=re.IGNORECASE)
        s = re.sub(r"\bENUM\([^)]*\)", "TEXT", s, flags=re.IGNORECASE)
        lines.append(s)
    return re.sub(r",(\s*\);)", r"\1", "\n".join(lines))


def free_port() -> int:
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.bind(("127.0.0.1", 0))
        return int(s.getsockname()[1])


def post_android_delta(base_url: str, body_obj: dict) -> dict:
    raw = json.dumps(body_obj, separators=(",", ":"))
    ts = str(int(time.time()))
    sig = hmac.new(TEST_HMAC.encode(), f"{ts}\n{raw}".encode(), hashlib.sha256).hexdigest()
    req = urllib.request.Request(
        base_url + "/api/push.php",
        data=raw.encode("utf-8"),
        method="POST",
        headers={
            "Authorization": f"Bearer {TEST_TOKEN}",
            "Content-Type": "application/json",
            "X-Device-Id": "DEV-android-sim",
            "X-Sync-Timestamp": ts,
            "X-Sync-Signature": sig,
        },
    )
    with urllib.request.urlopen(req, timeout=10) as resp:
        return json.loads(resp.read().decode("utf-8"))


def main():
    if not EXE_PATH.is_file():
        raise RuntimeError(f"Missing compiled binary: {EXE_PATH}")

    php_exe = find_php_exe()
    tmpdir = Path(tempfile.mkdtemp(prefix="schoolcms_e2e_sync_"))
    srv_db = tmpdir / "cpanel_e2e.db"

    conn = sqlite3.connect(str(srv_db))
    conn.executescript(convert_mysql_schema_to_sqlite(SCHEMA_SQL.read_text(encoding="utf-8")))
    conn.close()

    port = free_port()
    env = os.environ.copy()
    env["SCHOOLCMS_DB_DSN"] = f"sqlite:{srv_db}"
    env["SCHOOLCMS_API_TOKEN"] = TEST_TOKEN
    env["SCHOOLCMS_HMAC_SECRET"] = TEST_HMAC

    server = subprocess.Popen(
        [php_exe, "-S", f"127.0.0.1:{port}", "-t", str(CPANEL_DIR)],
        env=env,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )

    try:
        base_url = f"http://127.0.0.1:{port}"
        time.sleep(0.5)

        # Phase A: Desktop C11 WinHTTP tests + GThread debounced push
        p1 = subprocess.run([str(EXE_PATH), base_url, TEST_TOKEN, TEST_HMAC], cwd=str(ROOT_DIR))
        assert p1.returncode == 0, "Desktop WinHTTP Phase A failed"

        # Inspect server DB to verify Desktop pushed Class 11, Section A, Kavya Singh Desktop
        chk = sqlite3.connect(str(srv_db))
        cls_uuid = chk.execute("SELECT sync_uuid FROM classes WHERE name='Class 11'").fetchone()[0]
        sec_uuid = chk.execute("SELECT sync_uuid FROM sections WHERE class_uuid=? AND name='A'", (cls_uuid,)).fetchone()[0]
        stu_name = chk.execute("SELECT full_name FROM students WHERE full_name='Kavya Singh Desktop'").fetchone()[0]
        chk.close()
        assert cls_uuid and sec_uuid and stu_name == "Kavya Singh Desktop"
        print(f"  [PASS] Server DB verified Desktop auto-pushed class={cls_uuid}, section={sec_uuid}, student='{stu_name}'")

        # Phase B: Simulated Android client pushes a new student referencing Desktop's Class 11 + Section A
        android_stu_uuid = "99999999-8888-4777-8666-555555555501"
        res = post_android_delta(
            base_url,
            {
                "device_id": "DEV-android-sim",
                "device_prefix": "M",
                "changes": [
                    {
                        "outbox_id": 1,
                        "table_name": "students",
                        "record_uuid": android_stu_uuid,
                        "operation": "UPSERT",
                        "updated_at_ms": int(time.time() * 1000) + 2000,
                        "payload": {
                            "admission_no": "ADM-2026-M0099",
                            "full_name": "Kabir Android Synced",
                            "gender": "MALE",
                            "admission_date": "2026-11-27",
                            "class_uuid": cls_uuid,
                            "section_uuid": sec_uuid,
                            "roll_number": 99,
                            "status": "ACTIVE",
                        },
                    }
                ],
            },
        )
        assert res["ok"] and res["accepted_count"] == 1
        print("  [PASS] Simulated Android pushed ADM-2026-M0099 to cPanel server")

        # Phase C: Desktop pulls Android student via WinHTTP and verifies FK + zero echo
        p2 = subprocess.run([str(EXE_PATH), "pull_verify"], cwd=str(ROOT_DIR))
        assert p2.returncode == 0, "Desktop WinHTTP pull_verify failed"

        print("\n>>> END-TO-END DESKTOP WINHTTP <-> CPANEL PHP SYNC VERIFIED CLEANLY! <<<")
    finally:
        server.terminate()
        try:
            server.wait(timeout=3)
        except subprocess.TimeoutExpired:
            server.kill()
        shutil.rmtree(tmpdir, ignore_errors=True)
        leftover_db = ROOT_DIR / "test_sync_engine.db"
        if leftover_db.exists():
            leftover_db.unlink()


if __name__ == "__main__":
    main()
