/*
 * chip_debug_registry_gui.cpp — ImGui renderer for ChipDebugRegistry
 *
 * Dispatches on DataKind to decide presentation.  Each kind has a rendering
 * strategy that can evolve independently.  When a new visual capability is
 * added (e.g. color swatches, waveform plots), it benefits every chip that
 * registered that data kind — no registration changes needed.
 *
 * Compiled only under CERMU_HAS_GUI (GUI_SOURCES).
 * The non-GUI stub is at the bottom of this file.
 */

#include "../core/chip.h"

#ifdef CERMU_HAS_GUI
#include <imgui.h>
#include "chip_visualization.h"
#include <cstdio>
#include <algorithm>

// ============================================================================
// RENDERING HELPERS
// ============================================================================

namespace {

// Label column width for aligned name: value display
constexpr float LABEL_WIDTH = 180.0f;

/// Draw a label left-aligned, then the value right-aligned on the same line.
inline void label_text(const char* label, const char* fmt, ...) {
    ImGui::Text("%-22s", label);
    ImGui::SameLine(LABEL_WIDTH);
    va_list args;
    va_start(args, fmt);
    ImGui::TextV(fmt, args);
    va_end(args);
}

/// Apply indent in pixels (20px per level).
inline void apply_indent(uint8_t level) {
    if (level > 0) ImGui::Indent(20.0f * level);
}

inline void undo_indent(uint8_t level) {
    if (level > 0) ImGui::Unindent(20.0f * level);
}

/// Format a uint value based on the number of display bits.
inline void format_value(char* buf, size_t buf_size, uint32_t val, uint8_t bits) {
    switch (bits) {
        case 1:  snprintf(buf, buf_size, "%d", val & 1); break;
        case 4:  snprintf(buf, buf_size, "$%01X (%u)", val & 0xF, val & 0xF); break;
        case 16: snprintf(buf, buf_size, "$%04X (%u)", val & 0xFFFF, val & 0xFFFF); break;
        case 24: snprintf(buf, buf_size, "$%06X", val & 0xFFFFFF); break;
        case 32: snprintf(buf, buf_size, "$%08X", val); break;
        default: snprintf(buf, buf_size, "$%02X (%u)", val & 0xFF, val & 0xFF); break;
    }
}

} // anonymous namespace

// ============================================================================
// PER-KIND RENDERERS
// ============================================================================

namespace {

void render_value(const DebugField& f, const ChipDebugRegistry& reg) {
    // Text-only field (label but no value source)
    auto* fn_ptr = std::get_if<std::function<uint32_t()>>(&f.uint_src);
    if (fn_ptr && !(*fn_ptr)) {
        // No value callback — just a text label
        if (f.label) ImGui::TextUnformatted(f.label);
        return;
    }

    uint32_t val = reg.read(f.uint_src);
    char buf[32];
    format_value(buf, sizeof(buf), val, f.display_bits);
    label_text(f.label, "%s", buf);
}

void render_flag(const DebugField& f, const ChipDebugRegistry& reg) {
    uint32_t val = reg.read(f.uint_src);
    bool set = val != 0;
    if (set)
        ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "%-22s  1 (SET)", f.label);
    else
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "%-22s  0", f.label);
}

void render_state(const DebugField& f, const ChipDebugRegistry& reg) {
    uint32_t val = reg.read(f.uint_src);
    auto& sm = std::get<StateMeta>(f.meta);
    const char* name = (val < sm.count && sm.names) ? sm.names[val] : "?";
    label_text(f.label, "%s (%u)", name, val);
}

void render_address(const DebugField& f, const ChipDebugRegistry& reg) {
    uint32_t val = reg.read(f.uint_src);
    char buf[16];
    if (f.display_bits <= 16)
        snprintf(buf, sizeof(buf), "$%04X", val & 0xFFFF);
    else
        snprintf(buf, sizeof(buf), "$%06X", val & 0xFFFFFF);
    label_text(f.label, "%s", buf);
}

void render_counter(const DebugField& f, const ChipDebugRegistry& reg) {
    uint32_t val = reg.read(f.uint_src);
    auto& cm = std::get<CounterMeta>(f.meta);
    uint32_t max = ChipDebugRegistry::resolve(cm.max);
    label_text(f.label, "%u / %u", val, max);
    // Progress bar
    float progress = (max > 0) ? (float)val / (float)max : 0.0f;
    progress = std::clamp(progress, 0.0f, 1.0f);
    ImGui::ProgressBar(progress, ImVec2(-1, 0), nullptr);
}

