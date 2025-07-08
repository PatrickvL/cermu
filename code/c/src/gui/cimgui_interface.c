#include <SDL.h>
#include <SDL_opengl.h>
#ifndef CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#endif
#include <cimgui.h>

#include "cimgui_interface.h"
#include "../systems/c64/c64.h"
#include "../systems/c64/c64_bus.h"
#include "../systems/c64/c64_config.h"
#include "../utils/rom_loader.h"
#include "../chip/cpu/mos6510/mos6510.h"
#include "../chip/video/vic_ii/vicii_common.h"
#include "cimgui_backends.h"

#include <stdio.h>
#include <string.h>

// Macro to create RGBA color values for OpenGL GL_RGBA format
#define RGBA_COLOR(r, g, b, a) (((uint32_t)(a) << 24) | ((uint32_t)(b) << 16) | ((uint32_t)(g) << 8) | (uint32_t)(r))

// Global SDL and OpenGL state
static SDL_Window* g_window = NULL;
static SDL_GLContext g_gl_context = NULL;
static bool g_should_quit = false;

bool gui_init(const char* window_title, int width, int height) {
    // Initialize SDL subsystems
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_EVENTS) < 0) {
        printf("Failed to initialize SDL: %s\n", SDL_GetError());
        return false;
    }

    // SDL should already be initialized by gui_init_sdl_and_window

    // GL 3.0 + GLSL 130
    const char* glsl_version = "#version 130";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);

    // Create window with graphics context
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    g_window = SDL_CreateWindow(window_title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width, height, window_flags);
    if (g_window == NULL) {
        return false;
    }

    g_gl_context = SDL_GL_CreateContext(g_window);
    SDL_GL_MakeCurrent(g_window, g_gl_context);
    SDL_GL_SetSwapInterval(1); // Enable vsync
    
    // Show the window
    SDL_ShowWindow(g_window);

    // Setup Dear ImGui context
    igCreateContext(NULL);
    ImGuiIO* io = igGetIO_Nil();
    io->ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io->ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // Setup Dear ImGui style
    igStyleColorsDark(NULL);
    
    // Custom color scheme for C64 aesthetic
    ImGuiStyle* style = igGetStyle();
    style->Colors[ImGuiCol_WindowBg] = (ImVec4){0.06f, 0.06f, 0.2f, 0.95f};
    style->Colors[ImGuiCol_Header] = (ImVec4){0.2f, 0.2f, 0.8f, 0.8f};
    style->Colors[ImGuiCol_HeaderHovered] = (ImVec4){0.3f, 0.3f, 0.9f, 0.8f};
    style->Colors[ImGuiCol_HeaderActive] = (ImVec4){0.4f, 0.4f, 1.0f, 0.8f};
    style->Colors[ImGuiCol_Button] = (ImVec4){0.2f, 0.2f, 0.7f, 0.6f};
    style->Colors[ImGuiCol_ButtonHovered] = (ImVec4){0.3f, 0.3f, 0.8f, 0.8f};
    style->Colors[ImGuiCol_ButtonActive] = (ImVec4){0.4f, 0.4f, 0.9f, 1.0f};

    // Setup Platform/Renderer backends
    ImGui_ImplSDL2_InitForOpenGL_C(g_window, g_gl_context);
    ImGui_ImplOpenGL3_Init_C(glsl_version);

    return true;
}

void gui_cleanup(void) {
    // Cleanup ImGui
    ImGui_ImplOpenGL3_Shutdown_C();
    ImGui_ImplSDL2_Shutdown_C();
    igDestroyContext(NULL);

    if (g_gl_context) {
        SDL_GL_DeleteContext(g_gl_context);
    }
    if (g_window) {
        SDL_DestroyWindow(g_window);
    }
    SDL_Quit();
}

void gui_cleanup_state(gui_state_t* gui_state) {
    if (gui_state) {
        gui_cleanup_screen_display(gui_state);
    }
}

bool gui_should_quit(void) {
    return g_should_quit;
}

void gui_handle_events(emulation_context_t* emu_context) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        ImGui_ImplSDL2_ProcessEvent_C(&event);        if (event.type == SDL_QUIT) {
            g_should_quit = true;
            // Stop emulation when window is closed
            if (emu_context) {
                printf("GUI: Window closed, stopping emulation\n");
                // Force thread to stop immediately
                emu_context->thread_running = false;
                emu_context->current_state = EMU_STATE_STOPPED;
                // Use intercept to stop CPU execution immediately
                mos6510_start_intercept(emu_context->c64->mos6510);
                // Send quit signal to emulation thread
                gui_emulation_send_signal(emu_context, EMU_SIGNAL_QUIT);
            }
        }
        if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE && event.window.windowID == SDL_GetWindowID(g_window)) {
            g_should_quit = true;
            // Stop emulation when window is closed
            if (emu_context) {
                printf("GUI: Window close event, stopping emulation\n");
                // Force thread to stop immediately
                emu_context->thread_running = false;
                emu_context->current_state = EMU_STATE_STOPPED;
                // Use intercept to stop CPU execution immediately
                mos6510_start_intercept(emu_context->c64->mos6510);
                // Send quit signal to emulation thread
                gui_emulation_send_signal(emu_context, EMU_SIGNAL_QUIT);
            }
        }
    }
}

void gui_init_state(gui_state_t* gui_state) {
    memset(gui_state, 0, sizeof(gui_state_t));
    
    // Set default values
    gui_state->show_screen = true;  // Show screen by default
    gui_state->target_fps = 50;  // PAL C64 refresh rate
    gui_state->emulation_speed = 1.0f;
    gui_state->memory_columns = 16;
    gui_state->memory_address = 0x0000;
    
    // Screen display defaults
    gui_state->screen_texture_id = 0;
    gui_state->screen_scale = 2.0f;
    gui_state->screen_filter = false;
    gui_state->screen_scanlines = false;
    
    // Aspect ratio configuration defaults
    gui_state->aspect_ratio_mode = ASPECT_RATIO_ORIGINAL;
    gui_state->scaling_mode = SCALING_MODE_FIT;
    gui_state->custom_aspect_ratio = 4.0f / 3.0f;  // 4:3 default
    gui_state->maintain_pixel_aspect = true;
    gui_state->show_overscan = true;
    gui_state->center_display = true;
    gui_state->host_dpi_scale = 1.0f;  // Will be detected at runtime
    gui_state->show_invisible_area = false;  // Hide invisible area by default
    
    // PLA Debug window - now handled via chip debug system
    
    // Default ROM paths (can be modified by user)    
    strcpy(gui_state->rom_path_basic, "data/c64/roms/basic.901226-01.bin");
    strcpy(gui_state->rom_path_kernal, "data/c64/roms/kernal.901227-03.bin");
    strcpy(gui_state->rom_path_chargen, "data/c64/roms/characters.901225-01.bin");
}

