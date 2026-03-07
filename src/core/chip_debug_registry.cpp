/*
 * chip_debug_registry.cpp — Builder-method implementations for ChipDebugRegistry
 *
 * This file provides the non-inline registration methods.  The rendering side
 * lives in chip_debug_registry_gui.cpp (GUI builds) or is stubbed out
 * (non-GUI builds).
 */

#include "chip_debug_registry.h"

// ============================================================================
// Helpers
// ============================================================================

DebugCategory& ChipDebugRegistry::current_category() {
    if (categories_.empty()) {
        categories_.push_back({"General", true, {}});
    }
    return categories_.back();
}

void ChipDebugRegistry::push_field(DebugField&& f) {
    f.indent = current_indent_;
    current_category().fields.push_back(std::move(f));
}

uint32_t ChipDebugRegistry::read(const UIntSource& src) const {
    if (auto* rs = std::get_if<RegSource>(&src)) {
        if (!reg_data_ || rs->byte_offset >= reg_size_) return 0;
        uint32_t raw = 0;
        if (rs->byte_count >= 2 && (rs->byte_offset + 1) < reg_size_) {
            raw = reg_data_[rs->byte_offset] | (static_cast<uint32_t>(reg_data_[rs->byte_offset + 1]) << 8);
        } else {
            raw = reg_data_[rs->byte_offset];
        }
        uint32_t mask = (rs->bit_count >= 32) ? 0xFFFFFFFF : ((1u << rs->bit_count) - 1);
        return (raw >> rs->bit_offset) & mask;
    }
    auto& fn = std::get<std::function<uint32_t()>>(src);
    return fn ? fn() : 0;
}

// ============================================================================
// Category & flow control
// ============================================================================

ChipDebugRegistry& ChipDebugRegistry::category(const char* name, bool default_open) {
    categories_.push_back({name, default_open, {}});
    current_indent_ = 0;
    return *this;
}

ChipDebugRegistry& ChipDebugRegistry::indent(uint8_t level) {
    current_indent_ = level;
    return *this;
}

ChipDebugRegistry& ChipDebugRegistry::separator() {
    DebugField f;
    f.label = nullptr;
    f.kind  = DataKind::Custom;
    f.meta  = CustomMeta{[]{}};  // empty render — renderer draws a line on null-label custom
    push_field(std::move(f));
    return *this;
}

ChipDebugRegistry& ChipDebugRegistry::text(const char* static_text) {
    DebugField f;
    f.label = static_text;
    f.kind  = DataKind::Value;
    f.uint_src = std::function<uint32_t()>{};  // no value — renderer shows label only
    push_field(std::move(f));
    return *this;
}

ChipDebugRegistry& ChipDebugRegistry::text(std::function<const char*()> fn) {
    DebugField f;
    f.label      = nullptr;
    f.kind       = DataKind::Value;
    f.string_src = std::move(fn);
    push_field(std::move(f));
    return *this;
}

// ============================================================================
// Atomic scalar builders
// ============================================================================

ChipDebugRegistry& ChipDebugRegistry::value(const char* label, uint16_t reg_offset, uint8_t bits) {
    DebugField f;
    f.label        = label;
    f.kind         = DataKind::Value;
    f.display_bits = bits;
    f.uint_src     = RegSource{reg_offset, 0, bits, static_cast<uint8_t>(bits > 8 ? 2 : 1)};
    push_field(std::move(f));
    return *this;
}

ChipDebugRegistry& ChipDebugRegistry::value16(const char* label, uint16_t reg_lo, uint16_t reg_hi) {
    DebugField f;
    f.label        = label;
    f.kind         = DataKind::Value;
    f.display_bits = 16;
    // Uses a callback that combines the two register bytes.
    // This requires reg_data_ to be set.
    f.uint_src = [this, reg_lo, reg_hi]() -> uint32_t {
        if (!reg_data_) return 0;
        uint8_t lo = (reg_lo < reg_size_) ? reg_data_[reg_lo] : 0;
        uint8_t hi = (reg_hi < reg_size_) ? reg_data_[reg_hi] : 0;
        return lo | (static_cast<uint32_t>(hi) << 8);
    };
    push_field(std::move(f));
    return *this;
}

