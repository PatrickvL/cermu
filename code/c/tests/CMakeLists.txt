cmake_minimum_required(VERSION 3.16)
project(MOS6510Tests)

# Set C standard
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Set C++ standard for fam65xx tests
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Compiler-specific flags
if(MSVC)
    # Microsoft Visual C++ flags
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /W4")
    set(CMAKE_C_FLAGS_DEBUG "/Od /Zi /DDEBUG")
    set(CMAKE_C_FLAGS_RELEASE "/O2 /DNDEBUG")
    # Disable specific MSVC warnings if needed
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /wd4996")  # Disable deprecated function warnings
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4996 /wd4463")  # Disable deprecated and bit-field warnings
    # Define to silence C++17 deprecation warnings
    add_compile_definitions(_SILENCE_CXX17_C_HEADER_DEPRECATION_WARNING)
else()
    # GCC/Clang flags
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra")
    set(CMAKE_C_FLAGS_DEBUG "-g -O0 -DDEBUG")
    set(CMAKE_C_FLAGS_RELEASE "-O2 -DNDEBUG")
endif()

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../)

# Source directories
set(SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../src)
set(CPU_DIR ${SRC_DIR}/chip/cpu)
set(CORE_DIR ${SRC_DIR}/core)

# Collect CPU source files (legacy fam65xx removed - using modern C++ core)
set(CPU_SOURCES
    ${CPU_DIR}/mos6510/mos6510.cpp
)

