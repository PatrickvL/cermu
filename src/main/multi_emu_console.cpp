#include "core/cermu.hpp"
#include "core/system.hpp"
#include "core/formats/format_handler.hpp"
#include "core/signal/sync_types.hpp"
#include "core/signal/video_sample_types.hpp"
#include <cstdio>
#include <cstring>
#include <memory>
#include <cstdlib>
#include <unordered_set>

// Simple console test for multi-system architecture
int main(int argc, char** argv) {
    // Parse --frames N option (before positional args)
    int requested_frames = 300;
    bool quiet = false;
    bool gfx_check = false;
    int first_positional = 1;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--frames") == 0 && i + 1 < argc) {
            requested_frames = atoi(argv[++i]);
            first_positional = i + 1;
        } else if (strcmp(argv[i], "--quiet") == 0 || strcmp(argv[i], "-q") == 0) {
            quiet = true;
            first_positional = i + 1;
        } else if (strcmp(argv[i], "--gfx") == 0) {
            gfx_check = true;
            first_positional = i + 1;
        } else {
            first_positional = i;
            break;
        }
    }
    if (quiet) log_level = LogLevel::Error;

    log_info("=================================================\n");
    log_info("Multi-System Emulator - Console Test\n");
    log_info("=================================================\n\n");
    
    // Show all registered systems
    auto& registry = SystemRegistry::instance();
    const auto& systems = registry.get_systems();
    
    log_info("Registered Systems: %zu\n", systems.size());
    for (const auto& [desc, factory] : systems) {
        log_info("  - %s (%s)\n", desc.name, desc.short_name);
        log_info("    Description: %s\n", desc.description);
        log_info("    Formats: ");
        if (desc.supported_formats) {
            for (const format_descriptor_t* const* fmt = desc.supported_formats; *fmt != nullptr; fmt++) {
                log_info("%s ", (*fmt)->name);
            }
        }
        log_info("\n\n");
    }
    
    // Test file detection if file provided
    if (first_positional < argc) {
        const char* filepath = argv[first_positional];
        log_info("Testing file: %s\n", filepath);
        log_info("-------------------------------------------------\n");
        
        // Try to create system for file
        auto system = registry.create_system_for_file(filepath);
        
        if (!system) {
            log_info("ERROR: No system found for file: %s\n", filepath);
            return 1;
        }
        
        const auto& desc = system->get_descriptor();
        log_info("Detected System: %s\n", desc.name);
        log_info("Short Name: %s\n", desc.short_name);
        log_info("Description: %s\n\n", desc.description);
        
        // Initialize system
        log_info("Initializing %s...\n", desc.name);
        if (!system->initialize()) {
            log_info("ERROR: Failed to initialize system\n");
            return 1;
        }
        system->attach_default_peripherals();
        log_info("System initialized successfully\n\n");
        
        // Load file
        log_info("Loading file: %s\n", filepath);
        if (!system->load_file(filepath)) {
            log_info("ERROR: Failed to load file\n");
            system->shutdown();
            return 1;
        }
        log_info("File loaded successfully\n\n");
        
        // Get system info
        int width, height;
        system->get_display_dimensions(&width, &height);
        log_info("Display Dimensions: %dx%d\n", width, height);
        log_info("Target FPS: %u\n", system->get_target_fps());
        log_info("Total Cycles: %llu\n", (unsigned long long)system->get_total_cycles());
        log_info("Speed Multiplier: %.2f\n\n", system->get_speed_multiplier());
        
        // Allocate framebuffer for headless rendering
        uint32_t* fb = (uint32_t*)calloc(width * height, sizeof(uint32_t));
        if (fb) {
            system->set_framebuffer(fb, width, height);
            log_info("Allocated %dx%d framebuffer for headless rendering\n\n", width, height);
        }
        
        // Run enough frames for boot + program loading
        int total_frames = requested_frames;
        log_info("Running %d frames...\n", total_frames);
        
        // Drain audio periodically to prevent ring-buffer overflow
        float drain_buf[8192];
        for (int i = 0; i < total_frames; i++) {
            system->run_frame();
            if ((i + 1) % 50 == 0) {
                uint32_t got = system->get_audio_samples(drain_buf, 8192);
                log_info("  Frame %d - Cycles: %llu, audio: %u samples\n", i + 1,
                       (unsigned long long)system->get_total_cycles(), got);
            }
        }
        log_info("\n");
        
        // Graphics detection — analyze signal data for non-blank content
        if (gfx_check) {
            const auto& fd = system->get_last_frame_data();
            int unique = 0;
            int visible_pixels = 0;
            if (fd.signal_output && fd.signal_output_len > 0 &&
                fd.signal_type == VideoSignalType::Composite) {
                const auto* samples = static_cast<const CompositeVideoSample*>(fd.signal_output);
                std::unordered_set<uint8_t> colors;
                for (uint32_t i = 0; i < fd.signal_output_len; i++) {
                    if (!has_flag(samples[i].flags, SyncFlag::Blank)) {
                        colors.insert(samples[i].color_index);
                        visible_pixels++;
                    }
                }
                unique = static_cast<int>(colors.size());
            }
            // Machine-readable output (always printed, even in quiet mode)
            fprintf(stdout, "GFX_RESULT: unique_colors=%d visible_pixels=%d\n",
                    unique, visible_pixels);
        }
        
        // Shutdown
        log_info("Shutting down %s...\n", desc.name);
        system->shutdown();
        free(fb);
        log_info("Shutdown complete\n\n");
        
        log_info("=================================================\n");
        log_info("Test completed successfully!\n");
        log_info("=================================================\n");
        
    } else {
        log_info("Usage: %s <file>\n", argv[0]);
        log_info("  Provide a ROM/disk/binary file to test system detection\n");
        log_info("  Supported formats:\n");
        for (const auto& [desc, factory] : systems) {
            log_info("    %s: ", desc.short_name);
            if (desc.supported_formats) {
                for (const format_descriptor_t* const* fmt = desc.supported_formats; *fmt != nullptr; fmt++) {
                    log_info("%s ", (*fmt)->name);
                }
            }
            log_info("\n");
        }
    }
    
    return 0;
}