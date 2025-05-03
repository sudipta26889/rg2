idf_component_register(
    SRCS "main.cpp" "communications/rg_zigbee.cpp" "services/shared_data/shared_data.cpp"
    INCLUDE_DIRS "."
)
target_compile_definitions(${COMPONENT_LIB} PRIVATE CHIP_SYSTEM_CONFIG_USE_LWIP=1)
