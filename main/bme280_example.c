#include <stdio.h>
#include "esp_log.h"
#include "bme280.h"
#include "i2c_bus.h" // Required for bme280
#include "freertos/FreeRTOS.h" // Required for FreeRTOS functions
#include "freertos/task.h"     // Required for vTaskDelay

#define I2C_MASTER_SCL_IO           GPIO_NUM_5          // Set your own GPIO
#define I2C_MASTER_SDA_IO           GPIO_NUM_4          // Set your own GPIO
#define I2C_MASTER_NUM              I2C_NUM_0           // Use I2C_NUM_1 if needed
#define I2C_MASTER_FREQ_HZ          100000              // I2C master clock frequency

static const char *TAG = "BME280";

static i2c_bus_handle_t i2c_bus = NULL;
static bme280_handle_t bme280 = NULL;

// Function to initialize the I2C bus and BME280 sensor
static esp_err_t bme280_init(void)
{
    ESP_LOGI(TAG, "Initializing I2C bus...");

    // Configure I2C parameters
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };

    // Create the I2C bus
    i2c_bus = i2c_bus_create(I2C_MASTER_NUM, &conf);
    if (i2c_bus == NULL) {
        ESP_LOGE(TAG, "Failed to create I2C bus.");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Initializing BME280 sensor...");

    // Create the BME280 sensor handle
    bme280 = bme280_create(i2c_bus, BME280_I2C_ADDRESS_DEFAULT);
    if (bme280 == NULL) {
        ESP_LOGE(TAG, "Failed to initialize BME280.");
        return ESP_FAIL;
    }

    // Initialize the BME280 sensor with default settings
    if (bme280_default_init(bme280) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize BME280 with default settings.");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "BME280 initialization successful.");
    return ESP_OK;
}

// Function to deinitialize the BME280 sensor and I2C bus
static void bme280_deinit(void)
{
    if (bme280) {
        bme280_delete(&bme280);
        bme280 = NULL;
    }
    if (i2c_bus) {
        i2c_bus_delete(&i2c_bus);
        i2c_bus = NULL;
    }
    ESP_LOGI(TAG, "BME280 and I2C bus deinitialized.");
}

// Function to read and log sensor data
static void bme280_read_data(void)
{
    float temperature = 0.0, humidity = 0.0, pressure = 0.0;

    if (bme280_read_temperature(bme280, &temperature) == ESP_OK) {
        ESP_LOGI(TAG, "Temperature: %.2f°C", temperature);
    } else {
        ESP_LOGE(TAG, "Failed to read temperature.");
    }

    if (bme280_read_humidity(bme280, &humidity) == ESP_OK) {
        ESP_LOGI(TAG, "Humidity: %.2f%%", humidity);
    } else {
        ESP_LOGE(TAG, "Failed to read humidity.");
    }

    if (bme280_read_pressure(bme280, &pressure) == ESP_OK) {
        ESP_LOGI(TAG, "Pressure: %.2f hPa", pressure);
    } else {
        ESP_LOGE(TAG, "Failed to read pressure.");
    }
}

// void app_main(void)
// {
//     ESP_LOGI(TAG, "Starting BME280 application...");

//     // Initialize the BME280 sensor
//     if (bme280_init() != ESP_OK) {
//         ESP_LOGE(TAG, "BME280 initialization failed. Exiting application.");
//         return;
//     }

//     // Read and log sensor data in a loop
//     for (int i = 0; i < 10; i++) {
//         bme280_read_data();
//         vTaskDelay(pdMS_TO_TICKS(1000)); // Delay for 1 second
//     }

//     // Deinitialize the BME280 sensor and I2C bus
//     bme280_deinit();

//     ESP_LOGI(TAG, "BME280 application finished.");
// }