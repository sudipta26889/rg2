#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_bt.h"
#include "esp_zboss_api.h"
#include "esp_openthread.h"
#include "esp_matter.h"
#include "bme280.h"
#include "driver/i2c.h"

static const char *TAG = "MultiProtoSensor";

// Hardware Configuration
#define I2C_PORT        I2C_NUM_0
#define SDA_PIN         7
#define SCL_PIN         8
#define BME280_ADDR     BME280_I2C_ADDR_0

// Matter Configuration
static esp_matter_node_t *matter_node;
static uint16_t matter_endpoint_id;

// Protocol Handlers
static esp_zb_platform_config_t zb_config = ESP_ZB_PLATFORM_CONFIG();
static esp_openthread_platform_config_t ot_config = ESP_OPENTHREAD_DEFAULT_PLATFORM_CONFIG();

// BME280 Initialization
static esp_err_t init_bme280(bme280_t *dev) {
    dev->i2c_dev.bus = I2C_PORT;
    dev->i2c_dev.addr = BME280_ADDR;
    return bme280_init(dev);
}

// Wi-Fi Provisioning Setup
static void start_wifi_provisioning() {
    wifi_prov_mgr_config_t config = {
        .scheme = wifi_prov_scheme_ble,
        .scheme_event_handler = WIFI_PROV_SCHEME_BLE_EVENT_HANDLER_FREE_BTDM
    };
    
    wifi_prov_mgr_init(config);
    wifi_prov_security_t security = WIFI_PROV_SECURITY_1;
    wifi_prov_security1_params_t sec_params = { .data = NULL };
    
    wifi_prov_mgr_start_provisioning(security, (const void *)&sec_params, 
                                    "ESP32C6-Sensor", NULL);
}

// Matter Attribute Update
static void update_matter_attributes(float temp, float press, float hum) {
    esp_matter_attr_val_t val = esp_matter_nullable_float(temp);
    esp_matter_attribute_update(matter_endpoint_id, 
                               ESP_MATTER_CLUSTER_TEMPERATURE_MEASUREMENT_ID,
                               ESP_MATTER_ATTRIBUTE_MEASURED_VALUE, 
                               &val);
}

// Zigbee Attribute Reporting
static void zigbee_report_attributes(float temp, float hum) {
    esp_zb_zcl_attribute_list_t attr_list = {};
    attr_list.attr_count = 2;
    attr_list.attr_id[0] = ESP_ZB_ZCL_ATTR_TEMPERATURE_MEASUREMENT_VALUE_ID;
    attr_list.attr_type[0] = ESP_ZB_ZCL_ATTR_TYPE_FLOAT;
    attr_list.attr_value[0] = (uint8_t *)&temp;
    
    attr_list.attr_id[1] = ESP_ZB_ZCL_ATTR_HUMIDITY_MEASUREMENT_VALUE_ID;
    attr_list.attr_type[1] = ESP_ZB_ZCL_ATTR_TYPE_FLOAT;
    attr_list.attr_value[1] = (uint8_t *)&hum;
    
    esp_zb_zcl_report_attr_cmd(0x0001, &attr_list);
}

// Main Application
void app_main(void) 
{
    // Initialize NVS
    ESP_ERROR_CHECK(nvs_flash_init());
    
    // Configure I2C
    i2c_config_t i2c_conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = SDA_PIN,
        .scl_io_num = SCL_PIN,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 400000
    };
    ESP_ERROR_CHECK(i2c_param_config(I2C_PORT, &i2c_conf));
    ESP_ERROR_CHECK(i2c_driver_install(I2C_PORT, I2C_MODE_MASTER, 0, 0, 0));

    // Initialize BME280
    bme280_t bme280;
    ESP_ERROR_CHECK(init_bme280(&bme280));

    // Configure RF Coexistence
    ESP_ERROR_CHECK(esp_coex_preference_set(ESP_COEX_PREFER_BALANCED));
    ESP_ERROR_CHECK(esp_coex_wifi_chan_override_set(1, 11));

    // Initialize Protocols
    ESP_ERROR_CHECK(esp_zb_init(&zb_config));
    ESP_ERROR_CHECK(esp_openthread_init(&ot_config));
    
    // Matter Initialization
    esp_matter_node_config_t node_config = esp_matter_node_get_config();
    matter_node = esp_matter_node_create(&node_config);
    matter_endpoint_id = esp_matter_endpoint_create(matter_node);
    esp_matter_init(matter_node);
    esp_matter_commissioning_enable();

    // Start Protocol Stacks
    start_wifi_provisioning();
    esp_zb_start(true); // Start Zigbee
    esp_openthread_auto_start(); // Start Thread

    // Main Sensor Loop
    while(1) {
        float temp, press, hum;
        if(bme280_read_float(&bme280, &temp, &press, &hum) == ESP_OK) {
            ESP_LOGI(TAG, "Temp: %.1fC, Press: %.1fhPa, Hum: %.1f%%", 
                    temp, press, hum);
            
            // Update all protocols
            update_matter_attributes(temp, press, hum);
            zigbee_report_attributes(temp, hum);
            
            // Thread Update (using OpenThread CLI)
            otCliOutputFormat("sensor update temp %.1f press %.1f hum %.1f", 
                            temp, press, hum);
        }
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
