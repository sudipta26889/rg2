# This is the CMakeLists.txt for the 'main' component.
# It should ONLY define the component's properties using idf_component_register.
# Project-level settings are in the root CMakeLists.txt.

idf_component_register(
    SRCS
        "main.cpp"              # Your main application file
        "sensor_modules/rg_bme280.c" # Your BME280 implementation file
    INCLUDE_DIRS
        "."                     # Include the main component's directory
        "sensor_modules"        # Include the sensor_modules directory for header files
    REQUIRES
        "nvs_flash"             # Required for NVS flash operations (used by Matter)
        "log"                   # Required for logging
        "freertos"              # Required for FreeRTOS tasks
        "driver"                # Generic driver component (often transitively required)
        # "espressif__bme280"     # <-- Only include if you are using the standard component in *addition* to your own code
                                # If 'sensor_modules/rg_bme280.c' is your *only* BME280 code, remove this line.
        "esp_wifi"              # Required for Wi-Fi
        "esp_event"             # Required for event handling
        "esp_netif"             # Required for network interface management
        "esp_matter"            # <-- Required for Matter functionality
        "i2c_bus"               # <-- Required for your BME280 (likely uses i2c_bus)
        "espressif__bme280"     # <-- Required if you are using the managed component functions
        # Add any other managed components your code directly uses (e.g., specific drivers, utilities)
)