#include <stdio.h>
#include "communications/rg_wifi.h"
#include "communications/rg_zigbee.h"
#include "services/rg_http_server.h"
#include "components/rg_bme280.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_mac.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"


static const char *MAIN_TAG = "RoomGuardianV2::Main";
static data_for_html_server_t shared_data; // Shared data instance

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

extern "C" void app_main(void)
{
    ESP_ERROR_CHECK(bme280_init());

    wifi_hw_init();
    prov_start();
    zigbee_init();

    rg_bme280_data_t bme280_sensor_data = bme280_read_data();
    // Initialize the shared data
    shared_data.temperature = bme280_sensor_data.temperature;
    shared_data.humidity = bme280_sensor_data.humidity;
    shared_data.pressure = bme280_sensor_data.pressure;
    shared_data.ip_address = ip_address; // Assuming `ip_address` is defined elsewhere

    // Start the HTTP server with the shared data
    start_webserver(&shared_data);

    xTaskCreate(sensor_read_task, "sensor_read_task", 4096, NULL, 5, NULL);

    ESP_LOGI(MAIN_TAG, "Room Guardian V2 started");
}