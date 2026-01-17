#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "../../core/system_lines.h"

// VIC Register indices
#define VIC_REG_CONTROL1 0x00
#define VIC_REG_CONTROL2 0x01
#define VIC_REG_LIGHTPEN_X 0x02
#define VIC_REG_LIGHTPEN_Y 0x03
#define VIC_REG_ENABLE 0x04
#define VIC_REG_OSC1_FREQ 0x05
#define VIC_REG_OSC2_FREQ 0x06
#define VIC_REG_OSC3_FREQ 0x07
#define VIC_REG_OSC4_FREQ 0x08
#define VIC_REG_AUX_COLOR 0x09
#define VIC_REG_BACKGROUND 0x0A
#define VIC_REG_BORDER_COLOR 0x0B
#define VIC_REG_VIDEO_MATRIX 0x0C
#define VIC_REG_CHAR_BASE 0x0D
#define VIC_REG_RASTER 0x0E
#define VIC_REG_INTERRUPT 0x0F

// Control register 1 bit masks
#define VIC_C1_INTERLACE 0x80
#define VIC_C1_SCREEN_ORIGIN_X 0x7F

// Control register 2 bit masks
#define VIC_C2_SCREEN_ORIGIN_Y 0xFF

// Enable register bit masks
#define VIC_ENABLE_OSC1 0x80
#define VIC_ENABLE_OSC2 0x40
#define VIC_ENABLE_OSC3 0x20
#define VIC_ENABLE_OSC4 0x10
#define VIC_ENABLE_VIDEO 0x08
#define VIC_ENABLE_LIGHTPEN 0x04
#define VIC_ENABLE_PADDLES 0x02
#define VIC_ENABLE_SOUND 0x01

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

    // Framebuffer
    uint32_t* framebuffer;
    int framebuffer_width;
    int framebuffer_height;

    // Configuration
    bool is_pal;
    const vic_chip_config_t* config;

    // Video generation state
    bool in_display_area;
    bool in_char_area;
    bool is_char_fetch_cycle;
    uint16_t matrix_index;
    uint8_t matrix_video_byte;
    uint8_t foreground_color;
    uint8_t matrix_char_data;
    uint16_t screen_origin_x;
    uint16_t screen_origin_y;
    uint8_t no_of_columns;
    uint8_t no_of_rows;
    bool double_height;
    uint16_t base_video;
    uint16_t base_char;
} vic_base_t;

// Function prototypes
void vic_system_reset(vic_base_t* vic);
bus_state_t vic_tick(void* chip, bus_state_t bus_state);
void vic_bus_attach(void* chip, void* bus);
void vic_set_framebuffer(vic_base_t* vic, uint32_t* framebuffer, int width, int height);
uint8_t vic_read_register(vic_base_t* vic, uint8_t reg);
void vic_write_register(vic_base_t* vic, uint8_t reg, uint8_t value);