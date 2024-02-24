target_include_directories(LibPico PUBLIC .)

target_sources(LibPico PRIVATE
    Ble.cpp
    BleAttDatabase.cpp
    BleGatt.cpp
    BlePeripheral.cpp
    BleService.cpp
)