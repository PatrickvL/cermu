#pragma once
/*
 * atari_vector_system.hpp — Atari early vector arcade system (1979)
 *
 * Shared system class for Asteroids and Lunar Lander — both use the
 * same board architecture:
 *   - MOS 6502 CPU @ 1.512 MHz (12.096 MHz master / 8)
 *   - DVG (Digital Vector Generator) — XY vector display
 *   - 1 KB work RAM ($0000-$03FF)
 *   - 2 KB vector RAM ($4000-$47FF)
 *   - 2 KB vector ROM ($5000-$57FF)
 *   - Program ROM at $6000+ (size varies by game)
 *   - I/O decode at $2000-$3FFF (reads=inputs, writes=outputs)
 *   - NMI driven by 250 Hz timer (3 KHz clock / 12)
 *   - Discrete sound (no sound chip — modeled as CPU-driven DAC samples)
 *
 * Template parameter V selects the game variant:
 *   AtariVectorVariant::ASTEROIDS     — Asteroids (1979)
 *   AtariVectorVariant::LUNAR_LANDER  — Lunar Lander (1979)
 *
 * Differences between variants:
 *   - Program ROM base/size
 *   - I/O read map (Asteroids: 3 input ports; LL: 2 + thrust ADC)
 *   - Sound output latches (discrete circuits differ)
 *   - Default DIP switch settings
 *   - Display phosphor color (Asteroids: green; LL: white/blue)
 */

#include "systems/arcade/atari_vector/atari_vector_constants.hpp"
#include "core/system.hpp"
#include "core/board.hpp"
#include "core/signal/video_port.hpp"
#include "core/signal/audio_port.hpp"
#include "chip/cpu/fam65xx/mos6502.hpp"
#include "chip/video/dvg/dvg.hpp"
#include "chip/memory/ram_chip.hpp"
#include "chip/memory/rom_chip.hpp"
#include <cstdint>
#include <memory>
#include <vector>

namespace atv = atari_vector_constants;

// ============================================================================
// Variant traits — compile-time differences between Asteroids and LL
// ============================================================================

template<AtariVectorVariant V>
struct AtariVectorTraits;

template<>
struct AtariVectorTraits<AtariVectorVariant::ASTEROIDS> {
    static constexpr const char* NAME         = "Asteroids";
    static constexpr const char* SHORT_NAME   = "Asteroids";
    static constexpr const char* DESCRIPTION  = "Atari Asteroids (1979) — 6502 CPU, DVG vector display";
    static constexpr const char* DATA_FOLDER  = "asteroids";

    static constexpr uint16_t PROGROM_BASE    = atv::AST_PROGROM_BASE;   // $6000
    static constexpr uint16_t PROGROM_SIZE    = atv::AST_PROGROM_SIZE;   // 8 KB (manifest-aligned)
    static constexpr uint16_t PROGROM_ACTUAL  = atv::AST_PROGROM_ACTUAL; // 6 KB actual
    static constexpr uint16_t PROGROM_OFFSET  = atv::AST_PROGROM_OFFSET; // ROM starts at offset $800

    static constexpr const char* PALETTE_ID   = "green";   // Green phosphor CRT
};

template<>
struct AtariVectorTraits<AtariVectorVariant::LUNAR_LANDER> {
    static constexpr const char* NAME         = "Lunar Lander";
    static constexpr const char* SHORT_NAME   = "LunarLander";
    static constexpr const char* DESCRIPTION  = "Atari Lunar Lander (1979) — 6502 CPU, DVG vector display";
    static constexpr const char* DATA_FOLDER  = "lunar_lander";

    static constexpr uint16_t PROGROM_BASE    = atv::LL_PROGROM_BASE;    // $6000
    static constexpr uint16_t PROGROM_SIZE    = atv::LL_PROGROM_SIZE;    // 8 KB
    static constexpr uint16_t PROGROM_ACTUAL  = atv::LL_PROGROM_SIZE;    // Full 8 KB
    static constexpr uint16_t PROGROM_OFFSET  = 0;                       // No offset

    static constexpr const char* PALETTE_ID   = "white";   // White/blue phosphor CRT
};


// ============================================================================
// Chip manifest — parameterized by variant
// ============================================================================
//
// 15-bit address space ($0000-$7FFF), 256-byte pages.
//
// Common layout:
//   Slot 0: Work RAM       — 1 KB at $0000
//   Slot 1: Vector RAM     — 2 KB at $4000
//   Slot 2: Vector ROM     — 2 KB at $5000
//   Slot 3: Program ROM    — variant-specific base and size
//   Slot 4: MOS 6502 CPU   — non-bus (factory-created)
//
// I/O ($2000-$3FFF) is handled by manual dispatch in tick (not MMIO),
// because reads vs writes have entirely different decode logic and the
// address lines used for selection don't map cleanly to a contiguous
// MMIO region.

inline constexpr auto kAsteroidsChips = make_chip_manifest(
    Slot<RAMChip>{atv::RAM_BASE,     atv::RAM_SIZE,     0, "Work RAM"},
    Slot<RAMChip>{atv::VECRAM_BASE,  atv::VECRAM_SIZE,  0, "Vector RAM"},
    Slot<ROMChip>{atv::VECROM_BASE,  atv::VECROM_SIZE,  0, "Vector ROM"},
    Slot<ROMChip>{atv::AST_PROGROM_BASE, atv::AST_PROGROM_SIZE, 0, "Program ROM"},
    Slot<MOS6502>{0, 0, 0, "MOS 6502"}
);

