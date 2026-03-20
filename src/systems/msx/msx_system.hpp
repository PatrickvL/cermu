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
// MSX1:
//   Slot 0: ROM  — 32 KB at $0000 (BIOS 16KB + BASIC 16KB)
//   Slot 1: RAM  — 64 KB at $0000 (mapped to $8000-$FFFF by default)
//
// MSX2:
//   Slot 0: ROM  — 32 KB at $0000
//   Slot 1: RAM  — 128 KB at $0000 (banked via memory mapper)
//
// MSX2+:
//   Slot 0: ROM  — 32 KB at $0000
//   Slot 1: RAM  — 64 KB at $0000

inline constexpr auto kMSX1Chips = make_chip_manifest(
    Slot<ROMChip>{0x0000, 32768, 0, "BIOS+BASIC ROM"}.with_rom("msx.rom|msx1.rom|MSX.ROM"),
    Slot<RAMChip>{0x0000, 65536, 0, "Main RAM"},
    // Non-bus chips — factory-created, not address-decoded
    Slot<ZilogZ80A>  {0, 0, 0, "Z80A"},
    Slot<TMS9918A>   {0, 0, 0, "TMS9918A"},
    Slot<AY_3_8910>  {0, 0, 0, "AY-3-8910"},
    Slot<i8255_t>    {0, 0, 0, "i8255 PPI"}
);

inline constexpr auto kMSX2Chips = make_chip_manifest(
    Slot<ROMChip>{0x0000, 32768, 0, "BIOS+BASIC ROM"}.with_rom("msx2.rom|MSX2.ROM"),
    Slot<RAMChip>{0x0000, 131072, 0, "Main RAM", 0, 16384},
    // Non-bus chips
    Slot<ZilogZ80A>  {0, 0, 0, "Z80A"},
    Slot<V9938>      {0, 0, 0, "V9938"},
    Slot<AY_3_8910>  {0, 0, 0, "AY-3-8910"},
    Slot<i8255_t>    {0, 0, 0, "i8255 PPI"}
);

inline constexpr auto kMSX2PChips = make_chip_manifest(
    Slot<ROMChip>{0x0000, 32768, 0, "BIOS+BASIC ROM"}.with_rom("msx2p.rom|MSX2P.ROM|msx2+.rom"),
    Slot<RAMChip>{0x0000, 65536, 0, "Main RAM"},
    // Non-bus chips
    Slot<ZilogZ80A>  {0, 0, 0, "Z80A"},
    Slot<V9958>      {0, 0, 0, "V9958"},
    Slot<AY_3_8910>  {0, 0, 0, "AY-3-8910"},
    Slot<i8255_t>    {0, 0, 0, "i8255 PPI"}
);

// BusTraits — selects the correct manifest per variant
template<MSXVariant V> struct MSXBusTraits;

template<> struct MSXBusTraits<MSXVariant::MSX1> {
    static constexpr const auto& kManifest = kMSX1Chips;
    using Spec = ManifestBusSpec<kMSX1Chips, 16, 8>;
};

template<> struct MSXBusTraits<MSXVariant::MSX2> {
    static constexpr const auto& kManifest = kMSX2Chips;
    using Spec = ManifestBusSpec<kMSX2Chips, 16, 8>;
};

template<> struct MSXBusTraits<MSXVariant::MSX2P> {
    static constexpr const auto& kManifest = kMSX2PChips;
    using Spec = ManifestBusSpec<kMSX2PChips, 16, 8>;
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

    // Display
    void get_display_dimensions(int* width, int* height) const override;

    // Audio
    uint32_t get_audio_samples(float* buffer, uint32_t max_samples) override;
    void set_audio_sample_rate(int sample_rate_hz) override;

    // Input
    void handle_keyboard_event(SDL_Keycode key, bool pressed) override;

    // GUI
    void render_system_menu_items() override;
    void render_configuration_ui() override;

    // Emulation control
    void set_speed_multiplier(float multiplier) override;

    bool is_system_ready() const override { return system_ready_; }

private:
    // ── Chips ────────────────────────────────────────────────────────────
    ZilogZ80A*   cpu_ = nullptr;   // Z80A CPU — owned by board_
    VDP          vdp_;             // Video Display Processor
    AY_3_8910    psg_;             // AY-3-8910 PSG
    i8255_t      ppi_;             // i8255 PPI (keyboard + slot control)

    // ── Board + bus ──────────────────────────────────────────────────────
    using Bus       = MemoryBus<typename BT::Spec>;
    using PT        = PackingTraits<typename BT::Spec>;
    using MainBoard = Board<typename BT::Spec>;
    Bus       bus_;
    MainBoard board_{BT::kManifest};

    // ── Slot selection (PPI Port A) ──────────────────────────────────────
    uint8_t slot_select_ = 0;     // PPI Port A: 2 bits per page (pp3|pp2|pp1|pp0)

    // ── Display ──────────────────────────────────────────────────────────
    IndexedFrameBuffer display_;
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
    bool        system_ready_ = false;
    uint32_t    frame_tstate_counter_ = 0;

    // ── Internal helpers ─────────────────────────────────────────────────
    void configure_bus_memory_map();
    bus_state_t io_tick(bus_state_t pins);
    bool load_roms();
};
