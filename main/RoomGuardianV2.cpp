#include <stdio.h>
#include "esp_check.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_mac.h"
#include "esp_http_server.h"
#include "freertos/FreeRTOS.h"
#include "wifi_provisioning/manager.h"
#include "wifi_provisioning/scheme_softap.h"
#include "nvs_flash.h"
#include "qrcode.h"
#include "freertos/task.h"
#include <nlohmann/json.hpp>
#include "bme280.h"
#include "zigbee_app.h"
#include "zcl/esp_zigbee_zcl_basic.h"
#include "zcl/esp_zigbee_zcl_identify.h"
#include "ha/esp_zigbee_ha_standard.h"


#define MAX_RETRY_NUM 5
static const char* TAG = "RoomGuardianV2";

#define I2C_MASTER_SCL_IO           GPIO_NUM_5
#define I2C_MASTER_SDA_IO           GPIO_NUM_4
#define I2C_MASTER_NUM              I2C_NUM_0
#define I2C_MASTER_FREQ_HZ          100000

static i2c_bus_handle_t i2c_bus = NULL;
static bme280_handle_t bme280 = NULL;
static float temperature = 0.0, humidity = 0.0, pressure = 0.0;
static char ip_address[16] = "0.0.0.0"; // To store the IP address

// Zigbee cluster and endpoint configuration
static esp_zb_cluster_list_t *cluster_list = esp_zb_zcl_cluster_list_create();

/* Zigbee configuration */
#define INSTALLCODE_POLICY_ENABLE       false   /* Enable the install code policy for security */
#define HA_ESP_SENSOR_ENDPOINT          10      /* Endpoint for temperature measurement */
#define ESP_ZB_PRIMARY_CHANNEL_MASK     ESP_ZB_TRANSCEIVER_ALL_CHANNELS_MASK    /* Zigbee primary channel mask */

esp_zb_ep_list_t *ep_list = esp_zb_ep_list_create();

// Zigbee initialization
static void zigbee_init(void)
{
    esp_zb_cfg_t zb_config = ESP_ZB_ZED_CONFIG();

    esp_zb_init(&zb_config);
    ESP_ERROR_CHECK(esp_zb_device_register(ep_list));
    ESP_ERROR_CHECK(esp_zb_start(true));

    ESP_LOGI(TAG, "Zigbee router initialized successfully.");
}

static void bdb_start_top_level_commissioning_cb(uint8_t mode_mask)
{
    ESP_RETURN_ON_FALSE(esp_zb_bdb_start_top_level_commissioning(mode_mask) == ESP_OK, ,
                        TAG, "Failed to start Zigbee BDB commissioning");
}

void esp_zb_app_signal_handler(esp_zb_app_signal_t *signal_struct)
{
    uint32_t *p_sg_p     = signal_struct->p_app_signal;
    esp_err_t err_status = signal_struct->esp_err_status;
    esp_zb_app_signal_type_t sig_type = static_cast<esp_zb_app_signal_type_t>(*p_sg_p);
    switch (sig_type) {
    case ESP_ZB_ZDO_SIGNAL_SKIP_STARTUP:
        ESP_LOGI(TAG, "Initialize Zigbee stack");
        esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_INITIALIZATION);
        break;
    case ESP_ZB_BDB_SIGNAL_DEVICE_FIRST_START:
    case ESP_ZB_BDB_SIGNAL_DEVICE_REBOOT:
        if (err_status == ESP_OK) {
            ESP_LOGI(TAG, "Device started up in%s factory-reset mode", esp_zb_bdb_is_factory_new() ? "" : " non");
            if (esp_zb_bdb_is_factory_new()) {
                ESP_LOGI(TAG, "Start network steering");
                esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_NETWORK_STEERING);
            } else {
                ESP_LOGI(TAG, "Device rebooted");
            }
        } else {
            ESP_LOGW(TAG, "%s failed with status: %s, retrying", esp_zb_zdo_signal_to_string(sig_type),
                     esp_err_to_name(err_status));
            esp_zb_scheduler_alarm((esp_zb_callback_t)bdb_start_top_level_commissioning_cb,
                                   ESP_ZB_BDB_MODE_INITIALIZATION, 1000);
        }
        break;
    case ESP_ZB_BDB_SIGNAL_STEERING:
        if (err_status == ESP_OK) {
            esp_zb_ieee_addr_t extended_pan_id;
            esp_zb_get_extended_pan_id(extended_pan_id);
            ESP_LOGI(TAG, "Joined network successfully (Extended PAN ID: %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x, PAN ID: 0x%04hx, Channel:%d, Short Address: 0x%04hx)",
                     extended_pan_id[7], extended_pan_id[6], extended_pan_id[5], extended_pan_id[4],
                     extended_pan_id[3], extended_pan_id[2], extended_pan_id[1], extended_pan_id[0],
                     esp_zb_get_pan_id(), esp_zb_get_current_channel(), esp_zb_get_short_address());
        } else {
            ESP_LOGI(TAG, "Network steering was not successful (status: %s)", esp_err_to_name(err_status));
            esp_zb_scheduler_alarm((esp_zb_callback_t)bdb_start_top_level_commissioning_cb, ESP_ZB_BDB_MODE_NETWORK_STEERING, 1000);
        }
        break;
    default:
        ESP_LOGI(TAG, "ZDO signal: %s (0x%x), status: %s", esp_zb_zdo_signal_to_string(sig_type), sig_type,
                 esp_err_to_name(err_status));
        break;
    }
}

