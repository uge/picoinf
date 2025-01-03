target_include_directories(PicoInf PUBLIC .)

target_sources(PicoInf PRIVATE
    Clock.cpp
    FilesystemLittleFS.cpp
    FilesystemLittleFSFile.cpp
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