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
#include "core/typed_manifest.hpp"
#include "core/typed_port.hpp"
#include "core/signal/video_port.hpp"

#include "chip/cpu/z80/u880.hpp"
#include "chip/cpu/z80/z80.hpp"   // Z80_MREQ_BIT / Z80_IORQ_BIT
#include "chip/io/z80_pio.hpp"
#include "chip/io/z80_ctc.hpp"
#include "chip/memory/memory_chip.hpp"
#include "chip/video/char_display/char_display.hpp"
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
inline constexpr auto kZ9001Manifest = make_manifest(
    // Chips
    Slot<U880>{.base_addr = 0, .label = "U880"},
    Slot<RAMChip>{.base_addr = 0x0000, .size_bytes = 16384, .label = "RAM"},
    Slot<RAMChip>{.base_addr = 0xEC00, .size_bytes = 1024, .label = "Video RAM"},
    Slot<ROMChip>{.base_addr = 0xF000, .size_bytes = 4096, .label = "OS ROM", .rom = {"z9001_os.rom|os.rom|OS.ROM"}},
    Slot<z80_pio_t>{.base_addr = 0x0088, .addr_mask = 0x00FC, .label = "U855 PIO #1"},
    Slot<z80_pio_t>{.base_addr = 0x0090, .addr_mask = 0x00FC, .label = "U855 PIO #2"},
    Slot<z80_ctc_t>{.base_addr = 0x0080, .addr_mask = 0x00FC, .label = "U857 CTC"},
    // Ports
    Slot<PortCassette>{.name = "Cassette Port"},
    Slot<PortCompositeVideo>{.name = "Video Out", .default_device = "crt_junost"}
);

inline constexpr auto kKC87Manifest = make_manifest(
    // Chips
    Slot<U880>{.base_addr = 0, .label = "U880"},
    Slot<RAMChip>{.base_addr = 0x0000, .size_bytes = 65536, .label = "RAM", .effective_size = 0xC000},
    Slot<ROMChip>{.base_addr = 0xC000, .size_bytes = 8192, .label = "BASIC ROM lo", .rom = {"z9001_basic.rom@0|kc87_basic_lo.rom|basic_lo.rom"}},
    Slot<ROMChip>{.base_addr = 0xE000, .size_bytes = 2048, .label = "BASIC ROM hi", .rom = {"z9001_basic.rom@8192|kc87_basic_hi.rom|basic_hi.rom"}},
    Slot<RAMChip>{.base_addr = 0xE800, .size_bytes = 1024, .label = "Color RAM"},
    Slot<RAMChip>{.base_addr = 0xEC00, .size_bytes = 1024, .label = "Video RAM"},
    Slot<ROMChip>{.base_addr = 0xF000, .size_bytes = 4096, .label = "OS ROM", .rom = {"z9001_os.rom|os.rom|OS.ROM"}},
    Slot<z80_pio_t>{.base_addr = 0x0088, .addr_mask = 0x00FC, .label = "U855 PIO #1"},
    Slot<z80_pio_t>{.base_addr = 0x0090, .addr_mask = 0x00FC, .label = "U855 PIO #2"},
    Slot<z80_ctc_t>{.base_addr = 0x0080, .addr_mask = 0x00FC, .label = "U857 CTC"},
    // Ports
    Slot<PortCassette>{.name = "Cassette Port"},
    Slot<PortCompositeVideo>{.name = "Video Out", .default_device = "crt_junost"}
);

inline constexpr size_t kZ9001ChipCount = decltype(kZ9001Manifest)::chip_count;
inline constexpr size_t kKC87ChipCount  = decltype(kKC87Manifest)::chip_count;

// BusTraits — selects the correct manifest per variant
template<Z9001Variant V> struct Z9001BusTraits;

template<> struct Z9001BusTraits<Z9001Variant::Z9001> {
    static constexpr const auto& kManifest = kZ9001Manifest;
    using Spec = ManifestBusSpec<kZ9001Manifest, 16, 8>;
};

template<> struct Z9001BusTraits<Z9001Variant::KC87> {
    static constexpr const auto& kManifest = kKC87Manifest;
    using Spec = ManifestBusSpec<kKC87Manifest, 16, 8>;
};

// ── Board ──────────────────────────────────────────────────────────────
// Uses KC87 manifest component tuple (superset) for field declarations.
// Z9001 leaves unused fields (basic_rom_lo, basic_rom_hi, color_ram) unbound.
template<typename BSpec>
struct Z9001Board : Board<BSpec> {
    using ComponentTuple = decltype(kKC87Manifest)::component_tuple;
    ComponentTuple components_;

    // Chip aliases
    U880&       z80          = std::get<0>(components_);
    RAMChip&    ram          = std::get<1>(components_);
    ROMChip&    basic_rom_lo = std::get<2>(components_);
    ROMChip&    basic_rom_hi = std::get<3>(components_);
    RAMChip&    color_ram    = std::get<4>(components_);
    RAMChip&    video_ram    = std::get<5>(components_);
    ROMChip&    os_rom       = std::get<6>(components_);
    z80_pio_t&  pio1         = std::get<7>(components_);
    z80_pio_t&  pio2         = std::get<8>(components_);
    z80_ctc_t&  ctc          = std::get<9>(components_);

    // Port aliases
    PortCassette&       cassette_port = std::get<10>(components_);
    PortCompositeVideo& video_port    = std::get<11>(components_);

    template<size_t N>
    Z9001Board(const ChipManifest<N>& m) : Board<BSpec>(m) {}
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

    void handle_keyboard_event(SDL_Keycode key, bool pressed) override;

    void* get_video_port_ptr() override { return video_port_.get(); }

private:
    // Character ROM — NOT bus-mapped (used for display rendering only)
    std::vector<uint8_t> char_rom_;

    // ── MemoryBus — declarative setup via chip manifest ──────────────────
    using BT  = Z9001BusTraits<V>;
    using Bus = MemoryBus<typename BT::Spec>;
    using PT  = PackingTraits<typename BT::Spec>;
    using MainBoard = Z9001Board<typename BT::Spec>;
    Bus bus_;
    MainBoard board_{BT::kManifest};

    // ── Display ──────────────────────────────────────────────────────────
    std::unique_ptr<CompositeVideoPort> video_port_;  // Video output
    CharDisplayGenerator video_gen_;                  // TTL character display generator

    // ── Keyboard ─────────────────────────────────────────────────────────
    uint8_t keyboard_matrix_[z9001_constants::KEYBOARD_ROWS] = {};

    // ── System state ─────────────────────────────────────────────────────
    bus_state_t pins_       = Z9001_BUS_DEFAULT_STATE;
    int audio_sample_rate_  = z9001_constants::DEFAULT_SAMPLE_RATE;

    // ── Internal helpers ─────────────────────────────────────────────────
    void        configure_bus_memory_map();  // Trim RAM pages for KC87
    bus_state_t io_tick(bus_state_t pins);
    void        render_frame();   // Render one complete video frame to framebuffer_
    bool        load_roms();
};
