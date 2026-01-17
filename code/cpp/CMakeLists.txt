cmake_minimum_required(VERSION 3.16)

# Project definition
project(aiemuc
    VERSION 0.1.0
    DESCRIPTION "C64 Emulator - C++ implementation"
    LANGUAGES CXX
)

# CRITICAL: Force all binaries to output to a single consistent location
# This prevents confusion from multiple binary locations across different build methods
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/build/bin")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_SOURCE_DIR}/build/bin")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/build/bin")

# Set C++ standard for the entire project
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
        /wd4702                 # Disable unreachable code warnings (template instantiation)
        /wd4005                 # Disable macro redefinition warnings (ImGui)
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

# GUI include directories will be set later after detecting ImGui location

# Create ImGui library if building GUI
if(BUILD_GUI)
    # Check if imgui directory exists (try project root external first, then code/cpp/external)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../external/imgui/imgui.h")
        set(IMGUI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../external/imgui")
    elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/external/imgui/imgui.h")
        set(IMGUI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/imgui")
    else()
        set(IMGUI_DIR "")
    endif()
    
    if(IMGUI_DIR)
        # Create ImGui static library
        add_library(imgui STATIC
            ${IMGUI_DIR}/imgui.cpp
            ${IMGUI_DIR}/imgui_demo.cpp
            ${IMGUI_DIR}/imgui_draw.cpp
            ${IMGUI_DIR}/imgui_tables.cpp
            ${IMGUI_DIR}/imgui_widgets.cpp
            ${IMGUI_DIR}/backends/imgui_impl_sdl2.cpp
            ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
        )
        
        # Set ImGui include directories
        target_include_directories(imgui PUBLIC
            ${IMGUI_DIR}
            ${IMGUI_DIR}/backends
            ${SDL2_INCLUDE_DIRS}
        )
        
        # Link ImGui with OpenGL and SDL2
        target_link_libraries(imgui ${SDL2_LIBRARIES} ${OPENGL_LIBRARIES})
        
        message(STATUS "Using native Dear ImGui C++ from ${IMGUI_DIR}")
    else()
        message(WARNING "Dear ImGui not found - disabling GUI build")
        set(BUILD_GUI FALSE)
    endif()
endif()

# Add NFD (Native File Dialog Extended) library if building GUI
if(BUILD_GUI)
    # Check if NFD directory exists (in project root external directory)
    set(NFD_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../external/nfd")
    if(EXISTS "${NFD_ROOT}/CMakeLists.txt")
        # Use NFD's own CMakeLists.txt as a subdirectory
        add_subdirectory(${NFD_ROOT} ${CMAKE_CURRENT_BINARY_DIR}/nfd EXCLUDE_FROM_ALL)
        set(NFD_FOUND TRUE)
        message(STATUS "NFD (Native File Dialog) library configured at ${NFD_ROOT}")
    else()
        message(WARNING "NFD not found at ${NFD_ROOT} - file dialogs will not be available")
        set(NFD_FOUND FALSE)
    endif()
endif()

# Core library sources
set(CORE_SOURCES
    src/core/system.cpp
    src/core/chip.cpp
    src/core/ioport.cpp
    src/core/chip_layout.cpp
    src/core/pin_labels.cpp
    src/core/emulation_context.cpp
    src/core/emulated_system.cpp
    src/core/system_registry.cpp
    src/core/framebuffer_renderer.cpp
)

# FAM65XX Decoder - standalone library for instruction decoding
add_library(fam65xx_decoder STATIC
    src/chip/cpu/fam65xx/fam65xx_decoder.cpp
)
target_include_directories(fam65xx_decoder PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/chip/cpu/fam65xx
)

# Chip library sources - Updated for fam65xx
set(CHIP_SOURCES
    # CPU sources - using fam65xx template-based implementation
    src/chip/cpu/fam65xx/mos6502.cpp
    src/chip/cpu/fam65xx/mos6510.cpp
    src/chip/cpu/fam65xx/nes6502.cpp
    src/chip/cpu/fam65xx/wdc65c02.cpp
    src/chip/cpu/fam65xx/rockwell65c02.cpp
    src/chip/cpu/fam65xx/wdc65c816.cpp
)
# Always include other chip sources
list(APPEND CHIP_SOURCES
    # Input
    src/chip/input/commodore_keyboard.cpp

    # I/O
    src/chip/io/mos6526.cpp

    # Logic
    src/chip/logic/pla.cpp

    # Memory
    src/chip/memory/ram.cpp
    src/chip/memory/rom.cpp
    src/chip/memory/mos2114.cpp

    # Sound
    src/chip/sound/mos6581.cpp

    # Video
    src/chip/video/vic_ii/mos6569.cpp
    src/chip/video/vic_ii/mos6567.cpp
    src/chip/video/vic_ii/vicii_common.cpp
)
# System sources
set(SYSTEM_SOURCES
    # C64 system
    src/systems/c64/c64.cpp
    src/systems/c64/c64_bus.cpp
    src/systems/c64/c64_chips.cpp
    src/systems/c64/c64_config.cpp
    src/systems/c64/c64_test_loader.cpp
    src/systems/c64/c64_banking_verify.cpp
    src/systems/c64/c64_test_framework.cpp
    src/systems/c64/c64_screenshot.cpp
    src/systems/c64/c64_system_wrapper.cpp
    
    # CHIP-8 system
    src/systems/chip8/chip8_system.cpp
)


# Core utility sources (generic functionality)
set(CORE_UTIL_SOURCES
    src/core/storage/rom_loader.cpp
    src/core/config/path_discovery.cpp
)
# GUI sources (defined here so both c64emu_gui and multi_emu can use it)
if(BUILD_GUI)
    set(GUI_SOURCES
        src/gui/imgui_interface.cpp
        src/gui/chip_visualization.cpp
        src/gui/global_chip_style.cpp
        src/chip/video/vic_ii/vicii_gui.cpp
        src/chip/video/vic_ii/mos6567_gui.cpp
        src/chip/video/vic_ii/mos6569_gui.cpp
        src/chip/logic/pla_gui.cpp
        src/chip/cpu/fam65xx/fam65xx_gui.cpp
        src/chip/io/mos6526_gui.cpp
        src/chip/memory/ram_gui.cpp
        src/chip/memory/mos2114_gui.cpp
        src/chip/sound/mos6581_gui.cpp
    )
    
    # Generic GUI sources for multi-system support
    set(GENERIC_GUI_SOURCES
        src/gui/generic_gui.cpp
        src/gui/simple_system_gui.cpp
    )
endif()

# Main executable (console version)
add_executable(c64emu
    src/main/c64_main.cpp
    ${CORE_SOURCES}
    ${CHIP_SOURCES}
    ${SYSTEM_SOURCES}
    ${CORE_UTIL_SOURCES}
)

# Multi-system console test (no GUI dependencies)
add_executable(multi_emu_console
    src/main/multi_emu_console.cpp
    ${CORE_SOURCES}
    ${CHIP_SOURCES}
    ${SYSTEM_SOURCES}
    ${CORE_UTIL_SOURCES}
)

target_link_libraries(multi_emu_console
    fam65xx_decoder
)

if(NOT WIN32 AND NOT MSVC)
    target_link_libraries(multi_emu_console m)
endif()

message(STATUS "Console test executable multi_emu_console will be built")

# Multi-system GUI executable
if(BUILD_GUI)
    add_executable(multi_emu
        src/main/multi_emu_main.cpp
        ${CORE_SOURCES}
        ${CHIP_SOURCES}
        ${SYSTEM_SOURCES}
        ${CORE_UTIL_SOURCES}
        ${GUI_SOURCES}
        ${GENERIC_GUI_SOURCES}
    )
    
    # Add include directories
    target_include_directories(multi_emu PRIVATE
        ${SDL2_INCLUDE_DIRS}
        ${IMGUI_DIR}
        ${IMGUI_DIR}/backends
    )
    
    # Add NFD include if available
    if(NFD_FOUND)
        target_include_directories(multi_emu PRIVATE ${NFD_ROOT}/src/include)
    endif()
    
    # Define IMGUI_VERSION macro
    target_compile_definitions(multi_emu PRIVATE IMGUI_VERSION="1.92.1")
    
    # Disable warnings
    if(MSVC)
        target_compile_options(multi_emu PRIVATE /WX- /wd4201 /wd4996 /wd4244 /wd4189 /wd4100 /wd5287 /wd4702 /wd4005)
    else()
        target_compile_options(multi_emu PRIVATE -Wno-pedantic)
    endif()
    
    # Link libraries
    set(MULTI_EMU_LINK_LIBS
        imgui
        fam65xx_decoder
        ${SDL2_LIBRARIES}
        ${OPENGL_LIBRARIES}
        ${CMAKE_DL_LIBS}
    )
    
    if(NFD_FOUND)
        list(APPEND MULTI_EMU_LINK_LIBS nfd)
    endif()
    
    target_link_libraries(multi_emu ${MULTI_EMU_LINK_LIBS})
    
    # Platform-specific libraries
    if(WIN32)
        target_link_libraries(multi_emu imm32)
    endif()
    
    if(NOT WIN32 AND NOT MSVC)
        target_link_libraries(multi_emu m)
    endif()
    
    # Copy SDL2.dll on Windows
    if(WIN32)
        add_custom_command(TARGET multi_emu POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/deps/sdl2/SDL2-2.30.10/lib/x64/SDL2.dll"
            "$<TARGET_FILE_DIR:multi_emu>/SDL2.dll"
            COMMENT "Copying SDL2.dll for multi_emu"
        )
    endif()
    
    message(STATUS "Multi-system GUI executable multi_emu will be built")
endif()

# Link required libraries
# Link required libraries
target_link_libraries(c64emu
    fam65xx_decoder  # FAM65XX instruction decoder
)

if(NOT WIN32 AND NOT MSVC)
    target_link_libraries(c64emu m)  # Add math library for tanhf (Unix only)
endif()

# Output directory already set globally at top of CMakeLists.txt
# No need to set it per-target

# GUI executable (ImGui version) - only if SDL2 and OpenGL are available
if(BUILD_GUI)
    add_executable(c64emu_gui
        src/main/c64_main_gui.cpp
        ${CORE_SOURCES}
        ${CHIP_SOURCES}
        ${SYSTEM_SOURCES}
        ${CORE_UTIL_SOURCES}
        ${GUI_SOURCES}
    )
    # Add SDL2, ImGui, and NFD include directories to the GUI target
    target_include_directories(c64emu_gui PRIVATE
        ${SDL2_INCLUDE_DIRS}
        ${IMGUI_DIR}
        ${IMGUI_DIR}/backends
    )
    
    # Add NFD include if available
    if(NFD_FOUND)
        target_include_directories(c64emu_gui PRIVATE ${NFD_ROOT}/src/include)
    endif()
    
    # Define IMGUI_VERSION macro for GUI build to enable chip GUI functions
    target_compile_definitions(c64emu_gui PRIVATE IMGUI_VERSION="1.92.1")
    
    # Disable some warnings for GUI build to work around ImGui header issues
    if(MSVC)
        target_compile_options(c64emu_gui PRIVATE /WX- /wd4201 /wd4996 /wd4244 /wd4189 /wd4100 /wd5287 /wd4702 /wd4005)
    else()
        target_compile_options(c64emu_gui PRIVATE -Wno-pedantic)
    endif()

    # Link libraries for GUI version
    set(GUI_LINK_LIBS
        imgui  # Native Dear ImGui C++ static library
        fam65xx_decoder  # FAM65XX instruction decoder
        ${SDL2_LIBRARIES}
        ${OPENGL_LIBRARIES}
        ${CMAKE_DL_LIBS}
    )
    
    # Add NFD library if available
    if(NFD_FOUND)
        list(APPEND GUI_LINK_LIBS nfd)
    endif()
    
    target_link_libraries(c64emu_gui ${GUI_LINK_LIBS})
    
    # Add platform-specific libraries for GUI
    if(WIN32)
        target_link_libraries(c64emu_gui imm32)  # Required for ImGui on Windows
    endif()
    
    # Add math library for GUI version too (Unix only)
    if(NOT WIN32 AND NOT MSVC)
        target_link_libraries(c64emu_gui m)  # Add math library for tanhf (Unix only)
    endif()

    # Output directory already set globally at top of CMakeLists.txt
    # Keep as console app for debugging
    set_target_properties(c64emu_gui
        PROPERTIES
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
# Output directory already set globally at top of CMakeLists.txt


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

# Test runner tool
add_executable(c64_test_runner
    tools/c64_test_runner.cpp
    ${CORE_SOURCES}
    ${CHIP_SOURCES}
    ${SYSTEM_SOURCES}
    ${CORE_UTIL_SOURCES}
)

# Link required libraries for test runner
target_link_libraries(c64_test_runner
    fam65xx_decoder
)

if(NOT WIN32 AND NOT MSVC)
    target_link_libraries(c64_test_runner m)
endif()

message(STATUS "Test runner c64_test_runner will be built")

# Print configuration summary
message(STATUS "")
message(STATUS "Configuration Summary:")
message(STATUS "  Project: ${PROJECT_NAME} ${PROJECT_VERSION}")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  CXX Compiler: ${CMAKE_CXX_COMPILER}")
message(STATUS "  Install prefix: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "")
