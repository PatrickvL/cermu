cmake_minimum_required(VERSION 3.16)
project(MOS6510Tests)

# Set C standard
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Set C++ standard for fam65xx_cpp tests
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Compiler-specific flags
if(MSVC)
    # Microsoft Visual C++ flags
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /W4")
    set(CMAKE_C_FLAGS_DEBUG "/Od /Zi /DDEBUG")
    set(CMAKE_C_FLAGS_RELEASE "/O2 /DNDEBUG")
    # Disable specific MSVC warnings if needed
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /wd4996")  # Disable deprecated function warnings
else()
    # GCC/Clang flags
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra")
    set(CMAKE_C_FLAGS_DEBUG "-g -O0 -DDEBUG")
    set(CMAKE_C_FLAGS_RELEASE "-O2 -DNDEBUG")
endif()

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../)

# Add the CPU subdirectory to make fam65xx library available
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../src/chip/cpu cpu_build)

# Source directories
set(SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../src)
set(CPU_DIR ${SRC_DIR}/chip/cpu)
set(CORE_DIR ${SRC_DIR}/core)

# Collect CPU source files
set(CPU_SOURCES
    ${CPU_DIR}/mos6510/mos6510.c
    ${CPU_DIR}/fam65xx/fam65xx_core.c
    ${CPU_DIR}/fam65xx/fam65xx_opcodes.c
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
target_include_directories(mos6510_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../)
# Enable REDESIGN CPU dispatch mechanism for MOS6510 library
target_compile_definitions(mos6510_lib PRIVATE REDESIGN=1)
# Link against the mos6502_family library which contains shared fam65xx functions
target_link_libraries(mos6510_lib mos6502_family)

# JSON Parser library for ProcessorTests
add_library(json_parser STATIC json_parser.c)
target_include_directories(json_parser PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# ProcessorTests Runner - Ground Truth Validation
# TEMPORARILY DISABLED due to header compatibility issues with MSVC
# add_executable(processor_tests_runner
#     processor_tests_runner.c
# )
# target_link_libraries(processor_tests_runner mos6502 nes6502 mos6510 json_parser)
# target_sources(processor_tests_runner PRIVATE ${CORE_SOURCES})
# # Enable REDESIGN CPU dispatch mechanism for processor tests
# target_compile_definitions(processor_tests_runner PRIVATE REDESIGN=1)

# Comprehensive Test Suite (fallback/custom tests)
add_executable(test_mos6510_comprehensive
    test_mos6510_comprehensive.c
)
target_link_libraries(test_mos6510_comprehensive mos6510_lib)
# Enable REDESIGN CPU dispatch mechanism for comprehensive tests
target_compile_definitions(test_mos6510_comprehensive PRIVATE REDESIGN=1)

# Basic test (if exists) - renamed to avoid conflict with main project
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/../test_mos6510_basic.c)
    add_executable(test_mos6510_basic_tests
        ${CMAKE_CURRENT_SOURCE_DIR}/../test_mos6510_basic.c
    )
    target_link_libraries(test_mos6510_basic_tests mos6510_lib)
    # Enable REDESIGN CPU dispatch mechanism for basic tests
    target_compile_definitions(test_mos6510_basic_tests PRIVATE REDESIGN=1)
endif()

# fam65xx_cpp Klaus Test Suite - New C++ CPU Core Testing
add_executable(fam65xx_cpp_klaus_test_runner
    fam65xx_cpp_klaus_test_runner.cpp
    fam65xx_cpp_test_harness.cpp
)
target_include_directories(fam65xx_cpp_klaus_test_runner PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/chip/cpu/fam65xx_cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/core
)
# Link math library if needed
if(NOT WIN32 AND NOT MSVC)
    target_link_libraries(fam65xx_cpp_klaus_test_runner m)
endif()

# fam65xx_cpp Wolfgang Lorenz Test Suite - Cycle-accurate timing validation
add_executable(fam65xx_cpp_lorenz_test_runner
    fam65xx_cpp_lorenz_test_runner.cpp
    fam65xx_cpp_lorenz_test_harness.cpp
)
target_include_directories(fam65xx_cpp_lorenz_test_runner PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/chip/cpu/fam65xx_cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/core
)
# Link math library if needed
if(NOT WIN32 AND NOT MSVC)
    target_link_libraries(fam65xx_cpp_lorenz_test_runner m)
endif()

# fam65xx_cpp 65C02 CMOS Test Suite - CMOS feature validation
add_executable(fam65xx_cpp_65c02_test_runner
    fam65xx_cpp_65c02_test_runner.cpp
    fam65xx_cpp_65c02_test_harness.cpp
)
target_include_directories(fam65xx_cpp_65c02_test_runner PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/chip/cpu/fam65xx_cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/core
)
# Link math library if needed
if(NOT WIN32 AND NOT MSVC)
    target_link_libraries(fam65xx_cpp_65c02_test_runner m)
endif()

# fam65xx_cpp Interrupt Handling Tests - Advanced interrupt behavior validation
add_executable(nmi_edge_detection_test
    nmi_edge_detection_test.cpp
)
target_include_directories(nmi_edge_detection_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/chip/cpu/fam65xx_cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/core
)
# Link math library if needed
if(NOT WIN32 AND NOT MSVC)
    target_link_libraries(nmi_edge_detection_test m)
endif()

add_executable(irq_level_handling_test
    irq_level_handling_test.cpp
)
target_include_directories(irq_level_handling_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/chip/cpu/fam65xx_cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/core
)
# Link math library if needed
if(NOT WIN32 AND NOT MSVC)
    target_link_libraries(irq_level_handling_test m)
endif()

add_executable(brk_flag_setting_test
    brk_flag_setting_test.cpp
)
target_include_directories(brk_flag_setting_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/chip/cpu/fam65xx_cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/core
)
# Link math library if needed
if(NOT WIN32 AND NOT MSVC)
    target_link_libraries(brk_flag_setting_test m)
endif()

# Enable testing
enable_testing()

# ProcessorTests - Primary validation (if ProcessorTests directory exists)
# TEMPORARILY DISABLED due to header compatibility issues with MSVC
# if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/processor_tests)
#     add_test(NAME processor_tests
#              COMMAND processor_tests_runner processor_tests/6502/v1/
#              WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
#     set_tests_properties(processor_tests PROPERTIES
#         TIMEOUT 300
#         PASS_REGULAR_EXPRESSION "ALL TESTS PASSED"
#     )
# endif()

# Comprehensive test - Secondary validation
add_test(NAME comprehensive_test COMMAND test_mos6510_comprehensive)
set_tests_properties(comprehensive_test PROPERTIES
    TIMEOUT 60
    PASS_REGULAR_EXPRESSION "ALL TESTS PASSED"
)

# Basic test (if available)
if(TARGET test_mos6510_basic_tests)
    add_test(NAME basic_test COMMAND test_mos6510_basic_tests)
    set_tests_properties(basic_test PROPERTIES
        TIMEOUT 30
        PASS_REGULAR_EXPRESSION "ALL TESTS PASSED"
    )
endif()

# fam65xx_cpp Klaus test
add_test(NAME fam65xx_cpp_klaus_test COMMAND fam65xx_cpp_klaus_test_runner --functional)
set_tests_properties(fam65xx_cpp_klaus_test PROPERTIES
    TIMEOUT 300
    PASS_REGULAR_EXPRESSION "ALL TESTS PASSED"
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/..
)

# Custom targets for convenience
# TEMPORARILY DISABLED due to header compatibility issues with MSVC
# add_custom_target(run_processor_tests
#     COMMAND ${CMAKE_CTEST_COMMAND} --test-dir ${CMAKE_BINARY_DIR} -R processor_tests --verbose
#     DEPENDS processor_tests_runner
#     COMMENT "Running ProcessorTests ground truth validation"
# )

add_custom_target(run_comprehensive_test
    COMMAND ${CMAKE_CTEST_COMMAND} --test-dir ${CMAKE_BINARY_DIR} -R comprehensive_test --verbose
    DEPENDS test_mos6510_comprehensive
    COMMENT "Running comprehensive MOS6510 tests"
)

if(TARGET test_mos6510_basic_tests)
    add_custom_target(run_basic_test
        COMMAND ${CMAKE_CTEST_COMMAND} --test-dir ${CMAKE_BINARY_DIR} -R basic_test --verbose
        DEPENDS test_mos6510_basic_tests
        COMMENT "Running basic MOS6510 tests"
    )
endif()

add_custom_target(run_all_tests
    COMMAND ${CMAKE_CTEST_COMMAND} --test-dir ${CMAKE_BINARY_DIR} --verbose
    DEPENDS test_mos6510_comprehensive
    COMMENT "Running all MOS6510 tests"
)

# Install targets (optional)
# TEMPORARILY DISABLED due to header compatibility issues with MSVC
# install(TARGETS processor_tests_runner test_mos6510_comprehensive
install(TARGETS test_mos6510_comprehensive
    RUNTIME DESTINATION bin
)

if(TARGET test_mos6510_basic_tests)
    install(TARGETS test_mos6510_basic_tests
        RUNTIME DESTINATION bin
    )
endif()

# Install fam65xx_cpp test runner
install(TARGETS fam65xx_cpp_klaus_test_runner
    RUNTIME DESTINATION bin
)

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
message(STATUS "  fam65xx_cpp Klaus Test Runner: Built")