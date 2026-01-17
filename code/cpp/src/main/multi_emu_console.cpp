#include "../core/emulated_system.h"
#include <stdio.h>
#include <string.h>
#include <memory>

// Simple console test for multi-system architecture
int main(int argc, char** argv) {
    printf("=================================================\n");
    printf("Multi-System Emulator - Console Test\n");
    printf("=================================================\n\n");
    
    // Show all registered systems
    auto& registry = SystemRegistry::instance();
    const auto& systems = registry.get_systems();
    
    printf("Registered Systems: %zu\n", systems.size());
    for (const auto& [desc, factory] : systems) {
        printf("  - %s (%s)\n", desc.name, desc.short_name);
        printf("    Description: %s\n", desc.description);
        printf("    Extensions: ");
        for (const char** ext = desc.supported_extensions; *ext != nullptr; ext++) {
            printf("%s ", *ext);
        }
        printf("\n\n");
    }
    
    // Test file detection if file provided
    if (argc > 1) {
        const char* filepath = argv[1];
        printf("Testing file: %s\n", filepath);
        printf("-------------------------------------------------\n");
        
        // Try to create system for file
        auto system = registry.create_system_for_file(filepath);
        
        if (!system) {
            printf("ERROR: No system found for file: %s\n", filepath);
            return 1;
        }
        
        const auto& desc = system->get_descriptor();
        printf("Detected System: %s\n", desc.name);
        printf("Short Name: %s\n", desc.short_name);
        printf("Description: %s\n\n", desc.description);
        
        // Initialize system
        printf("Initializing %s...\n", desc.name);
        if (!system->initialize()) {
            printf("ERROR: Failed to initialize system\n");
            return 1;
        }
        printf("System initialized successfully\n\n");
        
        // Load file
        printf("Loading file: %s\n", filepath);
        if (!system->load_file(filepath)) {
            printf("ERROR: Failed to load file\n");
            system->shutdown();
            return 1;
        }
        printf("File loaded successfully\n\n");
        
        // Get system info
        int width, height;
        system->get_display_dimensions(&width, &height);
        printf("Display Dimensions: %dx%d\n", width, height);
        printf("Target FPS: %u\n", system->get_target_fps());
        printf("Total Cycles: %llu\n", (unsigned long long)system->get_total_cycles());
        printf("Speed Multiplier: %.2f\n\n", system->get_speed_multiplier());
        
        // Run a few frames
        printf("Running 10 frames...\n");
        for (int i = 0; i < 10; i++) {
            system->run_frame();
            if (i % 5 == 4) {
                printf("  Frame %d - Cycles: %llu\n", i + 1, 
                       (unsigned long long)system->get_total_cycles());
            }
        }
        printf("\n");
        
        // Test speed control
        printf("Testing speed multiplier...\n");
        system->set_speed_multiplier(2.0f);
        printf("  Set to 2.0x: %.2f\n", system->get_speed_multiplier());
        system->set_speed_multiplier(0.5f);
        printf("  Set to 0.5x: %.2f\n", system->get_speed_multiplier());
        system->set_speed_multiplier(1.0f);
        printf("  Reset to 1.0x: %.2f\n\n", system->get_speed_multiplier());
        
        // Shutdown
        printf("Shutting down %s...\n", desc.name);
        system->shutdown();
        printf("Shutdown complete\n\n");
        
        printf("=================================================\n");
        printf("Test completed successfully!\n");
        printf("=================================================\n");
        
    } else {
        printf("Usage: %s <file>\n", argv[0]);
        printf("  Provide a ROM/disk/binary file to test system detection\n");
        printf("  Supported extensions:\n");
        for (const auto& [desc, factory] : systems) {
            printf("    %s: ", desc.short_name);
            for (const char** ext = desc.supported_extensions; *ext != nullptr; ext++) {
                printf("%s ", *ext);
            }
            printf("\n");
        }
    }
    
    return 0;
}