void gui_render_frame(c64_t* c64, gui_state_t* gui_state, struct emulation_context_s* emu_context) {
    // Start the Dear ImGui frame
    ImGui_ImplOpenGL3_NewFrame_C();
    ImGui_ImplSDL2_NewFrame_C();
    igNewFrame();

    // Initialize screen display if needed
    if (gui_state->screen_texture_id == 0) {
        gui_init_screen_display(gui_state);
    }
    
    // Update screen texture with current frame
    gui_update_screen_texture(c64, gui_state);
    
    // Render C64 screen as background (fullscreen)
    gui_render_screen(c64, gui_state);

    // Render main menu bar with emulation context
    gui_render_menu_bar(c64, gui_state, emu_context);

    // Render windows based on gui_state (but not the screen window)
    if (gui_state->show_memory_viewer) {
        gui_render_memory_viewer(c64, gui_state);
    }
    if (gui_state->show_debugger) {
        gui_render_debugger(c64, gui_state, emu_context);
    }
    if (gui_state->show_settings) {
        gui_render_settings(c64, gui_state);
    }
    if (gui_state->show_about) {
        gui_render_about(gui_state);
    }
    // PLA debug is now handled through the chip system
    
    // Render chip debug and settings windows using chip callbacks
    if (c64 && c64->system.chip_count > 0) {
        for (uint8_t chip_id = 0; chip_id < c64->system.chip_count && chip_id < 16; chip_id++) {
            chip_entry_t* entry = &c64->system.chips[chip_id];
            
            // Render debug window if enabled and chip has debug callback
            if (gui_state->show_chip_debug[chip_id] && entry->desc && entry->desc->render_debug_window) {
                entry->desc->render_debug_window(entry->chip, &gui_state->show_chip_debug[chip_id]);
            }
            
            // Render settings window if enabled and chip has settings callback
            if (gui_state->show_chip_settings[chip_id] && entry->desc && entry->desc->render_settings_window) {
                entry->desc->render_settings_window(entry->chip, &gui_state->show_chip_settings[chip_id]);
            }
        }
    }

    // Rendering
    igRender();
    ImGuiIO* io = igGetIO_Nil();
    glViewport(0, 0, (int)io->DisplaySize.x, (int)io->DisplaySize.y);
    glClearColor(0.0f, 0.0f, 0.2f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData_C(igGetDrawData());    SDL_GL_SwapWindow(g_window);
}

void gui_render_menu_bar(c64_t* c64, gui_state_t* gui_state, struct emulation_context_s* emu_context) {
    if (igBeginMainMenuBar()) {
        if (igBeginMenu("File", true)) {
            if (igMenuItem_Bool("Load ROM...", NULL, false, true)) {
                // TODO: Open file dialog
            }
            if (igMenuItem_Bool("Load Disk Image...", NULL, false, true)) {
                // TODO: Open file dialog
            }
            if (igMenuItem_Bool("Load Cartridge...", NULL, false, true)) {
                // TODO: Open file dialog
            }
            igSeparator();
            if (igMenuItem_Bool("Exit", NULL, false, true)) {
                g_should_quit = true;
            }
            igEndMenu();
        }
        
        if (igBeginMenu("Emulation", true)) {
            if (igMenuItem_Bool("Reset", NULL, false, emu_context != NULL)) {
                if (emu_context) {
                    gui_emulation_reset(emu_context);
                    printf("GUI: Reset signal sent\n");
                }
            }
            igSeparator();
              // Start/Pause button
            bool is_running = emu_context ? (emu_context->current_state == EMU_STATE_RUNNING) : gui_state->emulation_running;
            const char* run_pause_text = is_running ? "Pause" : "Start";
            if (igMenuItem_Bool(run_pause_text, NULL, false, emu_context != NULL)) {
                if (emu_context) {
                    if (is_running) {
                        gui_emulation_pause(emu_context);
                        printf("GUI: Pause signal sent\n");
                        gui_state->emulation_running = false;
                    } else {
                        gui_emulation_start(emu_context);
                        printf("GUI: Start signal sent\n");
                        gui_state->emulation_running = true;
                    }
                }
            }
            
            // Single step button
            if (igMenuItem_Bool("Single Step", NULL, false, emu_context != NULL && !gui_state->emulation_running)) {
                if (emu_context) {
                    gui_emulation_step(emu_context);
                    printf("GUI: Step signal sent\n");
                }
            }
            
            igSeparator();
            
            // Speed control
            if (emu_context) {
                static float speed_multiplier = 1.0f;
                if (igSliderFloat("Speed", &speed_multiplier, 0.1f, 5.0f, "%.1fx", ImGuiSliderFlags_None)) {
                    gui_emulation_set_speed(emu_context, speed_multiplier);
                }
            }
            
            igEndMenu();
        }
        
        if (igBeginMenu("View", true)) {
            igMenuItem_BoolPtr("Screen Display", NULL, &gui_state->show_screen, true);
            igMenuItem_BoolPtr("Memory Viewer", NULL, &gui_state->show_memory_viewer, true);
            igMenuItem_BoolPtr("Debugger", NULL, &gui_state->show_debugger, true);
            
            igEndMenu();
        }

        if (igBeginMenu("Chips", true)) {
            // Remove special handling for PLA debug since it's now handled by the chip system
            // Add chip debug windows organized by categories
            if (c64 && c64->system.chip_count > 0) {
                igSeparator();
                
                // Memory & Logic category
                if (igBeginMenu("Memory & Logic", true)) {
                    for (uint8_t chip_id = 0; chip_id < c64->system.chip_count && chip_id < 16; chip_id++) {
                        chip_entry_t* entry = &c64->system.chips[chip_id];
                        if (entry->desc && entry->desc->render_debug_window) {
                            // Check if this is a memory/logic chip (RAM, ROM, PLA)
                            const char* desc = entry->desc->description;
                            if (strstr(desc, "RAM") || strstr(desc, "ROM") || strstr(desc, "PLA") || strstr(desc, "Memory")) {
                                igPushID_Int(chip_id);
                                char menu_label[64];
                                snprintf(menu_label, sizeof(menu_label), "%s", desc);
                                igMenuItem_BoolPtr(menu_label, NULL, &gui_state->show_chip_debug[chip_id], true);
                                igPopID();
                            }
                        }
                    }
                    igEndMenu();
                }
                
                // I/O & Peripherals category
                if (igBeginMenu("I/O & Peripherals", true)) {
                    for (uint8_t chip_id = 0; chip_id < c64->system.chip_count && chip_id < 16; chip_id++) {
                        chip_entry_t* entry = &c64->system.chips[chip_id];
                        if (entry->desc && entry->desc->render_debug_window) {
                            // Check if this is an I/O or peripheral chip (VIC-II, SID, CIA)
                            const char* desc = entry->desc->description;
                            if (strstr(desc, "VIC") || strstr(desc, "SID") || strstr(desc, "CIA") || 
                                strstr(desc, "I/O") || strstr(desc, "Video") || strstr(desc, "Audio")) {
                                igPushID_Int(chip_id);
                                char menu_label[64];
                                snprintf(menu_label, sizeof(menu_label), "%s", desc);
                                igMenuItem_BoolPtr(menu_label, NULL, &gui_state->show_chip_debug[chip_id], true);
                                igPopID();
                            }
                        }
                    }
                    igEndMenu();
                }
            }
            
            igEndMenu();
        }

        if (igBeginMenu("Screen", true)) {
            igText("Display Controls");
            igSeparator();
            
            igSliderFloat("Scale", &gui_state->screen_scale, 0.5f, 4.0f, "%.1fx", ImGuiSliderFlags_None);
            igCheckbox("Filter", &gui_state->screen_filter);
            igCheckbox("Scanlines", &gui_state->screen_scanlines);
            
            // Update texture filtering based on user preference
            if (gui_state->screen_texture_id != 0) {
                glBindTexture(GL_TEXTURE_2D, gui_state->screen_texture_id);
                if (gui_state->screen_filter) {
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                } else {
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                }
            }
            
            igSeparator();
            igText("Aspect Ratio & Scaling");
            
            // Aspect ratio mode selection
            const char* aspect_ratio_items[] = {
                "Original", "4:3", "16:10", "16:9", "Custom", "Pixel Perfect"
            };
            int current_aspect = (int)gui_state->aspect_ratio_mode;
            if (igCombo_Str_arr("Aspect Ratio", &current_aspect, aspect_ratio_items, ASPECT_RATIO_COUNT, -1)) {
                gui_state->aspect_ratio_mode = (aspect_ratio_mode_t)current_aspect;
            }
            
            // Custom aspect ratio input (only shown when Custom is selected)
            if (gui_state->aspect_ratio_mode == ASPECT_RATIO_CUSTOM) {
                igSliderFloat("Custom Ratio", &gui_state->custom_aspect_ratio, 0.5f, 3.0f, "%.2f", ImGuiSliderFlags_None);
            }
            
            // Scaling mode selection
            const char* scaling_mode_items[] = {
                "Fit (Black Bars)", "Fill (Crop)", "Stretch", "Integer Scale"
            };
            int current_scaling = (int)gui_state->scaling_mode;
            if (igCombo_Str_arr("Scaling Mode", &current_scaling, scaling_mode_items, SCALING_MODE_COUNT, -1)) {
                gui_state->scaling_mode = (scaling_mode_t)current_scaling;
            }
            
            // Additional options
            igCheckbox("Maintain Pixel Aspect", &gui_state->maintain_pixel_aspect);
            igCheckbox("Show Overscan/Border", &gui_state->show_overscan);
            igCheckbox("Center Display", &gui_state->center_display);
            igCheckbox("Show Invisible Area", &gui_state->show_invisible_area);
            
            // Debug: VIC-II color cycling
            igSeparator();
            igText("Debug Features:");
            static bool color_cycle_enabled = false;
            igCheckbox("Cycle Background Color", &color_cycle_enabled);
            
            // Implement background color cycling
            if (color_cycle_enabled && c64 && c64->vicii) {
                static uint32_t last_cycle_time = 0;
                static uint8_t current_bg_color = 0;
                uint32_t current_time = SDL_GetTicks();
                
                if (current_time - last_cycle_time > 2000) { // Change every 2 seconds
                    // Write to VIC-II background color register - this affects the center area
                    vicii_common_registers_write(c64->vicii, VICII_B0C, current_bg_color);
                    // Also cycle border (exterior) color to be more visible
                    vicii_common_registers_write(c64->vicii, VICII_EC, (current_bg_color + 8) % 16);
                    printf("DEBUG: Set VIC-II background color 0 (center) to %d, border to %d\n",
                           current_bg_color, (current_bg_color + 8) % 16);
                    
                    current_bg_color = (current_bg_color + 1) % 16;
                    last_cycle_time = current_time;
                }
                igText("Current BG Color: %d", (current_bg_color + 15) % 16); // Show the current one
            }
            
            // Host DPI information
            igSeparator();
            igText("Host DPI Scale: %.2f", gui_state->host_dpi_scale);
            
            igSeparator();
            
            // Display information - use the constants that are defined later in the file
            igText("Resolution: 320x200");
            igText("With border: 403x284");
            
            if (c64) {
                igText("Frame: %llu", c64->total_cycles / 20000);
            }
            
            igEndMenu();
        }

        if (igBeginMenu("Chips", true)) {
            if (c64 && c64->system.chip_count > 0) {
                if (igBeginMenu("Debug Windows", true)) {
                    for (uint8_t chip_id = 0; chip_id < c64->system.chip_count && chip_id < 16; chip_id++) {
                        chip_entry_t* entry = &c64->system.chips[chip_id];
                        if (entry->desc && entry->desc->render_debug_window) {
                            igPushID_Int(chip_id); // Push unique ID for each chip
                            char menu_label[64];
                            snprintf(menu_label, sizeof(menu_label), "%s Debug", entry->desc->description);
                            igMenuItem_BoolPtr(menu_label, NULL, &gui_state->show_chip_debug[chip_id], true);
                            igPopID(); // Pop chip ID
                        }
                    }
                    igEndMenu();
                }
                
                if (igBeginMenu("Settings Windows", true)) {
                    for (uint8_t chip_id = 0; chip_id < c64->system.chip_count && chip_id < 16; chip_id++) {
                        chip_entry_t* entry = &c64->system.chips[chip_id];
                        if (entry->desc && entry->desc->render_settings_window) {
                            igPushID_Int(chip_id); // Push unique ID for each chip
                            char menu_label[64];
                            snprintf(menu_label, sizeof(menu_label), "%s Settings", entry->desc->description);
                            igMenuItem_BoolPtr(menu_label, NULL, &gui_state->show_chip_settings[chip_id], true);
                            igPopID(); // Pop chip ID
                        }
                    }
                    igEndMenu();
                }
            } else {
                igMenuItem_Bool("No chips available", NULL, false, false);
            }
            igEndMenu();
        }
        
        if (igBeginMenu("Settings", true)) {
            igMenuItem_BoolPtr("Preferences", NULL, &gui_state->show_settings, true);
            igEndMenu();
        }
        
        if (igBeginMenu("Help", true)) {
            igMenuItem_BoolPtr("About", NULL, &gui_state->show_about, true);
            igEndMenu();
        }
          // Status bar on the right
        igSameLine(igGetWindowWidth() - 300, -1.0f);
        igText("Cycles: %llu", c64 ? c64->total_cycles : 0);
        igSameLine(0, -1.0f);
        
        // Show actual emulation state from context if available
        if (emu_context) {
            const char* state_text = 
                emu_context->current_state == EMU_STATE_RUNNING ? "Running" :
                emu_context->current_state == EMU_STATE_PAUSED ? "Paused" :
                emu_context->current_state == EMU_STATE_STEPPING ? "Stepping" :
                emu_context->current_state == EMU_STATE_STOPPED ? "Stopped" : "Unknown";
            igText("%s", state_text);
        } else {
            igText("%s", gui_state->emulation_running ? "Running" : "Paused");
        }
        
        igEndMainMenuBar();
    }
}

void gui_render_memory_viewer(c64_t* c64, gui_state_t* gui_state) {
    if (!igBegin("Memory Viewer", &gui_state->show_memory_viewer, 0)) {
        igEnd();
        return;
    }

    // Address input
    igInputInt("Address", (int*)&gui_state->memory_address, 1, 16, ImGuiInputTextFlags_CharsHexadecimal);
    gui_state->memory_address &= 0xFFFF; // Keep within 16-bit range
    
    igSliderInt("Columns", &gui_state->memory_columns, 8, 32, "%d", ImGuiSliderFlags_None);
    
    igSeparator();
    
    // Memory display
    if (c64) {
        ImVec2 child_size = {0, 0};
        igBeginChild_Str("MemoryData", child_size, true, 0);
        
        for (int row = 0; row < 16; row++) {
            uint16_t addr = gui_state->memory_address + (row * gui_state->memory_columns);
            igText("%04X: ", addr);
            
            for (int col = 0; col < gui_state->memory_columns; col++) {
                uint16_t byte_addr = addr + col;
                if (col >= gui_state->memory_columns || addr + col > 0xFFFF) break;
                // Read memory through proper C64 bus mapping
                uint8_t byte_value = c64->bus ? c64_bus_memory_read(c64->bus, byte_addr) : 0x00;
                
                igSameLine(0, -1.0f);
                igText("%02X", byte_value);
            }
        }
        
        igEndChild();
    } else {
        igText("C64 system not initialized");
    }

    igEnd();
}

void gui_render_debugger(c64_t* c64, gui_state_t* gui_state, emulation_context_t* emu_context) {
    if (!igBegin("Debugger", &gui_state->show_debugger, 0)) {
        igEnd();
        return;
    }
    // Emulation state display
    if (emu_context) {
        igText("Emulation State: %s",
               emu_context->current_state == EMU_STATE_RUNNING ? "Running" :
               emu_context->current_state == EMU_STATE_PAUSED ? "Paused" :
               emu_context->current_state == EMU_STATE_STEPPING ? "Stepping" :
               emu_context->current_state == EMU_STATE_STOPPED ? "Stopped" : "Unknown");
        igText("FPS: %u", emu_context->actual_fps);
        igText("Total Cycles: %llu", emu_context->total_cycles_executed);
        igSeparator();
        
        // System initialization status
        igText("System Status:");
        if (c64) {
            igText("CPU: %s", c64->mos6510 ? "Initialized" : "NOT INITIALIZED");
            igText("Bus: %s", c64->bus ? "Attached" : "NOT ATTACHED");
            igText("RAM: %s", c64->ram ? "Available" : "NOT AVAILABLE");
              // Check reset vector
            if (c64 && c64->bus) {
                // Read reset vector through proper memory mapping (ROM or RAM depending on banking)
                uint8_t reset_low = c64_bus_memory_read(c64->bus, 0xFFFC);
                uint8_t reset_high = c64_bus_memory_read(c64->bus, 0xFFFD);
                uint16_t reset_vector = (reset_high << 8) | reset_low;
                igText("Reset Vector: $%04X %s", reset_vector, 
                       reset_vector == 0x0000 ? "(NO ROM)" : "(ROM LOADED)");
            } else {
                igText("Reset Vector: N/A");
            }
        } else {
            igText("C64 System: NOT INITIALIZED");
        }
        igSeparator();
    }

    // Execution controls
    igText("Execution Control");
      if (emu_context) {
        // Start/Pause button
        bool is_running = (emu_context->current_state == EMU_STATE_RUNNING);
        const char* run_pause_text = is_running ? "Pause" : "Run";
        if (igButton(run_pause_text, (ImVec2){60, 0})) {
            if (is_running) {
                gui_emulation_pause(emu_context);
                printf("Debugger: Pause signal sent\n");
            } else {
                gui_emulation_start(emu_context);
                printf("Debugger: Start signal sent\n");
            }
        }
        
        igSameLine(0, -1.0f);
        if (igButton("Step", (ImVec2){60, 0})) {
            gui_emulation_step(emu_context);
            printf("Debugger: Step signal sent\n");
        }
        
        igSameLine(0, -1.0f);
        if (igButton("Reset", (ImVec2){60, 0})) {
            gui_emulation_reset(emu_context);
            printf("Debugger: Reset signal sent\n");
        }
    } else {
        igText("Emulation context not available");
    }
    
    igSeparator();
    
    // Breakpoint controls
    igText("Breakpoints");
    igInputInt("Address", (int*)&gui_state->breakpoint_address, 1, 16, ImGuiInputTextFlags_CharsHexadecimal);
    gui_state->breakpoint_address &= 0xFFFF;
    
    igSameLine(0, -1.0f);
    if (igButton("Add", (ImVec2){0, 0})) {
        gui_state->breakpoint_enabled = true;
        // TODO: Set breakpoint in emulator
        printf("Breakpoint set at $%04X\n", gui_state->breakpoint_address);
    }
    
    igSameLine(0, -1.0f);
    if (igButton("Clear All", (ImVec2){0, 0})) {
        gui_state->breakpoint_enabled = false;
        // TODO: Clear all breakpoints
        printf("All breakpoints cleared\n");
    }
    
    igSeparator();
    
    // Disassembly view
    igText("Disassembly");
    ImVec2 child_size = {0, 0};
    igBeginChild_Str("DisassemblyView", child_size, true, 0);
    
    if (c64) {
        // TODO: Show disassembled code around current PC
        for (int i = 0; i < 20; i++) {
            uint16_t addr = 0x8000 + i;
            igText("%04X: LDA #$42", addr);
        }
    } else {
        igText("C64 system not initialized");
    }
    
    igEndChild();

    igEnd();
}

void gui_render_settings(c64_t* c64, gui_state_t* gui_state) {
    if (!igBegin("Settings", &gui_state->show_settings, 0)) {
        igEnd();
        return;
    }

    igText("Emulation Settings");
    igSeparator();
    
    igSliderInt("Target FPS", &gui_state->target_fps, 25, 120, "%d", ImGuiSliderFlags_None);
    igSliderFloat("Emulation Speed", &gui_state->emulation_speed, 0.1f, 10.0f, "%.1fx", ImGuiSliderFlags_None);
    
    igSeparator();
    igText("ROM Files");
    
    igInputText("BASIC ROM", gui_state->rom_path_basic, sizeof(gui_state->rom_path_basic), 0, NULL, NULL);
    igSameLine(0, -1.0f);
    if (igButton("Browse##basic", (ImVec2){0, 0})) {
        // TODO: File dialog
    }
    
    igInputText("KERNAL ROM", gui_state->rom_path_kernal, sizeof(gui_state->rom_path_kernal), 0, NULL, NULL);
    igSameLine(0, -1.0f);
    if (igButton("Browse##kernal", (ImVec2){0, 0})) {
        // TODO: File dialog
    }
    
    igInputText("Character ROM", gui_state->rom_path_chargen, sizeof(gui_state->rom_path_chargen), 0, NULL, NULL);
    igSameLine(0, -1.0f);
    if (igButton("Browse##chargen", (ImVec2){0, 0})) {
        // TODO: File dialog
    }
      igSeparator();
    
    if (igButton("Reload ROMs", (ImVec2){0, 0})) {
        if (c64) {
            if (gui_reload_roms_from_state(c64, gui_state)) {
                printf("ROMs reloaded successfully from GUI settings\n");
            } else {
                printf("Failed to reload ROMs from GUI settings\n");
            }
        } else {
            printf("Cannot reload ROMs: C64 system not initialized\n");
        }
    }
    igSameLine(0, -1.0f);
    if (igButton("Apply Settings", (ImVec2){0, 0})) {
        // TODO: Apply other settings to emulator
    }
    igSameLine(0, -1.0f);
    if (igButton("Reset to Defaults", (ImVec2){0, 0})) {
        gui_init_state(gui_state);
    }

    igEnd();
}

void gui_render_about(gui_state_t* gui_state) {
    if (!igBegin("About", &gui_state->show_about, 0)) {
        igEnd();
        return;
    }

    igText("C64 Emulator");
    igText("Version 0.1.0");
    igSeparator();
    
    igText("A cycle-accurate Commodore 64 emulator");
    igText("Built with Dear ImGui and SDL2");
    
    igSeparator();
    
    igText("Features:");
    igBulletText("MOS 6510 CPU emulation");
    igBulletText("VIC-II graphics chip");
    igBulletText("SID sound chip");
    igBulletText("CIA I/O chips");
    igBulletText("Complete memory mapping");
    
    igSeparator();
    
    if (igButton("Close", (ImVec2){0, 0})) {
        gui_state->show_about = false;
    }

    igEnd();
}

// ============================================================================
// SCREEN DISPLAY IMPLEMENTATION
// ============================================================================

// C64 display constants
#define C64_SCREEN_WIDTH  320
#define C64_SCREEN_HEIGHT 200
#define C64_VISIBLE_WIDTH   403  // VIC-II visible area (including borders)
#define C64_VISIBLE_HEIGHT  284  // VIC-II visible area (including borders)
#define C64_TOTAL_WIDTH   512   // Full framebuffer width (centered VIC-II area)
#define C64_TOTAL_HEIGHT  384   // Full framebuffer height (centered VIC-II area)

// VIC-II area positioning within the larger framebuffer
#define VIC_OFFSET_X  ((C64_TOTAL_WIDTH - C64_VISIBLE_WIDTH) / 2)   // Center horizontally
#define VIC_OFFSET_Y  ((C64_TOTAL_HEIGHT - C64_VISIBLE_HEIGHT) / 2) // Center vertically

// Simple C64 color palette (16 colors)
static const uint32_t c64_palette[16] = {
    0xFF000000,  // 0: Black
    0xFFFFFFFF,  // 1: White  
    0xFF880000,  // 2: Red
    0xFFAAFFEE,  // 3: Cyan
    0xFFCC44CC,  // 4: Purple
    0xFF00CC55,  // 5: Green
    0xFF0000AA,  // 6: Blue
    0xFFEEEE77,  // 7: Yellow
    0xFFDD8855,  // 8: Orange
    0xFF664400,  // 9: Brown
    0xFFFF7777,  // 10: Light Red
    0xFF333333,  // 11: Dark Grey
    0xFF777777,  // 12: Grey
    0xFFAAFF66,  // 13: Light Green
    0xFF0088FF,  // 14: Light Blue
    0xFFBBBBBB   // 15: Light Grey
};

// Static framebuffer for C64 screen (double buffering handled at VIC-II level)
static uint32_t screen_buffer[C64_TOTAL_WIDTH * C64_TOTAL_HEIGHT];
// Remove vic_buffer - VIC-II will render directly to centered area of screen_buffer

bool gui_init_screen_display(gui_state_t* gui_state) {
    // Generate OpenGL texture
    glGenTextures(1, &gui_state->screen_texture_id);
    glBindTexture(GL_TEXTURE_2D, gui_state->screen_texture_id);
    
    // Set texture parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    
    // Initialize with completely black screen (no test patterns)
    memset(screen_buffer, 0, sizeof(screen_buffer));
    
    // Upload initial black data to texture
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, C64_TOTAL_WIDTH, C64_TOTAL_HEIGHT,
                 0, GL_RGBA, GL_UNSIGNED_BYTE, screen_buffer);
    
    // Initialize screen display state
    gui_state->screen_scale = 2.0f;
    gui_state->screen_filter = false;
    gui_state->screen_scanlines = false;
    
    return true;
}

