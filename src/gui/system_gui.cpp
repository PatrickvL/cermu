#include "system_gui.h"
#include "connector_icons.h"
#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"
#include "../core/config/path_discovery.h"
#include "../core/formats/format_handler.h"
#include "../devices/storage/drive_1541.h"
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

SystemGUI::SystemGUI(std::unique_ptr<EmulatedSystem> system, const char* pending_file)
    : GenericEmulatorGUI()
    , system_(std::move(system))
    , framebuffer_(nullptr)
    , fb_width_(0)
    , fb_height_(0)
    , emulation_running_(system_ != nullptr)  // Only run if we have a system
    , emulation_paused_(false)
    , speed_multiplier_(1.0f)
    , screen_textures_{0, 0}
    , texture_write_idx_(0)
    , total_frames_(0)
    , actual_fps_(0)
    , last_fps_time_(0)
    , last_fps_frame_count_(0)
    , frame_pace_counter_(0)
    , frame_time_accumulator_(0.0)
    , system_selection_dialog_()
    , pending_file_path_(pending_file ? pending_file : "")
    , audio_device_(0)
    , audio_sample_rate_(0)
{
    if (system_) {
        // System already initialised + file loaded before entering the GUI,
        // so clear the pending file path — it must not survive into a later
        // system switch (otherwise the new system would try to load it).
        pending_file_path_.clear();
        printf("SystemGUI created for system: %s\n",
               system_->get_descriptor().name);
    } else {
        printf("SystemGUI created without system - selection dialog will be shown\n");
        system_selection_dialog_.open();  // Open dialog if no system provided
    }
}

SystemGUI::~SystemGUI() {
    close_audio_device();
    teardown_current_system();
    ConnectorIcons::cleanup();
}

// ============================================================================
// Initialization Override
// ============================================================================

bool SystemGUI::init(const char* window_title, int width, int height) {
    // Call base class init to create OpenGL context
    if (!GenericEmulatorGUI::init(window_title, width, height)) {
        return false;
    }
    
    // Now that OpenGL context exists, allocate framebuffer and create texture
    allocate_framebuffer();

    // Create connector icon textures (shared across all systems)
    ConnectorIcons::init();

    // Open SDL audio for the current system (if it has audio)
    open_audio_device();

    return true;
}

// ============================================================================
// Virtual Hook Implementations
// ============================================================================

void SystemGUI::handle_events() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        ImGui_ImplSDL2_ProcessEvent(&event);
        
        // Handle F11 for fullscreen toggle (before other processing)
        if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_F11) {
            Uint32 flags = SDL_GetWindowFlags(get_window());
            if (flags & SDL_WINDOW_FULLSCREEN_DESKTOP) {
                SDL_SetWindowFullscreen(get_window(), 0);
            } else {
                SDL_SetWindowFullscreen(get_window(), SDL_WINDOW_FULLSCREEN_DESKTOP);
            }
        }
        
        // Handle quit events
        if (event.type == SDL_QUIT ||
            (event.type == SDL_WINDOWEVENT &&
             event.window.event == SDL_WINDOWEVENT_CLOSE &&
             event.window.windowID == SDL_GetWindowID(get_window()))) {
            should_quit_ = true;
        }
        
        // Release all keys on window focus loss to prevent stuck keys
        if (event.type == SDL_WINDOWEVENT &&
            event.window.event == SDL_WINDOWEVENT_FOCUS_LOST &&
            system_) {
            system_->release_all_keys();
        }
        
        // Forward keyboard events to system only when ImGui doesn't want input
        // This prevents conflicts with ImGui dialogs (like file browser) that need keyboard input
        ImGuiIO& io = ImGui::GetIO();
        if (system_ && !io.WantCaptureKeyboard) {
            if (event.type == SDL_KEYDOWN) {
                system_->handle_keyboard_event_ex(
                    event.key.keysym.sym,
                    event.key.keysym.scancode,
                    event.key.keysym.mod,
                    true,
                    event.key.repeat != 0);
            } else if (event.type == SDL_KEYUP) {
                system_->handle_keyboard_event_ex(
                    event.key.keysym.sym,
                    event.key.keysym.scancode,
                    event.key.keysym.mod,
                    false,
                    false);
            } else if (event.type == SDL_TEXTINPUT) {
                system_->handle_text_input(event.text.text);
            }
        }

        // Route SDL events to attached peripheral devices (joystick, mouse, etc.)
        // Keyboard events are only forwarded when ImGui doesn't claim keyboard focus.
        // Mouse/controller events are only forwarded when ImGui doesn't claim mouse focus.
        if (system_) {
            ImGuiIO& io2 = ImGui::GetIO();
            bool is_keyboard_event =
                (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP);
            bool is_mouse_event =
                (event.type == SDL_MOUSEMOTION ||
                 event.type == SDL_MOUSEBUTTONDOWN ||
                 event.type == SDL_MOUSEBUTTONUP);

            bool should_forward = true;
            if (is_keyboard_event && io2.WantCaptureKeyboard) should_forward = false;
            if (is_mouse_event && io2.WantCaptureMouse)       should_forward = false;

            if (should_forward) {
                system_->process_sdl_event_for_devices(event);
            }
        }
    }
    
    // Check if the system requested application exit (e.g. ESC in SID player)
    if (system_ && system_->is_quit_requested()) {
        should_quit_ = true;
    }
}