void render_level(const DebugField& f, const ChipDebugRegistry& /*reg*/) {
    float val = f.float_src ? f.float_src() : 0.0f;
    val = std::clamp(val, 0.0f, 1.0f);
    label_text(f.label, "%.2f", val);
    ImGui::ProgressBar(val, ImVec2(-1, 0), nullptr);
}

void render_color(const DebugField& f, const ChipDebugRegistry& reg) {
    uint32_t val = reg.read(f.uint_src);
    auto& cm = std::get<ColorMeta>(f.meta);

    char buf[32];
    snprintf(buf, sizeof(buf), "$%02X", val & 0xFF);

    // If we have a system palette, show a color swatch
    if (cm.system_palette && val < cm.system_palette_size) {
        uint32_t rgb = cm.system_palette[val];
        float r = ((rgb >> 16) & 0xFF) / 255.0f;
        float g = ((rgb >>  8) & 0xFF) / 255.0f;
        float b = ((rgb >>  0) & 0xFF) / 255.0f;
        ImVec4 color(r, g, b, 1.0f);
        ImGui::Text("%-22s", f.label);
        ImGui::SameLine(LABEL_WIDTH);
        ImGui::ColorButton("##swatch", color, ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoBorder,
                           ImVec2(14, 14));
        ImGui::SameLine();
        ImGui::Text("%s", buf);
    } else {
        label_text(f.label, "%s", buf);
    }
}

void render_frequency(const DebugField& f, const ChipDebugRegistry& reg) {
    uint32_t val = reg.read(f.uint_src);
    char buf[32];
    format_value(buf, sizeof(buf), val, f.display_bits);
    label_text(f.label, "%s", buf);
}

void render_signed_value(const DebugField& f, const ChipDebugRegistry& reg) {
    uint32_t raw = reg.read(f.uint_src);
    int32_t val = static_cast<int32_t>(raw);
    // Sign-extend based on display bits
    if (f.display_bits < 32) {
        uint32_t sign_bit = 1u << (f.display_bits - 1);
        if (raw & sign_bit) {
            val = static_cast<int32_t>(raw | ~((1u << f.display_bits) - 1));
        }
    }
    label_text(f.label, "%d", val);
}

void render_bitfield(const DebugField& f, const ChipDebugRegistry& reg) {
    uint32_t val = reg.read(f.uint_src);
    auto& bm = std::get<BitfieldMeta>(f.meta);

    // Header: label + hex value
    char buf[32];
    format_value(buf, sizeof(buf), val, bm.bit_count);
    label_text(f.label, "%s", buf);

    // Per-bit visual display with colored indicators
    ImGui::Text("  Bits: ");
    ImGui::SameLine();
    for (int i = bm.bit_count - 1; i >= 0; i--) {
        bool bit = (val >> i) & 1;
        if (bit)
            ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "1");
        else
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "0");
        if (i > 0) ImGui::SameLine();
    }

    // Bit labels row
    if (bm.labels) {
        ImGui::Text("       ");
        ImGui::SameLine();
        for (int i = bm.bit_count - 1; i >= 0; i--) {
            // Truncate to 2 chars for compact display
            char short_label[4];
            snprintf(short_label, sizeof(short_label), "%.2s", bm.labels[i]);
            ImGui::Text("%s", short_label);
            if (i > 0) ImGui::SameLine();
        }
    }
}

void render_port(const DebugField& f, const ChipDebugRegistry& reg) {
    auto& pm = std::get<PortMeta>(f.meta);
    uint32_t data = reg.read(pm.data_src);
    uint32_t ddr  = reg.read(pm.ddr_src);

    char data_buf[16], ddr_buf[16];
    snprintf(data_buf, sizeof(data_buf), "$%02X", data & 0xFF);
    snprintf(ddr_buf,  sizeof(ddr_buf),  "$%02X", ddr & 0xFF);

    label_text(f.label, "Data: %s  DDR: %s", data_buf, ddr_buf);

    // Pin-by-pin display with direction arrows
    ImGui::Text("  Pins: ");
    ImGui::SameLine();
    for (int i = pm.pin_count - 1; i >= 0; i--) {
        bool bit = (data >> i) & 1;
        bool output = (ddr >> i) & 1;
        // Output pins: green when high, orange; Input pins: gray
        ImVec4 color;
        if (output)
            color = bit ? ImVec4(0.2f, 1.0f, 0.2f, 1.0f) : ImVec4(1.0f, 0.6f, 0.0f, 1.0f);
        else
            color = bit ? ImVec4(0.7f, 0.7f, 1.0f, 1.0f) : ImVec4(0.5f, 0.5f, 0.5f, 1.0f);

        ImGui::TextColored(color, "%d", bit);
        if (i > 0) ImGui::SameLine();
    }

    // Direction arrows row
    ImGui::Text("        ");
    ImGui::SameLine();
    for (int i = pm.pin_count - 1; i >= 0; i--) {
        bool output = (ddr >> i) & 1;
        ImGui::Text("%s", output ? ">" : "<");
        if (i > 0) ImGui::SameLine();
    }
}

