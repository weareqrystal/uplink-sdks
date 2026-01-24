/**
 * Qrystal Uplink SDKs
 * Official SDKs for Qrystal Uplink - device monitoring and heartbeat service <https://uplink.qrystal.partners/>.
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025 Qrystal Uplink, Qrystal Partners, Mikayel Grigoryan
 * <https://uplink.qrystal.partners/>
 *
 * @file qrystal.cpp
 * @brief Implementation of the Qrystal Uplink SDK for ESP32.
 *
 * This file contains the implementation of the Qrystal class methods
 * for sending heartbeat signals to the Qrystal Uplink server.
 *
 * @see qrystal.hpp for the public API documentation.
 */

#include <stdio.h>
#include <esp_sntp.h>
#include <esp_crt_bundle.h>
#include <esp_log.h>

#include "qrystal.hpp"

/** @brief Log tag for ESP_LOG* macros */
static const char *TAG = "qrystal_uplink";

/**
 * @brief Minimum valid epoch timestamp (Jan 1, 2026 09:09:09 UTC+4).
 *
 * Used as a sanity check to ensure SNTP has actually synchronized the clock
 * to a reasonable value. This prevents accepting obviously incorrect times
 * that could cause issues with server authentication.
 */
static const uint32_t YEAR_2026_EPOCH = 1767244149;

/*
 * Static member definitions.
 * These maintain state across calls for connection reuse and credential caching.
 */
std::string Qrystal::credentials_cache;
esp_http_client_handle_t Qrystal::client = nullptr;

/* Non-blocking uplink state */
TaskHandle_t Qrystal::uplink_task_handle = nullptr;
std::atomic<bool> Qrystal::uplink_task_stop_flag{false};
qrystal_uplink_config_t Qrystal::uplink_config = QRYSTAL_UPLINK_CONFIG_DEFAULT();