void gui_cleanup_screen_display(gui_state_t* gui_state) {
    if (gui_state->screen_texture_id != 0) {
        glDeleteTextures(1, &gui_state->screen_texture_id);
        gui_state->screen_texture_id = 0;
    }
}

// Re-add vic_buffer but as a temporary solution until VIC-II can handle stride
static uint32_t vic_buffer[C64_VISIBLE_WIDTH * C64_VISIBLE_HEIGHT];

void gui_update_screen_texture(c64_t* c64, gui_state_t* gui_state) {
    static void* connected_vic_chip = NULL;  // Keep track of connected VIC chip
    
    // Initialize screen buffer with appropriate background color
    // When showing invisible area, use brownish color (VIC-II generates color 9 = brown in invisible area)
    uint32_t background_fill = 0x00000000; // Default black
    if (gui_state->show_invisible_area) {
        // Use color 9 (brown) from VIC-II palette for invisible area
        // This is what real VIC-II hardware shows in the invisible area
        background_fill = RGBA_COLOR(0x43, 0x39, 0x00, 0xFF); // Brown color
    }
    
    // Fill entire buffer with appropriate background
    for (int i = 0; i < C64_TOTAL_WIDTH * C64_TOTAL_HEIGHT; i++) {
        screen_buffer[i] = background_fill;
    }
    
    if (c64 && c64->vicii) {
        // Set up framebuffer connection if not already done or chip changed
        if (connected_vic_chip != c64->vicii) {
            // Connect VIC-II to the VIC-II sized buffer (403x284)
            vicii_common_set_framebuffer((vicii_common_t*)c64->vicii,
                                        vic_buffer, C64_VISIBLE_WIDTH, C64_VISIBLE_HEIGHT);
            connected_vic_chip = c64->vicii;
            printf("GUI: Connected VIC-II to buffer (%dx%d)\n", C64_VISIBLE_WIDTH, C64_VISIBLE_HEIGHT);
        }
        
        // Copy VIC-II content to properly centered position in full framebuffer
        for (int y = 0; y < C64_VISIBLE_HEIGHT; y++) {
            for (int x = 0; x < C64_VISIBLE_WIDTH; x++) {
                int vic_idx = y * C64_VISIBLE_WIDTH + x;
                int screen_x = VIC_OFFSET_X + x;
                int screen_y = VIC_OFFSET_Y + y;
                int screen_idx = screen_y * C64_TOTAL_WIDTH + screen_x;
                
                if (screen_x < C64_TOTAL_WIDTH && screen_y < C64_TOTAL_HEIGHT &&
                    screen_idx < C64_TOTAL_WIDTH * C64_TOTAL_HEIGHT &&
                    vic_idx < C64_VISIBLE_WIDTH * C64_VISIBLE_HEIGHT) {
                    screen_buffer[screen_idx] = vic_buffer[vic_idx];
                }
            }
        }
    }
    
    // Always upload the full screen buffer to OpenGL texture
    glBindTexture(GL_TEXTURE_2D, gui_state->screen_texture_id);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, C64_TOTAL_WIDTH, C64_TOTAL_HEIGHT,
                    GL_RGBA, GL_UNSIGNED_BYTE, screen_buffer);
}

