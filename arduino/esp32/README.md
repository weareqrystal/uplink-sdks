# Qrystal SDK for ESP32 (Arduino)

## Step 1: Adding board support to Arduino

Add the additional board URL shown below to your Arduino IDE and install the board extension titled "ESP32" by Espressif.

```txt
https://espressif.github.io/arduino-esp32/package_esp32_index.json
```

## Step 2: Adding the Qrystal library to your project

Once you've configured the Arduino IDE, create a new Arduino project. If you already have a project, run the following command in your terminal to download the header file:

```bash
curl -o qrystal.hpp https://raw.githubusercontent.com/mikayelgr/qrystal_uplink_sdks/refs/heads/main/arduino/esp32/qrystal.hpp
```

## Step 3: Setting up

Once the library has been added, initialize it like this:

```cpp
// include the library
#include "qrystal.hpp"

// initialize qrystal object
Qrystal q;

void setup() {
    // perform WiFi setup here as usual
}

void loop() {
    // place the uplink code in the main loop. replace <device-id> with your device id
    // and the <token> with your token.
    q.heartbeat("<device-id>:<token>");
}
```

After a while, you will see the state of the device change from "Waiting" to "Healthy". At that point, you're ready to go!
