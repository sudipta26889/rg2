# main/CMakeLists.txt
# ... (existing project() command or other lines)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

idf_component_register(SRCS "main.cpp" "sensor_modules/rg_bme280.c"
                       INCLUDE_DIRS "." "sensor_modules"
                       REQUIRES "nvs_flash" "log" "freertos" "driver"
                                "espressif__bme280" "esp_wifi" "esp_event" "esp_netif" "espressif__esp_matter"
)