#pragma once
/*
 * ay_psg_traits.hpp — Compile-time traits for the AY-3-8910 PSG family
 *
 * Covers the full family tree:
 *   AY-3-8910:  Original GI, 2 I/O ports, 40-pin DIP
 *   AY-3-8912:  1 I/O port, 28-pin DIP
 *   AY-3-8913:  No I/O ports, 24-pin DIP
 *   AY-3-8914:  Reshuffled register map (Intellivision)
 *   YM2149:     Yamaha SSG clone, half-step envelope (32 steps)
 *   YM3439:     CMOS YM2149
 *   AY8930:     Enhanced clone, per-channel envelope + extended noise
 *
 * Pattern follows fam65xx CPUTraits — NTTP via const&, inline constexpr
 * instances, helper methods for zero-overhead feature gating.
 */

#include <cstdint>

// ============================================================================
// AY Register Map — standard vs. Intellivision-reshuffled
// ============================================================================

enum class AYRegisterMap : uint8_t {
    STANDARD,       // AY-3-8910/8912/8913, YM2149, YM3439, AY8930
    INTELLIVISION,  // AY-3-8914 — shuffled register indices
};

// ============================================================================
// AYTraits — compile-time descriptor for each chip variant
// ============================================================================

struct AYTraits {
    const char*    vendor;
    const char*    chip_id;
    uint8_t        io_port_count;    // 0, 1, or 2
    uint8_t        clock_divider;    // 1 (YM — no internal divider) or 2 (AY internal ÷2)
    uint8_t        envelope_steps;   // 16 (AY) or 32 (YM half-step)
    AYRegisterMap  register_map;     // STANDARD or INTELLIVISION
    bool           extended_mode;    // AY8930 enhanced features

    // === Helpers ===
    constexpr bool has_io_port_a() const { return io_port_count >= 1; }
    constexpr bool has_io_port_b() const { return io_port_count >= 2; }
    constexpr bool has_half_step_envelope() const { return envelope_steps == 32; }
    constexpr bool has_register_remap() const {
        return register_map == AYRegisterMap::INTELLIVISION;
    }
    constexpr bool has_extended_mode() const { return extended_mode; }
    constexpr uint8_t envelope_max() const { return envelope_steps - 1; }
};

// ============================================================================
// Concrete trait instances — one per chip in the family
// ============================================================================

// --- Standard General Instrument parts ---

inline constexpr AYTraits AY_3_8910_Traits = {
    "General Instrument", "AY-3-8910",
    2, 2, 16, AYRegisterMap::STANDARD, false
};

inline constexpr AYTraits AY_3_8912_Traits = {
    "General Instrument", "AY-3-8912",
    1, 2, 16, AYRegisterMap::STANDARD, false
};

inline constexpr AYTraits AY_3_8913_Traits = {
    "General Instrument", "AY-3-8913",
    0, 2, 16, AYRegisterMap::STANDARD, false
};

inline constexpr AYTraits AY_3_8914_Traits = {
    "General Instrument", "AY-3-8914",
    2, 2, 16, AYRegisterMap::INTELLIVISION, false
};

// --- Yamaha clones ---

inline constexpr AYTraits YM2149_Traits = {
    "Yamaha", "YM2149",
    2, 1, 32, AYRegisterMap::STANDARD, false
};

inline constexpr AYTraits YM3439_Traits = {
    "Yamaha", "YM3439",
    2, 1, 32, AYRegisterMap::STANDARD, false
};

// --- Enhanced clone ---

inline constexpr AYTraits AY8930_Traits = {
    "Microchip", "AY8930",
    2, 2, 16, AYRegisterMap::STANDARD, true
};
