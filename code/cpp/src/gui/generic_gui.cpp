#include "generic_gui.h"
#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_sdl2.h"
#include <stdio.h>
#include <algorithm>

// ============================================================================
// Constructor / Destructor
// ============================================================================

GenericEmulatorGUI::GenericEmulatorGUI()
    : window_(nullptr)
    , gl_context_(nullptr)
    , should_quit_(false)
    , glsl_version_("#version 130")
    , screen_texture_id_(0)
    , screen_scale_(2.0f)
    , screen_filter_(false)
    , screen_scanlines_(false)
    , show_screen_(true)
    , show_memory_viewer_(false)
    , show_settings_(false)
    , show_about_(false)
    , window_width_(1200)
    , window_height_(800)
{
}

GenericEmulatorGUI::~GenericEmulatorGUI() {
    cleanup();
}

// ============================================================================
// Initialization / Cleanup
// ============================================================================

bool GenericEmulatorGUI::init(const char* window_title, int width, int height) {
    window_width_ = width;
    window_height_ = height;
    
    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_EVENTS) < 0) {
        printf("Failed to initialize SDL: %s\n", SDL_GetError());
        return false;
    }
    
    // GL 3.0 + GLSL 130
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    
    // Create window with graphics context
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    
    SDL_WindowFlags window_flags = (SDL_WindowFlags)(
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    
    window_ = SDL_CreateWindow(
        window_title,
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        width, height,
        window_flags);
    
    if (!window_) {
        printf("Failed to create window: %s\n", SDL_GetError());
        SDL_Quit();
        return false;
    }
    
    gl_context_ = SDL_GL_CreateContext(window_);
    if (!gl_context_) {
        printf("Failed to create GL context: %s\n", SDL_GetError());
        SDL_DestroyWindow(window_);
        SDL_Quit();
        return false;
    }
    
    SDL_GL_MakeCurrent(window_, gl_context_);
    SDL_GL_SetSwapInterval(1); // Enable vsync
    
    // Show the window
    SDL_ShowWindow(window_);
    
    // Setup Dear ImGui context
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
    
    // Setup ImGui style
    ImGui::StyleColorsDark();
    
    // Custom color scheme for retro aesthetic
    ImGuiStyle& style = ImGui::GetStyle();
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.06f, 0.06f, 0.2f, 0.95f);
    style.Colors[ImGuiCol_Header] = ImVec4(0.2f, 0.2f, 0.8f, 0.8f);
    style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.3f, 0.3f, 0.9f, 0.8f);
    style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.4f, 0.4f, 1.0f, 0.8f);
    style.Colors[ImGuiCol_Button] = ImVec4(0.2f, 0.2f, 0.7f, 0.6f);
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.3f, 0.3f, 0.8f, 0.8f);
    style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.4f, 0.4f, 0.9f, 1.0f);
    
    // Setup Platform/Renderer backends
    ImGui_ImplSDL2_InitForOpenGL(window_, gl_context_);
    ImGui_ImplOpenGL3_Init(glsl_version_);
    
    printf("Generic GUI initialized: %s (%dx%d)\n", window_title, width, height);
    return true;
}

void GenericEmulatorGUI::cleanup() {
    // Cleanup screen texture if allocated
    if (screen_texture_id_ != 0) {
        glDeleteTextures(1, &screen_texture_id_);
        screen_texture_id_ = 0;
    }
    
    // Cleanup ImGui
    if (ImGui::GetCurrentContext()) {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplSDL2_Shutdown();
        ImGui::DestroyContext();
    }
    
    // Cleanup SDL
    if (gl_context_) {
        SDL_GL_DeleteContext(gl_context_);
        gl_context_ = nullptr;
    }
    if (window_) {
        SDL_DestroyWindow(window_);
        window_ = nullptr;
    }
    SDL_Quit();
}

// ============================================================================
// Main Loop
// ============================================================================

void GenericEmulatorGUI::run() {
    printf("Starting main loop\n");
    
    while (!should_quit_) {
        // Handle events (system-specific)
        handle_events();
        
        // Update emulation state (system-specific)
        update_frame();
        
        // Render frame (system-specific)
        render_frame();
        
        // Frame rate limiting
        uint32_t delay = get_frame_delay_ms();
        if (delay > 0) {
            SDL_Delay(delay);
        }
    }
    
    printf("Main loop ended\n");
}

// ============================================================================
// Generic Helper Functions
// ============================================================================

void GenericEmulatorGUI::begin_frame() {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();
}

void GenericEmulatorGUI::end_frame() {
    ImGui::Render();
    ImGuiIO& io = ImGui::GetIO();
    glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    SDL_GL_SwapWindow(window_);
}

void GenericEmulatorGUI::render_file_menu_generic() {
    if (ImGui::MenuItem("Exit")) {
        should_quit_ = true;
    }
}

void GenericEmulatorGUI::render_view_menu_generic() {
    ImGui::MenuItem("Screen Display", nullptr, &show_screen_);
    ImGui::MenuItem("Memory Viewer", nullptr, &show_memory_viewer_);
    ImGui::Separator();
    ImGui::MenuItem("Settings", nullptr, &show_settings_);
}

void GenericEmulatorGUI::render_help_menu_generic() {
    ImGui::MenuItem("About", nullptr, &show_about_);
}

void GenericEmulatorGUI::render_about_dialog_generic() {
    if (!show_about_) return;
    
    if (ImGui::Begin("About", &show_about_, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Multi-System Emulator");
        ImGui::Text("Version 0.1.0");
        ImGui::Separator();
        ImGui::Text("A modular emulator supporting multiple retro systems");
        ImGui::Text("Built with Dear ImGui and SDL2");
        ImGui::Separator();
        
        if (ImGui::Button("Close", ImVec2(120, 0))) {
            show_about_ = false;
        }
    }
    ImGui::End();
}

// ============================================================================
// OpenGL Texture Management
// ============================================================================

GLuint GenericEmulatorGUI::create_screen_texture(int width, int height) {
    GLuint texture_id = 0;
    glGenTextures(1, &texture_id);
    glBindTexture(GL_TEXTURE_2D, texture_id);
    
    // Set texture parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    
    // Allocate texture storage
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0,
                GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    
    printf("Created OpenGL texture %u (%dx%d)\n", texture_id, width, height);
    return texture_id;
}

void GenericEmulatorGUI::update_screen_texture(GLuint texture_id, int width, int height,
                                               const uint32_t* pixels) {
    if (texture_id == 0 || !pixels) return;
    
    glBindTexture(GL_TEXTURE_2D, texture_id);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height,
                   GL_RGBA, GL_UNSIGNED_BYTE, pixels);
}

// ============================================================================
// Display Scaling Helper
// ============================================================================

void GenericEmulatorGUI::calculate_integer_scaled_dimensions(
    int window_width, int window_height,
    int content_width, int content_height,
    int* out_display_width, int* out_display_height,
    int* out_pos_x, int* out_pos_y) const
{
    // Calculate maximum integer scale that fits in window
    float scale_x = (float)window_width / (float)content_width;
    float scale_y = (float)window_height / (float)content_height;
    float scale = std::min(scale_x, scale_y);
    
    // Use integer scaling for pixel-perfect display
    int integer_scale = (int)scale;
    if (integer_scale < 1) integer_scale = 1;
    
    // Calculate final dimensions
    *out_display_width = content_width * integer_scale;
    *out_display_height = content_height * integer_scale;
    
    // Center in window
    *out_pos_x = (window_width - *out_display_width) / 2;
    *out_pos_y = (window_height - *out_display_height) / 2;
}