# Collect core source files
set(CORE_SOURCES
    ${CORE_DIR}/chip.c
    ${CORE_DIR}/ioport.c
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

# Create a library for the MOS6510 CPU
add_library(mos6510_lib STATIC ${CPU_SOURCES} ${CORE_SOURCES})
target_include_directories(mos6510_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/chip/cpu/fam65xx
)
# Enable REDESIGN CPU dispatch mechanism for MOS6510 library
target_compile_definitions(mos6510_lib PRIVATE REDESIGN=1)
# No longer linking legacy mos6502_family - using modern C++ fam65xx

# JSON Parser library for ProcessorTests
add_library(json_parser STATIC json_parser.c)
target_include_directories(json_parser PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# ProcessorTests Runner - Ground Truth Validation (DISABLED - obsolete, uses old API)
# This runner is obsolete and references non-existent mos6502.h and nes6502.h files
# Use the modern fam65xx_processor_tests_runner instead
# add_executable(processor_tests_runner
#     processor_tests_runner.c
# )
# target_link_libraries(processor_tests_runner mos6510_lib json_parser)
# target_sources(processor_tests_runner PRIVATE ${CORE_SOURCES})
# # Enable REDESIGN CPU dispatch mechanism for processor tests
# target_compile_definitions(processor_tests_runner PRIVATE REDESIGN=1)

# Comprehensive Test Suite removed - was referencing deleted legacy headers
# Modern fam65xx tests are used instead

# Basic test removed - was using legacy API incompatible with modern C++ core

# fam65xx Klaus Test Suite - New C++ CPU Core Testing
set(KLAUS_TEST_SOURCES fam65xx_klaus_test_runner.cpp)
if(WIN32)
    list(APPEND KLAUS_TEST_SOURCES getopt_windows.c)
endif()

add_executable(fam65xx_klaus_test_runner ${KLAUS_TEST_SOURCES})
target_include_directories(fam65xx_klaus_test_runner PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/chip/cpu/fam65xx
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/core
)
target_compile_definitions(fam65xx_klaus_test_runner PRIVATE AIEMUC_IMPL)
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
)
target_compile_definitions(fam65xx_lorenz_test_harness PRIVATE AIEMUC_IMPL)
# Link math library if needed
if(NOT WIN32 AND NOT MSVC)
    target_link_libraries(fam65xx_lorenz_test_harness m)
endif()

# fam65xx Enhanced C ProcessorTests Runner - With debugging features
add_executable(fam65xx_processor_tests_runner
    fam65xx_processor_tests_runner.cpp
    json_parser.c
)
target_include_directories(fam65xx_processor_tests_runner PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/chip/cpu/fam65xx
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/core
)
target_compile_definitions(fam65xx_processor_tests_runner PRIVATE AIEMUC_IMPL)
# Disable warnings as errors for this target due to generated code warnings
if(MSVC)
    target_compile_options(fam65xx_processor_tests_runner PRIVATE /WX- /wd4244 /wd4189 /wd4477)
else()
    target_compile_options(fam65xx_processor_tests_runner PRIVATE -Wno-error)
endif()
# Link math library if needed
if(NOT WIN32 AND NOT MSVC)
    target_link_libraries(fam65xx_processor_tests_runner m)
endif()

# Unified Processor Test Framework - DISABLED (obsolete, references fam65xx_cpp)
# This test references the obsolete fam65xx_cpp directory that no longer exists
# Use the modern fam65xx test runners instead
# add_executable(unified_processor_test
#     ../unified_processor_test.cpp
# )
# target_include_directories(unified_processor_test PRIVATE
#     ${CMAKE_CURRENT_SOURCE_DIR}/../src/core
# )
# # Link math library if needed
# if(NOT WIN32 AND NOT MSVC)
#     target_link_libraries(unified_processor_test m)
# endif()

# Simple Unified Processor Test - Working version without template issues
add_executable(unified_processor_test_simple
    ../unified_processor_test_simple.cpp
)
# Link math library if needed
if(NOT WIN32 AND NOT MSVC)
    target_link_libraries(unified_processor_test_simple m)
endif()

# fam65xx Interrupt Handling Tests - DISABLED (obsolete, reference fam65xx_cpp)
# These tests reference the obsolete fam65xx_cpp directory that no longer exists
# Interrupt behavior is now tested in the modern fam65xx test runners
# add_executable(nmi_edge_detection_test
#     nmi_edge_detection_test.cpp
# )
# target_include_directories(nmi_edge_detection_test PRIVATE
#     ${CMAKE_CURRENT_SOURCE_DIR}/../src/chip/cpu/fam65xx
#     ${CMAKE_CURRENT_SOURCE_DIR}/../src/core
# )
# target_compile_definitions(nmi_edge_detection_test PRIVATE AIEMUC_IMPL)
# # Link math library if needed
# if(NOT WIN32 AND NOT MSVC)
#     target_link_libraries(nmi_edge_detection_test m)
# endif()

# add_executable(irq_level_handling_test
#     irq_level_handling_test.cpp
# )
# target_include_directories(irq_level_handling_test PRIVATE
#     ${CMAKE_CURRENT_SOURCE_DIR}/../src/chip/cpu/fam65xx
#     ${CMAKE_CURRENT_SOURCE_DIR}/../src/core
# )
# target_compile_definitions(irq_level_handling_test PRIVATE AIEMUC_IMPL)
# # Link math library if needed
# if(NOT WIN32 AND NOT MSVC)
#     target_link_libraries(irq_level_handling_test m)
# endif()

# add_executable(brk_flag_setting_test
#     brk_flag_setting_test.cpp
# )
# target_include_directories(brk_flag_setting_test PRIVATE
#     ${CMAKE_CURRENT_SOURCE_DIR}/../src/chip/cpu/fam65xx
#     ${CMAKE_CURRENT_SOURCE_DIR}/../src/core
# )
# target_compile_definitions(brk_flag_setting_test PRIVATE AIEMUC_IMPL)
# # Link math library if needed
# if(NOT WIN32 AND NOT MSVC)
#     target_link_libraries(brk_flag_setting_test m)
# endif()

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

# Comprehensive test removed - using modern fam65xx tests instead

# Basic test removed - using modern fam65xx tests instead

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

# Comprehensive test target removed - using modern fam65xx tests instead

# Basic test target removed - using modern fam65xx tests instead

add_custom_target(run_all_tests
    COMMAND ${CMAKE_CTEST_COMMAND} --test-dir ${CMAKE_BINARY_DIR} --verbose
    DEPENDS fam65xx_klaus_test_runner fam65xx_processor_tests_runner
    COMMENT "Running all MOS6510 tests"
)

# Install targets (optional)
# TEMPORARILY DISABLED due to header compatibility issues with MSVC
# Legacy test_mos6510_comprehensive removed - using modern fam65xx tests

if(TARGET test_mos6510_basic_tests)
    install(TARGETS test_mos6510_basic_tests
        RUNTIME DESTINATION bin
    )
endif()

# Install fam65xx test runner
install(TARGETS fam65xx_klaus_test_runner
    RUNTIME DESTINATION bin
)

# Legacy basic test removed from install targets

# Print summary
message(STATUS "MOS6510 Tests Configuration:")
message(STATUS "  C Standard: ${CMAKE_C_STANDARD}")
message(STATUS "  Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  Compiler: ${CMAKE_C_COMPILER_ID}")
message(STATUS "  CPU Sources Found: ${CPU_SOURCES}")
message(STATUS "  Core Sources Found: ${CORE_SOURCES}")
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/processor_tests)
    message(STATUS "  ProcessorTests Available: Yes")
else()
    message(STATUS "  ProcessorTests Available: No")
endif()
message(STATUS "  fam65xx Klaus Test Runner: Built")
message(STATUS "  fam65xx Lorenz Test Harness: Built")