#pragma once

#include <SDL.h>
#include <SDL_opengl.h>
#include <cstdint>

#include <memory>
#include <thread>
#include <mutex>
#include <atomic>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

#include "utils/ring_buffer.hpp"
#include "utils/performance_metrics.hpp"
#include "core/signal/sync_types.hpp"  // FrameData, SyncEvent

// Forward declarations
struct ImGuiIO;

// ============================================================================
// Display Configuration Enums
// ============================================================================

// Aspect ratio configuration modes
enum aspect_ratio_mode_t {
    ASPECT_RATIO_ORIGINAL = 0,    // Use original guest aspect ratio
    ASPECT_RATIO_4_3,             // Force 4:3 aspect ratio
    ASPECT_RATIO_16_10,           // Force 16:10 aspect ratio
    ASPECT_RATIO_16_9,            // Force 16:9 aspect ratio
    ASPECT_RATIO_CUSTOM,          // Use custom aspect ratio
    ASPECT_RATIO_PIXEL_PERFECT,   // 1:1 pixel aspect ratio
    ASPECT_RATIO_COUNT
};

// Scaling mode configuration
enum scaling_mode_t {
    SCALING_MODE_FIT = 0,         // Fit display within window (may add black bars)
    SCALING_MODE_FILL,            // Fill entire window (may crop)
    SCALING_MODE_STRETCH,         // Stretch to fill window (may distort)
    SCALING_MODE_INTEGER,         // Use integer scaling only
    SCALING_MODE_COUNT
};

/**
 * EmulatorHost - Host substrate for the emulator
 * 
 * Owns everything on the host side: SDL window, GL context, ImGui loop,
 * primary emu thread, framebuffer pipeline, audio device, input queue.
 * Has no knowledge of what system is running. Derived classes (SessionGUI)
 * wire a Session/System into this substrate.
 *
 * Threading model:
 *   - GUI thread   : SDL events, ImGui rendering, texture upload
 *   - Emu thread   : run_frame(), audio sample generation
 *   - SDL audio    : reads from lock-free AudioRingBuffer
 *
 * Synchronisation:
 *   emu_mutex_   \u2014 held by the emu thread during run_frame(); GUI thread
 *                  uses try_lock for debug/menu reads (skips if busy).
 *   fb_mutex_    \u2014 protects the framebuffer snapshot (very brief lock).
 *   input_mutex_ \u2014 protects the queued SDL input events.
 */
class EmulatorHost {
protected:
    // SDL/OpenGL state
    SDL_Window* window_;
    SDL_GLContext gl_context_;
    bool should_quit_;
    const char* glsl_version_;
    
    // Display state
    GLuint screen_texture_id_;
    float screen_scale_;
    bool screen_filter_;
    bool screen_scanlines_;
    
    // Window visibility flags
    bool show_screen_;
    bool show_memory_viewer_;
    bool show_settings_;
    bool show_about_;
    
    // Window dimensions
    int window_width_;
    int window_height_;

    // Aspect ratio and scaling configuration
    aspect_ratio_mode_t aspect_ratio_mode_;
    scaling_mode_t scaling_mode_;
    float custom_aspect_ratio_;       // For ASPECT_RATIO_CUSTOM mode
    bool maintain_pixel_aspect_;      // Maintain square pixels
    bool show_overscan_;              // Include overscan/border area
    bool center_display_;             // Center display in available space
    bool show_invisible_area_;        // Show non-visible area around display output
    float host_dpi_scale_;            // Host DPI scaling factor

    // ========================================================================
    // Mouse cursor auto-hide
    // ========================================================================
    int    cursor_last_x_      = 0;     ///< Last observed mouse X position
    int    cursor_last_y_      = 0;     ///< Last observed mouse Y position
    Uint32 cursor_last_move_   = 0;     ///< SDL_GetTicks() of last mouse movement
    bool   cursor_hidden_      = false; ///< True when the host cursor is hidden

