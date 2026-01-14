/**
 * Qrystal Uplink SDK - ESP-IDF Example
 *
 * This example demonstrates how to send heartbeat signals to Qrystal Uplink
 * using the native ESP-IDF SDK.
 *
 * Before running:
 * 1. Set your WiFi credentials below
 * 2. Set your Qrystal device credentials
 * 3. Build and flash: idf.py build flash monitor
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
#include <freertos/event_groups.h>

#include "qrystal.hpp"

// TODO: Set your WiFi credentials
#define WIFI_SSID "your-wifi-ssid"
#define WIFI_PASSWORD "your-wifi-password"

// TODO: Set your Qrystal credentials (format: "device-id:token")
#define QRYSTAL_CREDENTIALS "your-device-id:your-token"

static const char *TAG = "qrystal_example";
static EventGroupHandle_t wifi_event_group;
static const int WIFI_CONNECTED_BIT = BIT0;

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        esp_wifi_connect();
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        ESP_LOGI(TAG, "Disconnected, reconnecting...");
        esp_wifi_connect();
        xEventGroupClearBits(wifi_event_group, WIFI_CONNECTED_BIT);
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "Connected, IP: " IPSTR, IP2STR(&event->ip_info.ip));
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

static void wifi_init(void)
{
    wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL));

    wifi_config_t wifi_config = {};
    strncpy((char *)wifi_config.sta.ssid, WIFI_SSID, sizeof(wifi_config.sta.ssid));
    strncpy((char *)wifi_config.sta.password, WIFI_PASSWORD, sizeof(wifi_config.sta.password));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "WiFi initialization complete");
}

static void sntp_init_time(void)
{
    ESP_LOGI(TAG, "Initializing SNTP");
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_setservername(1, "time.nist.gov");
    esp_sntp_init();
}

extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "Qrystal Uplink Example Starting");

    // Initialize NVS (required for WiFi)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Initialize WiFi
    wifi_init();

    // Wait for WiFi connection
    ESP_LOGI(TAG, "Waiting for WiFi connection...");
    xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_BIT,
                        pdFALSE, pdTRUE, portMAX_DELAY);

    // Initialize SNTP for time synchronization
    sntp_init_time();

    // Main heartbeat loop
    ESP_LOGI(TAG, "Starting heartbeat loop");
    while (true)
    {
        Qrystal::QRYSTAL_STATE status = Qrystal::uplink_blocking(QRYSTAL_CREDENTIALS);

        switch (status)
        {
        case Qrystal::Q_OK:
            ESP_LOGI(TAG, "Heartbeat sent successfully");
            break;
        case Qrystal::Q_ERR_NO_WIFI:
            ESP_LOGW(TAG, "No WiFi connection");
            break;
        case Qrystal::Q_ERR_TIME_NOT_READY:
            ESP_LOGW(TAG, "Time not synchronized yet, retrying...");
            vTaskDelay(pdMS_TO_TICKS(2000));
            continue;
        case Qrystal::Q_QRYSTAL_ERR:
            ESP_LOGE(TAG, "Server error - check credentials");
            break;
        default:
            ESP_LOGE(TAG, "Heartbeat failed with code: %d", status);
        }

        vTaskDelay(pdMS_TO_TICKS(10000)); // Send heartbeat every 10 seconds
    }
}
