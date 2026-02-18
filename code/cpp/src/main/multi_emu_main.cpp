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
#include "../systems/c64/c64_kernal_patches.h"

// ============================================================================
// MAIN FUNCTION - Multi-System Emulator with Automatic Detection
// ============================================================================
int main(int argc, char** argv) {
    const char* file_path = nullptr;
    const char* system_name = nullptr;
    bool vicii_test_mode = false;
    bool vicii_dump_mode = false;
    bool skip_memtest = false;
    
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
        } else if (strcmp(argv[i], "--vicii-dump") == 0) {
            vicii_dump_mode = true;
            system_name = "C64";
            printf("VIC-II dump mode enabled\n");
        } else if (strcmp(argv[i], "--skip-memtest") == 0) {
            skip_memtest = true;
            printf("KERNAL memory test skip enabled\n");
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            printf("Usage: %s [options] [file]\n", argv[0]);
            printf("\nOptions:\n");
            printf("  --system, -s <name>   Select system by short name (e.g., C64, CHIP8)\n");
            printf("  --vicii-test          Run VIC-II register test suite (headless)\n");
            printf("  --skip-memtest        Patch C64 KERNAL to skip RAMTAS memory test\n");
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
    // Apply --skip-memtest patch for C64 systems
    // TODO: This should eventually be part of a configurable patch registry
    // where users can enable/disable patches per system.  For now, only
    // applies when explicitly requested via CLI.  Some whitelisted software
    // (SID files) already applies this automatically via ensure_compatible_for_sid.
    // =========================================================================
    if (skip_memtest && system) {
        auto* c64_wrapper = dynamic_cast<C64SystemWrapper*>(system.get());
        if (c64_wrapper && c64_wrapper->get_c64_system()) {
            c64_patch_skip_memtest(c64_wrapper->get_c64_system());
        } else if (c64_wrapper) {
            printf("WARNING: --skip-memtest specified but C64 not yet initialized\n");
        } else {
            printf("WARNING: --skip-memtest is only supported for C64 systems\n");
        }
    }

    // =========================================================================
    // VIC-II DUMP MODE — normal boot + framebuffer pixel dump
    // =========================================================================
    if (vicii_dump_mode && system) {
        C64SystemWrapper* c64_wrapper = dynamic_cast<C64SystemWrapper*>(system.get());
        if (!c64_wrapper) { printf("ERROR: --vicii-dump requires C64\n"); return 1; }
        c64_t* c64 = c64_wrapper->get_c64_system();
        if (!c64) { printf("ERROR: C64 not initialized\n"); return 1; }

        // Allocate headless framebuffer (no GUI)
        int fb_width, fb_height;
        system->get_display_dimensions(&fb_width, &fb_height);
        std::unique_ptr<uint32_t[]> fb(new uint32_t[fb_width * fb_height]());
        system->set_framebuffer(fb.get(), fb_width, fb_height);

        printf("VICII-DUMP: Normal boot, %dx%d framebuffer\n", fb_width, fb_height);

        // Palette lookup
        static const uint32_t PAL[16] = {
            0xFF000000, 0xFFFFFFFF, 0xFF2B3768, 0xFFB2A470,
            0xFF863D6F, 0xFF438D58, 0xFF792835, 0xFF6FC7B8,
            0xFF254F6F, 0xFF003943, 0xFF59679A, 0xFF444444,
            0xFF6C6C6C, 0xFF84D29A, 0xFFB55E6C, 0xFF959595
        };
        auto color_name = [&](uint32_t rgba) -> const char* {
            for (int i = 0; i < 16; i++) {
                if (PAL[i] == rgba) {
                    static const char* n[16] = {"BLK","WHT","RED","CYN","PUR","GRN","BLU","YEL","ORN","BRN","LRD","DG1","DG2","LGN","LBL","LG3"};
                    return n[i];
                }
            }
            return "???";
        };
        auto fb_px = [&](int x, int y) -> uint32_t {
            if (x < 0 || y < 0 || x >= fb_width || y >= fb_height) return 0xDEADBEEF;
            return fb.get()[y * fb_width + x];
        };

        // Run frames and dump at key points
        float drain[4096];
        int frame_targets[] = { 5, 200 };
        int frame_count = 0;
        for (int t = 0; t < 2; t++) {
            while (frame_count < frame_targets[t]) {
                system->run_frame();
                system->get_audio_samples(drain, 4096);
                frame_count++;
            }
            printf("\n=== Frame %d ===\n", frame_count);
            uint8_t d011 = c64->vicii->registers.data[0x11];
            uint8_t d016 = c64->vicii->registers.data[0x16];
            uint8_t d020 = c64->vicii->registers.data[0x20];
            uint8_t d021 = c64->vicii->registers.data[0x21];
            uint8_t yscroll = d011 & 0x07;
            uint8_t xscroll = d016 & 0x07;
            bool csel = (d016 & 0x08) != 0;
            printf("$D011=$%02X $D016=$%02X $D020=$%02X $D021=$%02X YSCROLL=%d XSCROLL=%d CSEL=%d\n",
                   d011, d016, d020, d021, yscroll, xscroll, csel?1:0);
            
            // Screen codes
            printf("Screen[0..9]: ");
            for (int i = 0; i < 10; i++) printf("$%02X ", c64->ram->memory[0x0400+i]);
            printf("\n");
            printf("Screen[$0400+40..49]: ");
            for (int i = 40; i < 50; i++) printf("$%02X ", c64->ram->memory[0x0400+i]);
            printf("\n");
            
            // Color RAM
            printf("ColorRAM[0..9]: ");
            for (int i = 0; i < 10; i++) printf("%d ", c64->ram->memory[0xD800+i] & 0x0F);
            printf("\n");
            
            // Character ROM reference for screen code at position 5
            uint8_t sc = c64->ram->memory[0x0400+5];
            uint8_t sc_row1_5 = c64->ram->memory[0x0400+40+5];
            printf("CharROM ref for sc=$%02X: ", sc);
            // Read from character ROM chip (offset $D000 in VIC-II bank 0)
            // The character ROM is at the chip labeled 'characters' 
            // For a C64, the character ROM is mapped at $D000-$DFFF in VIC-II address space
            if (c64->charrom && c64->charrom->memory) {
                for (int row = 0; row < 8; row++) {
                    printf("$%02X ", c64->charrom->memory[sc * 8 + row]);
                }
            }
            printf("\n");
            printf("CharROM ref for row1 sc=$%02X: ", sc_row1_5);
            if (c64->charrom && c64->charrom->memory) {
                for (int row = 0; row < 8; row++) {
                    printf("$%02X ", c64->charrom->memory[sc_row1_5 * 8 + row]);
                }
            }
            printf("\n");

            // First bad line for YSCROLL=3 is raster 51
            // Char row N starts at raster 51 + N*8
            int first_raster = 48 + yscroll; // $30 + YSCROLL
            printf("First bad line at raster %d\n", first_raster);
            
            // Dump char row 0 — full 8 rasters, showing pixel bits for col 5
            printf("\nChar row 0 (rasters %d-%d), col5 fb_x 82-89:\n", first_raster, first_raster+7);
            for (int fy = first_raster; fy <= first_raster+7; fy++) {
                int rc_expected = fy - first_raster;
                printf("  y=%3d (RC%d):", fy, rc_expected);
                // Show 8 pixels from col 5
                uint8_t bits = 0;
                for (int px = 0; px < 8; px++) {
                    uint32_t c = fb_px(82 + px, fy);
                    const char* cn = color_name(c);
                    // Check if foreground or background
                    bool is_fg = (c != PAL[d021]); // not background
                    bits |= (is_fg ? 1 : 0) << (7 - px);
                    printf(" %s", cn);
                }
                printf(" = $%02X", bits);
                // Compare with expected character ROM row
                if (c64->charrom && c64->charrom->memory) {
                    uint8_t expected = c64->charrom->memory[sc * 8 + rc_expected];
                    printf(" (ROM row%d=$%02X %s)", rc_expected, expected, bits == expected ? "OK" : "MISMATCH!");
                }
                printf("\n");
            }
            
            // Also dump char row 1
            int r1_start = first_raster + 8;
            printf("\nChar row 1 (rasters %d-%d), col5 fb_x 82-89:\n", r1_start, r1_start+7);
            for (int fy = r1_start; fy <= r1_start+7; fy++) {
                int rc_expected = fy - r1_start;
                printf("  y=%3d (RC%d):", fy, rc_expected);
                uint8_t bits = 0;
                for (int px = 0; px < 8; px++) {
                    uint32_t c = fb_px(82 + px, fy);
                    const char* cn = color_name(c);
                    bool is_fg = (c != PAL[d021]);
                    bits |= (is_fg ? 1 : 0) << (7 - px);
                    printf(" %s", cn);
                }
                printf(" = $%02X", bits);
                if (c64->charrom && c64->charrom->memory) {
                    uint8_t expected = c64->charrom->memory[sc_row1_5 * 8 + rc_expected];
                    printf(" (ROM row%d=$%02X %s)", rc_expected, expected, bits == expected ? "OK" : "MISMATCH!");
                }
                printf("\n");
            }
            
            // Dump char row 2 for good measure
            int r2_start = first_raster + 16;
            uint8_t sc_row2_5 = c64->ram->memory[0x0400+80+5];
            printf("\nChar row 2 col5 sc=$%02X (rasters %d-%d):\n", sc_row2_5, r2_start, r2_start+7);
            for (int fy = r2_start; fy <= r2_start+7; fy++) {
                int rc_expected = fy - r2_start;
                printf("  y=%3d (RC%d):", fy, rc_expected);
                uint8_t bits = 0;
                for (int px = 0; px < 8; px++) {
                    uint32_t c = fb_px(82 + px, fy);
                    const char* cn = color_name(c);
                    bool is_fg = (c != PAL[d021]);
                    bits |= (is_fg ? 1 : 0) << (7 - px);
                    printf(" %s", cn);
                }
                printf(" = $%02X", bits);
                if (c64->charrom && c64->charrom->memory) {
                    uint8_t expected = c64->charrom->memory[sc_row2_5 * 8 + rc_expected];
                    printf(" (ROM row%d=$%02X %s)", rc_expected, expected, bits == expected ? "OK" : "MISMATCH!");
                }
                printf("\n");
            }
        }
        
        // Save final framebuffer as PNG for visual inspection
        printf("\nSaving framebuffer to /tmp/vicii_dump_f200.png...\n");
        c64_set_framebuffer(c64, fb.get(), fb_width, fb_height);
        // Use stbi_write_png via c64_save_screenshot  
        c64_save_screenshot(c64, "/tmp/vicii_dump_f200.png");
        printf("Done. Check /tmp/vicii_dump_f200.png\n");
        
        system->shutdown();
        return 0;
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