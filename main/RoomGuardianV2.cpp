#include <stdio.h>
#include "esp_wifi.h"
#include "wifi_manager.h" // Ensure this header is included for wifi_manager_get_sta_ip_string
#include <http_app.h>
#include "communications/rg_zigbee.h"
#include "services/rg_http_server.h"
#include "components/rg_bme280.h"
#include "esp_log.h"
#include "wifi_provisioning/manager.h" // Include for wifi_prov_mgr_is_provisioned
#include "esp_system.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_mac.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs.h" // Include for nvs_erase_all
#include "nvs_flash.h" // Include for nvs_flash_init

// Define the Wi-Fi manager NVS namespace
static const char *wifi_manager_nvs_namespace = "espwifimgr";


static const char *MAIN_TAG = "RoomGuardianV2::Main";

static char ip_address[16] = "0.0.0.0";

void sensor_read_task(void *pvParameters) {
    while (1) {
        rg_bme280_data_t bme280_sensor_data = bme280_read_data();
        
        // Update the shared data
        shared_data.temperature = bme280_sensor_data.temperature;
        shared_data.humidity = bme280_sensor_data.humidity;
        shared_data.pressure = bme280_sensor_data.pressure;

        ESP_LOGI(MAIN_TAG, "Temperature: %.2f°C, Humidity: %.2f%%, Pressure: %.2f hPa IP: %s", 
            bme280_sensor_data.temperature, bme280_sensor_data.humidity, bme280_sensor_data.pressure, ip_address);
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}


void cb_connection_ok(void *pvParameter) {
    ip_event_got_ip_t *param = (ip_event_got_ip_t *)pvParameter;

    // Transform IP to human-readable string
    esp_ip4addr_ntoa(&param->ip_info.ip, ip_address, sizeof(ip_address));

    ESP_LOGI(MAIN_TAG, "Connected to Wi-Fi. IP Address: %s", ip_address);
    shared_data.ip_address = ip_address;

    // Stop the Wi-Fi Manager
    // wifi_manager_destroy();
    // wifi_manager_disconnect_async();
    // http_app_stop();
    
    // Attach RG endpoint to HTTP server
    http_app_set_handler_hook(HTTP_GET, &my_get_handler);

}

// Reset Wi-Fi provisioning data
void reset_wifi_provisioning(void) {
    ESP_LOGI(MAIN_TAG, "Resetting Wi-Fi provisioning data...");

    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // NVS partition was truncated or a new version was found, erase and reinitialize
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    nvs_handle handle;
    esp_err_t err = nvs_open(wifi_manager_nvs_namespace, NVS_READWRITE, &handle);
    if (err == ESP_OK) {
        // Erase all keys in the "espwifimgr" namespace
        err = nvs_erase_all(handle);
        if (err == ESP_OK) {
            ESP_LOGI(MAIN_TAG, "Wi-Fi provisioning data erased successfully.");
        } else {
            ESP_LOGE(MAIN_TAG, "Failed to erase Wi-Fi provisioning data: %s", esp_err_to_name(err));
        }
        nvs_close(handle);
    } else {
        ESP_LOGE(MAIN_TAG, "Failed to open NVS: %s", esp_err_to_name(err));
    }

    // Commit changes to NVS
    nvs_commit(handle);
}

extern "C" void app_main(void) {

    // Reset Wi-Fi provisioning data (if needed)
    // reset_wifi_provisioning();

    // Initialize the BME280 component
    ESP_ERROR_CHECK(bme280_init());

    // Initialize Wi-Fi Manager
    wifi_manager_start();

    // Register callback for successful connection
    wifi_manager_set_callback(WM_EVENT_STA_GOT_IP, &cb_connection_ok);

    // Initialize Zigbee
    zigbee_init();

    // Initialize the shared data
    rg_bme280_data_t bme280_sensor_data = bme280_read_data();
    shared_data.temperature = bme280_sensor_data.temperature;
    shared_data.humidity = bme280_sensor_data.humidity;
    shared_data.pressure = bme280_sensor_data.pressure;


    // Start the sensor read task
    xTaskCreate(sensor_read_task, "sensor_read_task", 4096, NULL, 5, NULL);

    ESP_LOGI(MAIN_TAG, "Room Guardian V2 started");
}