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
 *
 * @section requirements Requirements
 * - WiFi configured and connected
 * - SNTP configured (SDK will attempt initialization if not done)
 *
 * @section usage Basic Usage
 * @code
 * #include "qrystal.hpp"
 *
 * // In your main loop or task:
 * const std::string credentials = "your-device-id:your-auth-token";
 * Qrystal::QRYSTAL_STATE result = Qrystal::uplink_blocking(credentials);
 *
 * switch (result) {
 *     case Qrystal::Q_OK:
 *         // Heartbeat sent successfully
 *         break;
 *     case Qrystal::Q_ERR_NO_WIFI:
 *         // WiFi not connected, retry later
 *         break;
 *     case Qrystal::Q_ERR_TIME_NOT_READY:
 *         // Time not synchronized yet, retry later
 *         break;
 *     default:
 *         // Handle other errors
 *         break;
 * }
 * @endcode
 *
 * @copyright Copyright (c) 2026 Qrystal Uplink, Qrystal Partners, Mikayel Grigoryan
 * @license MIT License
 */

#ifndef QRYSTAL_UPLINK
#define QRYSTAL_UPLINK

#include <string>
#include <esp_http_client.h>
#include <esp_wifi.h>

/**
 * @class Qrystal
 * @brief Main SDK class for Qrystal Uplink functionality.
 *
 * This class provides static methods to send heartbeat signals to the Qrystal
 * Uplink server. It handles all the complexity of WiFi checks, time synchronization,
 * HTTP connection management, and error recovery internally.
 *
 * @note All methods are static - no instantiation required.
 * @note Thread-safety: Not thread-safe. Call from a single task only.
 */
class Qrystal
{
private:
    /** @brief Cached credentials to detect changes and avoid redundant re-initialization */
    static std::string credentials_cache;

    /** @brief Persistent HTTP client handle for connection reuse */
    static esp_http_client_handle_t client;

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
     * @note This is a blocking call. For non-blocking behavior, call from a FreeRTOS task.
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
};

#endif // QRYSTAL_UPLINK
