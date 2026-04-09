#pragma once
/*
 * mc6809_vdg_system.hpp — MC6809E + MC6847 VDG Unified System
 *
 * The MC6809E + MC6847 VDG chipset was the basis for several home computers
 * that are hardware-identical save for ROM content, keyboard layout, and
 * video timing:
 *
 *   TRS-80 CoCo 1   (Tandy/Radio Shack, 1980)  — 4–64 KB, NTSC
 *   TRS-80 CoCo 2   (Tandy/Radio Shack, 1983)  — 16–64 KB, NTSC
 *   Dragon 32        (Dragon Data, 1982)         — 32 KB, PAL
 *   Dragon 64        (Dragon Data, 1983)         — 64 KB, PAL
 *
 * All variants share:
 *   MC6809E CPU, MC6847 VDG, 2× PIA 6821, MC6883 SAM
 *   Identical I/O map ($FF00/$FF20/$FFC0), identical bus decoding
 *   Identical tick loop, interrupt routing, PIA keyboard scanning
 *
 * They differ only in: ROM image names, keyboard matrix, PAL/NTSC timing
 */

#include "systems/mc6809_vdg/mc6809_vdg_constants.hpp"
#include "core/system.hpp"
#include "core/system_lines.hpp"
#include "core/board.hpp"
#include "core/signal/video_port.hpp"
#include "core/signal/audio_port.hpp"
#include "chip/cpu/mc6809/motorola_mc6809e.hpp"
#include "chip/video/mc6847/mc6847.hpp"
#include "chip/io/pia6820.hpp"
#include "chip/io/mc6883_sam.hpp"
#include "chip/memory/memory_chip.hpp"
#include "utils/ring_buffer.hpp"
#include <cstdint>
#include <memory>

// ============================================================================
// Variant enumeration — all hardware-identical MC6809E + MC6847 systems
// ============================================================================

enum class MC6809VDGVariant {
    COCO1,      // TRS-80 Color Computer (1980), 4–64 KB, NTSC
    COCO2,      // TRS-80 Color Computer 2 (1983), 16–64 KB, NTSC
    DRAGON32,   // Dragon 32 (1982), 32 KB, PAL
    DRAGON64,   // Dragon 64 (1983), 64 KB, PAL
};

// ============================================================================
// Variant traits — compile-time properties per variant
// ============================================================================

template<MC6809VDGVariant V> struct MC6809VDGVariantTraits;

template<> struct MC6809VDGVariantTraits<MC6809VDGVariant::COCO1> {
    static constexpr uint32_t       ram_size         = 0x8000;    // 32 KB
    static constexpr VideoStandard  video_std        = VideoStandard::NTSC;
    static constexpr int            target_fps       = 60;
    static constexpr uint32_t       cycles_per_frame = mc6809_vdg::CYCLES_PER_FRAME_NTSC;
    static constexpr const char*    ext_basic_roms   = "extbas11.rom|extbasic.rom|EXTBASIC.ROM";
    static constexpr const char*    basic_roms       = "bas13.rom|bas12.rom|color.rom|BASIC.ROM";
    static constexpr const char*    rom_folder       = "coco";
    static constexpr const char*    log_prefix       = "CoCo";
};

template<> struct MC6809VDGVariantTraits<MC6809VDGVariant::COCO2> {
    static constexpr uint32_t       ram_size         = 0x10000;   // 64 KB
    static constexpr VideoStandard  video_std        = VideoStandard::NTSC;
    static constexpr int            target_fps       = 60;
    static constexpr uint32_t       cycles_per_frame = mc6809_vdg::CYCLES_PER_FRAME_NTSC;
    static constexpr const char*    ext_basic_roms   = "extbas11.rom|extbasic.rom|EXTBASIC.ROM";
    static constexpr const char*    basic_roms       = "bas13.rom|bas12.rom|color.rom|BASIC.ROM";
    static constexpr const char*    rom_folder       = "coco";
    static constexpr const char*    log_prefix       = "CoCo";
};

template<> struct MC6809VDGVariantTraits<MC6809VDGVariant::DRAGON32> {
    static constexpr uint32_t       ram_size         = 0x8000;    // 32 KB
    static constexpr VideoStandard  video_std        = VideoStandard::PAL;
    static constexpr int            target_fps       = 50;
    static constexpr uint32_t       cycles_per_frame = mc6809_vdg::CYCLES_PER_FRAME_PAL;
    static constexpr const char*    ext_basic_roms   = "d64extbas.rom|dragon64_extbasic.rom";
    static constexpr const char*    basic_roms       = "d32.rom|d64_1.rom|dragon32.rom|dragon64.rom";
    static constexpr const char*    rom_folder       = "dragon";
    static constexpr const char*    log_prefix       = "Dragon";
};

