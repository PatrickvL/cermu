#ifndef GENERIC_GUI_H
#define GENERIC_GUI_H

#include <SDL.h>
#include <SDL_opengl.h>
#include <stdint.h>
#include <stdbool.h>

// Forward declarations
struct ImGuiIO;

// ============================================================================
// Display Configuration Enums
// ============================================================================

// Aspect ratio configuration modes
typedef enum {
    ASPECT_RATIO_ORIGINAL = 0,    // Use original guest aspect ratio
    ASPECT_RATIO_4_3,             // Force 4:3 aspect ratio
    ASPECT_RATIO_16_10,           // Force 16:10 aspect ratio
    ASPECT_RATIO_16_9,            // Force 16:9 aspect ratio
    ASPECT_RATIO_CUSTOM,          // Use custom aspect ratio
    ASPECT_RATIO_PIXEL_PERFECT,   // 1:1 pixel aspect ratio
    ASPECT_RATIO_COUNT
} aspect_ratio_mode_t;

// Scaling mode configuration
typedef enum {
    SCALING_MODE_FIT = 0,         // Fit display within window (may add black bars)
    SCALING_MODE_FILL,            // Fill entire window (may crop)
    SCALING_MODE_STRETCH,         // Stretch to fill window (may distort)
    SCALING_MODE_INTEGER,         // Use integer scaling only
    SCALING_MODE_COUNT
} scaling_mode_t;

/**
 * GenericEmulatorGUI - Base class for all emulator GUIs
 * 
 * This class provides the common SDL/ImGui initialization, window management,
 * and basic rendering loop. Derived classes (C64GUI, SystemGUI, etc.)
 * override virtual methods to provide system-specific behavior.
 */
class GenericEmulatorGUI {
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
    
    // Aspect ratio and scaling configuration
    aspect_ratio_mode_t aspect_ratio_mode_;
    scaling_mode_t scaling_mode_;
    float custom_aspect_ratio_;       // For ASPECT_RATIO_CUSTOM mode
    bool maintain_pixel_aspect_;      // Maintain square pixels
    bool show_overscan_;              // Include overscan/border area
    bool center_display_;             // Center display in available space
    bool show_invisible_area_;        // Show non-visible area around display output
    float host_dpi_scale_;            // Host DPI scaling factor
    
    // Window visibility flags
    bool show_screen_;
    bool show_memory_viewer_;
    bool show_settings_;
    bool show_about_;
    
    // Window dimensions
    int window_width_;
    int window_height_;
    
public:
    GenericEmulatorGUI();
    virtual ~GenericEmulatorGUI();
    
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
     * Per-frame delay for timing control
     * Override to customize frame rate limiting
     */
    virtual uint32_t get_frame_delay_ms() const { return 16; } // ~60 FPS default
    
    // ========================================================================
    // Generic helper functions for derived classes
    // ========================================================================
    
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

private:
    // Prevent copying
    GenericEmulatorGUI(const GenericEmulatorGUI&) = delete;
    GenericEmulatorGUI& operator=(const GenericEmulatorGUI&) = delete;
};

#endif // GENERIC_GUI_H