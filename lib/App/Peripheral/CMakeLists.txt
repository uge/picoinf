# leaf of library subtree

target_include_directories(LibPico PUBLIC .)

target_sources(LibPico PRIVATE
    Pin.cpp
    UART.cpp
    WDT.cpp
    USB.cpp
    USB_BOSDescriptor.cpp
    USB_CDC.cpp
    USB_ConfigurationDescriptor.cpp
    USB_DeviceDescriptor.cpp
    USB_StringDescriptor.cpp
)

# file(GLOB lib_sources *.cpp)
# if (lib_sources)
#   zephyr_library_sources(${lib_sources})
# endif()