#pragma once
// chip_layout_ascii.hpp — ASCII pinout renderer for CLI chip inspection
//
// Renders a ChipLayout as a text-mode DIP/quad/BGA diagram, independent
// of ImGui.  Used by the --pinout CLI command, usable anywhere stdout
// or a std::string is desired.

#include "core/chip_layout.hpp"
#include <algorithm>
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

    // Chip name centered inside the body — no package suffix
    std::string name_str = chip_name ? chip_name : "";
    size_t name_dw = display_width(name_str);

    // Fixed narrow body width — real DIP packages are thin and tall.
    // 12 columns looks right for most chips; allow the name to widen
    // it slightly if needed, but cap at a reasonable maximum.
    size_t body_inner = 12;
    if (name_dw + 4 > body_inner) {
        // Name needs more room — allow up to 20, truncate beyond that
        body_inner = std::min(name_dw + 4, size_t(20));
    }
    // Ensure even so the notch looks ok
    if (body_inner % 2 != 0) ++body_inner;

    // Truncate name if it still doesn't fit
    if (name_dw > body_inner - 2) {
        // Shorten to fit with 1 space each side
        size_t max_name = body_inner - 2;
        // Trim to max_name visible columns
        size_t cols = 0;
        size_t byte_pos = 0;
        while (byte_pos < name_str.size() && cols < max_name) {
            auto c = static_cast<unsigned char>(name_str[byte_pos]);
            if (c < 0x80)       byte_pos += 1;
            else if (c < 0xE0)  byte_pos += 2;
            else if (c < 0xF0)  byte_pos += 3;
            else                byte_pos += 4;
            ++cols;
        }
        name_str = name_str.substr(0, byte_pos);
        name_dw = cols;
    }

    // Margin = left label column + " " + left number column + " "
    size_t left_margin = max_left_lbl + 1 + max_left_num + 1;

    // --- Top border with notch ---
    std::string top_border(left_margin, ' ');
    {
        size_t half = body_inner / 2;
        size_t notch = std::min(size_t(4), half);
        size_t solid_left = half - notch / 2;
        size_t solid_right = body_inner - solid_left - notch;
        // Use box-drawing characters that work in most terminals
        top_border += "+";
        for (size_t j = 0; j < solid_left; ++j) top_border += "-";
        // Notch
        top_border += "\\";
        for (size_t j = 0; j < notch - 2; ++j) top_border += " ";
        top_border += "/";
        for (size_t j = 0; j < solid_right; ++j) top_border += "-";
        top_border += "+";
    }
    fprintf(out, "%s\n", top_border.c_str());

    // --- Pin rows ---
    for (size_t i = 0; i < rows; ++i) {
        // Left side: "  LABEL  NUM |"
        // Right-align label, right-align number
        fprintf(out, "%*s %*s |",
                pad_width(left_labels[i], max_left_lbl), left_labels[i].c_str(),
                static_cast<int>(max_left_num), left_nums[i].c_str());

        // Body interior — chip name centered on middle row
        if (i == rows / 2 && !name_str.empty()) {
            int pad_total = static_cast<int>(body_inner) - static_cast<int>(name_dw);
            int pad_left = pad_total / 2;
            int pad_right = pad_total - pad_left;
            fprintf(out, "%*s%s%*s",
                    pad_left, "", name_str.c_str(), pad_right, "");
        } else {
            fprintf(out, "%*s", static_cast<int>(body_inner), "");
        }

        // Right side: "| NUM  LABEL"
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
    size_t max_top_num = 0, max_bot_num = 0;

    for (size_t i = 0; i < T.size(); ++i) {
        top_labels[i] = format_pin_label_ascii(T[i]);
        top_nums[i] = std::to_string(T[i].pin_number);
        max_top_lbl = std::max(max_top_lbl, display_width(top_labels[i]));
        max_top_num = std::max(max_top_num, top_nums[i].size());
    }
    for (size_t i = 0; i < B.size(); ++i) {
        bot_labels[i] = format_pin_label_ascii(B[i]);
        bot_nums[i] = std::to_string(B[i].pin_number);
        max_bot_lbl = std::max(max_bot_lbl, display_width(bot_labels[i]));
        max_bot_num = std::max(max_bot_num, bot_nums[i].size());
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

    std::string name_str = chip_name ? chip_name : "";
    size_t name_dw = display_width(name_str);
    
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

    // --- Top border ---
    {
        std::string border(left_margin, ' ');
        border += "+";
        for (size_t j = 0; j < body_inner; ++j) border += "-";
        border += "+";
        fprintf(out, "%s\n", border.c_str());
    }

    // --- Pin rows (left / right sides) ---
    for (size_t i = 0; i < rows; ++i) {
        fprintf(out, "%*s %*s |",
                pad_width(left_labels[i], max_left_lbl), left_labels[i].c_str(),
                static_cast<int>(max_left_num), left_nums[i].c_str());

        if (i == rows / 2 && !name_str.empty()) {
            int pad_total = static_cast<int>(body_inner) - static_cast<int>(name_dw);
            int pad_left = pad_total / 2;
            int pad_right = pad_total - pad_left;
            fprintf(out, "%*s%s%*s",
                    pad_left, "", name_str.c_str(), pad_right, "");
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
}

// ============================================================================
// BGA RENDERER (grid pins)
// ============================================================================

inline void render_bga_ascii(const ChipLayout& layout,
                             const char* chip_name,
                             FILE* out = stdout) {
    // Simple table: row letter + column numbers
    const auto& G = layout.grid_pins;
    if (G.empty()) {
        fprintf(out, "  (no grid pins defined)\n");
        return;
    }

    fprintf(out, "  %s  %s  (BGA — pin grid listing)\n\n",
            chip_name ? chip_name : "",
            layout.get_package_name().c_str());

    // Just list pins in order
    for (const auto& pin : G) {
        std::string lbl = format_pin_label_ascii(pin);
        fprintf(out, "  %3d  %-s\n", pin.pin_number, lbl.c_str());
    }
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
