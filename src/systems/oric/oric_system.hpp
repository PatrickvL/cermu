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

inline constexpr auto kOric1Manifest = make_manifest(
    // Chips
    Slot<MOS6502>{.base_addr = 0x0000, .label = "MOS 6502"},
    Slot<RAMChip>{.base_addr = 0x0000, .size_bytes = 0x00010000, .label = "RAM"},
    Slot<mos6522_t>{.base_addr = 0x0300, .addr_mask = 0xFFF0, .label = "VIA 6522"},
    Slot<ROMChip>{.base_addr = 0xC000, .size_bytes = 0x4000, .label = "ROM", .rom = {"oric1.rom|basic10.rom|BASIC10.ROM"}},
    Slot<AY_3_8912>{.base_addr = 0x0000, .label = "AY-3-8912"},
    // Ports
    Slot<PortCassette>{.name = "Cassette Port"},
    Slot<PortExpansion>{.name = "Expansion Port"},
    Slot<PortCompositeVideo>{.name = "Video Out", .default_device = "crt_tv_pal"},
    Slot<PortAudioMono>{.name = "Audio Out"}
);

inline constexpr auto kOricAtmosManifest = make_manifest(
    // Chips
    Slot<MOS6502>{.base_addr = 0x0000, .label = "MOS 6502"},
    Slot<RAMChip>{.base_addr = 0x0000, .size_bytes = 0x00010000, .label = "RAM"},
    Slot<mos6522_t>{.base_addr = 0x0300, .addr_mask = 0xFFF0, .label = "VIA 6522"},
    Slot<ROMChip>{.base_addr = 0xC000, .size_bytes = 0x4000, .label = "ROM", .rom = {"atmos.rom|basic11.rom|BASIC11.ROM"}},
    Slot<AY_3_8912>{.base_addr = 0x0000, .label = "AY-3-8912"},
    // Ports
    Slot<PortCassette>{.name = "Cassette Port"},
    Slot<PortExpansion>{.name = "Expansion Port"},
    Slot<PortCompositeVideo>{.name = "Video Out", .default_device = "crt_tv_pal"},
    Slot<PortAudioMono>{.name = "Audio Out"}
);

inline constexpr size_t kOric1ChipCount = decltype(kOric1Manifest)::chip_count;
inline constexpr size_t kOricAtmosChipCount = decltype(kOricAtmosManifest)::chip_count;
// BusTraits — selects the correct manifest per variant
template<OricVariant V> struct OricBusTraits;

template<> struct OricBusTraits<OricVariant::ORIC_1> {
    static constexpr const auto& kManifest = kOric1Manifest;
    using Spec = ManifestBusSpec<kOric1Manifest, 16, 8, 1, true>;
};

template<> struct OricBusTraits<OricVariant::ORIC_ATMOS> {
    static constexpr const auto& kManifest = kOricAtmosManifest;
    using Spec = ManifestBusSpec<kOricAtmosManifest, 16, 8, 1, true>;
};

template<typename BSpec>
struct OricBoard : Board<BSpec> {
    using ComponentTuple = decltype(kOric1Manifest)::component_tuple;
    ComponentTuple components_;

    // Chip aliases
    MOS6502&   cpu = std::get<0>(components_);
    RAMChip&   ram = std::get<1>(components_);
    mos6522_t& via = std::get<2>(components_);
    ROMChip&   rom = std::get<3>(components_);
    AY_3_8912& psg = std::get<4>(components_);

    // Port aliases
    PortCassette&       cassette_port  = std::get<5>(components_);
    PortExpansion&      expansion_port = std::get<6>(components_);
    PortCompositeVideo& video_port     = std::get<7>(components_);
    PortAudioMono&      audio_port     = std::get<8>(components_);

    template<size_t N>
    OricBoard(const ChipManifest<N>& m) : Board<BSpec>(m) {}
};
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

    void* get_video_port_ptr() override { return video_port_.get(); }

private:
    // ── Board + bus ──────────────────────────────────────────────────────
    using Bus       = MemoryBus<typename BTraits::Spec>;
    using MainBoard = OricBoard<typename BTraits::Spec>;
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
