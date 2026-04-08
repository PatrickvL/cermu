#pragma once
/*
 * msx_system.hpp — MSX1 / MSX2 / MSX2+ Emulated System
 *
 * The MSX standard (1983) was a unified home computer architecture designed
 * by ASCII Corporation and manufactured by multiple vendors (Sony, Panasonic,
 * Philips, etc.).  All MSX machines share a common architecture:
 *
 * MSX1:   Z80A + TMS9918A + AY-3-8910 + i8255 PPI
 * MSX2:   Z80A + V9938    + AY-3-8910 + i8255 PPI + memory mapper
 * MSX2+:  Z80A + V9958    + AY-3-8910 + i8255 PPI + memory mapper
 *
 * The i8255 PPI handles:
 *   Port A ($A8): Primary slot select register
 *   Port B ($A9): Keyboard column data (active-low)
 *   Port C ($AA): Keyboard row select (bits 0-3), caps LED, cassette
 *
 * Templated on MSXVariant to share code between MSX1, MSX2, and MSX2+.
 */

#include "systems/msx/msx_constants.hpp"
#include "core/system.hpp"
#include "core/system_lines.hpp"
#include "core/board.hpp"
#include "core/typed_manifest.hpp"
#include "core/typed_port.hpp"
#include "core/signal/video_port.hpp"
#include "core/signal/audio_port.hpp"
#include "chip/cpu/z80/zilog_z80a.hpp"
#include "chip/video/tms9918/tms9918a.hpp"
#include "chip/video/tms9918/v9938.hpp"
#include "chip/video/tms9918/v9958.hpp"
#include "chip/sound/ay_psg/ay_3_8910.hpp"
#include "chip/io/i8255.hpp"
#include "chip/memory/memory_chip.hpp"
#include "utils/ring_buffer.hpp"
#include <cstdint>
#include <memory>

// ============================================================================
// MSX Variant Template
// ============================================================================

enum class MSXVariant { MSX1, MSX2, MSX2P };

template<MSXVariant V> struct MSXVariantTraits;

template<> struct MSXVariantTraits<MSXVariant::MSX1> {
    static constexpr const char* name         = "MSX1";
    static constexpr const char* short_name   = "MSX1";
    static constexpr const char* description  = "MSX1 — Z80A, TMS9918A, AY-3-8910 (1983)";
    static constexpr const char* data_folder  = "msx";
    static constexpr uint32_t    ram_size     = msx_constants::RAM_SIZE_MSX1;
    static constexpr int         display_w    = msx_constants::DISPLAY_WIDTH_MSX1;
    static constexpr int         display_h    = msx_constants::DISPLAY_HEIGHT_MSX1;
    static constexpr uint16_t    vdp_lines    = 262;  // NTSC
    static std::vector<const char*> get_aliases() {
        return {"MSX", "MSX1"};
    }
    using VDP = TMS9918A;
};

template<> struct MSXVariantTraits<MSXVariant::MSX2> {
    static constexpr const char* name         = "MSX2";
    static constexpr const char* short_name   = "MSX2";
    static constexpr const char* description  = "MSX2 — Z80A, V9938, AY-3-8910 (1985)";
    static constexpr const char* data_folder  = "msx";
    static constexpr uint32_t    ram_size     = msx_constants::RAM_SIZE_MSX2;
    static constexpr int         display_w    = msx_constants::DISPLAY_WIDTH_MSX2;
    static constexpr int         display_h    = msx_constants::DISPLAY_HEIGHT_MSX2;
    static constexpr uint16_t    vdp_lines    = 262;
    static std::vector<const char*> get_aliases() {
        return {"MSX2"};
    }
    using VDP = V9938;
};

template<> struct MSXVariantTraits<MSXVariant::MSX2P> {
    static constexpr const char* name         = "MSX2+";
    static constexpr const char* short_name   = "MSX2+";
    static constexpr const char* description  = "MSX2+ — Z80A, V9958, AY-3-8910 (1988)";
    static constexpr const char* data_folder  = "msx";
    static constexpr uint32_t    ram_size     = msx_constants::RAM_SIZE_MSX2P;
    static constexpr int         display_w    = msx_constants::DISPLAY_WIDTH_MSX2;
    static constexpr int         display_h    = msx_constants::DISPLAY_HEIGHT_MSX2;
    static constexpr uint16_t    vdp_lines    = 262;
    static std::vector<const char*> get_aliases() {
        return {"MSX2+", "MSX2Plus"};
    }
    using VDP = V9958;
};

