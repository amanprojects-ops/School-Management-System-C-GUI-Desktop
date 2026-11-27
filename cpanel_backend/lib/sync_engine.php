<?php
/**
 * @file lib/sync_engine.php
 * @brief Shared PDO delta sync engine for cPanel REST endpoints (MySQL & SQLite compatible).
 *
 * Enforces bearer token + optional HMAC-SHA256 verification, 19-table schema mapping,
 * append-only protection for fee_payments, Last-Write-Wins (LWW) resolution on
 * mutable entities, natural-key conflict matching, tombstone deletes, and anti-echo pull.
 */

declare(strict_types=1);

/**
 * Emit a JSON HTTP response and terminate cleanly.
 */
function sync_json_response(int $status, array $payload, array $extraHeaders = []): void
{
    http_response_code($status);
    header('Content-Type: application/json; charset=utf-8');
    header('X-Content-Type-Options: nosniff');
    foreach ($extraHeaders as $k => $v) {
        header($k . ': ' . $v);
    }
    echo json_encode($payload, JSON_UNESCAPED_UNICODE | JSON_UNESCAPED_SLASHES);
    exit;
}

/**
 * Extract Authorization Bearer token safely across Apache, LiteSpeed, and PHP-FPM.
 */
function sync_extract_bearer_token(): string
{
    $candidates = [
        $_SERVER['HTTP_AUTHORIZATION'] ?? '',
        $_SERVER['REDIRECT_HTTP_AUTHORIZATION'] ?? '',
    ];

    if (function_exists('getallheaders')) {
        $hdrs = getallheaders();
        if (is_array($hdrs)) {
            foreach ($hdrs as $k => $v) {
                if (strcasecmp((string)$k, 'Authorization') === 0) {
                    $candidates[] = (string)$v;
                }
            }
        }
    }

    foreach ($candidates as $raw) {
        if (preg_match('/^\s*Bearer\s+(.+)$/i', trim((string)$raw), $m)) {
            return trim($m[1]);
        }
    }

    if (!empty($_SERVER['HTTP_X_SYNC_TOKEN'])) {
        return trim((string)$_SERVER['HTTP_X_SYNC_TOKEN']);
    }

    return '';
}

/**
 * Authenticate Bearer token and optional HMAC-SHA256 signature.
 *
 * @return string Sanitized X-Device-Id header if supplied (may be empty).
 */
function sync_require_auth(string $rawBody = '', string $method = 'GET'): string
{
    if (($_SERVER['REQUEST_METHOD'] ?? '') !== $method) {
        sync_json_response(405, ['ok' => false, 'error' => 'Method not allowed; expected ' . $method]);
    }

    $expectedToken = (string)SYNC_API_TOKEN;
    if ($expectedToken === '' || $expectedToken === 'CHANGE_ME_GENERATE_STRONG_64_CHAR_TOKEN') {
        sync_json_response(500, ['ok' => false, 'error' => 'Server sync token is not configured']);
    }

    $provided = sync_extract_bearer_token();
    if ($provided === '' || !hash_equals($expectedToken, $provided)) {
        sync_json_response(401, ['ok' => false, 'error' => 'Unauthorized device token']);
    }

    if (SYNC_HMAC_SECRET !== '') {
        $tsHeader  = trim((string)($_SERVER['HTTP_X_SYNC_TIMESTAMP'] ?? ''));
        $sigHeader = strtolower(trim((string)($_SERVER['HTTP_X_SYNC_SIGNATURE'] ?? '')));

        if ($tsHeader === '' || !ctype_digit($tsHeader) || $sigHeader === '') {
            sync_json_response(401, ['ok' => false, 'error' => 'Missing HMAC signature headers']);
        }

        $reqTs = (int)$tsHeader;
        if (abs(time() - $reqTs) > SYNC_HMAC_MAX_SKEW_SEC) {
            sync_json_response(401, ['ok' => false, 'error' => 'Request timestamp outside allowed clock skew']);
        }

        $signedData = ($method === 'POST') ? $rawBody : (string)($_SERVER['QUERY_STRING'] ?? '');
        $expectedSig = hash_hmac('sha256', $tsHeader . "\n" . $signedData, SYNC_HMAC_SECRET);
        if (!hash_equals($expectedSig, $sigHeader)) {
            sync_json_response(401, ['ok' => false, 'error' => 'Invalid HMAC request signature']);
        }
    }

    $rawDev = (string)($_SERVER['HTTP_X_DEVICE_ID'] ?? ($_GET['device_id'] ?? ''));
    return substr(preg_replace('/[^A-Za-z0-9._:-]/', '', $rawDev) ?? '', 0, 64);
}