    // ========================================================================
    // Windows accessibility shortcut suppression
    // ========================================================================
#ifdef _WIN32
    STICKYKEYS  saved_sticky_keys_  = {sizeof(STICKYKEYS),  0};
    TOGGLEKEYS  saved_toggle_keys_  = {sizeof(TOGGLEKEYS),  0};
    FILTERKEYS  saved_filter_keys_  = {sizeof(FILTERKEYS),  0};
    bool        saved_access_keys_  = false; ///< True if original settings were captured
#endif

    /// Idle time (ms) before hiding the cursor over the main window.
    static constexpr Uint32 CURSOR_HIDE_DELAY_MS = 2000;

    // ========================================================================
    // Emulation state (generic)
    // ========================================================================
    std::atomic<bool> emulation_running_;
    std::atomic<bool> emulation_paused_;
    float speed_multiplier_;

    // ========================================================================
    // Framebuffer & double-buffered display
    // ========================================================================
    uint32_t* framebuffer_;
    int fb_width_;
    int fb_height_;

    /// Double-buffered GL textures — upload to one while the GPU
    /// may still be rendering the previous frame from the other.
    GLuint screen_textures_[2];
    int    texture_write_idx_;

    // ========================================================================
    // GPU indexed palette rendering
    // ========================================================================
    /// When the active system supports GPU indexed rendering, palette lookups
    /// are deferred to a fragment shader.  The emu thread writes 8-bit palette
    /// indices to index_framebuffer_; the GUI thread uploads them as an R8
    /// texture and lets the shader sample a 256×1 palette texture.

    bool      use_gpu_indexed_       = false;  ///< Active system uses indexed mode
    uint8_t*  index_framebuffer_     = nullptr; ///< Index buffer (emu thread writes)
    uint8_t*  index_snapshot_        = nullptr; ///< Snapshot (GUI thread reads)
    GLuint    index_textures_[2]     = {};      ///< Double-buffered R8 textures
    GLuint    palette_texture_       = 0;       ///< 256×1 RGBA palette texture
    GLuint    indexed_shader_        = 0;       ///< Shader program for indexed rendering
    GLint     indexed_loc_proj_      = -1;      ///< ProjMtx uniform location
    int       gpu_palette_size_      = 0;       ///< Number of palette entries

    // ========================================================================
    // GPU stream reconstruction
    // ========================================================================
    /// When the system drives per-dot-clock video samples to a VideoPort,
    /// the raw stream can be uploaded directly to the GPU.  A fragment
    /// shader performs scanline mapping and palette lookup in a single pass,
    /// bypassing the CPU-side reconstruct_to_framebuffer() bridge.

    bool      use_stream_shader_     = false;   ///< Active system has stream data
    GLuint    stream_texture_        = 0;       ///< R8 packed 1D→2D stream texture
    GLuint    stream_shader_         = 0;       ///< Stream reconstruction shader program
    GLint     stream_loc_proj_       = -1;      ///< ProjMtx uniform location
    GLint     stream_loc_scanline_map_ = -1;    ///< ScanlineMap uniform location
    GLint     stream_loc_tex_width_  = -1;      ///< StreamTexWidth uniform location
    GLint     stream_loc_display_h_  = -1;      ///< DisplayHeight uniform location
    GLint     stream_loc_display_w_  = -1;      ///< DisplayWidth uniform location

    /// Stream snapshot — copied from last_frame_data_ by emu thread under fb_mutex_.
    /// GUI thread reads these to upload stream texture and compute scanline map.
    uint8_t*  stream_snapshot_       = nullptr;  ///< Color indices extracted from stream
    SyncEvent* sync_snapshot_        = nullptr;  ///< Sync events snapshot
    uint32_t  stream_snapshot_len_   = 0;        ///< Number of samples in snapshot
    uint32_t  sync_snapshot_count_   = 0;        ///< Number of sync events in snapshot
    int       stream_back_porch_     = 0;        ///< Back porch pixels for scanline map
    int       stream_display_width_  = 0;        ///< Visible pixels per scanline
    int       stream_display_height_ = 0;        ///< Visible scanlines (from scanline map)

