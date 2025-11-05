#include "../systems/c64/c64.h"
#include "../systems/c64/c64_config.h"
#include "../gui/cimgui_interface.h"
#include <stdio.h>

// ============================================================================
// MAIN FUNCTION - Threaded C64 Emulator with GUI
// ============================================================================
int main(int argc, char** argv) {
    (void)argc;
    (void)argv;
    
    // Initialize GUI
    if (!gui_init("C64 Emulator", 1200, 800)) {
        return 1;
    }
      // Initialize the C64 system
    c64_config_t config = {
        .vicii_standard = VIC_PAL,
        .rom_config = NULL  // Use default ROM paths, can be overridden by GUI later
    };
    c64_t* c64 = c64_system_create(&config);
    if (!c64) {
        gui_cleanup();
        return 1;
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
    emulation_context_t emu_context;
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
    
    printf("C64 Emulator started with threaded execution\n");
    
    // Track previous GUI state for signal generation
    bool prev_emulation_running = false;
    bool prev_emulation_paused = true;
    emulation_state_t prev_emu_state = EMU_STATE_STOPPED;
      // Main GUI loop (runs at ~60 FPS)
    while (!gui_should_quit()) {        // Handle events and input (with emulation context for proper shutdown)
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
