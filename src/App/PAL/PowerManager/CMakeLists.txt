# leaf of library subtree

zephyr_include_directories(.)

file(GLOB lib_sources *.cpp *.c)
if (lib_sources)
  zephyr_library_sources(${lib_sources})
endif()