#pragma once
/*
 * oric_system.h — Oric-1 / Oric Atmos system declaration
 *
 * Oric-1 (1983) and Oric Atmos (1984) from Tangerine / Oric International.
 *
 * Oric-1:
 *   - MOS 6502A @ 1 MHz
 *   - 16KB or 48KB RAM, 16KB ROM (BASIC 1.0)
 *   - Custom ULA (attribute-based display, 240×200 hi-res / 40×28 text)
 *   - AY-3-8912 PSG (3 channels, accent accessed via VIA)
 *   - MOS 6522 VIA (keyboard, cassette, printer, AY control)
 *
 * Oric Atmos:
 *   - Same hardware as 48K Oric-1
 *   - Improved BASIC 1.1 ROM with better keyboard handling
 *   - Better keyboard (real keys vs. membrane)
 *
 * The Oric ULA handles:
 *   - Generating video output from RAM contents (text + hi-res modes)
 *   - Ink/paper attribute decoding (serial attributes per character row)
 *   - Cassette I/O signal modulation
 * The ULA is simple enough to implement as system-level glue code rather
 * than a standalone chip — it has no programmable registers of its own.
 *
 * Memory map:
 *   $0000–$02FF : Zero page, stack, system variables
 *   $0300–$030F : MOS 6522 VIA
 *   $0400–$BFFF : RAM (user area + screen memory)
 *   $BB80–$BFE0 : Text screen RAM (40 × 28)
 *   $A000–$BF3F : Hi-res bitmap RAM (240 × 200)
 *   $C000–$FFFF : ROM (BASIC + Monitor)
 */

#include "systems/oric/oric_constants.hpp"
#include "core/system.hpp"
#include "core/system_lines.hpp"
#include "core/board.hpp"
#include "core/standard_chips.hpp"
#include "core/signal/video_port.hpp"
#include "chip/cpu/fam65xx/mos6502.hpp"
#include "chip/sound/ay_psg/ay_3_8912.hpp"
#include "chip/io/mos6522.hpp"
#include "chip/memory/memory_chip.hpp"
#include <cstdint>

// ============================================================================
// Oric Variant Template
// ============================================================================

enum class OricVariant { ORIC_1, ORIC_ATMOS };

template<OricVariant V> struct OricVariantTraits;

template<> struct OricVariantTraits<OricVariant::ORIC_1> {
    static constexpr const char* name         = "Oric-1";
    static constexpr const char* short_name   = "Oric1";
    static constexpr const char* description  = "Oric-1 (1983) — MOS 6502 @ 1MHz, AY-3-8912, 48KB RAM";
    static constexpr const char* data_folder  = "oric";
    static constexpr uint32_t    default_ram  = oric_constants::RAM_48K;
    static constexpr bool        is_atmos     = false;
    static std::vector<const char*> get_aliases() {
        return {"Oric1", "Oric-1", "Oric"};
    }
};

template<> struct OricVariantTraits<OricVariant::ORIC_ATMOS> {
    static constexpr const char* name         = "Oric Atmos";
    static constexpr const char* short_name   = "OricAtmos";
    static constexpr const char* description  = "Oric Atmos (1984) — MOS 6502 @ 1MHz, AY-3-8912, 48KB RAM, BASIC 1.1";
    static constexpr const char* data_folder  = "oric";
    static constexpr uint32_t    default_ram  = oric_constants::RAM_48K;
    static constexpr bool        is_atmos     = true;
    static std::vector<const char*> get_aliases() {
        return {"OricAtmos", "Oric Atmos", "Atmos"};
    }
};

// ============================================================================
// Oric default bus state
// ============================================================================

#define ORIC_BUS_DEFAULT_STATE (MOS6502::default_bus_state())

// =============================================================================
// Oric chip manifests — declarative memory layout
// =============================================================================
//
// Oric-1 (48K):
//   Slot 0: RAM       — 64 KB at $0000 (address space covers up to $BFFF)
//   Slot 1: ROM       — 16 KB at $C000 (BASIC 1.0 + Monitor)
//   Slot 2: VIA       — MMIO-only, 16-byte window at $0300
//
// Oric Atmos (48K):
//   Same layout, different ROM (BASIC 1.1)
//

