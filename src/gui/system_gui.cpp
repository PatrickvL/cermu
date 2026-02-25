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
#include <chrono>
#include <ctime>

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
    , system_selection_dialog_()
    , pending_file_path_(pending_file ? pending_file : "")
{
    // Set emulation running if we already have a system loaded
    if (system_) {
        emulation_running_.store(true);
    }
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
    stop_emu_thread();
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

    // Start the emulation thread (runs independently of the GUI loop)
    start_emu_thread();

    // Set window title with system name and loaded program (if any)
    update_window_title();

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
            // Queue a synthetic focus-loss event for the emu thread
            std::lock_guard<std::mutex> lock(input_mutex_);
            input_queue_.push_back(event);
        }
        
        // Queue keyboard/text/mouse/controller events for the emulation thread.
        // ImGui capture filtering is applied HERE on the GUI thread so the emu
        // thread doesn't need to know about ImGui state.
        if (system_) {
            ImGuiIO& io = ImGui::GetIO();

            bool is_keyboard_event =
                (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP ||
                 event.type == SDL_TEXTINPUT);
            bool is_mouse_event =
                (event.type == SDL_MOUSEMOTION ||
                 event.type == SDL_MOUSEBUTTONDOWN ||
                 event.type == SDL_MOUSEBUTTONUP);
            bool is_controller_event =
                (event.type == SDL_JOYAXISMOTION ||
                 event.type == SDL_JOYBUTTONDOWN ||
                 event.type == SDL_JOYBUTTONUP ||
                 event.type == SDL_JOYHATMOTION ||
                 event.type == SDL_CONTROLLERAXISMOTION ||
                 event.type == SDL_CONTROLLERBUTTONDOWN ||
                 event.type == SDL_CONTROLLERBUTTONUP);

            bool should_queue = false;
            if (is_keyboard_event && !io.WantCaptureKeyboard) should_queue = true;
            if (is_mouse_event && !io.WantCaptureMouse)       should_queue = true;
            if (is_controller_event)                          should_queue = true;

            if (should_queue) {
                std::lock_guard<std::mutex> lock(input_mutex_);
                input_queue_.push_back(event);
            }
        }
    }
    
    // Check if the system requested application exit (e.g. ESC in SID player)
    if (system_ && system_->is_quit_requested()) {
        should_quit_ = true;
    }
}

