#ifndef CIMGUI_INTERFACE_H
#define CIMGUI_INTERFACE_H

#include <stdint.h>
#include <stdbool.h>

// Forward declarations
struct c64_s;

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

// Emulation thread context
typedef struct emulation_context_s {
    struct c64_s* c64;              // C64 system pointer
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
} emulation_context_t;

// GUI initialization and cleanup
bool gui_init(const char* window_title, int width, int height);
void gui_cleanup(void);
void gui_cleanup_state(gui_state_t* gui_state);
void gui_delay(uint32_t ms);

// Main GUI functions
void gui_render_frame(struct c64_s* c64, gui_state_t* gui_state);
void gui_render_frame_with_context(struct c64_s* c64, gui_state_t* gui_state, struct emulation_context_s* emu_context);
bool gui_should_quit(void);
void gui_handle_events(void);

// Window rendering functions
void gui_render_menu_bar(struct c64_s* c64, gui_state_t* gui_state);
void gui_render_menu_bar_with_context(struct c64_s* c64, gui_state_t* gui_state, struct emulation_context_s* emu_context);
void gui_render_memory_viewer(struct c64_s* c64, gui_state_t* gui_state);
void gui_render_debugger(struct c64_s* c64, gui_state_t* gui_state);
void gui_render_debugger_with_context(struct c64_s* c64, gui_state_t* gui_state, struct emulation_context_s* emu_context);
void gui_render_settings(struct c64_s* c64, gui_state_t* gui_state);
void gui_render_about(gui_state_t* gui_state);
void gui_render_screen(struct c64_s* c64, gui_state_t* gui_state);

// Screen display functions
bool gui_init_screen_display(gui_state_t* gui_state);
void gui_cleanup_screen_display(gui_state_t* gui_state);
void gui_update_screen_texture(struct c64_s* c64, gui_state_t* gui_state);

// Utility functions
void gui_init_state(gui_state_t* gui_state);
void gui_load_rom_file(const char* filepath, const char* type);
void gui_load_disk_image(const char* filepath);

// Emulation thread functions (SDL-based implementation)
bool gui_emulation_thread_init(emulation_context_t* context, struct c64_s* c64);
void gui_emulation_thread_cleanup(emulation_context_t* context);
bool gui_emulation_thread_start(emulation_context_t* context);
void gui_emulation_thread_stop(emulation_context_t* context);

// Emulation control functions (called from GUI to send signals)
void gui_emulation_send_signal(emulation_context_t* emu_context, emulation_signal_t signal);
emulation_state_t gui_emulation_get_state(emulation_context_t* emu_context);
void gui_emulation_set_speed(emulation_context_t* emu_context, float speed_multiplier);
uint32_t gui_emulation_get_fps(emulation_context_t* emu_context);
uint64_t gui_emulation_get_total_cycles(emulation_context_t* emu_context);

// Frame rendering and timing functions (called from main GUI thread)
void gui_emulation_render_frame(emulation_context_t* emu_context);
void gui_emulation_update_fps(emulation_context_t* emu_context);

// Convenience wrapper functions
void gui_emulation_start(emulation_context_t* emu_context);
void gui_emulation_pause(emulation_context_t* emu_context);
void gui_emulation_step(emulation_context_t* emu_context);
void gui_emulation_reset(emulation_context_t* emu_context);

#endif // CIMGUI_INTERFACE_H
