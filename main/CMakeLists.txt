idf_component_register(
    SRCS 
        "main.cpp" 
        "communications/rg_zigbee.cpp"
        "communications/matter/rg_matter.cpp"
    INCLUDE_DIRS 
        "." 
        "sensor_modules"
        "communications"
        "services/shared_data"
        "communications/matter"
    REQUIRES 
        esp_matter  # Only this line needed
)