void render_timer(const DebugField& f, const ChipDebugRegistry& reg) {
    auto& tm = std::get<TimerMeta>(f.meta);
    uint32_t counter = reg.read(tm.counter_src);
    uint32_t latch   = reg.read(tm.latch_src);
    uint32_t running = reg.read(tm.running_src);

    ImGui::TextUnformatted(f.label);
    ImGui::Indent(20.0f);
    label_text("Counter:", "$%04X (%u)", counter, counter);
    label_text("Latch:",   "$%04X (%u)", latch, latch);
    label_text("Running:", "%s", running ? "YES" : "NO");
    if (tm.mode_fn) {
        label_text("Mode:", "%s", tm.mode_fn());
    } else if (tm.mode_label) {
        label_text("Mode:", "%s", tm.mode_label);
    }
    ImGui::Unindent(20.0f);
}

void render_audio_channel(const DebugField& f, const ChipDebugRegistry& reg) {
    auto& ac = std::get<AudioChannelMeta>(f.meta);
    uint32_t enabled = reg.read(ac.enabled_src);

    // Channel header with colored active indicator
    if (enabled)
        ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "%s [ACTIVE]", f.label);
    else
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "%s [OFF]", f.label);

    ImGui::Indent(20.0f);
    uint32_t freq = reg.read(ac.frequency_src);
    uint32_t vol  = reg.read(ac.volume_src);
    label_text("Frequency:", "$%04X (%u)", freq, freq);
    label_text("Volume:",    "%u", vol);

    if (ac.waveform_names && ac.waveform_count > 0) {
        uint32_t wf = reg.read(ac.waveform_src);
        const char* name = (wf < ac.waveform_count) ? ac.waveform_names[wf] : "?";
        label_text("Waveform:", "%s (%u)", name, wf);
    }

    if (ac.extra_render_fn) ac.extra_render_fn();
    ImGui::Unindent(20.0f);
}

void render_palette(const DebugField& f, const ChipDebugRegistry& /*reg*/) {
    auto& pm = std::get<PaletteMeta>(f.meta);
    if (!pm.data_fn) return;

    auto [data, size] = pm.data_fn();
    if (!data || size == 0) return;

    ImGui::TextUnformatted(f.label);

    // If system palette available, show color swatches
    if (pm.system_palette && pm.system_palette_size > 0) {
        // Color rectangles + indices
        int cols = 16;
        ImGui::Indent(10.0f);
        for (uint16_t i = 0; i < pm.entries && i < size; i++) {
            uint8_t idx = data[i];
            if (idx < pm.system_palette_size) {
                uint32_t rgb = pm.system_palette[idx];
                float r = ((rgb >> 16) & 0xFF) / 255.0f;
                float g = ((rgb >>  8) & 0xFF) / 255.0f;
                float b = ((rgb >>  0) & 0xFF) / 255.0f;
                ImVec4 color(r, g, b, 1.0f);

                char sw_id[16];
                snprintf(sw_id, sizeof(sw_id), "##pal%d", i);
                ImGui::ColorButton(sw_id, color,
                    ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoBorder,
                    ImVec2(14, 14));

                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("$%02X: idx $%02X", i, idx);
                }
            } else {
                ImGui::Text("%02X", idx);
            }
            if ((i + 1) % cols != 0 && i + 1 < pm.entries)
                ImGui::SameLine();
        }
        ImGui::Unindent(10.0f);
    }

    // Always show hex dump below (or instead of swatches if no palette)
    for (size_t i = 0; i < pm.entries && i < size; i += 4) {
        ImGui::Text("  $%02X: %02X %02X %02X %02X",
                    (unsigned)i,
                    data[i],
                    (i + 1 < size) ? data[i + 1] : 0,
                    (i + 2 < size) ? data[i + 2] : 0,
                    (i + 3 < size) ? data[i + 3] : 0);
    }
}

