#include "simple_system_gui.h"
#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"
#include <stdio.h>
#include <cstring>

#ifdef __has_include
#if __has_include("ImGuiFileDialog.h")
#include "ImGuiFileDialog.h"
#define HAS_IMGUIFILEDIALOG 1
#endif
#endif

// ============================================================================
// Constructor / Destructor
// ============================================================================

SimpleSystemGUI::SimpleSystemGUI(std::unique_ptr<EmulatedSystem> system)
    : GenericEmulatorGUI()
    , system_(std::move(system))
    , framebuffer_(nullptr)
    , fb_width_(0)
    , fb_height_(0)
    , emulation_running_(system_ != nullptr)  // Only run if we have a system
    , emulation_paused_(false)
    , speed_multiplier_(1.0f)
    , total_frames_(0)
    , actual_fps_(0)
    , last_fps_time_(0)
    , fps_counter_(0)
    , system_selection_dialog_()
{
    if (system_) {
        // Note: System should already be initialized and loaded before passing to GUI
        // Framebuffer allocation happens after init() when OpenGL context exists
        printf("SimpleSystemGUI created for system: %s\n",
               system_->get_descriptor().name);
    } else {
        printf("SimpleSystemGUI created without system - selection dialog will be shown\n");
        system_selection_dialog_.open();  // Open dialog if no system provided
    }
}

SimpleSystemGUI::~SimpleSystemGUI() {
    teardown_current_system();
}

// ============================================================================
// Initialization Override
// ============================================================================

bool SimpleSystemGUI::init(const char* window_title, int width, int height) {
    // Call base class init to create OpenGL context
    if (!GenericEmulatorGUI::init(window_title, width, height)) {
        return false;
    }
    
    // Now that OpenGL context exists, allocate framebuffer and create texture
    allocate_framebuffer();
    
    return true;
}

// ============================================================================
// Virtual Hook Implementations
// ============================================================================

void SimpleSystemGUI::handle_events() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        ImGui_ImplSDL2_ProcessEvent(&event);
        
        // Handle quit events
        if (event.type == SDL_QUIT ||
            (event.type == SDL_WINDOWEVENT &&
             event.window.event == SDL_WINDOWEVENT_CLOSE &&
             event.window.windowID == SDL_GetWindowID(get_window()))) {
            should_quit_ = true;
        }
        
        // Forward keyboard events to system
        // NOTE: We forward all keyboard events to the system, not just when ImGui doesn't want them
        // This is because CHIP-8/games need keyboard input even when menu is visible
        if (system_) {
            if (event.type == SDL_KEYDOWN) {
                system_->handle_keyboard_event(event.key.keysym.sym, true);
            } else if (event.type == SDL_KEYUP) {
                system_->handle_keyboard_event(event.key.keysym.sym, false);
            }
        }
    }
}

void SimpleSystemGUI::update_frame() {
    if (!system_ || !emulation_running_ || emulation_paused_) {
        return;
    }
    
    // Run one frame of emulation
    system_->run_frame();
    total_frames_++;
    
    // Update FPS counter
    update_fps();
}

void SimpleSystemGUI::render_frame() {
    begin_frame();
    
    // Only render dialog if it's actually open
    if (system_selection_dialog_.is_open()) {
        system_selection_dialog_.render(!system_);  // Don't allow cancel if no system loaded
        
        // Check if dialog selection was confirmed
        if (system_selection_dialog_.selection_confirmed()) {
            const char* selected = system_selection_dialog_.get_selected_system();
            int memory_opt = system_selection_dialog_.get_selected_memory_option();
            int region_opt = system_selection_dialog_.get_selected_region_option();
            const auto& peripherals = system_selection_dialog_.get_selected_peripherals();
            if (selected) {
                switch_system(selected, memory_opt, region_opt, &peripherals);
            }
            system_selection_dialog_.reset();
        }
    }
    
#ifdef HAS_IMGUIFILEDIALOG
    // Display file dialog and handle results
    if (ImGuiFileDialog::Instance()->Display("ChooseFileDlgKey")) {
        if (ImGuiFileDialog::Instance()->IsOk()) {
            std::string filePathName = ImGuiFileDialog::Instance()->GetFilePathName();
            printf("User selected file: %s\n", filePathName.c_str());
            
            // Save the last selected file path for next time
            last_file_path_ = filePathName;
            
            // Pause emulation while loading
            bool was_running = emulation_running_ && !emulation_paused_;
            if (was_running) {
                pause_emulation();
            }
            
            // Reset system before loading file for clean state
            if (system_) {
                system_->reset();
            }
            
            // Load the file
            if (system_ && system_->load_file(filePathName.c_str())) {
                printf("File loaded successfully: %s\n", filePathName.c_str());
                
                // Ensure emulation is running after successful file load
                if (!emulation_running_ || emulation_paused_) {
                    start_emulation();
                }
            } else {
                printf("Failed to load file: %s\n", filePathName.c_str());
                
                // Resume previous state if load failed
                if (was_running) {
                    start_emulation();
                }
            }
        } else {
            // User canceled - save the current path they were browsing
            std::string currentPath = ImGuiFileDialog::Instance()->GetCurrentPath();
            if (!currentPath.empty()) {
                last_file_path_ = currentPath;
            }
        }
        ImGuiFileDialog::Instance()->Close();
    }
#endif
    
    // Render screen (full-screen background)
    if (show_screen_ && system_) {
        render_screen();
    }
    
    // Render menu bar (on top of screen)
    render_menu_bar();
    
    // Render optional windows
    if (show_memory_viewer_) {
        render_memory_viewer();
    }
    if (show_settings_) {
        render_settings();
    }
    if (show_about_) {
        render_about();
    }
    
    // Let system render its debug windows
    if (system_) {
        system_->render_debug_windows(nullptr);
    }
    
    end_frame();
}