void gui_render_screen(c64_t* c64, gui_state_t* gui_state) {
    // Get the main viewport to create a fullscreen background window
    const ImGuiViewport* viewport = igGetMainViewport();
    
    // Detect and update host DPI scale if needed
    ImGuiIO* io = igGetIO_Nil();
    if (gui_state->host_dpi_scale <= 0.0f) {
        gui_state->host_dpi_scale = io->DisplayFramebufferScale.x > 0.0f ? io->DisplayFramebufferScale.x : 1.0f;
    }
    
    // Set window position and size to cover the entire viewport
    igSetNextWindowPos(viewport->Pos, ImGuiCond_Always, (ImVec2){0, 0});
    igSetNextWindowSize(viewport->Size, ImGuiCond_Always);
    
    // Window flags for a fullscreen background window
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDecoration |
                                   ImGuiWindowFlags_NoMove |
                                   ImGuiWindowFlags_NoResize |
                                   ImGuiWindowFlags_NoSavedSettings |
                                   ImGuiWindowFlags_NoBringToFrontOnFocus |
                                   ImGuiWindowFlags_NoFocusOnAppearing |
                                   ImGuiWindowFlags_NoNav |
                                   ImGuiWindowFlags_NoBackground;
    
    // Push window to background (behind all other windows)
    igPushStyleVar_Float(ImGuiStyleVar_WindowRounding, 0.0f);
    igPushStyleVar_Float(ImGuiStyleVar_WindowBorderSize, 0.0f);
    
    if (igBegin("##C64ScreenBackground", NULL, window_flags)) {
        // Determine if this is PAL or NTSC (check if c64 system is available)
        bool is_pal = true;  // Default to PAL
        if (c64 && c64->system.chip_count > 0) {
            // Try to detect VIC-II type from chip system
            for (uint8_t i = 0; i < c64->system.chip_count; i++) {
                chip_entry_t* entry = &c64->system.chips[i];
                if (entry->desc && strstr(entry->desc->description, "6567")) {
                    is_pal = false;  // NTSC
                    break;
                } else if (entry->desc && strstr(entry->desc->description, "6569")) {
                    is_pal = true;   // PAL
                    break;
                }
            }
        }
        
        // Get guest display dimensions
        float guest_width, guest_height;
        gui_get_guest_dimensions(gui_state, is_pal, &guest_width, &guest_height);
        
        // Calculate display dimensions and position using new aspect ratio system
        float display_width, display_height, pos_x, pos_y;
        gui_calculate_display_dimensions(gui_state, viewport->Size.x, viewport->Size.y,
                                       guest_width, guest_height, is_pal,
                                       &display_width, &display_height, &pos_x, &pos_y);
        
        // Set cursor position
        igSetCursorPos((ImVec2){pos_x, pos_y});
        
        // Render the C64 screen texture
        ImTextureID tex_id = (ImTextureID)(intptr_t)gui_state->screen_texture_id;
        ImVec2 image_size = {display_width, display_height};
        
        // Calculate UV coordinates based on display settings
        ImVec2 uv_min = {0, 0};
        ImVec2 uv_max = {1, 1};
        
        if (!gui_state->show_invisible_area) {
            // Default: Show only VIC-II visible area (crop the invisible area)
            float u_offset = (float)VIC_OFFSET_X / (float)C64_TOTAL_WIDTH;
            float v_offset = (float)VIC_OFFSET_Y / (float)C64_TOTAL_HEIGHT;
            float u_scale = (float)C64_VISIBLE_WIDTH / (float)C64_TOTAL_WIDTH;
            float v_scale = (float)C64_VISIBLE_HEIGHT / (float)C64_TOTAL_HEIGHT;
            
            uv_min.x = u_offset;
            uv_min.y = v_offset;
            uv_max.x = u_offset + u_scale;
            uv_max.y = v_offset + v_scale;
        }
        // When show_invisible_area is true, use full texture (uv_min/max = 0,0 to 1,1)
        
        if (gui_state->screen_scanlines) {
            // TODO: Implement scanline shader effect
            // For now, just draw the image normally
            igImage(tex_id, image_size, uv_min, uv_max);
        } else {
            igImage(tex_id, image_size, uv_min, uv_max);
        }
        // Handle mouse interaction with fullscreen screen
        if (igIsItemHovered(ImGuiHoveredFlags_None)) {
            ImVec2 mouse_pos;
            igGetMousePos(&mouse_pos);
            ImVec2 image_min;
            igGetItemRectMin(&image_min);
            
            // Calculate relative position within the displayed image
            float rel_x = (mouse_pos.x - image_min.x) / display_width;
            float rel_y = (mouse_pos.y - image_min.y) / display_height;
            
            // Convert to guest screen coordinates
            int guest_x = (int)(rel_x * guest_width);
            int guest_y = (int)(rel_y * guest_height);
            
            if (rel_x >= 0.0f && rel_x <= 1.0f && rel_y >= 0.0f && rel_y <= 1.0f &&
                guest_x >= 0 && guest_x < (int)guest_width &&
                guest_y >= 0 && guest_y < (int)guest_height) {
                
                if (gui_state->show_overscan) {
                    igSetTooltip("C64 Screen coordinates: (%d, %d)", guest_x, guest_y);
                } else {
                    igSetTooltip("C64 Active area: (%d, %d)", guest_x, guest_y);
                }
            }
        }
    }
    igEnd();
    
    igPopStyleVar(2); // Pop WindowRounding and WindowBorderSize
}

