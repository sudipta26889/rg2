#pragma once

#include "esp_zigbee_core.h"
#include "zcl/esp_zigbee_zcl_common.h"
#include "esp_log.h"

// Endpoint and cluster IDs
#define HA_SENSOR_ENDPOINT 10
#define ESP_ZB_PRIMARY_CHANNEL_MASK ESP_ZB_TRANSCEIVER_ALL_CHANNELS_MASK

// Attribute variables (must be global/static)
extern int16_t zb_temperature_value;   // in 0.01 degC (Zigbee format)
extern uint16_t zb_humidity_value;     // in 0.01 %RH (Zigbee format)

// Zigbee initialization
void zigbee_init(void);

// Update Zigbee attributes from sensor readings
void zigbee_update_sensor_values(float temperature, float humidity);