// Function to initialize the I2C bus and BME280 sensor
static esp_err_t bme280_init(void)
{
    ESP_LOGI(TAG, "Initializing I2C bus...");

    // Configure I2C parameters
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE
    };

    conf.master.clk_speed = I2C_MASTER_FREQ_HZ; 

    // Create the I2C bus
    i2c_bus = i2c_bus_create(I2C_MASTER_NUM, &conf);
    if (i2c_bus == NULL) {
        ESP_LOGE(TAG, "Failed to create I2C bus.");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Initializing BME280 sensor...");

    // Create the BME280 sensor handle
    bme280 = bme280_create(i2c_bus, BME280_I2C_ADDRESS_DEFAULT); // Use 0x77 if needed
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

// Function to read sensor data
static void bme280_read_data(void)
{
    if (bme280_read_temperature(bme280, &temperature) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read temperature.");
    }
    if (bme280_read_humidity(bme280, &humidity) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read humidity.");
    }
    if (bme280_read_pressure(bme280, &pressure) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read pressure.");
    }
}

// HTTP GET handler
static esp_err_t http_get_handler(httpd_req_t *req)
{
    // Larger response buffer
    char response[1024];
    
    // Note that we're not including any dynamic data here inside the JavaScript code
    snprintf(response, sizeof(response),
             "<html><body>"
             "<h1>RoomGuardian V2 Sensor Data</h1>"
             "<p id=\"ip_address\">IP Address: %s</p>"
             "<p id=\"temperature\">Temperature: %.2f°C</p>"
             "<p id=\"humidity\">Humidity: %.2f%%</p>"
             "<p id=\"pressure\">Pressure: %.2f hPa</p>"
             "<script>"
             "setInterval(() => {"
             "    fetch('/data').then(response => response.json()).then(data => {"
             "        document.getElementById('ip_address').innerText = 'IP Address: ' + data.ip_address;"
             "        document.getElementById('temperature').innerText = 'Temperature: ' + data.temperature.toFixed(2) + '°C';"
             "        document.getElementById('humidity').innerText = 'Humidity: ' + data.humidity.toFixed(2) + '%%';" // Double %% escapes the %
             "        document.getElementById('pressure').innerText = 'Pressure: ' + data.pressure.toFixed(2) + ' hPa';"
             "    });"
             "}, 5000);"
             "</script>"
             "</body></html>",
             ip_address, temperature, humidity, pressure);

    // Set response type and send response
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, response, strlen(response));
    return ESP_OK;
}

// HTTP GET handler for sensor data
static esp_err_t http_get_data_handler(httpd_req_t *req)
{
    nlohmann::json json_data;
    json_data["temperature"] = temperature;
    json_data["humidity"] = humidity;
    json_data["pressure"] = pressure;
    json_data["ip_address"] = ip_address;

    std::string response = json_data.dump();
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, response.c_str(), response.size());
    return ESP_OK;
}

// Start the HTTP server
static httpd_handle_t start_webserver(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t server = NULL;

    if (httpd_start(&server, &config) == ESP_OK) {
        // Register handler for the main page
        httpd_uri_t uri_get = {
            .uri = "/",
            .method = HTTP_GET,
            .handler = http_get_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &uri_get);

        // Register handler for sensor data
        httpd_uri_t uri_data_get = {
            .uri = "/data",
            .method = HTTP_GET,
            .handler = http_get_data_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &uri_data_get);
    }
    return server;
}