void SystemGUI::update_frame() {
    if (!system_ || !emulation_running_ || emulation_paused_) {
        // Reset pacing when not running so we don't accumulate stale time
        frame_pace_counter_ = 0;
        frame_time_accumulator_ = 0.0;
        return;
    }
    
    // High-resolution timing for frame pacing
    uint64_t now = SDL_GetPerformanceCounter();
    
    // First frame: initialize counter and run one frame
    if (frame_pace_counter_ == 0) {
        frame_pace_counter_ = now;
        system_->run_frame();
        total_frames_++;
        update_fps();
        return;
    }
    
    // Calculate elapsed real time since last update
    double freq = static_cast<double>(SDL_GetPerformanceFrequency());
    double elapsed = static_cast<double>(now - frame_pace_counter_) / freq;
    frame_pace_counter_ = now;
    
    // Accumulate real time
    frame_time_accumulator_ += elapsed;
    
    // Target time per emulation frame based on system's target FPS (PAL=50, NTSC=60)
    double target_frame_time = 1.0 / system_->get_target_fps();
    
    // Cap accumulator to prevent death spiral after lag spikes (max 3 frames catch-up)
    double max_accumulator = target_frame_time * 3.0;
    if (frame_time_accumulator_ > max_accumulator) {
        frame_time_accumulator_ = max_accumulator;
    }
    
    // Run emulation frames as needed to keep in sync with real time
    while (frame_time_accumulator_ >= target_frame_time) {
        system_->run_frame();
        total_frames_++;
        frame_time_accumulator_ -= target_frame_time;
    }
    
    // Update FPS counter
    update_fps();
}

