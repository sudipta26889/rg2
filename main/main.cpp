// main/main.cpp
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <nvs_flash.h>
#include <sdkconfig.h> // Include sdkconfig.h to access CONFIG_ macros

// Wi-Fi specific includes
#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_netif.h>

// BME280 sensor include (adjusted path for main/sensor_modules)
#include "rg_bme280.h"

// Matter specific includes
#include <esp_matter.h>
#include <esp_matter_core.h>
#include <esp_matter_endpoint.h>
#include <esp_matter_attribute.h>
#include <esp_matter_identify.h>
#include <esp_matter_temperature_sensor.h>
#include <esp_matter_humidity_sensor.h>
// You might add other Matter cluster headers here if your device has more features
// For example:
// #include <esp_matter_on_off.h>

// Define tags for logging
static const char *TAG = "APP_MAIN";
static const char *WIFI_TAG = "WIFI";
static const char *MATTER_TAG = "MATTER";

// BME280 I2C Address and Port (can be Kconfig'd if you need more flexibility)
#define BME280_I2C_ADDR BME280_I2C_ADDR_PRIM // 0x76 (primary address for BME280) or BME280_I2C_ADDR_SEC (0x77)
#define BME280_I2C_PORT I2C_NUM_0            // I2C port, typically I2C_NUM_0 for single bus

// Global instance for the BME280 sensor context
static rg_bme280_t s_bme280_sensor_context;

// Matter endpoint and attribute value handles
static esp_matter_endpoint_t *s_bme280_sensor_endpoint = nullptr;
static esp_matter_attr_val_t s_temperature_measured_value;
static esp_matter_attr_val_t s_humidity_measured_value;

// --- Wi-Fi Initialization ---
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGW(WIFI_TAG, "Wi-Fi disconnected. Retrying in 5s...");
        vTaskDelay(pdMS_TO_TICKS(5000)); // Short delay before retry
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = static_cast<ip_event_got_ip_t*>(event_data);
        ESP_LOGI(WIFI_TAG, "Wi-Fi connected! Got IP address: " IPSTR, IP2STR(&event->ip_info.ip));
        esp_matter_commissioning_start(); // Start Matter commissioning after getting IP
    }
}

static void wifi_init_sta()
{
    // 1. Initialize TCP/IP adapter
    ESP_ERROR_CHECK(esp_netif_init());

    // 2. Create default event loop
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // 3. Create Wi-Fi station network interface
    esp_netif_create_default_wifi_sta();

    // 4. Configure Wi-Fi
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // 5. Register event handlers
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, nullptr));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, nullptr));

    // 6. Set Wi-Fi configuration
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = CONFIG_ESP_WIFI_SSID,        // Defined in sdkconfig
            .password = CONFIG_ESP_WIFI_PASSWORD, // Defined in sdkconfig
            .threshold.authmode = WIFI_AUTH_WPA2_PSK, // Or WIFI_AUTH_WPA_WPA2_PSK, etc.
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));

    // 7. Start Wi-Fi
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(WIFI_TAG, "Wi-Fi station initialization finished.");
}

