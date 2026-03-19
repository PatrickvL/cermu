#pragma once
/*
 * z9001_system.h — Robotron Z9001 / KC 87 system declaration
 *
 * Two variants:
 *   Z9001  — 16 KB RAM, no color, optional BASIC (1984)
 *   KC 87  — 48 KB RAM, color attribute RAM, built-in BASIC (1987)
 */


#include "systems/ddr/z9001/z9001_constants.hpp"
#include "core/system.hpp"
#include "core/system_lines.hpp"
#include "core/board.hpp"
#include "core/signal/video_port.hpp"

#include "chip/cpu/z80/u880.hpp"
#include "chip/cpu/z80/z80.hpp"   // Z80_MREQ_BIT / Z80_IORQ_BIT
#include "chip/io/z80_pio.hpp"
#include "chip/io/z80_ctc.hpp"
#include "chip/memory/memory_chip.hpp"
#include <cstdint>
#include <vector>

#define Z9001_BUS_DEFAULT_STATE (U880::default_bus_state())

// ── Variants ─────────────────────────────────────────────────────────────
enum class Z9001Variant { Z9001, KC87 };

template<Z9001Variant V> struct Z9001VariantTraits;

template<> struct Z9001VariantTraits<Z9001Variant::Z9001> {
    static constexpr const char* name            = "Robotron Z9001";
    static constexpr const char* short_name      = "Z9001";
    static constexpr const char* description     = "Robotron Z9001 — U880 @ 2.4576MHz, 16KB RAM, 40×24 text (1984)";
    static constexpr uint32_t    ram_size        = z9001_constants::RAM_SIZE_Z9001;
    static constexpr bool        has_color_ram   = false;
    static constexpr bool        has_basic_rom   = false;
};

template<> struct Z9001VariantTraits<Z9001Variant::KC87> {
    static constexpr const char* name            = "Robotron KC 87";
    static constexpr const char* short_name      = "KC87";
    static constexpr const char* description     = "Robotron KC 87 — U880 @ 2.4576MHz, 48KB RAM, color text, BASIC (1987)";
    static constexpr uint32_t    ram_size        = z9001_constants::RAM_SIZE_KC87;
    static constexpr bool        has_color_ram   = true;
    static constexpr bool        has_basic_rom   = true;
};

// ============================================================================
// Z9001 / KC87 chip manifests — declarative memory layout
// ============================================================================
//
// Z9001 (16 KB RAM, no color, no BASIC):
//   Slot 0: RAM          — 16 KB at $0000
//   Slot 1: Video RAM    —  1 KB at $EC00
//   Slot 2: OS ROM       —  4 KB at $F000
//
// KC 87 (48 KB RAM, color RAM, BASIC ROM):
//   Slot 0: RAM          — 64 KB at $0000 (trimmed to 48 KB)
//   Slot 1: BASIC ROM lo —  8 KB at $C000  (first 8 KB of 10 KB BASIC)
//   Slot 2: BASIC ROM hi —  2 KB at $E000  (last 2 KB of 10 KB BASIC)
//   Slot 3: Color RAM    —  1 KB at $E800
//   Slot 4: Video RAM    —  1 KB at $EC00
//   Slot 5: OS ROM       —  4 KB at $F000
//
// The character ROM is NOT bus-mapped (used only for display rendering).
// All I/O is Z80 port-based (IORQ) — no MMIO slots needed.
//
// BASIC ROM is split into 8 KB + 2 KB because ChipSlot requires power-of-2
// sizes and the original 10 KB ($2800) is not a power of 2.
// RAM for KC87 is allocated as 64 KB (power of 2); effective_size in the
// manifest limits Phase 1 to 48 KB ($C000), leaving $C000+ for ROM/I/O.
//
inline constexpr auto kZ9001Chips = make_chip_manifest(
    Slot<RAMChip>{0x0000, 16384, 0, "RAM"},
    Slot<RAMChip>{0xEC00,  1024, 0, "Video RAM"},
    Slot<ROMChip>{0xF000,  4096, 0, "OS ROM"}.with_rom("z9001_os.rom|os.rom|OS.ROM"),
    // Non-bus chips — factory-created, not address-decoded
    Slot<U880>      {0, 0, 0, "U880"},
    Slot<z80_pio_t> {0, 0, 0, "U855 PIO #1"},
    Slot<z80_pio_t> {0, 0, 0, "U855 PIO #2"},
    Slot<z80_ctc_t> {0, 0, 0, "U857 CTC"}
);

