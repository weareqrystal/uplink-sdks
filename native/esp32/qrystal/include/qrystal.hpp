/**
 * Qrystal Uplink SDKs
 * Official SDKs for Qrystal Uplink - device monitoring and heartbeat service <https://uplink.qrystal.partners/>.
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025 Qrystal Uplink, Qrystal Partners, Mikayel Grigoryan
 * <https://uplink.qrystal.partners/>
 *
 * @file qrystal.hpp
 * @brief Qrystal Uplink SDK for ESP32 (ESP-IDF)
 *
 * This SDK provides a simple interface to send heartbeat signals to the Qrystal
 * Uplink server, enabling device monitoring and connectivity tracking for IoT devices.
 *
 * @section features Features
 * - Automatic WiFi connectivity checks
 * - SNTP time synchronization with staleness detection
 * - Persistent HTTP connection with keep-alive for efficiency
 * - Automatic connection recovery on network failures
 * - Credential validation and caching
 * - Non-blocking mode with background FreeRTOS task
 *
 * @section requirements Requirements
 * - WiFi configured and connected
 * - SNTP configured (SDK will attempt initialization if not done)
 *
 * @section usage Basic Usage
 * @code
 * #include "qrystal.hpp"
 *
 * // Non-blocking mode (recommended):
 * qrystal_uplink_config_t config = QRYSTAL_UPLINK_CONFIG_DEFAULT();
 * config.credentials = "your-device-id:your-auth-token";
 * config.interval_s = 30;
 * Qrystal::uplink(&config);
 *
 * // Or blocking mode:
 * Qrystal::QRYSTAL_STATE result = Qrystal::uplink_blocking("your-device-id:your-auth-token");
 * @endcode
 *
 * @copyright Copyright (c) 2026 Qrystal Uplink, Qrystal Partners, Mikayel Grigoryan
 * @license MIT License
 */

#ifndef QRYSTAL_UPLINK
#define QRYSTAL_UPLINK

#include <atomic>
#include <string>
#include <esp_http_client.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

/**
 * @brief Callback function type for non-blocking uplink operations.
 *
 * This callback is invoked when an asynchronous uplink operation completes.
 * It runs in the context of the uplink task, so keep callback execution brief.
 *
 * @param state The result of the uplink operation (Qrystal::QRYSTAL_STATE cast to int)
 * @param user_data User-provided context pointer passed to uplink()
 */
typedef void (*qrystal_uplink_callback_t)(int state, void *user_data);

/**
 * @brief Configuration for non-blocking uplink operations.
 */
typedef struct
{
    /** @brief Device credentials in "deviceId:authToken" format */
    const char *credentials;

    /** @brief Interval between heartbeats in seconds (default: 30) */
    uint32_t interval_s;

    /** @brief Optional callback invoked after each uplink attempt (can be NULL) */
    qrystal_uplink_callback_t callback;

    /** @brief User data passed to the callback (can be NULL) */
    void *user_data;

    /** @brief Stack size for the uplink task in bytes (default: 4096) */
    uint32_t stack_size;

    /** @brief Task priority (default: 5) */
    UBaseType_t priority;
} qrystal_uplink_config_t;

/**
 * @brief Default initializer for qrystal_uplink_config_t.
 *
 * Usage:
 * @code
 * qrystal_uplink_config_t config = QRYSTAL_UPLINK_CONFIG_DEFAULT();
 * config.credentials = "device-id:auth-token";
 * config.callback = my_callback;
 * @endcode
 */
#define QRYSTAL_UPLINK_CONFIG_DEFAULT() \
    {                                   \
        .credentials = NULL,            \
        .interval_s = 30,               \
        .callback = NULL,               \
        .user_data = NULL,              \
        .stack_size = 4096,             \
        .priority = 5}

