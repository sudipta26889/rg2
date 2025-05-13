#include <esp_event.h>
#include <esp_log.h>
#include <esp_netif.h>
#include <esp_system.h>
#include <esp_wifi.h>
#include <nvs_flash.h>
#include <sdkconfig.h> // Include sdkconfig to access configuration options
// Keep old header include for i2c_bus definitions if rg_bme280.h doesn't pull it in sufficiently
#include "i2c_bus.h"
#include "driver/gpio.h" // Required for gpio_num_t
#include "freertos/FreeRTOS.h" // Required for FreeRTOS types and task creation
#include "freertos/task.h"     // Required for xTaskCreate and vTaskDelay
#include <string.h> // Required for strncpy


// Include for BME280 sensor module
// Ensure this path is correct relative to your main component's directory
#include "sensor_modules/rg_bme280.h"

// --- Removed Matter Includes and Namespaces ---
// All includes and namespaces related to esp_matter have been removed.

static const char *TAG = "APP_MAIN"; // Using a simple static tag


// Simple delay function using FreeRTOS vTaskDelay
static void delay_ms(uint32_t ms) {
    vTaskDelay(pdMS_TO_TICKS(ms));
}

// Declare an instance of the BME280 sensor structure
static rg_bme280_t s_bme280_instance;

// BME280 Sensor Task (Reads data and uses the instance)
static void bme280_task(void *pvParameter) {
    ESP_LOGI(TAG, "BME280 sensor task started.");

    rg_bme280_values_t sensor_values; // Structure to hold readings

    while (1) {
        // Read sensor data using the initialized instance
        if (rg_bme280_read_values(&s_bme280_instance, &sensor_values) == ESP_OK) {
            ESP_LOGI(TAG, "Sensor Data: Temp=%.2f C, Pres=%.2f hPa, Hum=%.2f %%", sensor_values.temperature, sensor_values.pressure, sensor_values.humidity);
            // --- Removed code that would update Matter attributes with sensor data ---
        } else {
            ESP_LOGE(TAG, "Failed to read BME280 sensor data.");
        }

        // Delay before next reading
        delay_ms(10000); // Read every 10 seconds (adjust as needed)
    }
}

// Basic Wi-Fi initialization (Temporarily using string literals for SSID/Password)
static esp_err_t wifi_init() {
    // NVS flash initialization - required for Wi-Fi and other components
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_LOGI(TAG, "NVS flash initialized.");

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // Initialize the wifi_config struct with zeros first to avoid missing initializer warnings
    wifi_config_t wifi_config = {}; // Zero-initialize the entire struct

    // Now set the specific members you need using strncpy for safety
    strncpy((char *)wifi_config.sta.ssid, "BrajOfy_IoT", sizeof(wifi_config.sta.ssid)); // Use strncpy for safety
    wifi_config.sta.ssid[sizeof(wifi_config.sta.ssid) - 1] = '\0'; // Ensure null termination

    strncpy((char *)wifi_config.sta.password, "9851904515@abc", sizeof(wifi_config.sta.password)); // Use strncpy for safety
    wifi_config.sta.password[sizeof(wifi_config.sta.password) - 1] = '\0'; // Ensure null termination


    // Explicitly set authmode after struct definition to avoid initializer error
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;


    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Wi-Fi initialization complete.");
    return ESP_OK;
}


extern "C" void app_main() {
    ESP_LOGI(TAG, "Starting RoomGuardian V2 (without Matter)");

    // NVS flash initialization is now handled within wifi_init for simplicity in this template.
    // It only needs to be called once.

    // Initialize Wi-Fi (assuming it's needed for other functionality or general connectivity)
    wifi_init();

    // --- Removed All Matter Initialization Code ---
    // Removed Zigbee and wifi_manager calls as they are custom components not addressed here.
    // Removed shared_data initialization calls as well.


    // Initialize the BME280 sensor module instance BEFORE starting the task
    if (rg_bme280_init(&s_bme280_instance) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize BME280 sensor module. Cannot start BME280 task.");
        // Don't start the BME280 task if initialization fails
    } else {
        ESP_LOGI(TAG, "BME280 sensor module initialized successfully.");
        // Create the BME280 sensor task to run independently
        // Ensure stack size is sufficient. Adjusted based on common needs.
        xTaskCreate(bme280_task, "bme280_task", configMINIMAL_STACK_SIZE + 4096, NULL, 5, NULL); // Increased stack for BME task
    }


    ESP_LOGI(TAG, "Application setup complete. Running tasks.");

    // app_main can return when FreeRTOS tasks take over execution.
}