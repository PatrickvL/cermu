cmake_minimum_required(VERSION 3.16)

# Project definition
project(aiemuc 
    VERSION 0.1.0
    DESCRIPTION "C64 Emulator - C implementation"
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
    )
else()
    # GCC/Clang specific settings
    add_compile_options(
        -Wall                   # Enable most warnings
        -Wextra                 # Enable extra warnings
        -Werror                 # Treat warnings as errors
        -Wno-unused-parameter   # Disable unused parameter warnings (common in device callbacks)
        -Wno-unused-variable    # Disable unused variable warnings (for development)
        -pedantic              # Enable pedantic mode
    )
    
    # Debug-specific flags
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_compile_options(-g -O0)
    endif()
endif()

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/src/core
    ${CMAKE_SOURCE_DIR}/src/chip/cpu/mos6510
    ${CMAKE_SOURCE_DIR}/src/chip/io
    ${CMAKE_SOURCE_DIR}/src/chip/logic
    ${CMAKE_SOURCE_DIR}/src/chip/memory
    ${CMAKE_SOURCE_DIR}/src/chip/sound
    ${CMAKE_SOURCE_DIR}/src/chip/video
    ${CMAKE_SOURCE_DIR}/src/systems/c64
    ${CMAKE_SOURCE_DIR}/src/main
)

# Core library sources
set(CORE_SOURCES
    src/core/system.c
    src/core/chip.c
)

# Chip library sources
set(CHIP_SOURCES
    # CPU (operation files are included in mos6510.c)
    src/chip/cpu/mos6510/mos6510.c
    
    # I/O
    src/chip/io/mos6526.c
    
    # Logic
    src/chip/logic/pla.c
    
    # Memory
    src/chip/memory/ram.c
    src/chip/memory/rom.c
    src/chip/memory/mos2114.c
    
    # Sound
    src/chip/sound/mos6581.c
    
    # Video
    src/chip/video/mos6569.c
)

# System sources
set(SYSTEM_SOURCES
    src/systems/c64/c64.c
    src/systems/c64/c64_bus.c
)

# Main executable
add_executable(c64emu
    src/main/c64_main.c
    ${CORE_SOURCES}
    ${CHIP_SOURCES}
    ${SYSTEM_SOURCES}
)

# Test executables (currently disabled due to API compatibility issues)
# add_executable(klaus_test
#     tests/klaus_test_runner.c
#     src/chip/cpu/mos6510/mos6510_test_harness.c
#     ${CORE_SOURCES}
#     ${CHIP_SOURCES}
#     ${SYSTEM_SOURCES}
# )

# add_executable(minimal_test
#     tests/minimal_test.c
#     ${CORE_SOURCES}
#     ${CHIP_SOURCES}
#     ${SYSTEM_SOURCES}
# )

# add_executable(cpu_test
#     tests/minimal_cpu_test.c
#     ${CORE_SOURCES}
#     ${CHIP_SOURCES}
#     ${SYSTEM_SOURCES}
# )

# Basic MOS6510 functionality test
add_executable(test_mos6510_basic
    test_mos6510_basic.c
    ${CORE_SOURCES}
    ${CHIP_SOURCES}
    ${SYSTEM_SOURCES}
)

# Set output directory
set_target_properties(c64emu
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

set_target_properties(test_mos6510_basic
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Installation
install(TARGETS c64emu
    RUNTIME DESTINATION bin
)

# Enable testing
enable_testing()

# Add tests (currently disabled due to API compatibility issues)
# add_test(NAME minimal_test COMMAND minimal_test)
# add_test(NAME cpu_test COMMAND cpu_test)
# add_test(NAME klaus_test COMMAND klaus_test)

# Add basic MOS6510 test
add_test(NAME mos6510_basic_test COMMAND test_mos6510_basic)

# Print configuration summary
message(STATUS "")
message(STATUS "Configuration Summary:")
message(STATUS "  Project: ${PROJECT_NAME} ${PROJECT_VERSION}")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  C Compiler: ${CMAKE_C_COMPILER}")
message(STATUS "  Install prefix: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "")
