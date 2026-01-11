#ifndef QRYSTAL_H
#define QRYSTAL_H

#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include "esp_crt_bundle.h"
#include "time.h"

class Qrystal {
public:
    /**
     * SECURE Uplink.
     * Automatically syncs time and verifies SSL certificates.
     * safe for Data and OTA.
     */
    static void uplink(String credentials, String payload = "") {
        static bool timeConfigured = false;

        // 1. Safety Check
        if (WiFi.status() != WL_CONNECTED) return;

        // 2. AUTO TIME SYNC (The Magic Fix)
        // If we haven't synced time yet, do it now.
        if (!timeConfigured) {
            configTime(0, 0, "pool.ntp.org", "time.nist.gov");
            // Check if time is valid (year > 2016)
            time_t now = time(nullptr);
            struct tm timeinfo;
            gmtime_r(&now, &timeinfo);
            
            // If time is still 1970, we can't send securely yet.
            // For a blocking heartbeat, we might wait here, or return.
            // For MVP, let's just check if it worked.
            if (timeinfo.tm_year > (2016 - 1900)) {
                timeConfigured = true;
            } else {
                // Time not ready yet, skip this loop to avoid blocking 
                return; 
            }
        }

        // 3. Parse Credentials
        int splitIndex = credentials.indexOf(':');
        if (splitIndex == -1) return;
        String deviceId = credentials.substring(0, splitIndex);
        String token = credentials.substring(splitIndex + 1);

        // 4. SECURE Connection
        WiFiClientSecure client;
        client.setCACertBundle(rootca_crt_bundle); // Uses ESP32's built-in trusted roots

        HTTPClient http;
        if (http.begin(client, "https://on.uplink.qrystal.partners/api/v1/telemetry")) {
            http.addHeader("Authorization", "Bearer " + token);
            http.addHeader("X-Qrystal-Uplink-DID", deviceId);
            
            // Send payload if it exists (JSON), otherwise empty heartbeat
            if (payload.length() > 0) {
                 http.addHeader("Content-Type", "application/json");
                 http.POST(payload);
            } else {
                 http.POST("");
            }
            http.end();
        }
    }
};

#endif
