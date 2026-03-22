#pragma once
/*
 * z1013_system.h — Robotron Z1013 system declaration
 *
 * Simple DDR home computer with character display and membrane keyboard.
 * Three known variants: Z1013.01 (original), Z1013.12, Z1013.64 (64KB).
 */


#include "systems/ddr/z1013/z1013_constants.hpp"
#include "core/system.hpp"
#include "core/system_lines.hpp"
#include "core/board.hpp"
#include "core/standard_chips.hpp"
#include "core/signal/video_port.hpp"

#include "chip/cpu/z80/u880.hpp"
#include "chip/cpu/z80/z80.hpp"   // Z80_MREQ_BIT / Z80_IORQ_BIT
#include "chip/io/z80_pio.hpp"
#include "chip/memory/memory_chip.hpp"
#include "chip/video/char_display/char_display.hpp"
#include <cstdint>
#include <vector>

#define Z1013_BUS_DEFAULT_STATE (U880::default_bus_state())

// ── Variants ─────────────────────────────────────────────────────────────
enum class Z1013Variant { Z1013_01, Z1013_16, Z1013_64 };

template<Z1013Variant V> struct Z1013VariantTraits;

template<> struct Z1013VariantTraits<Z1013Variant::Z1013_01> {
    static constexpr const char* name            = "Robotron Z1013.01";
    static constexpr const char* short_name      = "Z1013.01";
    static constexpr const char* description     = "Robotron Z1013.01 — U880 @ 2MHz, 16KB RAM, 32×32 text (1985)";
    static constexpr uint32_t    ram_size        = z1013_constants::RAM_SIZE_16K;
    static constexpr bool        has_basic_rom   = false;
};

template<> struct Z1013VariantTraits<Z1013Variant::Z1013_16> {
    static constexpr const char* name            = "Robotron Z1013.16";
    static constexpr const char* short_name      = "Z1013.16";
    static constexpr const char* description     = "Robotron Z1013.16 — U880 @ 2MHz, 16KB RAM, membrane keyboard (1987)";
    static constexpr uint32_t    ram_size        = z1013_constants::RAM_SIZE_16K;
    static constexpr bool        has_basic_rom   = false;
};

template<> struct Z1013VariantTraits<Z1013Variant::Z1013_64> {
    static constexpr const char* name            = "Robotron Z1013.64";
    static constexpr const char* short_name      = "Z1013.64";
    static constexpr const char* description     = "Robotron Z1013.64 — U880 @ 2MHz, 64KB RAM, ROM BASIC (1988)";
    static constexpr uint32_t    ram_size        = z1013_constants::RAM_SIZE_64K;
    static constexpr bool        has_basic_rom   = true;
};

// ============================================================================
// Z1013 chip manifests — declarative memory layout
// ============================================================================
//
// Z1013.01 / Z1013.16 (16 KB RAM):
//   Slot 0: RAM          — 16 KB at $0000
//   Slot 1: Video RAM    —  1 KB at $EC00
//   Slot 2: Monitor ROM  —  2 KB at $F000
//
// Z1013.64 (64 KB RAM, ROM BASIC):
//   Slot 0: RAM          — 64 KB at $0000
//   Slot 1: BASIC ROM lo —  8 KB at $C000  (first 8 KB of 10 KB BASIC ROM)
//   Slot 2: BASIC ROM hi —  2 KB at $E000  (last 2 KB of 10 KB BASIC ROM)
//   Slot 3: Video RAM    —  1 KB at $EC00
//   Slot 4: Monitor ROM  —  2 KB at $F000
//
// The character ROM is NOT bus-mapped (used only for display rendering).
// All I/O is Z80 port-based (IORQ) — no MMIO slots needed.
//
// BASIC ROM is split into 8 KB + 2 KB because ChipSlot requires power-of-2
// sizes and the original 10 KB is not a power of 2.  The ordering ensures
// apply() maps them correctly: RAM first (base layer), BASIC ROM overlays
// RAM reads, then Video RAM and Monitor ROM overlay the remaining gaps.
//
inline constexpr auto kZ1013_16K_Chips = make_chip_manifest(
    Slot<RAMChip>{0x0000, 16384, 0, "RAM"},
    Slot<RAMChip>{0xEC00,  1024, 0, "Video RAM"},
    Slot<ROMChip>{0xF000,  2048, 0, "Monitor ROM"}.with_rom("z1013_mon.rom|monitor.rom|MON.ROM"),
    // Non-bus chips — factory-created, not address-decoded
    Slot<U880>      {0, 0, 0, "U880"},
    Slot<z80_pio_t> {0, 0, 0, "U855 PIO"}
);

