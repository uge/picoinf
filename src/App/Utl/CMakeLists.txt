target_include_directories(PicoInf PUBLIC .)

target_sources(PicoInf PRIVATE
    BitField.cpp
    Blinker.cpp
    DataStreamDistributor.cpp
    JSON.cpp
    JSONMsgRouter.cpp
    LineStreamDistributor.cpp
    Timeline.cpp
    Utl.cpp
    UtlBits.cpp
    UtlFormat.cpp
    UtlString.cpp
    VersionStr.cpp
)