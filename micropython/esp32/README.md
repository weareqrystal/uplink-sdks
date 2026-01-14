# Qrystal SDK for ESP32 (MicroPython)

Send heartbeats to [Qrystal Uplink](https://uplink.qrystal.partners/) from ESP32 devices using MicroPython.

## Installation

1. Flash [MicroPython firmware](https://micropython.org/download/ESP32_GENERIC/) to your ESP32 using [esptool](https://docs.espressif.com/projects/esptool/en/latest/esp32/) or your preferred flashing tool.

2. Upload `qrystal.py` to your device using [mpremote](https://docs.micropython.org/en/latest/reference/mpremote.html), [Thonny](https://thonny.org/), or [ampy](https://github.com/scientifichackers/ampy):

```bash
mpremote cp qrystal.py :
```

## Usage

```python
import network
import time
from qrystal import Qrystal

# Connect to WiFi
wlan = network.WLAN(network.STA_IF)
wlan.active(True)
wlan.connect("your-wifi", "your-password")
while not wlan.isconnected():
    time.sleep(0.5)

# Sync time (required for HTTPS)
import ntptime
ntptime.settime()

# Send heartbeats
while True:
    status = Qrystal.uplink_blocking("device-id:token")

    if status != Qrystal.Q_OK:
        print("Heartbeat failed:", status)

    time.sleep(10)
```

## Status Codes

| Status | Meaning |
| ------ | ------- |
| `Q_OK` | Success |
| `Q_QRYSTAL_ERR` | Server error (check credentials) |
| `Q_ERR_NO_WIFI` | WiFi not connected |
| `Q_ERR_TIME_NOT_READY` | NTP sync in progress, retry shortly |
| `Q_ERR_INVALID_CREDENTIALS` | Bad format (expected `id:token`) |
| `Q_ERR_INVALID_DID` | Device ID must be 10-40 chars |
| `Q_ERR_INVALID_TOKEN` | Token must be 5+ chars |
| `Q_HTTP_ERROR` | HTTP request failed |
