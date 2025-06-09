#include "cimgui_interface.h"
#include "../systems/c64/c64.h"
#include "../systems/c64/c64_bus.h"
#include "../chip/cpu/mos6510/mos6510.h"
#include "cimgui_backends.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
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
    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0) {
        printf("Error: %s\n", SDL_GetError());
        return false;
    }

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
        printf("Error: SDL_CreateWindow(): %s\n", SDL_GetError());
        return false;
    }

    g_gl_context = SDL_GL_CreateContext(g_window);
    SDL_GL_MakeCurrent(g_window, g_gl_context);
    SDL_GL_SetSwapInterval(1); // Enable vsync

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
    gui_state->show_cpu_state = true;
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
    strcpy(gui_state->rom_path_basic, "data/c64/roms/basic.rom");
    strcpy(gui_state->rom_path_kernal, "data/c64/roms/kernal.rom");
    strcpy(gui_state->rom_path_chargen, "data/c64/roms/chargen.rom");
}

void gui_render_frame(c64_t* c64, gui_state_t* gui_state) {
    // Start the Dear ImGui frame
    ImGui_ImplOpenGL3_NewFrame_C();
    ImGui_ImplSDL2_NewFrame_C();
    igNewFrame();

    // Render main menu bar
    gui_render_menu_bar(c64, gui_state);

    // Render windows based on gui_state
    if (gui_state->show_screen) {
        gui_render_screen(c64, gui_state);
    }
    if (gui_state->show_cpu_state) {
        gui_render_cpu_state(c64, gui_state);
    }
    if (gui_state->show_memory_viewer) {
        gui_render_memory_viewer(c64, gui_state);
    }
    if (gui_state->show_vic_registers) {
        gui_render_vic_registers(c64, gui_state);
    }
    if (gui_state->show_cia_registers) {
        gui_render_cia_registers(c64, gui_state);
    }
    if (gui_state->show_sid_registers) {
        gui_render_sid_registers(c64, gui_state);
    }
    if (gui_state->show_debugger) {
        gui_render_debugger(c64, gui_state);
    }
    if (gui_state->show_settings) {
        gui_render_settings(c64, gui_state);
    }
    if (gui_state->show_about) {
        gui_render_about(gui_state);
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
            if (igMenuItem_Bool("Reset", NULL, false, true)) {
                // TODO: Reset C64
            }
            igSeparator();
            if (igMenuItem_Bool(gui_state->emulation_running ? "Pause" : "Run", NULL, false, true)) {
                gui_state->emulation_running = !gui_state->emulation_running;
            }
            if (igMenuItem_Bool("Step", NULL, false, true)) {
                // TODO: Single step execution
            }
            igEndMenu();
        }
        
        if (igBeginMenu("View", true)) {
            igMenuItem_BoolPtr("Screen Display", NULL, &gui_state->show_screen, true);
            igMenuItem_BoolPtr("CPU State", NULL, &gui_state->show_cpu_state, true);
            igMenuItem_BoolPtr("Memory Viewer", NULL, &gui_state->show_memory_viewer, true);
            igMenuItem_BoolPtr("VIC-II Registers", NULL, &gui_state->show_vic_registers, true);
            igMenuItem_BoolPtr("CIA Registers", NULL, &gui_state->show_cia_registers, true);
            igMenuItem_BoolPtr("SID Registers", NULL, &gui_state->show_sid_registers, true);
            igMenuItem_BoolPtr("Debugger", NULL, &gui_state->show_debugger, true);
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
        igText(gui_state->emulation_running ? "Running" : "Paused");
        
        igEndMainMenuBar();
    }
}

