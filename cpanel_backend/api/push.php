<?php
/**
 * @file api/push.php
 * @brief Batched offline-first delta push endpoint for Desktop & Android clients.
 *
 * Accepts POST JSON:
 *   {"device_id":"DEV-...","device_prefix":"D","changes":[...]}
 */

declare(strict_types=1);

require_once __DIR__ . '/../config.php';
require_once __DIR__ . '/../lib/sync_engine.php';

$rawBody = file_get_contents('php://input');
if ($rawBody === false) {
    sync_json_response(400, ['ok' => false, 'error' => 'Failed to read request body']);
}

$headerDevId = sync_require_auth($rawBody, 'POST');

$data = json_decode($rawBody, true);
if (!is_array($data)) {
    sync_json_response(400, ['ok' => false, 'error' => 'Invalid JSON payload']);
}

$bodyDevId = preg_replace('/[^A-Za-z0-9._:-]/', '', trim((string)($data['device_id'] ?? ''))) ?? '';
$deviceId  = $bodyDevId !== '' ? substr($bodyDevId, 0, 64) : $headerDevId;
if ($deviceId === '') {
    sync_json_response(400, ['ok' => false, 'error' => 'device_id is required']);
}

$devicePrefix = (string)($data['device_prefix'] ?? 'D');
$changes      = $data['changes'] ?? null;
if (!is_array($changes)) {
    sync_json_response(400, ['ok' => false, 'error' => 'changes array is required']);
}
if (count($changes) > SYNC_MAX_PUSH_BATCH) {
    sync_json_response(400, ['ok' => false, 'error' => 'Batch exceeds maximum limit of ' . SYNC_MAX_PUSH_BATCH]);
}

try {
    $pdo = sync_get_pdo();
    $result = sync_process_push($pdo, $deviceId, $devicePrefix, $changes);
    sync_json_response(200, $result);
} catch (Throwable $e) {
    error_log('[SchoolCMS Sync push.php] ' . $e->getMessage());
    sync_json_response(500, ['ok' => false, 'error' => 'Internal server error processing sync push']);
}
