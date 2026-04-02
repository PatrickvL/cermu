#pragma once
// chip_layout_ascii.hpp — ASCII pinout renderer for CLI chip inspection
//
// Renders a ChipLayout as a text-mode DIP/quad/BGA diagram, independent
// of ImGui.  Used by the --pinout CLI command, usable anywhere stdout
// or a std::string is desired.

#include "core/chip_layout.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

// ============================================================================
// UTF-8 DISPLAY WIDTH HELPERS
// ============================================================================

// Count the number of visible columns a UTF-8 string occupies.
// Assumes all codepoints are single-width (no CJK fullwidth).
inline size_t display_width(const std::string& s) {
    size_t cols = 0;
    for (size_t i = 0; i < s.size(); ) {
        auto c = static_cast<unsigned char>(s[i]);
        if (c < 0x80)      { i += 1; }
        else if (c < 0xE0) { i += 2; }
        else if (c < 0xF0) { i += 3; }
        else                { i += 4; }
        ++cols;
    }
    return cols;
}

// Printf field width needed to pad a UTF-8 string to `target_cols` visible
// columns.  Because printf counts bytes, we add the byte/column surplus.
inline int pad_width(const std::string& s, size_t target_cols) {
    size_t dw = display_width(s);
    size_t extra = s.size() - dw;  // multibyte surplus
    return static_cast<int>(target_cols + extra);
}

// ============================================================================
// PIN LABEL FORMATTING
// ============================================================================

// Format a ChipPin label for ASCII display.
// Active-low pins get a "/" prefix; others are returned verbatim.
inline std::string format_pin_label_ascii(const ChipPin& pin) {
    const char* name = pin_label_to_string(pin.label);
    if (!name || pin.label == PinLabel::UNKNOWN) return "?";
    if (get_invert_logic_from_label(pin.label))
        return std::string("/") + name;
    return name;
}

// ============================================================================
// BODY TEXT HELPERS
// ============================================================================

// Collect the lines to render inside the chip body: manufacturer,
// part name, package variant, and custom text.
inline std::vector<std::string> collect_body_lines(const ChipLayout& layout,
                                                    const char* chip_name) {
    std::vector<std::string> lines;
    if (layout.chip_info && !layout.chip_info->manufacturer.empty())
        lines.push_back(std::string(layout.chip_info->manufacturer));
    if (chip_name && *chip_name)
        lines.push_back(chip_name);
    if (!layout.markings.package_variant.empty())
        lines.push_back(std::string(layout.markings.package_variant));
    if (!layout.markings.custom_text.empty())
        lines.push_back(std::string(layout.markings.custom_text));
    return lines;
}

// Truncate a string to at most `max_cols` visible columns.
inline void truncate_to_width(std::string& s, size_t max_cols) {
    size_t cols = 0, byte_pos = 0;
    while (byte_pos < s.size() && cols < max_cols) {
        auto c = static_cast<unsigned char>(s[byte_pos]);
        if (c < 0x80)       byte_pos += 1;
        else if (c < 0xE0)  byte_pos += 2;
        else if (c < 0xF0)  byte_pos += 3;
        else                 byte_pos += 4;
        ++cols;
    }
    if (byte_pos < s.size()) s = s.substr(0, byte_pos);
}

// ============================================================================
// DIP / SOIC RENDERER (left + right pins only)
// ============================================================================

