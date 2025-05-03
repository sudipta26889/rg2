#include "esp_openthread.h"
#include "esp_openthread_netif.h"

void ot_task_worker(void *ctx) {
    esp_openthread_launch_mainloop();
}

void openthread_init() {
    esp_openthread_platform_config_t config = {
        .radio_config = ESP_OPENTHREAD_DEFAULT_RADIO_CONFIG(),
        .host_config = ESP_OPENTHREAD_DEFAULT_HOST_CONFIG(),
    };
    ESP_ERROR_CHECK(esp_openthread_init(&config));
    xTaskCreate(ot_task_worker, "ot_main", 4096, NULL, 5, NULL);
}
