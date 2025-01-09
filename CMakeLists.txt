cmake_minimum_required(VERSION 3.15...3.31)

#####################################################################
# Library Options
#####################################################################

if (NOT DEFINED APP_BUILD_VERSION)
    set(APP_BUILD_VERSION "1970-01-01 00:00:00")
endif()

if (NOT DEFINED PICO_BOARD)
    set(PICO_BOARD "pico")
endif()


set(PICO_INF_ENABLE_WIRELESS 0)
if (PICO_BOARD STREQUAL "pico_w")
    set(PICO_INF_ENABLE_WIRELESS 1)
endif()


#####################################################################
# Compile Settings
#####################################################################

# Set up output of compile commands
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Set up language configuration
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)


#####################################################################
# SDK Import
#####################################################################

# Set up pico-sdk
set(PICO_SDK_PATH "${CMAKE_CURRENT_LIST_DIR}/ext/pico-sdk")
include(${PICO_SDK_PATH}/external/pico_sdk_import.cmake)


#####################################################################
# Project Declaration
#####################################################################

project(PicoInf LANGUAGES C CXX ASM)


#####################################################################
# SDK Init
#####################################################################

pico_sdk_init()


#####################################################################
# PicoInf Library
#####################################################################

# Declare Library
add_library(PicoInf)

# Build link libs list
set(TARGET_LINK_LIBS_LIST)


#####################################################################
# Shims
#####################################################################

add_subdirectory(shim)


#####################################################################
# Arduino Library Integration
#####################################################################

set(ADAFRUIT_BME280_LIBRARY_PATH "${CMAKE_CURRENT_LIST_DIR}/ext/Adafruit_BME280_Library")
target_include_directories(PicoInf PUBLIC ${ADAFRUIT_BME280_LIBRARY_PATH})
target_sources(PicoInf PRIVATE ${ADAFRUIT_BME280_LIBRARY_PATH}/Adafruit_BME280.cpp)
set_source_files_properties(
    ${ADAFRUIT_BME280_LIBRARY_PATH}/Adafruit_BME280.cpp
    PROPERTIES
        COMPILE_DEFINITIONS "ARDUINO=1000"
)

set(ADAFRUIT_BMP280_LIBRARY_PATH "${CMAKE_CURRENT_LIST_DIR}/ext/Adafruit_BMP280_Library")
target_include_directories(PicoInf PUBLIC ${ADAFRUIT_BMP280_LIBRARY_PATH})
target_sources(PicoInf PRIVATE ${ADAFRUIT_BMP280_LIBRARY_PATH}/Adafruit_BMP280.cpp)
set_source_files_properties(
    ${ADAFRUIT_BMP280_LIBRARY_PATH}/Adafruit_BMP280.cpp
    PROPERTIES
        COMPILE_DEFINITIONS "ARDUINO=1000"
)

set(ADAFRUIT_BUSIO_LIBRARY_PATH "${CMAKE_CURRENT_LIST_DIR}/ext/Adafruit_BusIO")
target_include_directories(PicoInf PUBLIC ${ADAFRUIT_BUSIO_LIBRARY_PATH})
target_sources(PicoInf PRIVATE
    ${ADAFRUIT_BUSIO_LIBRARY_PATH}/Adafruit_BusIO_Register.cpp
    ${ADAFRUIT_BUSIO_LIBRARY_PATH}/Adafruit_I2CDevice.cpp
    ${ADAFRUIT_BUSIO_LIBRARY_PATH}/Adafruit_SPIDevice.cpp
)
set_source_files_properties(
    ${ADAFRUIT_BUSIO_LIBRARY_PATH}/Adafruit_BusIO_Register.cpp
    ${ADAFRUIT_BUSIO_LIBRARY_PATH}/Adafruit_I2CDevice.cpp
    ${ADAFRUIT_BUSIO_LIBRARY_PATH}/Adafruit_SPIDevice.cpp
    PROPERTIES
        # COMPILE_DEFINITIONS "ARDUINO=1000;DEBUG_SERIAL=Serial"
        COMPILE_DEFINITIONS "ARDUINO=1000"
)


set(ADAFRUIT_MMC56X3_LIBRARY_PATH "${CMAKE_CURRENT_LIST_DIR}/ext/Adafruit_MMC56x3")
target_include_directories(PicoInf PUBLIC ${ADAFRUIT_MMC56X3_LIBRARY_PATH})
target_sources(PicoInf PRIVATE ${ADAFRUIT_MMC56X3_LIBRARY_PATH}/Adafruit_MMC56x3.cpp)
set_source_files_properties(
    ${ADAFRUIT_MMC56X3_LIBRARY_PATH}/Adafruit_MMC56x3.cpp
    PROPERTIES
        COMPILE_DEFINITIONS "ARDUINO=1000"
)

