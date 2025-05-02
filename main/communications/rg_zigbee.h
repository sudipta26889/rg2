#include "esp_zigbee_core.h"
#include "esp_log.h"
#include "esp_check.h"
#include "constants.h"

static const std::string ZTEMPTAG = std::string(DEVICE_NAME) + "-" + DEVICE_VERSION + "::Zigbee";
static const char *ZB_TAG = ZTEMPTAG.c_str();

/* Zigbee configuration */
#define INSTALLCODE_POLICY_ENABLE       false   /* enable the install code policy for security */
#define ED_AGING_TIMEOUT                ESP_ZB_ED_AGING_TIMEOUT_64MIN
#define ED_KEEP_ALIVE                   3000    /* 3000 millisecond */
#define HA_ESP_SENSOR_ENDPOINT          10      /* esp temperature sensor device endpoint, used for temperature measurement */
#define ESP_ZB_PRIMARY_CHANNEL_MASK     ESP_ZB_TRANSCEIVER_ALL_CHANNELS_MASK    /* Zigbee primary channel mask use in the example */

/* Attribute values in ZCL string format
 * The string should be started with the length of its own.
 */
#define MODEL_IDENTIFIER                "\x07".CONFIG_IDF_TARGET

#define ESP_ZB_ZED_CONFIG()                                         \
    {                                                               \
        .esp_zb_role = ESP_ZB_DEVICE_TYPE_ROUTER,                   \
        .install_code_policy = INSTALLCODE_POLICY_ENABLE,           \
        .nwk_cfg = {                                                \
            .zczr_cfg = {                                           \
                .max_children = 10,                                 \
            },                                                      \
        },                                                          \
    }

#define ESP_ZB_DEFAULT_RADIO_CONFIG()                           \
    {                                                           \
        .radio_mode = ZB_RADIO_MODE_NATIVE,                     \
    }

#define ESP_ZB_DEFAULT_HOST_CONFIG()                            \
    {                                                           \
        .host_connection_mode = ZB_HOST_CONNECTION_MODE_NONE,   \
    }

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

    ESP_LOGI(ZB_TAG, "Zigbee router initialized successfully.");
}

static void bdb_start_top_level_commissioning_cb(uint8_t mode_mask)
{
    ESP_RETURN_ON_FALSE(esp_zb_bdb_start_top_level_commissioning(mode_mask) == ESP_OK, ,
                        ZB_TAG, "Failed to start Zigbee BDB commissioning");
}

void esp_zb_app_signal_handler(esp_zb_app_signal_t *signal_struct)
{
    uint32_t *p_sg_p     = signal_struct->p_app_signal;
    esp_err_t err_status = signal_struct->esp_err_status;
    esp_zb_app_signal_type_t sig_type = static_cast<esp_zb_app_signal_type_t>(*p_sg_p);
    switch (sig_type) {
    case ESP_ZB_ZDO_SIGNAL_SKIP_STARTUP:
        ESP_LOGI(ZB_TAG, "Initialize Zigbee stack");
        esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_INITIALIZATION);
        break;
    case ESP_ZB_BDB_SIGNAL_DEVICE_FIRST_START:
    case ESP_ZB_BDB_SIGNAL_DEVICE_REBOOT:
        if (err_status == ESP_OK) {
            ESP_LOGI(ZB_TAG, "Device started up in%s factory-reset mode", esp_zb_bdb_is_factory_new() ? "" : " non");
            if (esp_zb_bdb_is_factory_new()) {
                ESP_LOGI(ZB_TAG, "Start network steering");
                esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_NETWORK_STEERING);
            } else {
                ESP_LOGI(ZB_TAG, "Device rebooted");
            }
        } else {
            ESP_LOGW(ZB_TAG, "%s failed with status: %s, retrying", esp_zb_zdo_signal_to_string(sig_type),
                     esp_err_to_name(err_status));
            esp_zb_scheduler_alarm((esp_zb_callback_t)bdb_start_top_level_commissioning_cb,
                                   ESP_ZB_BDB_MODE_INITIALIZATION, 1000);
        }
        break;
    case ESP_ZB_BDB_SIGNAL_STEERING:
        if (err_status == ESP_OK) {
            esp_zb_ieee_addr_t extended_pan_id;
            esp_zb_get_extended_pan_id(extended_pan_id);
            ESP_LOGI(ZB_TAG, "Joined network successfully (Extended PAN ID: %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x, PAN ID: 0x%04hx, Channel:%d, Short Address: 0x%04hx)",
                     extended_pan_id[7], extended_pan_id[6], extended_pan_id[5], extended_pan_id[4],
                     extended_pan_id[3], extended_pan_id[2], extended_pan_id[1], extended_pan_id[0],
                     esp_zb_get_pan_id(), esp_zb_get_current_channel(), esp_zb_get_short_address());
        } else {
            ESP_LOGI(ZB_TAG, "Network steering was not successful (status: %s)", esp_err_to_name(err_status));
            esp_zb_scheduler_alarm((esp_zb_callback_t)bdb_start_top_level_commissioning_cb, ESP_ZB_BDB_MODE_NETWORK_STEERING, 1000);
        }
        break;
    default:
        ESP_LOGI(ZB_TAG, "ZDO signal: %s (0x%x), status: %s", esp_zb_zdo_signal_to_string(sig_type), sig_type,
                 esp_err_to_name(err_status));
        break;
    }
}
