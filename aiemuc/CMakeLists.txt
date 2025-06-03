cmake_minimum_required(VERSION 3.16)

# Project definition
project(aiemuc 
    VERSION 0.1.0
    DESCRIPTION "C implementation of MOS 6510 CPU emulator"
    LANGUAGES C
)

# Set C standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Add compiler flags for debugging and warnings
# Always enable warning flags
if(MSVC)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /W4")
else()
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra")
endif()
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -O0")
else()
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3 -DNDEBUG")
endif()

# Collect all source files
set(SOURCES
    c64_main.c
    c64.c
    c64_bus.c
    ram.c
    rom.c
    mos6526.c
    mos6581.c
    mos6569.c
    mos6510.c
)

# Create executable with all source files
add_executable(aiemuc ${SOURCES})

# Set output directory
set_target_properties(aiemuc PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# Platform-specific settings
if(UNIX)
    target_link_libraries(aiemuc m) # Link math library
endif()
