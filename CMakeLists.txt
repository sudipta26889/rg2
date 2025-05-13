# /Users/sudipta/Workspace/Startup/RoomGuardian/rg2/CMakeLists.txt

cmake_minimum_required(VERSION 3.16) # Or higher depending on your IDF version

# Add C++20 standard globally for the entire project, required by esp-matter
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF) # Recommended for strict C++ standard adherence

# ... (rest of your project's CMakeLists.txt)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(rg2) # Ensure this matches your folder name (rg2)

# ... (your components and source files)