/**
 * @class Qrystal
 * @brief Main SDK class for Qrystal Uplink functionality.
 *
 * This class provides static methods to send heartbeat signals to the Qrystal
 * Uplink server. It handles all the complexity of WiFi checks, time synchronization,
 * HTTP connection management, and error recovery internally.
 *
 * Two modes of operation are supported:
 * - **Blocking**: Use uplink_blocking() for manual control in your own task
 * - **Non-blocking**: Use uplink() to start a background task that sends heartbeats automatically
 *
 * @note All methods are static - no instantiation required.
 * @note Thread-safety: The blocking API is not thread-safe. The non-blocking API
 *       manages its own task and is safe to start/stop from any task.
 */
class Qrystal
{
private:
    /** @brief Cached credentials to detect changes and avoid redundant re-initialization */
    static std::string credentials_cache;

    /** @brief Persistent HTTP client handle for connection reuse */
    static esp_http_client_handle_t client;

    /** @brief Handle to the non-blocking uplink task */
    static TaskHandle_t uplink_task_handle;

    /** @brief Flag to signal the uplink task to stop (accessed atomically) */
    static std::atomic<bool> uplink_task_stop_flag;

    /** @brief Current configuration for non-blocking mode */
    static qrystal_uplink_config_t uplink_config;

    /**
     * @brief Cleans up the HTTP client and resets cached credentials.
     *
     * Called internally when connection errors occur or when credentials change.
     * This forces a fresh connection on the next uplink_blocking() call.
     */
    static void reset_client()
    {
        if (client)
        {
            esp_http_client_cleanup(client);
            client = nullptr;
        }

        credentials_cache.clear();
    }

    /**
     * @brief FreeRTOS task function for non-blocking uplink.
     *
     * This task runs continuously, sending heartbeats at the configured interval
     * and invoking the callback after each attempt.
     *
     * @param pvParameters Unused (configuration stored in static member)
     */
    static void uplink_task(void *pvParameters);

public:
    /**
     * @enum QRYSTAL_STATE
     * @brief Return codes for Qrystal SDK operations.
     *
     * These codes indicate the result of an uplink operation and help
     * diagnose issues with connectivity, credentials, or server communication.
     */
    typedef enum
    {
        /** @brief Success - heartbeat was sent and acknowledged by the server */
        Q_OK = 0x0,

        /** @brief Server returned an error (4xx/5xx HTTP status) */
        Q_QRYSTAL_ERR,

        /** @brief WiFi is not connected - ensure WiFi is configured and connected */
        Q_ERR_NO_WIFI,

        /** @brief System time not synchronized via SNTP - retry after a short delay */
        Q_ERR_TIME_NOT_READY,

        /** @brief Credentials string is empty or malformed (missing ':' separator) */
        Q_ERR_INVALID_CREDENTIALS,

        /** @brief Device ID length is invalid (must be 10-40 characters) */
        Q_ERR_INVALID_DID,

        /** @brief Auth token length is invalid (must be at least 5 characters) */
        Q_ERR_INVALID_TOKEN,

        /** @brief Failed to initialize the ESP HTTP client */
        Q_ESP_HTTP_INIT_FAILED,

        /** @brief HTTP request failed (network error, connection reset, timeout, etc.) */
        Q_ESP_HTTP_ERROR
    } QRYSTAL_STATE;

