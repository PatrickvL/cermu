cmake_minimum_required(VERSION 3.16)

# ---- vcpkg integration (optional) -----------------------------------------
# When the vcpkg toolchain is active (passed via -DCMAKE_TOOLCHAIN_FILE or the
# VCPKG_ROOT environment variable), use our custom triplet overlay so that
# Windows gets static libs with dynamic CRT (x64-windows-static-md).
if(DEFINED ENV{VCPKG_ROOT} AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
        CACHE STRING "vcpkg toolchain file")
endif()
if(WIN32 OR MSVC)
    set(VCPKG_OVERLAY_TRIPLETS "${CMAKE_CURRENT_SOURCE_DIR}/cmake/triplets"
        CACHE STRING "vcpkg triplet overlay")
    if(NOT DEFINED VCPKG_TARGET_TRIPLET)
        set(VCPKG_TARGET_TRIPLET "x64-windows-static-md"
            CACHE STRING "vcpkg target triplet")
    endif()
endif()

# Project definition
project(cermu
    VERSION 0.1.0
    DESCRIPTION "cermu - Multi-system emulator"
    LANGUAGES CXX
)

# Default to RelWithDebInfo (optimized with debug symbols) for single-config
# generators.  Debug builds disable all inlining, making cycle-accurate chip
# emulation 3-5× slower than real-time.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Build type" FORCE)
    message(STATUS "Build type defaulted to RelWithDebInfo")
endif()

# Add toml++ library using FetchContent
include(FetchContent)
FetchContent_Declare(
    tomlplusplus
    GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git
    GIT_TAG v3.4.0
)
FetchContent_MakeAvailable(tomlplusplus)

# Find libarchive (archive reading — ZIP, 7z, RAR, tar, etc.)
# Strategy: CMake find_package first (works with vcpkg and system installs),
# then PkgConfig (Linux), then fall back to stubs.
#
# When building with vcpkg static linkage, libarchive's vcpkg wrapper
# injects transitive dependencies (lz4, zstd, …) via set_property on
# LibArchive::LibArchive.  With CMake 4.x this becomes a hard error if
# those targets don't exist yet.  Work around by pre-loading them.
# The QUIET flag ensures this is harmless on systems without vcpkg.
if(DEFINED VCPKG_TARGET_TRIPLET)
    find_package(lz4 CONFIG QUIET)
    find_package(zstd CONFIG QUIET)
    find_package(ZLIB QUIET)
    find_package(BZip2 QUIET)
    find_package(LibLZMA QUIET)
    find_package(OpenSSL QUIET)
endif()

set(LIBARCHIVE_FOUND FALSE)
find_package(LibArchive QUIET)
if(LibArchive_FOUND)
    set(LIBARCHIVE_FOUND TRUE)
    # Prefer the imported target when available (vcpkg, modern CMake).
    # Set LIBARCHIVE_LIBRARIES so every target_link_libraries() call works
    # whether we got the imported target or the raw .lib path.
    if(TARGET LibArchive::LibArchive)
        # Fix multi-config (MSVC) debug/release selection.  CMake's built-in
        # FindLibArchive sets a single IMPORTED_LOCATION from find_library(),
        # which may pick the debug lib.  When vcpkg provides both debug and
        # release static libs, patch the target with per-config locations so
        # the linker gets the right CRT flavour.
        get_target_property(_la_loc LibArchive::LibArchive IMPORTED_LOCATION)
        if(_la_loc MATCHES "/debug/")
            string(REPLACE "/debug/" "/" _la_release "${_la_loc}")
            if(EXISTS "${_la_release}")
                set_target_properties(LibArchive::LibArchive PROPERTIES
                    IMPORTED_LOCATION                 "${_la_release}"
                    IMPORTED_LOCATION_RELEASE         "${_la_release}"
                    IMPORTED_LOCATION_RELWITHDEBINFO  "${_la_release}"
                    IMPORTED_LOCATION_MINSIZEREL      "${_la_release}"
                    IMPORTED_LOCATION_DEBUG            "${_la_loc}")
            endif()
        endif()
        set(LIBARCHIVE_LIBRARIES LibArchive::LibArchive)
        set(LIBARCHIVE_INCLUDE_DIRS "")  # target carries its own includes
    else()
        set(LIBARCHIVE_INCLUDE_DIRS ${LibArchive_INCLUDE_DIRS})
        set(LIBARCHIVE_LIBRARIES ${LibArchive_LIBRARIES})
    endif()
    message(STATUS "Found libarchive via CMake find_package")
endif()
if(NOT LIBARCHIVE_FOUND)
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(LIBARCHIVE QUIET libarchive)
        if(LIBARCHIVE_FOUND)
            message(STATUS "Found libarchive via pkg-config")
        endif()
    endif()
