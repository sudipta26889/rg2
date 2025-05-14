cmake_minimum_required(VERSION 3.16)

# Set the path to your esp-matter installation
# !!! IMPORTANT: Make sure this path is correct for your system !!!
# set(ESP_MATTER_PATH "/Users/sudipta/esp/v5.2.3/esp-matter")

# Add the esp-matter path to the component search paths
# This allows idf_component.yml to find components like esp_matter, openthread, etc.
set(EXTRA_COMPONENT_DIRS ${ESP_MATTER_PATH})

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(rg2)