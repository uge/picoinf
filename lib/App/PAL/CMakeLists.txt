target_include_directories(LibPico PUBLIC .)

file(GLOB lib_sources *.cpp *.c)
if (lib_sources)
    target_sources(LibPico PRIVATE ${lib_sources})
endif()