endif()
if(NOT LIBARCHIVE_FOUND)
    message(STATUS "libarchive not found — archive support disabled (using stubs)")
    set(LIBARCHIVE_INCLUDE_DIRS "")
    set(LIBARCHIVE_LIBRARIES "")
    add_compile_definitions(CERMU_NO_LIBARCHIVE)
endif()

# Find SQLite3 (catalog database for ROM library)
find_package(SQLite3 QUIET)
if(SQLite3_FOUND)
    message(STATUS "Found SQLite3: ${SQLite3_VERSION}")
else()
    # Try pkg-config as fallback
    if(PkgConfig_FOUND)
        pkg_check_modules(SQLITE3 QUIET sqlite3)
    endif()
    if(NOT SQLITE3_FOUND)
        message(STATUS "SQLite3 not found — catalog/library features disabled")
        add_compile_definitions(CERMU_NO_SQLITE)
    endif()
endif()

# CRITICAL: Force all binaries to output to a single consistent location
# This prevents confusion from multiple binary locations across different build methods
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/build/bin")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_SOURCE_DIR}/build/bin")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/build/bin")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_CURRENT_SOURCE_DIR}/build/bin")

# Set C++ standard for the entire project
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Set default build type to Release BEFORE setting CMAKE_CONFIGURATION_TYPES.
# This ensures that builds without an explicit -DCMAKE_BUILD_TYPE=... get
# full optimization (-O3 -DNDEBUG) instead of an unoptimized debug build.
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "RelWithDebInfo" "MinSizeRel")
    message(STATUS "No build type specified — defaulting to Release")
endif()

# Enable extra build types for multi-config generators (Visual Studio, Xcode)
set(CMAKE_CONFIGURATION_TYPES "Debug;Release;RelWithDebInfo;MinSizeRel" CACHE STRING "" FORCE)

# Compiler-specific settings and warning flags
if(MSVC)
    # MSVC specific settings
    add_compile_options(
        /Zc:preprocessor        # Use conforming preprocessor (__VA_OPT__, correct __VA_ARGS__)
        /W4                     # Enable all warnings
        /WX                     # Treat warnings as errors
        /utf-8                  # Source and execution character set UTF-8
        /FS                     # Force synchronous PDB writes (parallel compilation)
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
        /wd5030                 # Disable unrecognized attribute warnings (GNU attributes for GCC/Clang)
        /wd4267                 # Disable size_t to int conversion warnings
        /wd4477                 # Disable printf format mismatch warnings
        /wd4099                 # Disable struct/class type mismatch warnings
        /wd4458                 # Disable declaration hides class member warnings
        /wd4457                 # Disable declaration hides function parameter warnings
        /wd4661                 # Disable missing template instantiation warnings (GUI methods in non-GUI builds)
        /wd4701                 # Disable potentially uninitialized local variable warnings
        /wd4127                 # Disable constant conditional expression warnings (if constexpr pattern)
        /wd4456                 # Disable declaration hides previous local declaration warnings
        /wd4576                 # Disable non-standard explicit type conversion warnings (compound literals)
    )
    # Define to silence C++17 deprecation warnings
    add_compile_definitions(_SILENCE_CXX17_C_HEADER_DEPRECATION_WARNING)
    # Define M_PI etc. from <cmath> on MSVC
    add_compile_definitions(_USE_MATH_DEFINES)
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
set(SDL2_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/SDL2/SDL2-2.30.10")
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
# SDL2 include dirs added project-wide because core headers (system.hpp,
# keyboard_mapper.hpp) reference SDL_Keycode types unconditionally.
# LIBARCHIVE include dirs added because os_archive.cpp needs <archive.h>.
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/external
    ${SDL2_INCLUDE_DIRS}
    ${LIBARCHIVE_INCLUDE_DIRS}
)

# GUI include directories will be set later after detecting ImGui location

