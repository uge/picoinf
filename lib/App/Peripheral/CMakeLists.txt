# leaf of library subtree

target_include_directories(LibPico PUBLIC .)

target_sources(LibPico PRIVATE
    FilesystemLittleFS.cpp
    I2C.cpp
    Pin.cpp
    PWM.cpp
    UART.cpp
    USB_BOSDescriptor.cpp
    USB_CDC.cpp
    USB_ConfigurationDescriptor.cpp
    USB_DeviceDescriptor.cpp
    USB_StringDescriptor.cpp
    USB.cpp
    WDT.cpp
)

# file(GLOB lib_sources *.cpp)
# if (lib_sources)
#   zephyr_library_sources(${lib_sources})
# endif()