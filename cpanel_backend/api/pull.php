<?php
/**
 * @file api/pull.php
 * @brief Lightweight cursor-based delta pull endpoint (GET ?since=0&device_id=DEV-...&limit=100).
 *
 * Excludes requester's own origin_device_id to prevent sync loops and returns HTTP 204
 * (with X-Sync-Last-Seq header) when idle to minimize cPanel shared-hosting bandwidth.
 * Pass &empty=json to force a JSON 200 envelope on empty polls.
 */

declare(strict_types=1);

require_once __DIR__ . '/../config.php';
require_once __DIR__ . '/../lib/sync_engine.php';

$headerDevId = sync_require_auth('', 'GET');

$since = isset($_GET['since']) ? max(0, (int)$_GET['since']) : 0;
$limit = isset($_GET['limit']) ? (int)$_GET['limit'] : 100;
$emptyMode = strtolower(trim((string)($_GET['empty'] ?? '204')));

$queryDevId = preg_replace('/[^A-Za-z0-9._:-]/', '', trim((string)($_GET['device_id'] ?? ''))) ?? '';
$deviceId   = $queryDevId !== '' ? substr($queryDevId, 0, 64) : $headerDevId;

try {
    $pdo = sync_get_pdo();
    $result = sync_process_pull($pdo, $deviceId, $since, $limit);

    $headers = [
        'X-Sync-Last-Seq' => (string)$result['next_since'],
        'X-Sync-Has-More' => $result['has_more'] ? '1' : '0',
    ];

    if ($result['count'] === 0 && $emptyMode !== 'json') {
        http_response_code(204);
        foreach ($headers as $k => $v) {
            header($k . ': ' . $v);
        }
        exit;
    }

    sync_json_response(200, $result, $headers);
} catch (Throwable $e) {
    error_log('[SchoolCMS Sync pull.php] ' . $e->getMessage());
    sync_json_response(500, ['ok' => false, 'error' => 'Internal server error processing sync pull']);
}
