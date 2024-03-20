# Delcare project and executable to CMake
project(${PROJECT_NAME} C CXX ASM)
add_executable(${PROJECT_NAME} main.cpp)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()
pico_set_program_name(${PROJECT_NAME} "${PROJECT_NAME}")
pico_set_program_version(${PROJECT_NAME} "0.1")
pico_add_extra_outputs(${PROJECT_NAME})

# Pull in LibPico
include(${LIB_PICO}/cmake.includes.cmake)
include(${LIB_PICO}/cmake.compiledefs.cmake)
add_subdirectory("${LIB_PICO}" "./LIB_PICO")
target_link_libraries(${PROJECT_NAME} LibPico)