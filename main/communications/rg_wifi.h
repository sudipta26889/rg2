#include <http_app.h>
#include "nvs.h"
#include "nvs_flash.h"
#include "constants.h"
#include "services/rg_http_server.h"

// Define the Wi-Fi manager NVS namespace
static const char *wifi_manager_nvs_namespace = "espwifimgr";

static const std::string WTEMPTAG = std::string(DEVICE_NAME) + "-" + DEVICE_VERSION + "::Wifi";
static const char *WIFI_TAG = WTEMPTAG.c_str();

static char ip_address[16] = "0.0.0.0";

void cb_connection_ok(void *pvParameter) {
    ip_event_got_ip_t *param = (ip_event_got_ip_t *)pvParameter;

    // Transform IP to human-readable string
    esp_ip4addr_ntoa(&param->ip_info.ip, ip_address, sizeof(ip_address));

    ESP_LOGI(WIFI_TAG, "Connected to Wi-Fi. IP Address: %s", ip_address);
    shared_data.ip_address = ip_address;
    
    // Attach RG endpoint to HTTP server
    http_app_set_handler_hook(HTTP_GET, &my_get_handler);

}

// Reset Wi-Fi provisioning data
void reset_wifi_provisioning(void) {
    ESP_LOGI(WIFI_TAG, "Resetting Wi-Fi provisioning data...");

    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // NVS partition was truncated or a new version was found, erase and reinitialize
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    nvs_handle handle;
    esp_err_t err = nvs_open(wifi_manager_nvs_namespace, NVS_READWRITE, &handle);
    if (err == ESP_OK) {
        // Erase all keys in the "espwifimgr" namespace
        err = nvs_erase_all(handle);
        if (err == ESP_OK) {
            ESP_LOGI(WIFI_TAG, "Wi-Fi provisioning data erased successfully.");
        } else {
            ESP_LOGE(WIFI_TAG, "Failed to erase Wi-Fi provisioning data: %s", esp_err_to_name(err));
        }
        nvs_close(handle);
    } else {
        ESP_LOGE(WIFI_TAG, "Failed to open NVS: %s", esp_err_to_name(err));
    }

    // Commit changes to NVS
    nvs_commit(handle);
}