void SimpleSystemGUI::render_menu_bar() {
    if (!ImGui::BeginMainMenuBar()) {
        return;
    }
    
    // File menu
    if (ImGui::BeginMenu("File")) {
        // Switch System option
        if (ImGui::MenuItem("Switch System...")) {
            system_selection_dialog_.open();
        }
        
        ImGui::Separator();
        // Load File option (only if system is loaded)
        ImGui::BeginDisabled(!system_);
        if (ImGui::MenuItem("Load File...")) {
            load_file_dialog();
        }
        ImGui::EndDisabled();
        
        ImGui::Separator();
        
        render_file_menu_generic();
        ImGui::EndMenu();
    }
    
    // System menu
    if (ImGui::BeginMenu("System")) {
        // Generic system controls (only if system is loaded)
        ImGui::BeginDisabled(!system_);
        
        if (ImGui::MenuItem("Reset")) {
            reset_emulation();
        }
        
        bool is_running = emulation_running_ && !emulation_paused_;
        if (ImGui::MenuItem(is_running ? "Pause" : "Resume")) {
            if (is_running) {
                pause_emulation();
            } else {
                start_emulation();
            }
        }
        
        if (ImGui::MenuItem("Single Step", nullptr, false, emulation_paused_)) {
            step_emulation();
        }
        
        ImGui::Separator();
        
        // Speed control
        if (ImGui::SliderFloat("Speed", &speed_multiplier_, 0.1f, 5.0f, "%.1fx")) {
            if (system_) {
                system_->set_speed_multiplier(speed_multiplier_);
            }
        }
        
        ImGui::EndDisabled();
        
        // System-specific menu items (if system is loaded)
        if (system_) {
            ImGui::Separator();
            system_->render_system_menu_items();
        }
        
        ImGui::EndMenu();
    }
    
    // View menu
    if (ImGui::BeginMenu("View")) {
        render_view_menu_generic();
        ImGui::EndMenu();
    }
    
    // Settings menu
    if (ImGui::BeginMenu("Settings")) {
        if (ImGui::MenuItem("System Configuration")) {
            show_settings_ = true;
        }
        ImGui::EndMenu();
    }
    
    // Help menu
    if (ImGui::BeginMenu("Help")) {
        render_help_menu_generic();
        ImGui::EndMenu();
    }
    
    // Status bar on the right
    if (system_) {
        ImGui::SameLine(ImGui::GetWindowWidth() - 450);
        ImGui::Text("%s", system_->get_descriptor().short_name);
        ImGui::SameLine();
        ImGui::Text("Cycles: %llu", (unsigned long long)system_->get_total_cycles());
        ImGui::SameLine();
        ImGui::Text("FPS: %u", actual_fps_);
        ImGui::SameLine();
        ImGui::Text("%s", emulation_paused_ ? "Paused" : 
                         emulation_running_ ? "Running" : "Stopped");
    } else {
        ImGui::SameLine(ImGui::GetWindowWidth() - 200);
        ImGui::Text("No system loaded");
    }
    
    ImGui::EndMainMenuBar();
}