// --- Matter Application Data and Callback ---
static esp_err_t app_matter_event_cb(esp_matter_event_t *event)
{
    switch (event->type) {
    case ESP_MATTER_EVENT_COMMISSIONING_STARTED:
        ESP_LOGI(MATTER_TAG, "Matter Commissioning Started");
        // Your device might want to indicate this state (e.g., LED blinking)
        break;
    case ESP_MATTER_EVENT_COMMISSIONING_COMPLETE:
        ESP_LOGI(MATTER_TAG, "Matter Commissioning Complete");
        // Your device might want to indicate success (e.g., LED solid)
        break;
    case ESP_MATTER_EVENT_COMMISSIONING_FAILED:
        ESP_LOGE(MATTER_TAG, "Matter Commissioning Failed. Error: %d", static_cast<int>(event->error));
        // Your device might want to indicate failure and restart commissioning
        esp_matter_commissioning_start();
        break;
    case ESP_MATTER_EVENT_ATTRIBUTE_WRITE:
        ESP_LOGD(MATTER_TAG, "Attribute Write received: Endpoint %d, Cluster 0x%lX, Attribute 0x%lX",
                 static_cast<int>(event->attribute_write.endpoint_id),
                 static_cast<long>(event->attribute_write.cluster_id),
                 static_cast<long>(event->attribute_write.attribute_id));
        // Handle specific attribute writes if needed (e.g., Identify cluster)
        break;
    case ESP_MATTER_EVENT_DEVICE_PAIRED:
        ESP_LOGI(MATTER_TAG, "Matter Device Paired with Controller");
        break;
    case ESP_MATTER_EVENT_DEVICE_UNPAIRED:
        ESP_LOGI(MATTER_TAG, "Matter Device Unpaired from Controller");
        break;
    default:
        ESP_LOGD(MATTER_TAG, "Unhandled Matter event type: %d", static_cast<int>(event->type));
        break;
    }
    return ESP_OK;
}

// Task to read sensor data and update Matter attributes periodically
static void sensor_read_task(void *pvParameters)
{
    rg_bme280_values_t sensor_data;
    esp_err_t ret;

    while (true) {
        ret = rg_bme280_read_values(&s_bme280_sensor_context, &sensor_data);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "BME280 Readings: Temp: %.2f C, Pres: %.2f hPa, Hum: %.2f %%",
                     sensor_data.temperature, sensor_data.pressure, sensor_data.humidity);

            // Update Matter Temperature Measurement Cluster
            if (s_bme280_sensor_endpoint != nullptr) {
                esp_matter_cluster_t *temp_cluster = esp_matter_cluster_get(s_bme280_sensor_endpoint, ESP_MATTER_CLUSTER_ID_TEMPERATURE_MEASUREMENT);
                if (temp_cluster != nullptr) {
                    // Matter temperature is in 0.01 degree Celsius
                    s_temperature_measured_value = esp_matter_attr_val_temperature_measurement_measured_value_init(static_cast<int16_t>(sensor_data.temperature * 100));
                    esp_matter_attribute_update_and_report(temp_cluster, ESP_MATTER_ATTRIBUTE_ID_TEMPERATURE_MEASUREMENT_MEASURED_VALUE, &s_temperature_measured_value);
                } else {
                    ESP_LOGW(MATTER_TAG, "Temperature Measurement Cluster not found on endpoint.");
                }

                // Update Matter Humidity Measurement Cluster
                esp_matter_cluster_t *hum_cluster = esp_matter_cluster_get(s_bme280_sensor_endpoint, ESP_MATTER_CLUSTER_ID_HUMIDITY_MEASUREMENT);
                if (hum_cluster != nullptr) {
                    // Matter humidity is in 0.01 %
                    s_humidity_measured_value = esp_matter_attr_val_humidity_measurement_measured_value_init(static_cast<uint16_t>(sensor_data.humidity * 100));
                    esp_matter_attribute_update_and_report(hum_cluster, ESP_MATTER_ATTRIBUTE_ID_HUMIDITY_MEASUREMENT_MEASURED_VALUE, &s_humidity_measured_value);
                } else {
                    ESP_LOGW(MATTER_TAG, "Humidity Measurement Cluster not found on endpoint.");
                }
            } else {
                ESP_LOGW(MATTER_TAG, "BME280 Sensor Endpoint not initialized yet.");
            }
        } else {
            ESP_LOGE(TAG, "Failed to read BME280 sensor data.");
        }

        vTaskDelay(pdMS_TO_TICKS(5000)); // Read every 5 seconds
    }
}