    // ========================================================================
    // GPU RGB stream reconstruction
    // ========================================================================
    /// For RGB signal types: separate shader + RGBA8 stream texture.
    /// RGB samples are {r, g, b, flags} — 4 bytes each; the shader reads
    /// raw RGB values without a palette lookup.

    bool      use_rgb_stream_shader_   = false;
    GLuint    rgb_stream_texture_      = 0;       ///< RGBA8 packed 1D→2D stream texture
    GLuint    rgb_stream_shader_       = 0;       ///< RGB stream reconstruction shader
    GLint     rgb_stream_loc_proj_     = -1;
    GLint     rgb_stream_loc_scanline_map_ = -1;
    GLint     rgb_stream_loc_tex_width_    = -1;
    GLint     rgb_stream_loc_display_h_    = -1;
    GLint     rgb_stream_loc_display_w_    = -1;
    uint8_t*  rgb_stream_snapshot_     = nullptr;  ///< RGBA bytes extracted from stream

    // ========================================================================
    // GPU vector display rendering
    // ========================================================================
    /// For systems with SignalType::Vector (DVG, Vectrex, etc.), the raw
    /// VectorVideoSample stream is extracted into line segments on the CPU
    /// and rendered as beam quads via a dedicated vertex+fragment shader.
    /// No texture is involved — vertices carry all data.

    bool      use_vector_shader_        = false;
    GLuint    vector_shader_            = 0;
    GLuint    vector_vao_               = 0;
    GLuint    vector_vbo_               = 0;
    GLint     vector_loc_proj_          = -1;
    GLint     vector_loc_phosphor_      = -1;

    /// Video signal type of the active system (cached from System::get_video_signal_type()).
    SignalType active_signal_type_       = SignalType::Composite;

    /// Vector stream snapshot — raw VectorVideoSample bytes copied from
    /// last_frame_data_ by emu thread.  8 bytes per sample.
    uint8_t*  vector_stream_snapshot_   = nullptr;
    uint32_t  vector_stream_len_        = 0;  ///< Number of samples in snapshot

    /// CPU-expanded beam quad vertices for the current frame.
    /// Built during render_screen() from vector_stream_snapshot_,
    /// consumed by the vector draw callback.  Stored as raw bytes
    /// (BeamVertex = 16 bytes each).
    std::vector<uint8_t> vector_beam_buf_;
    int                  vector_beam_count_  = 0;

    // ========================================================================
    // Statistics / Frame pacing
    // ========================================================================
    std::atomic<uint64_t> total_frames_;
    uint32_t actual_fps_;
    uint32_t last_fps_time_;
    uint64_t last_fps_frame_count_;

    /// Per-frame emulation time (microseconds, exponential moving average).
    std::atomic<uint32_t> emu_frame_time_us_{0};

    /// Aggregated performance metrics (time-series + counters).
    /// Pushed by emu thread, read by GUI thread for the performance window.
    PerformanceMetrics perf_metrics_;
    bool show_performance_     = false;
    bool show_perf_frame_time_ = true;
    bool show_perf_vblank_     = true;
    bool show_perf_headroom_   = true;
    bool show_perf_vps_        = true;
    bool show_perf_fps_        = true;
    bool show_perf_speed_      = true;
    bool show_perf_colors_     = false;
    bool show_perf_audio_      = true;

    /// Frame pacing (private to emu thread)
    uint64_t frame_pace_counter_;
    double frame_time_accumulator_;

    // ========================================================================
    // Emulation threading
    // ========================================================================
    std::thread emu_thread_;
    std::atomic<bool> emu_thread_running_{false};

    /// Protects system state during run_frame() and other mutating operations.
    std::mutex emu_mutex_;