inline void render_dip_ascii(const ChipLayout& layout,
                             const char* chip_name,
                             FILE* out = stdout) {
    const auto& L = layout.left_pins;
    const auto& R = layout.right_pins;
    size_t rows = std::max(L.size(), R.size());

    // Measure widths for alignment
    std::vector<std::string> left_labels(rows), right_labels(rows);
    std::vector<std::string> left_nums(rows), right_nums(rows);
    size_t max_left_lbl = 0, max_left_num = 0;
    size_t max_right_lbl = 0, max_right_num = 0;

    for (size_t i = 0; i < rows; ++i) {
        if (i < L.size()) {
            left_labels[i] = format_pin_label_ascii(L[i]);
            left_nums[i] = std::to_string(L[i].pin_number);
        }
        if (i < R.size()) {
            right_labels[i] = format_pin_label_ascii(R[i]);
            right_nums[i] = std::to_string(R[i].pin_number);
        }
        max_left_lbl  = std::max(max_left_lbl,  display_width(left_labels[i]));
        max_left_num  = std::max(max_left_num,  left_nums[i].size());
        max_right_lbl = std::max(max_right_lbl, display_width(right_labels[i]));
        max_right_num = std::max(max_right_num, right_nums[i].size());
    }

    // --- Body text lines: manufacturer, chip name, variant, custom ---
    auto body_lines = collect_body_lines(layout, chip_name);

    // Minimum rows for body text when no pins are defined
    if (rows == 0) rows = std::max(body_lines.size() + 2, size_t(3));

    // --- Body width: base from package width, expand for text ---
    // 300mil narrow body → 12 cols; 600mil wide → 16; 800mil+ extra-wide → 20
    size_t body_inner = 12;
    if (layout.package.width >= 800.0f) body_inner = 20;
    else if (layout.package.width >= 500.0f) body_inner = 16;

    for (const auto& line : body_lines) {
        size_t need = display_width(line) + 4;
        if (need > body_inner) body_inner = std::min(need, size_t(24));
    }
    if (body_inner % 2 != 0) ++body_inner;

    // Truncate body lines that still don't fit
    for (auto& line : body_lines)
        truncate_to_width(line, body_inner - 2);

    size_t left_margin = max_left_lbl + 1 + max_left_num + 1;

    // --- Orientation marker → top-left corner character + border style ---
    OrientationMarker marker = layout.package.marker;
    char top_left_ch = '+';
    bool pin1_dot_in_body = false;
    switch (marker) {
        case OrientationMarker::DOT:           top_left_ch = '*'; break;
        case OrientationMarker::CHAMFER:       top_left_ch = '/'; break;
        case OrientationMarker::CIRCLE:        top_left_ch = 'o'; break;
        case OrientationMarker::NOTCH_AND_DOT: pin1_dot_in_body = true; break;
        default: break;
    }

    bool has_notch = (marker == OrientationMarker::NOTCH ||
                      marker == OrientationMarker::NOTCH_AND_DOT);
    bool has_bar   = (marker == OrientationMarker::BAR);
    bool has_tri   = (marker == OrientationMarker::TRIANGLE);
    char fill = has_bar ? '=' : '-';

    // --- Top border ---
    std::string top_border(left_margin, ' ');
    top_border += top_left_ch;
    if (has_notch) {
        size_t half = body_inner / 2;
        size_t notch = std::min(size_t(4), half);
        size_t solid_left = half - notch / 2;
        size_t solid_right = body_inner - solid_left - notch;
        for (size_t j = 0; j < solid_left; ++j) top_border += fill;
        top_border += "\\";
        for (size_t j = 0; j < notch - 2; ++j) top_border += " ";
        top_border += "/";
        for (size_t j = 0; j < solid_right; ++j) top_border += fill;
    } else if (has_tri) {
        size_t half = body_inner / 2;
        for (size_t j = 0; j < half; ++j) top_border += fill;
        top_border += "v";
        for (size_t j = half + 1; j < body_inner; ++j) top_border += fill;
    } else {
        for (size_t j = 0; j < body_inner; ++j) top_border += fill;
    }
    top_border += "+";
    fprintf(out, "%s\n", top_border.c_str());

    // --- Body text block, vertically centered ---
    size_t block_size = body_lines.size();
    size_t block_start = (rows > block_size) ? (rows - block_size) / 2 : 0;

    // --- Pin rows ---
    for (size_t i = 0; i < rows; ++i) {
        fprintf(out, "%*s %*s |",
                pad_width(left_labels[i], max_left_lbl), left_labels[i].c_str(),
                static_cast<int>(max_left_num), left_nums[i].c_str());

        // Body interior — text block centered vertically
        size_t line_idx = i - block_start;
        bool show_line = (i >= block_start && line_idx < block_size);

        if (show_line) {
            const auto& line = body_lines[line_idx];
            size_t ldw = display_width(line);
            int pad_total = static_cast<int>(body_inner) - static_cast<int>(ldw);
            int pl = pad_total / 2;
            int pr = pad_total - pl;
            if (pin1_dot_in_body && i == 0 && pl >= 1) {
                fprintf(out, "*%*s%s%*s", pl - 1, "", line.c_str(), pr, "");
            } else {
                fprintf(out, "%*s%s%*s", pl, "", line.c_str(), pr, "");
            }
        } else if (pin1_dot_in_body && i == 0) {
            fprintf(out, "*%*s", static_cast<int>(body_inner) - 1, "");
        } else {
            fprintf(out, "%*s", static_cast<int>(body_inner), "");
        }

        fprintf(out, "| %-*s %-s\n",
                static_cast<int>(max_right_num), right_nums[i].c_str(),
                right_labels[i].c_str());
    }

    // --- Bottom border ---
    std::string bot_border(left_margin, ' ');
    bot_border += "+";
    for (size_t j = 0; j < body_inner; ++j) bot_border += "-";
    bot_border += "+";
    fprintf(out, "%s\n", bot_border.c_str());

    // --- Package name footer ---
    std::string pkg_name = layout.get_package_name();
    if (!pkg_name.empty()) {
        size_t total_body = body_inner + 2;
        size_t pkg_dw = display_width(pkg_name);
        size_t pkg_pad = left_margin + (total_body > pkg_dw ? (total_body - pkg_dw) / 2 : 0);
        fprintf(out, "%*s%s\n", static_cast<int>(pkg_pad), "", pkg_name.c_str());
    }
    if (layout.package.has_thermal_pad || layout.package.has_center_slug)
        fprintf(out, "%*s[Thermal pad]\n", static_cast<int>(left_margin + 1), "");
}

