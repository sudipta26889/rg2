// main/sensor_modules/rg_bme280.c
#include "rg_bme280.h" // Include our custom header first
#include "esp_log.h"
#include "freertos/FreeRTOS.h" // Required for FreeRTOS types
#include "freertos/task.h"     // Required for vTaskDelay or similar

// Include necessary ESP-IDF headers for older I2C bus and GPIO
#include "i2c_bus.h" // Include the header for the older I2C API
#include "driver/gpio.h"

// Include header from the espressif/bme280 managed component's interface
#include "bme280.h"

static const char *TAG = "RG_BME280";

// Define I2C_MASTER_FREQ_HZ if not already defined elsewhere (e.g., constants.h)
#ifndef I2C_MASTER_FREQ_HZ
#define I2C_MASTER_FREQ_HZ 100000
#endif

// Initialize the BME280 sensor module using the component's API
esp_err_t rg_bme280_init(rg_bme280_t *rg_bme280)
{
    if (!rg_bme280) {
        ESP_LOGE(TAG, "rg_bme280 context pointer is NULL.");
        return ESP_ERR_INVALID_ARG;
    }

    // Initialize handles to NULL
    rg_bme280->i2c_bus_handle = NULL;
    rg_bme280->bme280_handle = NULL;

    esp_err_t ret;

    // Get I2C configuration from Kconfig (defined in your sdkconfig.defaults)
    // Ensure these are correctly configured in idf.py menuconfig
    i2c_port_t i2c_port = I2C_NUM_0; // Assuming I2C_NUM_0, verify in menuconfig if needed
    gpio_num_t sda_pin = (gpio_num_t)CONFIG_BME280_I2C_SDA_GPIO; // From sdkconfig.defaults
    gpio_num_t scl_pin = (gpio_num_t)CONFIG_BME280_I2C_SCL_GPIO; // From sdkconfig.defaults
    uint8_t bme280_addr = RG_BME280_I2C_ADDR_PRIM; // Use the address defined in our header


    // 1. Configure I2C master parameters using the older i2c_bus API
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = sda_pin,
        .scl_io_num = scl_pin,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master = { // Corrected syntax for initializing the nested 'master' struct
            .clk_speed = I2C_MASTER_FREQ_HZ, // Use the defined frequency
        },
        .clk_flags = 0,
    };

    // 2. Create the I2C bus using the older i2c_bus API
    rg_bme280->i2c_bus_handle = i2c_bus_create(i2c_port, &conf);
    if (rg_bme280->i2c_bus_handle == NULL) {
        ESP_LOGE(TAG, "Failed to create I2C bus.");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "I2C bus created on port %d, SDA:%d, SCL:%d", i2c_port, sda_pin, scl_pin);

    // 3. Create the BME280 sensor handle using the component's API
    rg_bme280->bme280_handle = bme280_create(rg_bme280->i2c_bus_handle, bme280_addr);
    if (rg_bme280->bme280_handle == NULL) {
        ESP_LOGE(TAG, "Failed to create BME280 sensor handle.");
        // Cleanup I2C bus on failure
        i2c_bus_delete(&rg_bme280->i2c_bus_handle);
        rg_bme280->i2c_bus_handle = NULL;
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "BME280 sensor handle created.");


    // 4. Initialize the BME280 sensor with default settings using the component's API
    // Note: bme280_default_init often puts the sensor into a specific mode (e.g., Normal)
    ret = bme280_default_init(rg_bme280->bme280_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize BME280 with default settings: %s", esp_err_to_name(ret));
        // Cleanup BME280 handle and I2C bus on failure
        bme280_delete(&rg_bme280->bme280_handle);
        rg_bme280->bme280_handle = NULL;
        i2c_bus_delete(&rg_bme280->i2c_bus_handle);
        rg_bme280->i2c_bus_handle = NULL;
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "BME280 initialization successful.");

    // *** Add a delay here to allow the first measurement cycle to complete ***
    // The default settings likely put it in Normal mode with a 1000ms standby.
    // Wait slightly longer than the expected measurement period (standby time).
    vTaskDelay(pdMS_TO_TICKS(1100)); // Wait 1.1 seconds

    return ESP_OK;
}

// Read compensated temperature, pressure, and humidity from the BME280 sensor
esp_err_t rg_bme280_read_values(rg_bme280_t *rg_bme280, rg_bme280_values_t *values)
{
    if (!rg_bme280 || !values || !rg_bme280->bme280_handle) {
        ESP_LOGE(TAG, "Invalid arguments or BME280 handle not initialized.");
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret_temp = ESP_FAIL, ret_press = ESP_FAIL, ret_hum = ESP_FAIL;

    // Use the component's individual read functions
    ret_temp = bme280_read_temperature(rg_bme280->bme280_handle, &values->temperature);
    ret_hum = bme280_read_humidity(rg_bme280->bme280_handle, &values->humidity);
    ret_press = bme280_read_pressure(rg_bme280->bme280_handle, &values->pressure);

    if (ret_temp != ESP_OK || ret_hum != ESP_OK || ret_press != ESP_OK) {
         ESP_LOGE(TAG, "Failed to read BME280 sensor data.");
         // You could log which specific read failed here if needed
         return ESP_FAIL; // Return failure if any read failed
    }


    // If all reads were successful
    return ESP_OK;
}

// Deinitializes the BME280 sensor module, freeing allocated resources.
void rg_bme280_deinit(rg_bme280_t *rg_bme280)
{
    if (rg_bme280) {
        if (rg_bme280->bme280_handle) {
             bme280_delete(&rg_bme280->bme280_handle);
             rg_bme280->bme280_handle = NULL;
        }
        // The older API i2c_bus_delete also takes a pointer to the handle.
        if (rg_bme280->i2c_bus_handle) {
             i2c_bus_delete(&rg_bme280->i2c_bus_handle);
             rg_bme280->i2c_bus_handle = NULL;
        }
        ESP_LOGI(TAG, "BME280 module deinitialized.");
    }
}