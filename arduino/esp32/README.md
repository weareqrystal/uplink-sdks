# Qrystal SDK for ESP32 (Arduino)

Send heartbeats to [Qrystal Uplink](https://uplink.qrystal.partners/) from ESP32 devices using Arduino.

## Installation

1. Add ESP32 board support in Arduino IDE: **File > Preferences**, add `https://espressif.github.io/arduino-esp32/package_esp32_index.json` to Board Manager URLs, then install "ESP32" from Boards Manager.

2. Copy `qrystal.hpp` to your project folder.

## Usage

```cpp
#include <WiFi.h>
#include "qrystal.hpp"

Qrystal q;

void setup() {
    Serial.begin(115200);
    WiFi.begin("your-wifi", "your-password");
    while (WiFi.status() != WL_CONNECTED) delay(500);
}

void loop() {
    Qrystal::QRYSTAL_STATE status = Qrystal::uplink_blocking("device-id:token");

    if (status != Qrystal::Q_OK) {
        Serial.printf("Heartbeat failed: %d\n", status);
    }

    delay(10000);
}
```

## Status Codes

| Status | Meaning |
|--------|---------|
| `Q_OK` | Success |
| `Q_QRYSTAL_ERR` | Server error (check credentials) |
| `Q_ERR_NO_WIFI` | WiFi not connected |
| `Q_ERR_TIME_NOT_READY` | NTP sync in progress, retry shortly |
| `Q_ERR_INVALID_CREDENTIALS` | Bad format (expected `id:token`) |
| `Q_ERR_INVALID_DID` | Device ID must be 10-40 chars |
| `Q_ERR_INVALID_TOKEN` | Token must be 5+ chars |
| `Q_ESP_HTTP_INIT_FAILED` | HTTP client init failed |
| `Q_ESP_HTTP_ERROR` | HTTP request failed |
