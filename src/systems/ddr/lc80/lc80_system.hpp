#pragma once
/*
 * lc80_system.h — LC 80 learning computer system declaration
 *
 * The LC 80 has no video output; it uses multiplexed 7-segment LEDs.
 * We render the LED display into a small framebuffer for the GUI.
 */


#include "systems/ddr/lc80/lc80_constants.hpp"
#include "core/system.hpp"
#include "core/system_lines.hpp"
#include "core/board.hpp"
#include "core/system_chip_visitors.hpp"
#include "chip/cpu/z80/u880.hpp"
#include "chip/io/z80_pio.hpp"
#include "chip/io/z80_ctc.hpp"
#include "chip/memory/memory_chip.hpp"
#include <cstdint>
#include <vector>

// LC80 bus — Z80 bus defaults
#define LC80_BUS_DEFAULT_STATE (U880::default_bus_state())

// =============================================================================
// LC80 chip declaration — single source of truth
// =============================================================================
//
// Row: X(ctx, type, chip, base, size, mask, overlay, label, rom_files)
//
// All I/O is Z80 port-based (IORQ) — PIO/CTC slots are non-bus (no MMIO).
// ROM and RAM use addr_mask for hardware mirroring:
//   ROM: 2 KB physical, mirrored 4× through $0000-$1FFF (mask 0x07FF)
//   RAM: 1 KB physical, mirrored 8× through $2000-$3FFF (mask 0x03FF)
//

#define LC80_FOR_EACH_SYSTEM_CHIP(X, ctx)                                                              \
    X(ctx, U880,       z80,  0x0000,    0,      0, 0, "U880",        nullptr)                          \
    X(ctx, ROMChip,    rom,  0x0000, 8192, 0x07FF, 0, "Monitor ROM", nullptr)                          \
    X(ctx, RAMChip,    ram,  0x2000, 8192, 0x03FF, 0, "RAM",         nullptr)                          \
    X(ctx, z80_pio_t,  pio,  0x00F4,    0, 0x00FC, 0, "U855 PIO #1", nullptr)                          \
    X(ctx, z80_pio_t,  pio2, 0x00F8,    0, 0x00FC, 0, "U855 PIO #2", nullptr)                          \
    X(ctx, z80_ctc_t,  ctc,  0x00EC,    0, 0x00FC, 0, "U857 CTC",    nullptr)

static constexpr size_t kLC80ChipCount = 0 LC80_FOR_EACH_SYSTEM_CHIP(CERMU_CHIP_VISITOR_COUNT_ONE, unused);

inline constexpr ChipManifest<kLC80ChipCount> kLC80Chips = ChipManifest<kLC80ChipCount>{{
    LC80_FOR_EACH_SYSTEM_CHIP(CERMU_CHIP_VISITOR_MANIFEST_ROW, unused)
}};

using LC80BusSpec = ManifestBusSpec<kLC80Chips, 16, 8>;

// ── Chips ──────────────────────────────────────────────────────────────
struct LC80Chipset {
    LC80_FOR_EACH_SYSTEM_CHIP(CERMU_CHIP_VISITOR_DECLARE_FIELD, unused)
};

class LC80System : public System {
public:
    LC80System();
    ~LC80System() override;

    // ── System interface ─────────────────────────────────────────
    const SystemDescriptor& get_descriptor() const override;
    bool set_configuration(const SystemConfiguration& config) override;
    bool apply_configuration() override;

    bool initialize() override;
    void shutdown() override;
    void reset() override;

    void tick() override;
    void run_frame() override;


private:
    // ── Chips (value-typed via Board Chips) ────────────────────────────

    // ── MemoryBus — declarative setup via chip manifest ──────────────────
    using Bus = MemoryBus<LC80BusSpec>;
    using PT  = PackingTraits<LC80BusSpec>;
    using MainBoard = Board<LC80BusSpec, LC80Chipset>;
    Bus bus_;
    MainBoard board_{kLC80Chips};

    // ── LED display ──────────────────────────────────────────────────────
    // Segment data for each of the 6 digits (bit 0..6 = a..g, bit 7 = dp)
    uint8_t led_segments_[lc80_constants::LED_DIGIT_COUNT] = {};

    // Render the LED display into a small framebuffer for visualization
    static constexpr int FB_WIDTH  = 192;   // 6 digits x 32 px each
    static constexpr int FB_HEIGHT = 48;
    uint32_t framebuffer_[FB_WIDTH * FB_HEIGHT] = {};

    // ── Keyboard ─────────────────────────────────────────────────────────
    uint32_t key_state_ = 0;        // Bit field for 25 keys

    // ── Speaker ──────────────────────────────────────────────────────────
    bool speaker_state_ = false;

    // ── System state ─────────────────────────────────────────────────────
    bus_state_t pins_       = LC80_BUS_DEFAULT_STATE;
    int audio_sample_rate_  = lc80_constants::DEFAULT_SAMPLE_RATE;

    // ── Internal helpers ─────────────────────────────────────────────────
    void configure_bus_memory_map();
    bus_state_t io_tick(bus_state_t pins);
    bool load_roms();
};