// ============================================================================
// MSX default bus state
// ============================================================================

#define MSX_BUS_DEFAULT_STATE (ZilogZ80A::default_bus_state())

// =============================================================================
// MSX chip manifests — declarative memory layout
// =============================================================================
//
// MSX primary slot architecture:
//   Slot 0: BIOS+BASIC ROM (32 KB)
//   Slot 1: Cartridge ROM  (up to 64 KB, loaded at runtime)
//   Slot 2: (unused in MSX1; expansion in MSX2+)
//   Slot 3: Main RAM
//
// PPI Port A ($A8) selects which slot appears in each 16 KB page:
//   Bits 1:0 → page 0 ($0000-$3FFF)
//   Bits 3:2 → page 1 ($4000-$7FFF)
//   Bits 5:4 → page 2 ($8000-$BFFF)
//   Bits 7:6 → page 3 ($C000-$FFFF)
//
// Memory mapping uses precalculated snapshots (256 configurations,
// one per PPI value), switched via load_snapshot() for O(1) banking.

inline constexpr auto kMSX1Manifest = make_manifest(
    // Chips  (tuple indices 0-6)
    Slot<ZilogZ80A>{.base_addr = 0, .label = "Z80A"},                                              // [0]
    Slot<ROMChip>{.base_addr = 0x0000, .size_bytes = 32768, .label = "BIOS+BASIC ROM",              // [1] slot 0
                  .rom = {"msx.rom|msx1.rom|MSX.ROM"}},
    Slot<ROMChip>{.base_addr = 0x0000, .size_bytes = 65536, .label = "Cartridge ROM"},               // [2] slot 1
    Slot<RAMChip>{.base_addr = 0x0000, .size_bytes = 65536, .label = "Main RAM"},                    // [3] slot 3
    Slot<TMS9918A>{.base_addr = 0x0098, .addr_mask = 0x00FC, .label = "TMS9918A"},                   // [4]
    Slot<AY_3_8910>{.base_addr = 0x00A0, .addr_mask = 0x00FC, .label = "AY-3-8910"},                 // [5]
    Slot<i8255_t>{.base_addr = 0x00A8, .addr_mask = 0x00FC, .label = "i8255 PPI"},                   // [6]
    // Ports
    Slot<PortControlDB9>{.name = "Joystick Port 1", .port_number = 1, .default_device = "joystick"},
    Slot<PortControlDB9>{.name = "Joystick Port 2", .port_number = 2, .default_device = "joystick"},
    Slot<PortCassette>{.name = "Cassette Port"},
    Slot<PortExpansion>{.name = "Cartridge Slot"},
    Slot<PortRgb>{.name = "Video Out", .default_device = "crt_rgb"},
    Slot<PortAudioMono>{.name = "Audio Out"}
);

inline constexpr auto kMSX2Manifest = make_manifest(
    // Chips  (tuple indices 0-6)
    Slot<ZilogZ80A>{.base_addr = 0, .label = "Z80A"},                                              // [0]
    Slot<ROMChip>{.base_addr = 0x0000, .size_bytes = 32768, .label = "BIOS+BASIC ROM",              // [1] slot 0
                  .rom = {"msx2.rom|MSX2.ROM"}},
    Slot<ROMChip>{.base_addr = 0x0000, .size_bytes = 65536, .label = "Cartridge ROM"},               // [2] slot 1
    Slot<RAMChip>{.base_addr = 0x0000, .size_bytes = 131072, .label = "Main RAM", .bank_size = 16384}, // [3] slot 3
    Slot<V9938>{.base_addr = 0x0098, .addr_mask = 0x00FC, .label = "V9938"},                         // [4]
    Slot<AY_3_8910>{.base_addr = 0x00A0, .addr_mask = 0x00FC, .label = "AY-3-8910"},                 // [5]
    Slot<i8255_t>{.base_addr = 0x00A8, .addr_mask = 0x00FC, .label = "i8255 PPI"},                   // [6]
    // Ports
    Slot<PortControlDB9>{.name = "Joystick Port 1", .port_number = 1, .default_device = "joystick"},
    Slot<PortControlDB9>{.name = "Joystick Port 2", .port_number = 2, .default_device = "joystick"},
    Slot<PortCassette>{.name = "Cassette Port"},
    Slot<PortExpansion>{.name = "Cartridge Slot"},
    Slot<PortRgb>{.name = "Video Out", .default_device = "crt_rgb"},
    Slot<PortAudioMono>{.name = "Audio Out"}
);

