#ifndef QRYSTAL_UPLINK
#define QRYSTAL_UPLINK

#include <string>
#include <esp_http_client.h>
#include <esp_wifi.h>

class Qrystal
{
private:
    static std::string credentials_cache;
    static esp_http_client_handle_t client;

    static void reset_client()
    {
        if (client)
        {
            esp_http_client_cleanup(client);
            client = nullptr;
        }

        credentials_cache.clear();
    }

public:
    typedef enum
    {
        Q_OK = 0x0,
        Q_QRYSTAL_ERR,
        Q_ERR_NO_WIFI,
        Q_ERR_TIME_NOT_READY,
        Q_ERR_INVALID_CREDENTIALS,
        Q_ERR_INVALID_DID,
        Q_ERR_INVALID_TOKEN,
        Q_ESP_HTTP_INIT_FAILED,
        Q_ESP_HTTP_ERROR
    } QRYSTAL_STATE;

    /**
     * @brief Sends a blocking uplink heartbeat to the Qrystal server.
     * @param credentials The credentials string in the format "deviceId:token".
     * @return QRYSTAL_STATE indicating the result of the operation.
     */
    static QRYSTAL_STATE uplink_blocking(const std::string &credentials);
};

#endif // QRYSTAL_UPLINK