    /**
     * @brief Sends a blocking heartbeat to the Qrystal Uplink server.
     *
     * This function performs a complete uplink operation, including:
     * 1. Verifying WiFi connectivity
     * 2. Ensuring system time is synchronized via SNTP
     * 3. Validating and parsing credentials
     * 4. Sending the HTTP POST request to the server
     *
     * The function maintains a persistent HTTP connection for efficiency.
     * If the connection is lost, it will be automatically re-established
     * on the next call.
     *
     * @param credentials Device credentials in the format "deviceId:authToken"
     *                    - deviceId: 10-40 characters, obtained from Qrystal dashboard
     *                    - authToken: minimum 5 characters, obtained from Qrystal dashboard
     *
     * @return QRYSTAL_STATE indicating the result:
     *         - Q_OK: Heartbeat sent successfully
     *         - Q_ERR_NO_WIFI: WiFi not connected
     *         - Q_ERR_TIME_NOT_READY: SNTP sync pending (retry after ~1 second)
     *         - Q_ERR_INVALID_CREDENTIALS: Empty or malformed credentials
     *         - Q_ERR_INVALID_DID: Device ID length out of range
     *         - Q_ERR_INVALID_TOKEN: Token too short
     *         - Q_ESP_HTTP_INIT_FAILED: HTTP client initialization failed
     *         - Q_ESP_HTTP_ERROR: Network/connection error (will auto-recover on retry)
     *         - Q_QRYSTAL_ERR: Server rejected the request (check credentials)
     *
     * @note This is a blocking call. For non-blocking behavior, use uplink() instead.
     * @note Recommended call interval: 30-60 seconds for typical monitoring use cases.
     *
     * @code
     * // Example with error handling
     * void heartbeat_task(void *pvParameters) {
     *     const std::string creds = "my-device-id:my-secret-token";
     *
     *     while (true) {
     *         Qrystal::QRYSTAL_STATE state = Qrystal::uplink_blocking(creds);
     *
     *         if (state == Qrystal::Q_OK) {
     *             ESP_LOGI("app", "Heartbeat sent successfully");
     *         } else if (state == Qrystal::Q_ERR_TIME_NOT_READY) {
     *             ESP_LOGW("app", "Waiting for time sync...");
     *         } else {
     *             ESP_LOGE("app", "Heartbeat failed with code: %d", state);
     *         }
     *
     *         vTaskDelay(pdMS_TO_TICKS(30000)); // 30 seconds
     *     }
     * }
     * @endcode
     */
    static QRYSTAL_STATE uplink_blocking(const std::string &credentials);

    /**
     * @brief Starts a non-blocking background task that sends heartbeats automatically.
     *
     * This function creates a FreeRTOS task that continuously sends heartbeats
     * at the configured interval. The optional callback is invoked after each
     * attempt, allowing you to monitor status without blocking your main code.
     *
     * @param config Configuration structure specifying credentials, interval, and callback.
     *               Use QRYSTAL_UPLINK_CONFIG_DEFAULT() for sensible defaults.
     *
     * @return true if the task was started successfully
     * @return false if credentials are NULL, task creation failed, or a task is already running
     *
     * @note Call uplink_stop() to stop the background task.
     * @note Only one non-blocking uplink task can run at a time.
     *
     * @code
     * // Example: Start non-blocking uplink with callback
     * void on_uplink_complete(int state, void *user_data) {
     *     if (state == Qrystal::Q_OK) {
     *         ESP_LOGI("app", "Heartbeat sent successfully");
     *     } else {
     *         ESP_LOGW("app", "Heartbeat failed: %d", state);
     *     }
     * }
     *
     * void app_main() {
     *     // ... WiFi and SNTP setup ...
     *
     *     qrystal_uplink_config_t config = QRYSTAL_UPLINK_CONFIG_DEFAULT();
     *     config.credentials = "my-device-id:my-auth-token";
     *     config.interval_s = 30;  // 30 seconds
     *     config.callback = on_uplink_complete;
     *
     *     if (Qrystal::uplink(&config)) {
     *         ESP_LOGI("app", "Uplink task started");
     *     }
     *
     *     // Your main application continues here - uplink runs in background
     * }
     * @endcode
     */
    static bool uplink(const qrystal_uplink_config_t *config);

    /**
     * @brief Stops the non-blocking uplink background task.
     *
     * Signals the background task to stop and waits for it to terminate cleanly.
     * After this call returns, no more callbacks will be invoked and resources
     * are freed.
     *
     * @note Safe to call even if no task is running (will do nothing).
     * @note This function blocks until the task has stopped.
     *
     * @code
     * // Stop uplink when entering low-power mode
     * Qrystal::uplink_stop();
     * esp_wifi_stop();
     * esp_deep_sleep_start();
     * @endcode
     */
    static void uplink_stop();

    /**
     * @brief Checks if the non-blocking uplink task is currently running.
     *
     * @return true if the background task is active
     * @return false if no task is running
     */
    static bool uplink_is_running();
};

#endif // QRYSTAL_UPLINK