// ============================================================================
// ASPECT RATIO CALCULATION IMPLEMENTATION
// ============================================================================

// Get guest display dimensions based on current settings
void gui_get_guest_dimensions(gui_state_t* gui_state, bool is_pal,
                             float* out_width, float* out_height) {
    if (gui_state->show_invisible_area) {
        // Show full framebuffer including invisible area
        *out_width = (float)C64_TOTAL_WIDTH;
        *out_height = (float)C64_TOTAL_HEIGHT;
    } else {
        // Default: Show only VIC-II visible area
        *out_width = (float)C64_VISIBLE_WIDTH;
        *out_height = (float)C64_VISIBLE_HEIGHT;
    }
}

// Get target aspect ratio based on configuration
float gui_get_target_aspect_ratio(gui_state_t* gui_state, bool is_pal) {
    switch (gui_state->aspect_ratio_mode) {
        case ASPECT_RATIO_4_3:
            return 4.0f / 3.0f;
            
        case ASPECT_RATIO_16_10:
            return 16.0f / 10.0f;
            
        case ASPECT_RATIO_16_9:
            return 16.0f / 9.0f;
            
        case ASPECT_RATIO_CUSTOM:
            return gui_state->custom_aspect_ratio;
            
        case ASPECT_RATIO_PIXEL_PERFECT:
            return 1.0f;  // Square pixels
            
        case ASPECT_RATIO_ORIGINAL:
        default: {
            // Calculate original C64 aspect ratio
            float guest_width, guest_height;
            gui_get_guest_dimensions(gui_state, is_pal, &guest_width, &guest_height);
            
            // C64 pixels are not square - they have different aspect ratios for PAL vs NTSC
            float pixel_aspect = is_pal ? (312.0f / 50.0f) / (263.0f / 60.0f) : 1.0f;
            
            if (gui_state->maintain_pixel_aspect) {
                return (guest_width / guest_height) * pixel_aspect;
            } else {
                return guest_width / guest_height;
            }
        }
    }
}

