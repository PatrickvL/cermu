# MOS 6502 Family CPU Library

# Shared MOS 6502 Family Core
add_library(mos6502_family STATIC
    mos6502_family/mos6502_family_core.c
    mos6502_family/mos6502_family_gui.c
    mos6502_family/mos6502_family_arithmetic.c
    mos6502_family/mos6502_family_control.c
    mos6502_family/mos6502_family_memory.c
    mos6502_family/mos6502_family_registers.c
    mos6502_family/mos6502_family_shifts.c
    mos6502_family/mos6502_family_misc.c
    mos6502_family/mos6502_family_opcodes.c
)
target_include_directories(mos6502_family PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/mos6502_family
    ${CMAKE_CURRENT_SOURCE_DIR}/../../..
)

# Standard MOS 6502 (with decimal mode)
add_library(mos6502 STATIC
    mos6502/mos6502.c
    mos6502/mos6502_gui.c
)
target_link_libraries(mos6502 PUBLIC mos6502_family)
target_include_directories(mos6502 PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/mos6502
    ${CMAKE_CURRENT_SOURCE_DIR}/../../..
)

# MOS 6510 (C64 CPU, now using shared family code with MOS6510-specific illegal opcodes)
add_library(mos6510 STATIC
    mos6510/mos6510.c
    mos6510/mos6510_gui.c
    mos6510/mos6510_illegal.c
)
target_include_directories(mos6510 PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/mos6510
    ${CMAKE_CURRENT_SOURCE_DIR}/../../..
)

# MOS6510 now uses shared family code for all legal operations
target_link_libraries(mos6510 PUBLIC mos6502_family)

# Create an interface library that provides access to all CPU types
add_library(mos6502_cpus INTERFACE)
target_link_libraries(mos6502_cpus INTERFACE mos6502_family mos6502 nes6502 mos6510)

# NES 6502 (6502 without decimal mode)
add_library(nes6502 STATIC
    nes6502/nes6502.c
    nes6502/nes6502_gui.c
)
target_link_libraries(nes6502 PUBLIC mos6502_family)
target_include_directories(nes6502 PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/nes6502
    ${CMAKE_CURRENT_SOURCE_DIR}/../../..
)
