# root of library tree known to have compilable sources

# headers in this dir are directly includable (-I)
target_include_directories(LibPico PUBLIC .)

# compile everything in this directory
file(GLOB lib_sources *.cpp)
if (lib_sources)
  target_sources(LibPico PRIVATE ${lib_sources})
endif()

# traverse down build-wise
# add_subdirectory(RP2040)
# add_subdirectory(Evm)
add_subdirectory(Kernel)
# add_subdirectory(Log)
# add_subdirectory(PAL)
add_subdirectory(Peripheral)
# add_subdirectory(Service)
# add_subdirectory(Shell)
add_subdirectory(Utl)
