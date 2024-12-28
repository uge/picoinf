target_include_directories(PicoInf PUBLIC .)

target_sources(PicoInf PRIVATE
    JSONMsgRouter.cpp
    Timeline.cpp
    Utl.cpp
    VersionStr.cpp
)