/**
 * Canonical column & conflict specifications for all 19 synced tables.
 * Each column entry: 'col_name' => ['type' => 'str'|'int'|'float'|'nullable_str'|'nullable_int', 'default' => mixed]
 */
function sync_get_table_specs(): array
{
    static $specs = null;
    if ($specs !== null) {
        return $specs;
    }

    $specs = [
        'roles' => [
            'append_only' => false,
            'cols' => [
                'name'        => ['type' => 'str',          'default' => ''],
                'description' => ['type' => 'nullable_str', 'default' => null],
            ],
            'natural' => [['name']],
        ],
        'users' => [
            'append_only' => false,
            'cols' => [
                'username'      => ['type' => 'str',          'default' => ''],
                'password_hash' => ['type' => 'str',          'default' => ''],
                'salt'          => ['type' => 'str',          'default' => ''],
                'role_uuid'     => ['type' => 'nullable_str', 'default' => null],
                'role_name'     => ['type' => 'nullable_str', 'default' => null],
                'full_name'     => ['type' => 'str',          'default' => ''],
                'email'         => ['type' => 'nullable_email', 'default' => null],
                'phone'         => ['type' => 'nullable_str', 'default' => null],
                'status'        => ['type' => 'str',          'default' => 'ACTIVE'],
                'created_at'    => ['type' => 'nullable_str', 'default' => null],
                'updated_at'    => ['type' => 'nullable_str', 'default' => null],
            ],
            'natural' => [['username'], ['email']],
        ],
        'academic_years' => [
            'append_only' => false,
            'cols' => [
                'name'       => ['type' => 'str', 'default' => ''],
                'start_date' => ['type' => 'str', 'default' => ''],
                'end_date'   => ['type' => 'str', 'default' => ''],
                'is_current' => ['type' => 'int', 'default' => 0],
            ],
            'natural' => [['name']],
        ],
        'classes' => [
            'append_only' => false,
            'cols' => [
                'name'               => ['type' => 'str',          'default' => ''],
                'numeric_value'      => ['type' => 'int',          'default' => 0],
                'capacity'           => ['type' => 'int',          'default' => 40],
                'academic_year_uuid' => ['type' => 'nullable_str', 'default' => null],
                'academic_year_name' => ['type' => 'nullable_str', 'default' => null],
            ],
            'natural' => [],
        ],
        'sections' => [
            'append_only' => false,
            'cols' => [
                'class_uuid'         => ['type' => 'str',          'default' => ''],
                'name'               => ['type' => 'str',          'default' => ''],
                'class_teacher_uuid' => ['type' => 'nullable_str', 'default' => null],
            ],
            'natural' => [['class_uuid', 'name']],
        ],
        'teachers' => [
            'append_only' => false,
            'cols' => [
                'teacher_code'  => ['type' => 'str',            'default' => ''],
                'user_uuid'     => ['type' => 'nullable_str',   'default' => null],
                'username'      => ['type' => 'nullable_str',   'default' => null],
                'full_name'     => ['type' => 'str',            'default' => ''],
                'phone'         => ['type' => 'str',            'default' => ''],
                'email'         => ['type' => 'nullable_email', 'default' => null],
                'address'       => ['type' => 'nullable_str',   'default' => null],
                'qualification' => ['type' => 'nullable_str',   'default' => null],
                'subject'       => ['type' => 'nullable_str',   'default' => null],
                'joining_date'  => ['type' => 'str',            'default' => '1970-01-01'],
                'salary'        => ['type' => 'float',          'default' => 0.0],
                'status'        => ['type' => 'str',            'default' => 'ACTIVE'],
                'created_at'    => ['type' => 'nullable_str',   'default' => null],
            ],
            'natural' => [['teacher_code'], ['email']],
        ],
        'staff' => [
            'append_only' => false,
            'cols' => [
                'staff_code'   => ['type' => 'str',            'default' => ''],
                'user_uuid'    => ['type' => 'nullable_str',   'default' => null],
                'username'     => ['type' => 'nullable_str',   'default' => null],
                'full_name'    => ['type' => 'str',            'default' => ''],
                'role_title'   => ['type' => 'str',            'default' => 'Staff'],
                'phone'        => ['type' => 'str',            'default' => ''],
                'email'        => ['type' => 'nullable_email', 'default' => null],
                'address'      => ['type' => 'nullable_str',   'default' => null],
                'joining_date' => ['type' => 'str',            'default' => '1970-01-01'],
                'salary'       => ['type' => 'float',          'default' => 0.0],
                'status'       => ['type' => 'str',            'default' => 'ACTIVE'],
                'created_at'   => ['type' => 'nullable_str',   'default' => null],
            ],
            'natural' => [['staff_code'], ['email']],
        ],
        'students' => [
            'append_only' => false,
            'cols' => [
                'admission_no'      => ['type' => 'str',          'default' => ''],
                'full_name'         => ['type' => 'str',          'default' => ''],
                'father_name'       => ['type' => 'nullable_str', 'default' => null],
                'mother_name'       => ['type' => 'nullable_str', 'default' => null],
                'dob'               => ['type' => 'nullable_str', 'default' => null],
                'gender'            => ['type' => 'str',          'default' => 'OTHER'],
                'category'          => ['type' => 'nullable_str', 'default' => null],
                'phone'             => ['type' => 'nullable_str', 'default' => null],
                'email'             => ['type' => 'nullable_str', 'default' => null],
                'address'           => ['type' => 'nullable_str', 'default' => null],
                'admission_date'    => ['type' => 'str',          'default' => '1970-01-01'],
                'class_uuid'        => ['type' => 'nullable_str', 'default' => null],
                'section_uuid'      => ['type' => 'nullable_str', 'default' => null],
                'roll_number'       => ['type' => 'nullable_int', 'default' => null],
                'previous_school'   => ['type' => 'nullable_str', 'default' => null],
                'emergency_contact' => ['type' => 'nullable_str', 'default' => null],
                'blood_group'       => ['type' => 'nullable_str', 'default' => null],
                'photo_path'        => ['type' => 'nullable_str', 'default' => null],
                'status'            => ['type' => 'str',          'default' => 'ACTIVE'],
                'created_at'        => ['type' => 'nullable_str', 'default' => null],
                'updated_at'        => ['type' => 'nullable_str', 'default' => null],
            ],
            'natural' => [['admission_no']],
        ],
        'subjects' => [
            'append_only' => false,
            'cols' => [
                'code'         => ['type' => 'str',          'default' => ''],
                'name'         => ['type' => 'str',          'default' => ''],
                'class_uuid'   => ['type' => 'str',          'default' => ''],
                'teacher_uuid' => ['type' => 'nullable_str', 'default' => null],
            ],
            'natural' => [['code', 'class_uuid']],
        ],
        'attendance' => [
            'append_only' => false,
            'cols' => [
                'student_uuid'         => ['type' => 'str',          'default' => ''],
                'class_uuid'           => ['type' => 'nullable_str', 'default' => null],
                'section_uuid'         => ['type' => 'nullable_str', 'default' => null],
                'date'                 => ['type' => 'str',          'default' => ''],
                'status'               => ['type' => 'str',          'default' => 'PRESENT'],
                'remarks'              => ['type' => 'nullable_str', 'default' => null],
                'recorded_by_uuid'     => ['type' => 'nullable_str', 'default' => null],
                'recorded_by_username' => ['type' => 'nullable_str', 'default' => null],
                'created_at'           => ['type' => 'nullable_str', 'default' => null],
            ],
            'natural' => [['student_uuid', 'date']],
        ],
        'fee_structures' => [
            'append_only' => false,
            'cols' => [
                'name'               => ['type' => 'str',          'default' => ''],
                'class_uuid'         => ['type' => 'nullable_str', 'default' => null],
                'academic_year_uuid' => ['type' => 'nullable_str', 'default' => null],
                'academic_year_name' => ['type' => 'nullable_str', 'default' => null],
                'fee_type'           => ['type' => 'str',          'default' => 'OTHER'],
                'amount'             => ['type' => 'float',        'default' => 0.0],
                'due_date'           => ['type' => 'nullable_str', 'default' => null],
            ],
            'natural' => [],
        ],
        'fee_payments' => [
            'append_only' => true, // Financial receipts are immutable once recorded
            'cols' => [
                'receipt_no'            => ['type' => 'str',          'default' => ''],
                'student_uuid'          => ['type' => 'str',          'default' => ''],
                'fee_structure_uuid'    => ['type' => 'nullable_str', 'default' => null],
                'amount_paid'           => ['type' => 'float',        'default' => 0.0],
                'discount'              => ['type' => 'float',        'default' => 0.0],
                'late_fine'             => ['type' => 'float',        'default' => 0.0],
                'total_amount'          => ['type' => 'float',        'default' => 0.0],
                'payment_method'        => ['type' => 'str',          'default' => 'CASH'],
                'transaction_ref'       => ['type' => 'nullable_str', 'default' => null],
                'payment_date'          => ['type' => 'str',          'default' => '1970-01-01'],
                'remarks'               => ['type' => 'nullable_str', 'default' => null],
                'collected_by_uuid'     => ['type' => 'nullable_str', 'default' => null],
                'collected_by_username' => ['type' => 'nullable_str', 'default' => null],
                'created_at'            => ['type' => 'nullable_str', 'default' => null],
            ],
            'natural' => [['receipt_no']],
        ],
        'examinations' => [
            'append_only' => false,
            'cols' => [
                'name'               => ['type' => 'str',          'default' => ''],
                'academic_year_uuid' => ['type' => 'nullable_str', 'default' => null],
                'academic_year_name' => ['type' => 'nullable_str', 'default' => null],
                'start_date'         => ['type' => 'str',          'default' => '1970-01-01'],
                'end_date'           => ['type' => 'str',          'default' => '1970-01-01'],
                'grading_system'     => ['type' => 'nullable_str', 'default' => 'PERCENTAGE'],
                'status'             => ['type' => 'str',          'default' => 'PLANNED'],
            ],
            'natural' => [],
        ],
        'exam_subjects' => [
            'append_only' => false,
            'cols' => [
                'exam_uuid'     => ['type' => 'str',   'default' => ''],
                'subject_uuid'  => ['type' => 'str',   'default' => ''],
                'exam_date'     => ['type' => 'str',   'default' => '1970-01-01'],
                'max_marks'     => ['type' => 'float', 'default' => 100.0],
                'passing_marks' => ['type' => 'float', 'default' => 35.0],
            ],
            'natural' => [['exam_uuid', 'subject_uuid']],
        ],
        'exam_results' => [
            'append_only' => false,
            'cols' => [
                'exam_subject_uuid' => ['type' => 'str',          'default' => ''],
                'student_uuid'      => ['type' => 'str',          'default' => ''],
                'marks_obtained'    => ['type' => 'float',        'default' => 0.0],
                'grade'             => ['type' => 'nullable_str', 'default' => null],
                'remarks'           => ['type' => 'nullable_str', 'default' => null],
            ],
            'natural' => [['exam_subject_uuid', 'student_uuid']],
        ],
        'timetables' => [
            'append_only' => false,
            'cols' => [
                'class_uuid'    => ['type' => 'str',          'default' => ''],
                'section_uuid'  => ['type' => 'str',          'default' => ''],
                'day_of_week'   => ['type' => 'str',          'default' => 'MONDAY'],
                'period_number' => ['type' => 'int',          'default' => 1],
                'start_time'    => ['type' => 'str',          'default' => '09:00'],
                'end_time'      => ['type' => 'str',          'default' => '09:45'],
                'subject_uuid'  => ['type' => 'str',          'default' => ''],
                'teacher_uuid'  => ['type' => 'nullable_str', 'default' => null],
                'room_number'   => ['type' => 'nullable_str', 'default' => null],
            ],
            'natural' => [['class_uuid', 'section_uuid', 'day_of_week', 'period_number']],
        ],
        'notifications' => [
            'append_only' => false,
            'cols' => [
                'user_uuid'  => ['type' => 'nullable_str', 'default' => null],
                'username'   => ['type' => 'nullable_str', 'default' => null],
                'title'      => ['type' => 'str',          'default' => ''],
                'message'    => ['type' => 'str',          'default' => ''],
                'is_read'    => ['type' => 'int',          'default' => 0],
                'created_at' => ['type' => 'nullable_str', 'default' => null],
            ],
            'natural' => [],
        ],
        'settings' => [
            'append_only' => false,
            'cols' => [
                'key'         => ['type' => 'str',          'default' => ''],
                'value'       => ['type' => 'str',          'default' => ''],
                'description' => ['type' => 'nullable_str', 'default' => null],
                'updated_at'  => ['type' => 'nullable_str', 'default' => null],
            ],
            'natural' => [['key']],
        ],
        'role_permissions' => [
            'append_only' => false,
            'cols' => [
                'role_uuid'   => ['type' => 'str',          'default' => ''],
                'role_name'   => ['type' => 'nullable_str', 'default' => null],
                'module_name' => ['type' => 'str',          'default' => ''],
                'can_view'    => ['type' => 'int',          'default' => 1],
                'can_edit'    => ['type' => 'int',          'default' => 0],
            ],
            'natural' => [['role_uuid', 'module_name'], ['role_name', 'module_name']],
        ],
    ];

    return $specs;
}

