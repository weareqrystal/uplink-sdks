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

```cpp
#include "qrystal.hpp"

// In your main loop (after WiFi is connected):
Qrystal::QRYSTAL_STATE status = Qrystal::uplink_blocking("device-id:token");

if (status == Qrystal::Q_OK) {
    // Heartbeat sent
}
```

## Running the Example

```bash
cd examples/espidf_qrystal
idf.py set-target <YOUR ESP BOARD NAME>
idf.py flash
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
