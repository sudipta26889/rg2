#include <esp_event.h>
#include <esp_log.h>
#include <esp_netif.h>
#include <esp_system.h>
#include <esp_wifi.h>
#include <nvs_flash.h>
#include <sdkconfig.h> // Include sdkconfig to access configuration options

// Include for BME280 sensor module (keeping this as it's not Matter)
#include "sensor_modules/rg_bme280.h"

// --- REMOVED: esp_matter.h include ---
// The following includes and related code blocks were removed as they are specific to esp_matter.
// #include <esp_matter.h>
// #include <esp_matter_endpoint.h>
// #include <esp_matter_attribute.h>
// #include <esp_matter_identify.h>
// #include <esp_matter_temperature_sensor.h>
// #include <esp_matter_on_off.h> // Example if you had an On/Off cluster

// --- REMOVED: Matter specific namespaces ---
// using namespace esp_matter;
// using namespace esp_matter::endpoint;
// using namespace esp_matter::attribute;
// using namespace esp_matter::cluster;

static const char *TAG = "APP_MAIN";

// --- REMOVED: Matter event callback functions ---
// static esp_err_t app_attribute_update_cb(callback_id_t callback_id, uint16_t endpoint_id, uint32_t cluster_id, uint32_t attribute_id, esp_matter_attr_value_t *value, void *context) { /* ... */ return ESP_OK; }
// static esp_err_t app_identification_cb(identification_mode_t mode) { /* ... */ return ESP_OK; }
// static esp_err_t app_event_cb(const ChipDeviceEvent *event, intptr_t arg) { /* ... */ return ESP_OK; }

// --- REMOVED: Matter endpoint/cluster/attribute creation logic ---
// static esp_err_t create_temperature_sensor_endpoint() { /* ... */ return ESP_OK; }

// Simple delay function using FreeRTOS vTaskDelay
static void delay_ms(uint32_t ms) {
    vTaskDelay(pdMS_TO_TICKS(ms));
}

// BME280 Sensor Task (Modified to remove Matter interactions)
static void bme280_task(void *pvParameter) {
    ESP_LOGI(TAG, "BME280 sensor task started.");

    // Initialize the BME280 sensor
    // Assuming rg_bme280_init() is blocking or handles retries internally based on your module.
    if (rg_bme280_init() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize BME280 sensor. Task will exit.");
        // Ensure any resources allocated by rg_bme280_init are cleaned up if it fails partially.
        vTaskDelete(NULL); // Exit the task on initialization failure
    }

    float temperature, humidity, pressure;

    while (1) {
        // Read sensor data
        if (rg_bme280_read_data(&temperature, &pressure, &humidity) == ESP_OK) {
            ESP_LOGI(TAG, "Sensor Data: Temp=%.2f C, Pres=%.2f hPa, Hum=%.2f %%", temperature, pressure, humidity);
            // --- REMOVED: Code that would update Matter attributes with sensor data ---
            // e.g., esp_matter_attr_report_update(endpoint_id, cluster_id, attribute_id, &temperature_value);
        } else {
            ESP_LOGE(TAG, "Failed to read BME280 sensor data.");
        }

        // Delay before next reading
        delay_ms(10000); // Read every 10 seconds (adjust as needed)
    }
}

// Basic Wi-Fi initialization (kept as it's likely needed for other things or general connectivity)
static esp_err_t wifi_init() {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    wifi_config_t wifi_config = {
        .sta = {
            // Use configurations from sdkconfig.defaults or menuconfig
            // Ensure these are defined in your sdkconfig.defaults or set via menuconfig
            .ssid = CONFIG_ESP_WIFI_SSID,
            .password = CONFIG_ESP_WIFI_PASSWORD,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK, // Assuming WPA2-PSK based on your sdkconfig.defaults
            // .sae_pwe_h2e = WIFI_SAE_PWE_BOTH, // Kept from your sdkconfig.defaults
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Wi-Fi initialization complete.");
    return ESP_OK;
}


extern "C" void app_main() {
    ESP_LOGI(TAG, "Starting RoomGuardian V2 (without Matter)");

    // Initialize NVS Flash - required for Wi-Fi and other components
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_LOGI(TAG, "NVS flash initialized.");

    // Initialize Wi-Fi (assuming it's needed for other functionality or general connectivity)
    wifi_init();

    // --- REMOVED: All Matter initialization code ---
    // This included node/endpoint/cluster/attribute creation, callbacks registration,
    // and starting the Matter stack.
    // e.g., esp_matter_node_manager_init();
    // esp_matter_config_t matter_config = { /* ... */ };
    // esp_matter_node_create(&matter_config);
    // esp_matter_start_without_thread();
    // esp_matter_chip_start();

    // Create the BME280 sensor task to run independently
    // Ensure stack size is sufficient. Added extra based on common needs.
    xTaskCreate(bme280_task, "bme280_task", configMINIMAL_STACK_SIZE + 3072, NULL, 5, NULL);

    ESP_LOGI(TAG, "Application setup complete. Running tasks.");

    // app_main can return when FreeRTOS tasks take over execution.
}