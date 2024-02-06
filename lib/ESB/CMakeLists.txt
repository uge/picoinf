# root of library tree known to NOT have compilable sources

# headers in this dir are directly includable (-I)
zephyr_include_directories(.)

# declare this is a library
# zephyr_library()

# compile everything in this directory
# file(GLOB lib_sources *.cpp)
# if (lib_sources)
#   zephyr_library_sources(${lib_sources})
# endif()

# traverse down build-wise
