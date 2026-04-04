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
#include "chip/cpu/z80/u880.hpp"
#include "chip/io/z80_pio.hpp"
#include "chip/io/z80_ctc.hpp"
#include "chip/memory/memory_chip.hpp"
#include <cstdint>
#include <vector>

// LC80 bus — Z80 bus defaults
#define LC80_BUS_DEFAULT_STATE (U880::default_bus_state())

inline constexpr auto kLC80Manifest = make_manifest(
    // Chips
    Slot<U880>{.base_addr = 0x0000, .label = "U880"},
    Slot<ROMChip>{.base_addr = 0x0000, .size_bytes = 0x2000, .addr_mask = 0x07FF, .label = "Monitor ROM", .rom = {"lc80_mon.rom|monitor.rom|MON.ROM"}},
    Slot<RAMChip>{.base_addr = 0x2000, .size_bytes = 0x2000, .addr_mask = 0x03FF, .label = "RAM"},
    Slot<z80_pio_t>{.base_addr = 0x00F4, .addr_mask = 0x00FC, .label = "U855 PIO #1"},
    Slot<z80_pio_t>{.base_addr = 0x00F8, .addr_mask = 0x00FC, .label = "U855 PIO #2"},
    Slot<z80_ctc_t>{.base_addr = 0x00EC, .addr_mask = 0x00FC, .label = "U857 CTC"},
    // Ports
    Slot<PortCassette>{.name = "Cassette Port"}
);

inline constexpr size_t kLC80ChipCount = decltype(kLC80Manifest)::chip_count;
using LC80BusSpec = ManifestBusSpec<kLC80Manifest, 16, 8>;

struct LC80Board : Board<LC80BusSpec> {
    using ComponentTuple = decltype(kLC80Manifest)::component_tuple;
    ComponentTuple components_;

    // Chip aliases
    U880&      z80  = std::get<0>(components_);
    ROMChip&   rom  = std::get<1>(components_);
    RAMChip&   ram  = std::get<2>(components_);
    z80_pio_t& pio  = std::get<3>(components_);
    z80_pio_t& pio2 = std::get<4>(components_);
    z80_ctc_t& ctc  = std::get<5>(components_);

    // Port aliases
    PortCassette& cassette_port = std::get<6>(components_);

    LC80Board() : Board(kLC80Manifest) {}
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
    using MainBoard = LC80Board;
    Bus bus_;
    MainBoard board_;

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
