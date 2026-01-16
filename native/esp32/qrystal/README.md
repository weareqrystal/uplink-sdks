# Qrystal SDK for ESP32 (ESP-IDF)

Heartbeat SDK for [Qrystal Uplink](https://uplink.qrystal.partners/).

## Installation

Download the SDK directly:

```bash
curl -L "https://download-directory.github.io/?url=https%3A%2F%2Fgithub.com%2Fweareqrystal%2Fuplink-sdks%2Ftree%2Fmain%2Fnative%2Fesp32%2Fqrystal" -o qrystal.zip && unzip qrystal.zip -d components/ && rm qrystal.zip
```

Or copy this `qrystal` folder into your project's `components` directory:

```text
your_project/
├── components/
│   └── qrystal/
├── main/
└── CMakeLists.txt
```

## Usage

The SDK supports two modes of operation:

### Non-Blocking (Recommended)

Heartbeats run automatically in a background FreeRTOS task:

```cpp
#include "qrystal.hpp"

// Optional: callback to monitor heartbeat results
void on_heartbeat(int state, void *user_data) {
    if (state == Qrystal::Q_OK) {
        ESP_LOGI("app", "Heartbeat sent");
    }
}

// After WiFi is connected:
qrystal_uplink_config_t config = QRYSTAL_UPLINK_CONFIG_DEFAULT();
config.credentials = "device-id:token";
config.interval_s = 30;            // 30 seconds
config.callback = on_heartbeat;    // Optional

Qrystal::uplink(&config);          // Starts background task

// Your app continues - heartbeats happen automatically
// ...

Qrystal::uplink_stop();            // Stop when needed (e.g., before deep sleep)
```

### Blocking

For manual control in your own task loop:

```cpp
#include "qrystal.hpp"

// In your main loop (after WiFi is connected):
while (true) {
    Qrystal::QRYSTAL_STATE status = Qrystal::uplink_blocking("device-id:token");

    if (status == Qrystal::Q_OK) {
        // Heartbeat sent
    }

    vTaskDelay(pdMS_TO_TICKS(30000));  // 30 seconds
}
```

## API Reference

### Non-Blocking API

| Function | Description |
|----------|-------------|
| `Qrystal::uplink(config)` | Start background heartbeat task |
| `Qrystal::uplink_stop()` | Stop the background task |
| `Qrystal::uplink_is_running()` | Check if task is active |

### Configuration (`qrystal_uplink_config_t`)

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `credentials` | `const char*` | - | Device credentials (`"device-id:token"`) |
| `interval_s` | `uint32_t` | 30 | Heartbeat interval in seconds |
| `callback` | `qrystal_uplink_callback_t` | NULL | Optional completion callback |
| `user_data` | `void*` | NULL | Context passed to callback |
| `stack_size` | `uint32_t` | 4096 | Task stack size in bytes |
| `priority` | `UBaseType_t` | 5 | FreeRTOS task priority |

### Blocking API

| Function | Description |
|----------|-------------|
| `Qrystal::uplink_blocking(credentials)` | Send a single heartbeat (blocks until complete) |

## Running the Examples

### Blocking Example

```bash
cd examples/espidf_qrystal
idf.py set-target <YOUR_ESP_BOARD>
idf.py flash monitor
```

### Non-Blocking Example

```bash
cd examples/espidf_qrystal_nonblocking
idf.py set-target <YOUR_ESP_BOARD>
idf.py flash monitor
```

## Return Codes

| Status | Meaning |
|--------|---------|
| `Q_OK` | Success |
| `Q_ERR_NO_WIFI` | WiFi not connected |
| `Q_ERR_TIME_NOT_READY` | SNTP sync pending |
| `Q_QRYSTAL_ERR` | Server error |
| `Q_ERR_INVALID_CREDENTIALS` | Bad format |
| `Q_ERR_INVALID_DID` | Invalid device ID |
| `Q_ERR_INVALID_TOKEN` | Invalid token |
| `Q_ESP_HTTP_INIT_FAILED` | HTTP init failed |
| `Q_ESP_HTTP_ERROR` | HTTP request failed |
