#pragma once
/*
 * genesis_system.hpp — Sega Genesis / Mega Drive Emulated System
 *
 * The Sega Genesis (1988) uses a dual-CPU architecture:
 *   Main CPU: Motorola 68000 @ 7.67 MHz (NTSC) / 7.60 MHz (PAL)
 *   Sub CPU:  Zilog Z80 @ 3.58 MHz (sound CPU, optional 68K bus access)
 *
 * Chips:
 *   VDP: Yamaha YM7101 (315-5313) — tile/sprite engine with DMA
 *   FM:  Yamaha YM2612 (OPN2) — 6-channel FM synthesis
 *   PSG: SN76489 — 3+1 channel PSG (Master System compatible)
 *   I/O: Controller ports (active-low DB-9 style, active select multiplexing)
 *
 * Memory map (68000):
 *   $000000–$3FFFFF : Cartridge ROM (up to 4MB)
 *   $A00000–$A0FFFF : Z80 address space (bank window when Z80 has bus)
 *   $A10000–$A1001F : I/O area (controller, TMSS, version)
 *   $A11100–$A11101 : Z80 bus request
 *   $A11200–$A11201 : Z80 reset
 *   $C00000–$C0001F : VDP (data, control, HV counter)
 *   $FF0000–$FFFFFF : 64KB Main RAM
 */

#include "core/system.hpp"
#include "core/system_lines.hpp"
#include "core/board.hpp"
#include "core/signal/video_port.hpp"
#include "core/signal/audio_port.hpp"
#include "chip/cpu/m680x0/mc68000.hpp"
#include "chip/cpu/z80/zilog_z80a.hpp"
#include "chip/video/genesis_vdp/genesis_vdp.hpp"
#include "chip/sound/ym_fm/ym2612.hpp"
#include "chip/sound/sn76489/sn76489.hpp"
#include "chip/memory/memory_chip.hpp"
#include "utils/ring_buffer.hpp"
#include <cstdint>
#include <memory>

enum class GenesisVariant { GENESIS, MEGADRIVE };

namespace genesis_constants {
    inline constexpr uint32_t M68K_FREQ_NTSC       = 7670454;
    inline constexpr uint32_t M68K_FREQ_PAL        = 7600489;
    inline constexpr uint32_t Z80_FREQ             = 3579545;

    inline constexpr int DISPLAY_WIDTH             = 320;
    inline constexpr int DISPLAY_HEIGHT            = 224;
    inline constexpr int DISPLAY_HEIGHT_PAL        = 240;

    // NTSC: 262 lines × 3420 dot clocks / 7 ≈ 127580 68K cycles/frame
    inline constexpr uint32_t CYCLES_PER_FRAME_NTSC = 127580;
    inline constexpr uint32_t CYCLES_PER_FRAME_PAL  = 152720;

    inline constexpr uint32_t MAIN_RAM_SIZE        = 65536;    // 64KB
    inline constexpr uint32_t Z80_RAM_SIZE         = 8192;     // 8KB
    inline constexpr uint32_t MAX_ROM_SIZE         = 0x400000; // 4MB

    inline constexpr int TARGET_FPS_NTSC           = 60;
    inline constexpr int TARGET_FPS_PAL            = 50;
    inline constexpr int DEFAULT_SAMPLE_RATE       = 44100;

    // I/O registers
    inline constexpr uint32_t IO_VERSION           = 0xA10001;
    inline constexpr uint32_t IO_DATA1             = 0xA10003;
    inline constexpr uint32_t IO_DATA2             = 0xA10005;
    inline constexpr uint32_t IO_CTRL1             = 0xA10009;
    inline constexpr uint32_t IO_CTRL2             = 0xA1000B;
    inline constexpr uint32_t Z80_BUS_REQ          = 0xA11100;
    inline constexpr uint32_t Z80_RESET            = 0xA11200;
}

// Note: Genesis uses 24-bit address bus, but we map it into 16-bit segments
// for the manifest system. The 68K address decoding is done in the system tick.

#define GENESIS_BUS_DEFAULT_STATE (MC68000::default_bus_state())