void SystemGUI::update_frame() {
    // Emulation now runs on a separate thread (emu_thread_func).
    // The GUI thread only updates the FPS counter from the atomic frame count.
    update_fps();

    // Refresh window title periodically — systems may update program_title_,
    // mode label, or subtitle info asynchronously (e.g. subtune switches).
    update_window_title();
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
            
            // Stop emulation thread while loading for exclusive system access
            bool was_running = emulation_running_.load() && !emulation_paused_.load();
            stop_emu_thread();
            
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
                
                // Update window title with loaded program name
                update_window_title();
                
                // Ensure emulation is running after successful file load
                emulation_running_.store(true);
                emulation_paused_.store(false);
            } else {
                printf("Failed to load file: %s\n", filePathName.c_str());
                
                // Resume previous state if load failed
                if (was_running) {
                    emulation_running_.store(true);
                    emulation_paused_.store(false);
                }
            }
            
            // Restart emulation thread
            start_emu_thread();
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
            {
                std::lock_guard<std::mutex> lock(emu_mutex_);
                if (pending_drive_insert_->insert_disk(filePathName.c_str())) {
                    printf("Disk inserted successfully into drive %d: %s\n",
                           pending_drive_insert_->get_device_number(), filePathName.c_str());
                } else {
                    printf("Failed to insert disk into drive %d: %s\n",
                           pending_drive_insert_->get_device_number(), filePathName.c_str());
                }
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
    
    // Let system render its debug windows (try_lock: skip if emu thread is busy)
    if (system_) {
        std::unique_lock<std::mutex> lock(emu_mutex_, std::try_to_lock);
        if (lock.owns_lock()) {
            system_->render_debug_windows(nullptr);
        }
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
                std::unique_lock<std::mutex> lock(emu_mutex_, std::try_to_lock);
                if (lock.owns_lock()) {
                    system_->set_speed_multiplier(speed_multiplier_);
                }
            }
        }
        
        ImGui::EndDisabled();
        
        // System-specific menu items (if system is loaded)
        if (system_) {
            ImGui::Separator();
            std::unique_lock<std::mutex> lock(emu_mutex_, std::try_to_lock);
            if (lock.owns_lock()) {
                system_->render_system_menu_items();
            }
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
                    // Lock popup size after first render so it doesn't grow
                    // as register values change width over time.
                    if (sc.submenu_locked_w > 0.0f) {
                        // Already measured — force fixed size
                        ImGui::SetNextWindowSizeConstraints(
                            ImVec2(sc.submenu_locked_w, sc.submenu_locked_h),
                            ImVec2(sc.submenu_locked_w, sc.submenu_locked_h));
                    } else {
                        // First open — auto-size with a minimum floor
                        ImGui::SetNextWindowSizeConstraints(
                            ImVec2(600.0f, 200.0f),
                            ImVec2(FLT_MAX, FLT_MAX));
                    }
                    if (ImGui::BeginMenu(sc.display_name)) {
                        // Pin button at top-right to detach into a standalone window
                        {
                            float avail = ImGui::GetContentRegionAvail().x;
                            float btn_h = ImGui::GetFrameHeight();
                            float btn_w = btn_h; // square
                            ImVec2 cursor = ImGui::GetCursorPos();
                            ImGui::SetCursorPosX(cursor.x + avail - btn_w);

                            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
                            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetStyleColorVec4(ImGuiCol_HeaderHovered));

                            // Draw a pin/thumbtack shape via the draw list
                            ImVec2 btn_pos = ImGui::GetCursorScreenPos();
                            bool already = sc.show_detached != 0;
                            if (ImGui::InvisibleButton("##pin", ImVec2(btn_w, btn_h)) && !already) {
                                sc.show_detached = 1;
                                ImGui::CloseCurrentPopup();
                            }
                            bool hovered = ImGui::IsItemHovered();

                            // Highlight on hover
                            if (hovered && !already) {
                                ImDrawList* dl = ImGui::GetWindowDrawList();
                                dl->AddRectFilled(btn_pos,
                                    ImVec2(btn_pos.x + btn_w, btn_pos.y + btn_h),
                                    ImGui::GetColorU32(ImGuiCol_HeaderHovered),
                                    ImGui::GetStyle().FrameRounding);
                            }

                            // Draw pin icon
                            {
                                ImDrawList* dl = ImGui::GetWindowDrawList();
                                ImVec2 center(btn_pos.x + btn_w * 0.5f, btn_pos.y + btn_h * 0.5f);
                                float r = btn_h * 0.28f;
                                ImU32 col = already
                                    ? ImGui::GetColorU32(ImGuiCol_TextDisabled)
                                    : (hovered
                                        ? ImGui::GetColorU32(ImGuiCol_Text)
                                        : ImGui::GetColorU32(ImGuiCol_TextDisabled));

                                // Pin head (circle)
                                dl->AddCircleFilled(ImVec2(center.x, center.y - r * 0.3f), r, col);
                                // Pin needle (line down from head)
                                dl->AddLine(
                                    ImVec2(center.x, center.y - r * 0.3f + r),
                                    ImVec2(center.x, center.y + r * 1.4f),
                                    col, 2.0f);
                            }

                            ImGui::PopStyleColor(2);
                            if (hovered) {
                                ImGui::SetTooltip(already ? "Already detached" : "Detach to window");
                            }
                            ImGui::SetCursorPos(cursor); // restore so content renders from top-left
                        }

                        if (sc.chip->has_debug_content()) {
                            // try_lock: chip state is read-only for rendering;
                            // the emu thread may be updating it concurrently.
                            std::unique_lock<std::mutex> chip_lock(emu_mutex_, std::try_to_lock);
                            if (chip_lock.owns_lock()) {
                                sc.chip->render_debug_content();
                            } else {
                                ImGui::TextDisabled("(updating...)");
                            }
                        } else if (sc.chip->has_layout_content()) {
                            std::unique_lock<std::mutex> chip_lock(emu_mutex_, std::try_to_lock);
                            if (chip_lock.owns_lock()) {
                                sc.chip->render_layout_content();
                            } else {
                                ImGui::TextDisabled("(updating...)");
                            }
                        }

                        if (sc.chip->has_settings_content()) {
                            ImGui::Separator();
                            if (ImGui::CollapsingHeader("Settings")) {
                                std::unique_lock<std::mutex> chip_lock(emu_mutex_, std::try_to_lock);
                                if (chip_lock.owns_lock()) {
                                    sc.chip->render_settings_content();
                                } else {
                                    ImGui::TextDisabled("(updating...)");
                                }
                            }
                        }

                        // Lock the popup size after the first render so it
                        // stays stable as emulation values change width.
                        if (sc.submenu_locked_w <= 0.0f) {
                            ImVec2 sz = ImGui::GetWindowSize();
                            if (sz.x > 0.0f && sz.y > 0.0f) {
                                sc.submenu_locked_w = sz.x;
                                sc.submenu_locked_h = sz.y;
                            }
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

        ImGui::Separator();
        ImGui::BeginDisabled(!system_ || !framebuffer_);
        if (ImGui::MenuItem("Save Screenshot...")) {
            // Generate timestamped filename
            time_t now = time(nullptr);
            struct tm* tm_info = localtime(&now);
            char filename[128];
            strftime(filename, sizeof(filename),
                     "cermu_%Y%m%d_%H%M%S.png", tm_info);

            std::lock_guard<std::mutex> lock(emu_mutex_);
            if (system_->save_screenshot(filename)) {
                printf("Screenshot saved: %s\n", filename);
            }
        }
        ImGui::EndDisabled();

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
        const float status_text_w = 450.0f;  // approx. width for status text
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
            uint32_t emu_us = emu_frame_time_us_.load(std::memory_order_relaxed);
            ImGui::Text("Cycles: %llu", (unsigned long long)system_->get_total_cycles());
            ImGui::SameLine();
            ImGui::Text("FPS: %u", actual_fps_);
            ImGui::SameLine();
            ImGui::Text("Frame: %.2f ms", emu_us * 0.001);
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
    
    // Read the latest framebuffer snapshot produced by the emulation thread.
    // Only re-upload the texture when a new frame is available; otherwise
    // the GPU keeps displaying the previously uploaded texture.
    bool have_new_frame = fb_new_frame_.exchange(false, std::memory_order_acquire);
    if (have_new_frame && fb_snapshot_ && screen_textures_[0]) {
        // Hold fb_mutex_ for the *entire* texture upload so the emu thread
        // cannot memcpy a new frame into fb_snapshot_ while glTexSubImage2D
        // is reading from it.  The previous code released the lock before
        // update_screen_texture(), creating a data-race window.
        std::lock_guard<std::mutex> lock(fb_mutex_);

        // Double-buffered texture upload: write to the current write
        // texture while the GPU may still be reading from the other one.
        GLuint upload_tex = screen_textures_[texture_write_idx_];
        update_screen_texture(upload_tex, fb_width_, fb_height_, fb_snapshot_);

        // Swap write index for next frame
        texture_write_idx_ ^= 1;
        // Keep base-class id in sync for filter-change code
        screen_texture_id_ = screen_textures_[texture_write_idx_];
    }
    // Always render the most recently uploaded texture (read index = opposite of write)
    if (screen_textures_[0]) {
        GLuint display_tex = screen_textures_[texture_write_idx_ ^ 1];
        
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
        
        // Set cursor position and render from the last-uploaded texture
        ImGui::SetCursorPos(ImVec2(pos_x, pos_y));
        ImGui::Image((ImTextureID)(intptr_t)display_tex,
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
        std::unique_lock<std::mutex> lock(emu_mutex_, std::try_to_lock);
        if (lock.owns_lock()) {
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
        } else {
            ImGui::TextDisabled("(emulation busy)");
        }
    }
    
    ImGui::End();
}

void SystemGUI::render_about() {
    render_about_dialog_generic();
}

// ============================================================================
// System Control
// ============================================================================

void SystemGUI::reset_emulation() {
    if (system_) {
        // Stop the emu thread so we have exclusive access to system_
        stop_emu_thread();

        system_->reset();
        total_frames_.store(0);
        reset_frame_pacing();

        // Restart thread and ensure emulation is running
        emulation_running_.store(true);
        emulation_paused_.store(false);
        start_emu_thread();
        printf("System reset\n");
    }
}

void SystemGUI::step_emulation() {
    if (system_ && emulation_paused_.load()) {
        std::lock_guard<std::mutex> lock(emu_mutex_);
        system_->tick();
        // Snapshot the framebuffer so the GUI sees the result
        if (framebuffer_ && fb_snapshot_) {
            std::lock_guard<std::mutex> flock(fb_mutex_);
            memcpy(fb_snapshot_, framebuffer_,
                   static_cast<size_t>(fb_width_) * fb_height_ * sizeof(uint32_t));
        }
        printf("Single step executed\n");
    }
}

// ============================================================================
// Helper Functions
// ============================================================================

void SystemGUI::update_window_title() {
    if (!system_) {
        if (last_window_title_ != "cermu") {
            last_window_title_ = "cermu";
            SDL_SetWindowTitle(get_window(), "cermu");
        }
        return;
    }

    // Build: cermu - {system} [{mode}] - {title} {subtitle}
    std::string buf = "cermu - ";
    buf += system_->get_descriptor().short_name;

    const char* mode = system_->get_mode_label();
    if (mode) {
        buf += " [";
        buf += mode;
        buf += "]";
    }

    const auto& title = system_->get_program_title();
    if (!title.empty()) {
        buf += " - ";
        buf += title;
    }

    std::string subtitle = system_->get_subtitle_info();
    if (!subtitle.empty()) {
        buf += " ";
        buf += subtitle;
    }

    // Only call SDL if the title actually changed
    if (buf != last_window_title_) {
        last_window_title_ = buf;
        SDL_SetWindowTitle(get_window(), buf.c_str());
    }
}

void SystemGUI::allocate_framebuffer() {
    if (!system_) return;
    
    // Get display dimensions from system
    const auto& traits = system_->get_hardware_traits();
    fb_width_ = traits.display.visible_width;
    fb_height_ = traits.display.visible_height;
    
    size_t fb_bytes = static_cast<size_t>(fb_width_) * fb_height_ * sizeof(uint32_t);

    // Allocate framebuffer (written by emulation thread via run_frame)
    framebuffer_ = new uint32_t[fb_width_ * fb_height_];
    memset(framebuffer_, 0, fb_bytes);
    
    // Allocate snapshot buffer (read by GUI thread for texture upload)
    fb_snapshot_ = new uint32_t[fb_width_ * fb_height_];
    memset(fb_snapshot_, 0, fb_bytes);
    
    // Give the live buffer to the system
    system_->set_framebuffer(framebuffer_, fb_width_, fb_height_);

    // Resize the temporary audio buffer for the emu thread
    // Enough for ~2 frames at 44100 Hz / 50 fps = 1764 samples, rounded up
    emu_audio_tmp_.resize(2048);
    
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

// ============================================================================
// System Switching
// ============================================================================

void SystemGUI::teardown_current_system() {
    // Stop emulation thread before touching system_
    stop_emu_thread();

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
    emulation_running_.store(false);
    emulation_paused_.store(false);
    total_frames_.store(0);
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
    emulation_running_.store(true);
    emulation_paused_.store(false);

    // Start the emulation thread for the new system
    start_emu_thread();

    // Update window title with system name and loaded program
    update_window_title();

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
// Emulation Thread
// ============================================================================

void SystemGUI::emu_thread_func() {
    uint64_t pace_counter = 0;
    double accumulator = 0.0;
    bool was_running = false;
    std::vector<SDL_Event> events;  // Reuse allocation across loop iterations

    while (emu_thread_running_.load()) {
        bool running = emulation_running_.load() && !emulation_paused_.load();

        if (!running || !system_) {
            // Reset pacing so we start fresh when unpaused
            pace_counter = 0;
            accumulator = 0.0;
            was_running = false;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }

        // Detect transition to running — reset pacing
        if (!was_running) {
            pace_counter = SDL_GetPerformanceCounter();
            accumulator = 0.0;
            was_running = true;
        }

        // ----- Drain input queue (brief input_mutex_ only) -----
        events.clear();
        {
            std::lock_guard<std::mutex> lock(input_mutex_);
            events.swap(input_queue_);
        }

        // ----- Time accumulator (no lock needed) -----
        uint64_t now = SDL_GetPerformanceCounter();
        double freq = static_cast<double>(SDL_GetPerformanceFrequency());
        double elapsed = static_cast<double>(now - pace_counter) / freq;
        pace_counter = now;
        accumulator += elapsed;

        // ----- Frame pacing (no lock needed) -----
        // Use the precise frame time derived from cycles_per_frame / cpu_freq
        // to avoid integer-FPS rounding error.  PAL ≈ 19.95 ms (not 20.00),
        // NTSC ≈ 16.72 ms (not 16.67).  Configuration changes only happen
        // with the emu thread stopped.
        const double target_fps = system_->get_target_fps();
        const double target_frame_time = system_->get_target_frame_time();

        // Cap accumulator (death-spiral prevention: max 3 frames catch-up)
        if (accumulator > target_frame_time * 3.0)
            accumulator = target_frame_time * 3.0;

        int frames_ran = 0;
        uint32_t samples_needed = 0;
        {
            // ----- emu_mutex_ scope: input + frame simulation only -----
            // Audio sample generation moved OUTSIDE the lock: the SID's
            // sample_buffer is an SPSC ring buffer with atomic indices,
            // only read by this thread.  This reduces lock hold time and
            // gives the GUI thread's try_lock more windows to succeed.
            std::lock_guard<std::mutex> lock(emu_mutex_);

            // Process queued input events
            for (auto& evt : events) {
                // Focus loss → release all keys
                if (evt.type == SDL_WINDOWEVENT &&
                    evt.window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
                    system_->release_all_keys();
                    continue;
                }
                // Keyboard events → system keyboard handler + device routing
                if (evt.type == SDL_KEYDOWN) {
                    system_->handle_keyboard_event_ex(
                        evt.key.keysym.sym, evt.key.keysym.scancode,
                        evt.key.keysym.mod, true, evt.key.repeat != 0);
                } else if (evt.type == SDL_KEYUP) {
                    system_->handle_keyboard_event_ex(
                        evt.key.keysym.sym, evt.key.keysym.scancode,
                        evt.key.keysym.mod, false, false);
                } else if (evt.type == SDL_TEXTINPUT) {
                    system_->handle_text_input(evt.text.text);
                }
                // All events also go through the peripheral device router
                system_->process_sdl_event_for_devices(evt);
            }

            // Run emulation frames
            while (accumulator >= target_frame_time) {
                uint64_t t0 = SDL_GetPerformanceCounter();
                system_->run_frame();
                uint64_t t1 = SDL_GetPerformanceCounter();
                total_frames_.fetch_add(1, std::memory_order_relaxed);
                accumulator -= target_frame_time;
                frames_ran++;

                // Exponential moving average of frame emulation time (µs).
                // Alpha ≈ 0.05 gives a ~20-frame smoothing window.
                double frame_us = static_cast<double>(t1 - t0) / freq * 1e6;
                uint32_t prev = emu_frame_time_us_.load(std::memory_order_relaxed);
                uint32_t smoothed = prev == 0
                    ? static_cast<uint32_t>(frame_us)
                    : static_cast<uint32_t>(prev * 0.95 + frame_us * 0.05);
                emu_frame_time_us_.store(smoothed, std::memory_order_relaxed);
            }

            // Compute how many audio samples to generate (under lock for
            // consistent frames_ran, but the actual generation is outside).
            if (frames_ran > 0 && audio_ring_ && audio_sample_rate_ > 0) {
                uint32_t samples_per_frame = static_cast<uint32_t>(
                    audio_sample_rate_ / target_fps + 0.5);
                samples_needed = samples_per_frame * static_cast<uint32_t>(frames_ran);
            }
        }

        // ----- Audio sample generation (outside emu_mutex_) -----
        // SID sample_buffer is SPSC with atomic read/write positions.
        // Only the emu thread reads (here) and writes (during run_frame).
        // No concurrent access possible: run_frame completed above, and
        // the GUI thread never touches the sample buffer.
        uint32_t audio_got = 0;
        if (samples_needed > 0) {
            if (samples_needed > static_cast<uint32_t>(emu_audio_tmp_.size()))
                emu_audio_tmp_.resize(samples_needed);
            audio_got = system_->get_audio_samples(emu_audio_tmp_.data(), samples_needed);
        }

        // ----- Snapshot framebuffer (separate fb_mutex_) -----
        if (frames_ran > 0) {
            if (framebuffer_ && fb_snapshot_) {
                std::lock_guard<std::mutex> lock(fb_mutex_);
                memcpy(fb_snapshot_, framebuffer_,
                       static_cast<size_t>(fb_width_) * fb_height_ * sizeof(uint32_t));
                fb_new_frame_.store(true, std::memory_order_release);
            }
        }

        // Write audio to ring buffer (lock-free SPSC, outside any mutex)
        if (audio_got > 0)
            audio_ring_->write(emu_audio_tmp_.data(), audio_got);

        // ----- Yield CPU if we're ahead of schedule -----
        if (accumulator < target_frame_time * 0.5) {
            std::this_thread::sleep_for(std::chrono::microseconds(500));
        }
    }
}

// ============================================================================
// Drive File Dialog Polling
// ============================================================================

void SystemGUI::poll_drive_file_dialog_requests() {
#ifdef HAS_IMGUIFILEDIALOG
    if (!system_ || pending_drive_insert_) return;  // Already have a pending request

    // Scan IEC bus devices for any 1541 drive that wants a file dialog.
    // Use try_lock because the emulation thread may be running.
    std::unique_lock<std::mutex> lock(emu_mutex_, std::try_to_lock);
    if (!lock.owns_lock()) return;

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