set(ADAFRUIT_SENSOR_LIBRARY_PATH "${CMAKE_CURRENT_LIST_DIR}/ext/Adafruit_Sensor")
target_include_directories(PicoInf PUBLIC ${ADAFRUIT_SENSOR_LIBRARY_PATH})
target_sources(PicoInf PRIVATE ${ADAFRUIT_SENSOR_LIBRARY_PATH}/Adafruit_Sensor.cpp)
set_source_files_properties(
    ${ADAFRUIT_SENSOR_LIBRARY_PATH}/Adafruit_Sensor.cpp
    PROPERTIES
        COMPILE_DEFINITIONS "ARDUINO=1000"
)

set(ADAFRUIT_SI7021_LIBRARY_PATH "${CMAKE_CURRENT_LIST_DIR}/ext/Adafruit_Si7021")
target_include_directories(PicoInf PUBLIC ${ADAFRUIT_SI7021_LIBRARY_PATH})
target_sources(PicoInf PRIVATE ${ADAFRUIT_SI7021_LIBRARY_PATH}/Adafruit_Si7021.cpp)
set_source_files_properties(
    ${ADAFRUIT_SI7021_LIBRARY_PATH}/Adafruit_Si7021.cpp
    PROPERTIES
        COMPILE_DEFINITIONS "ARDUINO=1000"
)

set(BH1750_LIBRARY_PATH "${CMAKE_CURRENT_LIST_DIR}/ext/BH1750FVI_RT/")
target_include_directories(PicoInf PUBLIC ${BH1750_LIBRARY_PATH})
target_sources(PicoInf PRIVATE ${BH1750_LIBRARY_PATH}/BH1750FVI.cpp)
set_source_files_properties(
    ${BH1750_LIBRARY_PATH}/BH1750FVI.cpp
    PROPERTIES
        COMPILE_DEFINITIONS "ARDUINO=1000"
)


#####################################################################
# FreeRTOS Integration
#####################################################################

# Locate FreeRTOS
# Also include config dir, which is where the FreeRTOSConfig.h is
set(FREERTOS_KERNEL_PATH "${CMAKE_CURRENT_LIST_DIR}/ext/FreeRTOS-Kernel")
target_include_directories(PicoInf PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/config
    ${FREERTOS_KERNEL_PATH}/include
)

# Pull in FreeRTOS
include(${FREERTOS_KERNEL_PATH}/portable/ThirdParty/GCC/RP2040/FreeRTOS_Kernel_import.cmake)

list(APPEND TARGET_LINK_LIBS_LIST
    FreeRTOS-Kernel
    FreeRTOS-Kernel-Heap3
)


#####################################################################
# TinyUSB Integration
#####################################################################

# Locate TinyUSB
# Also include config dir, which is where the tusb_config.h is
set(TUSB_ROOT ${PICO_SDK_PATH}/lib/tinyusb)
target_include_directories(PicoInf PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/config
    ${TUSB_ROOT}/src
)