ChipDebugRegistry& ChipDebugRegistry::value(const char* label, std::function<uint32_t()> fn, uint8_t bits) {
    DebugField f;
    f.label        = label;
    f.kind         = DataKind::Value;
    f.display_bits = bits;
    f.uint_src     = std::move(fn);
    push_field(std::move(f));
    return *this;
}

ChipDebugRegistry& ChipDebugRegistry::value(const char* label, std::function<uint32_t()> fn, uint8_t bits, uint8_t ind) {
    auto saved = current_indent_;
    current_indent_ = ind;
    value(label, std::move(fn), bits);
    current_indent_ = saved;
    return *this;
}

ChipDebugRegistry& ChipDebugRegistry::flag(const char* label, uint16_t reg_offset, uint8_t bit) {
    DebugField f;
    f.label        = label;
    f.kind         = DataKind::Flag;
    f.display_bits = 1;
    f.uint_src     = RegSource{reg_offset, bit, 1, 1};
    push_field(std::move(f));
    return *this;
}

ChipDebugRegistry& ChipDebugRegistry::flag(const char* label, std::function<uint32_t()> fn) {
    DebugField f;
    f.label        = label;
    f.kind         = DataKind::Flag;
    f.display_bits = 1;
    f.uint_src     = std::move(fn);
    push_field(std::move(f));
    return *this;
}

ChipDebugRegistry& ChipDebugRegistry::state(const char* label, uint16_t reg_offset,
                                             uint8_t bit_count, uint8_t bit_offset,
                                             const char* const* names, uint8_t name_count) {
    DebugField f;
    f.label        = label;
    f.kind         = DataKind::State;
    f.display_bits = bit_count;
    f.uint_src     = RegSource{reg_offset, bit_offset, bit_count, 1};
    f.meta         = StateMeta{names, name_count};
    push_field(std::move(f));
    return *this;
}

ChipDebugRegistry& ChipDebugRegistry::state(const char* label, std::function<uint32_t()> fn,
                                             const char* const* names, uint8_t name_count) {
    DebugField f;
    f.label        = label;
    f.kind         = DataKind::State;
    f.uint_src     = std::move(fn);
    f.meta         = StateMeta{names, name_count};
    push_field(std::move(f));
    return *this;
}

ChipDebugRegistry& ChipDebugRegistry::address(const char* label, uint16_t reg_offset, uint8_t bits) {
    DebugField f;
    f.label        = label;
    f.kind         = DataKind::Address;
    f.display_bits = bits;
    f.uint_src     = RegSource{reg_offset, 0, bits, static_cast<uint8_t>(bits > 8 ? 2 : 1)};
    push_field(std::move(f));
    return *this;
}

ChipDebugRegistry& ChipDebugRegistry::address(const char* label, std::function<uint32_t()> fn, uint8_t bits) {
    DebugField f;
    f.label        = label;
    f.kind         = DataKind::Address;
    f.display_bits = bits;
    f.uint_src     = std::move(fn);
    push_field(std::move(f));
    return *this;
}

ChipDebugRegistry& ChipDebugRegistry::counter(const char* label, std::function<uint32_t()> fn, uint32_t max) {
    DebugField f;
    f.label    = label;
    f.kind     = DataKind::Counter;
    f.uint_src = std::move(fn);
    f.meta     = CounterMeta{max};
    push_field(std::move(f));
    return *this;
}

ChipDebugRegistry& ChipDebugRegistry::counter(const char* label, std::function<uint32_t()> fn,
                                               std::function<uint32_t()> max_fn) {
    DebugField f;
    f.label    = label;
    f.kind     = DataKind::Counter;
    f.uint_src = std::move(fn);
    f.meta     = CounterMeta{std::move(max_fn)};
    push_field(std::move(f));
    return *this;
}

ChipDebugRegistry& ChipDebugRegistry::level(const char* label, std::function<float()> fn) {
    DebugField f;
    f.label     = label;
    f.kind      = DataKind::Level;
    f.float_src = std::move(fn);
    push_field(std::move(f));
    return *this;
}

ChipDebugRegistry& ChipDebugRegistry::color(const char* label, uint16_t reg_offset,
                                             const uint32_t* pal, uint16_t pal_size) {
    DebugField f;
    f.label    = label;
    f.kind     = DataKind::Color;
    f.uint_src = RegSource{reg_offset, 0, 8, 1};
    f.meta     = ColorMeta{pal, pal_size};
    push_field(std::move(f));
    return *this;
}

