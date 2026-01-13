#include <stdio.h>
#include <esp_sntp.h>
#include <esp_crt_bundle.h>

#include "qrystal.hpp"

static const uint32_t YEAR_2026_EPOCH = 1767244149; // Epoch time for Thu Jan 01 2026 09:09:09 (Armenian Time)

Qrystal::QRYSTAL_STATE Qrystal::uplink_blocking(const std::string &credentials)
{
    static bool timeReady = false;

    // what matters most for us in the first place is whether wifi connection exists,
    // if it doesn't there is no point in proceeding further.
    wifi_ap_record_t ap_info;
    if (esp_wifi_sta_get_ap_info(&ap_info) != ESP_OK)
    {
        return Q_ERR_NO_WIFI;
    }

    // timeReady will be initialized once and only once when the time is confirmed to be synchronized.
    if (!timeReady)
    {
        // in case wifi connection exists, we can proceed with the second most important check
        // which is time synchronization.
        if (sntp_get_sync_status() != SNTP_SYNC_STATUS_COMPLETED)
        {
            esp_sntp_init(); // try to init sntp just in case if not already done
            return Q_ERR_TIME_NOT_READY;
        }

        uint32_t sec;
        // getting the system time to ensure that sntp is really working and not compromised
        // by verifying that the year is reasonable enough. since this code was written in
        // 2026, it makes sense to check for 2026 threshold.
        sntp_get_system_time(&sec, nullptr);
        if (sec < YEAR_2026_EPOCH)
        {
            return Q_ERR_TIME_NOT_READY;
        }

        timeReady = true;
    }

    // at this point both wifi connection and time synchronization are confirmed to be valid.
    // we can now proceed with the actual uplink_blocking logic.
    if (credentials.empty())
    {
        return Q_ERR_INVALID_CREDENTIALS;
    }

    // in case credentials have changed or the client is not initialized yet, we need to (re)initialize
    // the credentials as well as the HTTP client.
    if (client == NULL || credentials != credentials_cache)
    {
        int splitIndex = credentials.find(':');
        if (splitIndex == std::string::npos || splitIndex == 0 || splitIndex < 0)
        {
            return Q_ERR_INVALID_CREDENTIALS;
        }

        // making sure that the device ID is correct length-wise. this permissive check
        // is done intentionally here, more strict checks are done on server-side.
        const std::string deviceId = credentials.substr(0, splitIndex);
        if (deviceId.length() < 10 || deviceId.length() > 30)
        {
            return Q_ERR_INVALID_DID;
        }

        // making sure that the token has logical length. again, more strict checks
        // are done on server-side.
        const std::string token = credentials.substr(splitIndex + 1);
        if (token.length() < 5)
        {
            return Q_ERR_INVALID_TOKEN;
        }

        if (client == NULL)
        {
            esp_http_client_config_t cfg = {
                .url = "https://qrystal-uplink_blocking-server-staging.up.railway.app/api/v1/heartbeat",
                .crt_bundle_attach = esp_crt_bundle_attach,
                .keep_alive_enable = true,
            };

            client = esp_http_client_init(&cfg);
            if (!client)
            {
                return Q_ESP_HTTP_INIT_FAILED;
            }

            esp_http_client_set_method(client, HTTP_METHOD_POST);
            esp_http_client_set_header(client, "X-Qrystal-Uplink-DID", deviceId.c_str());
            esp_http_client_set_header(client, "Authorization", ("Bearer " + token).c_str());
            credentials_cache = credentials;
        }
    }

    esp_err_t state = esp_http_client_perform(client);
    if (state == ESP_OK)
    {
        int http_code = esp_http_client_get_status_code(client);
        if (http_code >= 200 && http_code < 300)
        {
            return Q_OK;
        }

        // in case of API-level error, we just tell the user that it's a qrystal-related error.
        return Q_QRYSTAL_ERR;
    }
    else
    {
        reset_client();
        return Q_ESP_HTTP_ERROR;
    }
}
