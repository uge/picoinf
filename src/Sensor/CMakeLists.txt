target_include_directories(PicoInf PUBLIC .)
target_sources(PicoInf PRIVATE
    ADCInternal.cpp
    BME280.cpp
    BMP280.cpp
    SI7021.cpp
)