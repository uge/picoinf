# leaf of library subtree

zephyr_include_directories(.)

file(GLOB lib_sources *.cpp *.c)
zephyr_library_sources(${lib_sources})

# pull in select bits of the rpi sdk.
# zephyr doesn't actually pull everything in it seems...
# learned from zephyr/modules/hal_rpi_pico/CMakeLists.txt
set(rp2_common_dir ${ZEPHYR_HAL_RPI_PICO_MODULE_DIR}/src/rp2_common)
set(rp2040_dir ${ZEPHYR_HAL_RPI_PICO_MODULE_DIR}/src/rp2040)
set(common_dir ${ZEPHYR_HAL_RPI_PICO_MODULE_DIR}/src/common)
set(host_dir ${ZEPHYR_HAL_RPI_PICO_MODULE_DIR}/src/host)

zephyr_include_directories(${common_dir}/pico_time/include)
zephyr_include_directories(${common_dir}/pico_stdlib/include)
zephyr_include_directories(${common_dir}/pico_sync/include)
zephyr_include_directories(${host_dir}/pico_stdio/include)
zephyr_include_directories(${rp2_common_dir}/pico_bootrom/include)


zephyr_include_directories(${rp2_common_dir}/hardware_claim/include)
zephyr_library_sources(    ${rp2_common_dir}/hardware_claim/claim.c)

zephyr_include_directories(${rp2_common_dir}/hardware_timer/include)
zephyr_library_sources(    ${rp2_common_dir}/hardware_timer/timer.c)

zephyr_include_directories(${rp2_common_dir}/hardware_i2c/include)
zephyr_library_sources(    ${rp2_common_dir}/hardware_i2c/i2c.c)

zephyr_include_directories(${rp2_common_dir}/hardware_vreg/include)
zephyr_library_sources(    ${rp2_common_dir}/hardware_vreg/vreg.c)
    
