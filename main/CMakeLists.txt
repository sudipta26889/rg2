# main/CMakeLists.txt

# Register the main component
idf_component_register(
    SRCS
        "main.cpp"              # Your main application file
        "sensor_modules/rg_bme280.c" # Your BME280 implementation file
    INCLUDE_DIRS
        "."                     # Include the main directory
        "sensor_modules"        # Include the sensor_modules directory for header files
    REQUIRES
        "nvs_flash"             # Required for NVS flash operations (used by Matter)
        "log"                   # Required for logging
        "freertos"              # Required for FreeRTOS tasks
        "driver"                # Generic driver component (often transitively required)
        "espressif__bme280"     # If you are using Espressif's standard BME280 component alongside yours?
                                # **NOTE:** If 'sensor_modules/rg_bme280.c' is your *only* BME280 code,
                                # you might not need 'espressif__bme280' here. Verify your code's dependencies.
        "esp_wifi"              # Required for Wi-Fi
        "esp_event"             # Required for event handling
        "esp_netif"             # Required for network interface management
        "esp_matter"            # <-- **Add this line to depend on esp_matter**
        # Add any other components your code directly uses if not covered by the above or esp_matter's transitive deps
)