void SystemGUI::render_frame() {
    begin_frame();
    
    // Only render dialog if it's actually open
    if (system_selection_dialog_.is_open()) {
        system_selection_dialog_.render(system_ != nullptr);  // Allow cancel/close only when a system is already active
        
        // Check if dialog selection was confirmed
        if (system_selection_dialog_.selection_confirmed()) {
            const char* selected = system_selection_dialog_.get_selected_system();
            int memory_opt = system_selection_dialog_.get_selected_memory_option();
            int region_opt = system_selection_dialog_.get_selected_region_option();
            const auto& peripherals = system_selection_dialog_.get_selected_peripherals();
            const auto& custom_settings = system_selection_dialog_.get_selected_custom_settings();
            if (selected) {
                // Pass pending file so switch_system applies config before init
                const char* pf = pending_file_path_.empty() ? nullptr : pending_file_path_.c_str();
                switch_system(selected, memory_opt, region_opt, &peripherals, pf, &custom_settings);
                pending_file_path_.clear();
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
            
            // Auto-detect optimal configuration (e.g. memory expansion) from file.
            // Never downgrades from the user's current selection — only increases.
            if (system_) {
                system_->apply_file_configuration(filePathName.c_str());
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

    // Display drive insert disk dialog and handle results
    if (ImGuiFileDialog::Instance()->Display("DriveInsertDiskKey")) {
        if (ImGuiFileDialog::Instance()->IsOk() && pending_drive_insert_) {
            std::string filePathName = ImGuiFileDialog::Instance()->GetFilePathName();
            printf("Drive insert: user selected file: %s\n", filePathName.c_str());

            // Save the last selected file path for next time
            last_file_path_ = filePathName;

            // Insert disk into the drive (this auto-populates the fliplist)
            if (pending_drive_insert_->insert_disk(filePathName.c_str())) {
                printf("Disk inserted successfully into drive %d: %s\n",
                       pending_drive_insert_->get_device_number(), filePathName.c_str());
            } else {
                printf("Failed to insert disk into drive %d: %s\n",
                       pending_drive_insert_->get_device_number(), filePathName.c_str());
            }
        } else {
            // User canceled - save the current path they were browsing
            std::string currentPath = ImGuiFileDialog::Instance()->GetCurrentPath();
            if (!currentPath.empty()) {
                last_file_path_ = currentPath;
            }
        }
        pending_drive_insert_ = nullptr;
        ImGuiFileDialog::Instance()->Close();
    }

    // Poll attached drives for file dialog requests
    poll_drive_file_dialog_requests();
#endif
    
    // Render screen (full-screen background)
    if (show_screen_ && system_) {
        render_screen();
    }
    
    // Render menu bar (on top of screen) - only if not in fullscreen mode
    Uint32 window_flags = SDL_GetWindowFlags(get_window());
    if (!(window_flags & SDL_WINDOW_FULLSCREEN_DESKTOP)) {
        render_menu_bar();
    }
    
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

void SystemGUI::render_menu_bar() {
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
    
    // Hardware menu — chip submenus with live preview + detach-to-window
    if (system_) {
        auto& chips = system_->get_registered_chips();
        if (!chips.empty()) {
            if (ImGui::BeginMenu("Hardware")) {
                for (size_t i = 0; i < chips.size(); i++) {
                    auto& sc = chips[i];
                    bool has_content = sc.chip && (
                        sc.chip->has_debug_content() ||
                        sc.chip->has_layout_content() ||
                        sc.chip->has_settings_content());

                    if (!has_content) {
                        ImGui::TextDisabled("%s", sc.display_name);
                        continue;
                    }

                    ImGui::PushID(static_cast<int>(i));

                    // Each chip with content opens as a submenu on hover,
                    // showing combined layout+debug+settings inline.
                    // Set minimum size so chip layout + debug content has room.
                    ImGui::SetNextWindowSizeConstraints(
                        ImVec2(600.0f, 200.0f),   // min
                        ImVec2(FLT_MAX, FLT_MAX)  // max (unconstrained)
                    );
                    if (ImGui::BeginMenu(sc.display_name)) {
                        if (sc.chip->has_debug_content()) {
                            sc.chip->render_debug_content();
                        } else if (sc.chip->has_layout_content()) {
                            sc.chip->render_layout_content();
                        }

                        if (sc.chip->has_settings_content()) {
                            ImGui::Separator();
                            if (ImGui::CollapsingHeader("Settings")) {
                                sc.chip->render_settings_content();
                            }
                        }

                        ImGui::Separator();
                        if (sc.show_detached) {
                            ImGui::TextDisabled("(already detached)");
                        } else if (ImGui::Button("Detach Window")) {
                            sc.show_detached = 1;
                            ImGui::CloseCurrentPopup();
                        }

                        ImGui::EndMenu();
                    }

                    ImGui::PopID();
                }

                ImGui::EndMenu();
            }
        }
    }
    
    // View menu
    if (ImGui::BeginMenu("View")) {
        render_view_menu_generic();
        ImGui::EndMenu();
    }
    
    // Screen menu
    if (ImGui::BeginMenu("Screen")) {
        render_screen_menu_generic();
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
    
    // Status bar on the right — connector icons + text status
    if (system_) {
        // Position connector icons and status text right-aligned.
        // Layout:  [menus...]   [connector icons]  [status text]
        const float status_text_w = 350.0f;  // approx. width for status text
        const float bar_width = ImGui::GetWindowWidth();

        // Render connector icons first (they need to calculate their width)
        float icons_start = bar_width - status_text_w - 8.0f;

        // Count external ports to estimate icon area width
        int ext_port_count = 0;
        for (auto& p : system_->get_connector_ports())
            if (!p->get_definition().is_internal) ext_port_count++;
        float icon_area_w = ext_port_count > 0
            ? (ext_port_count * 24.0f + (ext_port_count - 1) * 2.0f + 8.0f)
            : 0.0f;

        if (ext_port_count > 0) {
            ImGui::SameLine(icons_start - icon_area_w);
            system_->render_connector_menu_bar_icons();
        }

        ImGui::SameLine(bar_width - status_text_w);
        ImGui::Text("%s", system_->get_descriptor().short_name);
        ImGui::SameLine();
        if (system_->is_system_ready()) {
            ImGui::Text("Cycles: %llu", (unsigned long long)system_->get_total_cycles());
            ImGui::SameLine();
            ImGui::Text("FPS: %u", actual_fps_);
            ImGui::SameLine();
            ImGui::Text("%s", emulation_paused_ ? "Paused" : 
                             emulation_running_ ? "Running" : "Stopped");
        } else {
            ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.2f, 1.0f), "No ROM loaded");
        }
    } else {
        ImGui::SameLine(ImGui::GetWindowWidth() - 200);
        ImGui::Text("No system loaded");
    }
    
    ImGui::EndMainMenuBar();
}

void SystemGUI::render_screen() {
    if (!system_) return;
    
    // Get viewport for fullscreen rendering
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize(viewport->Size);
    
    // Detect and update host DPI scale if needed
    ImGuiIO& io = ImGui::GetIO();
    if (host_dpi_scale_ <= 0.0f) {
        host_dpi_scale_ = io.DisplayFramebufferScale.x > 0.0f
                            ? io.DisplayFramebufferScale.x
                            : 1.0f;
    }
    
    // Fullscreen window flags
    // NoMouseInputs: the screen window is display-only; without this flag
    // ImGui sets WantCaptureMouse=true when the mouse hovers the display,
    // which blocks SDL mouse events from reaching peripheral devices
    // (e.g. lightpen needs mouse position and button state).
    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoBackground |
        ImGuiWindowFlags_NoMouseInputs;
    
    ImGui::Begin("##Screen", nullptr, flags);
    
    // Get system framebuffer
    uint32_t* fb = system_->get_framebuffer();
    if (fb && screen_textures_[0]) {
        // Double-buffered texture upload: write to the current write
        // texture while the GPU may still be reading from the other one.
        GLuint upload_tex = screen_textures_[texture_write_idx_];
        update_screen_texture(upload_tex, fb_width_, fb_height_, fb);
        
        // Get hardware traits to determine PAL/NTSC (default to PAL for most systems)
        const auto& traits = system_->get_hardware_traits();
        bool is_pal = true;  // Default to PAL, systems can override via traits
        bool use_pixel_aspect = true;  // Use pixel aspect correction by default
        
        // Calculate display dimensions with full aspect ratio support
        float display_w, display_h, pos_x, pos_y;
        calculate_display_dimensions(
            viewport->Size.x, viewport->Size.y,
            (float)fb_width_, (float)fb_height_,
            is_pal, use_pixel_aspect,
            &display_w, &display_h, &pos_x, &pos_y);
        
        // Set cursor position and render from the just-uploaded texture
        ImGui::SetCursorPos(ImVec2(pos_x, pos_y));
        ImGui::Image((ImTextureID)(intptr_t)upload_tex,
                    ImVec2(display_w, display_h));
        
        // Store display rect in SDL window coordinates for peripheral devices
        // (e.g. lightpen uses this to map mouse position to emulated screen)
        if (system_) {
            auto item_min = ImGui::GetItemRectMin();
            auto item_max = ImGui::GetItemRectMax();
            system_->set_display_screen_rect(
                item_min.x, item_min.y,
                item_max.x - item_min.x, item_max.y - item_min.y);
        }

        // Swap write index for next frame
        texture_write_idx_ ^= 1;
        // Keep base-class id in sync for filter-change code
        screen_texture_id_ = screen_textures_[texture_write_idx_];
    }
    
    ImGui::End();
}

void SystemGUI::render_memory_viewer() {
    if (!ImGui::Begin("Memory Viewer", &show_memory_viewer_)) {
        ImGui::End();
        return;
    }
    
    ImGui::Text("Memory viewer not yet implemented for generic systems");
    ImGui::Text("System-specific memory viewers can be added via system callbacks");
    
    ImGui::End();
}

void SystemGUI::render_settings() {
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

        // Generic peripheral connector UI (available for all systems)
        system_->render_peripheral_connector_ui();
    }
    
    ImGui::End();
}

void SystemGUI::render_about() {
    render_about_dialog_generic();
}

// ============================================================================
// System Control
// ============================================================================

void SystemGUI::start_emulation() {
    emulation_running_ = true;
    emulation_paused_ = false;
    reset_frame_pacing();
    printf("Emulation started\n");
}

void SystemGUI::pause_emulation() {
    emulation_paused_ = true;
    printf("Emulation paused\n");
}

void SystemGUI::reset_emulation() {
    if (system_) {
        system_->reset();
        total_frames_ = 0;
        reset_frame_pacing();
        // Ensure emulation is running after reset — the user may have
        // triggered reset while emulation was paused (e.g. after a failed
        // file load or from the file-dialog flow that pauses first).
        if (!emulation_running_ || emulation_paused_) {
            start_emulation();
        }
        printf("System reset\n");
    }
}

void SystemGUI::step_emulation() {
    if (system_ && emulation_paused_) {
        system_->tick();
        printf("Single step executed\n");
    }
}

// ============================================================================
// Helper Functions
// ============================================================================

void SystemGUI::update_fps() {
    // Count emulated frames completed this second (not main loop iterations).
    // total_frames_ is incremented once per run_frame() call, so the delta
    // over one second gives the true emulated FPS.
    uint32_t current_time = SDL_GetTicks();
    if (last_fps_time_ == 0) {
        last_fps_time_ = current_time;
        last_fps_frame_count_ = total_frames_;
    }
    
    if (current_time - last_fps_time_ >= 1000) {
        actual_fps_ = total_frames_ - last_fps_frame_count_;
        last_fps_frame_count_ = total_frames_;
        last_fps_time_ = current_time;
    }
}

void SystemGUI::reset_frame_pacing() {
    frame_pace_counter_ = 0;
    frame_time_accumulator_ = 0.0;
}

void SystemGUI::allocate_framebuffer() {
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
    
    // Create double-buffered OpenGL textures.
    // Two textures let us upload to one while the GPU may still be
    // reading from the other for the previous frame's draw call,
    // avoiding driver-level stalls or hidden copies.
    if (window_) {
        screen_textures_[0] = create_screen_texture(fb_width_, fb_height_);
        screen_textures_[1] = create_screen_texture(fb_width_, fb_height_);
        texture_write_idx_ = 0;
        // Keep base-class id pointing at the first texture for legacy code
        screen_texture_id_ = screen_textures_[0];
        printf("Allocated %dx%d framebuffer with double-buffered textures %u/%u\n",
               fb_width_, fb_height_, screen_textures_[0], screen_textures_[1]);
    } else {
        printf("Allocated %dx%d framebuffer (texture creation deferred until init)\n", fb_width_, fb_height_);
    }
}

void SystemGUI::free_framebuffer() {
    if (framebuffer_) {
        delete[] framebuffer_;
        framebuffer_ = nullptr;
    }
    
    // Delete double-buffered textures
    for (int i = 0; i < 2; i++) {
        if (screen_textures_[i]) {
            glDeleteTextures(1, &screen_textures_[i]);
            screen_textures_[i] = 0;
        }
    }
    screen_texture_id_ = 0;
}

// ============================================================================
// System Switching
// ============================================================================

void SystemGUI::teardown_current_system() {
    // Stop audio before destroying the system (callback references system_)
    close_audio_device();

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
    reset_frame_pacing();
}
void SystemGUI::switch_system(const char* system_name, int memory_option, int region_option, const std::map<std::string, bool>* peripherals, const char* pending_file, const std::map<std::string, std::string>* custom_settings) {
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
        
        // Apply custom settings if provided
        if (custom_settings) {
            config.custom_settings = *custom_settings;
            for (const auto& [key, value] : *custom_settings) {
                printf("Custom setting: %s = %s\n", key.c_str(), value.c_str());
            }
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
    
    // If a pending file was provided, apply its configuration BEFORE init
    // so memory expansion / region are set up correctly
    if (pending_file) {
        system_->apply_file_configuration(pending_file);
    }
    
    // Initialize the system
    if (!system_->initialize()) {
        printf("ERROR: Failed to initialize %s system\n",
               system_->get_descriptor().name);
        system_.reset();
        return;
    }

    // Auto-bind available host input devices (gamepads, mouse) to peripherals
    system_->auto_bind_host_inputs();

    // Allocate framebuffer for new system
    allocate_framebuffer();

    // Open audio for the new system
    open_audio_device();

    // Load pending file after system is fully initialized
    if (pending_file) {
        printf("Loading pending file into %s: %s\n",
               system_->get_descriptor().short_name, pending_file);
        if (system_->load_file(pending_file)) {
            printf("Pending file loaded successfully\n");
        } else {
            printf("Failed to load pending file: %s\n", pending_file);
        }
    }

    // Start emulation
    emulation_running_ = true;
    emulation_paused_ = false;

    // Update window title with system name
    char title_buf[256];
    snprintf(title_buf, sizeof(title_buf), "cermu — %s",
             system_->get_descriptor().name);
    SDL_SetWindowTitle(get_window(), title_buf);

    printf("Successfully switched to %s\n", system_->get_descriptor().name);
}

// ============================================================================
// File Loading
// ============================================================================

void SystemGUI::load_file_dialog() {
    open_file_dialog("ChooseFileDlgKey", "Choose File");
}

void SystemGUI::open_file_dialog(const char* dialog_key, const char* title) {
    if (!system_) return;
    
#ifdef HAS_IMGUIFILEDIALOG
    // Build filter from system descriptor using ImGuiFileDialog collection syntax
    // Collection format: "Title{.ext1,.ext2,...}" shows ALL matching files at once
    // Individual filters separated by commas create separate dropdown entries
    const auto& desc = system_->get_descriptor();
    std::string filter_str;
    if (desc.supported_formats && desc.supported_formats[0]) {
        // Build filter from format descriptors using format_list_dialog_filter()
        filter_str = format_list_dialog_filter(desc.supported_formats,
                                               desc.short_name ? desc.short_name : "System");
    } else {
        filter_str = ".*"; // All files if no formats specified
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
    } else if (system_) {
        // Default to the system-specific data folder (where ROMs live)
        const char* short_name = system_->get_descriptor().short_name;
        if (short_name) {
            // Convert to lowercase for data folder lookup (e.g. "VIC20" -> "vic20")
            std::string sys_lower;
            for (const char* p = short_name; *p; ++p)
                sys_lower += (char)tolower((unsigned char)*p);
            char data_root[1024];
            if (system_config_discover_data_root(sys_lower.c_str(), data_root, sizeof(data_root))) {
                default_path = data_root;
                printf("File dialog defaulting to data folder: %s\n", data_root);
            }
        }
    }
    
    // Open the dialog with case-insensitive extension filtering
    IGFD::FileDialogConfig config;
    config.path = default_path;
    config.fileName = default_filename;
    config.flags = ImGuiFileDialogFlags_CaseInsensitiveExtentionFiltering;
    ImGuiFileDialog::Instance()->OpenDialog(dialog_key, title, filter_str.c_str(), config);
#else
    (void)dialog_key;
    (void)title;
    printf("ImGuiFileDialog not available - file loading disabled\n");
#endif
}

// ============================================================================
// Drive File Dialog Polling
// ============================================================================

void SystemGUI::poll_drive_file_dialog_requests() {
#ifdef HAS_IMGUIFILEDIALOG
    if (!system_ || pending_drive_insert_) return;  // Already have a pending request

    // Scan IEC bus devices for any 1541 drive that wants a file dialog
    for (auto& port : system_->get_connector_ports()) {
        if (!port->get_definition().is_bus) continue;
        for (auto* dev : port->get_attached_devices()) {
            auto* drive = dynamic_cast<Drive1541Device*>(dev);
            if (drive && drive->wants_file_dialog()) {
                drive->clear_file_dialog_request();
                pending_drive_insert_ = drive;
                open_file_dialog("DriveInsertDiskKey", "Insert Disk");
                return;
            }
        }
    }
#endif
}

// ============================================================================
// Audio Output
// ============================================================================

void SystemGUI::sdl_audio_callback(void* userdata, uint8_t* stream, int len) {
    SystemGUI* gui = static_cast<SystemGUI*>(userdata);
    int sample_count = len / static_cast<int>(sizeof(float));
    float* out = reinterpret_cast<float*>(stream);

    uint32_t written = 0;
    if (gui->system_ && gui->emulation_running_ && !gui->emulation_paused_) {
        written = gui->system_->get_audio_samples(out, static_cast<uint32_t>(sample_count));
    }
    // Fill remainder with silence
    for (uint32_t i = written; i < static_cast<uint32_t>(sample_count); i++) {
        out[i] = 0.0f;
    }
}

void SystemGUI::open_audio_device() {
    close_audio_device();

    if (!system_) return;

    const AudioTraits& traits = system_->get_audio_traits();
    if (traits.format == AudioFormat::NONE || traits.sample_rate_hz == 0) {
        printf("Audio: system reports no audio\n");
        return;
    }

    SDL_AudioSpec want = {};
    want.freq = traits.sample_rate_hz;
    want.format = AUDIO_F32SYS;   // always request float; conversion happens in get_audio_samples
    want.channels = 1;            // mono — systems mix down to mono
    want.samples = 1024;          // ~23 ms at 44100 Hz
    want.callback = sdl_audio_callback;
    want.userdata = this;

    SDL_AudioSpec have = {};
    audio_device_ = SDL_OpenAudioDevice(nullptr, 0, &want, &have,
                                         SDL_AUDIO_ALLOW_FREQUENCY_CHANGE);
    if (audio_device_ == 0) {
        printf("Audio: SDL_OpenAudioDevice failed: %s\n", SDL_GetError());
        return;
    }

    audio_sample_rate_ = have.freq;
    printf("Audio: opened device — requested %d Hz, got %d Hz (buffer %d samples)\n",
           want.freq, have.freq, have.samples);

    // If SDL negotiated a different sample rate (common on Linux with
    // PipeWire/PulseAudio), update the system's audio generator to match.
    if (have.freq != want.freq && system_) {
        system_->set_audio_sample_rate(have.freq);
    }

    // Unpause — SDL audio devices start paused
    SDL_PauseAudioDevice(audio_device_, 0);
}

void SystemGUI::close_audio_device() {
    if (audio_device_ != 0) {
        SDL_CloseAudioDevice(audio_device_);
        audio_device_ = 0;
        audio_sample_rate_ = 0;
        printf("Audio: device closed\n");
    }
}
