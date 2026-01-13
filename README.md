# Qrystal Uplink SDKs

Official SDKs for [Qrystal Uplink](https://uplink.qrystal.partners/).

## Available SDKs

| Platform | Language | Path |
|----------|----------|------|
| ESP32 (Arduino) | C++ | [arduino/esp32/](arduino/esp32/) |

## Quick Start (ESP32)

1. Add ESP32 board support to Arduino IDE:

   ```
   https://espressif.github.io/arduino-esp32/package_esp32_index.json
   ```

2. Download the library:

   ```bash
   curl -O https://raw.githubusercontent.com/qrystal-partners/qrystal_uplink_sdks/main/arduino/esp32/qrystal.hpp
   ```

3. Use in your sketch:

   ```cpp
   #include "qrystal.hpp"

   Qrystal qrystal;

   void setup() {
       // Connect to WiFi first
       qrystal.begin("your-device-id", "your-api-token");
   }

   void loop() {
       qrystal.heartbeat();
       delay(30000);
   }
   ```

See [arduino/esp32/README.md](arduino/esp32/README.md) for details.