Qrystal::QRYSTAL_STATE Qrystal::uplink_blocking(const std::string &credentials)
{
    /*
     * Track time synchronization state.
     * - timeReady: Set to true once SNTP sync is confirmed valid
     * - lastSyncTime: Used to detect stale time (>24h) or clock adjustments
     */
    static bool timeReady = false;
    static uint32_t lastSyncTime = 0;

    /*
     * =========================================================================
     * STEP 1: Verify WiFi Connectivity
     * =========================================================================
     * WiFi must be connected before attempting any network operations.
     * This is the first check because all subsequent operations require network.
     */
    wifi_ap_record_t ap_info;
    if (esp_wifi_sta_get_ap_info(&ap_info) != ESP_OK)
    {
        return Q_ERR_NO_WIFI;
    }

    /*
     * =========================================================================
     * STEP 2: Verify Time Synchronization
     * =========================================================================
     * Accurate time is required for:
     * - TLS certificate validation
     * - Server-side request timestamp verification
     *
     * We perform two levels of validation:
     * 1. SNTP sync status check (provided by ESP-IDF)
     * 2. Sanity check that time is after 2026 (when this SDK was written)
     */
    if (!timeReady)
    {
        /* Check if SNTP has completed synchronization */
        if (sntp_get_sync_status() != SNTP_SYNC_STATUS_COMPLETED)
        {
            /* Only initialize SNTP if not already running */
            if (!esp_sntp_enabled())
            {
                ESP_LOGW(TAG, "SNTP not initialized, starting SNTP");
                esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
                esp_sntp_setservername(0, "pool.ntp.org");
                esp_sntp_init();
            }

            /* Return error - caller should retry later (non-blocking approach) */
            return Q_ERR_TIME_NOT_READY;
        }

        /* Verify the synchronized time is reasonable (sanity check) */
        uint32_t sec, usec; // we need usec only to match function signature
        sntp_get_system_time(&sec, &usec);
        if (sec < YEAR_2026_EPOCH)
        {
            ESP_LOGW(TAG, "System time not yet valid (epoch: %lu, expected >= %lu)", sec, YEAR_2026_EPOCH);
            return Q_ERR_TIME_NOT_READY;
        }

        timeReady = true;
        lastSyncTime = sec;
    }
    else
    {
        /*
         * Time was previously synchronized - check for staleness.
         * Re-sync is required if:
         * - Clock has gone backwards (adjustment or rollover)
         * - More than 24 hours since last sync (drift prevention)
         */
        uint32_t sec, usec;
        sntp_get_system_time(&sec, &usec);
        if (sec < lastSyncTime || (sec - lastSyncTime) > 86400)
        {
            ESP_LOGW(TAG, "Time sync stale or clock adjusted - forcing re-sync (current: %lu, last: %lu)", sec, lastSyncTime);
            timeReady = false;
            return Q_ERR_TIME_NOT_READY;
        }
    }

    /*
     * =========================================================================
     * STEP 3: Validate Credentials Format
     * =========================================================================
     * Credentials must be in the format "deviceId:authToken".
     * Basic validation is performed here; the server performs stricter checks.
     */
    if (credentials.empty())
    {
        ESP_LOGE(TAG, "Empty credentials provided");
        return Q_ERR_INVALID_CREDENTIALS;
    }

    /*
     * =========================================================================
     * STEP 4: Initialize/Update HTTP Client
     * =========================================================================
     * The HTTP client is initialized once and reused for efficiency.
     * Re-initialization occurs when:
     * - First call (client == NULL)
     * - Credentials have changed
     * - Previous request failed (reset_client was called)
     */
    if (client == NULL || credentials != Qrystal::credentials_cache)
    {
        /* Parse credentials: "deviceId:authToken" */
        size_t splitIndex = credentials.find(':');
        if (splitIndex == std::string::npos || splitIndex == 0)
        {
            ESP_LOGE(TAG, "Invalid credentials format - missing or misplaced ':' separator");
            return Q_ERR_INVALID_CREDENTIALS;
        }

        /* Validate device ID length (permissive check, server validates strictly) */
        const std::string deviceId = credentials.substr(0, splitIndex);
        if (deviceId.length() < 10 || deviceId.length() > 40)
        {
            ESP_LOGE(TAG, "Invalid device ID length: %d (expected 10-40)", deviceId.length());
            return Q_ERR_INVALID_DID;
        }

        /* Validate token length (permissive check, server validates strictly) */
        const std::string token = credentials.substr(splitIndex + 1);
        if (token.length() < 5)
        {
            ESP_LOGE(TAG, "Invalid token length: %d (expected >= 5)", token.length());
            return Q_ERR_INVALID_TOKEN;
        }

        /* Initialize HTTP client if not already done */
        if (client == NULL)
        {
            /*
             * HTTP client configuration:
             * - Uses ESP certificate bundle for TLS
             * - Keep-alive enabled for connection reuse
             * - Aggressive keep-alive probes to detect dead connections quickly
             */
            esp_http_client_config_t cfg = {
                .url = "https://on.qrystaluplink.io/api/v1/heartbeat",
                .crt_bundle_attach = esp_crt_bundle_attach,
                .keep_alive_enable = true,
                .keep_alive_idle = 5,     /* Start probes after 5s idle */
                .keep_alive_interval = 5, /* Probe every 5s */
                .keep_alive_count = 3,    /* Close after 3 failed probes */
            };

            client = esp_http_client_init(&cfg);
            if (!client)
            {
                ESP_LOGE(TAG, "Failed to initialize HTTP client");
                return Q_ESP_HTTP_INIT_FAILED;
            }

            esp_http_client_set_method(client, HTTP_METHOD_POST);
        }

        /* Set authentication headers */
        esp_http_client_set_header(client, "X-Qrystal-Uplink-DID", deviceId.c_str());
        esp_http_client_set_header(client, "Authorization", ("Bearer " + token).c_str());
        credentials_cache = credentials;
    }

    /*
     * =========================================================================
     * STEP 5: Send HTTP Request
     * =========================================================================
     * Perform the actual heartbeat request to the server.
     * On connection reset errors (stale keep-alive), retry once with fresh connection.
     */
    esp_err_t state = esp_http_client_perform(client);

    /*
     * Handle stale connection errors by retrying with a fresh connection.
     * ESP_ERR_HTTP_WRITE_DATA (0x7003) and ESP_ERR_HTTP_CONNECT (0x7002) often
     * indicate the server closed an idle keep-alive connection.
     * Reset client and return error - caller can retry if needed.
     */
    if (state == ESP_ERR_HTTP_WRITE_DATA || state == ESP_ERR_HTTP_CONNECT)
    {
        ESP_LOGW(TAG, "Connection error (0x%x), resetting client for next attempt", state);
        reset_client();
        return Q_ESP_HTTP_ERROR;
    }

    if (state == ESP_OK)
    {
        int http_code = esp_http_client_get_status_code(client);
        if (http_code >= 200 && http_code < 300)
        {
            return Q_OK;
        }

        /* Server returned an error status code (4xx, 5xx) */
        ESP_LOGE(TAG, "Server returned HTTP %d", http_code);
        return Q_QRYSTAL_ERR;
    }
    else
    {
        /*
         * Network-level error occurred (connection reset, timeout, etc.)
         * Reset the client to force a fresh connection on the next attempt.
         */
        ESP_LOGE(TAG, "HTTP request failed: %s (0x%x)", esp_err_to_name(state), state);
        reset_client();
        return Q_ESP_HTTP_ERROR;
    }
}

