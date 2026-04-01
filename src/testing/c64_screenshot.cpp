#include "core/cermu.hpp"
#include "testing/c64_screenshot.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <unordered_set>

// stb_image for PNG loading (test comparison only — implementation in stb_impl.cpp)
#include <stb_image.h>

namespace c64_test {

// Helper: Calculate color distance (RGB only, ignore alpha)
static inline int color_distance(uint32_t c1, uint32_t c2) {
    int r1 = (c1 >> 0) & 0xFF;
    int g1 = (c1 >> 8) & 0xFF;
    int b1 = (c1 >> 16) & 0xFF;
    
    int r2 = (c2 >> 0) & 0xFF;
    int g2 = (c2 >> 8) & 0xFF;
    int b2 = (c2 >> 16) & 0xFF;
    
    int dr = r1 - r2;
    int dg = g1 - g2;
    int db = b1 - b2;
    
    return dr*dr + dg*dg + db*db;
}

// Extract unique color palette from image
std::vector<uint32_t> extract_palette(const uint32_t* image_data, int width, int height) {
    std::unordered_set<uint32_t> unique_colors;
    
    for (int i = 0; i < width * height; i++) {
        // Mask out alpha channel, keep RGB only
        uint32_t rgb = image_data[i] & 0x00FFFFFF;
        unique_colors.insert(rgb);
    }
    
    std::vector<uint32_t> palette(unique_colors.begin(), unique_colors.end());
    std::sort(palette.begin(), palette.end());
    
    return palette;
}

// Find closest color in palette
uint32_t find_closest_palette_color(uint32_t color, const std::vector<uint32_t>& palette) {
    if (palette.empty()) {
        return color;
    }
    
    uint32_t rgb = color & 0x00FFFFFF;
    uint32_t closest = palette[0];
    int min_dist = color_distance(rgb, palette[0]);
    
    for (size_t i = 1; i < palette.size(); i++) {
        int dist = color_distance(rgb, palette[i]);
        if (dist < min_dist) {
            min_dist = dist;
            closest = palette[i];
        }
    }
    
    return closest | (color & 0xFF000000); // Preserve alpha
}

// Detect border regions by finding uniform color edges
bool detect_borders(const uint32_t* image_data, int width, int height, BorderInfo& info) {
    info.detected = false;
    info.left_border = 0;
    info.right_border = 0;
    info.top_border = 0;
    info.bottom_border = 0;
    info.border_color = 0;
    
    if (!image_data || width < 10 || height < 10) {
        return false;
    }
    
    // Sample corner pixels to detect border color
    // Check if all corners have the same color
    uint32_t tl = image_data[0] & 0x00FFFFFF;
    uint32_t tr = image_data[width - 1] & 0x00FFFFFF;
    uint32_t bl = image_data[(height - 1) * width] & 0x00FFFFFF;
    uint32_t br = image_data[(height - 1) * width + (width - 1)] & 0x00FFFFFF;
    
    // If corners don't match, likely no uniform border
    if (tl != tr || tl != bl || tl != br) {
        return false;
    }
    
    info.border_color = tl | 0xFF000000;
    
    // Detect left border (scan from left edge inward)
    for (int x = 0; x < width / 2; x++) {
        bool all_match = true;
        for (int y = 0; y < height; y++) {
            if ((image_data[y * width + x] & 0x00FFFFFF) != tl) {
                all_match = false;
                break;
            }
        }
        if (!all_match) {
            info.left_border = x;
            break;
        }
    }
    
    // Detect right border (scan from right edge inward)
    for (int x = width - 1; x >= width / 2; x--) {
        bool all_match = true;
        for (int y = 0; y < height; y++) {
            if ((image_data[y * width + x] & 0x00FFFFFF) != tl) {
                all_match = false;
                break;
            }
        }
        if (!all_match) {
            info.right_border = width - 1 - x;
            break;
        }
    }
    
    // Detect top border (scan from top edge downward)
    for (int y = 0; y < height / 2; y++) {
        bool all_match = true;
        for (int x = 0; x < width; x++) {
            if ((image_data[y * width + x] & 0x00FFFFFF) != tl) {
                all_match = false;
                break;
            }
        }
        if (!all_match) {
            info.top_border = y;
            break;
        }
    }
    
    // Detect bottom border (scan from bottom edge upward)
    for (int y = height - 1; y >= height / 2; y--) {
        bool all_match = true;
        for (int x = 0; x < width; x++) {
            if ((image_data[y * width + x] & 0x00FFFFFF) != tl) {
                all_match = false;
                break;
            }
        }
        if (!all_match) {
            info.bottom_border = height - 1 - y;
            break;
        }
    }
    
    // Consider borders detected if we found non-zero borders
    info.detected = (info.left_border > 0 || info.right_border > 0 ||
                     info.top_border > 0 || info.bottom_border > 0);
    
    return info.detected;
}

// Analyze reference image to extract dimensions, borders, and palette
bool analyze_reference_image(const std::string& reference_png, ReferenceImageInfo& info) {
    // Load reference image
    int width, height, channels;
    unsigned char* img_data = stbi_load(reference_png.c_str(), &width, &height, &channels, 4);
    
    if (!img_data) {
        log_error("ERROR: Failed to load reference image: %s\n", reference_png.c_str());
        return false;
    }
    
    info.width = width;
    info.height = height;
    
    // Convert to uint32_t* for analysis
    uint32_t* rgba_data = reinterpret_cast<uint32_t*>(img_data);
    
    // Detect borders
    detect_borders(rgba_data, width, height, info.borders);
    
    // Extract palette
    info.palette = extract_palette(rgba_data, width, height);
    info.has_palette = !info.palette.empty();
    
    if (info.borders.detected) {
        log_info("  Border detection: L=%d R=%d T=%d B=%d (color=0x%06X)\n",
               info.borders.left_border, info.borders.right_border,
               info.borders.top_border, info.borders.bottom_border,
               info.borders.border_color & 0x00FFFFFF);
    }
    
    if (info.has_palette) {
        log_info("  Palette extracted: %zu unique colors\n", info.palette.size());
    }
    
    stbi_image_free(img_data);
    return true;
}

// Enhanced comparison with palette support
bool compare_png_images(const std::string& generated_png,
                       const std::string& reference_png,
                       const ReferenceImageInfo* reference_info,
                       uint8_t diff_threshold,
                       int max_diff_pixels,
                       int* out_diff_count) {
    
    // Load generated image
    int gen_width, gen_height, gen_channels;
    unsigned char* gen_data = stbi_load(generated_png.c_str(), &gen_width, &gen_height, 
                                       &gen_channels, 4); // Force RGBA
    if (!gen_data) {
        log_error("ERROR: Failed to load generated PNG: %s\n", generated_png.c_str());
        return false;
    }

    // Load reference image
    int ref_width, ref_height, ref_channels;
    unsigned char* ref_data = stbi_load(reference_png.c_str(), &ref_width, &ref_height, 
                                       &ref_channels, 4); // Force RGBA
    if (!ref_data) {
        log_error("ERROR: Failed to load reference PNG: %s\n", reference_png.c_str());
        stbi_image_free(gen_data);
        return false;
    }

    // Check dimensions match
    if (gen_width != ref_width || gen_height != ref_height) {
        log_error("ERROR: Image dimensions mismatch: generated %dx%d vs reference %dx%d\n",
                gen_width, gen_height, ref_width, ref_height);
        stbi_image_free(gen_data);
        stbi_image_free(ref_data);
        return false;
    }

    // Get or analyze reference info
    ReferenceImageInfo local_info;
    const ReferenceImageInfo* info = reference_info;
    if (!info) {
        if (analyze_reference_image(reference_png, local_info)) {
            info = &local_info;
        }
    }
    
    // Determine comparison mode
    bool use_palette = (info && info->has_palette && info->palette.size() <= 256);
    
    // Pixel-by-pixel comparison
    int diff_count = 0;
    int total_pixels = gen_width * gen_height;
    uint32_t* gen_rgba = reinterpret_cast<uint32_t*>(gen_data);
    uint32_t* ref_rgba = reinterpret_cast<uint32_t*>(ref_data);
    
    for (int i = 0; i < total_pixels; i++) {
        uint32_t gen_pixel = gen_rgba[i];
        uint32_t ref_pixel = ref_rgba[i];
        
        // If using palette, snap generated pixel to nearest palette color
        if (use_palette) {
            gen_pixel = find_closest_palette_color(gen_pixel, info->palette);
        }
        
        // Compare RGB channels (ignore alpha)
        int r_gen = (gen_pixel >> 0) & 0xFF;
        int g_gen = (gen_pixel >> 8) & 0xFF;
        int b_gen = (gen_pixel >> 16) & 0xFF;
        
        int r_ref = (ref_pixel >> 0) & 0xFF;
        int g_ref = (ref_pixel >> 8) & 0xFF;
        int b_ref = (ref_pixel >> 16) & 0xFF;
        
        int r_diff = abs(r_gen - r_ref);
        int g_diff = abs(g_gen - g_ref);
        int b_diff = abs(b_gen - b_ref);
        
        // If any channel exceeds threshold, count as different pixel
        if (r_diff > diff_threshold || g_diff > diff_threshold || b_diff > diff_threshold) {
            diff_count++;
        }
    }

    // Clean up
    stbi_image_free(gen_data);
    stbi_image_free(ref_data);

    // Output diff count
    if (out_diff_count) {
        *out_diff_count = diff_count;
    }

    // Check if within acceptable difference
    bool match = (diff_count <= max_diff_pixels);
    
    if (!match) {
        log_error("Image comparison failed: %d pixels differ (threshold: %d, max_allowed: %d, palette: %s)\n",
                diff_count, diff_threshold, max_diff_pixels, use_palette ? "yes" : "no");
    }
    
    return match;
}

} // namespace c64_test
