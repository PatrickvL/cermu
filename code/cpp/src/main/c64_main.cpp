#include "../systems/c64/c64.h"
#include "../systems/c64/c64_config.h"
#include <iostream>
#include <memory>
#include <cstdint>

// ============================================================================
// MAIN FUNCTION - Modern C++ test harness
// ============================================================================
int main() {
    try {
        // Create system configuration (PAL by default) using C++ initialization
        c64_config_t config{
            .vicii_standard = VIC_PAL,
            .rom_config = nullptr,  // Use default ROM paths
            .roml_present = false,
            .romh_present = false,
            .roml_filename = nullptr,
            .romh_filename = nullptr,
            .initial_exrom_state = true,   // Default EXROM high (inactive)
            .initial_game_state = true     // Default GAME high (inactive)
        };
        
        // Initialize the C64 system and get the instance
        c64_t* c64 = c64_system_create(&config);
        if (!c64) {
            std::cerr << "Failed to initialize C64 system!" << std::endl;
            return 1;
        }

        // Simple run loop on the C++ 6510 core
        std::cout << "C64 emulator initialized. Running 5000 cycles..." << std::endl;
        constexpr std::uint64_t cycles = 5000;
        
        for (std::uint64_t i = 0; i < cycles; ++i) {
            c64_cpu_cycle(c64);
        }
        
        std::cout << "Completed " << cycles << " cycles." << std::endl;

        // Clean up
        c64_system_destroy(c64);
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Exception occurred: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Unknown exception occurred!" << std::endl;
        return 1;
    }
}