#include "cimgui_interface.h"
#include "../systems/c64/c64.h"
#include "../systems/c64/c64_bus.h"
#include "../systems/c64/c64_config.h"
#include "../utils/rom_loader.h"
#include "../chip/cpu/mos6510/mos6510.h"
#include "cimgui_backends.h"

#include <SDL.h>
#include <SDL_opengl.h>
#ifndef CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#endif
#include <cimgui.h>

#include <stdio.h>
#include <string.h>

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

void gui_handle_events(void) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        ImGui_ImplSDL2_ProcessEvent_C(&event);
        if (event.type == SDL_QUIT) {
            g_should_quit = true;
        }
        if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE && event.window.windowID == SDL_GetWindowID(g_window)) {
            g_should_quit = true;
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
      // Default ROM paths (can be modified by user)
    strcpy(gui_state->rom_path_basic, "data/c64/roms/basic.901226-01.bin");
    strcpy(gui_state->rom_path_kernal, "data/c64/roms/kernal.901227-03.bin");
    strcpy(gui_state->rom_path_chargen, "data/c64/roms/characters.901225-01.bin");
}

void gui_render_frame(c64_t* c64, gui_state_t* gui_state) {
    gui_render_frame_with_context(c64, gui_state, NULL);
}

void gui_render_frame_with_context(c64_t* c64, gui_state_t* gui_state, struct emulation_context_s* emu_context) {
    // Start the Dear ImGui frame
    ImGui_ImplOpenGL3_NewFrame_C();
    ImGui_ImplSDL2_NewFrame_C();
    igNewFrame();

    // Render main menu bar with emulation context
    gui_render_menu_bar_with_context(c64, gui_state, emu_context);

    // Render windows based on gui_state
    if (gui_state->show_screen) {
        gui_render_screen(c64, gui_state);
    }
    if (gui_state->show_memory_viewer) {
        gui_render_memory_viewer(c64, gui_state);
    }
    if (gui_state->show_debugger) {
        gui_render_debugger_with_context(c64, gui_state, emu_context);
    }
    if (gui_state->show_settings) {
        gui_render_settings(c64, gui_state);
    }
    if (gui_state->show_about) {
        gui_render_about(gui_state);
    }

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
    ImGui_ImplOpenGL3_RenderDrawData_C(igGetDrawData());
    SDL_GL_SwapWindow(g_window);
}

void gui_render_menu_bar(c64_t* c64, gui_state_t* gui_state) {
    gui_render_menu_bar_with_context(c64, gui_state, NULL);
}

