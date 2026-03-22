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
#include "core/core_chips.hpp"
#include "chip/cpu/z80/u880.hpp"
#include "chip/io/z80_pio.hpp"
#include "chip/io/z80_ctc.hpp"
#include "chip/memory/memory_chip.hpp"
#include <cstdint>
#include <vector>

// LC80 bus — Z80 bus defaults
#define LC80_BUS_DEFAULT_STATE (U880::default_bus_state())

// =============================================================================
// LC80 chip manifest — declarative memory layout
// =============================================================================
//
// Slot 0: ROM — 2 KB at $0000 (mirrored 4× through $0000-$1FFF via addr_mask)
// Slot 1: RAM — 1 KB at $2000 (mirrored 8× through $2000-$3FFF via addr_mask)
// Slot 2: CPU — U880 (Z80A clone), non-bus
// Slot 3: PIO #1 — U855, Z80 port-based I/O at $F4-$F7
// Slot 4: PIO #2 — U855, Z80 port-based I/O at $F8-$FB
// Slot 5: CTC   — U857, Z80 port-based I/O at $EC-$EF
//
// All I/O is Z80 port-based (IORQ) — PIO/CTC slots are non-bus (no MMIO).
//
inline constexpr auto kLC80Chips = make_chip_manifest(
    Slot<ROMChip>{0x0000, 8192, 0x07FF, "Monitor ROM"},
    Slot<RAMChip>{0x2000, 8192, 0x03FF, "RAM"},
    // Non-bus chips — factory-created, not address-decoded
    Slot<U880>      {0, 0, 0, "U880"},
    Slot<z80_pio_t> {0, 0, 0, "U855 PIO #1"},
    Slot<z80_pio_t> {0, 0, 0, "U855 PIO #2"},
    Slot<z80_ctc_t> {0, 0, 0, "U857 CTC"}
);

using LC80BusSpec = ManifestBusSpec<kLC80Chips, 16, 8>;

// ── Chips ──────────────────────────────────────────────────────────────
struct LC80Chipset : CoreChips<U880, NoChip, NoChip, z80_pio_t> {
    z80_pio_t pio2;     // U855 PIO #2 (keyboard scan + cassette)
    z80_ctc_t ctc;      // U857 CTC (speaker on channel 2)

    template<typename BoardT>
    void bind_extras(BoardT& board) {
        board.bind_chip(board.template find_index<z80_pio_t>(1), &pio2);
        board.bind_chip(board.template find_index<z80_ctc_t>(), &ctc);
    }

    template<typename BoardT>
    void register_extras(BoardT& board) {
        board.register_component(&pio2);
        board.register_component(&ctc);
    }
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
