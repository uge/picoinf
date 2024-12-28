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

# Library and code
add_library(PicoInf)
add_subdirectory(src)

# Set up test target executable
add_executable(PicoInfTest test.cpp)
target_link_libraries(PicoInfTest PicoInf)
pico_add_extra_outputs(PicoInfTest)


#####################################################################
# Library Includes
#####################################################################

# Locate FreeRTOS
# Also include config dir, which is where the FreeRTOSConfig.h is
set(FREERTOS_KERNEL_PATH "${CMAKE_CURRENT_LIST_DIR}/ext/FreeRTOS-Kernel")
target_include_directories(PicoInf PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/config
    ${FREERTOS_KERNEL_PATH}/include
)

# Locate TinyUSB
# Also include config dir, which is where the tusb_config.h is
set(TUSB_ROOT ${PICO_SDK_PATH}/lib/tinyusb)
target_include_directories(PicoInf PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/config
    ${TUSB_ROOT}/src
)

# Locate LittleFS
set(LITTLEFS_ROOT "${CMAKE_CURRENT_LIST_DIR}/ext/littlefs")
target_include_directories(PicoInf PUBLIC
    ${LITTLEFS_ROOT}
)

# Locate BTstack
set(BTSTACK_ROOT ${PICO_SDK_PATH}/lib/btstack)
target_include_directories(PicoInf PRIVATE
    ${BTSTACK_ROOT}/src
)

# Locate LwIP
# Also include config dir, which is where the lwipopts.h is
set(LWIP_ROOT ${PICO_SDK_PATH}/lib/lwip)
target_include_directories(PicoInf PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/config
)

# Include WsprEncoded
set(WSPR_ENCODED_ROOT "${CMAKE_CURRENT_LIST_DIR}/ext/WsprEncoded")
target_include_directories(PicoInf PUBLIC
    ${WSPR_ENCODED_ROOT}/src
)


#####################################################################
# Library Build
#####################################################################

# Pull in FreeRTOS
include(${FREERTOS_KERNEL_PATH}/portable/ThirdParty/GCC/RP2040/FreeRTOS_Kernel_import.cmake)

# Target TinyUSB with FreeRTOS
file(GLOB lib_sources ${TUSB_ROOT}/*.cpp)
target_sources(PicoInf PRIVATE ${lib_sources})

# Target LittleFS
file(GLOB lib_sources ${LITTLEFS_ROOT}/*.c)
target_sources(PicoInf PRIVATE ${lib_sources})

# Link libs
set(TARGET_LINK_LIBS_LIST)

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

list(APPEND TARGET_LINK_LIBS_LIST
    FreeRTOS-Kernel
    FreeRTOS-Kernel-Heap3
)

list(APPEND TARGET_LINK_LIBS_LIST
    tinyusb_device
    tinyusb_host
)

target_link_libraries(PicoInf ${TARGET_LINK_LIBS_LIST})


#####################################################################
# Compile Defs
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


