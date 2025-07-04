cmake_minimum_required(VERSION 3.16)

# Project definition
project(aiemuc 
    VERSION 0.1.0
    DESCRIPTION "C64 Emulator - C implementation"
    LANGUAGES C CXX
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
        /wd4201                 # Disable nameless struct/union warning (ImGui compatibility)
        /wd4100                 # Disable unreferenced parameter warning
        /wd5287                 # Disable different enum types warning (ImGui compatibility)
    )
else()
    # GCC/Clang specific settings
    add_compile_options(
        -Wall                   # Enable most warnings
        -Wextra                 # Enable extra warnings
        -Werror                 # Treat warnings as errors
        -Wno-unused-parameter   # Disable unused parameter warnings (common in device callbacks)
        -Wno-unused-variable    # Disable unused variable warnings (for development)
        -Wno-unused-function    # Disable unused function warnings (for development)
        -pedantic              # Enable pedantic mode
    )
    
    # Debug-specific flags
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_compile_options(-g -O0)
    endif()
endif()

# Find required packages for GUI (optional)
find_package(OpenGL QUIET)

# Check for local SDL2 installation
set(SDL2_DIR "${CMAKE_SOURCE_DIR}/deps/sdl2/SDL2-2.30.10")
if(EXISTS "${SDL2_DIR}/include/SDL.h")
    set(SDL2_FOUND TRUE)
    set(SDL2_INCLUDE_DIRS "${SDL2_DIR}/include")
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        # 64-bit - exclude SDL2main.lib since we define our own main
        set(SDL2_LIBRARIES "${SDL2_DIR}/lib/x64/SDL2.lib")
        set(SDL2_DLLS "${SDL2_DIR}/lib/x64/SDL2.dll")
    else()
        # 32-bit - exclude SDL2main.lib since we define our own main
        set(SDL2_LIBRARIES "${SDL2_DIR}/lib/x86/SDL2.lib")
        set(SDL2_DLLS "${SDL2_DIR}/lib/x86/SDL2.dll")
    endif()
    message(STATUS "Using local SDL2 from: ${SDL2_DIR}")
else()
    # Fallback to pkg-config
    find_package(PkgConfig QUIET)
    if (PkgConfig_FOUND)
        pkg_check_modules(SDL2 sdl2)
    else()
        set(SDL2_FOUND FALSE)
    endif()
endif()

if(SDL2_FOUND AND OpenGL_FOUND)
    message(STATUS "SDL2 and OpenGL found - GUI will be built")
    set(BUILD_GUI TRUE)
else()
    message(STATUS "SDL2 or OpenGL not found - GUI will be skipped")
    set(BUILD_GUI FALSE)
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
    ${CMAKE_SOURCE_DIR}/src/gui
    ${CMAKE_SOURCE_DIR}/../../external/cimgui
    ${CMAKE_SOURCE_DIR}/../../external/cimgui/imgui
    ${CMAKE_SOURCE_DIR}/../../external/cimgui/imgui/backends
    ${SDL2_INCLUDE_DIRS}
)

# Add cimgui subdirectory if building GUI
if(BUILD_GUI)
    # Configure cimgui to build as static library
    set(IMGUI_STATIC "yes" CACHE STRING "Build as a static library" FORCE)
    
    add_subdirectory(../../external/cimgui cimgui_build)
endif()

# Core library sources
set(CORE_SOURCES
    src/core/system.c
    src/core/chip.c
)

# Chip library sources
set(CHIP_SOURCES    # CPU family core functions (shared by all family members)
    src/chip/cpu/fam65xx/fam65xx_core.c
    src/chip/cpu/fam65xx/fam65xx_opcodes.c
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
    src/chip/video/vic_ii/mos6569.c
    src/chip/video/vic_ii/mos6567.c
    src/chip/video/vic_ii/vicii_common.c
)

# System sources
set(SYSTEM_SOURCES
    src/systems/c64/c64.c
    src/systems/c64/c64_bus.c
    src/systems/c64/c64_config.c
)

# Core utility sources (generic functionality)
set(CORE_UTIL_SOURCES
    src/core/storage/rom_loader.c
    src/core/config/path_discovery.c
)

# Main executable (console version)
add_executable(c64emu
    src/main/c64_main.c
    ${CORE_SOURCES}
    ${CHIP_SOURCES}
    ${SYSTEM_SOURCES}
    ${CORE_UTIL_SOURCES}
)

# Link required libraries
# Note: mos6502_family functions are inline in headers, no separate library needed
target_link_libraries(c64emu)

# Set output directory for console executable
set_target_properties(c64emu
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# GUI executable (ImGui version) - only if SDL2 and OpenGL are available
if(BUILD_GUI)
    # GUI sources (just our interface file - cimgui handles the rest)
    set(GUI_SOURCES
        src/gui/cimgui_interface.c
        src/gui/cimgui_backends.cpp
        src/chip/video/vic_ii/vicii_gui.c
        src/chip/cpu/fam65xx/fam65xx_gui.c
        src/chip/cpu/mos6510/mos6510_gui.c
    )

    add_executable(c64emu_gui
        src/main/c64_main_gui.c
        ${CORE_SOURCES}
        ${CHIP_SOURCES}
        ${SYSTEM_SOURCES}
        ${CORE_UTIL_SOURCES}
        ${GUI_SOURCES}
    )

    # Define CIMGUI_DEFINE_ENUMS_AND_STRUCTS only for our GUI code
    target_compile_definitions(c64emu_gui PRIVATE CIMGUI_DEFINE_ENUMS_AND_STRUCTS=1)
    
    # Add SDL2 include directories to the GUI target
    target_include_directories(c64emu_gui PRIVATE ${SDL2_INCLUDE_DIRS})
    
    # Disable some warnings for GUI build to work around cimgui header issues
    if(MSVC)
        target_compile_options(c64emu_gui PRIVATE /WX- /wd4201 /wd4996 /wd4244 /wd4189 /wd4100 /wd5287)
    else()
        target_compile_options(c64emu_gui PRIVATE -Wno-pedantic)
    endif()

    # Link libraries for GUI version
    target_link_libraries(c64emu_gui
        ${SDL2_LIBRARIES}
        ${OPENGL_LIBRARIES}
        ${CMAKE_DL_LIBS}
        cimgui
    )

    # Set output directory for GUI executable (keep as console app for now)
    set_target_properties(c64emu_gui
        PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
        WIN32_EXECUTABLE FALSE
    )
    
    # Copy SDL2.dll to output directory on Windows
    if(WIN32)
        add_custom_command(TARGET c64emu_gui POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/deps/sdl2/SDL2-2.30.10/lib/x64/SDL2.dll"
            "$<TARGET_FILE_DIR:c64emu_gui>/SDL2.dll"
            COMMENT "Copying SDL2.dll to output directory"
        )
    endif()
    
    message(STATUS "GUI executable c64emu_gui will be built")
else()
    message(STATUS "GUI executable c64emu_gui will be skipped (missing SDL2 or OpenGL)")
endif()

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
    ${CORE_UTIL_SOURCES}
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
if(BUILD_GUI)
    install(TARGETS c64emu c64emu_gui
        RUNTIME DESTINATION bin
    )
else()
    install(TARGETS c64emu
        RUNTIME DESTINATION bin
    )
endif()

# Enable testing
enable_testing()

# Add tests subdirectory for comprehensive MOS6510 testing
add_subdirectory(tests)

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
