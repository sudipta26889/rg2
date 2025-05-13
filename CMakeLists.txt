# /Users/sudipta/Workspace/Startup/RoomGuardian/rg2/CMakeLists.txt

cmake_minimum_required(VERSION 3.16) # Or higher depending on your IDF version

# Add C++20 standard globally for the entire project, required by esp-matter
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF) # Recommended for strict C++ standard adherence

# Include the ESP-IDF project framework functions
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# Define your project
project(rg2) # Ensure this matches your folder name (rg2)

# Add your main component subdirectory
# CMake will look for main/CMakeLists.txt and build it
add_subdirectory(main)

# If you had other custom components in a 'components' directory,
# you would add them similarly:
# add_subdirectory(components/your_custom_component)