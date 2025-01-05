target_include_directories(PicoInf PUBLIC .)

target_sources(PicoInf PRIVATE
    BitField.cpp
    Blinker.cpp
    DataStreamDistributor.cpp
    JSON.cpp
    JSONMsgRouter.cpp
    Timeline.cpp
    Utl.cpp
    VersionStr.cpp
)