#define SDL_MAIN_HANDLED
#include "../core/emulated_system.h"
#include "../gui/simple_system_gui.h"
#include "../testing/vicii_test_harness.h"
#include "../testing/vicii_pixel_tests.h"
#include <stdio.h>
#include <memory>
#include <cstring>

// Force linker to include system registrations
// Systems self-register during static initialization via REGISTER_SYSTEM macro
// We just need to ensure the system object files are linked
#include "../systems/chip8/chip8_system.h"
#include "../systems/c64/c64_system_wrapper.h"

// ============================================================================
// MAIN FUNCTION - Multi-System Emulator with Automatic Detection
// ============================================================================
int main(int argc, char** argv) {
    const char* file_path = nullptr;
    const char* system_name = nullptr;
    bool vicii_test_mode = false;
    
    // Parse command-line arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--system") == 0 || strcmp(argv[i], "-s") == 0) {
            if (i + 1 < argc) {
                system_name = argv[++i];
                printf("System specified: %s\n", system_name);
            } else {
                printf("ERROR: --system requires an argument\n");
                return 1;
            }
        } else if (strcmp(argv[i], "--vicii-test") == 0) {
            vicii_test_mode = true;
            system_name = "C64";  // Force C64 system
            printf("VIC-II test mode enabled\n");
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            printf("Usage: %s [options] [file]\n", argv[0]);
            printf("\nOptions:\n");
            printf("  --system, -s <name>   Select system by short name (e.g., C64, CHIP8)\n");
            printf("  --vicii-test          Run VIC-II register test suite (headless)\n");
            printf("  --help, -h            Show this help message\n");
            printf("\nAvailable systems:\n");
            for (const auto& desc : SystemRegistry::instance().get_all_descriptors()) {
                printf("  %-10s  %s\n", desc.short_name, desc.name);
            }
            return 0;
        } else if (file_path == nullptr) {
            file_path = argv[i];
            printf("File specified: %s\n", file_path);
        }
    }
    
    std::unique_ptr<EmulatedSystem> system;
    
    // If system name specified, create it directly
    if (system_name != nullptr) {
        printf("Creating system: %s\n", system_name);
        system = SystemRegistry::instance().create_system(system_name);
        
        if (!system) {
            printf("ERROR: System not found: %s\n", system_name);
            printf("Available systems:\n");
            for (const auto& desc : SystemRegistry::instance().get_all_descriptors()) {
                printf("  %s (%s)\n", desc.short_name, desc.name);
            }
            return 1;
        }
        
        printf("Created system: %s (%s)\n", 
               system->get_descriptor().name,
               system->get_descriptor().short_name);
        
        // If a file was specified, auto-detect optimal configuration
        // (e.g. memory expansion) before initializing
        if (file_path != nullptr) {
            system->apply_file_configuration(file_path);
        }

        // Initialize the system
        if (!system->initialize()) {
            printf("ERROR: Failed to initialize %s system\n", 
                   system->get_descriptor().name);
            return 1;
        }
        
        // Load file if specified
        if (file_path != nullptr) {
            if (!system->load_file(file_path)) {
                printf("ERROR: Failed to load file: %s\n", file_path);
                system->shutdown();
                return 1;
            }
            printf("Successfully loaded file into %s\n", system->get_descriptor().name);
        }
    }
    // If file was specified (but no system), try to auto-detect system
    else if (file_path != nullptr) {
        printf("Detecting system for file: %s\n", file_path);
        system = SystemRegistry::instance().create_system_for_file(file_path);
        
        if (!system) {
            printf("WARNING: Could not detect system for file: %s\n", file_path);
            printf("No emulator supports this file format.\n");
            printf("System selection dialog will be shown...\n\n");
            // Keep file_path so the GUI can load it after user picks a system
        } else {
            printf("Detected system: %s (%s)\n", 
                   system->get_descriptor().name,
                   system->get_descriptor().short_name);
            printf("Description: %s\n", system->get_descriptor().description);
            
            // Initialize the system
            if (!system->initialize()) {
                printf("ERROR: Failed to initialize %s system\n", 
                       system->get_descriptor().name);
                return 1;
            }
            
            // Load the file
            if (!system->load_file(file_path)) {
                printf("ERROR: Failed to load file: %s\n", file_path);
                system->shutdown();
                return 1;
            }
            printf("Successfully loaded file into %s\n", system->get_descriptor().name);
        }
    }
    
    // =========================================================================
    // VIC-II TEST MODE — headless test suite
    // =========================================================================
    if (vicii_test_mode && system) {
        // Get the C64 system wrapper to access the underlying c64_t
        C64SystemWrapper* c64_wrapper = dynamic_cast<C64SystemWrapper*>(system.get());
        if (!c64_wrapper) {
            printf("ERROR: --vicii-test requires C64 system\n");
            return 1;
        }
        c64_t* c64 = c64_wrapper->get_c64_system();
        if (!c64) {
            printf("ERROR: C64 system not initialized\n");
            return 1;
        }

        // 1. Patch KERNAL to skip memory test + redirect BASIC→test program
        vicii_test::patch_kernal_for_test(c64);

        // 2. Inject the 6510 test program into RAM
        vicii_test::inject_test_program(c64);

        // 3. Allocate headless framebuffer
        int fb_width, fb_height;
        system->get_display_dimensions(&fb_width, &fb_height);
        std::unique_ptr<uint32_t[]> fb(new uint32_t[fb_width * fb_height]());
        system->set_framebuffer(fb.get(), fb_width, fb_height);

        // 4. Initialize harness
        vicii_test::vicii_test_state_t test_state;
        vicii_test::harness_init(&test_state);

        printf("VICII-TEST: Running headless test suite...\n");
        printf("VICII-TEST: Display: %dx%d, FPS target: %u\n",
               fb_width, fb_height, system->get_target_fps());

        // 5. Run frames until tests complete or timeout
        float drain_buf[4096];
        while (vicii_test::harness_poll(&test_state, c64)) {
            system->run_frame();
            // Drain audio to prevent overflow
            system->get_audio_samples(drain_buf, 4096);
        }

        // One final poll to capture last result
        vicii_test::harness_poll(&test_state, c64);

        // 6. Read results buffer and print details
        vicii_test::harness_read_results(&test_state, c64);

        // 7. Print summary
        vicii_test::harness_summary(&test_state);

        // =====================================================================
        // Phase 2: Pixel verification tests (C++ driven)
        // =====================================================================
        printf("\nVICII-TEST: Starting Phase 2 — Pixel Verification...\n");
        auto pixel_results = vicii_test::run_pixel_verification_tests(
            c64, system.get(), fb.get(), fb_width, fb_height);

        int total_fail = test_state.total_fail + pixel_results.total_fail;
        int total_pass = test_state.total_pass + pixel_results.total_pass;
        printf("\n═══ COMBINED RESULTS ═══\n");
        printf("Phase 1 (register):  %d pass / %d fail\n",
               test_state.total_pass, test_state.total_fail);
        printf("Phase 2 (pixel):     %d pass / %d fail\n",
               pixel_results.total_pass, pixel_results.total_fail);
        printf("TOTAL:               %d pass / %d fail\n", total_pass, total_fail);

        // Cleanup
        system->shutdown();
        return (total_fail == 0 && test_state.all_done) ? 0 : 1;
    }

    // Create GUI (with or without a system)
    // If no system, nullptr will cause GUI to show system selection dialog
    // Pass any pending file path so it can be loaded after system selection
    SimpleSystemGUI gui(std::move(system), file_path);
    
    // Initialize GUI with window title
    const char* window_title = "Multi-System Emulator";
    if (!gui.init(window_title, 1200, 800)) {
        printf("ERROR: Failed to initialize GUI\n");
        return 1;
    }
    
    printf("Starting GUI main loop\n");
    
    // Run the GUI main loop
    gui.run();
    
    printf("Shutting down\n");
    
    // Cleanup
    gui.cleanup();
    
    return 0;
}