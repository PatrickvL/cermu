cmake_minimum_required(VERSION 3.16)

# Project definition
project(aiemuc 
    VERSION 0.1.0
    DESCRIPTION "C implementation of MOS 6510 CPU emulator"
    LANGUAGES C
)

# Set C standard
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)  # Disable compiler-specific extensions

# Enable extra build types
set(CMAKE_CONFIGURATION_TYPES "Debug;Release;RelWithDebInfo;MinSizeRel" CACHE STRING "" FORCE)

# Set default build type if not specified
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Choose the type of build" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "RelWithDebInfo" "MinSizeRel")
endif()

# Compiler-specific settings and warning flags
if(MSVC)
    # MSVC specific settings
    add_compile_options(
        /W4                     # Enable all warnings
        /WX                     # Treat warnings as errors
        /wd4324                 # Disable "structure was padded" warning
        /wd4459                 # Disable declaration hiding global declaration warnings
        /wd4255                 # Disable 'function' : no function prototype given
        /analyze               # Enable static code analysis
    )
else()
    # GCC/Clang settings
    add_compile_options(
        -Wall                   # Enable all common warnings
        -Wextra                 # Enable extra warnings
        -Werror                 # Treat warnings as errors
        -pedantic               # Strict ISO C compliance
        -Wcast-align            # Warn about cast alignment issues
        -Wcast-qual             # Warn about cast qualifiers
        -Wmissing-prototypes    # Warn about missing function prototypes
        -Wstrict-prototypes     # Warn about non-prototype function declarations
        -Wno-unused-parameter   # Allow unused parameters (common in callbacks)
    )
endif()

# Platform specific definitions
if(WIN32)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
    add_definitions(-D_WIN32_WINNT=0x0601)  # Target Windows 7 or later
    add_definitions(-DNOMINMAX)             # Don't define min/max macros
endif()

# Debug vs Release specific settings
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_definitions(-DDEBUG)
    if(MSVC)
        add_compile_options(/Od /Zi)        # No optimization, generate debug info
    else()
        add_compile_options(-O0 -g)         # No optimization, generate debug info
    endif()
else()
    add_definitions(-DNDEBUG)
    if(MSVC)
        add_compile_options(/O2)            # Optimize for speed
    else()
        add_compile_options(-O2)            # Optimize for speed
    endif()
endif()

# Source files
set(EMULATOR_SOURCES
    mos6510.c
    mos6510_test_harness.c
    c64.c
    c64_bus.c
    system.c
    ram.c
    rom.c
    mos6526.c
    mos6581.c
    mos6569.c
)

set(EMULATOR_HEADERS
    mos6510.h
    mos6510_test_harness.h
    mos6510_cycles.h
    c64.h
    c64_bus.h
    system.h
    device.h
    ram.h
    rom.h
    mos6526.h
    mos6581.h
    mos6569.h
    aiemuc.h
)

# Main emulator executable
add_executable(c64emu c64_main.c ${EMULATOR_SOURCES} ${EMULATOR_HEADERS})

# Test harness for Klaus 6502 functional tests
add_executable(klaus_test_runner klaus_test_runner.c ${EMULATOR_SOURCES} ${EMULATOR_HEADERS})

# Verification program for cycle calculation
add_executable(verify_cycles verify_cycles.c)
target_include_directories(verify_cycles PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# Include directories
target_include_directories(c64emu PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(klaus_test_runner PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# Platform-specific libraries
if(WIN32)
    # No additional libraries needed for basic functionality
else()
    # Link against math library on Unix-like systems
    target_link_libraries(c64emu m)
    target_link_libraries(klaus_test_runner m)
endif()

# Set output directories
set_target_properties(c64emu klaus_test_runner verify_cycles
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/$<CONFIG>
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/$<CONFIG>
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/$<CONFIG>
)

# Installation targets (optional)
install(TARGETS c64emu klaus_test_runner verify_cycles
    RUNTIME DESTINATION bin
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)

# CPack configuration for packaging (optional)
include(CPack)
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "MOS 6510 CPU Emulator")
set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})
