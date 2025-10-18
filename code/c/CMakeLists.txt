cmake_minimum_required(VERSION 3.16)

# Project definition
project(aiemuc 
    VERSION 0.1.0
    DESCRIPTION "C64 Emulator - C implementation"
    LANGUAGES C CXX
)

# Optionally enable the cycle-accurate MOS6510 core (experimental)
option(C64_ENABLE_CYCLE_CPU "Enable cycle-accurate MOS6510 core" OFF)

# Set C standard
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)  # Disable compiler-specific extensions

# C++ standard for the new 65xx C++ core
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

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
        /wd4996                 # Disable deprecated function warnings (C++17 headers)
        /wd4463                 # Disable bit-field overflow warnings
        /wd4505                 # Disable unreferenced function with internal linkage warnings
        /wd4189                 # Disable local variable initialized but not referenced warnings
        /wd4244                 # Disable data conversion warnings
    )
    # Define to silence C++17 deprecation warnings
    add_compile_definitions(_SILENCE_CXX17_C_HEADER_DEPRECATION_WARNING)
else()
    # GCC/Clang specific settings
    add_compile_options(
        -Wall                   # Enable most warnings
        -Wextra                 # Enable extra warnings
        #-Werror                 # Treat warnings as errors
        -Wno-unused-parameter   # Disable unused parameter warnings (common in device callbacks)
        -Wno-unused-variable    # Disable unused variable warnings (for development)
        #-pedantic              # Enable pedantic mode
    )
    
    # Debug-specific flags
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_compile_options(-g -O0)
    endif()
endif()

# Find required packages for GUI (optional)
find_package(OpenGL QUIET)

# Check for local SDL2 installation
set(SDL2_DIR "${CMAKE_CURRENT_SOURCE_DIR}/deps/sdl2/SDL2-2.30.10")
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

# GUI detection logic - re-enabled with new fam65xx support
if(SDL2_FOUND AND OpenGL_FOUND)
    message(STATUS "SDL2 and OpenGL found - GUI will be built with fam65xx support")
    set(BUILD_GUI TRUE)
else()
    message(STATUS "SDL2 or OpenGL not found - GUI will be skipped")
    set(BUILD_GUI FALSE)
endif()

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core
    ${CMAKE_CURRENT_SOURCE_DIR}/src/chip/cpu/mos6510
    ${CMAKE_CURRENT_SOURCE_DIR}/src/chip/cpu/fam65xx
    ${CMAKE_CURRENT_SOURCE_DIR}/src/chip/io
    ${CMAKE_CURRENT_SOURCE_DIR}/src/chip/logic
    ${CMAKE_CURRENT_SOURCE_DIR}/src/chip/memory
    ${CMAKE_CURRENT_SOURCE_DIR}/src/chip/sound
    ${CMAKE_CURRENT_SOURCE_DIR}/src/chip/video
    ${CMAKE_CURRENT_SOURCE_DIR}/src/systems/c64
    ${CMAKE_CURRENT_SOURCE_DIR}/src/main
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gui
)

# Add cycle-accurate MOS6510 include path only when enabled
if(C64_ENABLE_CYCLE_CPU)
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src/chip/cpu/mos6510_cycle)
endif()

# Add GUI-specific includes only if building GUI
if(BUILD_GUI)
    include_directories(
        ${CMAKE_CURRENT_SOURCE_DIR}/external/cimgui
        ${CMAKE_CURRENT_SOURCE_DIR}/external/cimgui/imgui
        ${CMAKE_CURRENT_SOURCE_DIR}/external/cimgui/imgui/backends
        ${SDL2_INCLUDE_DIRS}
    )
endif()

# Add cimgui subdirectory if building GUI
if(BUILD_GUI)
    # Check if cimgui directory exists
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/external/cimgui/CMakeLists.txt")
        # Configure cimgui to build as static library
        set(IMGUI_STATIC "yes" CACHE STRING "Build as a static library" FORCE)
        
        # Add cimgui from the external directory
        add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/external/cimgui ${CMAKE_CURRENT_BINARY_DIR}/external/cimgui)
    else()
        message(WARNING "cimgui not found at ${CMAKE_CURRENT_SOURCE_DIR}/external/cimgui - disabling GUI build")
        set(BUILD_GUI FALSE)
    endif()
endif()

# Core library sources
set(CORE_SOURCES
    src/core/system.c
    src/core/chip.c
    src/core/ioport.c
)

