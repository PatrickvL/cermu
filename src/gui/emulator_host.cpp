#include "gui/emulator_host.hpp"
#include "gui/display_panel.hpp"
#include "gui/gl_api.hpp"
#include "gui/decoder/signal_decoder.hpp"
#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_sdl2.h>
#include <cstdio>
#include <algorithm>
#include <chrono>
#include <cstring>

// ============================================================================
// Constructor / Destructor
// ============================================================================

EmulatorHost::EmulatorHost()
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
    // Aspect ratio configuration defaults
    , aspect_ratio_mode_(ASPECT_RATIO_ORIGINAL)
    , scaling_mode_(SCALING_MODE_FIT)
    , custom_aspect_ratio_(4.0f / 3.0f)  // 4:3 default
    , maintain_pixel_aspect_(true)
    , show_overscan_(true)
    , center_display_(true)
    , show_invisible_area_(false)
    , host_dpi_scale_(1.0f)  // Will be detected at runtime
    , display_zoom_(1.0f)
    , display_pan_x_(0.0f)
    , display_pan_y_(0.0f)
    // Emulation state
    , emulation_running_(false)
    , emulation_paused_(false)
    , speed_multiplier_(1.0f)
    // Framebuffer
    , framebuffer_(nullptr)
    , fb_width_(0)
    , fb_height_(0)
    // Statistics
    , total_frames_(0)
    , actual_fps_(0)
    , last_fps_time_(0)
    , last_fps_frame_count_(0)
    , frame_pace_counter_(0)
    , frame_time_accumulator_(0.0)
    // Threading
    // Audio
    , audio_device_(0)
    , audio_sample_rate_(0)
    , audio_ring_(std::make_unique<AudioRingBuffer>(8192))
{
}

EmulatorHost::~EmulatorHost() {
    cleanup();
}

// ============================================================================
// Initialization / Cleanup
// ============================================================================

bool EmulatorHost::init(const char* window_title, int width, int height) {
    window_width_ = width;
    window_height_ = height;
    
    // Initialize SDL (including game controller/joystick for peripheral input)
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER | SDL_INIT_EVENTS |
                 SDL_INIT_GAMECONTROLLER | SDL_INIT_JOYSTICK) < 0) {
        printf("Failed to initialize SDL: %s\n", SDL_GetError());
        return false;
    }

    // Disable Windows accessibility shortcut hotkeys (Sticky Keys, Toggle Keys,
    // Filter Keys) so that repeatedly pressing Shift / Ctrl for emulated buttons
    // does not trigger the OS popup.  Original settings are restored in cleanup().
#ifdef _WIN32
    {
        SystemParametersInfo(SPI_GETSTICKYKEYS, sizeof(STICKYKEYS), &saved_sticky_keys_, 0);
        SystemParametersInfo(SPI_GETTOGGLEKEYS, sizeof(TOGGLEKEYS), &saved_toggle_keys_, 0);
        SystemParametersInfo(SPI_GETFILTERKEYS, sizeof(FILTERKEYS), &saved_filter_keys_, 0);
        saved_access_keys_ = true;

        STICKYKEYS sk = saved_sticky_keys_;
        sk.dwFlags &= ~SKF_HOTKEYACTIVE;
        sk.dwFlags &= ~SKF_CONFIRMHOTKEY;
        SystemParametersInfo(SPI_SETSTICKYKEYS, sizeof(STICKYKEYS), &sk, 0);

        TOGGLEKEYS tk = saved_toggle_keys_;
        tk.dwFlags &= ~TKF_HOTKEYACTIVE;
        tk.dwFlags &= ~TKF_CONFIRMHOTKEY;
        SystemParametersInfo(SPI_SETTOGGLEKEYS, sizeof(TOGGLEKEYS), &tk, 0);

        FILTERKEYS fk = saved_filter_keys_;
        fk.dwFlags &= ~FKF_HOTKEYACTIVE;
        fk.dwFlags &= ~FKF_CONFIRMHOTKEY;
        SystemParametersInfo(SPI_SETFILTERKEYS, sizeof(FILTERKEYS), &fk, 0);
    }
