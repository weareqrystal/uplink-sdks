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
#include "time.h"

class Qrystal {
private:
  // Static handle keeps the SSL connection alive between calls
  static esp_http_client_handle_t client;
  // Cache credentials to avoid re-parsing headers on every heartbeat
  static String cachedCredentials;

  static void reset_client() {
    if (client != NULL) {
      esp_http_client_cleanup(client);
      client = NULL;
    }
    cachedCredentials.clear();  // Force re-parse on next connect
  }

public:
  typedef enum {
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

  static QRYSTAL_STATE uplink_blocking(const String &credentials) {
    // static variable to track if time is ready
    static bool timeReady = false;
    // To track the last sync time for staleness checks
    static uint32_t lastSyncTime = 0;

    // Network Check
    if (WiFi.status() != WL_CONNECTED) {
      return Q_ERR_NO_WIFI;
    }

    // We assume if the year is correct, NTP is valid. We don't re-run configTime aggressively.
    time_t now = time(nullptr);
    struct tm timeinfo;
    gmtime_r(&now, &timeinfo);

    if (timeinfo.tm_year < (2026 - 1900)) {
      static bool sntpStarted = false;
      if (!sntpStarted) {
        configTime(0, 0, "pool.ntp.org", "time.nist.gov");
        sntpStarted = true;
      }
      return Q_ERR_TIME_NOT_READY;
    }

    // Initialize Client OR Re-configure if credentials changed
    // We enter this block if the connection is dead (NULL) OR if the user swapped keys.
    if (client == NULL || credentials != cachedCredentials) {
      if (credentials.isEmpty())
        return Q_ERR_INVALID_CREDENTIALS;

      // A. Parse Credentials
      int splitIndex = credentials.indexOf(':');
      if (splitIndex == -1)
        return Q_ERR_INVALID_CREDENTIALS;

      String deviceId = credentials.substring(0, splitIndex);
      if (deviceId.length() < 10 || deviceId.length() > 40)
        return Q_ERR_INVALID_DID;

      String token = credentials.substring(splitIndex + 1);
      if (token.length() < 5)
        return Q_ERR_INVALID_TOKEN;

      // B. Init Client (Singleton Pattern)
      if (client == NULL) {
        esp_http_client_config_t cfg = {
          .url = "https://qrystal-uplink-server-staging.up.railway.app/api/v1/heartbeat",
          .crt_bundle_attach = esp_crt_bundle_attach,
          .keep_alive_enable = true  // Keeps SSL session open
        };

        client = esp_http_client_init(&cfg);
        if (!client)
          return Q_ESP_HTTP_INIT_FAILED;
        // after client is created successfully set the method once
        esp_http_client_set_method(client, HTTP_METHOD_POST);
      }

      // C. Set Headers (Only needed once per connection/credential change)
      esp_http_client_set_header(client, "X-Qrystal-Uplink-DID", deviceId.c_str());

      // String concatenation creates a temp object, but only happens on init/change now.
      String authHeader = "Bearer " + token;
      esp_http_client_set_header(client, "Authorization", authHeader.c_str());

      // Update cache
      cachedCredentials = credentials;
    }

    // Perform Request (Fast Path)
    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
      int http_code = esp_http_client_get_status_code(client);
      // 200-299 is success
      if (http_code >= 200 && http_code < 300) {
        return Q_OK;
      }

      // If 401/403, credentials might be bad. We don't kill the socket, just report error.
      return Q_QRYSTAL_ERR;
    } else {
      // Connection Lost: Auto-Recovery
      // If the SSL tunnel died (WiFi glitch, timeout), we MUST cleanup.
      // Next call will see (client == NULL) and rebuild the connection.
      reset_client();
      return Q_ESP_HTTP_ERROR;
    }
  }
};

// Define static members
esp_http_client_handle_t Qrystal::client = NULL;
String Qrystal::cachedCredentials = "";

#endif