# Create ImGui library if building GUI
if(BUILD_GUI)
    # Check if imgui directory exists (try project root external paths, then code/cpp/external)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/external/imgui/imgui.h")
        set(IMGUI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/imgui")
    elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/external/cimgui/imgui/imgui.h")
        set(IMGUI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/cimgui/imgui")
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

# Add ImGuiFileDialog library if building GUI
if(BUILD_GUI)
    # Check if ImGuiFileDialog directory exists (in project root external directory)
    set(IMGUIFILEDIALOG_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/external/ImGuiFileDialog")
    if(EXISTS "${IMGUIFILEDIALOG_ROOT}/ImGuiFileDialog.h")
        # Create ImGuiFileDialog static library
        add_library(ImGuiFileDialog STATIC
            ${IMGUIFILEDIALOG_ROOT}/ImGuiFileDialog.cpp
        )
        
        # Set ImGuiFileDialog include directories
        target_include_directories(ImGuiFileDialog PUBLIC
            ${IMGUIFILEDIALOG_ROOT}
            ${IMGUI_DIR}
        )
        # Let ImGuiFileDialog find the custom VFS filesystem header
        target_include_directories(ImGuiFileDialog PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src/gui
        )

        # cermu customizations: VFS integration
        target_compile_definitions(ImGuiFileDialog PRIVATE
            USE_CUSTOM_FILESYSTEM
            CUSTOM_FILESYSTEM_INCLUDE="vfs_file_system.hpp"
        )
        
        # Link ImGuiFileDialog with ImGui
        target_link_libraries(ImGuiFileDialog imgui)
        
        set(IMGUIFILEDIALOG_FOUND TRUE)
        message(STATUS "ImGuiFileDialog library configured at ${IMGUIFILEDIALOG_ROOT}")
    else()
        message(WARNING "ImGuiFileDialog not found at ${IMGUIFILEDIALOG_ROOT} - file dialogs will not be available")
        set(IMGUIFILEDIALOG_FOUND FALSE)
    endif()
endif()

# Core library sources
set(CORE_SOURCES
    src/core/chip.cpp
    src/core/chip_debug_registry.cpp
    src/core/ioport.cpp
    src/core/chip_layout.cpp
    src/core/pin_labels.cpp
    src/core/system.cpp
    src/utils/stb_impl.cpp
    src/core/system_registry.cpp
    src/core/rom_set.cpp
    src/core/text_terminal.cpp
    src/core/port.cpp
    src/core/device_registry.cpp
    src/core/chip_registry.cpp
    src/core/port_registry.cpp
    src/core/session.cpp
    src/core/board_base.cpp
    src/core/audio_thread.cpp
    # Signal — analog video/audio stream infrastructure
    src/core/signal/video_port_bridge.cpp
    # Ports — cross-system port registrations
    src/ports/nes_ports.cpp
    # Devices — input peripherals
    src/devices/input/joystick_device.cpp
    src/devices/input/commodore_1351_mouse.cpp
    src/devices/input/commodore_1312_paddles.cpp
    src/devices/input/commodore_1350_mouse.cpp
    src/devices/input/neos_mouse.cpp
    src/devices/input/lightpen_device.cpp
    src/devices/input/nes_standard_controller.cpp
    src/devices/input/nes_four_score.cpp
    src/devices/input/nes_zapper.cpp
    src/devices/input/snes_standard_controller.cpp
    # Devices — storage
    src/devices/storage/storage_device.cpp
    src/devices/storage/datasette_1530.cpp
    src/devices/storage/drive_1541.cpp
    src/devices/storage/drive_1541_system.cpp
    # Devices — keyboard
    src/devices/keyboard/commodore_keyboard_device.cpp
    # Devices — display
    src/devices/display/generic_crt.cpp
)

# FAM65XX Decoder - standalone library for instruction decoding
add_library(fam65xx_decoder STATIC
    src/chip/cpu/fam65xx/fam65xx_decoder.cpp
)
target_include_directories(fam65xx_decoder PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/chip/cpu/fam65xx
)

# Z80 Decoder - standalone library for instruction decoding
add_library(z80_decoder STATIC
    src/chip/cpu/z80/z80_decoder.cpp
)
target_include_directories(z80_decoder PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/chip/cpu/z80
)

# M680x0 Decoder - standalone library for instruction decoding
add_library(m680x0_decoder STATIC
    src/chip/cpu/m680x0/m680x0_decoder.cpp
)
target_include_directories(m680x0_decoder PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/chip/cpu/m680x0
)

# MC6809 Decoder - standalone library for instruction decoding
add_library(mc6809_decoder STATIC
    src/chip/cpu/mc6809/mc6809_decoder.cpp
)
target_include_directories(mc6809_decoder PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/chip/cpu/mc6809
)

# Chip library sources
set(CHIP_SOURCES
    # CPU - fam65xx template instantiation + ChipRegistry registration
    src/chip/cpu/fam65xx/fam65xx_registry.cpp
    # CPU - Z80 template instantiation + ChipRegistry registration
    src/chip/cpu/z80/z80_registry.cpp
    # CPU - M680x0 template instantiation + ChipRegistry registration
    src/chip/cpu/m680x0/m680x0_registry.cpp
    # CPU - MC6809 template instantiation + ChipRegistry registration
    src/chip/cpu/mc6809/mc6809_registry.cpp
)
# Always include other chip sources
list(APPEND CHIP_SOURCES
    # Input chips
    src/chip/input/commodore_keyboard.cpp
    src/chip/input/input_registry.cpp

    # Input pipeline (host input abstractions)
    src/core/input/keyboard_mapper.cpp
    src/core/input/emu_key_sdl_map.cpp

    # I/O
    src/chip/io/mos6526.cpp
    src/chip/io/mos6522.cpp
    src/chip/io/pia6820.cpp
    src/chip/io/pia6532.cpp
    src/chip/io/io_registry.cpp

    # Logic
    src/chip/logic/pla.cpp
    src/chip/logic/logic_registry.cpp
    src/chip/logic/ls138.cpp
    src/chip/logic/ls139.cpp
    src/chip/logic/ls175.cpp
    # MMU
    src/chip/mmu/mmu_registry.cpp

    # Memory
    src/chip/memory/mos2114.cpp
    src/chip/memory/memory_chip_base.cpp
    src/chip/memory/memory_registry.cpp

    # Sound
    src/chip/sound/mos6581.cpp
    src/chip/sound/sid_waveform_tables.cpp
    src/chip/sound/sound_registry.cpp

    # Video - VIC-II (C64)
    src/chip/video/vic_ii/mos6569.cpp
    src/chip/video/vic_ii/mos6567.cpp
    src/chip/video/vic_ii/vicii_common.cpp
    
    # Video - VIC (VIC-20)
    src/chip/video/vic/mos6560.cpp
    src/chip/video/vic/mos6561.cpp
    src/chip/video/vic/vic_common.cpp
    
    # Video - TED (C16/Plus4)
    src/chip/video/ted/ted7360.cpp
    
    # Video - MC6845/VDC CRTC family (PET, BBC Micro, Amstrad CPC, C128, ...)
    src/chip/video/fam6845/crtc_common.cpp
    
    # Video - TIA (Atari 2600)
    src/chip/video/tia/tia.cpp

    # Video - TMS9918 VDP family (TMS9918/A, TMS9928/29, V9938, V9958, Sega)
    src/chip/video/tms9918/tms9918_registry.cpp

    # Video - KC85 video generator (TTL / U82720)
    src/chip/video/kc85_video/kc85_video.cpp

    # Video - Character display generator (Z9001, Z1013)
    src/chip/video/char_display/char_display.cpp

    # Video - Bomb Jack foreground tile renderer
    src/chip/video/bombjack_video/bombjack_video.cpp

    # Video - Namco arcade tile renderer (Pac-Man, Pengo)
    src/chip/video/namco_video/namco_video.cpp

    # Video - Registry (header-only chips)
    src/chip/video/video_registry.cpp
)
# System sources
set(SYSTEM_SOURCES
    # C64 system
    src/systems/commodore/c64/c64_config.cpp
    src/systems/commodore/c64/c64_system.cpp
    src/systems/commodore/c64/c64_kernal_patches.cpp
    src/systems/commodore/c64/c64_screen_utils.cpp
    src/systems/commodore/c64/c64_sid_player.cpp
    src/systems/commodore/c64/c64_keyboard_matrix.cpp
    
    # CHIP-8 system
    src/systems/chip8/chip8_system.cpp
    
    # VIC-20 system (clean CHIP-8-style implementation)
    src/systems/commodore/vic20/vic20_system.cpp
    src/systems/commodore/vic20/vic20_keyboard_matrix.cpp
    
    # Commodore shared base class (keyboard mapper, config, speed control)
    src/systems/commodore/commodore_system.cpp
    
    # NES system (clean System implementation)
    src/systems/nes/nes_system.cpp
    src/chip/video/nes_ppu/nes_ppu.cpp
    src/systems/nes/bus/nes_bus.cpp
    src/systems/nes/cartridge/nes_cartridge.cpp
    src/systems/nes/nes_screen_utils.cpp
    src/systems/nes/nsf/nes_nsf_player.cpp
    
    # Commodore 264 series (C16/C116/Plus4 — TED 7360 family)
    src/systems/commodore/c16/c16_system.cpp
    src/systems/commodore/c16/c16_keyboard_matrix.cpp
    
    # Apple 1 system (clean System implementation)
    src/systems/apple1/apple1_system.cpp
    
    # Commodore PET system
    src/systems/commodore/pet/pet_system.cpp
    src/systems/commodore/pet/pet_keyboard_matrix.cpp
    
    # Atari 2600 system
    src/systems/atari2600/atari2600_system.cpp
    
    # BBC Micro system
    src/systems/bbc/bbc_micro_system.cpp

    # ZX Spectrum 48K/128K
    src/systems/spectrum/spectrum_system.cpp

    # Amstrad CPC 464/664/6128
    src/systems/amstrad_cpc/amstrad_cpc_system.cpp

    # Acorn Atom
    src/systems/acorn_atom/acorn_atom_system.cpp

    # DDR computers
    src/systems/ddr/lc80/lc80_system.cpp
    src/systems/ddr/z1013/z1013_system.cpp
    src/systems/ddr/z9001/z9001_system.cpp
    src/systems/ddr/kc85/kc85_system.cpp

    # Arcade systems
    src/systems/arcade/bombjack/bombjack_system.cpp
    src/systems/arcade/namco/namco_arcade_system.cpp
    src/systems/arcade/atari_vector/atari_vector_system.cpp

    # Apple II family (Apple II, IIe, IIc)
    src/systems/apple_ii/apple_ii_system.cpp

    # Commodore 128
    src/systems/commodore/c128/c128_system.cpp
    src/systems/commodore/c128/c128_keyboard_matrix.cpp

    # VTech Laser / VZ series (VZ200, VZ300)
    src/systems/vtech_vz/vtech_vz_system.cpp

    # Oric (Oric-1, Oric Atmos)
    src/systems/oric/oric_system.cpp

    # BBC Micro B+ / Master 128
    src/systems/bbc/bbc_master_system.cpp

    # MSX family (MSX1 / MSX2 / MSX2+)
    src/systems/msx/msx_system.cpp

    # Sega SG-1000 / SC-3000
    src/systems/sega/sg1000/sega_sg1000_system.cpp

    # Sega Master System
    src/systems/sega/sms/sega_sms_system.cpp

    # ColecoVision
    src/systems/colecovision/colecovision_system.cpp

    # Memotech MTX500 / MTX512
    src/systems/memotech_mtx/memotech_mtx_system.cpp

    # Tatung Einstein TC-01
    src/systems/tatung_einstein/tatung_einstein_system.cpp

    # Spectravideo SVI-318 / SVI-328
    src/systems/spectravideo/spectravideo_system.cpp

    # MC6809E + MC6847 VDG family (CoCo 1/2, Dragon 32/64)
    src/systems/mc6809_vdg/mc6809_vdg_system.cpp
    src/systems/coco/coco_system.cpp
    src/systems/dragon/dragon_system.cpp

    # Atari 8-bit (800/800XL/130XE)
    src/systems/atari8/atari8_system.cpp

    # Sega Genesis / Mega Drive
    src/systems/genesis/genesis_system.cpp

    # Nintendo Game Boy
    src/systems/gameboy/gameboy_system.cpp

    # NEC PC Engine / TurboGrafx-16
    src/systems/pce/pce_system.cpp

    # Atari ST
    src/systems/atari_st/atari_st_system.cpp
)

# Testing sources
set(TESTING_SOURCES
    src/testing/vicii_test_harness.cpp
    src/testing/vicii_pixel_tests.cpp
    src/testing/c64_test_framework.cpp
    src/testing/c64_test_loader.cpp
    src/testing/c64_screenshot.cpp
    src/testing/c16_test_framework.cpp
)

# Core utility sources (generic functionality)
set(CORE_UTIL_SOURCES
    src/core/storage/rom_loader.cpp
    src/core/config/path_discovery.cpp
    src/core/archive_scanner.cpp
    src/core/formats/format_handler.cpp
    src/core/formats/format_registry.cpp
    src/core/formats/format_load_helpers.cpp
    src/core/formats/prg_format.cpp
    src/core/formats/bin_format.cpp
    src/core/formats/d64_format.cpp
    src/core/formats/t64_format.cpp
    src/core/formats/tap_format.cpp
    src/core/formats/crt_format.cpp
    src/core/formats/lnx_format.cpp
    src/core/formats/sid_format.cpp
    src/core/formats/nsf_format.cpp
    src/core/formats/ines_format.cpp
    src/core/formats/a26_format.cpp
    src/core/formats/sna_format.cpp
    src/core/formats/spectrum_tap_format.cpp
    src/core/formats/z80_snapshot_format.cpp
    src/core/formats/scl_format.cpp
    src/core/formats/trd_format.cpp
    src/core/formats/vz_format.cpp
    src/core/formats/oric_tap_format.cpp
    src/core/formats/cpr_format.cpp
    src/core/formats/ssd_format.cpp
    src/core/formats/cpc_dsk_format.cpp
    src/core/formats/msx_cas_format.cpp
    src/core/formats/uef_format.cpp
    src/core/formats/apple_dsk_format.cpp
    src/core/formats/kc_tap_format.cpp
    src/core/formats/fds_format.cpp
    src/core/formats/d71_format.cpp
    src/core/formats/d81_format.cpp
    src/core/formats/rom_format.cpp
    src/core/formats/coco_cas_format.cpp
    src/core/formats/atari_xex_format.cpp
    src/core/formats/genesis_format.cpp
    src/core/formats/gb_format.cpp
    src/core/formats/pce_format.cpp
    src/core/formats/atari_st_format.cpp
    src/systems/commodore/commodore_load_helpers.cpp
    src/core/os/os_filesystem.cpp
    src/core/os/os_archive.cpp
    src/core/vfs/vfs.cpp
    src/systems/commodore/petscii.cpp
    src/systems/commodore/basic_parser.cpp
)
# GUI sources
if(BUILD_GUI)
    set(GUI_SOURCES
        src/gui/chip_visualization.cpp
        src/gui/chip_debug_registry_gui.cpp
        src/gui/global_chip_style.cpp
        src/chip/video/vic_ii/vicii_gui.cpp
        src/chip/video/vic/vic_gui.cpp
        src/chip/video/ted/ted7360_gui.cpp
        src/systems/commodore/c64/c64_pla_chip.cpp
        src/chip/cpu/fam65xx/fam65xx_gui.cpp
        src/chip/cpu/z80/z80_gui.cpp
        src/chip/cpu/m680x0/m680x0_gui.cpp
        src/chip/cpu/mc6809/mc6809_gui.cpp
        src/chip/io/mos6526_gui.cpp
        src/chip/io/mos6522_gui.cpp
        src/chip/io/mos6529_gui.cpp
        src/chip/mmu/mos8722_gui.cpp
        src/chip/memory/mos2114_gui.cpp
        src/chip/memory/er2055_gui.cpp
        src/chip/sound/mos6581_gui.cpp
        src/chip/sound/nes_apu_gui.cpp
        src/chip/video/nes_ppu/nes_ppu_gui.cpp
        src/chip/input/cd4021_gui.cpp
        src/chip/logic/ls259_gui.cpp
        src/chip/logic/pla_gui.cpp
        src/chip/video/tia/tia_gui.cpp
        src/chip/video/fam6845/crtc_gui.cpp
        src/chip/io/pia6532_gui.cpp
        src/chip/io/pia6820_gui.cpp
        src/chip/io/i8255_gui.cpp
        src/chip/io/z80_pio_gui.cpp
        src/chip/io/z80_ctc_gui.cpp
        src/chip/sound/ay_psg/ay_psg_gui.cpp
        src/chip/sound/pokey/pokey_gui.cpp
        src/chip/sound/sn76489/sn76489_gui.cpp
        src/chip/sound/ym_fm/ym_fm_gui.cpp
        src/chip/video/spectrum_ula/ferranti_ula_gui.cpp
        src/chip/video/amstrad_gate_array/amstrad_gate_array_gui.cpp
        src/chip/video/bbc_vidproc/bbc_vidproc_gui.cpp
        src/chip/video/mc6847/mc6847_gui.cpp
        src/chip/video/tms9918/tms9918_gui.cpp
        src/chip/input/keyboard_encoder_gui.cpp
        src/systems/nes/cartridge/nes_cartridge_gui.cpp
    )
    
    # Generic GUI sources for multi-system support
    set(EMULATOR_HOST_SOURCES
        src/gui/emulator_host.cpp
        src/gui/session_gui.cpp
        src/gui/system_selection_dialog.cpp
        src/gui/port_icons.cpp
        src/gui/vfs_file_system.cpp
        src/gui/performance_gui.cpp
    )
endif()

# GUI stub sources — provide virtual method implementations that compile
# to empty bodies when CERMU_HAS_GUI is not defined.  Required by any
# non-GUI executable that links CHIP_SOURCES / SYSTEM_SOURCES.
set(GUI_STUB_SOURCES
    src/gui/chip_debug_registry_gui.cpp
    src/chip/cpu/fam65xx/fam65xx_gui.cpp
    src/chip/cpu/z80/z80_gui.cpp
    src/chip/cpu/m680x0/m680x0_gui.cpp
    src/chip/cpu/mc6809/mc6809_gui.cpp
    src/chip/io/mos6526_gui.cpp
    src/chip/io/mos6522_gui.cpp
    src/chip/io/mos6529_gui.cpp
    src/chip/mmu/mos8722_gui.cpp
    src/chip/memory/mos2114_gui.cpp
    src/chip/memory/er2055_gui.cpp
    src/chip/sound/mos6581_gui.cpp
    src/chip/video/vic_ii/vicii_gui.cpp
    src/chip/video/vic/vic_gui.cpp
    src/chip/video/ted/ted7360_gui.cpp
    src/systems/commodore/c64/c64_pla_chip.cpp
    src/chip/video/nes_ppu/nes_ppu_gui.cpp
    src/chip/sound/nes_apu_gui.cpp
    src/chip/input/cd4021_gui.cpp
    src/chip/logic/ls259_gui.cpp
    src/chip/logic/pla_gui.cpp
    src/chip/video/tia/tia_gui.cpp
    src/chip/video/fam6845/crtc_gui.cpp
    src/chip/io/pia6532_gui.cpp
    src/chip/io/pia6820_gui.cpp
    src/chip/io/i8255_gui.cpp
    src/chip/io/z80_pio_gui.cpp
    src/chip/io/z80_ctc_gui.cpp
    src/chip/sound/ay_psg/ay_psg_gui.cpp
    src/chip/sound/pokey/pokey_gui.cpp
    src/chip/sound/sn76489/sn76489_gui.cpp
    src/chip/sound/ym_fm/ym_fm_gui.cpp
    src/chip/video/spectrum_ula/ferranti_ula_gui.cpp
    src/chip/video/amstrad_gate_array/amstrad_gate_array_gui.cpp
    src/chip/video/bbc_vidproc/bbc_vidproc_gui.cpp
    src/chip/video/mc6847/mc6847_gui.cpp
    src/chip/video/tms9918/tms9918_gui.cpp
    src/chip/input/keyboard_encoder_gui.cpp
    src/systems/nes/cartridge/nes_cartridge_gui.cpp
)

# Console executable (no GUI dependencies)
add_executable(cermu_console
    src/main/multi_emu_console.cpp
    ${CORE_SOURCES}
    ${CHIP_SOURCES}
    ${SYSTEM_SOURCES}
    ${CORE_UTIL_SOURCES}
    ${GUI_STUB_SOURCES}
)

target_link_libraries(cermu_console
    fam65xx_decoder
    z80_decoder
    m680x0_decoder
    mc6809_decoder
    ${LIBARCHIVE_LIBRARIES}
    ${SDL2_LIBRARIES}
)

if(NOT WIN32 AND NOT MSVC)
    target_link_libraries(cermu_console m)
endif()

message(STATUS "Console executable cermu_console will be built")

# GUI executable
if(BUILD_GUI)
    add_executable(cermu WIN32
        src/main/multi_emu_main.cpp
        ${CORE_SOURCES}
        ${CHIP_SOURCES}
        ${SYSTEM_SOURCES}
        ${TESTING_SOURCES}
        ${CORE_UTIL_SOURCES}
        ${GUI_SOURCES}
        ${EMULATOR_HOST_SOURCES}
    )
    
    # Add include directories
    target_include_directories(cermu PRIVATE
        ${SDL2_INCLUDE_DIRS}
        ${IMGUI_DIR}
        ${IMGUI_DIR}/backends
    )
    
    # Add ImGuiFileDialog include if available
    if(IMGUIFILEDIALOG_FOUND)
        target_include_directories(cermu PRIVATE ${IMGUIFILEDIALOG_ROOT})
    endif()
    
    # Define CERMU_HAS_GUI — gates all ImGui-dependent code throughout the project.
    # Non-GUI builds (test runners, headless) omit this, so _gui.cpp files
    # compile to empty stubs and #ifdef CERMU_HAS_GUI blocks are skipped.
    target_compile_definitions(cermu PRIVATE CERMU_HAS_GUI)
    
    # Disable warnings
    if(NOT MSVC)
        target_compile_options(cermu PRIVATE -Wno-pedantic)
    endif()
    
    # Link libraries
    set(CERMU_LINK_LIBS
        imgui
        fam65xx_decoder
        z80_decoder
        m680x0_decoder
        mc6809_decoder
        ${LIBARCHIVE_LIBRARIES}
        ${SDL2_LIBRARIES}
        ${OPENGL_LIBRARIES}
        ${CMAKE_DL_LIBS}
    )

    if(SQLite3_FOUND)
        list(APPEND CERMU_LINK_LIBS SQLite::SQLite3)
    elseif(SQLITE3_FOUND)
        list(APPEND CERMU_LINK_LIBS ${SQLITE3_LIBRARIES})
    endif()
    
    if(IMGUIFILEDIALOG_FOUND)
        list(APPEND CERMU_LINK_LIBS ImGuiFileDialog)
    endif()
    
    target_link_libraries(cermu ${CERMU_LINK_LIBS})
    
    # Platform-specific libraries
    if(WIN32)
        target_link_libraries(cermu imm32)
        # Use WINDOWS subsystem (no console window) but keep main() as entry point
        if(MSVC)
            set_target_properties(cermu PROPERTIES LINK_FLAGS "/ENTRY:mainCRTStartup")
        else()
            target_link_options(cermu PRIVATE -mwindows)
        endif()
    endif()
    
    if(NOT WIN32 AND NOT MSVC)
        target_link_libraries(cermu m)
    endif()
    
    # Copy SDL2.dll on Windows
    if(WIN32)
        add_custom_command(TARGET cermu POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${SDL2_DLLS}"
            "$<TARGET_FILE_DIR:cermu>/SDL2.dll"
            COMMENT "Copying SDL2.dll for cermu"
        )
    endif()
    
    message(STATUS "GUI executable cermu will be built")
endif()



# Installation
if(BUILD_GUI)
    install(TARGETS cermu cermu_console
        RUNTIME DESTINATION bin
    )
else()
    install(TARGETS cermu_console
        RUNTIME DESTINATION bin
    )
endif()

# Enable testing
enable_testing()

# Add tests subdirectory for comprehensive MOS6510 testing
add_subdirectory(tests)

# Test runner tool
add_executable(c64_test_runner
    tests/c64_test_runner.cpp
    ${CORE_SOURCES}
    ${CHIP_SOURCES}
    ${SYSTEM_SOURCES}
    ${TESTING_SOURCES}
    ${CORE_UTIL_SOURCES}
    ${GUI_STUB_SOURCES}
)

# Link required libraries for test runner
target_link_libraries(c64_test_runner
    fam65xx_decoder
    z80_decoder
    m680x0_decoder
    mc6809_decoder
    ${LIBARCHIVE_LIBRARIES}
    ${SDL2_LIBRARIES}
)

if(NOT WIN32 AND NOT MSVC)
    target_link_libraries(c64_test_runner m)
endif()

message(STATUS "Test runner c64_test_runner will be built")

# NES test ROM validation runner
add_executable(nes_test_runner
    tests/nes_test_runner.cpp
    ${CORE_SOURCES}
    ${CHIP_SOURCES}
    ${SYSTEM_SOURCES}
    ${CORE_UTIL_SOURCES}
    ${GUI_STUB_SOURCES}
)

target_link_libraries(nes_test_runner
    fam65xx_decoder
    z80_decoder
    m680x0_decoder
    mc6809_decoder
    ${LIBARCHIVE_LIBRARIES}
    ${SDL2_LIBRARIES}
)

if(NOT WIN32 AND NOT MSVC)
    target_link_libraries(nes_test_runner m)
endif()

set_target_properties(nes_test_runner PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

message(STATUS "Test runner nes_test_runner will be built")

# C16/Plus4 test runner (VICE-testprogs TED/Plus4 regression tests)
add_executable(c16_test_runner
    tests/c16_test_runner.cpp
    ${CORE_SOURCES}
    ${CHIP_SOURCES}
    ${SYSTEM_SOURCES}
    ${TESTING_SOURCES}
    ${CORE_UTIL_SOURCES}
    ${GUI_STUB_SOURCES}
)

target_link_libraries(c16_test_runner
    fam65xx_decoder
    z80_decoder
    m680x0_decoder
    mc6809_decoder
    ${LIBARCHIVE_LIBRARIES}
    ${SDL2_LIBRARIES}
)

if(NOT WIN32 AND NOT MSVC)
    target_link_libraries(c16_test_runner m)
endif()

set_target_properties(c16_test_runner PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

message(STATUS "Test runner c16_test_runner will be built")

# VFS smoke test tool
add_executable(vfs_smoke_test
    tests/vfs_smoke_test.cpp
    src/core/os/os_filesystem.cpp
    src/core/os/os_archive.cpp
    src/core/vfs/vfs.cpp
)

target_include_directories(vfs_smoke_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core
    ${LIBARCHIVE_INCLUDE_DIRS}
)

target_link_libraries(vfs_smoke_test
    ${LIBARCHIVE_LIBRARIES}
)

set_target_properties(vfs_smoke_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

message(STATUS "VFS smoke test tool will be built")

# Probe verification tool — batch tests system identification across data folders
add_executable(probe_verify
    tests/probe_verify.cpp
    ${CORE_SOURCES}
    ${CHIP_SOURCES}
    ${SYSTEM_SOURCES}
    ${CORE_UTIL_SOURCES}
    ${GUI_STUB_SOURCES}
)

target_link_libraries(probe_verify
    fam65xx_decoder
    z80_decoder
    m680x0_decoder
    mc6809_decoder
    ${LIBARCHIVE_LIBRARIES}
    ${SDL2_LIBRARIES}
)

if(NOT WIN32 AND NOT MSVC)
    target_link_libraries(probe_verify m)
endif()

set_target_properties(probe_verify PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

message(STATUS "Probe verification tool will be built")

# Link toml++ library (commented out - toml_test target not defined)
# target_link_libraries(toml_test
#     tomlplusplus::tomlplusplus
# )

# Print configuration summary
message(STATUS "")
message(STATUS "Configuration Summary:")
message(STATUS "  Project: ${PROJECT_NAME} ${PROJECT_VERSION}")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  CXX Compiler: ${CMAKE_CXX_COMPILER}")
message(STATUS "  Install prefix: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "")
