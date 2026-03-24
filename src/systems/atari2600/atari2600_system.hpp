#pragma once
/*
 * atari2600_system.h — Atari 2600 (VCS) Emulated System
 *
 * The Atari 2600 (1977) was the first widely successful home video game
 * console. It features:
 *   - MOS 6507 CPU (6502 core, 13-bit address bus, no IRQ pin)
 *   - TIA chip (video + audio generation, collision detection)
 *   - PIA 6532 RIOT (128 bytes RAM, I/O ports, timer)
 *   - ROM cartridge slot (2KB–32KB+ with bank switching)
 *   - Two DB-9 joystick ports (active-low, same pinout as Commodore)
 *   - Console switches: Reset, Select, Color/B&W, Difficulty A/B
 *
 * Clock: TIA runs at 3.579545 MHz (NTSC); CPU divides by 3 ≈ 1.19 MHz.
 * Display: 160×~192 visible pixels, 128-color NTSC palette.
 */

#include "core/system.hpp"
#include "core/board.hpp"
#include "core/core_chips.hpp"
#include "core/signal/video_port.hpp"
#include "core/signal/audio_port.hpp"
#include "chip/cpu/fam65xx/mos6507.hpp"
#include "chip/video/tia/tia.hpp"
#include "chip/io/pia6532.hpp"
#include "systems/atari2600/mappers/a2600_mapper.hpp"
#include "systems/atari2600/atari2600_constants.hpp"
#include <cstdint>
#include <memory>
#include <vector>

// Atari 2600 default bus state — derived from MOS6507 CPU.
#define ATARI2600_BUS_DEFAULT_STATE (MOS6507::default_bus_state())


// =============================================================================
// Cartridge MMIO adapter — wraps the runtime-polymorphic A2600Mapper as a
// ChipBase so it can participate in MemoryBus MMIO dispatch for the $1000-$1FFF
// cartridge address window.  The actual mapper is set after load_file().
// =============================================================================

class Atari2600CartChip : public ChipBase {
public:
    Atari2600CartChip() : ChipBase(ChipInfo{"Cartridge", "Various"}) {}

    void set_mapper(A2600Mapper* m) { mapper_ = m; }

    bool has_mmio() const override { return true; }
    bus_state_t on_bus_read(bus_state_t bus) noexcept override {
        if (mapper_)
            BUS_SET_DATA(bus, mapper_->read(BUS_GET_ADDR(bus) & 0x0FFF));
        return bus;
    }
    bus_state_t on_bus_write(bus_state_t bus) noexcept override {
        if (mapper_)
            mapper_->write(BUS_GET_ADDR(bus) & 0x0FFF, BUS_GET_DATA(bus));
        return bus;
    }

private:
    A2600Mapper* mapper_ = nullptr;  // non-owning; system owns the mapper
};


// =============================================================================
// Atari 2600 chip manifest — declarative memory layout
// =============================================================================
//
// 13-bit address bus ($0000-$1FFF), 256-byte pages (32 pages).
// All chips are MMIO-only — no buffer-backed RAM or ROM in the manifest.
//
// Address decode (from the real hardware):
//   A12=0, A7=0          → TIA registers (mirrors every 128 bytes)
//   A12=0, A7=1          → RIOT (A9 selects RAM vs I/O inside the chip)
//   A12=1                → Cartridge ROM (through bank-switching mapper)
//
// Slot 0: TIA  — MMIO-only, sub-page decode via A7 (addr_mask=0x0080, match A7=0)
// Slot 1: RIOT — MMIO-only, sub-page decode via A7 (addr_mask=0x0080, match A7=1)
// Slot 2: Cart — MMIO-only, full-page, wraps A2600Mapper
//
// apply() auto-wires:
//   - Pages 0-15 get a shared MaskedSubTable (TIA + RIOT), mirrored via
//     bank_size = 4096 (incomplete A12=0 address decode)
//   - Pages 16-31 get full-page MMIO for the cartridge mapper, mirrored
//     via bank_size = 4096 (A12=1 always selects cart)
//
inline constexpr auto kAtari2600Chips = make_chip_manifest(
    Slot<tia_t>             {0x0000, 0, 0x0080, nullptr, 0, 4096},
    Slot<pia6532_t>         {0x0080, 0, 0x0080, nullptr, 0, 4096},
    Slot<Atari2600CartChip> {0x1000, 0, 0, nullptr, 0, 4096},
    // Non-bus chip — factory-created, not address-decoded
    Slot<MOS6507>           {0, 0, 0, "MOS 6507"}
);

