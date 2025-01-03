target_include_directories(PicoInf PUBLIC .)
target_sources(PicoInf PRIVATE
    Arduino.cpp
    SPI.cpp
    Wire.cpp
)