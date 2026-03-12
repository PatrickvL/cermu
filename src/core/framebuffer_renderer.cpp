#include "core/framebuffer_renderer.h"
#include <algorithm>

// ============================================================================
// Main conversion dispatcher
// ============================================================================

void FramebufferRenderer::convert_to_rgba(
    const uint8_t* native_buffer,
    int native_width,
    int native_height,
    FramebufferFormat format,
    const std::vector<PaletteColor>& palette,
    uint32_t* rgba_buffer,
    int rgba_width,
    int rgba_height
) {
    if (!native_buffer || !rgba_buffer) {
        return;
    }
    
    switch (format) {
        case FramebufferFormat::MONOCHROME_1:
            if (palette.size() >= 2) {
                convert_monochrome_1bit(
                    native_buffer, native_width, native_height,
                    palette[0], palette[1],
                    rgba_buffer, rgba_width, rgba_height
                );
            }
            break;
            
        case FramebufferFormat::PALETTE_INDEXED_2:
            convert_palette_2bit(
                native_buffer, native_width, native_height, palette,
                rgba_buffer, rgba_width, rgba_height
            );
            break;
            
        case FramebufferFormat::PALETTE_INDEXED_4:
            convert_palette_4bit(
                native_buffer, native_width, native_height, palette,
                rgba_buffer, rgba_width, rgba_height
            );
            break;
            
        case FramebufferFormat::PALETTE_INDEXED_8:
            convert_palette_8bit(
                native_buffer, native_width, native_height, palette,
                rgba_buffer, rgba_width, rgba_height
            );
            break;
            
        case FramebufferFormat::RGB565:
            convert_rgb565(
                reinterpret_cast<const uint16_t*>(native_buffer),
                native_width, native_height,
                rgba_buffer, rgba_width, rgba_height
            );
            break;
            
        case FramebufferFormat::RGB888:
            convert_rgb888(
                native_buffer, native_width, native_height,
                rgba_buffer, rgba_width, rgba_height
            );
            break;
            
        case FramebufferFormat::RGBA8888:
            // Already in target format - direct copy
            if (native_width == rgba_width && native_height == rgba_height) {
                memcpy(rgba_buffer, native_buffer, 
                       native_width * native_height * sizeof(uint32_t));
            }
            break;
    }
}

// ============================================================================
// 1-bit monochrome conversion (CHIP-8, ZX Spectrum, etc.)
// ============================================================================

void FramebufferRenderer::convert_monochrome_1bit(
    const uint8_t* native_buffer,
    int native_width,
    int native_height,
    const PaletteColor& color0,
    const PaletteColor& color1,
    uint32_t* rgba_buffer,
    int rgba_width,
    int rgba_height
) {
    if (!native_buffer || !rgba_buffer) {
        return;
    }
    
    uint32_t rgba_color0 = color0.to_rgba32();
    uint32_t rgba_color1 = color1.to_rgba32();
    
    // Calculate stride (bytes per row)
    int bytes_per_row = (native_width + 7) / 8;  // Round up to nearest byte
    
    for (int y = 0; y < std::min(native_height, rgba_height); y++) {
        for (int x = 0; x < std::min(native_width, rgba_width); x++) {
            // Calculate byte and bit position
            int byte_index = y * bytes_per_row + (x / 8);
            int bit_index = 7 - (x % 8);  // MSB first
            
            // Extract bit
            uint8_t bit = (native_buffer[byte_index] >> bit_index) & 1;
            
            // Set RGBA pixel
            rgba_buffer[y * rgba_width + x] = bit ? rgba_color1 : rgba_color0;
        }
    }
}

// ============================================================================
// 2-bit palette conversion (4 colors)
// ============================================================================

void FramebufferRenderer::convert_palette_2bit(
    const uint8_t* native_buffer,
    int native_width,
    int native_height,
    const std::vector<PaletteColor>& palette,
    uint32_t* rgba_buffer,
    int rgba_width,
    int rgba_height
) {
    if (!native_buffer || !rgba_buffer || palette.size() < 4) {
        return;
    }
    
    // Pre-convert palette to RGBA32
    uint32_t rgba_palette[4];
    for (int i = 0; i < 4; i++) {
        rgba_palette[i] = palette[i].to_rgba32();
    }
    
    // 4 pixels per byte (2 bits each)
    int bytes_per_row = (native_width + 3) / 4;
    
    for (int y = 0; y < std::min(native_height, rgba_height); y++) {
        for (int x = 0; x < std::min(native_width, rgba_width); x++) {
            int byte_index = y * bytes_per_row + (x / 4);
            int pixel_in_byte = 3 - (x % 4);  // MSB first
            int shift = pixel_in_byte * 2;
            
            uint8_t palette_index = (native_buffer[byte_index] >> shift) & 0x03;
            rgba_buffer[y * rgba_width + x] = rgba_palette[palette_index];
        }
    }
}

