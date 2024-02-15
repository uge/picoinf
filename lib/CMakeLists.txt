# root of module tree


cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 23)

# set(PICO_DEOPTIMIZED_DEBUG 1)
# add_definitions(-O0)




# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)
set(PICO_SDK_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../pico-sdk")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(${PICO_SDK_PATH}/external/pico_sdk_import.cmake)

if (PICO_SDK_VERSION_STRING VERSION_LESS "1.4.0")
  message(FATAL_ERROR "Raspberry Pi Pico SDK version 1.4.0 (or later) required. Your version is ${PICO_SDK_VERSION_STRING}")
endif()

# Pull in FreeRTOS
set(FRTOS_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../FreeRTOS-Kernel")
include(${FRTOS_PATH}/portable/ThirdParty/GCC/RP2040/FreeRTOS_Kernel_import.cmake)


project(ProjLibPico)

set(PICO_BOARD pico_w)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

set(LIB_PICO ${CMAKE_CURRENT_SOURCE_DIR})
add_library(LibPico "")

# Target TinyUSB with FreeRTOS
set(TUSB_DIR ${PICO_SDK_PATH}/lib/tinyusb)
file(GLOB lib_sources ${TUSB_DIR}/*.cpp)
target_sources(LibPico PRIVATE ${lib_sources})
target_include_directories(LibPico PRIVATE
    ${TUSB_CONFIG_H_DIR}
    ${TUSB_DIR}/src
)

# Target FreeRTOS
target_sources(LibPico PRIVATE
    ${FRTOS_PATH}/tasks.c
    ${FRTOS_PATH}/queue.c
)
target_include_directories(LibPico PRIVATE
    ${LIB_PICO}
    ${FRTOS_PATH}/include
    ${FRTOS}/portable/GCC/ARM_CM0
)

# Target LittleFS
set(LITTLEFS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../littlefs")
file(GLOB lib_sources ${LITTLEFS_DIR}/*.c)
target_sources(LibPico PRIVATE ${lib_sources})
target_include_directories(LibPico PRIVATE
    ${LITTLEFS_DIR}
)

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
    pico_rand
    FreeRTOS-Kernel
    FreeRTOS-Kernel-Heap3
    tinyusb_device
    tinyusb_host
    cmsis_core
)