void gui_render_cpu_state(c64_t* c64, gui_state_t* gui_state) {
    if (!igBegin("CPU State", &gui_state->show_cpu_state, 0)) {
        igEnd();
        return;
    }

    if (c64 && c64->mos6510) {
        // TODO: Get CPU state from MOS6510
        // For now, show placeholder values
        igText("Program Counter: $%04X", 0x0000);
        igText("Accumulator:     $%02X", 0x00);
        igText("X Register:      $%02X", 0x00);
        igText("Y Register:      $%02X", 0x00);
        igText("Stack Pointer:   $%02X", 0xFF);
        
        igSeparator();
        igText("Status Flags:");
        igText("N V - B D I Z C");
        igText("0 0 1 0 0 0 0 0");
        
        igSeparator();
        igText("Current Instruction: NOP");
        igText("Cycles: %llu", c64->total_cycles);
    } else {
        igText("C64 system not initialized");
    }

    igEnd();
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
                
                // TODO: Read memory from C64 bus
                uint8_t byte_value = 0x00; // c64_bus_memory_read(c64->bus, byte_addr);
                
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

void gui_render_vic_registers(c64_t* c64, gui_state_t* gui_state) {
    if (!igBegin("VIC-II Registers", &gui_state->show_vic_registers, 0)) {
        igEnd();
        return;
    }

    if (c64 && c64->vicii) {
        igText("VIC-II (6569) Registers");
        igSeparator();
        
        // TODO: Display actual VIC registers
        for (int i = 0; i < 47; i++) {
            igText("$D%03X: $%02X", 0x000 + i, 0x00);
        }
    } else {
        igText("VIC-II not initialized");
    }

    igEnd();
}

void gui_render_cia_registers(c64_t* c64, gui_state_t* gui_state) {
    if (!igBegin("CIA Registers", &gui_state->show_cia_registers, 0)) {
        igEnd();
        return;
    }

    if (c64 && c64->cia1 && c64->cia2) {
        if (igBeginTabBar("CIATab", 0)) {
            if (igBeginTabItem("CIA1", NULL, 0)) {
                igText("CIA1 ($DC00-$DCFF)");
                igSeparator();
                
                // TODO: Display actual CIA1 registers
                for (int i = 0; i < 16; i++) {
                    igText("$DC%02X: $%02X", i, 0x00);
                }
                
                igEndTabItem();
            }
            
            if (igBeginTabItem("CIA2", NULL, 0)) {
                igText("CIA2 ($DD00-$DDFF)");
                igSeparator();
                
                // TODO: Display actual CIA2 registers
                for (int i = 0; i < 16; i++) {
                    igText("$DD%02X: $%02X", i, 0x00);
                }
                
                igEndTabItem();
            }
            
            igEndTabBar();
        }
    } else {
        igText("CIA chips not initialized");
    }

    igEnd();
}

void gui_render_sid_registers(c64_t* c64, gui_state_t* gui_state) {
    if (!igBegin("SID Registers", &gui_state->show_sid_registers, 0)) {
        igEnd();
        return;
    }

    if (c64 && c64->sid) {
        igText("SID (6581) Registers");
        igSeparator();
        
        // TODO: Display actual SID registers
        for (int i = 0; i < 29; i++) {
            igText("$D4%02X: $%02X", i, 0x00);
        }
    } else {
        igText("SID not initialized");
    }

    igEnd();
}

void gui_render_debugger(c64_t* c64, gui_state_t* gui_state) {
    if (!igBegin("Debugger", &gui_state->show_debugger, 0)) {
        igEnd();
        return;
    }

    // Breakpoint controls
    igText("Breakpoints");
    igInputInt("Address", (int*)&gui_state->breakpoint_address, 1, 16, ImGuiInputTextFlags_CharsHexadecimal);
    gui_state->breakpoint_address &= 0xFFFF;
    
    igSameLine(0, -1.0f);
    if (igButton("Add", (ImVec2){0, 0})) {
        gui_state->breakpoint_enabled = true;
        // TODO: Set breakpoint in emulator
    }
    
    igSameLine(0, -1.0f);
    if (igButton("Clear All", (ImVec2){0, 0})) {
        gui_state->breakpoint_enabled = false;
        // TODO: Clear all breakpoints
    }
    
    igSeparator();
    
    // Execution controls
    igText("Execution Control");
    if (igButton("Step Into", (ImVec2){0, 0})) {
        // TODO: Single step execution
    }
    igSameLine(0, -1.0f);
    if (igButton("Step Over", (ImVec2){0, 0})) {
        // TODO: Step over subroutines
    }
    igSameLine(0, -1.0f);
    if (igButton("Step Out", (ImVec2){0, 0})) {
        // TODO: Step out of current subroutine
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
    
    if (igButton("Apply Settings", (ImVec2){0, 0})) {
        // TODO: Apply settings to emulator
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

// File handling stubs - implement with platform-specific file dialogs
void gui_load_rom_file(const char* filepath, const char* type) {
    printf("Loading %s ROM from: %s\n", type, filepath);
    // TODO: Implement ROM loading
}

void gui_load_disk_image(const char* filepath) {
    printf("Loading disk image from: %s\n", filepath);
    // TODO: Implement disk image loading
}
