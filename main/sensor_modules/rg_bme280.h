// main/sensor_modules/rg_bme280.h
#ifndef RG_BME280_H_
#define RG_BME280_H_

#pragma once

#include "esp_err.h"
// Include the BME280 component's header - it includes i2c_bus.h
#include "bme280.h"
// Include GPIO driver header for gpio_num_t if needed by init function
#include "driver/gpio.h"
// Include i2c_bus.h explicitly for i2c_bus_handle_t definition
#include "i2c_bus.h"


// Include Kconfig header to access sensor configuration options
#include <sdkconfig.h>


// Structure to hold the I2C bus handle and the BME280 component's handle
typedef struct {
    i2c_bus_handle_t i2c_bus_handle; // Store the I2C bus handle
    bme280_handle_t bme280_handle; // Use the component's opaque sensor handle
} rg_bme280_t;

// Structure to hold BME280 sensor readings in desired units
typedef struct {
    float temperature; // Temperature in Celsius
    float pressure;    // Pressure in hPa (hectoPascals)
    float humidity;    // Humidity in %RH
} rg_bme280_values_t;


// Use extern "C" to prevent C++ name mangling for these C functions
#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initializes the BME280 sensor module using the espressif/bme280 component's API.
 *
 * Sets up the I2C bus using the older i2c_bus API and initializes the BME280 sensor.
 * Uses Kconfig for I2C pin configuration.
 *
 * @param rg_bme280 Pointer to an allocated rg_bme280_t structure to hold the instance data.
 * @return ESP_OK on success, error code otherwise.
 */
esp_err_t rg_bme280_init(rg_bme280_t *rg_bme280);

/**
 * @brief Reads compensated temperature, pressure, and humidity from the BME280 sensor.
 *
 * Uses the espressif/bme280 component's API to read individual sensor values.
 *
 * @param rg_bme280 Pointer to the initialized rg_bme280_t structure containing the sensor handle.
 * @param values Pointer to an rg_bme280_values_t structure to store the readings.
 * @return ESP_OK on success, error code otherwise.
 */
esp_err_t rg_bme280_read_values(rg_bme280_t *rg_bme280, rg_bme280_values_t *values);

/**
 * @brief Deinitializes the BME280 sensor module, freeing allocated resources.
 *
 * Deletes the BME280 sensor handle and the I2C bus handle.
 *
 * @param rg_bme280 Pointer to the rg_bme280_t structure to deinitialize.
 */
void rg_bme280_deinit(rg_bme280_t *rg_bme280);

#ifdef __cplusplus
} // extern "C"
#endif


// Define BME280 I2C addresses (these are from the bme280 component's common defines)
#define RG_BME280_I2C_ADDR_PRIM BME280_I2C_ADDRESS_DEFAULT // Use component's default
#define RG_BME280_I2C_ADDR_SEC  0x77 // Secondary address if needed


#endif /* RG_BME280_H_ */