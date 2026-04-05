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
#include "core/typed_manifest.hpp"
#include "core/typed_port.hpp"
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

// =============================================================================
// Z1013 chip declarations — variant-specific single source of truth
// =============================================================================
//
// Row: V(ctx, type, chip, base, size, mask, overlay, label, rom_files)
//
// Z1013.01 / Z1013.16 (16 KB RAM):
//   RAM 16 KB at $0000, Video RAM 1 KB at $EC00, Monitor ROM 2 KB at $F000
//
// Z1013.64 (64 KB RAM, ROM BASIC):
//   RAM 64 KB at $0000, BASIC ROM 8+2 KB split at $C000/$E000,
//   Video RAM 1 KB at $EC00, Monitor ROM 2 KB at $F000
//
// The character ROM is NOT bus-mapped (used only for display rendering).
// All I/O is Z80 port-based (IORQ) — no MMIO slots needed.
// BASIC ROM is split into 8 KB + 2 KB because ChipSlot requires power-of-2
// sizes and the original 10 KB is not a power of 2.
//

inline constexpr auto kZ1013_16K_Manifest = make_manifest(
    // Chips
    Slot<U880>{.base_addr = 0x0000, .label = "U880"},
    Slot<RAMChip>{.base_addr = 0x0000, .size_bytes = 16384, .label = "RAM"},
    Slot<RAMChip>{.base_addr = 0xEC00, .size_bytes = 1024, .label = "Video RAM"},
    Slot<ROMChip>{.base_addr = 0xF000, .size_bytes = 2048, .label = "Monitor ROM", .rom = {"z1013_mon.rom|monitor.rom|MON.ROM"}},
    Slot<z80_pio_t>{.base_addr = 0x0004, .addr_mask = 0x00FC, .label = "U855 PIO"},
    // Ports
    Slot<PortCassette>{.name = "Cassette Port"},
    Slot<PortCompositeVideo>{.name = "Video Out", .default_device = "crt_junost"}
);

inline constexpr auto kZ1013_64K_Manifest = make_manifest(
    // Chips
    Slot<U880>{.base_addr = 0x0000, .label = "U880"},
    Slot<RAMChip>{.base_addr = 0x0000, .size_bytes = 65536, .label = "RAM"},
    Slot<ROMChip>{.base_addr = 0xC000, .size_bytes = 8192, .label = "BASIC ROM lo", .rom = {"z1013_basic.rom@0|basic_lo.rom|BASIC.ROM@0"}},
    Slot<ROMChip>{.base_addr = 0xE000, .size_bytes = 2048, .label = "BASIC ROM hi", .rom = {"z1013_basic.rom@8192|basic_hi.rom|BASIC.ROM@8192"}},
    Slot<RAMChip>{.base_addr = 0xEC00, .size_bytes = 1024, .label = "Video RAM"},
    Slot<ROMChip>{.base_addr = 0xF000, .size_bytes = 2048, .label = "Monitor ROM", .rom = {"z1013_mon.rom|monitor.rom|MON.ROM"}},
    Slot<z80_pio_t>{.base_addr = 0x0004, .addr_mask = 0x00FC, .label = "U855 PIO"},
    // Ports
    Slot<PortCassette>{.name = "Cassette Port"},
    Slot<PortCompositeVideo>{.name = "Video Out", .default_device = "crt_junost"}
);

inline constexpr size_t kZ1013_16K_ChipCount = decltype(kZ1013_16K_Manifest)::chip_count;
inline constexpr size_t kZ1013_64K_ChipCount = decltype(kZ1013_64K_Manifest)::chip_count;

// BusTraits — selects the correct manifest per variant
template<Z1013Variant V> struct Z1013BusTraits;

template<> struct Z1013BusTraits<Z1013Variant::Z1013_01> {
    static constexpr const auto& kManifest = kZ1013_16K_Manifest;
    using Spec = ManifestBusSpec<kZ1013_16K_Manifest, 16, 8>;
};

template<> struct Z1013BusTraits<Z1013Variant::Z1013_16> {
    static constexpr const auto& kManifest = kZ1013_16K_Manifest;
    using Spec = ManifestBusSpec<kZ1013_16K_Manifest, 16, 8>;
};

template<> struct Z1013BusTraits<Z1013Variant::Z1013_64> {
    static constexpr const auto& kManifest = kZ1013_64K_Manifest;
    using Spec = ManifestBusSpec<kZ1013_64K_Manifest, 16, 8>;
};

// ── Board ──────────────────────────────────────────────────────────────
// Uses the 64K manifest component tuple (superset) — 16K variants
// simply leave the basic_lo/basic_hi fields unused.
template<typename BSpec>
struct Z1013Board : Board<BSpec> {
    using ComponentTuple = decltype(kZ1013_64K_Manifest)::component_tuple;
    ComponentTuple components_;

    // Chip aliases
    U880&       z80      = std::get<0>(components_);
    RAMChip&    ram      = std::get<1>(components_);
    ROMChip&    basic_lo = std::get<2>(components_);
    ROMChip&    basic_hi = std::get<3>(components_);
    RAMChip&    vram     = std::get<4>(components_);
    ROMChip&    monitor  = std::get<5>(components_);
    z80_pio_t&  pio      = std::get<6>(components_);

    // Port aliases
    PortCassette&       cassette_port = std::get<7>(components_);
    PortCompositeVideo& video_port    = std::get<8>(components_);

    template<size_t N>
    Z1013Board(const ChipManifest<N>& m) : Board<BSpec>(m) {}
};

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

    void* get_video_port_ptr() override { return video_port_.get(); }

private:
    // ── Chips (value-typed via Board Chips) ────────────────────────────

    // Character ROM — NOT bus-mapped (used for display rendering only)
    std::vector<uint8_t> char_rom_;

    // ── MemoryBus — declarative setup via chip manifest ──────────────────
    using BT  = Z1013BusTraits<V>;
    using Bus = MemoryBus<typename BT::Spec>;
    using PT  = PackingTraits<typename BT::Spec>;
    using MainBoard = Z1013Board<typename BT::Spec>;
    Bus bus_;
    MainBoard board_{BT::kManifest};

    // ── Display ──────────────────────────────────────────────────────────
    std::unique_ptr<CompositeVideoPort> video_port_;  // Video output
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
