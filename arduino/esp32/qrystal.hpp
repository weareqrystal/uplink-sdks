/**
 * Qrystal Uplink SDKs
 * Official SDKs for Qrystal Uplink - device monitoring and heartbeat service <https://uplink.qrystal.partners/>.
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025 Qrystal Uplink, Qrystal Partners, Mikayel Grigoryan
 * <https://uplink.qrystal.partners/>
 */

#ifndef QRYSTAL_H
#define QRYSTAL_H

#include <Arduino.h>
#include <WiFi.h>
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_sntp.h"
#include "esp_log.h"
#include "esp_wifi.h"

static const char *TAG = "qrystal_uplink";
static const uint32_t YEAR_2026_EPOCH = 1767244149;

class Qrystal
{
private:
  static esp_http_client_handle_t client;
  static String cachedCredentials;

  static void reset_client()
  {
    if (client != NULL)
    {
      esp_http_client_cleanup(client);
      client = NULL;
    }
    cachedCredentials.clear();
  }

public:
  typedef enum
  {
    Q_OK = 0x0,
    Q_QRYSTAL_ERR,
    Q_ERR_NO_WIFI,
    Q_ERR_TIME_NOT_READY,
    Q_ERR_INVALID_CREDENTIALS,
    Q_ERR_INVALID_DID,
    Q_ERR_INVALID_TOKEN,
    Q_ESP_HTTP_INIT_FAILED,
    Q_ESP_HTTP_ERROR
  } QRYSTAL_STATE;

  static QRYSTAL_STATE uplink_blocking(const String &credentials)
  {
    static bool timeReady = false;
    static uint32_t lastSyncTime = 0;

    // Step 1: Verify WiFi connectivity using ESP-IDF API
    wifi_ap_record_t ap_info;
    if (esp_wifi_sta_get_ap_info(&ap_info) != ESP_OK)
    {
      return Q_ERR_NO_WIFI;
    }

    // Step 2: Verify time synchronization with staleness detection
    if (!timeReady)
    {
      if (sntp_get_sync_status() != SNTP_SYNC_STATUS_COMPLETED)
      {
        static bool sntpInitialized = false;
        if (!sntpInitialized)
        {
          ESP_LOGI(TAG, "Configuring SNTP");
          esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
          esp_sntp_setservername(0, "pool.ntp.org");
          esp_sntp_init();
          sntpInitialized = true;
        }
        ESP_LOGW(TAG, "SNTP sync not completed, waiting for time sync");
        return Q_ERR_TIME_NOT_READY;
      }

      uint32_t sec, usec;
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
      // Check for time staleness (>24h) or clock adjustments
      uint32_t sec, usec;
      sntp_get_system_time(&sec, &usec);
      if (sec < lastSyncTime || (sec - lastSyncTime) > 86400)
      {
        ESP_LOGW(TAG, "Time sync stale or clock adjusted - forcing re-sync (current: %lu, last: %lu)", sec, lastSyncTime);
        timeReady = false;
        return Q_ERR_TIME_NOT_READY;
      }
    }

    // Step 3: Validate credentials
    if (credentials.isEmpty())
    {
      ESP_LOGE(TAG, "Empty credentials provided");
      return Q_ERR_INVALID_CREDENTIALS;
    }

    // Step 4: Initialize/Update HTTP Client
    if (client == NULL || credentials != cachedCredentials)
    {
      int splitIndex = credentials.indexOf(':');
      if (splitIndex == -1 || splitIndex == 0)
      {
        ESP_LOGE(TAG, "Invalid credentials format - missing or misplaced ':' separator");
        return Q_ERR_INVALID_CREDENTIALS;
      }

      String deviceId = credentials.substring(0, splitIndex);
      if (deviceId.length() < 10 || deviceId.length() > 40)
      {
        ESP_LOGE(TAG, "Invalid device ID length: %d (expected 10-40)", deviceId.length());
        return Q_ERR_INVALID_DID;
      }

      String token = credentials.substring(splitIndex + 1);
      if (token.length() < 5)
      {
        ESP_LOGE(TAG, "Invalid token length: %d (expected >= 5)", token.length());
        return Q_ERR_INVALID_TOKEN;
      }

      if (client == NULL)
      {
        esp_http_client_config_t cfg = {
            .url = "https://on.uplink.qrystal.partners/api/v1/heartbeat",
            .crt_bundle_attach = esp_crt_bundle_attach,
            .keep_alive_enable = true,
            .keep_alive_idle = 5,
            .keep_alive_interval = 5,
            .keep_alive_count = 3,
        };

        client = esp_http_client_init(&cfg);
        if (!client)
        {
          ESP_LOGE(TAG, "Failed to initialize HTTP client");
          return Q_ESP_HTTP_INIT_FAILED;
        }
        esp_http_client_set_method(client, HTTP_METHOD_POST);
      }

      esp_http_client_set_header(client, "X-Qrystal-Uplink-DID", deviceId.c_str());
      esp_http_client_set_header(client, "Authorization", ("Bearer " + token).c_str());
      cachedCredentials = credentials;
    }

    // Step 5: Send HTTP Request
    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK)
    {
      int http_code = esp_http_client_get_status_code(client);
      if (http_code >= 200 && http_code < 300)
      {
        return Q_OK;
      }
      ESP_LOGE(TAG, "Server returned HTTP %d", http_code);
      return Q_QRYSTAL_ERR;
    }
    else
    {
      ESP_LOGE(TAG, "HTTP request failed: %s (0x%x)", esp_err_to_name(err), err);
      reset_client();
      return Q_ESP_HTTP_ERROR;
    }
  }
};

// Define static members
esp_http_client_handle_t Qrystal::client = NULL;
String Qrystal::cachedCredentials = "";

#endif