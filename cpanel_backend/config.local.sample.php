<?php
/**
 * Copy this file to cpanel_backend/config.local.php on your cPanel server
 * and fill in your cPanel MySQL database credentials and a strong random token.
 *
 * Example token generator:
 *   php -r "echo bin2hex(random_bytes(32)) . PHP_EOL;"
 */

return [
    'SCHOOLCMS_DB_HOST'   => 'localhost',
    'SCHOOLCMS_DB_PORT'   => '3306',
    'SCHOOLCMS_DB_NAME'   => 'cpaneluser_schoolcms',
    'SCHOOLCMS_DB_USER'   => 'cpaneluser_sync',
    'SCHOOLCMS_DB_PASS'   => 'REPLACE_WITH_STRONG_MYSQL_PASSWORD',

    // Must match the Device Token entered in Windows Desktop & Android App Settings
    'SCHOOLCMS_API_TOKEN' => 'REPLACE_WITH_64_CHAR_HEX_TOKEN',

    // Optional: set a separate 64-char secret to require X-Sync-Signature HMAC-SHA256 verification
    'SCHOOLCMS_HMAC_SECRET' => '',
];