void print_qr(void)
{
    nlohmann::json qr_string = 
    {
        {"ver","v1"},
        {"name","PROV_ESP"},
        {"pop","abcd1234"},
        {"transport","softap"}
    };
    esp_qrcode_config_t qr = ESP_QRCODE_CONFIG_DEFAULT();
    esp_qrcode_generate(&qr,qr_string.dump().c_str());
}
static void wifi_prov_handler(void *user_data, wifi_prov_cb_event_t event, void *event_data)
{
    switch(event)
    {
        case WIFI_PROV_START :
        ESP_LOGI(TAG,"[WIFI_PROV_START]");
        break;
        case WIFI_PROV_CRED_RECV :
        ESP_LOGI(TAG ,"cred : ssid : %s pass : %s",((wifi_sta_config_t*)event_data)->ssid,((wifi_sta_config_t*)event_data)->password);
        break;
        case WIFI_PROV_CRED_SUCCESS :
        ESP_LOGI(TAG,"prov success");
        wifi_prov_mgr_stop_provisioning();
        break;
        case WIFI_PROV_CRED_FAIL :
        ESP_LOGE(TAG,"credentials worng");
        wifi_prov_mgr_reset_sm_state_on_failure();
        print_qr();
        break;
        case WIFI_PROV_END: 
        ESP_LOGI(TAG,"prov ended");
        wifi_prov_mgr_deinit();
        break;
        default : break;
    }
}
static void wifi_event_handler(void* event_handler_arg,
                                        esp_event_base_t event_base,
                                        int32_t event_id,
                                        void* event_data)
{
    static int retry_cnt =0 ;
    if(event_base == WIFI_EVENT)
    {
        switch(event_id)
        {
            case WIFI_EVENT_STA_START :
                esp_wifi_connect();
                break;
            case WIFI_EVENT_STA_DISCONNECTED :
                ESP_LOGE(TAG,"ESP32 Disconnected, retrying");
                retry_cnt ++;
                if(retry_cnt < MAX_RETRY_NUM)
                {
                    esp_wifi_connect();
                }
                else ESP_LOGE(TAG,"Connection error");
                break;
            default : break;
        }
    }
    else if(event_base == IP_EVENT)
    {
        if(event_id == IP_EVENT_STA_GOT_IP)
        {
            ip_event_got_ip_t* event = (ip_event_got_ip_t*)event_data;
            ESP_LOGI(TAG, "station ip :"IPSTR, IP2STR(&event->ip_info.ip));
            esp_ip4addr_ntoa(&event->ip_info.ip, ip_address, sizeof(ip_address));
        }
    }
}
void wifi_hw_init(void)
{
    nvs_flash_init();
    esp_netif_init();
    esp_event_loop_create_default();
    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    esp_event_handler_instance_register(WIFI_EVENT,ESP_EVENT_ANY_ID,&wifi_event_handler,NULL,&instance_any_id);
    esp_event_handler_instance_register(IP_EVENT,IP_EVENT_STA_GOT_IP,&wifi_event_handler,NULL,&instance_got_ip);
    esp_netif_create_default_wifi_sta();
    esp_netif_create_default_wifi_ap();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    esp_wifi_start();
 

}
static void prov_start(void)
{
    wifi_prov_mgr_config_t cfg = 
    {
        .scheme =wifi_prov_scheme_softap,
        .scheme_event_handler = WIFI_PROV_EVENT_HANDLER_NONE,
        .app_event_handler = wifi_prov_handler,
        
      
    };
    wifi_prov_mgr_init(cfg);
    bool is_provisioned = 0;
    wifi_prov_mgr_is_provisioned(&is_provisioned);
    wifi_prov_mgr_disable_auto_stop(100);
    if(is_provisioned)
    {
        ESP_LOGI(TAG,"Already provisioned");
        esp_wifi_set_mode(WIFI_MODE_STA);
        esp_wifi_start();
    }
    else
    {
        wifi_prov_mgr_start_provisioning(WIFI_PROV_SECURITY_1,"abcd1234","PROV_ESP",NULL);
        print_qr();

    }

}
extern "C" void app_main(void)
{
    wifi_hw_init();
    prov_start();

    ESP_ERROR_CHECK(bme280_init());
    zigbee_init();
    start_webserver();
    int i = 0;
    while (1) {
        bme280_read_data();
        ESP_LOGI(TAG, "Temperature: %.2f°C, Humidity: %.2f%%, Pressure: %.2f hPa IP: %s", temperature, humidity, pressure, ip_address);
        vTaskDelay(pdMS_TO_TICKS(5000)); // Update every 5 seconds
        i++;
    }
    // bme280_deinit();
}