// Calculate display dimensions and position with aspect ratio and scaling
void gui_calculate_display_dimensions(gui_state_t* gui_state, float viewport_width, float viewport_height,
                                     float guest_width, float guest_height, bool is_pal,
                                     float* out_display_width, float* out_display_height,
                                     float* out_pos_x, float* out_pos_y) {
    
    // Apply host DPI scaling
    float effective_viewport_width = viewport_width / gui_state->host_dpi_scale;
    float effective_viewport_height = viewport_height / gui_state->host_dpi_scale;
    
    // Get target aspect ratio
    float target_aspect = gui_get_target_aspect_ratio(gui_state, is_pal);
    float viewport_aspect = effective_viewport_width / effective_viewport_height;
    
    float display_width, display_height;
    
    switch (gui_state->scaling_mode) {
        case SCALING_MODE_FILL:
            // Fill entire viewport (may crop guest content)
            display_width = effective_viewport_width;
            display_height = effective_viewport_height;
            break;
            
        case SCALING_MODE_STRETCH:
            // Stretch to fill viewport (may distort aspect ratio)
            display_width = effective_viewport_width;
            display_height = effective_viewport_height;
            break;
            
        case SCALING_MODE_INTEGER: {
            // Use integer scaling only
            float max_scale_x = effective_viewport_width / guest_width;
            float max_scale_y = effective_viewport_height / guest_height;
            float integer_scale = floorf(fminf(max_scale_x, max_scale_y));
            
            if (integer_scale < 1.0f) integer_scale = 1.0f;
            
            display_width = guest_width * integer_scale;
            display_height = guest_height * integer_scale;
            break;
        }
        
        case SCALING_MODE_FIT:
        default: {
            // Fit within viewport maintaining aspect ratio (may add black bars)
            if (viewport_aspect > target_aspect) {
                // Viewport is wider - fit to height, add side bars
                display_height = effective_viewport_height;
                display_width = display_height * target_aspect;
            } else {
                // Viewport is taller - fit to width, add top/bottom bars
                display_width = effective_viewport_width;
                display_height = display_width / target_aspect;
            }
            break;
        }
    }
    
    // Apply user scaling
    display_width *= gui_state->screen_scale;
    display_height *= gui_state->screen_scale;
    
    // Clamp to viewport size if needed
    if (display_width > effective_viewport_width) {
        float scale_factor = effective_viewport_width / display_width;
        display_width = effective_viewport_width;
        display_height *= scale_factor;
    }
    if (display_height > effective_viewport_height) {
        float scale_factor = effective_viewport_height / display_height;
        display_height = effective_viewport_height;
        display_width *= scale_factor;
    }
    
    // Calculate position (center by default)
    float pos_x = 0.0f;
    float pos_y = 0.0f;
    
    if (gui_state->center_display) {
        pos_x = (effective_viewport_width - display_width) * 0.5f;
        pos_y = (effective_viewport_height - display_height) * 0.5f;
    }
    
    // Apply DPI scaling back to final values
    *out_display_width = display_width * gui_state->host_dpi_scale;
    *out_display_height = display_height * gui_state->host_dpi_scale;
    *out_pos_x = pos_x * gui_state->host_dpi_scale;
    *out_pos_y = pos_y * gui_state->host_dpi_scale;
}

// ============================================================================
// ROM LOADING IMPLEMENTATION
// ============================================================================

// Helper function to reload ROMs when new paths are provided
bool gui_apply_rom_changes(emulation_context_t* emu_context, gui_state_t* gui_state) {
    if (!emu_context || !emu_context->c64 || !gui_state) {
        return false;
    }
    
    // Check if any ROM paths have changed (simplified - just reload unconditionally for now)
    return gui_reload_roms_from_state(emu_context->c64, gui_state);
}

// Updated ROM loading function that updates GUI state
void gui_load_rom_file(const char* filepath, const char* type, gui_state_t* gui_state, emulation_context_t* emu_context) {
    if (!filepath || !type || !gui_state) {
        printf("Invalid ROM loading parameters\n");
        return;
    }
    
    printf("ROM loading requested: %s (%s)\n", filepath, type);
    
    // Update the appropriate ROM path in GUI state
    if (strcmp(type, "basic") == 0) {
        strncpy(gui_state->rom_path_basic, filepath, sizeof(gui_state->rom_path_basic) - 1);
        gui_state->rom_path_basic[sizeof(gui_state->rom_path_basic) - 1] = '\0';
    } else if (strcmp(type, "kernal") == 0) {
        strncpy(gui_state->rom_path_kernal, filepath, sizeof(gui_state->rom_path_kernal) - 1);
        gui_state->rom_path_kernal[sizeof(gui_state->rom_path_kernal) - 1] = '\0';
    } else if (strcmp(type, "chargen") == 0) {
        strncpy(gui_state->rom_path_chargen, filepath, sizeof(gui_state->rom_path_chargen) - 1);
        gui_state->rom_path_chargen[sizeof(gui_state->rom_path_chargen) - 1] = '\0';
    } else {
        printf("Unknown ROM type: %s\n", type);
        return;
    }
    
    // If emulation context is available, reload ROMs immediately
    if (emu_context) {
        if (gui_apply_rom_changes(emu_context, gui_state)) {
            printf("ROM reloaded successfully: %s\n", filepath);
        } else {
            printf("Failed to reload ROM: %s\n", filepath);
        }
    } else {
        printf("ROM path updated, will be loaded when emulation starts\n");
    }
}

void gui_load_disk_image(const char* filepath) {
    if (!filepath) {
        printf("Invalid disk image path\n");
        return;
    }
    
    printf("Disk image loading requested: %s\n", filepath);
    // TODO: Implement disk image loading
}

// Helper function to reload ROMs from GUI state
bool gui_reload_roms_from_state(c64_t* c64, const gui_state_t* gui_state) {
    if (!c64 || !gui_state) {
        return false;
    }
    
    // For now, we'll extract just the filename from the full path stored in GUI state
    // and create a custom ROM configuration
    char basic_filename[256] = {0};
    char kernal_filename[256] = {0};
    char chargen_filename[256] = {0};
    
    // Extract filename from full path (find last path separator)
    const char* basic_name = strrchr(gui_state->rom_path_basic, '/');
    if (!basic_name) basic_name = strrchr(gui_state->rom_path_basic, '\\');
    if (basic_name) {
        strncpy(basic_filename, basic_name + 1, sizeof(basic_filename) - 1);
    } else {
        strncpy(basic_filename, gui_state->rom_path_basic, sizeof(basic_filename) - 1);
    }
    
    const char* kernal_name = strrchr(gui_state->rom_path_kernal, '/');
    if (!kernal_name) kernal_name = strrchr(gui_state->rom_path_kernal, '\\');
    if (kernal_name) {
        strncpy(kernal_filename, kernal_name + 1, sizeof(kernal_filename) - 1);
    } else {
        strncpy(kernal_filename, gui_state->rom_path_kernal, sizeof(kernal_filename) - 1);
    }
    
    const char* chargen_name = strrchr(gui_state->rom_path_chargen, '/');
    if (!chargen_name) chargen_name = strrchr(gui_state->rom_path_chargen, '\\');
    if (chargen_name) {
        strncpy(chargen_filename, chargen_name + 1, sizeof(chargen_filename) - 1);
    } else {
        strncpy(chargen_filename, gui_state->rom_path_chargen, sizeof(chargen_filename) - 1);
    }
    
    // Create a custom ROM configuration from GUI state
    rom_config_t custom_rom_config = {
        .basic_rom_filenames = {
            basic_filename,
            NULL, NULL, NULL, NULL
        },
        .kernal_rom_filenames = {
            kernal_filename,
            NULL, NULL, NULL, NULL
        },
        .chargen_rom_filenames = {
            chargen_filename,
            NULL, NULL, NULL, NULL, NULL
        }
    };
    
    // Reload ROMs using the new configuration
    return c64_reload_roms(c64, &custom_rom_config);
}

// ============================================================================
// EMULATION THREADING IMPLEMENTATION (SDL-based)
// ============================================================================

#include <SDL_thread.h>
#include <SDL_mutex.h>
#include <SDL_timer.h>

