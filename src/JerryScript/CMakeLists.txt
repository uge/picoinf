target_include_directories(PicoInf PUBLIC .)

target_sources(PicoInf PRIVATE
    JerryScriptPORT.cpp
    JS_I2C.cpp
)

add_subdirectory(test)