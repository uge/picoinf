target_compile_definitions(${PROJECT_NAME} PRIVATE
    FREERTOS_KERNEL_PATH="${FREERTOS_KERNEL_PATH}"
    PICO_FLASH_ASSUME_CORE1_SAFE=1
    APP_BUILD_VERSION="${APP_BUILD_VERSION}"
    PICO_BOARD_ACTUAL="${PICO_BOARD_ACTUAL}"
    
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