#endif
    
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
    SDL_GL_SetSwapInterval(1); // VSync — paces the GUI render loop (~60Hz)

    // Load GL 2.0+ function pointers (needed for shader-based indexed rendering)
    gl_api::load_gl();
    
    // Show the window
    SDL_ShowWindow(window_);
    
    // Setup Dear ImGui context
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    // NOTE: Do NOT enable ImGuiConfigFlags_NavEnableKeyboard here.
    // It causes io.WantCaptureKeyboard to be true whenever any ImGui window
    // is present (including the always-visible menu bar), which blocks ALL
    // keyboard events from reaching the emulated systems.
    // SessionGUI::handle_events() toggles it dynamically when a menu,
    // dialog, or settings window is active.
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

void EmulatorHost::cleanup() {
    // Cleanup ImGui
    if (ImGui::GetCurrentContext()) {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplSDL2_Shutdown();
        ImGui::DestroyContext();
    }
    
    // Restore Windows accessibility shortcut hotkeys to their original state
#ifdef _WIN32
    if (saved_access_keys_) {
        SystemParametersInfo(SPI_SETSTICKYKEYS, sizeof(STICKYKEYS), &saved_sticky_keys_, 0);
        SystemParametersInfo(SPI_SETTOGGLEKEYS, sizeof(TOGGLEKEYS), &saved_toggle_keys_, 0);
        SystemParametersInfo(SPI_SETFILTERKEYS, sizeof(FILTERKEYS), &saved_filter_keys_, 0);
        saved_access_keys_ = false;
    }
#endif

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

void EmulatorHost::run() {
    printf("Starting main loop\n");
    
    while (!should_quit_) {
        // Handle events (system-specific)
        handle_events();

        // Auto-hide host mouse cursor after idle / in fullscreen with guest mouse
        update_mouse_cursor_visibility();
        
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
    
    // Restore cursor before exiting (in case it was hidden)
    if (cursor_hidden_) {
        SDL_ShowCursor(SDL_ENABLE);
        cursor_hidden_ = false;
    }

    printf("Main loop ended\n");
}

// ============================================================================
// Mouse Cursor Auto-Hide
// ============================================================================

void EmulatorHost::update_mouse_cursor_visibility() {
    // When a UI overlay (launcher, dialog) needs the cursor, keep it visible
    if (force_cursor_visible_) {
        if (cursor_hidden_) {
            SDL_ShowCursor(SDL_ENABLE);
            cursor_hidden_ = false;
        }
        // Let ImGui control cursor shape normally
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags &= ~ImGuiConfigFlags_NoMouseCursorChange;
        return;
    }

    int mx, my;
    SDL_GetMouseState(&mx, &my);

    Uint32 now = SDL_GetTicks();

    Uint32 wflags = SDL_GetWindowFlags(window_);
    bool is_fullscreen = (wflags & SDL_WINDOW_FULLSCREEN_DESKTOP) != 0;

    ImGuiIO& io = ImGui::GetIO();

    // In fullscreen, check whether the mouse is hovering over any ImGui
    // window (menu bar, performance overlay, detached chip panel, HUD, …).
    // When it is, keep the cursor visible so the user can interact.
    // When it isn't (mouse is over the emulated display), hide it.
    bool over_ui_window = is_fullscreen && io.WantCaptureMouse;

    // Tell ImGui not to touch the cursor while we're hiding it in
    // fullscreen, otherwise its SDL2 backend calls SDL_ShowCursor(TRUE)
    // every frame and overrides our hide.  But when the mouse is over a
    // UI window, let ImGui manage cursor shape normally (resize arrows,
    // text cursors, etc.).
    if (is_fullscreen && !over_ui_window)
        io.ConfigFlags |=  ImGuiConfigFlags_NoMouseCursorChange;
    else
        io.ConfigFlags &= ~ImGuiConfigFlags_NoMouseCursorChange;

    // Detect movement — reset idle timer and show cursor.
    if (mx != cursor_last_x_ || my != cursor_last_y_) {
        cursor_last_x_    = mx;
        cursor_last_y_    = my;
        cursor_last_move_ = now;

        // Re-show cursor on movement (except fullscreen over emulated screen)
        if (cursor_hidden_ && (!is_fullscreen || over_ui_window)) {
            SDL_ShowCursor(SDL_ENABLE);
            cursor_hidden_ = false;
        }
        return;
    }

    // Mouse is over a UI window in fullscreen — always keep cursor visible
    if (over_ui_window) {
        if (cursor_hidden_) {
            SDL_ShowCursor(SDL_ENABLE);
            cursor_hidden_ = false;
        }
        return;
    }

    // Already hidden — nothing to do.
    if (cursor_hidden_) return;

    // Don't hide if the pointer left the window (the WM may still need it).
    if (!(wflags & SDL_WINDOW_MOUSE_FOCUS)) return;

    // In fullscreen over the emulated display: hide immediately.
    if (is_fullscreen) {
        SDL_ShowCursor(SDL_DISABLE);
        cursor_hidden_ = true;
        return;
    }

    // Windowed mode: hide after the idle timeout.
    if (now - cursor_last_move_ >= CURSOR_HIDE_DELAY_MS) {
        SDL_ShowCursor(SDL_DISABLE);
        cursor_hidden_ = true;
    }
}

// ============================================================================
// Generic Helper Functions
// ============================================================================

void EmulatorHost::begin_frame() {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();
}

void EmulatorHost::end_frame() {
    ImGui::Render();
    ImGuiIO& io = ImGui::GetIO();
    // Guard against zero display size during monitor transitions / minimization
    if (io.DisplaySize.x <= 0.0f || io.DisplaySize.y <= 0.0f) {
        SDL_GL_SwapWindow(window_);
        return;
    }
    glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    SDL_GL_SwapWindow(window_);
}

void EmulatorHost::render_file_menu_generic() {
    if (ImGui::MenuItem("Exit")) {
        should_quit_ = true;
    }
}

void EmulatorHost::render_view_menu_generic() {
    ImGui::MenuItem("Screen Display", nullptr, &show_screen_);
    ImGui::MenuItem("Memory Viewer", nullptr, &show_memory_viewer_);
}

void EmulatorHost::render_help_menu_generic() {
    ImGui::MenuItem("About", nullptr, &show_about_);
}

void EmulatorHost::render_about_dialog_generic() {
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
// GPU Resource Cleanup
// ============================================================================

void EmulatorHost::cleanup_indexed_resources() {
    gpu_palette_size_ = 0;

    // Signal decoder owns all GPU resources (shader, textures, palette,
    // FBO, snapshot buffers).  Destroying it releases everything.
    if (signal_decoder_) {
        signal_decoder_->destroy();
        signal_decoder_.reset();
    }
}

// ============================================================================
// Display Scaling Helper
// ============================================================================

void EmulatorHost::calculate_integer_scaled_dimensions(
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

// ============================================================================
// Advanced Aspect Ratio and Scaling System
// ============================================================================

// Get target aspect ratio based on configuration
float EmulatorHost::get_target_aspect_ratio(float guest_width, float guest_height,
                                                   bool is_pal, bool use_pixel_aspect) const {
    switch (aspect_ratio_mode_) {
    case ASPECT_RATIO_4_3:
        return 4.0f / 3.0f;
    
    case ASPECT_RATIO_16_10:
        return 16.0f / 10.0f;
    
    case ASPECT_RATIO_16_9:
        return 16.0f / 9.0f;
    
    case ASPECT_RATIO_CUSTOM:
        return custom_aspect_ratio_;
    
    case ASPECT_RATIO_PIXEL_PERFECT:
        return 1.0f;  // Square pixels
    
    case ASPECT_RATIO_ORIGINAL:
    default: {
        // Calculate original aspect ratio
        // Pixels are not always square - different aspect ratios for PAL vs NTSC
        float pixel_aspect = is_pal ? (312.0f / 50.0f) / (263.0f / 60.0f) : 1.0f;
        
        if (use_pixel_aspect && maintain_pixel_aspect_) {
            return (guest_width / guest_height) * pixel_aspect;
        } else {
            return guest_width / guest_height;
        }
    }
    }
}

// Calculate display dimensions with full aspect ratio and scaling support
void EmulatorHost::calculate_display_dimensions(
    float viewport_width, float viewport_height,
    float guest_width, float guest_height,
    bool is_pal, bool use_pixel_aspect,
    float* out_display_width, float* out_display_height,
    float* out_pos_x, float* out_pos_y) const {
    
    // Apply host DPI scaling
    float effective_viewport_width = viewport_width / host_dpi_scale_;
    float effective_viewport_height = viewport_height / host_dpi_scale_;
    
    // Get target aspect ratio
    float target_aspect = get_target_aspect_ratio(guest_width, guest_height, is_pal, use_pixel_aspect);
    float viewport_aspect = effective_viewport_width / effective_viewport_height;
    
    float display_width, display_height;
    
    switch (scaling_mode_) {
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
        
        if (integer_scale < 1.0f)
            integer_scale = 1.0f;
        
        display_width = guest_width * integer_scale;
        display_height = guest_height * integer_scale;
        break;
    }
    
    case SCALING_MODE_FIT:
    default: {
        // Fit within viewport maintaining aspect ratio (may add black bars/letterboxing)
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
    display_width *= screen_scale_;
    display_height *= screen_scale_;

    // Apply display zoom (zooms into the rendered image)
    display_width  *= display_zoom_;
    display_height *= display_zoom_;
    
    // Calculate position (center by default, then apply pan offset)
    float pos_x = 0.0f;
    float pos_y = 0.0f;
    
    if (center_display_) {
        pos_x = (effective_viewport_width - display_width) * 0.5f;
        pos_y = (effective_viewport_height - display_height) * 0.5f;
    }

    // Apply pan — offset is proportional to the overflow (zoomed-in area)
    // display_pan_x/y range is -1..+1; maps to moving the full overflow distance
    float overflow_x = display_width - effective_viewport_width;
    float overflow_y = display_height - effective_viewport_height;
    if (overflow_x > 0.0f) {
        pos_x += -display_pan_x_ * overflow_x * 0.5f;
    }
    if (overflow_y > 0.0f) {
        pos_y += -display_pan_y_ * overflow_y * 0.5f;
    }
    
    // Apply DPI scaling back to final values
    *out_display_width = display_width * host_dpi_scale_;
    *out_display_height = display_height * host_dpi_scale_;
    *out_pos_x = pos_x * host_dpi_scale_;
    *out_pos_y = pos_y * host_dpi_scale_;
}

// ============================================================================
// Screen Menu with Display Controls
// ============================================================================

void EmulatorHost::render_screen_menu_generic() {
    ImGui::Text("Display Controls");
    ImGui::Separator();
    
    ImGui::SliderFloat("Scale", &screen_scale_, 0.5f, 4.0f, "%.1fx");
    ImGui::Checkbox("Filter", &screen_filter_);
    ImGui::Checkbox("Scanlines", &screen_scanlines_);
    
    // Update texture filtering based on user preference
    GLuint filter_tex = signal_decoder_ ? signal_decoder_->output_texture()
                                        : screen_texture_id_;
    if (filter_tex != 0) {
        GLint filter = screen_filter_ ? GL_LINEAR : GL_NEAREST;
        glBindTexture(GL_TEXTURE_2D, filter_tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
    }
    
    ImGui::Separator();
    ImGui::Text("Aspect Ratio & Scaling");
    
    // Aspect ratio mode selection
    const char* aspect_ratio_items[] = {
        "Original", "4:3", "16:10", "16:9", "Custom", "Pixel Perfect"
    };
    int current_aspect = (int)aspect_ratio_mode_;
    if (ImGui::Combo("Aspect Ratio", &current_aspect, aspect_ratio_items,
                     ASPECT_RATIO_COUNT)) {
        aspect_ratio_mode_ = (aspect_ratio_mode_t)current_aspect;
    }
    
    // Custom aspect ratio input (only shown when Custom is selected)
    if (aspect_ratio_mode_ == ASPECT_RATIO_CUSTOM) {
        ImGui::SliderFloat("Custom Ratio", &custom_aspect_ratio_,
                          0.5f, 3.0f, "%.2f");
    }
    
    // Scaling mode selection
    const char* scaling_mode_items[] = {
        "Fit (Black Bars)", "Fill (Crop)", "Stretch", "Integer Scale"
    };
    int current_scaling = (int)scaling_mode_;
    if (ImGui::Combo("Scaling Mode", &current_scaling, scaling_mode_items,
                     SCALING_MODE_COUNT)) {
        scaling_mode_ = (scaling_mode_t)current_scaling;
    }
    
    // Additional options
    ImGui::Checkbox("Maintain Pixel Aspect", &maintain_pixel_aspect_);
    ImGui::Checkbox("Show Overscan/Border", &show_overscan_);
    ImGui::Checkbox("Center Display", &center_display_);
    ImGui::Checkbox("Show Invisible Area", &show_invisible_area_);

    ImGui::Separator();
    ImGui::Text("Zoom & Pan");
    ImGui::SliderFloat("Zoom", &display_zoom_, 0.25f, 4.0f, "%.2fx");
    ImGui::BeginDisabled(display_zoom_ <= 1.0f);
    ImGui::SliderFloat("Pan X", &display_pan_x_, -1.0f, 1.0f, "%.2f");
    ImGui::SliderFloat("Pan Y", &display_pan_y_, -1.0f, 1.0f, "%.2f");
    ImGui::EndDisabled();
    if (ImGui::Button("Reset Zoom/Pan")) {
        display_zoom_ = 1.0f;
        display_pan_x_ = 0.0f;
        display_pan_y_ = 0.0f;
    }
    
    // Host DPI information
    ImGui::Separator();
    ImGui::Text("Host DPI Scale: %.2f", host_dpi_scale_);
}

// ============================================================================
// Emulation Lifecycle (generic)
// ============================================================================

void EmulatorHost::start_emulation() {
    emulation_running_.store(true);
    emulation_paused_.store(false);
    // Frame pacing is reset inside the emu thread when it detects
    // the transition from paused/stopped to running.
    printf("Emulation started\n");
}

void EmulatorHost::pause_emulation() {
    emulation_paused_.store(true);
    printf("Emulation paused\n");
}

void EmulatorHost::update_fps() {
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

void EmulatorHost::reset_frame_pacing() {
    frame_pace_counter_ = 0;
    frame_time_accumulator_ = 0.0;
}

void EmulatorHost::free_framebuffer() {
    if (framebuffer_) {
        delete[] framebuffer_;
        framebuffer_ = nullptr;
    }
    
    screen_texture_id_ = 0;

    // Clean up GPU indexed palette resources
    cleanup_indexed_resources();

    // Destroy display panel (owns CRT post-processing resources if CRTPanel)
    display_panel_.reset();

    // Clear display device reference (system owns the device)
    display_device_ = nullptr;
    display_characteristics_ = DisplayCharacteristics{};
    display_has_speakers_.store(false, std::memory_order_relaxed);
}


// ============================================================================
// Threading (generic)
// ============================================================================

void EmulatorHost::start_emu_thread() {
    if (emu_thread_running_.load()) return;  // Already running
    emu_thread_running_.store(true);
    audio_ring_->reset();
    emu_thread_ = std::thread(&EmulatorHost::emu_thread_func, this);
    printf("Emulation thread started\n");
}

void EmulatorHost::stop_emu_thread() {
    if (!emu_thread_running_.load()) return;
    emu_thread_running_.store(false);
    if (emu_thread_.joinable()) {
        emu_thread_.join();
    }
    printf("Emulation thread stopped\n");
}

// ============================================================================
// Audio (generic)
// ============================================================================

void EmulatorHost::sdl_audio_callback(void* userdata, uint8_t* stream, int len) {
    EmulatorHost* gui = static_cast<EmulatorHost*>(userdata);
    int sample_count = len / static_cast<int>(sizeof(float));
    float* out = reinterpret_cast<float*>(stream);

    // Read from the lock-free ring buffer (fed by the emulation thread)
    uint32_t written = 0;
    if (gui->audio_ring_) {
        written = static_cast<uint32_t>(
            gui->audio_ring_->read(out, static_cast<size_t>(sample_count)));
    }
    // Track underruns (SDL wanted samples but ring was empty/insufficient)
    if (written < static_cast<uint32_t>(sample_count))
        gui->perf_metrics_.audio_underruns.fetch_add(1, std::memory_order_relaxed);

    // Apply speaker simulation if enabled and display has built-in speakers
    if (gui->use_speaker_sim_
        && gui->display_has_speakers_.load(std::memory_order_relaxed)
        && gui->speaker_sim_.is_initialized() && written > 0) {
        gui->speaker_sim_.process(out, written);
    }

    // Fill remainder with silence
    for (uint32_t i = written; i < static_cast<uint32_t>(sample_count); i++) {
        out[i] = 0.0f;
    }
}

void EmulatorHost::close_audio_device() {
    if (audio_device_ != 0) {
        SDL_CloseAudioDevice(audio_device_);
        audio_device_ = 0;
        audio_sample_rate_ = 0;
        speaker_sim_.reset();
        printf("Audio: device closed\n");
    }
}