/*
 * =============================================================================
 * NON-BLOCKING UPLINK IMPLEMENTATION
 * =============================================================================
 */

void Qrystal::uplink_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Non-blocking uplink task started (interval: %lu s)", uplink_config.interval_s);
    const std::string credentials(uplink_config.credentials);

    while (!uplink_task_stop_flag.load())
    {
        Qrystal::QRYSTAL_STATE result = Qrystal::uplink_blocking(credentials);
        /* Use shorter delays for time sync issues to retry quickly */
        uint32_t delay_s = uplink_config.interval_s;
        if (result == Q_ERR_TIME_NOT_READY)
        {
            delay_s = 2; /* Retry time sync quickly */
        }

        /* Invoke callback if provided */
        if (uplink_config.callback != nullptr)
        {
            uplink_config.callback(static_cast<int>(result), uplink_config.user_data);
        }

        /*
         * Break delay into smaller chunks to allow faster response to stop signal.
         * Check stop flag every second instead of sleeping for the entire interval.
         */
        uint32_t elapsed_s = 0;
        while (elapsed_s < delay_s && !uplink_task_stop_flag.load())
        {
            vTaskDelay(pdMS_TO_TICKS(1000));
            elapsed_s++;
        }
    }

    ESP_LOGI(TAG, "Non-blocking uplink task stopping");
    reset_client();

    /*
     * Clear handle before self-deleting. There's a small race window here,
     * but uplink_stop() handles it by saving the handle at entry and checking
     * if the task is still valid before deletion.
     */
    uplink_task_handle = nullptr;
    vTaskDelete(nullptr);
}

bool Qrystal::uplink(const qrystal_uplink_config_t *config)
{
    if (config == nullptr || config->credentials == nullptr)
    {
        ESP_LOGE(TAG, "Invalid config: credentials cannot be NULL");
        return false;
    }

    if (uplink_task_handle != nullptr)
    {
        ESP_LOGW(TAG, "Uplink task already running - call uplink_stop() first");
        return false;
    }

    /* Store configuration */
    uplink_config = *config;

    /* Apply defaults for unset values */
    if (uplink_config.interval_s == 0)
    {
        uplink_config.interval_s = 30;
    }
    if (uplink_config.stack_size == 0)
    {
        uplink_config.stack_size = 4096;
    }
    if (uplink_config.priority >= configMAX_PRIORITIES)
    {
        ESP_LOGW(TAG, "Priority %u exceeds max %d, clamping", uplink_config.priority, configMAX_PRIORITIES - 1);
        uplink_config.priority = configMAX_PRIORITIES - 1;
    }

    /* Reset stop flag before starting */
    uplink_task_stop_flag.store(false);

    /* Create the uplink task */
    BaseType_t result = xTaskCreate(
        uplink_task,
        TAG,
        uplink_config.stack_size,
        nullptr,
        uplink_config.priority,
        &uplink_task_handle);

    if (result != pdPASS)
    {
        ESP_LOGE(TAG, "Failed to create uplink task");
        uplink_task_handle = nullptr;
        return false;
    }

    return true;
}

void Qrystal::uplink_stop()
{
    TaskHandle_t task = uplink_task_handle;
    if (task == nullptr)
    {
        return;
    }

    ESP_LOGI(TAG, "Stopping uplink task...");
    uplink_task_stop_flag.store(true);

    /*
     * Wait for task to exit gracefully by clearing its handle.
     * We saved the handle above in case we need to force-delete.
     */
    int timeout_ms = 5000;
    while (uplink_task_handle != nullptr && timeout_ms > 0)
    {
        vTaskDelay(pdMS_TO_TICKS(50));
        timeout_ms -= 50;
    }

    if (uplink_task_handle != nullptr)
    {
        /*
         * Task didn't stop gracefully - force delete using saved handle.
         * This is safe because we saved 'task' before the loop.
         */
        ESP_LOGW(TAG, "Uplink task did not stop gracefully, force deleting");
        vTaskDelete(task);
        uplink_task_handle = nullptr;
        reset_client();
    }

    uplink_task_stop_flag.store(false);
    ESP_LOGI(TAG, "Uplink task stopped");
}

bool Qrystal::uplink_is_running()
{
    return uplink_task_handle != nullptr;
}
