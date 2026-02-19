#pragma once

#include <stdint.h>
#include <string>
#include <vector>

namespace c64_test {

// Detected border region in reference image
struct BorderInfo {
    int left_border;    // Pixels from left edge
    int right_border;   // Pixels from right edge
    int top_border;     // Pixels from top edge
    int bottom_border;  // Pixels from bottom edge
    uint32_t border_color; // RGBA color of border
    bool detected;      // Whether borders were successfully detected
};

// Reference image analysis results
struct ReferenceImageInfo {
    int width;
    int height;
    BorderInfo borders;
    std::vector<uint32_t> palette;  // Unique colors found in reference
    bool has_palette;
};

/**
 * Analyze reference PNG to extract dimensions, border info, and color palette
 * @param reference_png Path to reference PNG
 * @param info Output: analysis results
 * @return true if analysis succeeded
 */
bool analyze_reference_image(const std::string& reference_png, ReferenceImageInfo& info);

/**
 * Detect border regions in an image by finding uniform color edges
 * @param image_data RGBA pixel data
 * @param width Image width
 * @param height Image height
 * @param info Output: border information
 * @return true if borders detected
 */
bool detect_borders(const uint32_t* image_data, int width, int height, BorderInfo& info);

/**
 * Extract unique color palette from image
 * @param image_data RGBA pixel data
 * @param width Image width
 * @param height Image height
 * @return Vector of unique RGBA colors
 */
std::vector<uint32_t> extract_palette(const uint32_t* image_data, int width, int height);

/**
 * Find closest color in palette (for palette-aware comparison)
 * @param color RGBA color to match
 * @param palette Vector of palette colors
 * @return Closest matching color from palette
 */
uint32_t find_closest_palette_color(uint32_t color, const std::vector<uint32_t>& palette);

/**
 * Compare two PNG images with palette-aware comparison
 * @param generated_png Path to generated PNG
 * @param reference_png Path to reference PNG
 * @param reference_info Pre-analyzed reference image info (optional, will analyze if null)
 * @param diff_threshold Maximum allowed pixel difference per channel (if not using palette)
 * @param max_diff_pixels Maximum number of different pixels allowed
 * @param out_diff_count Output: number of pixels that differ
 * @return true if images match within threshold
 */
bool compare_png_images(const std::string& generated_png,
                       const std::string& reference_png,
                       const ReferenceImageInfo* reference_info = nullptr,
                       uint8_t diff_threshold = 5,
                       int max_diff_pixels = 0,
                       int* out_diff_count = nullptr);

} // namespace c64_test