template<> struct MC6809VDGVariantTraits<MC6809VDGVariant::DRAGON64> {
    static constexpr uint32_t       ram_size         = 0x10000;   // 64 KB
    static constexpr VideoStandard  video_std        = VideoStandard::PAL;
    static constexpr int            target_fps       = 50;
    static constexpr uint32_t       cycles_per_frame = mc6809_vdg::CYCLES_PER_FRAME_PAL;
    static constexpr const char*    ext_basic_roms   = "d64extbas.rom|dragon64_extbasic.rom";
    static constexpr const char*    basic_roms       = "d32.rom|d64_1.rom|dragon32.rom|dragon64.rom";
    static constexpr const char*    rom_folder       = "dragon";
    static constexpr const char*    log_prefix       = "Dragon";
};

// ── Convenience helpers ─────────────────────────────────────────────────

template<MC6809VDGVariant V>
inline constexpr bool is_coco_variant =
    (V == MC6809VDGVariant::COCO1 || V == MC6809VDGVariant::COCO2);

template<MC6809VDGVariant V>
inline constexpr bool is_dragon_variant =
    (V == MC6809VDGVariant::DRAGON32 || V == MC6809VDGVariant::DRAGON64);

// ============================================================================
// Default bus state (MC6809E)
// ============================================================================

#define MC6809_VDG_BUS_DEFAULT_STATE (MotorolaMC6809E::default_bus_state())

// ============================================================================
// Hardware traits factory — used by per-variant registration files
// ============================================================================

template<MC6809VDGVariant V>
inline HardwareTraits create_mc6809_vdg_hardware_traits() {
    using VT = MC6809VDGVariantTraits<V>;
    HardwareTraits traits = {};

    traits.display.native_width    = mc6809_vdg::DISPLAY_WIDTH;
    traits.display.native_height   = mc6809_vdg::DISPLAY_HEIGHT;
    traits.display.visible_width   = mc6809_vdg::DISPLAY_WIDTH;
    traits.display.visible_height  = mc6809_vdg::DISPLAY_HEIGHT;
    traits.display.format          = FramebufferFormat::RGBA8888;
    traits.display.palette_size    = mc6847_font::PALETTE_SIZE;

    traits.audio.format            = AudioFormat::CUSTOM;
    traits.audio.sample_rate_hz    = mc6809_vdg::DEFAULT_SAMPLE_RATE;
    traits.audio.channels          = 1;
    traits.audio.chip_name         = "1-bit DAC";

    traits.timing.cpu_frequency_hz   = mc6809_vdg::CPU_FREQ_HZ;
    traits.timing.target_fps         = VT::target_fps;
    traits.timing.cycles_per_frame   = VT::cycles_per_frame;
    traits.timing.standard           = VT::video_std;

    return traits;
}

// ============================================================================
// Manifest — identical board layout, ROM names selected per variant
// ============================================================================

