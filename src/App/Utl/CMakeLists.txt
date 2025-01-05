target_include_directories(PicoInf PUBLIC .)

target_sources(PicoInf PRIVATE
    BitField.cpp
    JSONMsgRouter.cpp
    Timeline.cpp
    Utl.cpp
    VersionStr.cpp
)