#include "../core/emulated_system.h"
#include "../gui/simple_system_gui.h"
#include <stdio.h>
#include <memory>

// Force linker to include system registrations
#include "../systems/chip8/chip8_system.h"
#include "../systems/c64/c64_system_wrapper.h"

// ============================================================================
// MAIN FUNCTION - Multi-System Emulator with Automatic Detection
// ============================================================================
int main(int argc, char** argv) {
    const char* file_path = nullptr;
    
    // Parse command-line arguments
    for (int i = 1; i < argc; i++) {
        if (file_path == nullptr) {
            file_path = argv[i];
            printf("File specified: %s\n", file_path);
        }
    }
    
    // Must have a file to load
    if (file_path == nullptr) {
        printf("Usage: %s <file>\n", argv[0]);
        printf("Supported formats:\n");
        printf("  - CHIP-8: .ch8, .c8\n");
        printf("  - C64: .prg, .crt, .d64, .t64\n");
        return 1;
    }
    
    // Auto-detect system from file
    printf("Detecting system for file: %s\n", file_path);
    std::unique_ptr<IEmulatedSystem> system = 
        SystemRegistry::instance().create_system_for_file(file_path);
    
    if (!system) {
        printf("ERROR: Could not detect system for file: %s\n", file_path);
        printf("No emulator supports this file format.\n");
        return 1;
    }
    
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
    
    // Create GUI with the system
    SimpleSystemGUI gui(std::move(system));
    
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