ChipDebugRegistry& ChipDebugRegistry::color(const char* label, std::function<uint32_t()> fn,
                                             const uint32_t* pal, uint16_t pal_size) {
    DebugField f;
    f.label    = label;
    f.kind     = DataKind::Color;
    f.uint_src = std::move(fn);
    f.meta     = ColorMeta{pal, pal_size};
    push_field(std::move(f));
    return *this;
}

ChipDebugRegistry& ChipDebugRegistry::frequency(const char* label, uint16_t reg_offset, uint8_t bits) {
    DebugField f;
    f.label        = label;
    f.kind         = DataKind::Frequency;
    f.display_bits = bits;
    f.uint_src     = RegSource{reg_offset, 0, bits, static_cast<uint8_t>(bits > 8 ? 2 : 1)};
    push_field(std::move(f));
    return *this;
}

ChipDebugRegistry& ChipDebugRegistry::frequency(const char* label, std::function<uint32_t()> fn, uint8_t bits) {
    DebugField f;
    f.label        = label;
    f.kind         = DataKind::Frequency;
    f.display_bits = bits;
    f.uint_src     = std::move(fn);
    push_field(std::move(f));
    return *this;
}

ChipDebugRegistry& ChipDebugRegistry::signed_value(const char* label, std::function<int32_t()> fn, uint8_t bits) {
    DebugField f;
    f.label        = label;
    f.kind         = DataKind::SignedValue;
    f.display_bits = bits;
    // Wrap the signed function in an unsigned one — the renderer knows
    // DataKind::SignedValue and will reinterpret accordingly.
    f.uint_src = [fn = std::move(fn)]() -> uint32_t {
        return static_cast<uint32_t>(fn());
    };
    push_field(std::move(f));
    return *this;
}

// ============================================================================
// Structured / composite builders
// ============================================================================

ChipDebugRegistry& ChipDebugRegistry::bitfield(const char* label, uint16_t reg_offset,
                                                uint8_t bit_count, const char* const* labels) {
    DebugField f;
    f.label        = label;
    f.kind         = DataKind::Bitfield;
    f.display_bits = bit_count;
    f.uint_src     = RegSource{reg_offset, 0, bit_count, 1};
    f.meta         = BitfieldMeta{labels, bit_count};
    push_field(std::move(f));
    return *this;
}

ChipDebugRegistry& ChipDebugRegistry::bitfield(const char* label, std::function<uint32_t()> fn,
                                                uint8_t bit_count, const char* const* labels) {
    DebugField f;
    f.label        = label;
    f.kind         = DataKind::Bitfield;
    f.display_bits = bit_count;
    f.uint_src     = std::move(fn);
    f.meta         = BitfieldMeta{labels, bit_count};
    push_field(std::move(f));
    return *this;
}

ChipDebugRegistry& ChipDebugRegistry::port(const char* label,
                                            UIntSource data_src, UIntSource ddr_src,
                                            uint8_t pin_count,
                                            const char* const* pin_labels) {
    DebugField f;
    f.label = label;
    f.kind  = DataKind::Port;
    f.meta  = PortMeta{std::move(data_src), std::move(ddr_src), pin_count, pin_labels};
    push_field(std::move(f));
    return *this;
}

ChipDebugRegistry& ChipDebugRegistry::timer(const char* label,
                                             UIntSource counter_src, UIntSource latch_src,
                                             UIntSource running_src,
                                             const char* mode_label) {
    DebugField f;
    f.label = label;
    f.kind  = DataKind::Timer;
    f.meta  = TimerMeta{std::move(counter_src), std::move(latch_src),
                        std::move(running_src), mode_label, nullptr};
    push_field(std::move(f));
    return *this;
}

ChipDebugRegistry& ChipDebugRegistry::timer(const char* label,
                                             UIntSource counter_src, UIntSource latch_src,
                                             UIntSource running_src,
                                             std::function<const char*()> mode_fn) {
    DebugField f;
    f.label = label;
    f.kind  = DataKind::Timer;
    f.meta  = TimerMeta{std::move(counter_src), std::move(latch_src),
                        std::move(running_src), nullptr, std::move(mode_fn)};
    push_field(std::move(f));
    return *this;
}

