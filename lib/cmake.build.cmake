# Set up include paths
include(cmake.includes.cmake)

# Pull in FreeRTOS
include(${FRTOS_ROOT}/portable/ThirdParty/GCC/RP2040/FreeRTOS_Kernel_import.cmake)

# Target TinyUSB with FreeRTOS
file(GLOB lib_sources ${TUSB_ROOT}/*.cpp)
target_sources(${PROJECT_NAME} PRIVATE ${lib_sources})

# Target FreeRTOS
target_sources(${PROJECT_NAME} PRIVATE
    ${FRTOS_ROOT}/tasks.c
    ${FRTOS_ROOT}/queue.c
)

# Target LittleFS
file(GLOB lib_sources ${LITTLEFS_ROOT}/*.c)
target_sources(${PROJECT_NAME} PRIVATE ${lib_sources})

# Target BTstack
target_include_directories(${PROJECT_NAME} PRIVATE
    ${BTSTACK_ROOT}/src
)

target_link_libraries(${PROJECT_NAME}
    pico_stdlib
    pico_rand
    hardware_adc
    hardware_clocks
    hardware_i2c
    hardware_irq
    hardware_pll
    hardware_pwm
    hardware_rtc
    hardware_timer
    hardware_xosc
    FreeRTOS-Kernel
    FreeRTOS-Kernel-Heap3
    tinyusb_device
    tinyusb_host
    pico_cyw43_arch_none
    pico_btstack_cyw43
    pico_btstack_ble
    cmsis_core
)

include(${LIB_PICO}/cmake.compiledefs.cmake)