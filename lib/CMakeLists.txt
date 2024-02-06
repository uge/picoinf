# root of module tree

cmake_minimum_required(VERSION 3.13)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)
set(PICO_SDK_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../pico-sdk")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

if (PICO_SDK_VERSION_STRING VERSION_LESS "1.4.0")
  message(FATAL_ERROR "Raspberry Pi Pico SDK version 1.4.0 (or later) required. Your version is ${PICO_SDK_VERSION_STRING}")
endif()



project(ProjLibPico)

set(PICO_BOARD pico_w)


# add_library(LibPico "")
# target_sources(LibPico EXCLUDE_FROM_ALL ${CMAKE_CURRENT_SOURCE_DIR}/App/Utl/Test.cpp)
# add_library(LibPico ${CMAKE_CURRENT_SOURCE_DIR}/App/Utl/Test.cpp)

add_library(LibPico "")

add_subdirectory(App)
# add_subdirectory(Ble)
# add_subdirectory(ESB)
# add_subdirectory(GPS)
# add_subdirectory(Motor)
# add_subdirectory(MPSL)
# add_subdirectory(Sensor)
# add_subdirectory(Signal)
# add_subdirectory(WSPR)




