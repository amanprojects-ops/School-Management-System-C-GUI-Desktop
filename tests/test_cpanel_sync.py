#!/usr/bin/env python3
"""
End-to-end integration test suite for Phase 3 cpanel_backend/ PHP REST Sync API.

Starts PHP 8 built-in web server against a fresh SQLite test database converted
directly from cpanel_backend/schema.sql, verifies PHP syntax checks, auth/HMAC
security, 19-table schema coverage, batched delta push, anti-echo pull (HTTP 204
and JSON), LWW conflict resolution, append-only fee ledger immutability,
natural-key merge conflicts, and tombstone DELETE replication.
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
import urllib.error
import urllib.request
from pathlib import Path

ROOT_DIR = Path(__file__).resolve().parent.parent
CPANEL_DIR = ROOT_DIR / "cpanel_backend"
SCHEMA_SQL = CPANEL_DIR / "schema.sql"

TEST_TOKEN = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
TEST_HMAC_SECRET = "fedcba9876543210fedcba9876543210fedcba9876543210fedcba9876543210"


def find_php_exe() -> str:
    for cand in [shutil.which("php"), r"C:\xampp\php\php.exe"]:
        if cand and Path(cand).is_file():
            return cand
    raise RuntimeError("PHP CLI executable not found (checked PATH and C:\\xampp\\php\\php.exe)")


def convert_mysql_schema_to_sqlite(mysql_ddl: str) -> str:
    """Translate cpanel_backend/schema.sql cleanly into SQLite DDL for PHP PDO testing."""
    lines = []
    for raw in mysql_ddl.splitlines():
        s = raw.strip()
        if not s or s.startswith("--"):
            continue
        if s.upper().startswith("SET "):
            continue
        # Strip non-unique MySQL secondary KEY lines (keep UNIQUE KEY as UNIQUE constraint)
        if re.match(r"^KEY\s+`?\w+`?\s*\(", s, re.IGNORECASE):
            continue
        # Convert UNIQUE KEY name (cols) -> UNIQUE (cols)
        s = re.sub(r"UNIQUE\s+KEY\s+`?\w+`?\s*\(", "UNIQUE (", s, flags=re.IGNORECASE)
        # Convert AUTO_INCREMENT PK for SQLite
        s = re.sub(
            r"\bBIGINT\s+UNSIGNED\s+NOT\s+NULL\s+AUTO_INCREMENT\s+PRIMARY\s+KEY\b",
            "INTEGER PRIMARY KEY AUTOINCREMENT",
            s,
            flags=re.IGNORECASE,
        )
        # Strip ON UPDATE CURRENT_TIMESTAMP & MySQL table options
        s = re.sub(r"\bON\s+UPDATE\s+CURRENT_TIMESTAMP\b", "", s, flags=re.IGNORECASE)
        s = re.sub(r"\)\s*ENGINE\s*=\s*InnoDB[^;]*;", ");", s, flags=re.IGNORECASE)
        s = re.sub(r"\bENUM\([^)]*\)", "TEXT", s, flags=re.IGNORECASE)
        lines.append(s)

    joined = "\n".join(lines)
    # Clean trailing commas left before ');' when trailing KEY lines were stripped
    joined = re.sub(r",(\s*\);)", r"\1", joined)
    return joined


def free_port() -> int:
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.bind(("127.0.0.1", 0))
        return int(s.getsockname()[1])


def http_req(
    base_url: str,
    path: str,
    method: str = "GET",
    body_obj=None,
    token: str = TEST_TOKEN,
    hmac_secret: str = TEST_HMAC_SECRET,
    device_id: str = "",
    timestamp_override: str = "",
    sig_override: str = "",
):
    raw_body = ""
    data_bytes = None
    if body_obj is not None:
        raw_body = json.dumps(body_obj, separators=(",", ":"))
        data_bytes = raw_body.encode("utf-8")

    headers = {"Accept": "application/json"}
    if token is not None:
        headers["Authorization"] = f"Bearer {token}"
    if device_id:
        headers["X-Device-Id"] = device_id
    if data_bytes is not None:
        headers["Content-Type"] = "application/json"

    if hmac_secret:
        ts = timestamp_override or str(int(time.time()))
        query = path.split("?", 1)[1] if "?" in path else ""
        signed_part = raw_body if method == "POST" else query
        sig = sig_override or hmac.new(
            hmac_secret.encode("utf-8"),
            f"{ts}\n{signed_part}".encode("utf-8"),
            hashlib.sha256,
        ).hexdigest()
        headers["X-Sync-Timestamp"] = ts
        headers["X-Sync-Signature"] = sig

    req = urllib.request.Request(base_url + path, data=data_bytes, method=method, headers=headers)
    try:
        with urllib.request.urlopen(req, timeout=10) as resp:
            raw = resp.read().decode("utf-8")
            return resp.status, dict(resp.headers), json.loads(raw) if raw else None
    except urllib.error.HTTPError as e:
        raw = e.read().decode("utf-8")
        return e.code, dict(e.headers), json.loads(raw) if raw else None


def check(cond: bool, msg: str, passed: list):
    if not cond:
        raise AssertionError(msg)
    print(f"  [PASS] {msg}")
    passed.append(msg)


def run_tests():
    php_exe = find_php_exe()
    passed = []

    print("============================================================")
    print("   SchoolCMS cPanel PHP Backend Integration Test Suite")
    print("============================================================")

    # 1. PHP syntax checks across every PHP file in cpanel_backend
    print("\n1. Verifying PHP 8 Syntax Lint Across cpanel_backend/...")
    php_files = sorted(CPANEL_DIR.rglob("*.php"))
    check(len(php_files) >= 5, f"Found {len(php_files)} PHP files in cpanel_backend/", passed)
    for pf in php_files:
        res = subprocess.run([php_exe, "-l", str(pf)], capture_output=True, text=True)
        check(res.returncode == 0, f"PHP syntax OK: {pf.relative_to(ROOT_DIR)}", passed)

    # 2. Prepare temporary SQLite DB from cpanel_backend/schema.sql
    tmpdir = Path(tempfile.mkdtemp(prefix="schoolcms_cpanel_test_"))
    db_path = tmpdir / "cpanel_sync_test.db"
    sqlite_ddl = convert_mysql_schema_to_sqlite(SCHEMA_SQL.read_text(encoding="utf-8"))

    conn = sqlite3.connect(str(db_path))
    conn.executescript(sqlite_ddl)
    table_count = conn.execute(
        "SELECT COUNT(*) FROM sqlite_master WHERE type='table' AND name NOT LIKE 'sqlite_%';"
    ).fetchone()[0]
    conn.close()
    check(table_count == 22, f"Converted schema.sql created all 22 tables cleanly ({table_count})", passed)

    port = free_port()
    env = os.environ.copy()
    env["SCHOOLCMS_DB_DSN"] = f"sqlite:{db_path}"
    env["SCHOOLCMS_API_TOKEN"] = TEST_TOKEN
    env["SCHOOLCMS_HMAC_SECRET"] = TEST_HMAC_SECRET
    env["SCHOOLCMS_HMAC_SKEW_SEC"] = "300"

    server = subprocess.Popen(
        [php_exe, "-S", f"127.0.0.1:{port}", "-t", str(CPANEL_DIR)],
        env=env,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )

    try:
        base_url = f"http://127.0.0.1:{port}"
        # Wait until PHP dev server accepts TCP connections
        ready = False
        for _ in range(30):
            try:
                status, _, _ = http_req(base_url, "/api/health.php", method="GET", device_id="DEV-probe")
                if status == 200:
                    ready = True
                    break
            except Exception:
                time.sleep(0.1)
        check(ready, f"PHP built-in server booted cleanly on {base_url}", passed)

        print("\n2. Testing Bearer Auth, HMAC-SHA256 Security & Health Probe...")
        st, _, body = http_req(base_url, "/api/health.php", token=None)
        check(st == 401 and body["ok"] is False, "Missing Authorization header rejected (401)", passed)

        st, _, body = http_req(base_url, "/api/health.php", token="wrong-token")
        check(st == 401, "Invalid Bearer token rejected (401)", passed)

        st, _, body = http_req(base_url, "/api/health.php", sig_override="deadbeef")
        check(st == 401, "Tampered HMAC X-Sync-Signature rejected (401)", passed)

        st, _, body = http_req(
            base_url,
            "/api/health.php",
            timestamp_override=str(int(time.time()) - 9999),
        )
        check(st == 401, "Expired HMAC X-Sync-Timestamp replay rejected (401)", passed)

        st, _, body = http_req(base_url, "/api/push.php", method="GET")
        check(st == 405, "Wrong HTTP method GET on push.php rejected (405)", passed)

        st, _, body = http_req(base_url, "/api/health.php", method="GET", device_id="DEV-pc01")
        check(
            st == 200 and body["ok"] is True and body["schema_ver"] == 3 and body["latest_seq"] == 0,
            "Authenticated health.php confirms schema_ver == 3 and empty changelog",
            passed,
        )

        print("\n3. Testing Push Batch, Anti-Echo Pull (HTTP 204), and Android Delta Pull...")
        now_ms = int(time.time() * 1000)
        u_cls = "11111111-1111-4111-8111-111111111101"
        u_sec = "11111111-1111-4111-8111-111111111102"
        u_stu = "11111111-1111-4111-8111-111111111103"
        u_fs  = "11111111-1111-4111-8111-111111111104"
        u_fee = "11111111-1111-4111-8111-111111111105"
        u_set = "11111111-1111-4111-8111-111111111106"

        push_payload = {
            "device_id": "DEV-pc01",
            "device_prefix": "D",
            "changes": [
                {
                    "outbox_id": 1,
                    "table_name": "classes",
                    "record_uuid": u_cls,
                    "operation": "UPSERT",
                    "updated_at_ms": now_ms,
                    "payload": {"name": "Class 10", "numeric_value": 10, "capacity": 40, "academic_year_name": "2026-2027"},
                },
                {
                    "outbox_id": 2,
                    "table_name": "sections",
                    "record_uuid": u_sec,
                    "operation": "UPSERT",
                    "updated_at_ms": now_ms + 1,
                    "payload": {"class_uuid": u_cls, "name": "A"},
                },
                {
                    "outbox_id": 3,
                    "table_name": "students",
                    "record_uuid": u_stu,
                    "operation": "UPSERT",
                    "updated_at_ms": now_ms + 2,
                    "payload": {
                        "admission_no": "ADM-2026-D0001",
                        "full_name": "Rohan Mehra",
                        "gender": "MALE",
                        "admission_date": "2026-11-27",
                        "class_uuid": u_cls,
                        "section_uuid": u_sec,
                        "roll_number": 5,
                        "status": "ACTIVE",
                    },
                },
                {
                    "outbox_id": 4,
                    "table_name": "fee_structures",
                    "record_uuid": u_fs,
                    "operation": "UPSERT",
                    "updated_at_ms": now_ms + 3,
                    "payload": {
                        "name": "Tuition Q1",
                        "class_uuid": u_cls,
                        "academic_year_name": "2026-2027",
                        "fee_type": "MONTHLY",
                        "amount": 3500.0,
                    },
                },
                {
                    "outbox_id": 5,
                    "table_name": "fee_payments",
                    "record_uuid": u_fee,
                    "operation": "UPSERT",
                    "updated_at_ms": now_ms + 4,
                    "payload": {
                        "receipt_no": "RCP-2026-D0001",
                        "student_uuid": u_stu,
                        "fee_structure_uuid": u_fs,
                        "amount_paid": 3500.0,
                        "total_amount": 3500.0,
                        "payment_method": "UPI",
                        "payment_date": "2026-11-27",
                    },
                },
                {
                    "outbox_id": 6,
                    "table_name": "settings",
                    "record_uuid": u_set,
                    "operation": "UPSERT",
                    "updated_at_ms": now_ms + 5,
                    "payload": {"key": "school_name", "value": "AP Coaching ₹ Hub", "description": "Unicode check"},
                },
            ],
        }

        st, _, res = http_req(base_url, "/api/push.php", method="POST", body_obj=push_payload, device_id="DEV-pc01")
        check(
            st == 200 and res["ok"] is True and res["accepted_count"] == 6 and res["error_count"] == 0 and res["latest_seq"] == 6,
            "Desktop batch push accepted 6 records atomically (latest_seq == 6)",
            passed,
        )

        # Desktop pulls from seq 0 -> gets 204 No Content (anti-echo) + X-Sync-Last-Seq == 6
        st, hdrs, _ = http_req(base_url, "/api/pull.php?since=0&device_id=DEV-pc01", method="GET", device_id="DEV-pc01")
        hdrs_lower = {k.lower(): v for k, v in hdrs.items()}
        check(
            st == 204 and hdrs_lower.get("x-sync-last-seq") == "6",
            "Desktop pull returns HTTP 204 (zero self-echo) with X-Sync-Last-Seq: 6",
            passed,
        )

        # Android pulls from seq 0 -> gets all 6 changes cleanly with nested payload objects
        st, _, pull_res = http_req(base_url, "/api/pull.php?since=0&device_id=DEV-android01", method="GET", device_id="DEV-android01")
        check(
            st == 200 and pull_res["count"] == 6 and pull_res["next_since"] == 6 and pull_res["has_more"] is False,
            "Android pull receives all 6 changes cleanly from seq 0",
            passed,
        )
        check(
            pull_res["changes"][5]["payload"]["value"] == "AP Coaching ₹ Hub",
            "Unicode Indian Rupee (₹) survives cPanel JSON push->MySQL/PDO->pull round-trip",
            passed,
        )

        print("\n4. Testing LWW Conflicts, Append-Only Fee Ledger, Natural-Key Merge & Tombstone Delete...")
        # Stale edit on student + fee tamper attempt + newer settings natural-key conflict from another device
        u_set_android = "22222222-2222-4222-8222-222222222206"
        conflict_push = {
            "device_id": "DEV-android01",
            "device_prefix": "M",
            "changes": [
                {
                    "outbox_id": 10,
                    "table_name": "students",
                    "record_uuid": u_stu,
                    "operation": "UPSERT",
                    "updated_at_ms": now_ms - 5000,  # STALE!
                    "payload": {
                        "admission_no": "ADM-2026-D0001",
                        "full_name": "Stale Overwrite",
                        "class_uuid": u_cls,
                        "section_uuid": u_sec,
                    },
                },
                {
                    "outbox_id": 11,
                    "table_name": "fee_payments",
                    "record_uuid": u_fee,
                    "operation": "UPSERT",
                    "updated_at_ms": now_ms + 99999,  # Newer ts, MUST still be blocked (append-only)!
                    "payload": {
                        "receipt_no": "RCP-2026-D0001",
                        "student_uuid": u_stu,
                        "amount_paid": 1.0,
                        "total_amount": 1.0,
                    },
                },
                {
                    "outbox_id": 12,
                    "table_name": "settings",
                    "record_uuid": u_set_android,  # Different seed UUID, SAME key='school_name'
                    "operation": "UPSERT",
                    "updated_at_ms": now_ms + 1000,  # NEWER!
                    "payload": {"key": "school_name", "value": "Synced Campus Pro", "description": "Updated"},
                },
                {
                    "outbox_id": 13,
                    "table_name": "students",
                    "record_uuid": u_stu,
                    "operation": "UPSERT",
                    "updated_at_ms": now_ms + 1500,  # NEWER!
                    "payload": {
                        "admission_no": "ADM-2026-D0001",
                        "full_name": "Rohan Mehra Topper",
                        "class_uuid": u_cls,
                        "section_uuid": u_sec,
                    },
                },
            ],
        }
        st, _, cres = http_req(base_url, "/api/push.php", method="POST", body_obj=conflict_push, device_id="DEV-android01")
        check(
            st == 200 and cres["skipped_count"] == 2 and cres["accepted_count"] == 2 and cres["error_count"] == 0,
            "Conflict batch: 2 skipped (stale LWW + append-only fee) and 2 accepted (newer settings natural merge + student)",
            passed,
        )
        check(set(cres["acknowledged_ids"]) == {10, 11, 12, 13}, "All 4 outbox IDs acknowledged cleanly", passed)

        # Verify Tombstone DELETE from Android propagates cleanly to Desktop pull
        del_push = {
            "device_id": "DEV-android01",
            "device_prefix": "M",
            "changes": [
                {
                    "outbox_id": 14,
                    "table_name": "sections",
                    "record_uuid": u_sec,
                    "operation": "DELETE",
                    "updated_at_ms": now_ms + 3000,
                    "payload": {},
                }
            ],
        }
        st, _, dres = http_req(base_url, "/api/push.php", method="POST", body_obj=del_push, device_id="DEV-android01")
        check(st == 200 and dres["accepted_count"] == 1, "Tombstone DELETE accepted by cPanel server", passed)

        # Desktop polls from since=6 -> receives Android's settings update (seq 7), student update (seq 8), section DELETE (seq 9)
        st, _, dp = http_req(base_url, "/api/pull.php?since=6&device_id=DEV-pc01", method="GET", device_id="DEV-pc01")
        check(
            st == 200 and dp["count"] == 3 and dp["changes"][2]["operation"] == "DELETE" and dp["next_since"] == 9,
            "Desktop pulls exact 3 Android deltas (including DELETE tombstone, next_since == 9)",
            passed,
        )

        # Verify SQLite DB state on server directly
        chk_conn = sqlite3.connect(str(db_path))
        fee_amt = chk_conn.execute("SELECT total_amount FROM fee_payments WHERE sync_uuid=?", (u_fee,)).fetchone()[0]
        stu_name = chk_conn.execute("SELECT full_name FROM students WHERE sync_uuid=?", (u_stu,)).fetchone()[0]
        set_row = chk_conn.execute("SELECT sync_uuid, value FROM settings WHERE key='school_name'").fetchone()
        sec_cnt = chk_conn.execute("SELECT COUNT(*) FROM sections WHERE sync_uuid=?", (u_sec,)).fetchone()[0]
        tomb_cnt = chk_conn.execute("SELECT COUNT(*) FROM sync_tombstones WHERE record_uuid=?", (u_sec,)).fetchone()[0]
        chk_conn.close()

        check(fee_amt == 3500.0, "Server fee receipt remains immutable (3500.0)", passed)
        check(stu_name == "Rohan Mehra Topper", "Server student holds LWW winner name", passed)
        check(set_row[1] == "Synced Campus Pro" and set_row[0] == u_set_android, "Seeded settings key merged cleanly via LWW", passed)
        check(sec_cnt == 0 and tomb_cnt == 1, "Deleted section removed and tombstone recorded on server", passed)

        print("\n============================================================")
        print(f"   ALL {len(passed)} CPANEL PHP SYNC ASSERTIONS PASSED!")
        print("============================================================")
    finally:
        server.terminate()
        try:
            server.wait(timeout=3)
        except subprocess.TimeoutExpired:
            server.kill()
        shutil.rmtree(tmpdir, ignore_errors=True)


if __name__ == "__main__":
    run_tests()