// ============================================================================
// 4-bit palette conversion (16 colors - C64, NES, etc.)
// ============================================================================

void FramebufferRenderer::convert_palette_4bit(
    const uint8_t* native_buffer,
    int native_width,
    int native_height,
    const std::vector<PaletteColor>& palette,
    uint32_t* rgba_buffer,
    int rgba_width,
    int rgba_height
) {
    if (!native_buffer || !rgba_buffer || palette.size() < 16) {
        return;
    }
    
    // Pre-convert palette to RGBA32
    uint32_t rgba_palette[16];
    for (int i = 0; i < 16; i++) {
        rgba_palette[i] = palette[i].to_rgba32();
    }
    
    // 2 pixels per byte (4 bits each)
    int bytes_per_row = (native_width + 1) / 2;
    
    for (int y = 0; y < std::min(native_height, rgba_height); y++) {
        for (int x = 0; x < std::min(native_width, rgba_width); x++) {
            int byte_index = y * bytes_per_row + (x / 2);
            bool high_nibble = (x % 2) == 0;
            
            uint8_t palette_index = high_nibble 
                ? (native_buffer[byte_index] >> 4) & 0x0F
                : native_buffer[byte_index] & 0x0F;
            
            rgba_buffer[y * rgba_width + x] = rgba_palette[palette_index];
        }
    }
}

// ============================================================================
// 8-bit palette conversion (256 colors)
// ============================================================================

void FramebufferRenderer::convert_palette_8bit(
    const uint8_t* native_buffer,
    int native_width,
    int native_height,
    const std::vector<PaletteColor>& palette,
    uint32_t* rgba_buffer,
    int rgba_width,
    int rgba_height
) {
    if (!native_buffer || !rgba_buffer || palette.empty()) {
        return;
    }
    
    // Pre-convert palette to RGBA32 (stack array — palettes are always ≤256 entries)
    uint32_t rgba_palette[256];
    const size_t pal_count = std::min(palette.size(), size_t(256));
    for (size_t i = 0; i < pal_count; i++) {
        rgba_palette[i] = palette[i].to_rgba32();
    }
    
    for (int y = 0; y < std::min(native_height, rgba_height); y++) {
        for (int x = 0; x < std::min(native_width, rgba_width); x++) {
            int index = y * native_width + x;
            uint8_t palette_index = native_buffer[index];
            
            if (palette_index < pal_count) {
                rgba_buffer[y * rgba_width + x] = rgba_palette[palette_index];
            }
        }
    }
}

// ============================================================================
// RGB565 conversion
// ============================================================================

void FramebufferRenderer::convert_rgb565(
    const uint16_t* native_buffer,
    int native_width,
    int native_height,
    uint32_t* rgba_buffer,
    int rgba_width,
    int rgba_height
) {
    if (!native_buffer || !rgba_buffer) {
        return;
    }
    
    for (int y = 0; y < std::min(native_height, rgba_height); y++) {
        for (int x = 0; x < std::min(native_width, rgba_width); x++) {
            uint16_t rgb565 = native_buffer[y * native_width + x];
            
            // Extract RGB components
            uint8_t r = ((rgb565 >> 11) & 0x1F) << 3;  // 5 bits to 8 bits
            uint8_t g = ((rgb565 >> 5) & 0x3F) << 2;   // 6 bits to 8 bits
            uint8_t b = (rgb565 & 0x1F) << 3;          // 5 bits to 8 bits
            
            // Construct RGBA32
            rgba_buffer[y * rgba_width + x] = 
                (0xFF << 24) | (b << 16) | (g << 8) | r;
        }
    }
}

// ============================================================================
// RGB888 conversion
// ============================================================================

void FramebufferRenderer::convert_rgb888(
    const uint8_t* native_buffer,
    int native_width,
    int native_height,
    uint32_t* rgba_buffer,
    int rgba_width,
    int rgba_height
) {
    if (!native_buffer || !rgba_buffer) {
        return;
    }
    
    for (int y = 0; y < std::min(native_height, rgba_height); y++) {
        for (int x = 0; x < std::min(native_width, rgba_width); x++) {
            int src_index = (y * native_width + x) * 3;
            uint8_t r = native_buffer[src_index + 0];
            uint8_t g = native_buffer[src_index + 1];
            uint8_t b = native_buffer[src_index + 2];
            
            rgba_buffer[y * rgba_width + x] = 
                (0xFF << 24) | (b << 16) | (g << 8) | r;
        }
    }
}