// PAL C64 timing constants
#define PAL_CYCLES_PER_SECOND   985248
#define PAL_CYCLES_PER_FRAME    19705  // 985248 / 50 FPS
#define PAL_TARGET_FPS          50

// SDL-specific thread implementation
typedef struct {
    SDL_Thread* thread;
    SDL_mutex* signal_mutex;
    SDL_cond* signal_condition;
} sdl_thread_impl_t;

// Forward declarations
static int gui_emulation_thread_main(void* data);
static void gui_emulation_process_signal(emulation_context_t* context, emulation_signal_t signal);
static void gui_emulation_run_frame(emulation_context_t* context);

bool gui_emulation_thread_init(emulation_context_t* context, struct c64_s* c64) {
    if (!context || !c64) return false;
    
    // Initialize context
    context->c64 = c64;
    context->pending_signal = EMU_SIGNAL_NONE;
    context->current_state = EMU_STATE_STOPPED;
    context->thread_running = false;
    
    // Set PAL timing by default
    context->cycles_per_second = PAL_CYCLES_PER_SECOND;
    context->frame_cycles = PAL_CYCLES_PER_FRAME;
    context->target_fps = PAL_TARGET_FPS;
    
    // Initialize statistics
    context->total_cycles_executed = 0;
    context->frames_rendered = 0;
    context->actual_fps = 0;
    
    // Create SDL-specific implementation
    sdl_thread_impl_t* impl = (sdl_thread_impl_t*)calloc(1, sizeof(sdl_thread_impl_t));
    if (!impl) return false;
    
    impl->signal_mutex = SDL_CreateMutex();
    if (!impl->signal_mutex) {
        free(impl);
        return false;
    }
    
    impl->signal_condition = SDL_CreateCond();
    if (!impl->signal_condition) {
        SDL_DestroyMutex(impl->signal_mutex);
        free(impl);
        return false;
    }
    
    context->thread_impl = impl;
    return true;
}

void gui_emulation_thread_cleanup(emulation_context_t* context) {
    if (!context || !context->thread_impl) return;
    
    sdl_thread_impl_t* impl = (sdl_thread_impl_t*)context->thread_impl;
    
    // Stop thread if running
    gui_emulation_thread_stop(context);
    
    // Cleanup SDL objects
    if (impl->signal_condition) {
        SDL_DestroyCond(impl->signal_condition);
    }
    if (impl->signal_mutex) {
        SDL_DestroyMutex(impl->signal_mutex);
    }
    
    free(impl);
    context->thread_impl = NULL;
}

bool gui_emulation_thread_start(emulation_context_t* context) {
    if (!context || !context->thread_impl) return false;
    
    sdl_thread_impl_t* impl = (sdl_thread_impl_t*)context->thread_impl;
    if (impl->thread) return false; // Already running
    
    context->thread_running = true;
    impl->thread = SDL_CreateThread(gui_emulation_thread_main, "C64Emulation", context);
    
    if (!impl->thread) {
        context->thread_running = false;
        return false;
    }
    
    return true;
}

void gui_emulation_thread_stop(emulation_context_t* context) {
    if (!context || !context->thread_impl) return;
    
    sdl_thread_impl_t* impl = (sdl_thread_impl_t*)context->thread_impl;
    if (!impl->thread) return;
    
    // Signal thread to quit
    gui_emulation_send_signal(context, EMU_SIGNAL_QUIT);
    
    // Wait for thread to finish
    SDL_WaitThread(impl->thread, NULL);
    impl->thread = NULL;
}

void gui_emulation_send_signal(emulation_context_t* context, emulation_signal_t signal) {
    if (!context || !context->thread_impl) return;
    
    sdl_thread_impl_t* impl = (sdl_thread_impl_t*)context->thread_impl;
    
    SDL_LockMutex(impl->signal_mutex);
    context->pending_signal = signal;
    SDL_CondSignal(impl->signal_condition);
    SDL_UnlockMutex(impl->signal_mutex);
}

emulation_state_t gui_emulation_get_state(emulation_context_t* context) {
    if (!context) return EMU_STATE_STOPPED;
    return context->current_state;
}

void gui_emulation_set_speed(emulation_context_t* context, float speed_multiplier) {
    if (!context || !context->thread_impl) return;
    
    sdl_thread_impl_t* impl = (sdl_thread_impl_t*)context->thread_impl;
    
    SDL_LockMutex(impl->signal_mutex);
    context->cycles_per_second = (uint64_t)(PAL_CYCLES_PER_SECOND * speed_multiplier);
    context->frame_cycles = (uint64_t)(PAL_CYCLES_PER_FRAME * speed_multiplier);
    SDL_UnlockMutex(impl->signal_mutex);
}

uint32_t gui_emulation_get_fps(emulation_context_t* context) {
    if (!context) return 0;
    return context->actual_fps;
}

uint64_t gui_emulation_get_total_cycles(emulation_context_t* context) {
    if (!context) return 0;
    return context->total_cycles_executed;
}

// Convenience wrapper functions
void gui_emulation_start(emulation_context_t* emu_context) {
    if (emu_context) {
        gui_emulation_send_signal(emu_context, EMU_SIGNAL_START);
    }
}

void gui_emulation_pause(emulation_context_t* emu_context) {
    if (emu_context && emu_context->c64) {
        // First trigger the intercept to stop CPU execution
        mos6510_start_intercept(emu_context->c64->mos6510);
        // Then send the pause signal to update thread state
        gui_emulation_send_signal(emu_context, EMU_SIGNAL_PAUSE);
    }
}

void gui_emulation_step(emulation_context_t* emu_context) {
    if (emu_context) {
        gui_emulation_send_signal(emu_context, EMU_SIGNAL_STEP);
    }
}

void gui_emulation_reset(emulation_context_t* emu_context) {
    if (emu_context && emu_context->c64) {
        // Reset should happen from GUI thread for immediate response
        printf("GUI: Performing system reset\n");
        
        // First stop any running CPU execution
        mos6510_start_intercept(emu_context->c64->mos6510);
        
        // Perform CPU reset (could be extended to full system reset)
        mos6510_reset(emu_context->c64->mos6510);
        
        // Update thread state
        emu_context->current_state = EMU_STATE_STOPPED;
        emu_context->total_cycles_executed = 0;
        emu_context->frames_rendered = 0;
        
        printf("GUI: System reset completed\n");
    }
}