/**
 * Coerce incoming JSON payload values to strict schema column types.
 */
function sync_normalize_row(array $spec, array $payload, string $uuid, string $deviceId, int $updatedAtMs): array
{
    $row = [];
    foreach ($spec['cols'] as $col => $meta) {
        $hasKey = array_key_exists($col, $payload) && $payload[$col] !== null;
        $raw = $hasKey ? $payload[$col] : $meta['default'];

        switch ($meta['type']) {
            case 'int':
                $row[$col] = ($raw === null) ? 0 : (int)$raw;
                break;
            case 'nullable_int':
                $row[$col] = ($raw === null || $raw === '') ? null : (int)$raw;
                break;
            case 'float':
                $row[$col] = ($raw === null) ? 0.0 : (float)$raw;
                break;
            case 'nullable_email':
                $s = ($raw === null) ? '' : trim((string)$raw);
                $row[$col] = ($s === '') ? null : $s;
                break;
            case 'nullable_str':
                $row[$col] = ($raw === null) ? null : (string)$raw;
                break;
            case 'str':
            default:
                $row[$col] = ($raw === null) ? (string)$meta['default'] : (string)$raw;
                break;
        }
    }

    $row['sync_uuid'] = $uuid;
    $row['origin_device_id'] = $deviceId;
    $row['updated_at_ms'] = $updatedAtMs;
    return $row;
}

