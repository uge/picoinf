include(cmake.includes.cmake)

# Pull in FreeRTOS
include(${FREERTOS_KERNEL_PATH}/portable/ThirdParty/GCC/RP2040/FreeRTOS_Kernel_import.cmake)

# Target TinyUSB with FreeRTOS
file(GLOB lib_sources ${TUSB_ROOT}/*.cpp)
target_sources(${PROJECT_NAME} PRIVATE ${lib_sources})

# Target LittleFS
file(GLOB lib_sources ${LITTLEFS_ROOT}/*.c)
target_sources(${PROJECT_NAME} PRIVATE ${lib_sources})

# Link libs
target_link_libraries(${PROJECT_NAME}
    cmsis_core

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






    pico_rand
    pico_stdlib

    tinyusb_device
    tinyusb_host
)

include(cmake.compiledefs.cmake)