#include "rg_matter.h"
#include "shared_data/shared_data.h"

#define MATTER_ENDPOINT 1
static esp_matter::node_t *node;

void matter_init() {
    esp_matter::node_config_t node_config;
    node_config.role = ESP_MATTER_NODE_ROLE_END_DEVICE;
    
    // Initialize with Wi-Fi first
    esp_matter::wifi::config_t wifi_config;
    esp_matter::start(node_config, &wifi_config);
    
    node = esp_matter::node::create();
    
    // Temperature cluster (0x0402)
    esp_matter::endpoint::temperature_sensor::config_t temp_config;
    esp_matter::endpoint::temperature_sensor::create(node, &temp_config, MATTER_ENDPOINT);
    
    // Humidity cluster (0x0405)
    esp_matter::endpoint::humidity_sensor::config_t humi_config;
    esp_matter::endpoint::humidity_sensor::create(node, &humi_config, MATTER_ENDPOINT+1);
}