inline constexpr auto kZ1013_64K_Chips = make_chip_manifest(
    Slot<RAMChip>{0x0000, 65536, 0, "RAM"},
    Slot<ROMChip>{0xC000,  8192, 0, "BASIC ROM lo"},
    Slot<ROMChip>{0xE000,  2048, 0, "BASIC ROM hi"},
    Slot<RAMChip>{0xEC00,  1024, 0, "Video RAM"},
    Slot<ROMChip>{0xF000,  2048, 0, "Monitor ROM"}.with_rom("z1013_mon.rom|monitor.rom|MON.ROM"),
    // Non-bus chips — factory-created, not address-decoded
    Slot<U880>      {0, 0, 0, "U880"},
    Slot<z80_pio_t> {0, 0, 0, "U855 PIO"}
);

// BusTraits — selects the correct manifest per variant
template<Z1013Variant V> struct Z1013BusTraits;

template<> struct Z1013BusTraits<Z1013Variant::Z1013_01> {
    static constexpr const auto& kManifest = kZ1013_16K_Chips;
    using Spec = ManifestBusSpec<kZ1013_16K_Chips, 16, 8>;
};

template<> struct Z1013BusTraits<Z1013Variant::Z1013_16> {
    static constexpr const auto& kManifest = kZ1013_16K_Chips;
    using Spec = ManifestBusSpec<kZ1013_16K_Chips, 16, 8>;
};

template<> struct Z1013BusTraits<Z1013Variant::Z1013_64> {
    static constexpr const auto& kManifest = kZ1013_64K_Chips;
    using Spec = ManifestBusSpec<kZ1013_64K_Chips, 16, 8>;
};

// ── ChipSet ──────────────────────────────────────────────────────────────
struct Z1013ChipSet : CommonBoardChips<U880, NoChip, NoChip, z80_pio_t> {};

// ── System ───────────────────────────────────────────────────────────────
template<Z1013Variant V>
class Z1013System : public System {
    using Traits = Z1013VariantTraits<V>;
public:
    Z1013System();
    ~Z1013System() override;

    const SystemDescriptor& get_descriptor() const override;
    bool set_configuration(const SystemConfiguration& config) override;
    bool apply_configuration() override;

    bool initialize() override;
    void shutdown() override;
    void reset() override;

    void tick() override;
    void run_frame() override;

    void handle_keyboard_event(SDL_Keycode key, bool pressed) override;


private:
    // ── Chips (value-typed via Board ChipSet) ────────────────────────────

    // ── Memory — chip pointers for post-init access (owned by Board) ─
    ROMChip* basic_rom_lo_chip_    = nullptr;  // Z1013.64 only
    ROMChip* basic_rom_hi_chip_    = nullptr;  // Z1013.64 only
    RAMChip* video_ram_chip_       = nullptr;
    ROMChip* monitor_rom_chip_     = nullptr;

    // Character ROM — NOT bus-mapped (used for display rendering only)
    std::vector<uint8_t> char_rom_;

    // ── MemoryBus — declarative setup via chip manifest ──────────────────
    using BT  = Z1013BusTraits<V>;
    using Bus = MemoryBus<typename BT::Spec>;
    using PT  = PackingTraits<typename BT::Spec>;
    using MainBoard = Board<typename BT::Spec, Z1013ChipSet>;
    Bus bus_;
    MainBoard board_{BT::kManifest};

    // ── Display ──────────────────────────────────────────────────────────
    std::unique_ptr<CompositeVideoPort> video_port_;  // Video stream output
    CharDisplayGenerator video_gen_;                  // TTL character display generator

    // ── Keyboard ─────────────────────────────────────────────────────────
    uint8_t keyboard_matrix_[z1013_constants::KEYBOARD_ROWS] = {};
    uint8_t keyboard_column_select_ = 0xFF;

    // ── System state ─────────────────────────────────────────────────────
    bus_state_t pins_       = Z1013_BUS_DEFAULT_STATE;
    int audio_sample_rate_  = z1013_constants::DEFAULT_SAMPLE_RATE;

    // ── Internal helpers ─────────────────────────────────────────────────
    bus_state_t io_tick(bus_state_t pins);
    void        render_frame();   // Render one complete video frame to framebuffer_
    bool        load_roms();
};
