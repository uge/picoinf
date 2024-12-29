# Set up common source and library
set (SOURCE_FILE_LIST)
list(APPEND SOURCE_FILE_LIST
    JS_I2C.cpp
)

set (LINK_LIBRARY_LIST)
list(APPEND LINK_LIBRARY_LIST
    jerry-core
    jerry-ext
)

# Handle SDK vs Windows builds
if (IS_SDK_BUILD)
    list(APPEND SOURCE_FILE_LIST
        JerryScriptPORT.cpp
    )
else()
    list(APPEND LINK_LIBRARY_LIST
        jerry-port
    )
endif()

# Configure
add_library(JerryScript STATIC ${SOURCE_FILE_LIST})
target_link_libraries(JerryScript PUBLIC ${LINK_LIBRARY_LIST})
target_include_directories(JerryScript PUBLIC ${CMAKE_CURRENT_LIST_DIR})

add_subdirectory(test)