/**
 * Locate existing mirrored row either by sync_uuid or table natural unique keys.
 *
 * @return array|null ['sync_uuid' => string, 'updated_at_ms' => int, 'matched_by_uuid' => bool]
 */
function sync_find_existing_row(PDO $pdo, string $table, array $spec, string $uuid, array $row): ?array
{
    $stmt = $pdo->prepare("SELECT `sync_uuid`, COALESCE(`updated_at_ms`, 0) AS updated_at_ms FROM `{$table}` WHERE `sync_uuid` = ? LIMIT 1");
    $stmt->execute([$uuid]);
    $hit = $stmt->fetch();
    if ($hit) {
        return [
            'sync_uuid'       => (string)$hit['sync_uuid'],
            'updated_at_ms'   => (int)$hit['updated_at_ms'],
            'matched_by_uuid' => true,
        ];
    }

    foreach ($spec['natural'] as $keyGroup) {
        $clauses = [];
        $params = [];
        $valid = true;

        foreach ($keyGroup as $col) {
            $val = $row[$col] ?? null;
            if ($val === null || $val === '') {
                $valid = false;
                break;
            }
            $clauses[] = "`{$col}` = ?";
            $params[] = $val;
        }

        if (!$valid || count($clauses) === 0) {
            continue;
        }

        $sql = "SELECT `sync_uuid`, COALESCE(`updated_at_ms`, 0) AS updated_at_ms FROM `{$table}` WHERE "
             . implode(' AND ', $clauses) . " LIMIT 1";
        $nstmt = $pdo->prepare($sql);
        $nstmt->execute($params);
        $nhit = $nstmt->fetch();
        if ($nhit) {
            return [
                'sync_uuid'       => (string)$nhit['sync_uuid'],
                'updated_at_ms'   => (int)$nhit['updated_at_ms'],
                'matched_by_uuid' => false,
            ];
        }
    }

    return null;
}