// BusSpec auto-derived from the manifest (13-bit address, 256-byte pages)
using Atari2600BusSpec = ManifestBusSpec<kAtari2600Chips, 13, 8>;

// ── Chips ──────────────────────────────────────────────────────────────
struct Atari2600Chipset : CoreChips<MOS6507, tia_t, NoChip, pia6532_t> {
    Atari2600CartChip cart;   // Cart MMIO adapter (wraps mapper)

    template<typename BoardT>
    void bind_extras(BoardT& board) {
        board.bind_chip(board.template find_index<Atari2600CartChip>(), &cart);
    }

    template<typename BoardT>
    void register_extras(BoardT& board) {
        board.register_component(&cart);
    }
};


class Atari2600System : public System {
public:
    Atari2600System();
    ~Atari2600System() override;

    // System identification
    const SystemDescriptor& get_descriptor() const override;

    // Configuration management
    bool set_configuration(const SystemConfiguration& config) override;
    bool apply_configuration() override;

    // System lifecycle
    bool initialize() override;
    void shutdown() override;
    void reset() override;

    // Execution
    void tick() override;
    void run_frame() override;

    // File loading
    bool load_file(const char* filepath) override;

    // Audio
    uint32_t get_audio_samples(float* buffer, uint32_t max_samples) override;
    void set_audio_sample_rate(int sample_rate_hz) override;

    // Input
    void handle_keyboard_event(SDL_Keycode key, bool pressed) override;

    // GUI integration
    void render_system_menu_items() override;
    void render_configuration_ui() override;

    void* get_video_port_ptr() override { return video_port_.get(); }

    // System ready state

private:
    // ========================================================================
    // CHIPS (value-typed via Board Chips)
    // ========================================================================

    std::unique_ptr<CompositeVideoPort> video_port_;  // Video output
    std::unique_ptr<AudioPort> audio_port_;            // Audio port output

    // ========================================================================
    // CARTRIDGE ROM
    // ========================================================================

    std::vector<uint8_t> cart_rom_;         // Cartridge ROM data
    uint32_t cart_size_ = 0;                // Actual ROM size in bytes
    std::unique_ptr<A2600Mapper> mapper_;   // Bank-switching mapper
    bool mapper_snoop_ = false;             // Cached: mapper needs bus_snoop() calls

    // ========================================================================
    // MEMORY BUS — declarative setup via chip manifest + Board::apply()
    // ========================================================================

    using Bus = MemoryBus<Atari2600BusSpec>;
    using MainBoard = Board<Atari2600BusSpec, Atari2600Chipset>;

    Bus bus_;
    MainBoard board_{kAtari2600Chips};

    // ========================================================================
    // SYSTEM STATE
    // ========================================================================

    bus_state_t pins_;                   // CPU bus state (persisted across ticks)
    uint32_t cycles_per_frame_;          // CPU cycles per video frame

    // Console switch state (directly mapped to RIOT Port B)
    uint8_t console_switches_ = 0xFF;   // All switches default high (not pressed)

    // Joystick state (directly mapped to RIOT Port A)
    // Each nibble controls one joystick: P0=upper nibble, P1=lower nibble
    // Bits: 4=P0-UP, 5=P0-DOWN, 6=P0-LEFT, 7=P0-RIGHT (active-low)
    //       0=P1-UP, 1=P1-DOWN, 2=P1-LEFT, 3=P1-RIGHT (active-low)
    uint8_t joystick_state_ = 0xFF;     // All directions released (active-low)

    // ========================================================================
    // FRAME DETECTION
    // ========================================================================

    bool    frame_complete_ = false;
    bool    in_vsync_ = false;           // Tracks VSYNC transitions for frame boundary
    int     vsync_scanline_ = 0;         // Scanline where VSYNC started

    // ========================================================================
    // HELPER METHODS
    // ========================================================================

    void tick_cpu();

    // Configure page tables — mirrors non-cart pages and cart pages
    void configure_bus_memory_map();

    // Connector port setup
    void setup_ports();
    std::vector<DefaultPeripheral> get_default_peripherals() const override;

    // Read joystick signals from connector ports into RIOT/TIA
    void update_joystick_state();


};
