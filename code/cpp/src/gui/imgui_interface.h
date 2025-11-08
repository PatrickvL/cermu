#pragma once

#include <stdint.h>
#include <stdbool.h>

// Forward declarations
struct C64System;
typedef struct C64System c64_s;  // Use typedef instead of forward declaration to match our using alias

// Aspect ratio configuration enums
typedef enum {
    ASPECT_RATIO_ORIGINAL = 0,    // Use original guest aspect ratio
    ASPECT_RATIO_4_3,             // Force 4:3 aspect ratio
    ASPECT_RATIO_16_10,           // Force 16:10 aspect ratio
    ASPECT_RATIO_16_9,            // Force 16:9 aspect ratio
    ASPECT_RATIO_CUSTOM,          // Use custom aspect ratio
    ASPECT_RATIO_PIXEL_PERFECT,   // 1:1 pixel aspect ratio
    ASPECT_RATIO_COUNT
} aspect_ratio_mode_t;

typedef enum {
    SCALING_MODE_FIT = 0,         // Fit display within window (may add black bars)
    SCALING_MODE_FILL,            // Fill entire window (may crop)
    SCALING_MODE_STRETCH,         // Stretch to fill window (may distort)
    SCALING_MODE_INTEGER,         // Use integer scaling only
    SCALING_MODE_COUNT
} scaling_mode_t;

// GUI State structure
typedef struct {
    bool show_memory_viewer;
    bool show_debugger;
    bool show_settings;
    bool show_about;
    bool show_screen;
    bool emulation_running;
    bool emulation_paused;
    int target_fps;
    float emulation_speed;
    
    // Chip Debug Windows (indexed by chip ID)
    bool show_chip_debug[16];    // Debug windows for each chip (indexed by chip ID)
    bool show_chip_settings[16]; // Settings windows for each chip (indexed by chip ID)
    
    // PLA Debug Window - now handled via chip debug system
    
    // Memory viewer state
    uint16_t memory_address;
    int memory_columns;
    
    // Debugger state
    uint16_t breakpoint_address;
    bool breakpoint_enabled;
    
    // Screen display state
    unsigned int screen_texture_id;
    float screen_scale;
    bool screen_filter;
    bool screen_scanlines;
    
    // Aspect ratio configuration
    aspect_ratio_mode_t aspect_ratio_mode;
    scaling_mode_t scaling_mode;
    float custom_aspect_ratio;        // For ASPECT_RATIO_CUSTOM mode
    bool maintain_pixel_aspect;       // Maintain square pixels
    bool show_overscan;               // Include overscan/border area
    bool center_display;              // Center display in available space
    bool show_invisible_area;         // Show non-visible area around VIC-II output
    float host_dpi_scale;             // Host DPI scaling factor
    
    // File paths
    char rom_path_basic[512];
    char rom_path_kernal[512];
    char rom_path_chargen[512];
    char cartridge_path[512];
    char disk_image_path[512];
} gui_state_t;

// Emulation thread signals
typedef enum {
    EMU_SIGNAL_NONE = 0,
    EMU_SIGNAL_START,       // Start continuous emulation
    EMU_SIGNAL_PAUSE,       // Pause emulation (using CPU intercept)
    EMU_SIGNAL_STEP,        // Single step CPU (using intercept)
    EMU_SIGNAL_RESET,       // Reset C64 system
    EMU_SIGNAL_QUIT         // Terminate emulation thread
} emulation_signal_t;

// Emulation thread state
typedef enum {
    EMU_STATE_STOPPED = 0,
    EMU_STATE_RUNNING,
    EMU_STATE_PAUSED,
    EMU_STATE_STEPPING,
    EMU_STATE_RESETTING
} emulation_state_t;

// GUI Emulation thread context
typedef struct gui_emulation_context_s {
    c64_s* c64;              // C64 system pointer
    void* thread_impl;              // Platform-specific thread implementation
    
    // Thread communication
    volatile emulation_signal_t pending_signal;
    volatile emulation_state_t current_state;
    volatile bool thread_running;
    
    // Performance tracking
    uint64_t cycles_per_second;     // Target cycles per second (PAL: ~985248)
    uint64_t frame_cycles;          // Cycles per frame (~19705 for PAL)
    uint32_t target_fps;            // Target frame rate (50 for PAL, 60 for NTSC)
    
    // Statistics
    uint64_t total_cycles_executed;
    uint32_t frames_rendered;
    uint32_t actual_fps;
} gui_emulation_context_t;