void SimpleSystemGUI::render_screen() {
    if (!system_) return;
    
    // Get viewport for fullscreen rendering
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize(viewport->Size);
    
    // Fullscreen window flags
    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoBackground;
    
    ImGui::Begin("##Screen", nullptr, flags);
    
    // Get system framebuffer
    uint32_t* fb = system_->get_framebuffer();
    if (fb && screen_texture_id_) {
        // Update texture
        update_screen_texture(screen_texture_id_, fb_width_, fb_height_, fb);
        
        // Calculate display dimensions with integer scaling
        int display_w, display_h, pos_x, pos_y;
        calculate_integer_scaled_dimensions(
            (int)viewport->Size.x, (int)viewport->Size.y,
            fb_width_, fb_height_,
            &display_w, &display_h, &pos_x, &pos_y);
        
        // Center and render
        ImGui::SetCursorPos(ImVec2((float)pos_x, (float)pos_y));
        ImGui::Image((void*)(intptr_t)screen_texture_id_,
                    ImVec2((float)display_w, (float)display_h));
    }
    
    ImGui::End();
}

void SimpleSystemGUI::render_memory_viewer() {
    if (!ImGui::Begin("Memory Viewer", &show_memory_viewer_)) {
        ImGui::End();
        return;
    }
    
    ImGui::Text("Memory viewer not yet implemented for generic systems");
    ImGui::Text("System-specific memory viewers can be added via system callbacks");
    
    ImGui::End();
}

