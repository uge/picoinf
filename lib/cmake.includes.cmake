# The purpose of this file is to set up:
# - all the environment variables required to reach given libs
# - all the include directives which make the build work
# This does not attempt to compile or include code for build
#
# Expects variables to be set:
# - PROJECT_NAME the target you're building, whether executable or library

set(PICO_SDK_PATH "${CMAKE_CURRENT_LIST_DIR}/../pico-sdk")

# Locate FreeRTOS
# Also include this dir, which is where the FreeRTOSConfig.h is
set(FRTOS_ROOT "${CMAKE_CURRENT_LIST_DIR}/../FreeRTOS-Kernel")
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
    ${FRTOS_ROOT}/include
    ${FRTOS}/portable/GCC/ARM_CM0
)

# Locate TinyUSB
# Also include this dir, which is where the tusb_config.h is
set(TUSB_ROOT ${PICO_SDK_PATH}/lib/tinyusb)
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
    ${TUSB_ROOT}/src
)

# Locate LittleFS
set(LITTLEFS_ROOT "${CMAKE_CURRENT_LIST_DIR}/../littlefs")
target_include_directories(${PROJECT_NAME} PRIVATE
    ${LITTLEFS_ROOT}
)

# Locate BTstack
set(BTSTACK_ROOT ${PICO_SDK_PATH}/lib/btstack)
target_include_directories(${PROJECT_NAME} PRIVATE
    ${BTSTACK_ROOT}/src
)



