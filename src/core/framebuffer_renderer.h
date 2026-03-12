#pragma once

#include "core/system.h"
#include <cstdint>
#include <cstring>

/**
 * Generic framebuffer renderer
 * Converts native system framebuffers to RGBA8888 for display
 */
class FramebufferRenderer {
public:
    /**
     * Convert native framebuffer to RGBA8888
     * 
     * @param native_buffer Native system framebuffer
     * @param native_width Width of native framebuffer
     * @param native_height Height of native framebuffer
     * @param format Format of native framebuffer
     * @param palette Color palette (for indexed modes)
     * @param rgba_buffer Output RGBA8888 buffer (must be pre-allocated)
     * @param rgba_width Width of output buffer
     * @param rgba_height Height of output buffer
     */
    static void convert_to_rgba(
        const uint8_t* native_buffer,
        int native_width,
        int native_height,
        FramebufferFormat format,
        const std::vector<PaletteColor>& palette,
        uint32_t* rgba_buffer,
        int rgba_width,
        int rgba_height
    );
    
    /**
     * Convert 1-bit monochrome to RGBA8888
     * Each bit represents one pixel (0 or 1)
     * Packed format: 8 pixels per byte, MSB first
     */
    static void convert_monochrome_1bit(
        const uint8_t* native_buffer,
        int native_width,
        int native_height,
        const PaletteColor& color0,  // Color for 0 bits
        const PaletteColor& color1,  // Color for 1 bits
        uint32_t* rgba_buffer,
        int rgba_width,
        int rgba_height
    );
    
    /**
     * Convert 2-bit palette-indexed to RGBA8888
     * Each pixel is 2 bits (0-3), packed 4 pixels per byte
     */
    static void convert_palette_2bit(
        const uint8_t* native_buffer,
        int native_width,
        int native_height,
        const std::vector<PaletteColor>& palette,
        uint32_t* rgba_buffer,
        int rgba_width,
        int rgba_height
    );
    
    /**
     * Convert 4-bit palette-indexed to RGBA8888
     * Each pixel is 4 bits (0-15), packed 2 pixels per byte
     */
    static void convert_palette_4bit(
        const uint8_t* native_buffer,
        int native_width,
        int native_height,
        const std::vector<PaletteColor>& palette,
        uint32_t* rgba_buffer,
        int rgba_width,
        int rgba_height
    );
    
    /**
     * Convert 8-bit palette-indexed to RGBA8888
     * Each pixel is 8 bits (0-255), one pixel per byte
     */
    static void convert_palette_8bit(
        const uint8_t* native_buffer,
        int native_width,
        int native_height,
        const std::vector<PaletteColor>& palette,
        uint32_t* rgba_buffer,
        int rgba_width,
        int rgba_height
    );
    
    /**
     * Convert RGB565 to RGBA8888
     */
    static void convert_rgb565(
        const uint16_t* native_buffer,
        int native_width,
        int native_height,
        uint32_t* rgba_buffer,
        int rgba_width,
        int rgba_height
    );
    
    /**
     * Convert RGB888 to RGBA8888
     */
    static void convert_rgb888(
        const uint8_t* native_buffer,
        int native_width,
        int native_height,
        uint32_t* rgba_buffer,
        int rgba_width,
        int rgba_height
    );
};