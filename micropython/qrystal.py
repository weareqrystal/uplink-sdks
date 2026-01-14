# Qrystal Uplink SDKs
# Official SDKs for Qrystal Uplink - device monitoring and heartbeat service <https://uplink.qrystal.partners/>.
#
# SPDX-License-Identifier: MIT
# Copyright (c) 2025 Qrystal Uplink, Qrystal Partners, Mikayel Grigoryan
# <https://uplink.qrystal.partners/>

"""
Qrystal Uplink SDK for MicroPython

This module provides a simple interface for sending heartbeat signals
from MicroPython devices to the Qrystal Uplink service.

Usage:
    from qrystal import Qrystal

    status = Qrystal.uplink_blocking("my-device-id:token")
    if status == Qrystal.Q_OK:
        print("Heartbeat sent successfully")
"""

import network
import time

try:
    import urequests as requests
except ImportError:
    import requests

try:
    import ntptime
except ImportError:
    ntptime = None

try:
    from enum import IntEnum

    class QrystalState(IntEnum):
        """Status codes returned by Qrystal SDK operations."""

        Q_OK = 0x0  # Heartbeat sent successfully
        Q_QRYSTAL_ERR = 0x1  # Server returned an error (check credentials)
        Q_ERR_NO_WIFI = 0x2  # WiFi not connected
        Q_ERR_TIME_NOT_READY = 0x3  # NTP time sync not complete (retry shortly)
        Q_ERR_INVALID_CREDENTIALS = 0x4  # Credentials format invalid
        Q_ERR_INVALID_DID = 0x5  # Device ID has invalid length
        Q_ERR_INVALID_TOKEN = 0x6  # Token is too short
        Q_HTTP_ERROR = 0x7  # HTTP request failed

except ImportError:
    # MicroPython fallback: simple class mimicking IntEnum behavior
    class QrystalState:
        """Status codes returned by Qrystal SDK operations."""

        Q_OK = 0x0  # Heartbeat sent successfully
        Q_QRYSTAL_ERR = 0x1  # Server returned an error (check credentials)
        Q_ERR_NO_WIFI = 0x2  # WiFi not connected
        Q_ERR_TIME_NOT_READY = 0x3  # NTP time sync not complete (retry shortly)
        Q_ERR_INVALID_CREDENTIALS = 0x4  # Credentials format invalid
        Q_ERR_INVALID_DID = 0x5  # Device ID has invalid length
        Q_ERR_INVALID_TOKEN = 0x6  # Token is too short
        Q_HTTP_ERROR = 0x7  # HTTP request failed


# Minimum valid epoch timestamp (Jan 1, 2026 09:09:09 UTC+4)
# Used as a sanity check to ensure NTP has synchronized the clock
_YEAR_2026_EPOCH = 1767244149

# Server endpoint
_HEARTBEAT_URL = "https://on.uplink.qrystal.partners/api/v1/heartbeat"


class Qrystal:
    """
    Qrystal Uplink SDK for sending heartbeat signals.

    This class provides a static method for sending heartbeat signals
    to the Qrystal Uplink server. All methods are static, so no
    instantiation is required.

    Status Codes:
        Q_OK (0x0): Heartbeat sent successfully
        Q_QRYSTAL_ERR (0x1): Server returned an error (check credentials)
        Q_ERR_NO_WIFI (0x2): WiFi not connected
        Q_ERR_TIME_NOT_READY (0x3): NTP time sync not complete (retry shortly)
        Q_ERR_INVALID_CREDENTIALS (0x4): Credentials format invalid
        Q_ERR_INVALID_DID (0x5): Device ID has invalid length
        Q_ERR_INVALID_TOKEN (0x6): Token is too short
        Q_HTTP_ERROR (0x7): HTTP request failed
    """

    # Status codes (matching C++ SDKs)
    Q_OK = 0x0
    Q_QRYSTAL_ERR = 0x1
    Q_ERR_NO_WIFI = 0x2
    Q_ERR_TIME_NOT_READY = 0x3
    Q_ERR_INVALID_CREDENTIALS = 0x4
    Q_ERR_INVALID_DID = 0x5
    Q_ERR_INVALID_TOKEN = 0x6
    Q_HTTP_ERROR = 0x7

    # Internal state
    _time_ready = False
    _last_sync_time = 0
    _cached_credentials = ""
    _cached_device_id = ""
    _cached_token = ""

    @staticmethod
    def uplink_blocking(credentials):
        """
        Send a heartbeat signal to the Qrystal Uplink server.

        This is a blocking call that verifies connectivity, time sync,
        and credentials before sending the heartbeat request.

        Args:
            credentials (str): Device credentials in the format "device-id:token"

        Returns:
            int: Status code indicating the result of the operation.
                 Use the Q_* class constants to interpret the result.

        Example:
            status = Qrystal.uplink_blocking("my-device-id:my-secret-token")
            if status == Qrystal.Q_OK:
                print("Success!")
            elif status == Qrystal.Q_ERR_TIME_NOT_READY:
                time.sleep(1)  # Retry shortly
        """
        # Step 1: Verify WiFi connectivity
        wlan = network.WLAN(network.STA_IF)
        if not wlan.isconnected():
            return Qrystal.Q_ERR_NO_WIFI

        # Step 2: Verify time synchronization
        current_time = time.time()

        if not Qrystal._time_ready:
            # Check if time is valid (after 2026)
            if current_time < _YEAR_2026_EPOCH:
                # Try to sync time if ntptime is available
                if ntptime is not None:
                    try:
                        ntptime.settime()
                        current_time = time.time()
                    except Exception:
                        pass

                # Re-check after sync attempt
                if current_time < _YEAR_2026_EPOCH:
                    return Qrystal.Q_ERR_TIME_NOT_READY

            Qrystal._time_ready = True
            Qrystal._last_sync_time = current_time
        else:
            # Check for time staleness (>24h) or clock adjustments
            if current_time < Qrystal._last_sync_time or (current_time - Qrystal._last_sync_time) > 86400:
                Qrystal._time_ready = False
                return Qrystal.Q_ERR_TIME_NOT_READY

        # Step 3: Validate credentials format
        if not credentials:
            return Qrystal.Q_ERR_INVALID_CREDENTIALS

        # Parse and cache credentials if changed
        if credentials != Qrystal._cached_credentials:
            if ":" not in credentials:
                return Qrystal.Q_ERR_INVALID_CREDENTIALS

            split_index = credentials.index(":")
            if split_index == 0:
                return Qrystal.Q_ERR_INVALID_CREDENTIALS

            device_id = credentials[:split_index]
            token = credentials[split_index + 1 :]

            # Validate device ID length (10-40 characters)
            if len(device_id) < 10 or len(device_id) > 40:
                return Qrystal.Q_ERR_INVALID_DID

            # Validate token length (minimum 5 characters)
            if len(token) < 5:
                return Qrystal.Q_ERR_INVALID_TOKEN

            Qrystal._cached_credentials = credentials
            Qrystal._cached_device_id = device_id
            Qrystal._cached_token = token

        # Step 4: Send HTTP request
        headers = {
            "X-Qrystal-Uplink-DID": Qrystal._cached_device_id,
            "Authorization": "Bearer " + Qrystal._cached_token,
        }

        try:
            response = requests.post(_HEARTBEAT_URL, headers=headers)
            status_code = response.status_code
            response.close()

            if 200 <= status_code < 300:
                return Qrystal.Q_OK
            else:
                return Qrystal.Q_QRYSTAL_ERR

        except Exception:
            return Qrystal.Q_HTTP_ERROR
