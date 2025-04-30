#include "esp_zigbee_core.h"

/* Zigbee configuration */
#define INSTALLCODE_POLICY_ENABLE       false   /* enable the install code policy for security */
#define ED_AGING_TIMEOUT                ESP_ZB_ED_AGING_TIMEOUT_64MIN
#define ED_KEEP_ALIVE                   3000    /* 3000 millisecond */
#define HA_ESP_SENSOR_ENDPOINT          10      /* esp temperature sensor device endpoint, used for temperature measurement */
#define ESP_ZB_PRIMARY_CHANNEL_MASK     ESP_ZB_TRANSCEIVER_ALL_CHANNELS_MASK    /* Zigbee primary channel mask use in the example */

/* Attribute values in ZCL string format
 * The string should be started with the length of its own.
 */
#define MANUFACTURER_NAME               "\x09""SudiptaDhara"
#define MODEL_IDENTIFIER                "\x07"CONFIG_IDF_TARGET

#define ESP_ZB_ZED_CONFIG()                                         \
    {                                                               \
        .esp_zb_role = ESP_ZB_DEVICE_TYPE_ROUTER,                       \
        .install_code_policy = INSTALLCODE_POLICY_ENABLE,           \
        .nwk_cfg = {                                                \
            .zczr_cfg = {                                            \
                .max_children = 10,                        \
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