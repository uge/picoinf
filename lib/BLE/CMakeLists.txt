target_include_directories(LibPico PUBLIC .)

target_sources(LibPico PRIVATE
    BleAdvertisementDataConverter.cpp
    BleAdvertisement.cpp
    BleAttDatabase.cpp
    BleGatt.cpp
    Ble.cpp
)