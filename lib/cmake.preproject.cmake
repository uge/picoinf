
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 23)

# set(PICO_DEOPTIMIZED_DEBUG 1)
# add_definitions(-O0)
add_definitions(-Wall)


# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)
set(PICO_SDK_PATH "${CMAKE_CURRENT_LIST_DIR}/../pico-sdk")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(${PICO_SDK_PATH}/external/pico_sdk_import.cmake)

if (PICO_SDK_VERSION_STRING VERSION_LESS "1.4.0")
  message(FATAL_ERROR "Raspberry Pi Pico SDK version 1.4.0 (or later) required. Your version is ${PICO_SDK_VERSION_STRING}")
endif()