void render_memory(const DebugField& f, const ChipDebugRegistry& /*reg*/) {
    auto& mm = std::get<MemoryMeta>(f.meta);
    if (!mm.data_fn) return;

    auto [data, size] = mm.data_fn();
    if (!data || size == 0) return;

    size_t display = std::min(size, mm.max_display);

    ImGui::TextUnformatted(f.label);
    for (size_t i = 0; i < display; i += 16) {
        char line[128];
        int offset = snprintf(line, sizeof(line), "  $%04X:", (unsigned)(mm.base_address + i));
        for (size_t j = 0; j < 16 && (i + j) < display; j++) {
            offset += snprintf(line + offset, sizeof(line) - offset, " %02X", data[i + j]);
        }
        ImGui::TextUnformatted(line);
    }
}

void render_pattern_tile(const DebugField& f, const ChipDebugRegistry& /*reg*/) {
    auto& pt = std::get<PatternTileMeta>(f.meta);
    if (!pt.data_fn) return;

    auto [data, size] = pt.data_fn();
    if (!data || size == 0) return;

    // For now: hex dump.  Future: render as actual pixel image.
    ImGui::TextUnformatted(f.label);
    ImGui::Text("  %ux%u %ubpp", pt.width, pt.height, pt.bpp);
    for (size_t i = 0; i < size; i += 8) {
        char line[64];
        int offset = snprintf(line, sizeof(line), "  ");
        for (size_t j = 0; j < 8 && (i + j) < size; j++) {
            offset += snprintf(line + offset, sizeof(line) - offset, " %02X", data[i + j]);
        }
        ImGui::TextUnformatted(line);
    }
}

void render_waveform_buffer(const DebugField& f, const ChipDebugRegistry& /*reg*/) {
    auto& wm = std::get<WaveformBufferMeta>(f.meta);
    if (!wm.data_fn) return;

    auto [data, size] = wm.data_fn();
    if (!data || size == 0) return;

    ImGui::TextUnformatted(f.label);
    // Use ImGui::PlotLines for a simple oscilloscope view.
    ImGui::PlotLines("##waveform", data, static_cast<int>(size),
                     0, nullptr, -1.0f, 1.0f, ImVec2(-1, 80));
}

void render_raster_position(const DebugField& f, const ChipDebugRegistry& reg) {
    auto& rp = std::get<RasterPositionMeta>(f.meta);
    uint32_t scanline     = reg.read(rp.scanline_src);
    uint32_t cycle        = reg.read(rp.cycle_src);
    uint32_t total_lines  = ChipDebugRegistry::resolve(rp.total_lines);
    uint32_t total_cycles = ChipDebugRegistry::resolve(rp.total_cycles);

    ImGui::TextUnformatted(f.label);
    ImGui::Indent(20.0f);
    label_text("Scanline:", "%u / %u", scanline, total_lines);
    float sl_progress = (total_lines > 0) ? (float)(scanline + 1) / (float)total_lines : 0.0f;
    ImGui::ProgressBar(std::clamp(sl_progress, 0.0f, 1.0f), ImVec2(-1, 0), nullptr);

    label_text("Cycle:", "%u / %u", cycle, total_cycles);
    ImGui::Unindent(20.0f);
}

void render_flag_string(const DebugField& f, const ChipDebugRegistry& reg) {
    uint32_t val = reg.read(f.uint_src);
    auto& fs = std::get<FlagStringMeta>(f.meta);

    char buf[33] = {};
    for (int i = fs.bit_count - 1; i >= 0; i--) {
        bool set = (val >> i) & 1;
        buf[fs.bit_count - 1 - i] = set ? fs.flag_chars_when_set[fs.bit_count - 1 - i]
                                         : fs.flag_chars_when_clear[fs.bit_count - 1 - i];
    }

    ImGui::Text("%-22s", f.label);
    ImGui::SameLine(LABEL_WIDTH);
    // Color each flag character
    for (int i = 0; i < fs.bit_count; i++) {
        bool set = (val >> (fs.bit_count - 1 - i)) & 1;
        ImVec4 color = set ? ImVec4(0.2f, 1.0f, 0.2f, 1.0f) : ImVec4(0.5f, 0.5f, 0.5f, 1.0f);
        char ch[2] = {buf[i], '\0'};
        ImGui::TextColored(color, "%s", ch);
        if (i < fs.bit_count - 1) ImGui::SameLine(0, 2.0f);
    }
}

