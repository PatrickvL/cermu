#include "../core/emulated_system.h"
#include "../gui/imgui_interface.h"
#include <SDL.h>
#include <SDL_opengl.h>
#include "../../external/imgui/imgui.h"
#include "../../external/imgui/backends/imgui_impl_opengl3.h"
#include "../../external/imgui/backends/imgui_impl_sdl2.h"
#include <stdio.h>
#include <string.h>
#include <memory>

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
    
    // Initialize GUI first
    if (!gui_init("Multi-System Emulator", 1200, 800)) {
        printf("Failed to initialize GUI\n");
        return 1;
    }
    
    // Initialize GUI state
    gui_state_t gui_state;
    gui_init_state(&gui_state);
    
    // Create system if file provided, otherwise wait for user to load via GUI
    std::unique_ptr<IEmulatedSystem> system;
    
    if (file_path != nullptr) {
        // Auto-detect system from file
        printf("Detecting system for file: %s\n", file_path);
        system = SystemRegistry::instance().create_system_for_file(file_path);
        
        if (!system) {
            printf("ERROR: Could not detect system for file: %s\n", file_path);
            printf("No emulator supports this file format.\n");
            gui_cleanup();
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
            gui_cleanup();
            return 1;
        }
        
        // Load the file
        if (!system->load_file(file_path)) {
            printf("ERROR: Failed to load file: %s\n", file_path);
            system->shutdown();
            gui_cleanup();
            return 1;
        }
        
        printf("Successfully loaded file into %s\n", system->get_descriptor().name);
        
        // Allocate a framebuffer matching the system's native resolution
        const auto& traits = system->get_hardware_traits();
        int sys_width = traits.display.visible_width;
        int sys_height = traits.display.visible_height;
        uint32_t* system_framebuffer = new uint32_t[sys_width * sys_height];
        memset(system_framebuffer, 0, sys_width * sys_height * sizeof(uint32_t));
        
        // Set system framebuffer
        system->set_framebuffer(system_framebuffer, sys_width, sys_height);
        printf("Allocated %dx%d framebuffer for system\n", sys_width, sys_height);
    } else {
        printf("No file specified - starting without loaded system\n");
        printf("Use File menu to load a ROM/disk/binary\n");
    }
    
    // Main GUI loop
    printf("Entering main loop\n");
    
    // Get display dimensions from system
    if (system) {
        const auto& traits = system->get_hardware_traits();
        printf("Display resolution: %dx%d\n", traits.display.visible_width, traits.display.visible_height);
    }
    // Create a dedicated OpenGL texture for the system's framebuffer
    GLuint system_texture = 0;
    int sys_width = 0, sys_height = 0;
    
    if (system) {
        const auto& traits = system->get_hardware_traits();
        sys_width = traits.display.visible_width;
        sys_height = traits.display.visible_height;
        
        glGenTextures(1, &system_texture);
        glBindTexture(GL_TEXTURE_2D, system_texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, sys_width, sys_height, 0,
                    GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        
        // Override the gui_state texture with our system texture
        if (gui_state.screen_texture_id) {
            glDeleteTextures(1, &gui_state.screen_texture_id);
        }
        gui_state.screen_texture_id = system_texture;
        
        printf("Created %dx%d OpenGL texture for system display\n", sys_width, sys_height);
    }
    
    while (!gui_should_quit()) {
        // Handle SDL events with keyboard input for CHIP-8
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL2_ProcessEvent(&event);
            
            if (event.type == SDL_QUIT) {
                gui_state.emulation_running = false;
            }
            
            // Forward keyboard events to the system
            if (system && !ImGui::GetIO().WantCaptureKeyboard) {
                if (event.type == SDL_KEYDOWN) {
                    system->handle_keyboard_event(event.key.keysym.sym, true);
                } else if (event.type == SDL_KEYUP) {
                    system->handle_keyboard_event(event.key.keysym.sym, false);
                }
            }
        }
        
        // Run system frame if we have one
        if (system) {
            system->run_frame();
            
            // Update the OpenGL texture with the system's framebuffer
            uint32_t* fb = system->get_framebuffer();
            if (fb && system_texture) {
                glBindTexture(GL_TEXTURE_2D, system_texture);
                glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, sys_width, sys_height,
                              GL_RGBA, GL_UNSIGNED_BYTE, fb);
            }
        }
        
        // Custom rendering for generic systems
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();
        
        // Render display fullscreen with proper scaling
        if (system && system_texture) {
            const ImGuiViewport* viewport = ImGui::GetMainViewport();
            ImGui::SetNextWindowPos(viewport->Pos);
            ImGui::SetNextWindowSize(viewport->Size);
            
            ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                                    ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings |
                                    ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoBackground;
            
            ImGui::Begin("Display", nullptr, flags);
            
            // Calculate centered display with integer scaling
            float window_w = viewport->Size.x;
            float window_h = viewport->Size.y;
            float scale = fminf(window_w / sys_width, window_h / sys_height);
            scale = floorf(scale);  // Integer scaling for crisp pixels
            if (scale < 1.0f) scale = 1.0f;
            
            float display_w = sys_width * scale;
            float display_h = sys_height * scale;
            float pos_x = (window_w - display_w) * 0.5f;
            float pos_y = (window_h - display_h) * 0.5f;
            
            ImGui::SetCursorPos(ImVec2(pos_x, pos_y));
            ImGui::Image((void*)(intptr_t)system_texture, ImVec2(display_w, display_h));
            
            ImGui::End();
        }
        
        // Rendering
        ImGui::Render();
        ImGuiIO& io = ImGui::GetIO();
        glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow((SDL_Window*)gui_get_window());
        
        // Frame rate limiting
        gui_delay(16);  // ~60 FPS
    }
    
    // Cleanup system texture
    if (system_texture) {
        glDeleteTextures(1, &system_texture);
    }
    
    printf("Shutting down\n");
    
    // Cleanup
    if (system) {
        // Free the system framebuffer
        uint32_t* fb = system->get_framebuffer();
        if (fb) {
            delete[] fb;
        }
        system->shutdown();
    }
    
    gui_cleanup_state(&gui_state);
    gui_cleanup();
    
    return 0;
}