inline constexpr auto kOric1Chips = make_chip_manifest(
    Slot<RAMChip>{0x0000, 65536, 0, "RAM"},
    Slot<ROMChip>{0xC000, 16384, 0, "ROM"}.with_rom("oric1.rom|basic10.rom|BASIC10.ROM"),
    Slot<mos6522_t>{0x0300, 0, 0xFFF0},    // MMIO-only, 16-byte window at $0300
    // Non-bus chips — factory-created, not address-decoded
    Slot<MOS6502>    {0, 0, 0, "MOS 6502"},
    Slot<AY_3_8912>{0, 0, 0, "AY-3-8912"}
);

inline constexpr auto kOricAtmosChips = make_chip_manifest(
    Slot<RAMChip>{0x0000, 65536, 0, "RAM"},
    Slot<ROMChip>{0xC000, 16384, 0, "ROM"}.with_rom("atmos.rom|basic11.rom|BASIC11.ROM"),
    Slot<mos6522_t>{0x0300, 0, 0xFFF0},
    // Non-bus chips — factory-created, not address-decoded
    Slot<MOS6502>    {0, 0, 0, "MOS 6502"},
    Slot<AY_3_8912>{0, 0, 0, "AY-3-8912"}
);

// BusTraits — selects the correct manifest per variant
template<OricVariant V> struct OricBusTraits;

template<> struct OricBusTraits<OricVariant::ORIC_1> {
    static constexpr const auto& kManifest = kOric1Chips;
    using Spec = ManifestBusSpec<kOric1Chips, 16, 8>;
};

template<> struct OricBusTraits<OricVariant::ORIC_ATMOS> {
    static constexpr const auto& kManifest = kOricAtmosChips;
    using Spec = ManifestBusSpec<kOricAtmosChips, 16, 8>;
};

// ============================================================================
// Oric ChipSet — value-typed chips owned by Board
// ============================================================================

struct OricChips : CommonBoardChips<MOS6502, NoChip, AY_3_8912, mos6522_t> {};

// ============================================================================
// Oric System
// ============================================================================

template<OricVariant V>
class OricSystem : public System {
    using Traits = OricVariantTraits<V>;
    using BTraits = OricBusTraits<V>;

public:
    OricSystem();
    ~OricSystem() override;

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

    uint32_t get_audio_samples(float* buffer, uint32_t max_samples) override;
    void set_audio_sample_rate(int sample_rate_hz) override;

    void handle_keyboard_event(SDL_Keycode key, bool pressed) override;


private:
    // ── Board + bus ──────────────────────────────────────────────────────
    using Bus       = MemoryBus<typename BTraits::Spec>;
    using MainBoard = Board<typename BTraits::Spec, OricChips>;
    Bus       bus_;
    MainBoard board_{BTraits::kManifest};

    // ── Memory chips — post-init pointers ────────────────────────────────
    uint8_t* ram_ptr_ = nullptr;     // Direct pointer for video rendering

    // ── Display ──────────────────────────────────────────────────────────
    uint8_t pixel_buffer_[oric_constants::FB_WIDTH * oric_constants::FB_HEIGHT] = {};
    std::unique_ptr<CompositeVideoPort> video_port_;

    // ── Keyboard ─────────────────────────────────────────────────────────
    uint8_t keyboard_matrix_[oric_constants::KEYBOARD_ROWS] = {};

    // ── ULA state (simple — implemented as system-level glue) ────────────
    // The Oric ULA has no programmable registers.  It reads display data
    // directly from RAM and generates video output with serial attributes.
    bool hires_mode_ = false;        // Hi-res vs. text mode

    // ── System state ─────────────────────────────────────────────────────
    bus_state_t pins_      = ORIC_BUS_DEFAULT_STATE;
    int         audio_sample_rate_ = oric_constants::DEFAULT_SAMPLE_RATE;

    // ── Internal helpers ─────────────────────────────────────────────────
    void configure_bus_memory_map();
    void render_frame();             // ULA: render display from RAM
    bool load_roms();

    // VIA callbacks for AY-3-8912 accent control
    static void via_port_a_write(void* context, uint8_t data);
    static uint8_t via_port_b_read(void* context, uint8_t output);
};