// ============================================================================
// QUAD PACKAGE RENDERER (all 4 sides — QFP, PLCC, QFN)
// ============================================================================

inline void render_quad_ascii(const ChipLayout& layout,
                              const char* chip_name,
                              FILE* out = stdout) {
    const auto& T = layout.top_pins;
    const auto& B = layout.bottom_pins;
    const auto& L = layout.left_pins;
    const auto& R = layout.right_pins;
    size_t rows = std::max(L.size(), R.size());

    // Measure side label widths
    std::vector<std::string> left_labels(rows), right_labels(rows);
    std::vector<std::string> left_nums(rows), right_nums(rows);
    size_t max_left_lbl = 0, max_left_num = 0;
    size_t max_right_lbl = 0, max_right_num = 0;

    for (size_t i = 0; i < rows; ++i) {
        if (i < L.size()) {
            left_labels[i] = format_pin_label_ascii(L[i]);
            left_nums[i] = std::to_string(L[i].pin_number);
        }
        if (i < R.size()) {
            right_labels[i] = format_pin_label_ascii(R[i]);
            right_nums[i] = std::to_string(R[i].pin_number);
        }
        max_left_lbl  = std::max(max_left_lbl,  display_width(left_labels[i]));
        max_left_num  = std::max(max_left_num,  left_nums[i].size());
        max_right_lbl = std::max(max_right_lbl, display_width(right_labels[i]));
        max_right_num = std::max(max_right_num, right_nums[i].size());
    }

    // Top/bottom pin labels and nums
    std::vector<std::string> top_labels(T.size()), top_nums(T.size());
    std::vector<std::string> bot_labels(B.size()), bot_nums(B.size());
    size_t max_top_lbl = 0, max_bot_lbl = 0;

    for (size_t i = 0; i < T.size(); ++i) {
        top_labels[i] = format_pin_label_ascii(T[i]);
        top_nums[i] = std::to_string(T[i].pin_number);
        max_top_lbl = std::max(max_top_lbl, display_width(top_labels[i]));
    }
    for (size_t i = 0; i < B.size(); ++i) {
        bot_labels[i] = format_pin_label_ascii(B[i]);
        bot_nums[i] = std::to_string(B[i].pin_number);
        max_bot_lbl = std::max(max_bot_lbl, display_width(bot_labels[i]));
    }

    // Per-column width: max digit width of top/bottom pin number at each position
    size_t top_count = std::max(T.size(), B.size());
    std::vector<size_t> col_w(top_count, 2); // minimum 2 chars per column
    for (size_t i = 0; i < top_count; ++i) {
        if (i < T.size()) col_w[i] = std::max(col_w[i], top_nums[i].size());
        if (i < B.size()) col_w[i] = std::max(col_w[i], bot_nums[i].size());
    }
    // Body inner = sum of column widths + 1-space gaps between columns
    size_t body_inner = 0;
    for (size_t i = 0; i < top_count; ++i)
        body_inner += col_w[i] + (i + 1 < top_count ? 1 : 0);
    if (body_inner < 12) body_inner = 12;

    size_t left_margin = max_left_lbl + 1 + max_left_num + 1;

    // Body text
    auto body_lines = collect_body_lines(layout, chip_name);
    // Ensure body is wide enough for text
    for (const auto& line : body_lines) {
        size_t need = display_width(line) + 4;
        if (need > body_inner) body_inner = need;
    }
    for (auto& line : body_lines)
        truncate_to_width(line, body_inner - 2);
    
    // Stride helper: column width + trailing gap (0 for last column)
    auto col_stride = [&](size_t i) -> size_t {
        return col_w[i] + (i + 1 < top_count ? 1 : 0);
    };

    // --- Helper: render labels vertically (one char per row, rotated 90°) ---
    auto render_vertical_labels = [&](const std::vector<std::string>& labels,
                                      size_t count, size_t max_lbl) {
        for (size_t row = 0; row < max_lbl; ++row) {
            fprintf(out, "%*s", static_cast<int>(left_margin), "");
            for (size_t i = 0; i < count; ++i) {
                size_t w = col_stride(i);
                const auto& lbl = (i < labels.size()) ? labels[i] : labels[0];
                size_t dw = display_width(lbl);
                if (i < labels.size() && row < dw) {
                    size_t cols = 0, byte_start = 0, byte_end = 0;
                    for (size_t b = 0; b < lbl.size(); ) {
                        if (cols == row) byte_start = b;
                        auto c = static_cast<unsigned char>(lbl[b]);
                        if (c < 0x80) b += 1;
                        else if (c < 0xE0) b += 2;
                        else if (c < 0xF0) b += 3;
                        else b += 4;
                        ++cols;
                        if (cols == row + 1) { byte_end = b; break; }
                    }
                    std::string ch = lbl.substr(byte_start, byte_end - byte_start);
                    fprintf(out, "%s", ch.c_str());
                    for (size_t j = 1; j < w; ++j) fprintf(out, " ");
                } else {
                    fprintf(out, "%*s", static_cast<int>(w), "");
                }
            }
            fprintf(out, "\n");
        }
    };

    // --- Top pin labels (vertical) + pin numbers ---
    if (!T.empty()) {
        render_vertical_labels(top_labels, T.size(), max_top_lbl);
        // Pin numbers row
        fprintf(out, "%*s", static_cast<int>(left_margin), "");
        for (size_t i = 0; i < T.size(); ++i)
            fprintf(out, "%-*s", static_cast<int>(col_stride(i)), top_nums[i].c_str());
        fprintf(out, "\n");
        // Tick marks
        fprintf(out, "%*s", static_cast<int>(left_margin), "");
        for (size_t i = 0; i < T.size(); ++i) {
            fprintf(out, "|");
            for (size_t j = 1; j < col_stride(i); ++j) fprintf(out, " ");
        }
        fprintf(out, "\n");
    }

    // --- Pin-1 corner marker ---
    OrientationMarker marker = layout.package.marker;
    char corner_ch = '+';
    switch (marker) {
        case OrientationMarker::DOT:
        case OrientationMarker::NOTCH_AND_DOT: corner_ch = '*'; break;
        case OrientationMarker::CHAMFER:        corner_ch = '/'; break;
        case OrientationMarker::CIRCLE:         corner_ch = 'o'; break;
        default: break;
    }

    // --- Top border ---
    {
        std::string border(left_margin, ' ');
        border += corner_ch;
        for (size_t j = 0; j < body_inner; ++j) border += "-";
        border += "+";
        fprintf(out, "%s\n", border.c_str());
    }

    // --- Body text block, vertically centered ---
    size_t block_size = body_lines.size();
    size_t block_start = (rows > block_size) ? (rows - block_size) / 2 : 0;

    // --- Pin rows (left / right sides) ---
    for (size_t i = 0; i < rows; ++i) {
        fprintf(out, "%*s %*s |",
                pad_width(left_labels[i], max_left_lbl), left_labels[i].c_str(),
                static_cast<int>(max_left_num), left_nums[i].c_str());

        size_t line_idx = i - block_start;
        bool show_line = (i >= block_start && line_idx < block_size);

        if (show_line) {
            const auto& line = body_lines[line_idx];
            size_t ldw = display_width(line);
            int pad_total = static_cast<int>(body_inner) - static_cast<int>(ldw);
            int pl = pad_total / 2;
            int pr = pad_total - pl;
            fprintf(out, "%*s%s%*s", pl, "", line.c_str(), pr, "");
        } else {
            fprintf(out, "%*s", static_cast<int>(body_inner), "");
        }

        fprintf(out, "| %-*s %-s\n",
                static_cast<int>(max_right_num), right_nums[i].c_str(),
                right_labels[i].c_str());
    }

    // --- Bottom border ---
    {
        std::string border(left_margin, ' ');
        border += "+";
        for (size_t j = 0; j < body_inner; ++j) border += "-";
        border += "+";
        fprintf(out, "%s\n", border.c_str());
    }

    // --- Bottom pin labels (tick marks + pin numbers + vertical labels) ---
    if (!B.empty()) {
        // Tick marks
        fprintf(out, "%*s", static_cast<int>(left_margin), "");
        for (size_t i = 0; i < B.size(); ++i) {
            fprintf(out, "|");
            for (size_t j = 1; j < col_stride(i); ++j) fprintf(out, " ");
        }
        fprintf(out, "\n");
        // Pin numbers row
        fprintf(out, "%*s", static_cast<int>(left_margin), "");
        for (size_t i = 0; i < B.size(); ++i)
            fprintf(out, "%-*s", static_cast<int>(col_stride(i)), bot_nums[i].c_str());
        fprintf(out, "\n");
        render_vertical_labels(bot_labels, B.size(), max_bot_lbl);
    }

    // --- Package name footer ---
    std::string pkg_name = layout.get_package_name();
    if (!pkg_name.empty()) {
        size_t total_body = body_inner + 2;
        size_t pkg_dw = display_width(pkg_name);
        size_t pkg_pad = left_margin + (total_body > pkg_dw ? (total_body - pkg_dw) / 2 : 0);
        fprintf(out, "%*s%s\n", static_cast<int>(pkg_pad), "", pkg_name.c_str());
    }
    if (layout.package.has_thermal_pad || layout.package.has_center_slug)
        fprintf(out, "%*s[Thermal pad]\n", static_cast<int>(left_margin + 1), "");
}

