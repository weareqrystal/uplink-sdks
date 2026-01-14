/**
 * Qrystal Uplink SDKs
 * Official SDKs for Qrystal Uplink - device monitoring and heartbeat service <https://uplink.qrystal.partners/>.
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025 Qrystal Uplink, Qrystal Partners, Mikayel Grigoryan
 * <https://uplink.qrystal.partners/>
 */

#include <Arduino.h>
#include <WiFi.h>

// INCLUDE THE LIBRARY
#include "qrystal.hpp"

// TODO: SET WIFI NAME AND PASSWORD
#define WIFI_SSID ""
#define WIFI_PASSWORD ""

// Initialize Qrystal once
Qrystal q;

void setup()
{
    Serial.begin(115200);
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    int n = WiFi.scanNetworks();
    if (n <= 0)
    {
        Serial.println("error: no wifi networks available");
        WiFi.disconnect();
        return;
    }

    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    while (WiFi.status() != WL_CONNECTED)
    {
        Serial.print(".");
        delay(500);
    }

    Serial.println("wifi connected. local ip is: " + WiFi.localIP());
}

void loop()
{
    Serial.begin(115200);

    // put your main code here, to run repeatedly:
    for (;;)
    {
        // REPLACE DEVICE ID WITH YOUR DEVICE'S ID AND SET THE TOKEN
        Qrystal::QRYSTAL_STATE s = q.uplink_blocking("<DEVICE_ID>:<DEVICE_TOKEN>");
        // optionally, handle errors here
        // if (s != Q_OK) {
        //
        // }

        Serial.println(WiFi.localIP());
        delay(1000);
    }
}