void gui_render_menu_bar_with_context(c64_t* c64, gui_state_t* gui_state, struct emulation_context_s* emu_context) {
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
            
            // Add dynamic chip debug windows
            if (c64 && c64->system.chip_count > 0) {
                igSeparator();
                igText("Debug Windows:");
                for (uint8_t chip_id = 0; chip_id < c64->system.chip_count && chip_id < 16; chip_id++) {
                    chip_entry_t* entry = &c64->system.chips[chip_id];
                    if (entry->desc && entry->desc->render_debug_window) {
                        char menu_label[64];
                        snprintf(menu_label, sizeof(menu_label), "%s", entry->desc->description);
                        igMenuItem_BoolPtr(menu_label, NULL, &gui_state->show_chip_debug[chip_id], true);
                    }
                }
            }
            
            igEndMenu();
        }
        
        if (igBeginMenu("Chips", true)) {
            if (c64 && c64->system.chip_count > 0) {
                if (igBeginMenu("Debug Windows", true)) {
                    for (uint8_t chip_id = 0; chip_id < c64->system.chip_count && chip_id < 16; chip_id++) {
                        chip_entry_t* entry = &c64->system.chips[chip_id];
                        if (entry->desc && entry->desc->render_debug_window) {
                            char menu_label[64];
                            snprintf(menu_label, sizeof(menu_label), "%s Debug", entry->desc->description);
                            igMenuItem_BoolPtr(menu_label, NULL, &gui_state->show_chip_debug[chip_id], true);
                        }
                    }
                    igEndMenu();
                }
                
                if (igBeginMenu("Settings Windows", true)) {
                    for (uint8_t chip_id = 0; chip_id < c64->system.chip_count && chip_id < 16; chip_id++) {
                        chip_entry_t* entry = &c64->system.chips[chip_id];
                        if (entry->desc && entry->desc->render_settings_window) {
                            char menu_label[64];
                            snprintf(menu_label, sizeof(menu_label), "%s Settings", entry->desc->description);
                            igMenuItem_BoolPtr(menu_label, NULL, &gui_state->show_chip_settings[chip_id], true);
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

void gui_render_debugger(c64_t* c64, gui_state_t* gui_state) {
    gui_render_debugger_with_context(c64, gui_state, NULL);
}

void gui_render_debugger_with_context(c64_t* c64, gui_state_t* gui_state, emulation_context_t* emu_context) {
    if (!igBegin("Debugger", &gui_state->show_debugger, 0)) {
        igEnd();
        return;
    }    // Emulation state display
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
#define C64_TOTAL_WIDTH   403  // Including borders
#define C64_TOTAL_HEIGHT  284  // Including borders

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

// Static framebuffer for C64 screen
static uint32_t screen_buffer[C64_TOTAL_WIDTH * C64_TOTAL_HEIGHT];

bool gui_init_screen_display(gui_state_t* gui_state) {
    // Generate OpenGL texture
    glGenTextures(1, &gui_state->screen_texture_id);
    glBindTexture(GL_TEXTURE_2D, gui_state->screen_texture_id);
    
    // Set texture parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    
    // Initialize with placeholder content
    memset(screen_buffer, 0, sizeof(screen_buffer));
    
    // Create a simple test pattern (blue border, light blue center)
    for (int y = 0; y < C64_TOTAL_HEIGHT; y++) {
        for (int x = 0; x < C64_TOTAL_WIDTH; x++) {
            uint32_t color = c64_palette[6]; // Blue border
            
            // Inner screen area 
            if (x >= 40 && x < 360 && y >= 40 && y < 240) {
                color = c64_palette[14]; // Light blue screen
                
                // Add some test text pattern
                if (((x / 8) + (y / 8)) % 2) {
                    color = c64_palette[1]; // White
                }
            }
            
            screen_buffer[y * C64_TOTAL_WIDTH + x] = color;
        }
    }
    
    // Upload initial data to texture
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

void gui_update_screen_texture(c64_t* c64, gui_state_t* gui_state) {
    // TODO: Get actual pixel data from VIC-II
    // For now, update with dummy pattern that changes based on emulation state
    
    if (c64 && gui_state->emulation_running) {
        // Simple animation based on cycle count
        uint64_t frame = (c64->total_cycles / 20000) % 60;
        
        for (int y = 0; y < C64_TOTAL_HEIGHT; y++) {
            for (int x = 0; x < C64_TOTAL_WIDTH; x++) {
                uint32_t color = c64_palette[6]; // Blue border
                
                // Inner screen area
                if (x >= 40 && x < 360 && y >= 40 && y < 240) {
                    // Create animated pattern
                    int pattern = ((x / 8) + (y / 8) + frame) % 16;
                    color = c64_palette[pattern];
                }
                
                screen_buffer[y * C64_TOTAL_WIDTH + x] = color;
            }
        }
    }
    
    // Update OpenGL texture
    glBindTexture(GL_TEXTURE_2D, gui_state->screen_texture_id);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, C64_TOTAL_WIDTH, C64_TOTAL_HEIGHT,
                    GL_RGBA, GL_UNSIGNED_BYTE, screen_buffer);
}

void gui_render_screen(c64_t* c64, gui_state_t* gui_state) {
    if (!igBegin("C64 Screen", &gui_state->show_screen, 0)) {
        igEnd();
        return;
    }
    
    // Initialize screen display if needed
    if (gui_state->screen_texture_id == 0) {
        gui_init_screen_display(gui_state);
    }
    
    // Update screen texture with current frame
    gui_update_screen_texture(c64, gui_state);
    
    // Calculate display size
    float display_width = C64_TOTAL_WIDTH * gui_state->screen_scale;
    float display_height = C64_TOTAL_HEIGHT * gui_state->screen_scale;
    
    // Screen controls
    igText("Display Controls");
    igSliderFloat("Scale", &gui_state->screen_scale, 0.5f, 4.0f, "%.1fx", ImGuiSliderFlags_None);
    igCheckbox("Filter", &gui_state->screen_filter);
    igSameLine(0, -1.0f);
    igCheckbox("Scanlines", &gui_state->screen_scanlines);
    
    // Update texture filtering based on user preference
    glBindTexture(GL_TEXTURE_2D, gui_state->screen_texture_id);
    if (gui_state->screen_filter) {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    } else {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    }
    
    igSeparator();
    
    // Display information
    igText("Resolution: %dx%d (with border: %dx%d)", 
                C64_SCREEN_WIDTH, C64_SCREEN_HEIGHT,
                C64_TOTAL_WIDTH, C64_TOTAL_HEIGHT);
    igText("Display Size: %.0fx%.0f", display_width, display_height);
    
    if (c64) {
        igText("Frame: %llu", c64->total_cycles / 20000);
    }
    
    igSeparator();
    
    // Center the image in the available content region
    ImVec2 content_avail;
    igGetContentRegionAvail(&content_avail);
    ImVec2 image_size = {display_width, display_height};
    
    // Calculate centering offset
    ImVec2 cursor_pos;
    igGetCursorPos(&cursor_pos);
    if (content_avail.x > image_size.x) {
        cursor_pos.x += (content_avail.x - image_size.x) * 0.5f;
    }
    if (content_avail.y > image_size.y) {
        cursor_pos.y += (content_avail.y - image_size.y) * 0.5f;
    }
    igSetCursorPos(cursor_pos);
    
    // Render the screen texture
    ImTextureID tex_id = (ImTextureID)(intptr_t)gui_state->screen_texture_id;
    
    if (gui_state->screen_scanlines) {
        // TODO: Implement scanline shader effect
        // For now, just draw the image normally
        igImage(tex_id, image_size, (ImVec2){0, 0}, (ImVec2){1, 1});
    } else {
        igImage(tex_id, image_size, (ImVec2){0, 0}, (ImVec2){1, 1});
    }
    
    // Handle mouse interaction with screen
    if (igIsItemHovered(ImGuiHoveredFlags_None)) {
        ImVec2 mouse_pos;
        igGetMousePos(&mouse_pos);
        ImVec2 image_min;
        igGetItemRectMin(&image_min);
        ImVec2 relative_pos = {(mouse_pos.x - image_min.x) / gui_state->screen_scale,
                              (mouse_pos.y - image_min.y) / gui_state->screen_scale};
        
        if (relative_pos.x >= 0 && relative_pos.x < C64_TOTAL_WIDTH &&
            relative_pos.y >= 0 && relative_pos.y < C64_TOTAL_HEIGHT) {
            igSetTooltip("Screen coordinates: (%d, %d)", 
                         (int)relative_pos.x, (int)relative_pos.y);
        }
    }
    
    igEnd();
}

// ============================================================================
// ROM LOADING IMPLEMENTATION
// ============================================================================

void gui_load_rom_file(const char* filepath, const char* type) {
    if (!filepath || !type) {
        printf("Invalid ROM loading parameters\n");
        return;
    }
    
    printf("ROM loading requested: %s (%s)\n", filepath, type);
    
    // Store the ROM path in the global GUI state
    // Note: This assumes gui_state is accessible globally or through a context
    // For now, just print a message that implementation is needed
    printf("Warning: ROM loading needs to be connected to GUI state and emulation context\n");
    printf("TODO: Update GUI state ROM paths and trigger reload in emulation context\n");
}

// Helper function to reload ROMs when new paths are provided
bool gui_apply_rom_changes(emulation_context_t* emu_context, gui_state_t* gui_state) {
    if (!emu_context || !emu_context->c64 || !gui_state) {
        return false;
    }
    
    // Check if any ROM paths have changed (simplified - just reload unconditionally for now)
    return gui_reload_roms_from_state(emu_context->c64, gui_state);
}

// Updated ROM loading function that updates GUI state
void gui_load_rom_file_with_context(const char* filepath, const char* type, gui_state_t* gui_state, emulation_context_t* emu_context) {
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
        mos6510_start_intercept();
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
        mos6510_start_intercept();
        
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
                    
                    while (context->current_state == EMU_STATE_RUNNING && sim_cycles < MAX_SIM_CYCLES) {
                        // Simulate CPU step - this executes one instruction safely
                        if (mos6510_step(context->c64->mos6510)) {
                            context->total_cycles_executed++;
                            sim_cycles++;
                        } else {
                            printf("Emulation thread: CPU step failed, stopping simulation\n");
                            break;
                        }
                        
                        // Check for intercept every 1000 cycles to allow pause/stop
                        if ((sim_cycles % 1000) == 0) {
                            if (mos6510_is_intercepting()) {
                                printf("Emulation thread: Intercept detected during simulation\n");
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
                    }                } else {
                    // Real execution mode with proper ROM
                    printf("Emulation thread: Starting real CPU execution with intercept control\n");
                    
                    // Set up controlled execution with intercept mechanism
                    // This will allow the PAUSE signal to stop execution
                    const uint32_t batch_size = 10000; // Execute in batches
                    uint32_t last_log_time = SDL_GetTicks();
                    const uint32_t log_interval_ms = 5000; // Log every 5 seconds
                    uint64_t cycles_since_last_log = 0;
                    
                    while (context->current_state == EMU_STATE_RUNNING) {
                        // Execute a batch of instructions using controlled threaded dispatch
                        uint32_t batch_cycles = 0;
                        
                        // Use intercept to limit execution to a batch
                        for (int i = 0; i < (int)batch_size && context->current_state == EMU_STATE_RUNNING; i++) {
                            if (mos6510_step(context->c64->mos6510)) {
                                context->total_cycles_executed++;
                                batch_cycles++;
                                cycles_since_last_log++;
                            } else {
                                printf("Emulation thread: CPU execution failed\n");
                                context->current_state = EMU_STATE_STOPPED;
                                break;
                            }
                            
                            // Check for pause every 100 instructions
                            if ((i % 100) == 0 && mos6510_is_intercepting()) {
                                context->current_state = EMU_STATE_PAUSED;
                                break;
                            }
                        }
                        
                        // Time-based logging instead of per-batch logging
                        uint32_t current_time = SDL_GetTicks();
                        if (current_time - last_log_time >= log_interval_ms) {
                            printf("Emulation thread: Executed %llu cycles (total: %llu)\n", 
                                   (unsigned long long)cycles_since_last_log,
                                   (unsigned long long)context->total_cycles_executed);
                            last_log_time = current_time;
                            cycles_since_last_log = 0;
                        }
                        
                        // Small delay between batches to allow GUI responsiveness
                        if (context->current_state == EMU_STATE_RUNNING) {
                            SDL_Delay(10);
                        }
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
            mos6510_start_intercept();
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