    /// Framebuffer snapshot (written by emu thread, read by GUI for texture upload).
    uint32_t* fb_snapshot_;
    std::mutex fb_mutex_;
    std::atomic<bool> fb_new_frame_{false};

    /// Input event queue (pushed by GUI thread, consumed by emu thread).
    std::mutex input_mutex_;
    std::vector<SDL_Event> input_queue_;

    // ========================================================================
    // Audio
    // ========================================================================
    SDL_AudioDeviceID audio_device_;
    int audio_sample_rate_;

    /// Lock-free audio ring buffer (emu thread produces, SDL callback consumes).
    std::unique_ptr<AudioRingBuffer> audio_ring_;

    /// Temporary buffer used by the emu thread to call get_audio_samples().
    std::vector<float> emu_audio_tmp_;

public:
    EmulatorHost();
    virtual ~EmulatorHost();
    
    // Generic initialization/cleanup (non-virtual, final)
    bool init(const char* window_title, int width, int height);
    void cleanup();
    
    // Main loop - orchestrates the frame rendering
    virtual void run();
    
    // Query methods
    bool should_quit() const { return should_quit_; }
    SDL_Window* get_window() const { return window_; }
    
protected:
    // ========================================================================
    // Virtual hooks - Override these in derived classes
    // ========================================================================
    
    /**
     * Handle SDL events (keyboard, mouse, window events)
     * Called once per frame before rendering
     */
    virtual void handle_events() = 0;
    
    /**
     * Update emulation state for one frame
     * Called after event handling, before rendering
     */
    virtual void update_frame() = 0;
    
    /**
     * Render the complete frame (ImGui + OpenGL)
     * This should call begin_frame(), render UI, then end_frame()
     */
    virtual void render_frame() = 0;
    
    /**
     * Render the main menu bar
     * Override to add system-specific menus
     */
    virtual void render_menu_bar() = 0;
    
    /**
     * Render the emulated system's screen/display
     * Override to provide system-specific display rendering
     */
    virtual void render_screen() = 0;
    
    /**
     * Render optional windows based on visibility flags
     * Override to provide system-specific implementations
     */
    virtual void render_memory_viewer() {}
    virtual void render_debugger() {}
    virtual void render_settings() {}
    virtual void render_about() {}

    /**
     * Render the performance metrics overlay window.
     * Always available (not virtual — generic across all systems).
     */
    void render_performance_window();
    
    /**
     * Per-frame delay for timing control
     * Override to customize frame rate limiting
     */
    virtual uint32_t get_frame_delay_ms() const { return 16; } // ~60 FPS default

    /**
     * Returns true when a virtual mouse device is attached to the emulated
     * system (e.g. Commodore 1351 or NEOS mouse).  Used by the cursor
     * auto-hide logic to hide the host cursor immediately in fullscreen,
     * because the guest is expected to render its own cursor.
     */
    virtual bool has_virtual_mouse_attached() const { return false; }
    
    // ========================================================================
    // Generic helper functions for derived classes
    // ========================================================================
    
    /**
     * Update host mouse cursor visibility.
     *
     * Call once per frame (after event processing).  Hides the cursor when
     * it has been idle over the main window for CURSOR_HIDE_DELAY_MS, or
     * immediately when the window is fullscreen and a virtual mouse device
     * is attached to the emulated system.
     */
    void update_mouse_cursor_visibility();

    /**
     * Begin ImGui frame - call at start of render_frame()
     */
    void begin_frame();
    
    /**
     * End ImGui frame and swap buffers - call at end of render_frame()
     */
    void end_frame();
    
    /**
     * Generic file menu rendering
     * Can be called from derived class's render_menu_bar()
     */
    void render_file_menu_generic();
    
    /**
     * Generic view menu rendering
     * Can be called from derived class's render_menu_bar()
     */
    void render_view_menu_generic();
    