/**
 * Upsert or update a record inside sync_devices using portable ANSI PDO SQL.
 */
function sync_touch_device(PDO $pdo, string $deviceId, string $devicePrefix = 'D'): void
{
    if ($deviceId === '') {
        return;
    }
    $prefix = substr(preg_replace('/[^A-Za-z0-9]/', '', strtoupper($devicePrefix)) ?? 'D', 0, 4);
    if ($prefix === '') {
        $prefix = 'D';
    }

    $chk = $pdo->prepare('SELECT `device_id` FROM `sync_devices` WHERE `device_id` = ?');
    $chk->execute([$deviceId]);
    if ($chk->fetch()) {
        $upd = $pdo->prepare('UPDATE `sync_devices` SET `device_prefix` = ?, `last_seen_at` = CURRENT_TIMESTAMP WHERE `device_id` = ?');
        $upd->execute([$prefix, $deviceId]);
    } else {
        $ins = $pdo->prepare('INSERT INTO `sync_devices` (`device_id`, `device_prefix`) VALUES (?, ?)');
        $ins->execute([$deviceId, $prefix]);
    }
}

/**
 * Append a normalized delta entry to sync_changelog and return its server_seq.
 */
function sync_append_changelog(PDO $pdo, string $table, string $uuid, string $op, string $deviceId, int $tsMs, array $payload): int
{
    $json = json_encode($payload, JSON_UNESCAPED_UNICODE | JSON_UNESCAPED_SLASHES);
    if ($json === false) {
        throw new RuntimeException('Failed to encode canonical changelog payload');
    }

    $stmt = $pdo->prepare(
        'INSERT INTO `sync_changelog` (`table_name`, `record_uuid`, `operation`, `origin_device_id`, `updated_at_ms`, `payload_json`) '
        . 'VALUES (?, ?, ?, ?, ?, ?)'
    );
    $stmt->execute([$table, $uuid, $op, $deviceId, $tsMs, $json]);
    return (int)$pdo->lastInsertId();
}

