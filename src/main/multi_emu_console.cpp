#include "core/system.h"
#include "core/formats/format_handler.h"
#include <cstdio>
#include <cstring>
#include <memory>
#include <cstdlib>

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
        printf("    Formats: ");
        if (desc.supported_formats) {
            for (const format_descriptor_t* const* fmt = desc.supported_formats; *fmt != nullptr; fmt++) {
                printf("%s ", (*fmt)->name);
            }
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
        
        // Allocate framebuffer for headless rendering
        uint32_t* fb = (uint32_t*)calloc(width * height, sizeof(uint32_t));
        if (fb) {
            system->set_framebuffer(fb, width, height);
            printf("Allocated %dx%d framebuffer for headless rendering\n\n", width, height);
        }
        
        // Run enough frames for boot + program loading
        int total_frames = 300;
        printf("Running %d frames...\n", total_frames);
        
        // Drain audio periodically to prevent ring-buffer overflow
        float drain_buf[8192];
        for (int i = 0; i < total_frames; i++) {
            system->run_frame();
            if ((i + 1) % 50 == 0) {
                uint32_t got = system->get_audio_samples(drain_buf, 8192);
                printf("  Frame %d - Cycles: %llu, audio: %u samples\n", i + 1,
                       (unsigned long long)system->get_total_cycles(), got);
            }
        }
        printf("\n");
        
        // Shutdown
        printf("Shutting down %s...\n", desc.name);
        system->shutdown();
        free(fb);
        printf("Shutdown complete\n\n");
        
        printf("=================================================\n");
        printf("Test completed successfully!\n");
        printf("=================================================\n");
        
    } else {
        printf("Usage: %s <file>\n", argv[0]);
        printf("  Provide a ROM/disk/binary file to test system detection\n");
        printf("  Supported formats:\n");
        for (const auto& [desc, factory] : systems) {
            printf("    %s: ", desc.short_name);
            if (desc.supported_formats) {
                for (const format_descriptor_t* const* fmt = desc.supported_formats; *fmt != nullptr; fmt++) {
                    printf("%s ", (*fmt)->name);
                }
            }
            printf("\n");
        }
    }
    
    return 0;
}