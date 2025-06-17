cmake_minimum_required(VERSION 3.16)
project(MOS6510Tests)

# Set C standard
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

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

# Source directories
set(SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../src)
set(CPU_DIR ${SRC_DIR}/chip/cpu/mos6510)
set(CORE_DIR ${SRC_DIR}/core)

# Collect CPU source files
set(CPU_SOURCES
    ${CPU_DIR}/mos6510.c
    ${CPU_DIR}/mos6510_illegal.c
)

# Collect core source files
set(CORE_SOURCES
    ${CORE_DIR}/chip.c
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
target_link_libraries(mos6510_lib mos6502_family)

# JSON Parser library for ProcessorTests
add_library(json_parser STATIC json_parser.c)
target_include_directories(json_parser PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# ProcessorTests Runner - Ground Truth Validation
add_executable(processor_tests_runner
    processor_tests_runner.c
)
target_link_libraries(processor_tests_runner mos6510_lib json_parser mos6502_family)

# Comprehensive Test Suite (fallback/custom tests)
add_executable(test_mos6510_comprehensive
    test_mos6510_comprehensive.c
)
target_link_libraries(test_mos6510_comprehensive mos6510_lib mos6502_family)

# Basic test (if exists) - renamed to avoid conflict with main project
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/../test_mos6510_basic.c)
    add_executable(test_mos6510_basic_tests
        ${CMAKE_CURRENT_SOURCE_DIR}/../test_mos6510_basic.c
    )
    target_link_libraries(test_mos6510_basic_tests mos6510_lib mos6502_family)
endif()

# Enable testing
enable_testing()

# ProcessorTests - Primary validation (if ProcessorTests directory exists)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/processor_tests)
    add_test(NAME processor_tests 
             COMMAND processor_tests_runner processor_tests/6502/v1/
             WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
    set_tests_properties(processor_tests PROPERTIES
        TIMEOUT 300
        PASS_REGULAR_EXPRESSION "ALL TESTS PASSED"
    )
endif()

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

# Custom targets for convenience
add_custom_target(run_processor_tests
    COMMAND ${CMAKE_CTEST_COMMAND} --test-dir ${CMAKE_BINARY_DIR} -R processor_tests --verbose
    DEPENDS processor_tests_runner
    COMMENT "Running ProcessorTests ground truth validation"
)

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
    DEPENDS processor_tests_runner test_mos6510_comprehensive
    COMMENT "Running all MOS6510 tests"
)

# Install targets (optional)
install(TARGETS processor_tests_runner test_mos6510_comprehensive
    RUNTIME DESTINATION bin
)

if(TARGET test_mos6510_basic_tests)
    install(TARGETS test_mos6510_basic_tests
        RUNTIME DESTINATION bin
    )
endif()

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