# Qrystal Uplink SDKs

Official SDKs for [Qrystal Uplink](https://uplink.qrystal.partners/) — a device monitoring service for IoT devices.

## Overview

Qrystal Uplink allows you to monitor the health and connectivity of your IoT devices in real-time. These SDKs provide a simple way to send heartbeat signals from your ESP32 devices to the Qrystal Uplink service.

### Features

- Lightweight heartbeat API
- Automatic SSL/TLS encryption
- Connection pooling for efficient resource usage
- Automatic time synchronization via NTP
- Comprehensive error handling

## Available SDKs

| Platform | Framework | Language | Documentation |
|----------|-----------|----------|---------------|
| ESP32 | Arduino | C++ | [arduino/esp32/](arduino/esp32/) |
| ESP32 | ESP-IDF (Native) | C++ | [native/esp32/](native/esp32/qrystal/) |

### Which SDK should I use?

- **Arduino SDK**: Best for beginners and rapid prototyping. Uses the Arduino framework and is easy to integrate into existing Arduino projects.
- **Native ESP-IDF SDK**: Best for production deployments and projects already using ESP-IDF. Offers more control and slightly better performance.

## Quick Start

### 1. Get your credentials

Sign up at [Qrystal Uplink](https://uplink.qrystal.partners/) and create a device to get your `device-id` and `token`.

### 2. Choose your SDK

Follow the setup guide for your preferred framework:

- [Arduino SDK Setup](arduino/esp32/)
- [Native ESP-IDF SDK Setup](native/esp32/qrystal/)

### 3. Send heartbeats

Once configured, your device will send periodic heartbeats. When the Qrystal Uplink dashboard shows your device status as "Healthy", you're all set.

## API Reference

Both SDKs return status codes to indicate the result of each heartbeat:

| Status Code | Description |
|-------------|-------------|
| `Q_OK` | Heartbeat sent successfully |
| `Q_QRYSTAL_ERR` | Server returned an error (check credentials) |
| `Q_ERR_NO_WIFI` | WiFi not connected |
| `Q_ERR_TIME_NOT_READY` | NTP time sync not complete (retry shortly) |
| `Q_ERR_INVALID_CREDENTIALS` | Credentials format invalid (expected `device-id:token`) |
| `Q_ERR_INVALID_DID` | Device ID has invalid length |
| `Q_ERR_INVALID_TOKEN` | Token is too short |
| `Q_ESP_HTTP_INIT_FAILED` | HTTP client initialization failed |
| `Q_ESP_HTTP_ERROR` | HTTP request failed (connection lost) |

## License

This project is licensed under the MIT License. See [LICENSE](LICENSE) for details.
