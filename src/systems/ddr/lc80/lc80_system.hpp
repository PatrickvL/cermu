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

// =============================================================================
// LC80 chip manifest — declarative memory layout
// =============================================================================
//
// Slot 0: ROM — 2 KB at $0000 (monitor, mirrored through $0000-$1FFF)
// Slot 1: RAM — 1 KB at $2000 (mirrored through $2000-$3FFF)
//
// All I/O is Z80 port-based (IORQ) — no MMIO slots needed.
// Addresses above $3FFF are unmapped (reads return bus default).
//
inline constexpr auto kLC80Chips = make_chip_manifest(
    Slot<ROMChip>{0x0000, 2048, 0, "Monitor ROM"},
    Slot<RAMChip>{0x2000, 1024, 0, "RAM"},
    // Non-bus chip — factory-created, not address-decoded
    Slot<U880>   {0, 0, 0, "U880"}
);

namespace lc80_chips {
    inline constexpr size_t kRomSlot = 0;
    inline constexpr size_t kRamSlot = 1;
    inline constexpr size_t kCpuSlot = 2;
}

using LC80BusSpec = ManifestBusSpec<kLC80Chips, 16, 8>;

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

    bool load_file(const char* filepath) override;

    uint32_t* get_framebuffer() override;
    void get_display_dimensions(int* width, int* height) const override;
    void set_framebuffer(uint32_t* buffer, int width, int height) override;

    uint32_t get_audio_samples(float* buffer, uint32_t max_samples) override;
    void set_audio_sample_rate(int sample_rate_hz) override;

    void handle_keyboard_event(SDL_Keycode key, bool pressed) override;
    void render_system_menu_items() override;
    void render_configuration_ui() override;
    void set_speed_multiplier(float multiplier) override;

    bool is_system_ready() const override { return system_ready_; }

private:
    // ── Chips ────────────────────────────────────────────────────────────
    U880*       cpu_  = nullptr;     // U880 (Z80A clone) — owned by board_
    z80_pio_t   pio1_;               // U855 PIO #1 (LED display + keyboard)
    z80_pio_t   pio2_;               // U855 PIO #2 (keyboard scan + cassette)
    z80_ctc_t   ctc_;                // U857 CTC (speaker on channel 2)

    // ── MemoryBus — declarative setup via chip manifest ──────────────────
    using Bus = MemoryBus<LC80BusSpec>;
    using PT  = PackingTraits<LC80BusSpec>;
    using MainBoard = Board<LC80BusSpec>;
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
    bool        system_ready_ = false;
    uint64_t total_cycles_  = 0;
    int audio_sample_rate_  = lc80_constants::DEFAULT_SAMPLE_RATE;
    float speed_multiplier_ = 1.0f;

    // ── Internal helpers ─────────────────────────────────────────────────
    void configure_bus_memory_map();
    bus_state_t io_tick(bus_state_t pins);
    bool load_roms();
};
