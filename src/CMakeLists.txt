cmake_minimum_required(VERSION 3.13)

# Pull in initial cmake work
include(cmake.preproject.cmake)

# Primary configuration
set(PROJECT_NAME "LibPico")

# Delcare project and library to CMake
project(${PROJECT_NAME})
add_library(${PROJECT_NAME} "")

# Initialise the Raspberry Pi Pico SDK
# Notably unconditionally building for pico_w to ensure maximum
# binary size, no unexpected capacity changes to deal with later
set(PICO_BOARD pico_w)
pico_sdk_init()

# Build library modules
add_subdirectory(App)
add_subdirectory(Ble)
add_subdirectory(Compiler)
add_subdirectory(GPS)
# add_subdirectory(Motor)
add_subdirectory(Sensor)
add_subdirectory(Signal)
add_subdirectory(WSPR)

# Build external libs
include(cmake.build.cmake)