inline constexpr auto kLunarLanderChips = make_chip_manifest(
    Slot<RAMChip>{atv::RAM_BASE,     atv::RAM_SIZE,     0, "Work RAM"},
    Slot<RAMChip>{atv::VECRAM_BASE,  atv::VECRAM_SIZE,  0, "Vector RAM"},
    Slot<ROMChip>{atv::VECROM_BASE,  atv::VECROM_SIZE,  0, "Vector ROM"},
    Slot<ROMChip>{atv::LL_PROGROM_BASE, atv::LL_PROGROM_SIZE, 0, "Program ROM"},
    Slot<MOS6502>{0, 0, 0, "MOS 6502"}
);

// ============================================================================
// Manifest selector — pick the right manifest at compile time
// ============================================================================

template<AtariVectorVariant V>
constexpr const auto& select_manifest() {
    if constexpr (V == AtariVectorVariant::ASTEROIDS)
        return kAsteroidsChips;
    else
        return kLunarLanderChips;
}

// ============================================================================
// BusSpec for each variant
// ============================================================================

using AsteroidsBusSpec     = ManifestBusSpec<kAsteroidsChips, 16, 8>;
using LunarLanderBusSpec   = ManifestBusSpec<kLunarLanderChips, 16, 8>;

template<AtariVectorVariant V>
using VectorBusSpec = std::conditional_t<
    V == AtariVectorVariant::ASTEROIDS,
    AsteroidsBusSpec,
    LunarLanderBusSpec
>;


// ============================================================================
// AtariVectorSystem — shared system implementation
// ============================================================================

template<AtariVectorVariant V>
class AtariVectorSystem : public System {
    using Traits = AtariVectorTraits<V>;
    using Spec   = VectorBusSpec<V>;
    using Bus    = MemoryBus<Spec>;
    using MainBoard = Board<Spec>;

public:
    AtariVectorSystem();
    ~AtariVectorSystem() override;

    // ── System interface ─────────────────────────────────────────
    const SystemDescriptor& get_descriptor() const override;
    bool set_configuration(const SystemConfiguration& config) override;
    bool apply_configuration() override;

    bool initialize() override;
    void shutdown() override;
    void reset() override;

    void tick() override;
    void run_frame() override;

    bool load_file(const char* filepath) override;
    void get_display_dimensions(int* width, int* height) const override;

    uint32_t get_audio_samples(float* buffer, uint32_t max_samples) override;
    void set_audio_sample_rate(int sample_rate_hz) override;

    void handle_keyboard_event(SDL_Keycode key, bool pressed) override;
    void render_system_menu_items() override;
    void render_configuration_ui() override;
    VideoSignalType get_video_signal_type() const override { return VideoSignalType::Vector; }
    void set_speed_multiplier(float multiplier) override;

    // ROM set loading
    std::vector<const RomSetDescriptor*> get_rom_set_descriptors() const override;
    bool load_rom_set(const RomSetMatch& match) override;

    bool is_system_ready() const override { return system_ready_; }

private:
    // ── Chips ────────────────────────────────────────────────────────────
    MOS6502*    cpu_   = nullptr;    // MOS 6502 @ 1.512 MHz — owned by board_
    dvg_t       dvg_;                // Digital Vector Generator

    // Memory chips (non-owning; owned by board_)
    RAMChip*    vec_ram_  = nullptr; // Vector RAM ($4000-$47FF)
    ROMChip*    vec_rom_  = nullptr; // Vector ROM ($5000-$57FF)
    ROMChip*    prog_rom_ = nullptr; // Program ROM

    // ── Memory bus ───────────────────────────────────────────────────────
    Bus bus_;
    MainBoard board_{select_manifest<V>()};

    // ── Display ──────────────────────────────────────────────────────────
    std::unique_ptr<VectorVideoPort> video_port_;

    // ── Audio ────────────────────────────────────────────────────────────
    std::unique_ptr<AudioPort> audio_port_;
    int audio_sample_rate_ = atv::DEFAULT_SAMPLE_RATE;

    // ── System state ─────────────────────────────────────────────────────
    bus_state_t pins_ = MOS6502::default_bus_state();
    bool system_ready_    = false;
    uint32_t nmi_counter_ = 0;      // Cycles until next NMI
    bool nmi_pending_     = false;   // NMI flip-flop (cleared by NMI_ACK write)

    // ── Inputs (active-low) ──────────────────────────────────────────────
    uint8_t in0_       = 0xFF;      // Coin/system inputs
    uint8_t in1_       = 0xFF;      // Player 1 inputs
    uint8_t in2_       = 0xFF;      // Player 2 inputs (Asteroids only)
    uint8_t dsw1_      = 0x00;      // DIP switch bank 1
    uint8_t dsw2_      = 0x00;      // DIP switch bank 2
    uint8_t thrust_    = 0x00;      // Thrust lever ADC (Lunar Lander only, 0-255)

    // ── Sound output latches ─────────────────────────────────────────────
    uint8_t snd_latch_ = 0x00;      // Sound triggers / output bits

    // ── Internal helpers ─────────────────────────────────────────────────
    void tick_cpu();
    bus_state_t io_read(uint16_t addr, bus_state_t pins);
    bus_state_t io_write(uint16_t addr, uint8_t data, bus_state_t pins);
};

// ============================================================================
// Concrete type aliases for system registration
// ============================================================================

using AsteroidsSystem     = AtariVectorSystem<AtariVectorVariant::ASTEROIDS>;
using LunarLanderSystem   = AtariVectorSystem<AtariVectorVariant::LUNAR_LANDER>;
