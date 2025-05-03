#include "rg_zigbee.h"
#include "esp_log.h"
#include "esp_zigbee_core.h"
#include "zcl/esp_zigbee_zcl_common.h"
#include "constants.h"

// Cluster IDs and attribute IDs are defined in esp_zigbee_zcl_common.h

static const std::string ZTEMPTAG = std::string(DEVICE_NAME) + "-" + DEVICE_VERSION + "::Zigbee";
static const char *RG_ZB_TAG = ZTEMPTAG.c_str();

// Zigbee attribute storage (global variables)
int16_t zb_temperature_value = 0; // 0.01 degC
uint16_t zb_humidity_value = 0;   // 0.01 %RH

// Helper function to create temperature measurement cluster attribute list
static esp_zb_attribute_list_t *create_temperature_cluster(void)
{
    esp_zb_attribute_list_t *cluster = esp_zb_zcl_attr_list_create(ESP_ZB_ZCL_CLUSTER_ID_TEMP_MEASUREMENT);
    esp_zb_cluster_add_attr(cluster,
                           ESP_ZB_ZCL_CLUSTER_ID_TEMP_MEASUREMENT,
                           ESP_ZB_ZCL_ATTR_TEMP_MEASUREMENT_VALUE_ID,
                           ESP_ZB_ZCL_ATTR_TYPE_U16,
                           ESP_ZB_ZCL_ATTR_ACCESS_READ_ONLY,
                           &zb_temperature_value);
    return cluster;
}

// Helper function to create humidity measurement cluster attribute list
static esp_zb_attribute_list_t *create_humidity_cluster(void)
{
    esp_zb_attribute_list_t *cluster = esp_zb_zcl_attr_list_create(ESP_ZB_ZCL_CLUSTER_ID_REL_HUMIDITY_MEASUREMENT);
    esp_zb_cluster_add_attr(cluster,
                           ESP_ZB_ZCL_CLUSTER_ID_REL_HUMIDITY_MEASUREMENT,
                           ESP_ZB_ZCL_ATTR_REL_HUMIDITY_MEASUREMENT_VALUE_ID,
                           ESP_ZB_ZCL_ATTR_TYPE_U16,
                           ESP_ZB_ZCL_ATTR_ACCESS_READ_ONLY,
                           &zb_humidity_value);
    return cluster;
}

// Create Zigbee cluster list with temperature and humidity clusters
static esp_zb_cluster_list_t *create_zigbee_cluster_list(void)
{
    esp_zb_cluster_list_t *cluster_list = esp_zb_zcl_cluster_list_create();

    esp_zb_attribute_list_t *temp_cluster = create_temperature_cluster();
    esp_zb_cluster_list_add_custom_cluster(cluster_list, temp_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);

    esp_zb_attribute_list_t *humidity_cluster = create_humidity_cluster();
    esp_zb_cluster_list_add_custom_cluster(cluster_list, humidity_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);

    return cluster_list;
}

void zigbee_init(void)
{
    esp_zb_cfg_t zb_nwk_cfg = {
        .esp_zb_role = ESP_ZB_DEVICE_TYPE_ROUTER,
        .install_code_policy = false,
        .nwk_cfg = {
            .zczr_cfg = {
                .max_children = 10,
            },
        },
    };

    esp_zb_init(&zb_nwk_cfg);

    esp_zb_ep_list_t *ep_list = esp_zb_ep_list_create();
    if (!ep_list) {
        ESP_LOGE(RG_ZB_TAG, "Failed to create endpoint list");
        return;
    }

    esp_zb_cluster_list_t *cluster_list = create_zigbee_cluster_list();

    esp_zb_endpoint_config_t endpoint_config = {
        .endpoint = HA_SENSOR_ENDPOINT,
        .app_profile_id = ESP_ZB_AF_HA_PROFILE_ID,
        .app_device_id = ESP_ZB_HA_TEMPERATURE_SENSOR_DEVICE_ID,
        .app_device_version = 0,
    };

    esp_zb_ep_list_add_ep(ep_list, cluster_list, endpoint_config);

    ESP_ERROR_CHECK(esp_zb_device_register(ep_list));

    ESP_ERROR_CHECK(esp_zb_start(true));

    ESP_LOGI(RG_ZB_TAG, "Zigbee router initialized successfully");
}

void zigbee_update_sensor_values(float temperature, float humidity)
{
    // Convert to Zigbee format: 0.01 units
    zb_temperature_value = (int16_t)(temperature * 100);
    zb_humidity_value = (uint16_t)(humidity * 100);

    // Optionally, trigger attribute reporting here if your app supports it
    // esp_zb_zcl_set_attr_value(HA_SENSOR_ENDPOINT, ESP_ZB_ZCL_CLUSTER_ID_TEMP_MEASUREMENT,
    //                           ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, ESP_ZB_ZCL_ATTR_TEMP_MEASUREMENT_VALUE_ID,
    //                           &zb_temperature_value);
    //
    // esp_zb_zcl_set_attr_value(HA_SENSOR_ENDPOINT, ESP_ZB_ZCL_CLUSTER_ID_REL_HUMIDITY_MEASUREMENT,
    //                           ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, ESP_ZB_ZCL_ATTR_REL_HUMIDITY_MEASUREMENT_VALUE_ID,
    //                           &zb_humidity_value);
}

void esp_zb_app_signal_handler(esp_zb_app_signal_t *signal_struct)
{
    esp_zb_app_signal_type_t sig_type = static_cast<esp_zb_app_signal_type_t>(*(signal_struct->p_app_signal));
    esp_err_t err_status = signal_struct->esp_err_status;

    switch (sig_type) {
        case ESP_ZB_ZDO_SIGNAL_SKIP_STARTUP:
            ESP_LOGI(RG_ZB_TAG, "Zigbee stack initialized");
            break;
        case ESP_ZB_BDB_SIGNAL_DEVICE_FIRST_START:
            // Device started for the first time
            ESP_LOGI(RG_ZB_TAG, "First start");
            break;
        case ESP_ZB_BDB_SIGNAL_STEERING:
            if (err_status == ESP_OK) {
                ESP_LOGI(RG_ZB_TAG, "Joined network successfully");
            } else {
                ESP_LOGI(RG_ZB_TAG, "Failed to join network (status: %s)", esp_err_to_name(err_status));
            }
            break;
        default:
            ESP_LOGI(RG_ZB_TAG, "ZDO signal: %s (0x%x), status: %s",
                     esp_zb_zdo_signal_to_string(sig_type), sig_type, esp_err_to_name(err_status));
            break;
    }
}