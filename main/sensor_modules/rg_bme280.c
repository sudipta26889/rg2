// main/sensor_modules/rg_bme280.c
#include "rg_bme280.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h" // Required for vTaskDelayUs in bme280_dev.delay_us

static const char *TAG = "RG_BME280";

esp_err_t rg_bme280_init(rg_bme280_t *rg_bme280, i2c_port_t i2c_port, gpio_num_t sda_pin, gpio_num_t scl_pin, uint8_t bme280_addr)
{
    if (!rg_bme280) {
        ESP_LOGE(TAG, "rg_bme280 context pointer is NULL.");
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret;

    // 1. Create I2C master bus
    i2c_master_bus_config_t i2c_bus_config = {
        .i2c_port = i2c_port,
        .sda_io_num = sda_pin,
        .scl_io_num = scl_pin,
        .clk_src = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7, // Recommended value for robustness
        .flags.enable_pullup = true,
    };
    ret = i2c_new_master_bus(&i2c_bus_config, &rg_bme280->i2c_bus_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create I2C master bus: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "I2C master bus created on port %d, SDA:%d, SCL:%d", i2c_port, sda_pin, scl_pin);


    // 2. Add I2C device for BME280 to the bus
    i2c_device_config_t i2c_dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = bme280_addr,
        .scl_speed_hz = 100000, // 100 KHz, can be increased to 400KHz if needed
    };
    ret = i2c_master_bus_add_device(rg_bme280->i2c_bus_handle, &i2c_dev_config, &rg_bme280->i2c_dev_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add I2C device (BME280 at 0x%02X): %s", bme280_addr, esp_err_to_name(ret));
        // Cleanup bus if device add fails
        i2c_del_master_bus(rg_bme280->i2c_bus_handle);
        return ret;
    }
    ESP_LOGI(TAG, "BME280 I2C device added to bus.");

    // 3. Initialize the BME280 device struct for the component
    rg_bme280->bme280_dev.dev_id = bme280_addr;
    rg_bme280->bme280_dev.intf_ptr = rg_bme280->i2c_dev_handle; // Use the I2C device handle
    rg_bme280->bme280_dev.intf = BME280_I2C_INTF;
    rg_bme280->bme280_dev.delay_us = vTaskDelayUs; // Use FreeRTOS delay for `delay_us` callback

    // 4. Initialize the BME280 sensor itself
    ret = bme280_init_sensor(&rg_bme280->bme280_dev);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize BME280 sensor (component init): %s", esp_err_to_name(ret));
        // Cleanup device and bus
        i2c_master_bus_rm_device(rg_bme280->i2c_dev_handle);
        i2c_del_master_bus(rg_bme280->i2c_bus_handle);
        return ret;
    }
    ESP_LOGI(TAG, "BME280 sensor (component) initialized.");

    // 5. Configure BME280 sensor settings
    uint8_t settings_sel = BME280_OSR_PRESS_SEL | BME280_OSR_TEMP_SEL | BME280_OSR_HUM_SEL | BME280_FILTER_SEL | BME280_STANDBY_SEL;
    rg_bme280->bme280_dev.settings.osr_h = BME280_OVERSAMPLING_1X;
    rg_bme280->bme280_dev.settings.osr_p = BME280_OVERSAMPLING_1X;
    rg_bme280->bme280_dev.settings.osr_t = BME280_OVERSAMPLING_1X;
    rg_bme280->bme280_dev.settings.filter = BME280_FILTER_COEFF_OFF;
    rg_bme280->bme280_dev.settings.standby_time = BME280_STANDBY_TIME_1000_MS; // 1 second standby

    ret = bme280_set_sensor_settings(settings_sel, &rg_bme280->bme280_dev);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set BME280 sensor settings: %s", esp_err_to_name(ret));
        // Cleanup device and bus
        i2c_master_bus_rm_device(rg_bme280->i2c_dev_handle);
        i2c_del_master_bus(rg_bme280->i2c_bus_handle);
        return ret;
    }
    ESP_LOGI(TAG, "BME280 sensor settings applied.");

    // 6. Set sensor mode to normal
    ret = bme280_set_sensor_mode(BME280_NORMAL_MODE, &rg_bme280->bme280_dev);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set BME280 sensor mode: %s", esp_err_to_name(ret));
        // Cleanup device and bus
        i2c_master_bus_rm_device(rg_bme280->i2c_dev_handle);
        i2c_del_master_bus(rg_bme280->i2c_bus_handle);
        return ret;
    }
    ESP_LOGI(TAG, "BME280 sensor mode set to normal.");

    ESP_LOGI(TAG, "BME280 sensor initialization complete.");
    return ESP_OK;
}

esp_err_t rg_bme280_read_values(rg_bme280_t *rg_bme280, rg_bme280_values_t *values)
{
    if (!rg_bme280 || !values) {
        ESP_LOGE(TAG, "Invalid arguments: rg_bme280 or values pointer is NULL.");
        return ESP_ERR_INVALID_ARG;
    }

    bme280_data_t comp_data;
    esp_err_t ret = bme280_get_comp_data(&comp_data, &rg_bme280->bme280_dev);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get BME280 sensor data: %s", esp_err_to_name(ret));
        return ret;
    }

    values->temperature = comp_data.temperature;
    values->pressure = comp_data.pressure / 100.0f; // Convert Pa to hPa
    values->humidity = comp_data.humidity;

    return ESP_OK;
}