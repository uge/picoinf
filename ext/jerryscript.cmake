#####################################################################
# Build Environment Detection
#
# I want to be able to detect building against windows vs <else>,
# where <else> is the RPi-Pico SDK.
#
#####################################################################

set(IS_SDK_BUILD 1)
if (WIN32)
    set(IS_SDK_BUILD 0)
endif()


#####################################################################
# Library Features
#####################################################################

if (IS_SDK_BUILD)
    set(JERRY_PORT OFF) # Should the default/native port be implemented? No, I will implement my own

    set(JERRY_CMDLINE OFF)  # a necessary requirement for JERRY_PORT to be OFF

    set(ENABLE_LTO OFF) # doesn't work on sdk
endif()


#####################################################################
# Engine Features
#####################################################################

# Logging and error diagnosis
set(JERRY_LINE_INFO ON)
set(JERRY_ERROR_MESSAGES ON)
set(JERRY_LOGGING ON)

# CPU time limiting
set(JERRY_VM_HALT ON)
set(JERRY_VM_THROW ON)

# Memory limiting
set(JERRY_GLOBAL_HEAP_SIZE 4)   # in KB
set(JERRY_STACK_LIMIT 8)   # in KB

# Memory limiting -- maybe.
# Need to experiment and see effects (initially went up).
set(JERRY_LCACHE OFF)
set(JERRY_PROPERTY_HASHMAP OFF)

# Investigating memory usage
set(JERRY_MEM_STATS ON)


#####################################################################
# Language Features
#####################################################################





#####################################################################
# Include
#####################################################################

# tons of compile warnings about printf formatting flags for some
# reason that don't show up under windows.
# suppress them, unfortunately.
set(EXTERNAL_COMPILE_FLAGS
    -Wno-format
    -Wno-error=format
    -Wno-format-nonliteral
    -Wno-format-security
)

add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/jerryscript)