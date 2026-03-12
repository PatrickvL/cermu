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

#include "../../core/system.h"
#include "../../core/chip_manifest.hpp"
#include "../../chip/cpu/fam65xx/mos6507.h"
#include "../../chip/video/tia/tia.h"
#include "../../chip/io/pia6532.h"
#include "mappers/a2600_mapper.h"
#include "atari2600_constants.h"
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
// After apply():
//   - Page 0 gets a MaskedSubTable with two regions (TIA + RIOT)
//   - Pages 1-15 are manually mirrored to the same sub-table
//   - Page 16 gets full-page MMIO for cartridge
//   - Pages 17-31 are manually mirrored to the same cart MMIO handler
//
inline constexpr auto kAtari2600Chips = make_chip_manifest(
    Slot<tia_t>             {0x0000, 0, 0x0080},
    Slot<pia6532_t>         {0x0080, 0, 0x0080},
    Slot<Atari2600CartChip> {0x1000, 0}
);

// BusSpec auto-derived from the manifest (13-bit address, 256-byte pages)
using Atari2600BusSpec = ManifestBusSpec<kAtari2600Chips, 13, 8>;

namespace atari2600_chips {
    inline constexpr size_t kTiaSlot  = 0;
    inline constexpr size_t kRiotSlot = 1;
    inline constexpr size_t kCartSlot = 2;
}

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

    // Display
    uint32_t* get_framebuffer() override;
    void get_display_dimensions(int* width, int* height) const override;
    void set_framebuffer(uint32_t* buffer, int width, int height) override;

    // Audio
    uint32_t get_audio_samples(float* buffer, uint32_t max_samples) override;
    void set_audio_sample_rate(int sample_rate_hz) override;

    // Input
    void handle_keyboard_event(SDL_Keycode key, bool pressed) override;

    // GUI integration
    void render_system_menu_items() override;
    void render_configuration_ui() override;

    // Emulation control
    void set_speed_multiplier(float multiplier) override;

    // System ready state
    bool is_system_ready() const override { return system_ready_; }

private:
    // ========================================================================
    // CHIPS — owned by bus_mem_, borrowed here for direct access
    // ========================================================================

    MOS6507*           cpu_       = nullptr;  // MOS 6507 CPU (6502, 13-bit address bus)
    tia_t*             tia_       = nullptr;  // TIA — Television Interface Adapter
    pia6532_t*         riot_      = nullptr;  // PIA 6532 RIOT — RAM, I/O, Timer
    Atari2600CartChip* cart_chip_ = nullptr;  // Cart MMIO adapter (wraps mapper)

    // ========================================================================
    // CARTRIDGE ROM
    // ========================================================================

    std::vector<uint8_t> cart_rom_;         // Cartridge ROM data
    uint32_t cart_size_ = 0;                // Actual ROM size in bytes
    std::unique_ptr<A2600Mapper> mapper_;   // Bank-switching mapper
    bool mapper_snoop_ = false;             // Cached: mapper needs bus_snoop() calls

    // ========================================================================
    // MEMORY BUS — declarative setup via chip manifest + BusMemory::apply()
    // ========================================================================

    using Bus = MemoryBus<Atari2600BusSpec>;
    using Mem = BusMemory<Atari2600BusSpec>;

    Bus bus_;
    Mem bus_mem_{kAtari2600Chips};

    // ========================================================================
    // SYSTEM STATE
    // ========================================================================

    bus_state_t pins_;                   // CPU bus state (persisted across ticks)
    bool system_ready_ = false;          // True when cartridge is loaded
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
    void setup_connector_ports();
    std::vector<DefaultPeripheral> get_default_peripherals() const override;

    // Read joystick signals from connector ports into RIOT/TIA
    void update_joystick_state();


};
