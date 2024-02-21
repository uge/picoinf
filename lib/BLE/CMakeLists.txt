target_include_directories(LibPico PUBLIC .)

target_sources(LibPico PRIVATE
    BleAdvertisementDataConverter.cpp
    BleAdvertisement.cpp
    BleAttDatabase.cpp
    BleGattNew.cpp
    BleNew.cpp
)