inline constexpr auto kMSX2PManifest = make_manifest(
    // Chips  (tuple indices 0-6)
    Slot<ZilogZ80A>{.base_addr = 0, .label = "Z80A"},                                              // [0]
    Slot<ROMChip>{.base_addr = 0x0000, .size_bytes = 32768, .label = "BIOS+BASIC ROM",              // [1] slot 0
                  .rom = {"msx2p.rom|MSX2P.ROM|msx2+.rom"}},
    Slot<ROMChip>{.base_addr = 0x0000, .size_bytes = 65536, .label = "Cartridge ROM"},               // [2] slot 1
    Slot<RAMChip>{.base_addr = 0x0000, .size_bytes = 65536, .label = "Main RAM"},                    // [3] slot 3
    Slot<V9958>{.base_addr = 0x0098, .addr_mask = 0x00FC, .label = "V9958"},                         // [4]
    Slot<AY_3_8910>{.base_addr = 0x00A0, .addr_mask = 0x00FC, .label = "AY-3-8910"},                 // [5]
    Slot<i8255_t>{.base_addr = 0x00A8, .addr_mask = 0x00FC, .label = "i8255 PPI"},                   // [6]
    // Ports
    Slot<PortControlDB9>{.name = "Joystick Port 1", .port_number = 1, .default_device = "joystick"},
    Slot<PortControlDB9>{.name = "Joystick Port 2", .port_number = 2, .default_device = "joystick"},
    Slot<PortCassette>{.name = "Cassette Port"},
    Slot<PortExpansion>{.name = "Cartridge Slot"},
    Slot<PortRgb>{.name = "Video Out", .default_device = "crt_rgb"},
    Slot<PortAudioMono>{.name = "Audio Out"}
);

inline constexpr size_t kMSXChipCount = decltype(kMSX1Manifest)::chip_count;

// BusTraits — selects the correct manifest per variant
template<MSXVariant V> struct MSXBusTraits;

template<> struct MSXBusTraits<MSXVariant::MSX1> {
    static constexpr const auto& kManifest = kMSX1Manifest;
    using Spec = ManifestBusSpec<kMSX1Manifest, 16, 8>;
};

template<> struct MSXBusTraits<MSXVariant::MSX2> {
    static constexpr const auto& kManifest = kMSX2Manifest;
    using Spec = ManifestBusSpec<kMSX2Manifest, 16, 8>;
};

template<> struct MSXBusTraits<MSXVariant::MSX2P> {
    static constexpr const auto& kManifest = kMSX2PManifest;
    using Spec = ManifestBusSpec<kMSX2PManifest, 16, 8>;
};

// ── Board specializations ──────────────────────────────────────────────
// VDP type varies per variant (TMS9918A / V9938 / V9958), so each
// variant needs its own Board specialization with the correct
// component tuple and aliases.

template<MSXVariant V> struct MSXBoard;

template<>
struct MSXBoard<MSXVariant::MSX1>
    : Board<MSXBusTraits<MSXVariant::MSX1>::Spec> {
    using BT  = MSXBusTraits<MSXVariant::MSX1>;
    using VDP = TMS9918A;
    using ComponentTuple = decltype(kMSX1Manifest)::component_tuple;
    ComponentTuple components_;

    ZilogZ80A& z80      = std::get<0>(components_);
    ROMChip&   bios_rom = std::get<1>(components_);
    ROMChip&   cart_rom = std::get<2>(components_);
    RAMChip&   main_ram = std::get<3>(components_);
    TMS9918A&  vdp      = std::get<4>(components_);
    AY_3_8910& psg      = std::get<5>(components_);
    i8255_t&   ppi      = std::get<6>(components_);

    template<size_t N>
    MSXBoard(const ChipManifest<N>& m) : Board<BT::Spec>(m) {}
};