void SimpleSystemGUI::render_settings() {
    if (!ImGui::Begin("Settings", &show_settings_, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::End();
        return;
    }
    
    if (system_) {
        ImGui::Text("System: %s", system_->get_descriptor().name);
        ImGui::Text("Description: %s", system_->get_descriptor().description);
        ImGui::Separator();
        
        // Hardware information
        const auto& traits = system_->get_hardware_traits();
        ImGui::Text("Display: %dx%d", 
                   traits.display.visible_width,
                   traits.display.visible_height);
        ImGui::Text("Format: %s",
                   traits.display.format == FramebufferFormat::MONOCHROME_1 ? "1-bit Monochrome" :
                   traits.display.format == FramebufferFormat::PALETTE_INDEXED_8 ? "8-bit Indexed" :
                   traits.display.format == FramebufferFormat::RGBA8888 ? "RGBA8888" : "Unknown");
        ImGui::Text("Palette Size: %d colors", (int)traits.display.palette_size);
        ImGui::Separator();
        
        // System-specific configuration UI
        ImGui::Text("System Configuration:");
        system_->render_configuration_ui();
    }
    
    ImGui::End();
}

void SimpleSystemGUI::render_about() {
    render_about_dialog_generic();
}

// ============================================================================
// System Control
// ============================================================================

void SimpleSystemGUI::start_emulation() {
    emulation_running_ = true;
    emulation_paused_ = false;
    printf("Emulation started\n");
}

void SimpleSystemGUI::pause_emulation() {
    emulation_paused_ = true;
    printf("Emulation paused\n");
}

void SimpleSystemGUI::reset_emulation() {
    if (system_) {
        system_->reset();
        total_frames_ = 0;
        printf("System reset\n");
    }
}

void SimpleSystemGUI::step_emulation() {
    if (system_ && emulation_paused_) {
        system_->tick();
        printf("Single step executed\n");
    }
}

// ============================================================================
// Helper Functions
// ============================================================================

void SimpleSystemGUI::update_fps() {
    fps_counter_++;
    
    uint32_t current_time = SDL_GetTicks();
    if (last_fps_time_ == 0) {
        last_fps_time_ = current_time;
    }
    
    if (current_time - last_fps_time_ >= 1000) {
        actual_fps_ = fps_counter_;
        fps_counter_ = 0;
        last_fps_time_ = current_time;
    }
}

void SimpleSystemGUI::allocate_framebuffer() {
    if (!system_) return;
    
    // Get display dimensions from system
    const auto& traits = system_->get_hardware_traits();
    fb_width_ = traits.display.visible_width;
    fb_height_ = traits.display.visible_height;
    
    // Allocate framebuffer
    framebuffer_ = new uint32_t[fb_width_ * fb_height_];
    memset(framebuffer_, 0, fb_width_ * fb_height_ * sizeof(uint32_t));
    
    // Give it to the system
    system_->set_framebuffer(framebuffer_, fb_width_, fb_height_);
    
    // Create OpenGL texture (must be called after OpenGL context is created)
    if (window_) {  // Check if init() was called
        screen_texture_id_ = create_screen_texture(fb_width_, fb_height_);
        printf("Allocated %dx%d framebuffer with texture %u\n", fb_width_, fb_height_, screen_texture_id_);
    } else {
        printf("Allocated %dx%d framebuffer (texture creation deferred until init)\n", fb_width_, fb_height_);
    }
}

void SimpleSystemGUI::free_framebuffer() {
    if (framebuffer_) {
        delete[] framebuffer_;
        framebuffer_ = nullptr;
    }
    
    if (screen_texture_id_) {
        glDeleteTextures(1, &screen_texture_id_);
        screen_texture_id_ = 0;
    }
}

// ============================================================================
// System Switching
// ============================================================================

void SimpleSystemGUI::teardown_current_system() {
    if (system_) {
        printf("Tearing down current system: %s\n", system_->get_descriptor().name);
        system_->shutdown();
        system_.reset();
    }
    
    // Free framebuffer
    free_framebuffer();
    
    // Reset emulation state
    emulation_running_ = false;
    emulation_paused_ = false;
    total_frames_ = 0;
    actual_fps_ = 0;
}
void SimpleSystemGUI::switch_system(const char* system_name, int memory_option, int region_option, const std::map<std::string, bool>* peripherals) {
    if (!system_name) {
        printf("ERROR: switch_system called with null system name\n");
        return;
    }
    
    printf("Switching to system: %s (memory=%d, region=%d)\n", system_name, memory_option, region_option);
    
    // Teardown current system
    teardown_current_system();
    
    // Create new system
    system_ = SystemRegistry::instance().create_system_by_name(system_name);
    
    if (!system_) {
        printf("ERROR: Failed to create system: %s\n", system_name);
        return;
    }
    
    printf("Created system: %s (%s)\n",
           system_->get_descriptor().name,
           system_->get_descriptor().short_name);
    
    // Apply configuration if provided
    if (memory_option >= 0 || region_option >= 0 || peripherals) {
        SystemConfiguration config;
        config.memory_option_index = memory_option >= 0 ? memory_option : 0;
        config.region_option_index = region_option >= 0 ? region_option : 0;
        
        // Apply peripheral selections if provided
        if (peripherals) {
            config.enabled_peripherals = *peripherals;
            printf("Applied %zu peripheral selections\n", peripherals->size());
        }
        
        if (!system_->set_configuration(config)) {
            printf("WARNING: Failed to set configuration\n");
        } else {
            printf("Applied configuration: memory option %d, region option %d\n",
                   config.memory_option_index, config.region_option_index);
        }
        
        if (!system_->apply_configuration()) {
            printf("WARNING: Failed to apply configuration\n");
        }
    }
    
    // Initialize the system
    if (!system_->initialize()) {
        printf("ERROR: Failed to initialize %s system\n",
               system_->get_descriptor().name);
        system_.reset();
        return;
    }
    
    // Allocate framebuffer for new system
    allocate_framebuffer();
    
    // Start emulation
    emulation_running_ = true;
    emulation_paused_ = false;
    
    printf("Successfully switched to %s\n", system_->get_descriptor().name);
}

// ============================================================================
// File Loading
// ============================================================================

void SimpleSystemGUI::load_file_dialog() {
    if (!system_) return;
    
#ifdef HAS_IMGUIFILEDIALOG
    // Build filter from system descriptor
    const auto& desc = system_->get_descriptor();
    std::string filter_str;
    if (desc.supported_extensions && desc.supported_extensions[0]) {
        for (int i = 0; desc.supported_extensions[i] != nullptr; i++) {
            if (i > 0) filter_str += ",";
            // Remove leading dot if present
            const char* ext = desc.supported_extensions[i];
            if (ext[0] == '.') ext++;
            filter_str += ".";
            filter_str += ext;
        }
    } else {
        filter_str = ".*"; // All files if no extensions specified
    }
    
    // Extract directory and filename from last selected path
    std::string default_path = ".";
    std::string default_filename;
    
    if (!last_file_path_.empty()) {
        // Find the last path separator
        size_t last_sep = last_file_path_.find_last_of("/\\");
        if (last_sep != std::string::npos) {
            default_path = last_file_path_.substr(0, last_sep);
            default_filename = last_file_path_.substr(last_sep + 1);
        }
    }
    
    // Open the dialog with default config pointing to last location
    IGFD::FileDialogConfig config;
    config.path = default_path;
    config.fileName = default_filename;
    ImGuiFileDialog::Instance()->OpenDialog("ChooseFileDlgKey", "Choose File", filter_str.c_str(), config);
#else
    printf("ImGuiFileDialog not available - file loading disabled\n");
#endif
}