template<MC6809VDGVariant V>
inline constexpr auto kMC6809VDGManifest = make_manifest(
    // ── CPU ──────────────────────────────────────────────────────────
    Slot<MotorolaMC6809E>{.base_addr = 0x0000, .label = "MC6809E"},

    // ── Memory ───────────────────────────────────────────────────────
    Slot<RAMChip>{.base_addr = 0x0000,
                  .size_bytes = MC6809VDGVariantTraits<V>::ram_size,
                  .label = "RAM"},
    Slot<ROMChip>{.base_addr = 0x8000, .size_bytes = 0x2000,
                  .label = "Extended BASIC ROM",
                  .overlay_group = 1,
                  .rom = {MC6809VDGVariantTraits<V>::ext_basic_roms}},
    Slot<ROMChip>{.base_addr = 0xA000, .size_bytes = 0x2000,
                  .label = "BASIC ROM",
                  .rom = {MC6809VDGVariantTraits<V>::basic_roms}},
    Slot<ROMChip>{.base_addr = 0xC000, .size_bytes = 0x4000,
                  .label = "Cartridge ROM"},

    // ── I/O chips ────────────────────────────────────────────────────
    Slot<pia6820_t>{.base_addr = mc6809_vdg::PIA0_BASE,
                    .addr_mask = 0x0003,
                    .label = "PIA 0 (Keyboard/Joystick)"},
    Slot<pia6820_t>{.base_addr = mc6809_vdg::PIA1_BASE,
                    .addr_mask = 0x0003,
                    .label = "PIA 1 (VDG/Sound/Cassette)"},
    Slot<mc6883_sam_t>{.base_addr = mc6809_vdg::SAM_BASE,
                       .addr_mask = 0x001F,
                       .label = "MC6883 SAM"},
    Slot<mc6847_t>{.base_addr = 0x0000,
                   .label = "MC6847 VDG"},

    // ── Ports ────────────────────────────────────────────────────────
    Slot<PortControlDB9>{.name = "Joystick Port 1", .port_number = 1, .default_device = "joystick"},
    Slot<PortControlDB9>{.name = "Joystick Port 2", .port_number = 2},
    Slot<PortCassette>{.name = "Cassette"},
    Slot<PortExpansion>{.name = "Cartridge Slot"},
    Slot<PortCompositeVideo>{.name = "Video Out", .default_device = "crt_tv"},
    Slot<PortAudioMono>{.name = "Audio Out"}
);

template<MC6809VDGVariant V>
using MC6809VDGBusSpec = ManifestBusSpec<kMC6809VDGManifest<V>, 16, 8>;

// ============================================================================
// Board — identical component layout across all variants
// ============================================================================

template<MC6809VDGVariant V>
struct MC6809VDGBoard : Board<MC6809VDGBusSpec<V>> {
    using ComponentTuple = typename decltype(kMC6809VDGManifest<V>)::component_tuple;
    ComponentTuple components_;

    // Chip aliases
    MotorolaMC6809E& cpu       = std::get<0>(components_);
    RAMChip&         ram       = std::get<1>(components_);
    ROMChip&         ext_basic = std::get<2>(components_);
    ROMChip&         basic_rom = std::get<3>(components_);
    ROMChip&         cart      = std::get<4>(components_);
    pia6820_t&       pia0      = std::get<5>(components_);
    pia6820_t&       pia1      = std::get<6>(components_);
    mc6883_sam_t&    sam       = std::get<7>(components_);
    mc6847_t&        vdg       = std::get<8>(components_);

    // Port aliases
    PortControlDB9&     joy1_port  = std::get<9>(components_);
    PortControlDB9&     joy2_port  = std::get<10>(components_);
    PortCassette&       cassette   = std::get<11>(components_);
    PortExpansion&      cart_port  = std::get<12>(components_);
    PortCompositeVideo& video_port = std::get<13>(components_);
    PortAudioMono&      audio_port = std::get<14>(components_);

    MC6809VDGBoard() : Board<MC6809VDGBusSpec<V>>(kMC6809VDGManifest<V>) {}
};

// ============================================================================
// System — unified implementation for all MC6809E + MC6847 variants
// ============================================================================

template<MC6809VDGVariant V>
class MC6809VDGSystem : public System {
    using Traits = MC6809VDGVariantTraits<V>;

public:
    MC6809VDGSystem();
    ~MC6809VDGSystem() override;

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
    // ── Board + bus ──────────────────────────────────────────────────
    using Bus       = MemoryBus<MC6809VDGBusSpec<V>>;
    using PT        = PackingTraits<MC6809VDGBusSpec<V>>;
    using MainBoard = MC6809VDGBoard<V>;
    Bus       bus_;
    MainBoard board_;

    // ── Video ────────────────────────────────────────────────────────
    std::unique_ptr<CompositeVideoPort> video_port_;

    // ── Audio ────────────────────────────────────────────────────────
    uint32_t audio_sample_rate_ = mc6809_vdg::DEFAULT_SAMPLE_RATE;
    std::unique_ptr<AudioPort> audio_port_;

    // ── Keyboard matrix ──────────────────────────────────────────────
    uint8_t keyboard_matrix_[8] = {};   // 8 rows × 7 columns (active low)

    // ── System state ─────────────────────────────────────────────────
    bus_state_t pins_                = MC6809_VDG_BUS_DEFAULT_STATE;
    uint32_t    frame_cycle_counter_ = 0;

    // ── Internal helpers ─────────────────────────────────────────────
    void configure_bus_memory_map();
    void wire_pia_callbacks();
    bool load_roms();
};