# Chip library sources
set(CHIP_SOURCES
    src/chip/cpu/mos6510/mos6510.cpp
)

# Conditionally add cycle-accurate MOS6510 sources (DISABLED - sources not available)
# if(C64_ENABLE_CYCLE_CPU)
#     set(CYCLE_CPU_SOURCES
#         src/chip/cpu/mos6510_cycle/cpu_config.c
#         src/chip/cpu/mos6510_cycle/deferred_ops.c
#         src/chip/cpu/mos6510_cycle/instruction_table.c
#         src/chip/cpu/mos6510_cycle/internal_bus.c
#         src/chip/cpu/mos6510_cycle/interrupt_recognition.c
#         src/chip/cpu/mos6510_cycle/mos6510_registers.c
#         src/chip/cpu/mos6510_cycle/mos6510_state.c
#         src/chip/cpu/mos6510_cycle/mos6510_tick.c
#         src/chip/cpu/mos6510_cycle/nmi_skipping.c
#         src/chip/cpu/mos6510_cycle/pipeline.c
#         src/chip/cpu/mos6510_cycle/pla_lookup.c
#         src/chip/cpu/mos6510_cycle/timing_states.c
#     )
#     list(APPEND CHIP_SOURCES ${CYCLE_CPU_SOURCES})
#
#     # Relax warnings for cycle core sources only to allow build on MSVC
#     if(MSVC)
#         set_source_files_properties(${CYCLE_CPU_SOURCES} PROPERTIES
#             COMPILE_FLAGS "/WX- /wd4244 /wd4996 /wd4133 /wd4013"
#             COMPILE_DEFINITIONS "_CRT_SECURE_NO_WARNINGS"
#         )
#     endif()
# endif()

# Always include other chip sources
list(APPEND CHIP_SOURCES
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
# Enable REDESIGN CPU dispatch mechanism with Nostradamus Distributor
target_compile_definitions(c64emu PRIVATE REDESIGN=1)
if(C64_ENABLE_CYCLE_CPU)
    target_compile_definitions(c64emu PRIVATE C64_ENABLE_CYCLE_CPU=1)
else()
    target_compile_definitions(c64emu PRIVATE C64_ENABLE_CYCLE_CPU=0)
endif()


# Link required libraries
# Note: mos6502_family functions are inline in headers, no separate library needed
if(NOT WIN32 AND NOT MSVC)
    target_link_libraries(c64emu m)  # Add math library for tanhf (Unix only)
endif()

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
            src/chip/logic/pla_gui.c
            src/chip/cpu/fam65xx/fam65xx_gui.c
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
    # Enable REDESIGN CPU dispatch mechanism with Nostradamus Distributor
    target_compile_definitions(c64emu_gui PRIVATE REDESIGN=1)
    if(C64_ENABLE_CYCLE_CPU)
        target_compile_definitions(c64emu_gui PRIVATE C64_ENABLE_CYCLE_CPU=1)
    else()
        target_compile_definitions(c64emu_gui PRIVATE C64_ENABLE_CYCLE_CPU=0)
    endif()

    
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
        cimgui  # Properly linked cimgui static library
        ${SDL2_LIBRARIES}
        ${OPENGL_LIBRARIES}
        ${CMAKE_DL_LIBS}
    )
    
    # Add platform-specific libraries for GUI
    if(WIN32)
        target_link_libraries(c64emu_gui imm32)  # Required for ImGui on Windows
    endif()
    
    # Add math library for GUI version too (Unix only)
    if(NOT WIN32 AND NOT MSVC)
        target_link_libraries(c64emu_gui m)  # Add math library for tanhf (Unix only)
    endif()

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
            "${CMAKE_CURRENT_SOURCE_DIR}/deps/sdl2/SDL2-2.30.10/lib/x64/SDL2.dll"
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



# Link math library for test executable too

# Set output directory
set_target_properties(c64emu
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
add_subdirectory(tests)  # Re-enabled for fam65xx testing

# Add tests (currently disabled due to API compatibility issues)
# add_test(NAME minimal_test COMMAND minimal_test)
# add_test(NAME cpu_test COMMAND cpu_test)
# add_test(NAME klaus_test COMMAND klaus_test)

# Add basic MOS6510 test

# Print configuration summary
message(STATUS "")
message(STATUS "Configuration Summary:")
message(STATUS "  Project: ${PROJECT_NAME} ${PROJECT_VERSION}")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  C Compiler: ${CMAKE_C_COMPILER}")
message(STATUS "  Install prefix: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "")
