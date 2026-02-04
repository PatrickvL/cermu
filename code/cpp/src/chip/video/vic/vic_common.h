#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "../../core/system_lines.h"

// VIC Register indices
#define VIC_REG_CONTROL1 0x00         // Interlace | ScreenOriginX
#define VIC_REG_CONTROL2 0x01         // ScreenOriginY / 2
#define VIC_REG_VIDEO_MATRIX 0x02     // BaseVideo bit 9 | NoOfColumns
#define VIC_REG_ROWS 0x03             // RasterLine bit 0 | NoOfRows | DoubleHeight
#define VIC_REG_RASTER 0x04           // RasterLine bits 8-1
#define VIC_REG_CHAR_BASE 0x05        // BaseVideo bits 13-10 | BaseChar bits 13-10
#define VIC_REG_LIGHTPEN_X 0x06
#define VIC_REG_LIGHTPEN_Y 0x07
#define VIC_REG_PADDLE_X 0x08
#define VIC_REG_PADDLE_Y 0x09
#define VIC_REG_OSC1_FREQ 0x0A        // Enable | Frequency
#define VIC_REG_OSC2_FREQ 0x0B        // Enable | Frequency
#define VIC_REG_OSC3_FREQ 0x0C        // Enable | Frequency
#define VIC_REG_OSC4_FREQ 0x0D        // Enable | Frequency
#define VIC_REG_AUX_COLOR 0x0E        // AuxiliaryColor | Volume
#define VIC_REG_BACKGROUND 0x0F       // BackgroundColor | Reversed | BorderColor

// Control register 1 bit masks
#define VIC_C1_INTERLACE 0x80
#define VIC_C1_SCREEN_ORIGIN_X_MASK 0x7F

// Control register 2 bit masks
#define VIC_C2_SCREEN_ORIGIN_Y_MASK 0xFF

// Video matrix register bit masks
#define VIC_VM_BASE_VIDEO_BIT9 0x80
#define VIC_VM_COLUMNS_MASK 0x7F

// Rows register bit masks
#define VIC_ROWS_RASTER_BIT0 0x80
#define VIC_ROWS_ROWS_MASK 0x7E
#define VIC_ROWS_ROWS_SHIFT 1
#define VIC_ROWS_DOUBLE_HEIGHT 0x01

// Character base register bit masks
#define VIC_CB_BASE_VIDEO_MASK 0xF0
#define VIC_CB_BASE_VIDEO_SHIFT 6
#define VIC_CB_BASE_CHAR_MASK 0x0F
#define VIC_CB_BASE_CHAR_SHIFT 10

// Oscillator register bit masks
#define VIC_OSC_ENABLE 0x80
#define VIC_OSC_FREQ_MASK 0x7F

// Auxiliary color register bit masks
#define VIC_AUX_COLOR_MASK 0xF0
#define VIC_AUX_COLOR_SHIFT 4
#define VIC_AUX_VOLUME_MASK 0x0F

// Background register bit masks
#define VIC_BG_BACKGROUND_MASK 0xF0
#define VIC_BG_BACKGROUND_SHIFT 4
#define VIC_BG_REVERSED 0x08
#define VIC_BG_BORDER_MASK 0x07

// Color register bit masks (for color RAM reads)
#define VIC_COLOR_MULTICOLOR 0x08
#define VIC_COLOR_FOREGROUND_MASK 0x07

// VIC timing constants
#define VIC_PAL_CYCLES_PER_LINE 63
#define VIC_PAL_TOTAL_LINES 312
#define VIC_NTSC_CYCLES_PER_LINE 65
#define VIC_NTSC_TOTAL_LINES 262

// VIC color palette (16 colors)
typedef enum {
    VIC_COLOR_BLACK = 0,
    VIC_COLOR_WHITE = 1,
    VIC_COLOR_RED = 2,
    VIC_COLOR_CYAN = 3,
    VIC_COLOR_PURPLE = 4,
    VIC_COLOR_GREEN = 5,
    VIC_COLOR_BLUE = 6,
    VIC_COLOR_YELLOW = 7,
    VIC_COLOR_ORANGE = 8,
    VIC_COLOR_BROWN = 9,
    VIC_COLOR_LIGHT_RED = 10,
    VIC_COLOR_DARK_GREY = 11,
    VIC_COLOR_MEDIUM_GREEN = 12,
    VIC_COLOR_LIGHT_GREEN = 13,
    VIC_COLOR_LIGHT_BLUE = 14,
    VIC_COLOR_LIGHT_GREY = 15
} vic_color_t;

// VIC chip configuration
typedef struct {
    uint8_t cycles_per_line;
    uint16_t total_lines;
    uint32_t clock_frequency;
    const char* chip_name;
    bool is_pal;
} vic_chip_config_t;

// Memory read callback type for VIC to access system memory
typedef uint8_t (*vic_mem_read_fn_t)(void* user_data, uint16_t addr);

// VIC chip structure (common base)
typedef struct {
    void* desc;
    void* bus;

    // Registers
    uint8_t registers[16];

    // Timing
    uint16_t raster_counter;
    uint32_t current_cycle;
    uint32_t cycles_per_line;
    uint32_t total_lines;
    uint32_t clock_frequency;

    // Video state
    uint8_t current_line[40];
    uint8_t color_ram[1024];
    uint32_t pixel_line_buffer[284];  // Max line width for rendering
    int pixel_line_index;

    // Framebuffer
    uint32_t* framebuffer;
    int framebuffer_width;
    int framebuffer_height;

    // Configuration
    bool is_pal;
    const vic_chip_config_t* config;

    // Memory access callbacks
    vic_mem_read_fn_t mem_read;
    void* mem_user_data;
    vic_mem_read_fn_t color_read;
    void* color_user_data;

    // Video generation state (updated every tick, not just register copies)
    bool in_display_area;
    bool in_char_area;
    uint16_t matrix_index;
    uint8_t matrix_video_byte;
    uint8_t matrix_color_byte;
    uint8_t matrix_char_data;
} vic_base_t;

// Function prototypes
void vic_system_reset(vic_base_t* vic);
bus_state_t vic_tick(void* chip, bus_state_t bus_state);
void vic_bus_attach(void* chip, void* bus);
void vic_set_framebuffer(vic_base_t* vic, uint32_t* framebuffer, int width, int height);
void vic_set_memory_callbacks(vic_base_t* vic, vic_mem_read_fn_t mem_read, void* mem_user_data, 
                              vic_mem_read_fn_t color_read, void* color_user_data);
uint8_t vic_read_register(vic_base_t* vic, uint8_t reg);
void vic_write_register(vic_base_t* vic, uint8_t reg, uint8_t value);