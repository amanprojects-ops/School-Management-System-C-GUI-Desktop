<?php
/**
 * @file api/health.php
 * @brief Authenticated database & schema readiness probe for Desktop/Android "Test Connection".
 */

declare(strict_types=1);

require_once __DIR__ . '/../config.php';
require_once __DIR__ . '/../lib/sync_engine.php';

$deviceId = sync_require_auth('', 'GET');

try {
    $pdo = sync_get_pdo();
    $latestSeq = (int)$pdo->query('SELECT COALESCE(MAX(`server_seq`), 0) FROM `sync_changelog`')->fetchColumn();
    $deviceCount = (int)$pdo->query('SELECT COUNT(*) FROM `sync_devices`')->fetchColumn();

    sync_json_response(200, [
        'ok'          => true,
        'service'     => 'SchoolCMS cPanel Delta Sync API',
        'schema_ver'  => 3,
        'php_version' => PHP_VERSION,
        'db_driver'   => (string)$pdo->getAttribute(PDO::ATTR_DRIVER_NAME),
        'latest_seq'  => $latestSeq,
        'devices'     => $deviceCount,
        'requester'   => $deviceId,
        'server_ms'   => (int)round(microtime(true) * 1000),
    ]);
} catch (Throwable $e) {
    error_log('[SchoolCMS Sync health.php] ' . $e->getMessage());
    sync_json_response(500, ['ok' => false, 'error' => 'Database connectivity or schema check failed']);
}