// Main emulation thread function - focused on CPU dispatch only
static int gui_emulation_thread_main(void* data) {
    emulation_context_t* context = (emulation_context_t*)data;
    if (!context) return -1;
    
    sdl_thread_impl_t* impl = (sdl_thread_impl_t*)context->thread_impl;
    context->current_state = EMU_STATE_STOPPED;
    
    while (context->thread_running) {
        // Wait for signal
        SDL_LockMutex(impl->signal_mutex);
        while (context->pending_signal == EMU_SIGNAL_NONE && context->thread_running) {
            SDL_CondWait(impl->signal_condition, impl->signal_mutex);
        }
        emulation_signal_t signal = context->pending_signal;
        context->pending_signal = EMU_SIGNAL_NONE;
        SDL_UnlockMutex(impl->signal_mutex);
        
        if (!context->thread_running) break;
        
        // Process signal and execute CPU operation
        switch (signal) {            case EMU_SIGNAL_START:
                context->current_state = EMU_STATE_RUNNING;
                printf("Emulation thread: Starting CPU execution\n");
                
                // Ensure CPU is properly initialized before execution
                if (!context->c64->mos6510) {
                    printf("Emulation thread: ERROR - CPU not initialized\n");
                    context->current_state = EMU_STATE_STOPPED;
                    break;
                }
                
                // Check if CPU has proper interfaces attached
                if (!context->c64->bus) {
                    printf("Emulation thread: ERROR - Bus not attached to C64\n");
                    context->current_state = EMU_STATE_STOPPED;
                    break;
                }
                
                printf("Emulation thread: Checking system initialization...\n");
                
                // Check if system has ROM loaded by examining reset vector
                uint8_t reset_low = 0;
                uint8_t reset_high = 0;
                  // Try to read reset vector through the bus system
                if (context->c64->bus) {
                    // Read reset vector through proper memory mapping (ROM or RAM depending on banking)
                    reset_low = c64_bus_memory_read(context->c64->bus, 0xFFFC);
                    reset_high = c64_bus_memory_read(context->c64->bus, 0xFFFD);
                }
                
                uint16_t reset_vector = (reset_high << 8) | reset_low;
                printf("Emulation thread: Reset vector = $%04X\n", reset_vector);
                  if (reset_vector == 0x0000) {
                    printf("Emulation thread: WARNING - No ROM loaded, reset vector is $0000\n");
                    printf("Emulation thread: Using simulation mode instead of real CPU execution\n");
                    
                    // Simulation mode - execute instructions in a controlled manner
                    printf("Emulation thread: Starting simulation mode\n");
                    uint64_t sim_cycles = 0;
                    const uint64_t MAX_SIM_CYCLES = 100000;
                    uint32_t last_sim_log_time = SDL_GetTicks();
                    const uint32_t sim_log_interval_ms = 10000; // Log every 10 seconds for simulation
                      while (context->current_state == EMU_STATE_RUNNING && context->thread_running && sim_cycles < MAX_SIM_CYCLES) {
                        // Simulate CPU step - this executes one instruction safely
                        printf("Emulation thread: About to call mos6510_step, cycle %llu\n", (unsigned long long)sim_cycles);
                        if (mos6510_step(context->c64->mos6510)) {
                            context->total_cycles_executed++;
                            sim_cycles++;
                            printf("Emulation thread: mos6510_step succeeded, cycle %llu\n", (unsigned long long)sim_cycles);
                        } else {
                            printf("Emulation thread: CPU step failed, stopping simulation\n");
                            break;
                        }
                        
                        // Check for intercept every 1000 cycles to allow pause/stop
                        if ((sim_cycles % 1000) == 0) {
                            if (mos6510_is_intercepting(context->c64->mos6510) || !context->thread_running) {
                                printf("Emulation thread: Intercept or quit detected during simulation\n");
                                break;
                            }
                            // Small delay to prevent busy loop and allow GUI responsiveness
                            SDL_Delay(1);
                            
                            // Time-based logging for simulation mode
                            uint32_t current_sim_time = SDL_GetTicks();
                            if (current_sim_time - last_sim_log_time >= sim_log_interval_ms) {
                                printf("Emulation thread: Simulation executed %llu cycles (total: %llu)\n", 
                                       (unsigned long long)sim_cycles,
                                       (unsigned long long)context->total_cycles_executed);
                                last_sim_log_time = current_sim_time;
                            }
                        }
                    }
                      if (sim_cycles >= MAX_SIM_CYCLES) {
                        printf("Emulation thread: Simulation reached cycle limit (%llu cycles)\n", (unsigned long long)sim_cycles);
                    }
                } else {
                    // Real execution mode with proper ROM
                    printf("Emulation thread: Starting real CPU execution with continuous execution\n");
                    
                    // Reset CPU to proper initial state and load reset vector into PC
                    mos6510_reset(context->c64->mos6510);
                    printf("Emulation thread: CPU reset completed, PC set to reset vector\n");
                    
                    // Use continuous execution with mos6510_execute()
                    // This will run until intercept is triggered (for pause/stop)
                    uint32_t last_log_time = SDL_GetTicks();
                    const uint32_t log_interval_ms = 5000; // Log every 5 seconds
                    uint64_t last_total_cycles = context->total_cycles_executed;
                    
                    while (context->current_state == EMU_STATE_RUNNING && context->thread_running) {
                        // Start continuous execution - this will run until intercept
                        printf("Emulation thread: Starting continuous CPU execution\n");
                        fflush(stdout); // Force output before potential crash
                        
                        // Check CPU state before execution
                        mos6510_t* cpu = (mos6510_t*)context->c64->mos6510;
                        printf("Emulation thread: CPU PC=$%04X before execution\n", cpu->base.pc);
                        fflush(stdout);
                        
                        // Record start time to detect crashes
                        uint32_t start_time = SDL_GetTicks();
                        
                        mos6510_execute(context->c64->mos6510);
                        
                        uint32_t end_time = SDL_GetTicks();
                        uint32_t execution_time = end_time - start_time;
                        
                        printf("Emulation thread: CPU execution returned after %u ms (intercept hit)\n", execution_time);
                        fflush(stdout);
                        
                        // If execution returned very quickly, it might be a crash or error
                        if (execution_time < 100) {
                            printf("Emulation thread: WARNING - Execution returned very quickly (%u ms)\n", execution_time);
                            printf("Emulation thread: CPU PC=$%04X after quick return\n", cpu->base.pc);
                            fflush(stdout);
                        }
                        
                        // Update cycle count (approximate - continuous execution doesn't track individual cycles)
                        context->total_cycles_executed += 1000; // Rough estimate
                        
                        // Check why execution stopped
                        if (mos6510_is_intercepting(context->c64->mos6510)) {
                            printf("Emulation thread: Intercept detected, clearing intercept\n");
                            mos6510_stop_intercept(context->c64->mos6510);
                            
                            // Check if we should pause or continue
                            if (context->current_state != EMU_STATE_RUNNING || !context->thread_running) {
                                printf("Emulation thread: Pause/stop requested\n");
                                break;
                            }
                        }
                        
                        // Time-based logging
                        uint32_t current_time = SDL_GetTicks();
                        if (current_time - last_log_time >= log_interval_ms) {
                            uint64_t cycles_executed = context->total_cycles_executed - last_total_cycles;
                            printf("Emulation thread: Executed ~%llu cycles (total: %llu)\n", 
                                   (unsigned long long)cycles_executed,
                                   (unsigned long long)context->total_cycles_executed);
                            last_log_time = current_time;
                            last_total_cycles = context->total_cycles_executed;
                        }
                        
                        // Small delay to allow GUI responsiveness
                        SDL_Delay(10);
                    }
                }
                
                printf("Emulation thread: CPU execution stopped\n");
                // After execution returns, go back to paused
                context->current_state = EMU_STATE_PAUSED;
                break;
                
            case EMU_SIGNAL_STEP:
                context->current_state = EMU_STATE_STEPPING;
                printf("Emulation thread: Single step execution\n");
                // Execute single CPU instruction
                mos6510_step(context->c64->mos6510);
                context->current_state = EMU_STATE_PAUSED;
                break;
                
            case EMU_SIGNAL_RESET:
                // Reset is now handled by GUI thread, just acknowledge signal
                printf("Emulation thread: Reset signal received (handled by GUI thread)\n");
                context->current_state = EMU_STATE_STOPPED;
                break;
                
            case EMU_SIGNAL_PAUSE:
                // This signal should trigger intercept from GUI thread, not here
                printf("Emulation thread: Pause signal received\n");
                context->current_state = EMU_STATE_PAUSED;
                break;
                
            case EMU_SIGNAL_QUIT:
                context->thread_running = false;
                break;
                
            default:
                break;
        }
    }
    
    context->thread_running = false;
    return 0;
}

static void gui_emulation_process_signal(emulation_context_t* context, emulation_signal_t signal) {
    switch (signal) {
        case EMU_SIGNAL_START:
            context->current_state = EMU_STATE_RUNNING;
            break;
        case EMU_SIGNAL_PAUSE:
            context->current_state = EMU_STATE_PAUSED;
            break;
        case EMU_SIGNAL_STEP:
            context->current_state = EMU_STATE_STEPPING;
            break;
        case EMU_SIGNAL_RESET:
            context->current_state = EMU_STATE_RESETTING;
            break;
        case EMU_SIGNAL_QUIT:
            context->thread_running = false;
            break;
        default:
            break;
    }
}

// Frame rendering and timing functions (called from main GUI thread)
void gui_emulation_render_frame(emulation_context_t* context) {
    if (!context) return;
    
    // Only render frame if emulation is running
    if (context->current_state == EMU_STATE_RUNNING) {
        // Update frame counter
        context->frames_rendered++;
        
        // Trigger periodic intercept to allow GUI thread to regain control
        // This prevents the emulation thread from running indefinitely
        static uint32_t frame_count = 0;
        frame_count++;
        
        // Trigger intercept every few frames to maintain GUI responsiveness
        if ((frame_count % 3) == 0) {
            mos6510_start_intercept(context->c64->mos6510);
        }
    }
}

void gui_emulation_update_fps(emulation_context_t* context) {
    if (!context) return;
    
    static uint32_t last_fps_time = 0;
    static uint32_t fps_counter = 0;
    
    if (last_fps_time == 0) {
        last_fps_time = SDL_GetTicks();
    }
    
    fps_counter++;
    
    uint32_t current_time = SDL_GetTicks();
    if (current_time - last_fps_time >= 1000) {
        context->actual_fps = fps_counter;
        fps_counter = 0;
        last_fps_time = current_time;
    }
}

// SDL abstraction functions
void gui_delay(uint32_t ms) {
    SDL_Delay(ms);
}

// PLA debug function moved to pla_gui.c
// PLA is now handled through the chip system