// Genesis doesn't fit neatly into the 16-bit manifest pattern because of
// its 24-bit address space. We use a minimal manifest for chip registration
// and do address decoding manually in the tick loop.

inline constexpr auto kGenesisManifest = make_manifest(
    Slot<MC68000>{.base_addr = 0x0000, .label = "MC68000"},
    Slot<ZilogZ80A>{.base_addr = 0x0000, .label = "Z80 (Sound CPU)"},
    Slot<RAMChip>{.base_addr = 0x0000, .size_bytes = genesis_constants::MAIN_RAM_SIZE,
                  .label = "Main RAM (64KB)"},
    Slot<RAMChip>{.base_addr = 0x0000, .size_bytes = genesis_constants::Z80_RAM_SIZE,
                  .label = "Z80 RAM (8KB)"},
    Slot<ROMChip>{.base_addr = 0x0000, .size_bytes = genesis_constants::MAX_ROM_SIZE,
                  .label = "Cartridge ROM"},
    Slot<genesis_vdp_t>{.base_addr = 0xC000, .addr_mask = 0x001F,
                        .label = "VDP (315-5313)"},
    Slot<YM2612>{.base_addr = 0xA040, .addr_mask = 0x0003,
                   .label = "YM2612"},
    Slot<sn76489_t>{.base_addr = 0x0000, .label = "SN76489 PSG"},

    // Ports
    Slot<PortControlDB9>{.name = "Controller Port 1", .port_number = 1, .default_device = "genesis_pad"},
    Slot<PortControlDB9>{.name = "Controller Port 2", .port_number = 2, .default_device = "genesis_pad"},
    Slot<PortExpansion>{.name = "Cartridge Slot"},
    Slot<PortCompositeVideo>{.name = "Video Out", .default_device = "crt_tv"},
    Slot<PortAudioStereo>{.name = "Audio Out"}
);

using GenesisBusSpec = ManifestBusSpec<kGenesisManifest, 16, 8>;

struct GenesisBoard : Board<GenesisBusSpec> {
    using ComponentTuple = decltype(kGenesisManifest)::component_tuple;
    ComponentTuple components_;

    MC68000&        m68k     = std::get<0>(components_);
    ZilogZ80A&      z80      = std::get<1>(components_);
    RAMChip&        main_ram = std::get<2>(components_);
    RAMChip&        z80_ram  = std::get<3>(components_);
    ROMChip&        rom      = std::get<4>(components_);
    genesis_vdp_t&  vdp      = std::get<5>(components_);
    YM2612&         fm       = std::get<6>(components_);
    sn76489_t&      psg      = std::get<7>(components_);

    PortControlDB9&  ctrl1_port = std::get<8>(components_);
    PortControlDB9&  ctrl2_port = std::get<9>(components_);
    PortExpansion&   cart_port  = std::get<10>(components_);
    PortCompositeVideo& video_port = std::get<11>(components_);
    PortAudioStereo& audio_port = std::get<12>(components_);

    GenesisBoard() : Board(kGenesisManifest) {}
};

// ============================================================================
// Genesis System
// ============================================================================

template<GenesisVariant V>
class GenesisSystem : public System {
public:
    GenesisSystem();
    ~GenesisSystem() override;

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
    GenesisBoard board_;

    std::unique_ptr<CompositeVideoPort> video_port_;
    uint32_t audio_sample_rate_ = genesis_constants::DEFAULT_SAMPLE_RATE;
    std::unique_ptr<AudioPort> audio_port_;

    // Controller I/O (active-low, active select multiplexing)
    uint8_t io_data_[3]   = {};   // Data registers
    uint8_t io_ctrl_[3]   = {};   // Control (direction) registers

    // Z80 bus control
    bool z80_bus_granted_ = false;
    bool z80_reset_       = true;

    bus_state_t m68k_pins_ = GENESIS_BUS_DEFAULT_STATE;
    bus_state_t z80_pins_  = 0;
    uint32_t    frame_counter_ = 0;

    // 68K-to-peripherals dispatch
    bus_state_t m68k_bus_dispatch(bus_state_t pins);
    bool load_roms();
};
