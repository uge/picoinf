target_include_directories(PicoInf PUBLIC .)

target_sources(PicoInf PRIVATE
    JerryScriptPORT.cpp
)

add_subdirectory(test)