/**
 * Process a batched delta push from Windows Desktop or Android client atomically.
 */
function sync_process_push(PDO $pdo, string $deviceId, string $devicePrefix, array $changes): array
{
    $specs = sync_get_table_specs();
    $accepted = [];
    $skipped = [];
    $errors = [];
    $ackOutboxIds = [];

    $pdo->beginTransaction();
    try {
        sync_touch_device($pdo, $deviceId, $devicePrefix);

        foreach ($changes as $idx => $item) {
            if (!is_array($item)) {
                $errors[] = ['index' => $idx, 'error' => 'Malformed change entry'];
                continue;
            }

            $outboxId = isset($item['outbox_id']) ? (int)$item['outbox_id'] : 0;
            $table    = trim((string)($item['table_name'] ?? ''));
            $uuid     = trim((string)($item['record_uuid'] ?? ''));
            $op       = strtoupper(trim((string)($item['operation'] ?? 'UPSERT')));
            $tsMs     = isset($item['updated_at_ms']) ? (int)$item['updated_at_ms'] : (int)round(microtime(true) * 1000);
            $payload  = is_array($item['payload'] ?? null) ? $item['payload'] : [];

            if (!isset($specs[$table])) {
                $errors[] = ['outbox_id' => $outboxId, 'table_name' => $table, 'record_uuid' => $uuid, 'error' => 'Unsupported table_name'];
                continue;
            }
            if ($uuid === '' || strlen($uuid) > 40) {
                $errors[] = ['outbox_id' => $outboxId, 'table_name' => $table, 'record_uuid' => $uuid, 'error' => 'Invalid record_uuid'];
                continue;
            }
            if ($op !== 'UPSERT' && $op !== 'DELETE') {
                $errors[] = ['outbox_id' => $outboxId, 'table_name' => $table, 'record_uuid' => $uuid, 'error' => 'Invalid operation'];
                continue;
            }

            $spec = $specs[$table];

            if ($op === 'DELETE') {
                $stmt = $pdo->prepare("SELECT COALESCE(`updated_at_ms`, 0) FROM `{$table}` WHERE `sync_uuid` = ? LIMIT 1");
                $stmt->execute([$uuid]);
                $existingTs = $stmt->fetchColumn();

                if ($existingTs !== false && (int)$existingTs > $tsMs) {
                    $skipped[] = ['outbox_id' => $outboxId, 'table_name' => $table, 'record_uuid' => $uuid, 'reason' => 'lww_stale_delete'];
                    if ($outboxId > 0) $ackOutboxIds[] = $outboxId;
                    continue;
                }

                $del = $pdo->prepare("DELETE FROM `{$table}` WHERE `sync_uuid` = ?");
                $del->execute([$uuid]);

                // Record tombstone cleanly (portable check + upsert)
                $tChk = $pdo->prepare('SELECT `id` FROM `sync_tombstones` WHERE `table_name` = ? AND `record_uuid` = ? LIMIT 1');
                $tChk->execute([$table, $uuid]);
                if ($tRow = $tChk->fetch()) {
                    $tUpd = $pdo->prepare('UPDATE `sync_tombstones` SET `origin_device_id` = ?, `deleted_at_ms` = ? WHERE `id` = ?');
                    $tUpd->execute([$deviceId, $tsMs, (int)$tRow['id']]);
                } else {
                    $tIns = $pdo->prepare('INSERT INTO `sync_tombstones` (`table_name`, `record_uuid`, `origin_device_id`, `deleted_at_ms`) VALUES (?, ?, ?, ?)');
                    $tIns->execute([$table, $uuid, $deviceId, $tsMs]);
                }

                $seq = sync_append_changelog($pdo, $table, $uuid, 'DELETE', $deviceId, $tsMs, [
                    'sync_uuid'        => $uuid,
                    'origin_device_id' => $deviceId,
                    'updated_at_ms'    => $tsMs,
                ]);
                $accepted[] = ['outbox_id' => $outboxId, 'table_name' => $table, 'record_uuid' => $uuid, 'server_seq' => $seq];
                if ($outboxId > 0) $ackOutboxIds[] = $outboxId;
                continue;
            }

            // UPSERT flow
            $row = sync_normalize_row($spec, $payload, $uuid, $deviceId, $tsMs);
            $existing = sync_find_existing_row($pdo, $table, $spec, $uuid, $row);

            if ($existing !== null) {
                if (!empty($spec['append_only'])) {
                    $skipped[] = ['outbox_id' => $outboxId, 'table_name' => $table, 'record_uuid' => $uuid, 'reason' => 'append_only_immutable'];
                    if ($outboxId > 0) $ackOutboxIds[] = $outboxId;
                    continue;
                }

                if ($tsMs < $existing['updated_at_ms'] || ($tsMs === $existing['updated_at_ms'] && $existing['matched_by_uuid'])) {
                    $skipped[] = ['outbox_id' => $outboxId, 'table_name' => $table, 'record_uuid' => $uuid, 'reason' => 'lww_stale_or_duplicate'];
                    if ($outboxId > 0) $ackOutboxIds[] = $outboxId;
                    continue;
                }

                $setParts = [];
                $vals = [];
                foreach ($row as $col => $val) {
                    $setParts[] = "`{$col}` = ?";
                    $vals[] = $val;
                }
                $vals[] = $existing['sync_uuid'];

                $updSql = "UPDATE `{$table}` SET " . implode(', ', $setParts) . " WHERE `sync_uuid` = ?";
                $ustmt = $pdo->prepare($updSql);
                $ustmt->execute($vals);
            } else {
                $colNames = array_map(static fn(string $c): string => "`{$c}`", array_keys($row));
                $placeholders = array_fill(0, count($row), '?');
                $insSql = "INSERT INTO `{$table}` (" . implode(', ', $colNames) . ") VALUES (" . implode(', ', $placeholders) . ")";
                $istmt = $pdo->prepare($insSql);
                $istmt->execute(array_values($row));
            }

            $seq = sync_append_changelog($pdo, $table, $uuid, 'UPSERT', $deviceId, $tsMs, $row);
            $accepted[] = ['outbox_id' => $outboxId, 'table_name' => $table, 'record_uuid' => $uuid, 'server_seq' => $seq];
            if ($outboxId > 0) $ackOutboxIds[] = $outboxId;
        }

        $maxSeq = (int)$pdo->query('SELECT COALESCE(MAX(`server_seq`), 0) FROM `sync_changelog`')->fetchColumn();
        $pdo->commit();

        return [
            'ok'               => true,
            'accepted_count'   => count($accepted),
            'skipped_count'    => count($skipped),
            'error_count'      => count($errors),
            'latest_seq'       => $maxSeq,
            'acknowledged_ids' => $ackOutboxIds,
            'accepted'         => $accepted,
            'skipped'          => $skipped,
            'errors'           => $errors,
        ];
    } catch (Throwable $e) {
        if ($pdo->inTransaction()) {
            $pdo->rollBack();
        }
        throw $e;
    }
}

