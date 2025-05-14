# This is the CMakeLists.txt for the 'main' component.
# It should ONLY define the component's properties using idf_component_register.
# Project-level settings are in the root CMakeLists.txt.

idf_component_register(
    SRCS
        "main.cpp"              # Your main application file
        "sensor_modules/rg_bme280.c" # Your BME280 implementation file
    INCLUDE_DIRS
        "."                     # Include the main component's directory
    REQUIRES
        "nvs_flash"             # Required for NVS flash operations (used by Matter)
        "log"                   # Required for logging
        "freertos"              # Required for FreeRTOS tasks
        "driver"                # Generic driver component (often transitively required)
        "esp_wifi"              # Required for Wi-Fi
        "esp_event"             # Required for event handling
        "esp_netif"             # Required for network interface management
        "i2c_bus"               # <-- Required for your BME280 (likely uses i2c_bus)
        "espressif__bme280"     # <-- Required if you are using the managed component functions
)