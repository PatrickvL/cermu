#include "../systems/c64/c64.h"
#include "../systems/c64/c64_config.h"
#include <iostream>
//#include <memory>
#include <cstdint>
#include <cstring>

// Forward declarations for banking verification
extern void verify_c64_banking_modes();
extern void verify_c64_banking_by_table();

// ============================================================================
// MAIN FUNCTION - Modern C++ test harness
// ============================================================================
int main(int argc, char** argv) {
    // Parse command-line arguments for banking verification
    bool run_banking_verify = false;
    bool banking_verify_all = false;
    
    for (int i = 1; i < argc; i++) {
        if (std::strcmp(argv[i], "--verify-banking") == 0) {
            run_banking_verify = true;
        } else if (std::strcmp(argv[i], "--verify-banking-all") == 0) {
            run_banking_verify = true;
            banking_verify_all = true;
        }
    }
    
    // Run banking verification if requested (standalone mode)
    if (run_banking_verify) {
        std::cout << "\n";
        std::cout << "═══════════════════════════════════════════════════════════════════\n";
        std::cout << "  C64 Banking Mode Verification (Standalone)\n";
        std::cout << "═══════════════════════════════════════════════════════════════════\n";
        std::cout.flush();
        
        if (banking_verify_all) {
            std::cout << "Running verify_c64_banking_modes()...\n";
            std::cout.flush();
            verify_c64_banking_modes();
        } else {
            std::cout << "Running verify_c64_banking_by_table()...\n";
            std::cout.flush();
            verify_c64_banking_by_table();
        }
        std::cout << "\nVerification complete.\n";
        std::cout.flush();
        return 0;  // Exit after verification (no emulator run)
    }
    
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

        // Simple run loop on the C++ 6510 core - run enough cycles to complete boot sequence
        std::cout << "C64 emulator initialized. Running 1000000 cycles to complete boot sequence..." << std::endl;
        constexpr std::uint64_t cycles = 1000000;
   
        // Add detailed per-cycle logging to debug why PC is not incrementing
        for (std::uint64_t i = 0; i < cycles; ++i) {
            if (i < 50 || i % 10000 == 0) { // Log first 50 cycles and every 10K cycles
                std::cout << "[CYCLE " << i << "] ";
            }
            c64_system_tick(c64);
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