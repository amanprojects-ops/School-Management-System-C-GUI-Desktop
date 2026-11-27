# SchoolCMS (APCOACHING) — cPanel Serverless Sync Backend

This folder provides a lightweight **PHP 8+ & MySQL/MariaDB** REST sync backend designed specifically for standard shared **cPanel** hosting (no SSH daemon, no Docker, no external MySQL port `3306` required).

## Directory Structure

```text
cpanel_backend/
├── .htaccess               # Security headers, hides .sql/config, forwards Bearer auth headers
├── schema.sql              # MySQL 5.7+ / MariaDB 10.3+ schema (all 19 synced tables + changelog)
├── config.php              # Env/config loader & PDO connection factory
├── config.local.sample.php # Template for your private DB credentials & API tokens
├── lib/
│   └── sync_engine.php     # Conflict engine (LWW, append-only fee ledger, tombstones, anti-echo)
└── api/
    ├── health.php          # GET: DB connectivity & schema readiness check
    ├── push.php            # POST: Atomic batched delta ingestion
    └── pull.php            # GET: Cursor-based (?since=seq) delta feed (204 when idle)
```

---

## Step-by-Step cPanel Deployment Guide

### 1. Create MySQL Database & User (cPanel → MySQL® Databases)
1. Create a new database, e.g. `yourcpanel_schoolcms`.
2. Create a database user with a strong password, e.g. `yourcpanel_sync`.
3. Add the user to the database with **ALL PRIVILEGES**.
   *(Remote MySQL / port `3306` remains closed externally — only `localhost` PHP talks to MySQL.)*

### 2. Import Schema (`phpMyAdmin`)
1. Open **cPanel → phpMyAdmin** and select your new database (`yourcpanel_schoolcms`).
2. Click the **Import** tab and upload `cpanel_backend/schema.sql`.
3. Verify that all 22 tables (`sync_devices`, `sync_changelog`, `sync_tombstones`, plus the 19 operational tables) are created cleanly with `utf8mb4_unicode_ci`.

### 3. Generate Strong Device Token (and Optional HMAC Secret)
On any machine with PHP or Python (or an offline password generator), create a 64-character hex token:
```bash
php -r "echo bin2hex(random_bytes(32)) . PHP_EOL;"
```

### 4. Configure & Upload via cPanel File Manager
1. Create a subdomain or folder in cPanel (recommended with AutoSSL HTTPS enabled), e.g. `https://sync.yourschool.com/` (pointing to `public_html/sync` or dedicated subdomain root).
2. Upload the contents of `cpanel_backend/` into that folder (including the hidden `.htaccess` file — enable **"Show Hidden Files (dotfiles)"** in cPanel File Manager Settings!).
3. Copy `config.local.sample.php` to `config.local.php` on the server and set:
   - `SCHOOLCMS_DB_NAME`, `SCHOOLCMS_DB_USER`, `SCHOOLCMS_DB_PASS`
   - `SCHOOLCMS_API_TOKEN` (same token you will paste in Windows Desktop & Android Settings)
   - Optional: `SCHOOLCMS_HMAC_SECRET` for dual-factor `X-Sync-Signature` verification.

### 5. Verify Endpoints
Use Windows Desktop **Settings → Cloud Sync → Test Sync Connection** (or `curl`):
```bash
curl -H "Authorization: Bearer YOUR_64_CHAR_TOKEN" \
     -H "X-Device-Id: DEV-test01" \
     "https://sync.yourschool.com/api/health.php"
```
Expected response (`200 OK`):
```json
{"ok":true,"service":"SchoolCMS cPanel Delta Sync API","schema_ver":3,...}
```
