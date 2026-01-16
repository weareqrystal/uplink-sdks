/**
 * Qrystal Uplink - Non-Blocking Example
 *
 * Demonstrates the non-blocking API with a Hello World loop
 * running while heartbeats are sent in the background.
 */

#include <stdio.h>
#include <string.h>
#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_sntp.h>
#include <nvs_flash.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "qrystal.hpp"

#define WIFI_SSID ""
#define WIFI_PASSWORD ""
#define QRYSTAL_CREDENTIALS ""

static const char *TAG = "main";

static void on_uplink(int state, void *user_data)
{
    if (state == Qrystal::Q_OK)
        ESP_LOGI(TAG, "Heartbeat OK");
    else
        ESP_LOGW(TAG, "Heartbeat failed: %d", state);
}

extern "C" void app_main(void)
{
    // Initialize NVS (required for WiFi)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Initialize WiFi
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t wifi_cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wifi_cfg));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

    wifi_config_t sta_cfg = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASSWORD,
        }};
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_cfg));
    ESP_ERROR_CHECK(esp_wifi_start());
    esp_wifi_connect();

    // Start non-blocking uplink
    qrystal_uplink_config_t config = QRYSTAL_UPLINK_CONFIG_DEFAULT();
    config.credentials = QRYSTAL_CREDENTIALS;
    config.interval_s = 5; // 5 seconds for demo purposes
    config.callback = on_uplink;

    // runs as a FreeRTOS task in the background
    Qrystal::uplink(&config);

    // Main loop - runs while uplink works in background
    uint32_t count = 0;
    while (true)
    {
        printf("Hello World! (%lu)\n", count++);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
