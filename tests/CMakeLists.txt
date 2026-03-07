cmake_minimum_required(VERSION 3.16)
project(MOS6510Tests)

# Set C++ standard for all tests (now fully C++)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Compiler-specific flags
if(MSVC)
    # Microsoft Visual C++ flags
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4")
    set(CMAKE_CXX_FLAGS_DEBUG "/Od /Zi /DDEBUG")
    set(CMAKE_CXX_FLAGS_RELEASE "/O2 /DNDEBUG")
    # Disable specific MSVC warnings if needed
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4996 /wd4463")  # Disable deprecated and bit-field warnings
    # Define to silence C++17 deprecation warnings
    add_compile_definitions(_SILENCE_CXX17_C_HEADER_DEPRECATION_WARNING)
else()
    # GCC/Clang flags
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
    set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -DDEBUG")
    set(CMAKE_CXX_FLAGS_RELEASE "-O2 -DNDEBUG")
endif()

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../)

# Source directories
set(SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../src)
set(CPU_DIR ${SRC_DIR}/chip/cpu)
set(CORE_DIR ${SRC_DIR}/core)

# Collect CPU source files (fam65xx is header-only; only decoder needs compiling)
set(CPU_SOURCES
    ${CPU_DIR}/fam65xx/fam65xx_decoder.cpp
)

# Collect core source files
set(CORE_SOURCES
    ${CORE_DIR}/chip.cpp
    ${CORE_DIR}/ioport.cpp
)

# Filter out any missing files (in case some don't exist yet)
foreach(source ${CPU_SOURCES})
    if(NOT EXISTS ${source})
        list(REMOVE_ITEM CPU_SOURCES ${source})
        message(WARNING "Source file not found: ${source}")
    endif()
endforeach()

foreach(source ${CORE_SOURCES})
    if(NOT EXISTS ${source})
        list(REMOVE_ITEM CORE_SOURCES ${source})
        message(WARNING "Source file not found: ${source}")
    endif()
endforeach()

# GUI stub sources needed by chip classes that override ChipBase virtuals.
# These *_gui.cpp files compile to empty stubs when CERMU_HAS_GUI is not defined.
set(MOS6510_GUI_STUBS
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/chip/cpu/fam65xx/fam65xx_gui.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/chip/sound/nes_apu_gui.cpp
)

# Create a library for the MOS6510 CPU
add_library(mos6510_lib STATIC ${CPU_SOURCES} ${CORE_SOURCES} ${MOS6510_GUI_STUBS})
target_include_directories(mos6510_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/chip/cpu/fam65xx
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
)
# Enable REDESIGN CPU dispatch mechanism for MOS6510 library
target_compile_definitions(mos6510_lib PRIVATE REDESIGN=1)