# Target TinyUSB with FreeRTOS
file(GLOB lib_sources ${TUSB_ROOT}/*.cpp)
target_sources(PicoInf PRIVATE ${lib_sources})

list(APPEND TARGET_LINK_LIBS_LIST
    tinyusb_device
    tinyusb_host
)


#####################################################################
# LittleFS Integration
#####################################################################

# Locate LittleFS
set(LITTLEFS_ROOT "${CMAKE_CURRENT_LIST_DIR}/ext/littlefs")
target_include_directories(PicoInf PUBLIC
    ${LITTLEFS_ROOT}
)

# Target LittleFS
file(GLOB lib_sources ${LITTLEFS_ROOT}/*.c)
target_sources(PicoInf PRIVATE ${lib_sources})


#####################################################################
# BTstack Integration
#####################################################################

# Locate BTstack
set(BTSTACK_ROOT ${PICO_SDK_PATH}/lib/btstack)
target_include_directories(PicoInf PRIVATE
    ${BTSTACK_ROOT}/src
)

if (PICO_INF_ENABLE_WIRELESS)
    list(APPEND TARGET_LINK_LIBS_LIST
        # original
        # ------------------
        pico_cyw43_arch_none
        pico_btstack_cyw43
        pico_btstack_ble

        # new
        # ------------------


        # pico_cyw43_driver
        # pico_cyw43_arch_lwip_sys_freertos

        # pico_lwip_arch
        # pico_lwip_freertos
    )
endif()


#####################################################################
# LwIP Integration
#####################################################################

# Locate LwIP
# Also include config dir, which is where the lwipopts.h is
set(LWIP_ROOT ${PICO_SDK_PATH}/lib/lwip)
target_include_directories(PicoInf PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/config
)


#####################################################################
# WsprEncoded Integration
#####################################################################

# Include WsprEncoded
set(WSPR_ENCODED_ROOT "${CMAKE_CURRENT_LIST_DIR}/ext/WsprEncoded")
add_subdirectory(${WSPR_ENCODED_ROOT})

list(APPEND TARGET_LINK_LIBS_LIST
    WsprEncoded
)


#####################################################################
# JerryScript Integration
#####################################################################

#############################
# Library Features
#############################

set(JERRY_PORT OFF) # Should the default/native port be implemented? No, I will implement my own
set(JERRY_CMDLINE OFF)  # a necessary requirement for JERRY_PORT to be OFF
set(ENABLE_LTO OFF) # doesn't work on sdk


#############################
# Engine Features
#############################

# Logging and error diagnosis
set(JERRY_LINE_INFO ON)
set(JERRY_ERROR_MESSAGES ON)
set(JERRY_LOGGING ON)

# CPU time limiting
set(JERRY_VM_HALT ON)
set(JERRY_VM_THROW ON)

# Memory limiting
set(JERRY_GLOBAL_HEAP_SIZE 16)   # in KB
set(JERRY_STACK_LIMIT 8)        # in KB

# Investigating memory usage
# (actually seeing a script execute 14% slower when OFF, no idea why)
# (leave this ON)
set(JERRY_MEM_STATS ON)


#############################
# Language Features
#############################


#############################
# Add
#############################

# tons of compile warnings about printf formatting flags for some
# reason that don't show up under windows.
# suppress them, unfortunately.
set(EXTERNAL_COMPILE_FLAGS
    -Wno-format
    -Wno-error=format
    -Wno-format-nonliteral
    -Wno-format-security
)

set(JERRY_SCRIPT_ROOT "${CMAKE_CURRENT_LIST_DIR}/ext/jerryscript")
add_subdirectory(${JERRY_SCRIPT_ROOT})


list(APPEND TARGET_LINK_LIBS_LIST
    jerry-core
    jerry-ext
)


#####################################################################
# Pico SDK Integration
#####################################################################

list(APPEND TARGET_LINK_LIBS_LIST
    cmsis_core
)

list(APPEND TARGET_LINK_LIBS_LIST
    hardware_adc
    hardware_clocks
    hardware_i2c
    hardware_irq
    hardware_pll
    hardware_pwm
    hardware_rtc
    hardware_timer
    hardware_xosc
)

list(APPEND TARGET_LINK_LIBS_LIST
    pico_rand
    pico_stdlib
)


#####################################################################
# PicoInf Linking
#####################################################################

target_link_libraries(PicoInf ${TARGET_LINK_LIBS_LIST})


#####################################################################
# PicoInf Compile Defs
#####################################################################

target_compile_definitions(PicoInf PRIVATE
    PICO_INF_ENABLE_WIRELESS=${PICO_INF_ENABLE_WIRELESS}

    FREERTOS_KERNEL_PATH="${FREERTOS_KERNEL_PATH}"
    PICO_FLASH_ASSUME_CORE1_SAFE=1
    APP_BUILD_VERSION="${APP_BUILD_VERSION}"
    
    # ensure all asserts are enabled
    PARAM_ASSERTIONS_ENABLE_ALL=1

    # stack, enable full 4k for the core0 stack from the default 2k
    # and enable guards for overflow
    PICO_STACK_SIZE=0x1000
    PICO_USE_STACK_GUARDS=1

    # heap enable synchronization
    PICO_MALLOC_PANIC=1
    PICO_USE_MALLOC_MUTEX=1
    # PICO_DEBUG_MALLOC=1
)

# set(PICO_DEOPTIMIZED_DEBUG 1)
# add_definitions(-O0)
add_definitions(-Wall)
add_definitions(-Wno-psabi)
add_definitions(-fstack-usage)


#####################################################################
# PicoInf Library Code
#####################################################################

# Code
add_subdirectory(src)

# Set up test target executable
add_executable(PicoInfTest test.cpp)
target_link_libraries(PicoInfTest PicoInf)
pico_add_extra_outputs(PicoInfTest)



