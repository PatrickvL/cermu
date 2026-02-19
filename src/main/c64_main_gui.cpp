#include "../systems/c64/c64.h"
#include "../systems/c64/c64_config.h"
#include "../core/formats/prg_format.h"
#include "../gui/imgui_interface.h"
#include <stdio.h>
#include <string.h>

// Forward declarations for banking verification
extern void verify_c64_banking_modes();
extern void verify_c64_banking_by_table();

// ============================================================================
// MAIN FUNCTION - Threaded C64 Emulator with GUI
// ============================================================================
int main(int argc, char** argv) {
    const char* prg_file = NULL;
    bool run_banking_verify = false;
    bool banking_verify_all = false;
    
    // Parse command-line arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--verify-banking") == 0) {
            run_banking_verify = true;
        } else if (strcmp(argv[i], "--verify-banking-all") == 0) {
            run_banking_verify = true;
            banking_verify_all = true;
        } else if (prg_file == NULL) {
            prg_file = argv[i];
            printf("Loading PRG file: %s\n", prg_file);
        }
    }
    
    // Run banking verification if requested (standalone mode without GUI)
    if (run_banking_verify) {
        printf("\n");
        printf("═══════════════════════════════════════════════════════════════════\n");
        printf("  C64 Banking Mode Verification (Standalone)\n");
        printf("═══════════════════════════════════════════════════════════════════\n");
        fflush(stdout);
        
        if (banking_verify_all) {
            printf("Running verify_c64_banking_modes()...\n");
            fflush(stdout);
            verify_c64_banking_modes();
        } else {
            printf("Running verify_c64_banking_by_table()...\n");
            fflush(stdout);
            verify_c64_banking_by_table();
        }
        printf("\nVerification complete.\n");
        fflush(stdout);
        return 0;  // Exit after verification (no GUI)
    }
    
    // Initialize GUI
    if (!gui_init("C64 Emulator", 1200, 800)) {
        return 1;
    }
    
    // Initialize the C64 system
    c64_config_t config = {
        .vicii_standard = VIC_PAL,
        .rom_config = NULL,  // Use default ROM paths, can be overridden by GUI later
        .test_mode = C64_TEST_MODE_NORMAL,  // Normal boot mode
        .test_binary_config = NULL,         // No test binary
        .roml_present = false,
        .romh_present = false,
        .roml_filename = nullptr,
        .romh_filename = nullptr,
        .initial_exrom_state = true,   // Default EXROM high (inactive)
        .initial_game_state = true     // Default GAME high (inactive)
    };
    c64_t* c64 = c64_system_create(&config);
    if (!c64) {
        gui_cleanup();
        return 1;
    }
    
    // Load PRG file if provided on command line
    if (prg_file != NULL) {
        commodore_prg_t prg = {};
        if (!commodore_prg_load(prg_file, &prg)) {
            printf("Failed to load PRG file: %s\n", prg_file);
            // Continue anyway - user can load via GUI
        } else {
            memcpy(&c64->ram->memory[prg.load_addr], prg.data, prg.data_size);
            printf("Successfully loaded PRG file: %s\n", prg_file);
            printf("  Load address: $%04X\n", prg.load_addr);
            printf("  Size: %zu bytes\n", prg.data_size);
            commodore_prg_free(&prg);
        }
    }
    
    // Set up framebuffer for VIC-II
    uint32_t* screen_buffer = gui_get_screen_buffer();
    int width, height;
    gui_get_screen_dimensions(&width, &height);
    c64_set_framebuffer(c64, screen_buffer, width, height);
    
    // Initialize GUI state
    gui_state_t gui_state;
    gui_init_state(&gui_state);
    
    // Initialize emulation thread through GUI
    gui_emulation_context_t emu_context;
    if (!gui_emulation_thread_init(&emu_context, c64)) {
        printf("Failed to initialize emulation thread\n");
        c64_system_destroy(c64);
        gui_cleanup();
        return 1;
    }
    
    // Start emulation thread
    if (!gui_emulation_thread_start(&emu_context)) {
        printf("Failed to start emulation thread\n");
        gui_emulation_thread_cleanup(&emu_context);
        c64_system_destroy(c64);
        gui_cleanup();
        return 1;
    }
    
    // Automatically start emulation since gui_state.emulation_running is true by default
    printf("C64 Emulator started with threaded execution\n");
    printf("Auto-starting emulation...\n");
    gui_emulation_start(&emu_context);
    
    // Track previous GUI state for signal generation
    bool prev_emulation_running = false;
    bool prev_emulation_paused = true;
    emulation_state_t prev_emu_state = EMU_STATE_STOPPED;
    
    // Main GUI loop (runs at ~60 FPS)
    while (!gui_should_quit()) {
        // Handle events and input (with emulation context for proper shutdown)
        gui_handle_events(&emu_context);
        
        // Update GUI state with current emulation status
        emulation_state_t emu_state = gui_emulation_get_state(&emu_context);
        gui_state.emulation_running = (emu_state == EMU_STATE_RUNNING);
        gui_state.emulation_paused = (emu_state == EMU_STATE_PAUSED || emu_state == EMU_STATE_STOPPED);
        
        // Check for GUI state changes and send appropriate signals
        if (gui_state.emulation_running != prev_emulation_running) {
            if (gui_state.emulation_running) {
                printf("GUI: Starting emulation\n");
                gui_emulation_send_signal(&emu_context, EMU_SIGNAL_START);
            } else {
                printf("GUI: Stopping emulation\n");
                gui_emulation_send_signal(&emu_context, EMU_SIGNAL_PAUSE);
            }
            prev_emulation_running = gui_state.emulation_running;
        }
        
        if (gui_state.emulation_paused != prev_emulation_paused) {
            if (gui_state.emulation_paused && prev_emu_state == EMU_STATE_RUNNING) {
                printf("GUI: Pausing emulation\n");
                gui_emulation_send_signal(&emu_context, EMU_SIGNAL_PAUSE);
            } else if (!gui_state.emulation_paused && prev_emu_state == EMU_STATE_PAUSED) {
                printf("GUI: Resuming emulation\n");
                gui_emulation_send_signal(&emu_context, EMU_SIGNAL_START);
            }
            prev_emulation_paused = gui_state.emulation_paused;
        }
        
        prev_emu_state = emu_state;
        
        // Handle frame rendering if emulation is running
        if (emu_state == EMU_STATE_RUNNING) {
            gui_emulation_render_frame(&emu_context);
        }
        
        // Update FPS counter
        gui_emulation_update_fps(&emu_context);
        
        // Render GUI frame with emulation context for control buttons
        gui_render_frame(c64, &gui_state, &emu_context);
        
        // GUI frame rate limiting (60 FPS)
        gui_delay(16);
    }
    
    printf("Shutting down C64 emulator\n");
    
    // Cleanup emulation thread
    gui_emulation_thread_stop(&emu_context);
    gui_emulation_thread_cleanup(&emu_context);
    
    // Cleanup
    gui_cleanup_state(&gui_state);
    c64_system_destroy(c64);
    gui_cleanup();
    
    return 0;
}
