cmake_minimum_required(VERSION 3.16)

# NES System Library
add_library(nes_system
    nes_system.cpp
    nes_system.h
    ppu/nes_ppu.cpp
    bus/nes_bus.cpp
    cartridge/nes_cartridge.cpp
    screen/nes_screen_utils.cpp
    screen/nes_screen_utils.h
    nsf/nes_nsf_player.cpp
    nsf/nes_nsf_player.h
    cartridge/mappers/mapper_nsf.h
)

target_include_directories(nes_system PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(nes_system
    fam65xx
)

# Compiler-specific settings
if(MSVC)
    target_compile_options(nes_system PRIVATE /W4)
    target_compile_definitions(nes_system PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
    target_compile_options(nes_system PRIVATE -Wall -Wextra -std=c++17)
endif()

# Optional: Create example NES emulator executable
option(BUILD_NES_EXAMPLE "Build NES system example" ON)

if(BUILD_NES_EXAMPLE)
    add_executable(nes_example
        examples/nes_example.cpp
    )
    
    target_link_libraries(nes_example
        nes_system
    )
    
    if(MSVC)
        target_compile_options(nes_example PRIVATE /W4)
    else()
        target_compile_options(nes_example PRIVATE -Wall -Wextra -std=c++17)
    endif()
endif()