inline constexpr auto kKC87Chips = make_chip_manifest(
    Slot<RAMChip>{0x0000, 65536, 0, "RAM", 0, 0, 0, 0xC000},  // 48 KB visible of 64 KB
    Slot<ROMChip>{0xC000,  8192, 0, "BASIC ROM lo"},
    Slot<ROMChip>{0xE000,  2048, 0, "BASIC ROM hi"},
    Slot<RAMChip>{0xE800,  1024, 0, "Color RAM"},
    Slot<RAMChip>{0xEC00,  1024, 0, "Video RAM"},
    Slot<ROMChip>{0xF000,  4096, 0, "OS ROM"}.with_rom("z9001_os.rom|os.rom|OS.ROM"),
    // Non-bus chips — factory-created, not address-decoded
    Slot<U880>      {0, 0, 0, "U880"},
    Slot<z80_pio_t> {0, 0, 0, "U855 PIO #1"},
    Slot<z80_pio_t> {0, 0, 0, "U855 PIO #2"},
    Slot<z80_ctc_t> {0, 0, 0, "U857 CTC"}
);

// BusTraits — selects the correct manifest per variant
template<Z9001Variant V> struct Z9001BusTraits;

template<> struct Z9001BusTraits<Z9001Variant::Z9001> {
    static constexpr const auto& kManifest = kZ9001Chips;
    using Spec = ManifestBusSpec<kZ9001Chips, 16, 8>;
};

template<> struct Z9001BusTraits<Z9001Variant::KC87> {
    static constexpr const auto& kManifest = kKC87Chips;
    using Spec = ManifestBusSpec<kKC87Chips, 16, 8>;
};

// ── System ───────────────────────────────────────────────────────────────
template<Z9001Variant V>
class Z9001System : public System {
    using Traits = Z9001VariantTraits<V>;
public:
    Z9001System();
    ~Z9001System() override;

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
    void set_speed_multiplier(float multiplier) override;

    bool is_system_ready() const override { return system_ready_; }

private:
    // ── Chips ────────────────────────────────────────────────────────────
    U880*       cpu_  = nullptr;     // U880 @ 2.4576 MHz — owned by board_
    z80_pio_t*  pio1_ = nullptr;     // U855 PIO #1 (keyboard + system control) — owned by board_
    z80_pio_t*  pio2_ = nullptr;     // U855 PIO #2 (keyboard + cassette) — owned by board_
    z80_ctc_t*  ctc_  = nullptr;     // U857 CTC (timing + sound) — owned by board_

    // ── Memory — owned by Board, accessed via chip_as<>() ────────────
    ROMChip* basic_rom_lo_chip_  = nullptr;  // KC 87 only
    ROMChip* basic_rom_hi_chip_  = nullptr;  // KC 87 only
    RAMChip* color_ram_chip_     = nullptr;  // KC 87 only
    RAMChip* video_ram_chip_     = nullptr;
    ROMChip* os_rom_chip_        = nullptr;

    // Character ROM — NOT bus-mapped (used for display rendering only)
    std::vector<uint8_t> char_rom_;

    // ── MemoryBus — declarative setup via chip manifest ──────────────────
    using BT  = Z9001BusTraits<V>;
    using Bus = MemoryBus<typename BT::Spec>;
    using PT  = PackingTraits<typename BT::Spec>;
    using MainBoard = Board<typename BT::Spec>;
    Bus bus_;
    MainBoard board_{BT::kManifest};

    // ── Display ──────────────────────────────────────────────────────────
    IndexedFrameBuffer display_;
    std::unique_ptr<CompositeVideoPort> video_port_;  // Video stream output

    // ── Keyboard ─────────────────────────────────────────────────────────
    uint8_t keyboard_matrix_[z9001_constants::KEYBOARD_ROWS] = {};

    // ── System state ─────────────────────────────────────────────────────
    bus_state_t pins_       = Z9001_BUS_DEFAULT_STATE;
    bool        system_ready_ = false;
    int audio_sample_rate_  = z9001_constants::DEFAULT_SAMPLE_RATE;

    // ── Internal helpers ─────────────────────────────────────────────────
    void        configure_bus_memory_map();  // Trim RAM pages for KC87
    bus_state_t io_tick(bus_state_t pins);
    void        render_frame();   // Render one complete video frame to framebuffer_
    bool        load_roms();
};
