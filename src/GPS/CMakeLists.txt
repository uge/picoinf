target_include_directories(PicoInf PUBLIC .)

target_sources(PicoInf PRIVATE
    NMEAStringMaker.cpp
    NMEAStringParser.cpp
)