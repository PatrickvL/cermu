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

# Collect CPU source files (using fam65xx template-based implementation)
set(CPU_SOURCES
    ${CPU_DIR}/fam65xx/mos6502.cpp
    ${CPU_DIR}/fam65xx/mos6510.cpp
    ${CPU_DIR}/fam65xx/nes6502.cpp
    ${CPU_DIR}/fam65xx/wdc65c02.cpp
    ${CPU_DIR}/fam65xx/rockwell65c02.cpp
    ${CPU_DIR}/fam65xx/wdc65c816.cpp
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

# Create a library for the MOS6510 CPU
add_library(mos6510_lib STATIC ${CPU_SOURCES} ${CORE_SOURCES})
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
target_compile_definitions(fam65xx_klaus_test_runner PRIVATE AIEMUC_IMPL)
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
target_compile_definitions(fam65xx_lorenz_test_harness PRIVATE AIEMUC_IMPL)
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
)
target_include_directories(fam65xx_processor_tests_runner PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/chip/cpu/fam65xx
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/core
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_compile_definitions(fam65xx_processor_tests_runner PRIVATE AIEMUC_IMPL)
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

# Install targets (optional)

# Install fam65xx test runner
install(TARGETS fam65xx_klaus_test_runner
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