ChipDebugRegistry& ChipDebugRegistry::audio_channel(const char* label,
                                                     UIntSource enabled_src,
                                                     UIntSource frequency_src,
                                                     UIntSource volume_src,
                                                     const char* const* waveform_names,
                                                     uint8_t waveform_count,
                                                     UIntSource waveform_src,
                                                     std::function<void()> extra_fn) {
    DebugField f;
    f.label = label;
    f.kind  = DataKind::AudioChannel;
    f.meta  = AudioChannelMeta{std::move(enabled_src), std::move(frequency_src),
                               std::move(volume_src),
                               waveform_names, waveform_count,
                               std::move(waveform_src), std::move(extra_fn)};
    push_field(std::move(f));
    return *this;
}

ChipDebugRegistry& ChipDebugRegistry::palette(const char* label,
                                               std::function<std::pair<const uint8_t*, size_t>()> data_fn,
                                               uint16_t entries, uint8_t bits_per_entry,
                                               const uint32_t* sys_palette,
                                               uint16_t sys_palette_size) {
    DebugField f;
    f.label = label;
    f.kind  = DataKind::Palette;
    f.meta  = PaletteMeta{std::move(data_fn), entries, bits_per_entry,
                          sys_palette, sys_palette_size};
    push_field(std::move(f));
    return *this;
}

ChipDebugRegistry& ChipDebugRegistry::memory(const char* label,
                                              std::function<std::pair<const uint8_t*, size_t>()> data_fn,
                                              uint16_t base_address, size_t max_display) {
    DebugField f;
    f.label = label;
    f.kind  = DataKind::Memory;
    f.meta  = MemoryMeta{std::move(data_fn), base_address, max_display};
    push_field(std::move(f));
    return *this;
}

ChipDebugRegistry& ChipDebugRegistry::pattern_tile(const char* label,
                                                    std::function<std::pair<const uint8_t*, size_t>()> data_fn,
                                                    uint8_t width, uint8_t height, uint8_t bpp,
                                                    const uint32_t* pal, uint16_t pal_size) {
    DebugField f;
    f.label = label;
    f.kind  = DataKind::PatternTile;
    f.meta  = PatternTileMeta{std::move(data_fn), width, height, bpp, pal, pal_size};
    push_field(std::move(f));
    return *this;
}

ChipDebugRegistry& ChipDebugRegistry::waveform_buffer(const char* label,
                                                       std::function<std::pair<const float*, size_t>()> data_fn,
                                                       uint32_t sample_rate) {
    DebugField f;
    f.label = label;
    f.kind  = DataKind::WaveformBuffer;
    f.meta  = WaveformBufferMeta{std::move(data_fn), sample_rate};
    push_field(std::move(f));
    return *this;
}

ChipDebugRegistry& ChipDebugRegistry::raster_position(const char* label,
                                                       UIntSource scanline_src, UIntSource cycle_src,
                                                       std::variant<uint32_t, std::function<uint32_t()>> total_lines,
                                                       std::variant<uint32_t, std::function<uint32_t()>> total_cycles) {
    DebugField f;
    f.label = label;
    f.kind  = DataKind::RasterPosition;
    f.meta  = RasterPositionMeta{std::move(scanline_src), std::move(cycle_src),
                                 std::move(total_lines), std::move(total_cycles)};
    push_field(std::move(f));
    return *this;
}

ChipDebugRegistry& ChipDebugRegistry::flag_string(const char* label, UIntSource value_src,
                                                   const char* set_chars, const char* clear_chars,
                                                   uint8_t bit_count) {
    DebugField f;
    f.label    = label;
    f.kind     = DataKind::FlagString;
    f.uint_src = std::move(value_src);
    f.meta     = FlagStringMeta{set_chars, clear_chars, bit_count};
    push_field(std::move(f));
    return *this;
}

ChipDebugRegistry& ChipDebugRegistry::custom(std::function<void()> fn) {
    DebugField f;
    f.label = nullptr;
    f.kind  = DataKind::Custom;
    f.meta  = CustomMeta{std::move(fn)};
    push_field(std::move(f));
    return *this;
}