// The main application entry point for ESP-IDF
extern "C" void app_main(void)
{
    esp_err_t ret;

    // Initialize NVS (Non-Volatile Storage)
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_LOGI(TAG, "NVS initialized successfully.");

    // Initialize Wi-Fi
    wifi_init_sta();

    // Initialize BME280 sensor using your rg_bme280 module
    // Pass the pin numbers from sdkconfig.h using the CONFIG_ macros
    ret = rg_bme280_init(
        &s_bme280_sensor_context,
        BME280_I2C_PORT,
        static_cast<gpio_num_t>(CONFIG_BME280_I2C_SDA_GPIO), // Ensure casting to gpio_num_t
        static_cast<gpio_num_t>(CONFIG_BME280_I2C_SCL_GPIO), // Ensure casting to gpio_num_t
        BME280_I2C_ADDR
    );
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize BME280 sensor. Exiting application.");
        return;
    }
    ESP_LOGI(TAG, "BME280 sensor module initialized successfully using configured pins (SDA:%d, SCL:%d).",
             CONFIG_BME280_I2C_SDA_GPIO, CONFIG_BME280_I2C_SCL_GPIO);

    // --- Matter Initialization ---
    // Create a Matter node configuration. Replace with your own unique IDs for a deployed product.
    // Vendor ID 0xFFF1 is Espressif's test VID. Product ID 0x8001 is for testing.
    // Passcode and Discriminator are for commissioning.
    esp_matter_node_config_t node_config = {
        .chip_id = 0xABCD, // A unique ID for your device instance
        .commissioning_port = 5540,
        .discriminator = 0xF00D, // Matter commissioning discriminator (12 bits)
        .passcode = 12345678,    // Matter commissioning passcode (8 digits)
        .product_id = 0x8001,    // Your product ID within your Vendor ID range
        .vendor_id = 0xFFF1,     // Your Vendor ID (Espressif test VID for development)
    };
    esp_matter_node_t *node = esp_matter_node_create(&node_config, app_matter_event_cb);
    ESP_ERROR_CHECK(esp_matter_node_add(node));
    ESP_LOGI(MATTER_TAG, "Matter Node created.");

    // Create a Matter endpoint for the BME280 sensor
    // We are using a generic "HA_TEMPERATURE_SENSOR" template from esp-matter.
    // This will automatically add common clusters like Basic, Identify, Temperature Measurement, and Power Source.
    s_bme280_sensor_endpoint = esp_matter_endpoint_create_template(node, ESP_MATTER_ENDPOINT_HA_TEMPERATURE_SENSOR);
    if (s_bme280_sensor_endpoint == nullptr) {
        ESP_LOGE(MATTER_TAG, "Failed to create BME280 sensor endpoint.");
        return;
    }
    ESP_ERROR_CHECK(esp_matter_endpoint_add(s_bme280_sensor_endpoint));
    ESP_LOGI(MATTER_TAG, "Matter Endpoint for BME280 sensor created.");

    // Add Humidity Measurement Cluster to the BME280 sensor endpoint
    // The HA_TEMPERATURE_SENSOR template doesn't include humidity by default.
    esp_matter_cluster_t *hum_cluster = esp_matter_humidity_sensor_cluster_create(s_bme280_sensor_endpoint, app_matter_event_cb);
    if (hum_cluster == nullptr) {
        ESP_LOGE(MATTER_TAG, "Failed to add Humidity Measurement Cluster.");
        return;
    }
    // Initialize humidity attribute with a dummy value (will be updated by sensor task)
    s_humidity_measured_value = esp_matter_attr_val_humidity_measurement_measured_value_init(0);
    esp_matter_attribute_update_and_report(hum_cluster, ESP_MATTER_ATTRIBUTE_ID_HUMIDITY_MEASUREMENT_MEASURED_VALUE, &s_humidity_measured_value);
    ESP_LOGI(MATTER_TAG, "Humidity Measurement Cluster added.");

    // Start the Matter stack
    ESP_ERROR_CHECK(esp_matter_start(app_matter_event_cb));
    ESP_LOGI(MATTER_TAG, "Matter stack started.");

    // Create a FreeRTOS task to read sensor data and update Matter attributes
    xTaskCreate(sensor_read_task, "sensor_read_task", 4096, nullptr, 5, nullptr);
    ESP_LOGI(TAG, "Sensor read task created.");
}