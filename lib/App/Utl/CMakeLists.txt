# leaf of library subtree

target_include_directories(LibPico PUBLIC .)

target_sources(LibPico PRIVATE Test.cpp)

# file(GLOB lib_sources *.cpp)
# if (lib_sources)
#   target_sources(LibPico PRIVATE ${lib_sources})
# endif()