# JSON Parser library for ProcessorTests
add_library(json_parser STATIC json_parser.cpp)
target_include_directories(json_parser PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# fam65xx Klaus Test Suite - New C++ CPU Core Testing
add_executable(fam65xx_klaus_test_runner fam65xx_klaus_test_runner.cpp)
target_include_directories(fam65xx_klaus_test_runner PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/chip/cpu/fam65xx
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/core
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
)
target_compile_definitions(fam65xx_klaus_test_runner PRIVATE CERMU_IMPL)
# Link against MOS6510 library for implementation symbols
target_link_libraries(fam65xx_klaus_test_runner mos6510_lib)
# Link math library if needed
if(NOT WIN32 AND NOT MSVC)
    target_link_libraries(fam65xx_klaus_test_runner m)
endif()

# fam65xx Wolfgang Lorenz Test Suite - Cycle-accurate timing validation
add_executable(fam65xx_lorenz_test_harness
    fam65xx_lorenz_test_harness.cpp
)
target_include_directories(fam65xx_lorenz_test_harness PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/chip/cpu/fam65xx
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/core
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
)
target_compile_definitions(fam65xx_lorenz_test_harness PRIVATE CERMU_IMPL)
# Link against MOS6510 library for implementation symbols
target_link_libraries(fam65xx_lorenz_test_harness mos6510_lib)
# Link math library if needed
if(NOT WIN32 AND NOT MSVC)
    target_link_libraries(fam65xx_lorenz_test_harness m)
endif()

# fam65xx Enhanced C ProcessorTests Runner - With debugging features (using new fam65xx)
add_executable(fam65xx_processor_tests_runner
    fam65xx_processor_tests_runner.cpp
    ${CPU_DIR}/fam65xx/fam65xx_decoder.cpp
    ${MOS6510_GUI_STUBS}
)
target_include_directories(fam65xx_processor_tests_runner PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/chip/cpu/fam65xx
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/core
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_compile_definitions(fam65xx_processor_tests_runner PRIVATE CERMU_IMPL PROCESSOR_TESTS)
# Set C++20 standard for fam65xx template support
set_property(TARGET fam65xx_processor_tests_runner PROPERTY CXX_STANDARD 20)
set_property(TARGET fam65xx_processor_tests_runner PROPERTY CXX_STANDARD_REQUIRED ON)
# Enable full warning reporting to identify all compiler messages
if(MSVC)
    target_compile_options(fam65xx_processor_tests_runner PRIVATE /W4)
else()
    target_compile_options(fam65xx_processor_tests_runner PRIVATE -Wall -Wextra)
endif()
# Link against json_parser library and math library if needed
target_link_libraries(fam65xx_processor_tests_runner json_parser)
if(NOT WIN32 AND NOT MSVC)
    target_link_libraries(fam65xx_processor_tests_runner m)
endif()

# Enable testing
enable_testing()

# ProcessorTests - Primary validation (DISABLED - obsolete runner)
# Use fam65xx_processor_tests_runner instead for ProcessorTests validation
# if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/processor_tests)
#     add_test(NAME processor_tests
#              COMMAND processor_tests_runner processor_tests/6502/v1/
#              WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
#     set_tests_properties(processor_tests PROPERTIES
#         TIMEOUT 300
#         PASS_REGULAR_EXPRESSION "ALL TESTS PASSED"
#     )
# endif()

# fam65xx Klaus test
add_test(NAME fam65xx_klaus_test COMMAND fam65xx_klaus_test_runner --functional)
set_tests_properties(fam65xx_klaus_test PROPERTIES
    TIMEOUT 300
    PASS_REGULAR_EXPRESSION "ALL TESTS PASSED"
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/..
)

# Custom targets for convenience (updated to use modern fam65xx runner)
add_custom_target(run_processor_tests
    COMMAND ${CMAKE_CTEST_COMMAND} --test-dir ${CMAKE_BINARY_DIR} -R fam65xx --verbose
    DEPENDS fam65xx_processor_tests_runner
    COMMENT "Running ProcessorTests ground truth validation with modern fam65xx runner"
)

add_custom_target(run_all_tests
    COMMAND ${CMAKE_CTEST_COMMAND} --test-dir ${CMAKE_BINARY_DIR} --verbose
    DEPENDS fam65xx_klaus_test_runner fam65xx_processor_tests_runner
    COMMENT "Running all MOS6510 tests"
)

# =============================================================================
# SID MOS6581/8580 Digital Verification Test Runner
# =============================================================================

set(SOUND_DIR ${SRC_DIR}/chip/sound)
set(TESTING_DIR ${SRC_DIR}/testing)

add_executable(sid_test_runner
    sid_test_runner.cpp
    ${TESTING_DIR}/sid_test_harness.cpp
    ${SOUND_DIR}/mos6581.cpp
    ${SOUND_DIR}/mos6581_gui.cpp
    ${SOUND_DIR}/sid_waveform_tables.cpp
    ${CORE_DIR}/chip.cpp
    ${CORE_DIR}/chip_layout.cpp
    ${CORE_DIR}/pin_labels.cpp
    ${CORE_DIR}/ioport.cpp
)
target_include_directories(sid_test_runner PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/core
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/chip/sound
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/testing
)
target_compile_definitions(sid_test_runner PRIVATE CERMU_IMPL)
set_property(TARGET sid_test_runner PROPERTY CXX_STANDARD 20)
set_property(TARGET sid_test_runner PROPERTY CXX_STANDARD_REQUIRED ON)
if(NOT WIN32 AND NOT MSVC)
    target_link_libraries(sid_test_runner m)
endif()

# SID test CTest integration
add_test(NAME sid_digital_verification
    COMMAND sid_test_runner
)
set_tests_properties(sid_digital_verification PROPERTIES
    TIMEOUT 60
)

# Custom target for running SID tests
add_custom_target(run_sid_tests
    COMMAND ${CMAKE_CTEST_COMMAND} --test-dir ${CMAKE_BINARY_DIR} -R sid --verbose
    DEPENDS sid_test_runner
    COMMENT "Running SID digital verification tests"
)

# Update run_all_tests to include SID + APU tests
add_custom_target(run_all_tests_full
    COMMAND ${CMAKE_CTEST_COMMAND} --test-dir ${CMAKE_BINARY_DIR} --verbose
    DEPENDS fam65xx_klaus_test_runner fam65xx_processor_tests_runner sid_test_runner apu_test_runner a2600_test_runner
    COMMENT "Running all tests (CPU + SID + APU + A2600)"
)

# =============================================================================
# NES APU Digital Verification Tests
# =============================================================================

set(NES_DIR ${SRC_DIR}/systems/nes)

add_executable(apu_test_runner
    apu_test_runner.cpp
    ${TESTING_DIR}/apu_test_harness.cpp
    ${SRC_DIR}/chip/sound/nes_apu_gui.cpp
    ${CORE_DIR}/chip.cpp
    ${CORE_DIR}/chip_layout.cpp
    ${CORE_DIR}/pin_labels.cpp
    ${CORE_DIR}/ioport.cpp
)
target_include_directories(apu_test_runner PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/core
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/chip/cpu/fam65xx
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/testing
)
target_compile_definitions(apu_test_runner PRIVATE CERMU_IMPL)
set_property(TARGET apu_test_runner PROPERTY CXX_STANDARD 20)
set_property(TARGET apu_test_runner PROPERTY CXX_STANDARD_REQUIRED ON)
if(NOT WIN32 AND NOT MSVC)
    target_link_libraries(apu_test_runner m)
endif()

# APU test CTest integration
add_test(NAME apu_digital_verification
    COMMAND apu_test_runner
)
set_tests_properties(apu_digital_verification PROPERTIES
    TIMEOUT 60
)

# Custom target for running APU tests
add_custom_target(run_apu_tests
    COMMAND ${CMAKE_CTEST_COMMAND} --test-dir ${CMAKE_BINARY_DIR} -R apu --verbose
    DEPENDS apu_test_runner
    COMMENT "Running NES APU digital verification tests"
)

# =============================================================================
# Atari 2600 Hardware Verification Tests
# =============================================================================

set(A2600_DIR ${SRC_DIR}/systems/atari2600)
set(VIDEO_DIR ${SRC_DIR}/chip/video)
set(IO_DIR ${SRC_DIR}/chip/io)

add_executable(a2600_test_runner
    a2600_test_runner.cpp
    ${TESTING_DIR}/a2600_test_harness.cpp
    ${VIDEO_DIR}/tia.cpp
    ${VIDEO_DIR}/tia_gui.cpp
    ${IO_DIR}/pia6532.cpp
    ${IO_DIR}/pia6532_gui.cpp
    ${CORE_DIR}/chip.cpp
    ${CORE_DIR}/chip_layout.cpp
    ${CORE_DIR}/chip_debug_registry.cpp
    ${CORE_DIR}/pin_labels.cpp
    ${CORE_DIR}/ioport.cpp
    ${SRC_DIR}/gui/chip_debug_registry_gui.cpp
)
target_include_directories(a2600_test_runner PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/core
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/chip/video
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/chip/io
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/systems/atari2600
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/systems/atari2600/mappers
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/testing
)
target_compile_definitions(a2600_test_runner PRIVATE CERMU_IMPL)
set_property(TARGET a2600_test_runner PROPERTY CXX_STANDARD 20)
set_property(TARGET a2600_test_runner PROPERTY CXX_STANDARD_REQUIRED ON)
if(NOT WIN32 AND NOT MSVC)
    target_link_libraries(a2600_test_runner m)
endif()

# A2600 test CTest integration
add_test(NAME a2600_hardware_verification
    COMMAND a2600_test_runner
)
set_tests_properties(a2600_hardware_verification PROPERTIES
    TIMEOUT 60
)

# Custom target for running A2600 tests
add_custom_target(run_a2600_tests
    COMMAND ${CMAKE_CTEST_COMMAND} --test-dir ${CMAKE_BINARY_DIR} -R a2600 --verbose
    DEPENDS a2600_test_runner
    COMMENT "Running Atari 2600 hardware verification tests"
)

# Install targets (optional)

# Install fam65xx test runner
install(TARGETS fam65xx_klaus_test_runner
    RUNTIME DESTINATION bin
)

# Install SID test runner
install(TARGETS sid_test_runner
    RUNTIME DESTINATION bin
)

# Print summary
message(STATUS "MOS6510 Tests Configuration:")
message(STATUS "  CXX Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  Compiler: ${CMAKE_CXX_COMPILER_ID}")
message(STATUS "  CPU Sources Found: ${CPU_SOURCES}")
message(STATUS "  Core Sources Found: ${CORE_SOURCES}")
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/processor_tests)
    message(STATUS "  ProcessorTests Available: Yes")
else()
    message(STATUS "  ProcessorTests Available: No")
endif()
message(STATUS "  fam65xx Klaus Test Runner: Built")
message(STATUS "  fam65xx Lorenz Test Harness: Built")
message(STATUS "  SID Test Runner: Built")
message(STATUS "  APU Test Runner: Built")
message(STATUS "  A2600 Test Runner: Built")

# =============================================================================
# SID Reference Comparison Runner (cermu vs reSID)
# =============================================================================
# Builds reSID (Dag Lem) from the VICE source tree as a static library,
# then links it with our SID test harness for cycle-exact comparison.
# =============================================================================

# Locate reSID sources in the VICE workspace folder
set(RESID_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../VICE/vice/src/resid")
set(RESID_STANDALONE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../external/resid")

if(EXISTS "${RESID_SRC_DIR}/sid.cc")
    # Build reSID as a static library with our standalone siddefs.h
    # NOTE: filter.cc is excluded because filter8580new.cc provides the
    # same Filter class when NEW_8580_FILTER=1 (the improved model).
    add_library(resid STATIC
        ${RESID_SRC_DIR}/dac.cc
        ${RESID_SRC_DIR}/envelope.cc
        ${RESID_SRC_DIR}/extfilt.cc
        ${RESID_SRC_DIR}/filter8580new.cc
        ${RESID_SRC_DIR}/pot.cc
        ${RESID_SRC_DIR}/sid.cc
        ${RESID_SRC_DIR}/version.cc
        ${RESID_SRC_DIR}/voice.cc
        ${RESID_SRC_DIR}/wave.cc
    )
    target_include_directories(resid PRIVATE
        ${RESID_STANDALONE_DIR}   # Our standalone siddefs.h (found before VICE's)
        ${RESID_SRC_DIR}
    )
    target_include_directories(resid SYSTEM PUBLIC
        ${RESID_SRC_DIR}          # Public: so sid_comparison_runner can #include "sid.h"
    )
    target_compile_definitions(resid PRIVATE
        RESID_INLINING=1
        VERSION="1.0-cermu-standalone"
    )
    set_property(TARGET resid PROPERTY CXX_STANDARD 20)
    # Suppress warnings in third-party code
    if(NOT MSVC)
        target_compile_options(resid PRIVATE -w)
    else()
        target_compile_options(resid PRIVATE /w)
    endif()

    # Build the comparison runner
    add_executable(sid_comparison_runner
        sid_comparison_runner.cpp
        ${TESTING_DIR}/sid_test_harness.cpp
        ${SOUND_DIR}/mos6581.cpp
        ${SOUND_DIR}/mos6581_gui.cpp
        ${SOUND_DIR}/sid_waveform_tables.cpp
        ${CORE_DIR}/chip.cpp
        ${CORE_DIR}/chip_layout.cpp
        ${CORE_DIR}/pin_labels.cpp
        ${CORE_DIR}/ioport.cpp
    )
    target_include_directories(sid_comparison_runner PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../src
        ${CMAKE_CURRENT_SOURCE_DIR}/../src/core
        ${CMAKE_CURRENT_SOURCE_DIR}/../src/chip/sound
        ${CMAKE_CURRENT_SOURCE_DIR}/../src/testing
        ${RESID_STANDALONE_DIR}
    )
    target_compile_definitions(sid_comparison_runner PRIVATE CERMU_IMPL)
    set_property(TARGET sid_comparison_runner PROPERTY CXX_STANDARD 20)
    set_property(TARGET sid_comparison_runner PROPERTY CXX_STANDARD_REQUIRED ON)
    target_link_libraries(sid_comparison_runner resid)
    if(NOT WIN32 AND NOT MSVC)
        target_link_libraries(sid_comparison_runner m)
    endif()

    # CTest integration
    add_test(NAME sid_reference_comparison
        COMMAND sid_comparison_runner
    )
    set_tests_properties(sid_reference_comparison PROPERTIES TIMEOUT 120)

    # Custom target for running comparison
    add_custom_target(run_sid_comparison
        COMMAND sid_comparison_runner
        DEPENDS sid_comparison_runner
        COMMENT "Running SID reference comparison (cermu vs reSID)"
    )

    # Custom target for comparing against all VICE hardware traces
    set(VICE_TESTPROGS_SID "${CMAKE_CURRENT_SOURCE_DIR}/../../VICE-testprogs/SID/resid-test")
    if(EXISTS "${VICE_TESTPROGS_SID}")
        add_custom_target(run_sid_hw_comparison
            COMMAND sid_comparison_runner --all-dat "${VICE_TESTPROGS_SID}"
            DEPENDS sid_comparison_runner
            COMMENT "Comparing SID output against hardware-captured .dat traces"
        )
        message(STATUS "  SID HW Comparison: Available (VICE-testprogs found)")
    endif()

    # Install
    install(TARGETS sid_comparison_runner RUNTIME DESTINATION bin)

    message(STATUS "  SID Comparison Runner: Built (reSID found at ${RESID_SRC_DIR})")
else()
    message(STATUS "  SID Comparison Runner: SKIPPED (reSID not found at ${RESID_SRC_DIR})")
    message(STATUS "    To enable: clone VICE into ../VICE/vice/src/resid/")
endif()

# =============================================================================
# NES APU Reference Comparison Runner (cermu vs Blargg's Nes_Snd_Emu)
# =============================================================================
# Builds Blargg's Nes_Snd_Emu (from game-music-emu) as a static library,
# then links it with our APU test harness for waveform comparison.
# =============================================================================

set(NES_SND_EMU_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../external/nes_snd_emu")

if(EXISTS "${NES_SND_EMU_DIR}/Nes_Apu.cpp")
    # Build Nes_Snd_Emu as a static library
    add_library(nes_snd_emu STATIC
        ${NES_SND_EMU_DIR}/Blip_Buffer.cpp
        ${NES_SND_EMU_DIR}/Nes_Apu.cpp
        ${NES_SND_EMU_DIR}/Nes_Oscs.cpp
    )
    target_include_directories(nes_snd_emu PUBLIC
        ${NES_SND_EMU_DIR}
    )
    set_property(TARGET nes_snd_emu PROPERTY CXX_STANDARD 20)
    # Suppress warnings in third-party code
    if(NOT MSVC)
        target_compile_options(nes_snd_emu PRIVATE -w)
    else()
        target_compile_options(nes_snd_emu PRIVATE /w)
    endif()

    # Build the comparison runner
    add_executable(apu_comparison_runner
        apu_comparison_runner.cpp
        ${TESTING_DIR}/apu_test_harness.cpp
        ${SRC_DIR}/chip/sound/nes_apu_gui.cpp
        ${CORE_DIR}/chip.cpp
        ${CORE_DIR}/chip_layout.cpp
        ${CORE_DIR}/pin_labels.cpp
        ${CORE_DIR}/ioport.cpp
    )
    target_include_directories(apu_comparison_runner PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../src
        ${CMAKE_CURRENT_SOURCE_DIR}/../src/core
        ${CMAKE_CURRENT_SOURCE_DIR}/../src/chip/cpu/fam65xx
        ${CMAKE_CURRENT_SOURCE_DIR}/../src/testing
        ${NES_SND_EMU_DIR}
    )
    target_compile_definitions(apu_comparison_runner PRIVATE CERMU_IMPL)
    set_property(TARGET apu_comparison_runner PROPERTY CXX_STANDARD 20)
    set_property(TARGET apu_comparison_runner PROPERTY CXX_STANDARD_REQUIRED ON)
    target_link_libraries(apu_comparison_runner nes_snd_emu)
    if(NOT WIN32 AND NOT MSVC)
        target_link_libraries(apu_comparison_runner m)
    endif()

    # CTest integration
    add_test(NAME apu_reference_comparison
        COMMAND apu_comparison_runner
    )
    set_tests_properties(apu_reference_comparison PROPERTIES TIMEOUT 120)

    # Custom target for running comparison
    add_custom_target(run_apu_comparison
        COMMAND apu_comparison_runner
        DEPENDS apu_comparison_runner
        COMMENT "Running NES APU reference comparison (cermu vs Blargg)"
    )

    # Install
    install(TARGETS apu_comparison_runner RUNTIME DESTINATION bin)

    message(STATUS "  APU Comparison Runner: Built (Nes_Snd_Emu found)")
else()
    message(STATUS "  APU Comparison Runner: SKIPPED (Nes_Snd_Emu not found)")
    message(STATUS "    To enable: place Blargg's nes_snd_emu in external/nes_snd_emu/")
endif()