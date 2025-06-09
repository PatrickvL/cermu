#ifndef CIMGUI_INTERFACE_H
#define CIMGUI_INTERFACE_H

#include <stdint.h>
#include <stdbool.h>

// Forward declarations
struct c64_s;

// GUI State structure
typedef struct {
    bool show_cpu_state;
    bool show_memory_viewer;
    bool show_vic_registers;
    bool show_cia_registers;
    bool show_sid_registers;
    bool show_debugger;
    bool show_settings;
    bool show_about;
    bool show_screen;
    bool emulation_running;
    bool emulation_paused;
    int target_fps;
    float emulation_speed;
    
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

// GUI initialization and cleanup
bool gui_init(const char* window_title, int width, int height);
void gui_cleanup(void);
void gui_cleanup_state(gui_state_t* gui_state);

// Main GUI functions
void gui_render_frame(struct c64_s* c64, gui_state_t* gui_state);
bool gui_should_quit(void);
void gui_handle_events(void);

// Window rendering functions
void gui_render_menu_bar(struct c64_s* c64, gui_state_t* gui_state);
void gui_render_cpu_state(struct c64_s* c64, gui_state_t* gui_state);
void gui_render_memory_viewer(struct c64_s* c64, gui_state_t* gui_state);
void gui_render_vic_registers(struct c64_s* c64, gui_state_t* gui_state);
void gui_render_cia_registers(struct c64_s* c64, gui_state_t* gui_state);
void gui_render_sid_registers(struct c64_s* c64, gui_state_t* gui_state);
void gui_render_debugger(struct c64_s* c64, gui_state_t* gui_state);
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

#endif // CIMGUI_INTERFACE_H
