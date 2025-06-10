#include "../systems/c64/c64.h"
#include "../systems/c64/system_config.h"
#include "../gui/cimgui_interface.h"
#include <SDL.h>

// ============================================================================
// MAIN FUNCTION - ImGui-based C64 Emulator
// ============================================================================
int main(int argc, char** argv) {
    (void)argc;
    (void)argv;
    
    // Initialize GUI
    if (!gui_init("C64 Emulator", 1200, 800)) {
        return 1;
    }
    
    // Initialize the C64 system
    system_config_t config = {
        .vic_standard = VIC_PAL
    };
    c64_t* c64 = c64_system_create(&config);
    if (!c64) {
        gui_cleanup();
        return 1;
    }
    
    // Initialize GUI state
    gui_state_t gui_state;
    gui_init_state(&gui_state);
    
    // Main loop
    while (!gui_should_quit()) {
        // Handle SDL events and ImGui input
        gui_handle_events();
        
        // Run emulation if enabled
        if (gui_state.emulation_running && !gui_state.emulation_paused) {
            // TODO: Run C64 emulation step
            // For now, just increment cycle counter for demo
            c64->total_cycles++;
        }
        
        // Render GUI frame
        gui_render_frame(c64, &gui_state);
        
        // Simple frame rate limiting
        SDL_Delay(16); // ~60 FPS
    }
    
    // Cleanup
    gui_cleanup_state(&gui_state);
    c64_system_destroy(c64);
    gui_cleanup();
    
    return 0;
}
