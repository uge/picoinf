# leaf of library subtree

target_include_directories(LibPico PUBLIC .)

target_sources(LibPico PRIVATE Log.cpp)

# file(GLOB lib_sources *.cpp)
# if (lib_sources)
#   zephyr_library_sources(${lib_sources})
# endif()