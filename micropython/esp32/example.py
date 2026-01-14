# Qrystal Uplink SDKs
# Official SDKs for Qrystal Uplink - device monitoring and heartbeat service <https://uplink.qrystal.partners/>.
#
# SPDX-License-Identifier: MIT
# Copyright (c) 2025 Qrystal Uplink, Qrystal Partners, Mikayel Grigoryan
# <https://uplink.qrystal.partners/>

"""
Example usage of the Qrystal Uplink SDK for MicroPython.

This script demonstrates how to:
1. Connect to WiFi
2. Synchronize time via NTP
3. Send periodic heartbeat signals to Qrystal Uplink
"""

import network
import time

try:
    import ntptime
except ImportError:
    print("Warning: ntptime module not available")

from qrystal import Qrystal

# TODO: Set your WiFi credentials
WIFI_SSID = ""
WIFI_PASSWORD = ""

# TODO: Set your Qrystal Uplink credentials
# Get these from https://uplink.qrystal.partners/
DEVICE_CREDENTIALS = "<DEVICE_ID>:<DEVICE_TOKEN>"

# Heartbeat interval in seconds
HEARTBEAT_INTERVAL = 10


def connect_wifi():
    """Connect to WiFi network."""
    wlan = network.WLAN(network.STA_IF)
    wlan.active(True)

    if wlan.isconnected():
        print("Already connected to WiFi")
        print("IP address:", wlan.ifconfig()[0])
        return True

    print("Connecting to WiFi:", WIFI_SSID)
    wlan.connect(WIFI_SSID, WIFI_PASSWORD)

    # Wait for connection with timeout
    timeout = 30
    while not wlan.isconnected() and timeout > 0:
        print(".", end="")
        time.sleep(1)
        timeout -= 1

    if wlan.isconnected():
        print("\nWiFi connected!")
        print("IP address:", wlan.ifconfig()[0])
        return True
    else:
        print("\nFailed to connect to WiFi")
        return False


def sync_time():
    """Synchronize time via NTP."""
    print("Synchronizing time via NTP...")
    try:
        ntptime.settime()
        print("Time synchronized:", time.localtime())
        return True
    except Exception as e:
        print("Failed to sync time:", e)
        return False


def status_to_string(status):
    """Convert status code to human-readable string."""
    status_messages = {
        Qrystal.Q_OK: "Success",
        Qrystal.Q_QRYSTAL_ERR: "Server error (check credentials)",
        Qrystal.Q_ERR_NO_WIFI: "WiFi not connected",
        Qrystal.Q_ERR_TIME_NOT_READY: "Time not synchronized",
        Qrystal.Q_ERR_INVALID_CREDENTIALS: "Invalid credentials format",
        Qrystal.Q_ERR_INVALID_DID: "Invalid device ID length",
        Qrystal.Q_ERR_INVALID_TOKEN: "Invalid token length",
        Qrystal.Q_HTTP_ERROR: "HTTP request failed",
    }
    return status_messages.get(status, "Unknown error")


def main():
    """Main entry point."""
    # Step 1: Connect to WiFi
    if not connect_wifi():
        return

    # Step 2: Sync time
    sync_time()

    # Step 3: Send heartbeats in a loop
    print("\nStarting heartbeat loop...")
    print("Interval:", HEARTBEAT_INTERVAL, "seconds")
    print()

    while True:
        status = Qrystal.uplink_blocking(DEVICE_CREDENTIALS)

        if status == Qrystal.Q_OK:
            print("Heartbeat sent successfully")
        else:
            print("Heartbeat failed:", status_to_string(status))

            # Handle specific errors
            if status == Qrystal.Q_ERR_NO_WIFI:
                print("Attempting to reconnect to WiFi...")
                connect_wifi()
            elif status == Qrystal.Q_ERR_TIME_NOT_READY:
                print("Attempting to sync time...")
                sync_time()

        time.sleep(HEARTBEAT_INTERVAL)


if __name__ == "__main__":
    main()