template<>
struct MSXBoard<MSXVariant::MSX2>
    : Board<MSXBusTraits<MSXVariant::MSX2>::Spec> {
    using BT  = MSXBusTraits<MSXVariant::MSX2>;
    using VDP = V9938;
    using ComponentTuple = decltype(kMSX2Manifest)::component_tuple;
    ComponentTuple components_;

    ZilogZ80A& z80      = std::get<0>(components_);
    ROMChip&   bios_rom = std::get<1>(components_);
    ROMChip&   cart_rom = std::get<2>(components_);
    RAMChip&   main_ram = std::get<3>(components_);
    V9938&     vdp      = std::get<4>(components_);
    AY_3_8910& psg      = std::get<5>(components_);
    i8255_t&   ppi      = std::get<6>(components_);

    template<size_t N>
    MSXBoard(const ChipManifest<N>& m) : Board<BT::Spec>(m) {}
};

template<>
struct MSXBoard<MSXVariant::MSX2P>
    : Board<MSXBusTraits<MSXVariant::MSX2P>::Spec> {
    using BT  = MSXBusTraits<MSXVariant::MSX2P>;
    using VDP = V9958;
    using ComponentTuple = decltype(kMSX2PManifest)::component_tuple;
    ComponentTuple components_;

    ZilogZ80A& z80      = std::get<0>(components_);
    ROMChip&   bios_rom = std::get<1>(components_);
    ROMChip&   cart_rom = std::get<2>(components_);
    RAMChip&   main_ram = std::get<3>(components_);
    V9958&     vdp      = std::get<4>(components_);
    AY_3_8910& psg      = std::get<5>(components_);
    i8255_t&   ppi      = std::get<6>(components_);

    template<size_t N>
    MSXBoard(const ChipManifest<N>& m) : Board<BT::Spec>(m) {}
};

// ============================================================================
// MSX System
// ============================================================================

template<MSXVariant V>
class MSXSystem : public System {
    using Traits = MSXVariantTraits<V>;
    using BT     = MSXBusTraits<V>;
    using VDP    = typename Traits::VDP;

public:
    MSXSystem();
    ~MSXSystem() override;

    // System identification
    const SystemDescriptor& get_descriptor() const override;

    // Configuration
    bool set_configuration(const SystemConfiguration& config) override;
    bool apply_configuration() override;

    // Lifecycle
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

    void* get_video_port_ptr() override { return video_port_.get(); }

private:
    // ── Board + bus (chips live inside board_) ────────────────────────────
    using Bus       = MemoryBus<typename BT::Spec>;
    using PT        = PackingTraits<typename BT::Spec>;
    using MainBoard = MSXBoard<V>;
    Bus       bus_;
    MainBoard board_{BT::kManifest};

    // ── Slot selection (PPI Port A) ──────────────────────────────────────
    uint8_t slot_select_ = 0;     // PPI Port A: 2 bits per page (pp3|pp2|pp1|pp0)
    bool    cart_loaded_ = false; // true when a cartridge ROM has been loaded
    uint32_t cart_size_ = 0;      // loaded ROM size in bytes
    uint8_t cart_start_page_ = 0; // first 16 KB page occupied by cart (0-3)
    uint8_t cart_end_page_   = 0; // end-exclusive

    // ── Precalculated memory maps (256 PPI configurations) ───────────────
    // One snapshot per possible PPI Port A value.  Switching the slot
    // register is a single load_snapshot() call — O(1).
    using Snapshot = typename Bus::Snapshot;
    std::array<Snapshot, 256> slot_snapshots_{};

    std::unique_ptr<CompositeVideoPort> video_port_;

    // ── Audio ────────────────────────────────────────────────────────────
    uint32_t audio_sample_rate_    = msx_constants::DEFAULT_SAMPLE_RATE;
    uint32_t audio_sample_counter_ = 0;
    uint32_t audio_sample_period_  = 0;
    AudioRingBuffer audio_ring_buf_{8192};
    std::unique_ptr<AudioPort> audio_port_;

    // ── Keyboard ─────────────────────────────────────────────────────────
    uint8_t keyboard_matrix_[msx_constants::KEYBOARD_ROWS]{};
    uint8_t keyboard_row_select_ = 0;

    // ── System state ─────────────────────────────────────────────────────
    bus_state_t pins_         = MSX_BUS_DEFAULT_STATE;
    uint32_t    frame_tstate_counter_ = 0;

    // ── Internal helpers ─────────────────────────────────────────────────
    void configure_bus_memory_map();
    void generate_slot_snapshots();
    bus_state_t io_tick(bus_state_t pins);
    bool load_roms();
};
