#include <stdio.h>
#include "esp_wifi.h"
#include "wifi_manager.h"
#include "communications/zigbee/rg_zigbee.h"
#include "communications/wifi/rg_wifi.h"
#include "sensor_modules/rg_bme280.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "constants.h"
#include "services/shared_data/shared_data.h"

static const char *MAIN_TAG = (std::string(DEVICE_NAME) + "-" + DEVICE_VERSION + "::Main").c_str();

void sensor_read_task(void *pvParameters) {
    while (1) {
        rg_bme280_data_t bme280_sensor_data = bme280_read_data();
        
        // Update the shared data
        shared_data.temperature = bme280_sensor_data.temperature;
        shared_data.humidity = bme280_sensor_data.humidity;
        shared_data.pressure = bme280_sensor_data.pressure;
        zigbee_update_sensor_values(shared_data.temperature, shared_data.humidity);
        ESP_LOGI(MAIN_TAG, "Temperature: %.2f°C, Humidity: %.2f%%, Pressure: %.2f hPa IP: %s", 
            bme280_sensor_data.temperature, bme280_sensor_data.humidity, bme280_sensor_data.pressure, shared_data.ip_address);
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
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
    zigbee_update_sensor_values(shared_data.temperature, shared_data.humidity);


    // Start the sensor read task
    xTaskCreate(sensor_read_task, "sensor_read_task", 4096, NULL, 5, NULL);

    ESP_LOGI(MAIN_TAG, "%s %s started", DEVICE_NAME, DEVICE_VERSION);
}