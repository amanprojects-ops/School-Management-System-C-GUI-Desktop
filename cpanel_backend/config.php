<?php
/**
 * @file config.php
 * @brief cPanel serverless sync API configuration loader & PDO factory.
 *
 * Production credentials should be set in cpanel_backend/config.local.php
 * (gitignored) or environment variables (SCHOOLCMS_*).
 */

declare(strict_types=1);

$localConfigPath = __DIR__ . '/config.local.php';
if (is_file($localConfigPath)) {
    $loaded = require $localConfigPath;
    if (is_array($loaded)) {
        foreach ($loaded as $k => $v) {
            if (!defined($k)) {
                define($k, $v);
            }
        }
    }
}

$envOrDefault = static function (string $name, string $default): string {
    if (defined($name)) {
        return (string)constant($name);
    }
    $val = getenv($name);
    return ($val !== false && $val !== '') ? $val : $default;
};

define('SYNC_DB_DSN', $envOrDefault('SCHOOLCMS_DB_DSN', ''));
define('SYNC_DB_HOST', $envOrDefault('SCHOOLCMS_DB_HOST', 'localhost'));
define('SYNC_DB_PORT', $envOrDefault('SCHOOLCMS_DB_PORT', '3306'));
define('SYNC_DB_NAME', $envOrDefault('SCHOOLCMS_DB_NAME', 'schoolcms_sync'));
define('SYNC_DB_USER', $envOrDefault('SCHOOLCMS_DB_USER', 'schoolcms_user'));
define('SYNC_DB_PASS', $envOrDefault('SCHOOLCMS_DB_PASS', ''));

// Bearer Device API Token configured identically in Windows Desktop & Android Settings
define('SYNC_API_TOKEN', $envOrDefault('SCHOOLCMS_API_TOKEN', 'CHANGE_ME_GENERATE_STRONG_64_CHAR_TOKEN'));

// Optional HMAC-SHA256 shared secret for X-Sync-Signature / X-Sync-Timestamp verification
define('SYNC_HMAC_SECRET', $envOrDefault('SCHOOLCMS_HMAC_SECRET', ''));

// Maximum clock skew (seconds) allowed when HMAC signature headers are supplied
define('SYNC_HMAC_MAX_SKEW_SEC', (int)$envOrDefault('SCHOOLCMS_HMAC_SKEW_SEC', '300'));

// Pull batch safety ceiling (keeps cPanel Entry Process execution sub-100ms)
define('SYNC_MAX_PULL_LIMIT', 200);
define('SYNC_MAX_PUSH_BATCH', 250);

/**
 * Open a transactional PDO connection (MySQL in cPanel production, or explicit DSN for tests).
 */
function sync_get_pdo(): PDO
{
    static $pdo = null;
    if ($pdo instanceof PDO) {
        return $pdo;
    }

    $opts = [
        PDO::ATTR_ERRMODE            => PDO::ERRMODE_EXCEPTION,
        PDO::ATTR_DEFAULT_FETCH_MODE => PDO::FETCH_ASSOC,
        PDO::ATTR_EMULATE_PREPARES   => false,
    ];

    if (SYNC_DB_DSN !== '') {
        $pdo = new PDO(SYNC_DB_DSN, SYNC_DB_USER, SYNC_DB_PASS, $opts);
        if ($pdo->getAttribute(PDO::ATTR_DRIVER_NAME) === 'sqlite') {
            $pdo->exec('PRAGMA busy_timeout = 5000;');
        }
        return $pdo;
    }

    $dsn = sprintf(
        'mysql:host=%s;port=%s;dbname=%s;charset=utf8mb4',
        SYNC_DB_HOST,
        SYNC_DB_PORT,
        SYNC_DB_NAME
    );

    $pdo = new PDO($dsn, SYNC_DB_USER, SYNC_DB_PASS, $opts);
    $pdo->exec("SET NAMES utf8mb4 COLLATE utf8mb4_unicode_ci;");
    $pdo->exec("SET time_zone = '+00:00';");
    return $pdo;
}