    /**
     * Generic help menu rendering
     * Can be called from derived class's render_menu_bar()
     */
    void render_help_menu_generic();
    
    /**
     * Generic about dialog
     * Can be called from derived class's render_about()
     */
    void render_about_dialog_generic();
    
    /**
     * Create OpenGL texture for screen display
     * Returns texture ID, or 0 on failure
     */
    GLuint create_screen_texture(int width, int height);
    
    /**
     * Update OpenGL texture with new pixel data
     */
    void update_screen_texture(GLuint texture_id, int width, int height, 
                               const uint32_t* pixels);

    /**
     * Create an R8 OpenGL texture for GPU indexed rendering.
     */
    GLuint create_index_texture(int width, int height);

    /**
     * Update an R8 texture with new index data.
     */
    void update_index_texture(GLuint texture_id, int width, int height,
                              const uint8_t* indices);

    /**
     * Create a 256×1 RGBA texture for the palette.
     */
    GLuint create_palette_texture();

    /**
     * Update the palette texture with new RGBA palette data.
     */
    void update_palette_texture(const uint32_t* palette, int count);

    /**
     * Compile the indexed palette shader program.
     * Returns true on success.
     */
    bool compile_indexed_shader();

    /**
     * Clean up indexed rendering resources (shader, textures, buffers).
     */
    void cleanup_indexed_resources();
    
    /**
     * Calculate centered display dimensions with integer scaling
     * For pixel-perfect rendering of retro displays
     */
    void calculate_integer_scaled_dimensions(
        int window_width, int window_height,
        int content_width, int content_height,
        int* out_display_width, int* out_display_height,
        int* out_pos_x, int* out_pos_y) const;
    
    /**
     * Calculate display dimensions with aspect ratio and scaling support
     * This replaces the simpler integer scaling with full letterboxing/zoom control
     */
    void calculate_display_dimensions(
        float viewport_width, float viewport_height,
        float guest_width, float guest_height,
        bool is_pal, bool use_pixel_aspect,
        float* out_display_width, float* out_display_height,
        float* out_pos_x, float* out_pos_y) const;
    
    /**
     * Get target aspect ratio based on current configuration
     */
    float get_target_aspect_ratio(float guest_width, float guest_height,
                                   bool is_pal, bool use_pixel_aspect) const;
    
    /**
     * Render Screen menu with display controls
     * Can be called from derived class's render_menu_bar()
     */
    void render_screen_menu_generic();

    // ========================================================================
    // Emulation lifecycle (generic)
    // ========================================================================

    /**
     * Start/resume emulation
     */
    void start_emulation();

    /**
     * Pause emulation
     */
    void pause_emulation();

    /**
     * Update FPS counter from atomic frame count.
     */
    void update_fps();

    /**
     * Reset frame pacing accumulators.
     */
    void reset_frame_pacing();

    /**
     * Free framebuffer arrays and associated GL textures.
     */
    void free_framebuffer();

    // ========================================================================
    // Threading (generic)
    // ========================================================================

    /**
     * Start the emulation thread.
     * Calls the virtual emu_thread_func() on the new thread.
     */
    void start_emu_thread();

    /**
     * Stop the emulation thread and join.
     */
    void stop_emu_thread();

    /**
     * Emulation thread entry point — override in derived classes.
     * This runs on a separate thread and should loop while
     * emu_thread_running_ is true.
     */
    virtual void emu_thread_func() = 0;

    // ========================================================================
    // Audio (generic)
    // ========================================================================

    /**
     * Close the SDL audio device.
     */
    void close_audio_device();

    /**
     * SDL audio callback — reads from the lock-free AudioRingBuffer.
     */
    static void sdl_audio_callback(void* userdata, uint8_t* stream, int len);

private:
    // Prevent copying
    EmulatorHost(const EmulatorHost&) = delete;
    EmulatorHost& operator=(const EmulatorHost&) = delete;
};