/**
 * Pull deltas with server_seq > since excluding the requester's own echoed origin_device_id.
 */
function sync_process_pull(PDO $pdo, string $deviceId, int $since, int $limit): array
{
    $safeLimit = max(1, min($limit, SYNC_MAX_PULL_LIMIT));
    $fetchLimit = $safeLimit + 1;

    sync_touch_device($pdo, $deviceId);

    $globalMaxSeq = (int)$pdo->query('SELECT COALESCE(MAX(`server_seq`), 0) FROM `sync_changelog`')->fetchColumn();

    if ($deviceId !== '') {
        $sql = 'SELECT `server_seq`, `table_name`, `record_uuid`, `operation`, `origin_device_id`, `updated_at_ms`, `payload_json` '
             . 'FROM `sync_changelog` WHERE `server_seq` > ? AND `origin_device_id` != ? '
             . 'ORDER BY `server_seq` ASC LIMIT ?';
        $stmt = $pdo->prepare($sql);
        $stmt->bindValue(1, $since, PDO::PARAM_INT);
        $stmt->bindValue(2, $deviceId, PDO::PARAM_STR);
        $stmt->bindValue(3, $fetchLimit, PDO::PARAM_INT);
        $stmt->execute();
    } else {
        $sql = 'SELECT `server_seq`, `table_name`, `record_uuid`, `operation`, `origin_device_id`, `updated_at_ms`, `payload_json` '
             . 'FROM `sync_changelog` WHERE `server_seq` > ? ORDER BY `server_seq` ASC LIMIT ?';
        $stmt = $pdo->prepare($sql);
        $stmt->bindValue(1, $since, PDO::PARAM_INT);
        $stmt->bindValue(2, $fetchLimit, PDO::PARAM_INT);
        $stmt->execute();
    }

    $rows = $stmt->fetchAll();
    $hasMore = (count($rows) > $safeLimit);
    if ($hasMore) {
        $rows = array_slice($rows, 0, $safeLimit);
    }

    $changes = [];
    $nextSince = $since;
    foreach ($rows as $r) {
        $seq = (int)$r['server_seq'];
        if ($seq > $nextSince) {
            $nextSince = $seq;
        }
        $decodedPayload = json_decode((string)$r['payload_json'], true);
        if (!is_array($decodedPayload)) {
            $decodedPayload = new stdClass();
        }

        $changes[] = [
            'server_seq'       => $seq,
            'table_name'       => (string)$r['table_name'],
            'record_uuid'      => (string)$r['record_uuid'],
            'operation'        => (string)$r['operation'],
            'origin_device_id' => (string)$r['origin_device_id'],
            'updated_at_ms'    => (int)$r['updated_at_ms'],
            'payload'          => $decodedPayload,
        ];
    }

    // If all changes up to globalMaxSeq have been scanned (no other-device changes remain),
    // advance cursor past any own-device changelog entries to eliminate redundant rescans.
    if (!$hasMore && $globalMaxSeq > $nextSince) {
        $nextSince = $globalMaxSeq;
    }

    return [
        'ok'         => true,
        'next_since' => $nextSince,
        'latest_seq' => $globalMaxSeq,
        'has_more'   => $hasMore,
        'count'      => count($changes),
        'changes'    => $changes,
    ];
}
