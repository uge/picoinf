target_include_directories(PicoInf PUBLIC .)

file(GLOB lib_sources *.cpp)
if (lib_sources)
    target_sources(PicoInf PRIVATE ${lib_sources})
endif()