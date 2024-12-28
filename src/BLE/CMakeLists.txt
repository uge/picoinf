target_include_directories(PicoInf PUBLIC .)

target_sources(PicoInf PRIVATE
    Ble.cpp
    BleAttDatabase.cpp
    BleGatt.cpp
    BleService.cpp
)