void render_custom(const DebugField& f, const ChipDebugRegistry& /*reg*/) {
    auto& cm = std::get<CustomMeta>(f.meta);
    if (!f.label) {
        // Separator sentinel
        ImGui::Separator();
    } else if (cm.render_fn) {
        cm.render_fn();
    }
}

// ============================================================================
// MAIN DISPATCHER
// ============================================================================

void render_field(const DebugField& f, const ChipDebugRegistry& reg) {
    apply_indent(f.indent);

    // Dynamic text label (no value)
    if (f.string_src && !f.label) {
        const char* txt = f.string_src();
        if (txt) ImGui::TextUnformatted(txt);
        undo_indent(f.indent);
        return;
    }

    switch (f.kind) {
        case DataKind::Value:           render_value(f, reg);           break;
        case DataKind::Flag:            render_flag(f, reg);            break;
        case DataKind::State:           render_state(f, reg);           break;
        case DataKind::Address:         render_address(f, reg);         break;
        case DataKind::Counter:         render_counter(f, reg);         break;
        case DataKind::Level:           render_level(f, reg);           break;
        case DataKind::Color:           render_color(f, reg);           break;
        case DataKind::Frequency:       render_frequency(f, reg);       break;
        case DataKind::SignedValue:     render_signed_value(f, reg);    break;
        case DataKind::Bitfield:        render_bitfield(f, reg);        break;
        case DataKind::Port:            render_port(f, reg);            break;
        case DataKind::Timer:           render_timer(f, reg);           break;
        case DataKind::AudioChannel:    render_audio_channel(f, reg);   break;
        case DataKind::Palette:         render_palette(f, reg);         break;
        case DataKind::Memory:          render_memory(f, reg);          break;
        case DataKind::PatternTile:     render_pattern_tile(f, reg);    break;
        case DataKind::WaveformBuffer:  render_waveform_buffer(f, reg); break;
        case DataKind::RasterPosition:  render_raster_position(f, reg); break;
        case DataKind::FlagString:      render_flag_string(f, reg);     break;
        case DataKind::Custom:          render_custom(f, reg);          break;
    }

    undo_indent(f.indent);
}

} // anonymous namespace

// ============================================================================
// PUBLIC: ChipDebugRegistry::render()
// ============================================================================

void ChipDebugRegistry::render() const {
    for (auto& cat : categories_) {
        ImGuiTreeNodeFlags flags = cat.default_open ? ImGuiTreeNodeFlags_DefaultOpen : 0;
        if (ImGui::CollapsingHeader(cat.name.c_str(), flags)) {
            for (auto& field : cat.fields) {
                render_field(field, *this);
            }
        }
    }
}

// ============================================================================
// PUBLIC: ChipBase::render_debug_content()  — default two-column layout
// ============================================================================

void ChipBase::render_debug_content() {
    ImVec2 window_size = ImGui::GetContentRegionAvail();

    if (has_layout_content()) {
        // Left column: Chip Visualization (fixed width ~250px)
        ImVec2 chip_viz_size = ImVec2(250.0f, 0);
        if (ImGui::BeginChild("ChipVisualization", chip_viz_size, true,
                              ImGuiWindowFlags_HorizontalScrollbar)) {
            ImGui::Text("Chip Visualization");
            ImGui::Separator();
            render_layout_content();
        }
        ImGui::EndChild();

        ImGui::SameLine(0, 5.0f);

        // Right column: registry-driven debug info
        ImVec2 right_column_size = ImVec2(window_size.x - 270.0f, 0);
        if (ImGui::BeginChild("DebugInfo", right_column_size, true,
                              ImGuiWindowFlags_HorizontalScrollbar)) {
            debug_registry_.render();
        }
        ImGui::EndChild();
    } else {
        // No chip layout — render fields directly
        debug_registry_.render();
    }
}

#else // !CERMU_HAS_GUI

// Non-GUI stubs — the registry exists but rendering is a no-op.
void ChipDebugRegistry::render() const {}
void ChipBase::render_debug_content() {}

#endif // CERMU_HAS_GUI