// ============================================================================
// BGA RENDERER (grid pins)
// ============================================================================

inline void render_bga_ascii(const ChipLayout& layout,
                             const char* chip_name,
                             FILE* out = stdout) {
    const auto& G = layout.grid_pins;
    if (G.empty()) {
        fprintf(out, "  (no grid pins defined)\n");
        return;
    }

    // Header
    std::string name_str = chip_name ? chip_name : "";
    std::string pkg_name = layout.get_package_name();
    fprintf(out, "  %s", name_str.c_str());
    if (!pkg_name.empty()) fprintf(out, "  (%s)", pkg_name.c_str());
    fprintf(out, "\n");
    if (layout.chip_info && !layout.chip_info->manufacturer.empty())
        fprintf(out, "  %.*s\n",
                static_cast<int>(layout.chip_info->manufacturer.size()),
                layout.chip_info->manufacturer.data());
    fprintf(out, "\n");

    // Try grid layout for perfect squares
    size_t total = G.size();
    size_t grid_cols = static_cast<size_t>(std::sqrt(static_cast<double>(total)));
    if (grid_cols > 0 && grid_cols * grid_cols == total) {
        size_t grid_rows = grid_cols;
        // Measure max label width per cell
        size_t max_lbl = 3;
        for (const auto& pin : G) {
            std::string lbl = format_pin_label_ascii(pin);
            max_lbl = std::max(max_lbl, display_width(lbl));
        }
        size_t cell_w = std::min(max_lbl, size_t(5)) + 1;

        // Column headers
        fprintf(out, "      ");
        for (size_t c = 0; c < grid_cols; ++c)
            fprintf(out, "%-*zu", static_cast<int>(cell_w), c + 1);
        fprintf(out, "\n");

        // Grid rows
        for (size_t r = 0; r < grid_rows; ++r) {
            fprintf(out, "   %c  ", static_cast<char>('A' + r));
            for (size_t c = 0; c < grid_cols; ++c) {
                size_t idx = r * grid_cols + c;
                std::string lbl = format_pin_label_ascii(G[idx]);
                truncate_to_width(lbl, cell_w - 1);
                fprintf(out, "%-*s", static_cast<int>(cell_w), lbl.c_str());
            }
            fprintf(out, "\n");
        }
    } else {
        // Non-square — list format
        for (const auto& pin : G) {
            std::string lbl = format_pin_label_ascii(pin);
            fprintf(out, "  %3d  %-s\n", pin.pin_number, lbl.c_str());
        }
    }

    // Thermal pad / center slug annotation
    if (layout.package.has_thermal_pad || layout.package.has_center_slug)
        fprintf(out, "\n  [Exposed thermal/center pad]\n");
}

// ============================================================================
// DISPATCH — auto-select renderer from package type
// ============================================================================

inline void render_chip_pinout_ascii(const ChipLayout& layout,
                                     const char* chip_name,
                                     FILE* out = stdout) {
    bool has_top_bottom = !layout.top_pins.empty() || !layout.bottom_pins.empty();
    bool has_grid = !layout.grid_pins.empty();

    if (has_grid) {
        render_bga_ascii(layout, chip_name, out);
    } else if (has_top_bottom) {
        render_quad_ascii(layout, chip_name, out);
    } else {
        render_dip_ascii(layout, chip_name, out);
    }
}
