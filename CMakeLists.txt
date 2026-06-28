cmake_minimum_required(VERSION 3.24)
project(DeepStreamDesktop LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Export compile_commands.json for clangd / IDE tooling.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

option(BUILD_DESKTOP "Build the Qt desktop application" ON)
option(BUILD_SERVICE "Build the DeepStream service" ON)
option(ENABLE_DEEPSTREAM "Enable NVIDIA DeepStream (requires the SDK)" OFF)

# Defines the BUILD_TESTING option (ON by default) and enables CTest.
include(CTest)

add_subdirectory(common)

if(BUILD_DESKTOP)
  add_subdirectory(desktop)
endif()

if(BUILD_SERVICE)
  add_subdirectory(service)
endif()

if(BUILD_TESTING)
  add_subdirectory(tests)
endif()
