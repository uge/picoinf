# root of module tree


cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)
set(PICO_SDK_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../pico-sdk")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

if (PICO_SDK_VERSION_STRING VERSION_LESS "1.4.0")
  message(FATAL_ERROR "Raspberry Pi Pico SDK version 1.4.0 (or later) required. Your version is ${PICO_SDK_VERSION_STRING}")
endif()



project(ProjLibPico)

set(PICO_BOARD pico_w)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

add_library(LibPico "")

message("LibPico here: ${CMAKE_CURRENT_SOURCE_DIR}")

set(LIB_PICO ${CMAKE_CURRENT_SOURCE_DIR})

add_subdirectory(${LIB_PICO}/App)
# add_subdirectory(Ble)
# add_subdirectory(ESB)
# add_subdirectory(GPS)
# add_subdirectory(Motor)
# add_subdirectory(MPSL)
# add_subdirectory(Sensor)
# add_subdirectory(Signal)
# add_subdirectory(WSPR)

target_link_libraries(LibPico
    pico_stdlib
)