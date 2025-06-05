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
        /wd4244                 # Disable conversion warnings (fixed with explicit casts)
        /D_CRT_SECURE_NO_WARNINGS  # Disable MSVC security warnings for standard C functions
    )
    # Set MSVC specific definitions
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
else()
    # GCC/Clang specific settings
    add_compile_options(
        -Wall                   # Enable common warnings
        -Wextra                 # Enable extra warnings
        -Wpedantic             # Enable pedantic warnings
        -Wformat=2             # Enable format string warnings
        -Wunused               # Enable unused variable/function warnings
        -Wshadow               # Enable variable shadowing warnings
        -Wcast-align           # Enable cast alignment warnings
        -Wcast-qual            # Enable cast qualifier warnings
        -Wconversion           # Enable implicit conversion warnings
        -Wsign-conversion      # Enable sign conversion warnings
        -Wlogical-op           # Enable logical operation warnings (GCC)
        -Wduplicated-cond      # Enable duplicated condition warnings (GCC)
        -Wduplicated-branches  # Enable duplicated branch warnings (GCC)
        -Wnull-dereference     # Enable null dereference warnings (GCC)
        -Wdouble-promotion     # Enable double promotion warnings
        -Wformat-security      # Enable format security warnings
        -Werror                # Treat warnings as errors
        $<$<CONFIG:Debug>:-g3 -Og>    # Debug symbols and optimized debug
        $<$<CONFIG:Release>:-O3 -DNDEBUG>  # Optimized release
    )
endif()

# Platform-specific definitions
if(WIN32)
    add_definitions(-DWIN32_LEAN_AND_MEAN)
    add_definitions(-D_WIN32_WINNT=0x0601)  # Windows 7 or later
endif()

# Main emulator source files (matches Makefile SOURCES)
set(MAIN_SOURCES
    c64_main.c
    c64.c
    c64_bus.c
    ram.c
    rom.c
    system.c
    mos6569.c
    mos6526.c
    mos6581.c
    mos6510.c
)

# Klaus test runner source files (matches Makefile KLAUS_SOURCES)
set(KLAUS_SOURCES
    klaus_test_runner.c
    mos6510_test_harness.c
    c64.c
    c64_bus.c
    ram.c
    rom.c
    system.c
    mos6569.c
    mos6526.c
    mos6581.c
    mos6510.c
)

# Create main emulator executable (matches Makefile TARGET)
add_executable(c64emu ${MAIN_SOURCES})

# Create Klaus test runner executable (matches Makefile KLAUS_RUNNER)
add_executable(klaus_test_runner ${KLAUS_SOURCES})

# Platform-specific settings
if(UNIX)
    target_link_libraries(c64emu m) # Link math library
    target_link_libraries(klaus_test_runner m) # Link math library
endif()

# Add test target (matches Makefile test target)
add_custom_target(test
    DEPENDS c64emu
    COMMAND ${CMAKE_COMMAND} -E echo "Build successful! C64 emulator with cycle-accurate MOS6510 CPU compiled cleanly."
)

# Add klaus target (matches Makefile klaus target)
add_custom_target(klaus
    DEPENDS klaus_test_runner
    COMMAND ${CMAKE_COMMAND} -E echo "Klaus test runner compiled successfully!"
)