// GUI initialization and cleanup
bool gui_init(const char* window_title, int width, int height);
void gui_cleanup(void);
void gui_cleanup_state(gui_state_t* gui_state);
void gui_delay(uint32_t ms);

// Main GUI functions
void gui_render_frame(c64_s* c64, gui_state_t* gui_state, gui_emulation_context_t* emu_context);
bool gui_should_quit(void);
void gui_handle_events(gui_emulation_context_t* emu_context);

// Window rendering functions
void gui_render_menu_bar(c64_s* c64, gui_state_t* gui_state, gui_emulation_context_t* emu_context);
void gui_render_memory_viewer(c64_s* c64, gui_state_t* gui_state);
void gui_render_debugger(c64_s* c64, gui_state_t* gui_state, gui_emulation_context_t* emu_context);
void gui_render_settings(c64_s* c64, gui_state_t* gui_state);
void gui_render_about(gui_state_t* gui_state);
void gui_render_screen(c64_s* c64, gui_state_t* gui_state);
// PLA debug is now handled through the chip system via pla_render_debug_window

// Screen display functions
bool gui_init_screen_display(gui_state_t* gui_state);
void gui_cleanup_screen_display(gui_state_t* gui_state);
void gui_update_screen_texture(c64_s* c64, gui_state_t* gui_state);

// Framebuffer access functions
uint32_t* gui_get_screen_buffer(void);
void gui_get_screen_dimensions(int* width, int* height);

// Aspect ratio calculation functions
void gui_calculate_display_dimensions(gui_state_t* gui_state, float viewport_width, float viewport_height,
                                     float guest_width, float guest_height, bool is_pal,
                                     float* out_display_width, float* out_display_height,
                                     float* out_pos_x, float* out_pos_y);
float gui_get_target_aspect_ratio(gui_state_t* gui_state, bool is_pal);
void gui_get_guest_dimensions(gui_state_t* gui_state, bool is_pal,
                             float* out_width, float* out_height);

// Utility functions
void gui_init_state(gui_state_t* gui_state);
void gui_load_rom_file(const char* filepath, const char* type, gui_state_t* gui_state, gui_emulation_context_t* emu_context);
void gui_load_disk_image(const char* filepath);
bool gui_reload_roms_from_state(c64_s* c64, const gui_state_t* gui_state);
bool gui_apply_rom_changes(gui_emulation_context_t* emu_context, gui_state_t* gui_state);

// Emulation thread functions (SDL-based implementation)
bool gui_emulation_thread_init(gui_emulation_context_t* context, c64_s* c64);
void gui_emulation_thread_cleanup(gui_emulation_context_t* context);
bool gui_emulation_thread_start(gui_emulation_context_t* context);
void gui_emulation_thread_stop(gui_emulation_context_t* context);

// Emulation control functions (called from GUI to send signals)
void gui_emulation_send_signal(gui_emulation_context_t* emu_context, emulation_signal_t signal);
emulation_state_t gui_emulation_get_state(gui_emulation_context_t* emu_context);
void gui_emulation_set_speed(gui_emulation_context_t* emu_context, float speed_multiplier);
uint32_t gui_emulation_get_fps(gui_emulation_context_t* emu_context);
uint64_t gui_emulation_get_total_cycles(gui_emulation_context_t* emu_context);

// Frame rendering and timing functions (called from main GUI thread)
void gui_emulation_render_frame(gui_emulation_context_t* emu_context);
void gui_emulation_update_fps(gui_emulation_context_t* emu_context);

// Convenience wrapper functions
void gui_emulation_start(gui_emulation_context_t* emu_context);
void gui_emulation_pause(gui_emulation_context_t* emu_context);
void gui_emulation_step(gui_emulation_context_t* emu_context);
void gui_emulation_reset(gui_emulation_context_t* emu_context);


