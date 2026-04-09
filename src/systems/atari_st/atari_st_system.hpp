#pragma once
/*
 * atari_st_system.hpp — Atari ST / STe / Mega ST Emulated System
 *
 * The Atari ST (1985) was Atari's 16-bit home computer, competing with
 * the Amiga.  It uses a Motorola 68000 CPU with a custom chipset providing
 * palette-based graphics, YM2149 sound, and integrated floppy support.
 *
 * Variants:
 *   ST     (1985): 512KB–1MB RAM, TOS 1.0x–1.04, 192KB ROM
 *   STE    (1989): Enhanced Shifter (4096 colors), DMA sound, blitter
 *   MEGA_ST (1987): Same as ST, desktop form factor, blitter, RTC
 *
 * The MC68000 uses memory-mapped I/O exclusively (no port instructions).
 * The GLUE chip does address decoding — we implement it as system-level
 * dispatch in the tick loop.
 */

#include "systems/atari_st/atari_st_constants.hpp"
#include "core/system.hpp"
#include "core/system_lines.hpp"
#include "core/board.hpp"
#include "core/signal/video_port.hpp"
#include "core/signal/audio_port.hpp"
#include "chip/cpu/m680x0/mc68000.hpp"
#include "chip/video/st_shifter/st_shifter.hpp"
#include "chip/sound/ay_psg/ym2149.hpp"
#include "chip/io/mk68901/mk68901.hpp"
#include "chip/storage/wd1772/wd1772.hpp"
#include "chip/memory/memory_chip.hpp"
#include "utils/ring_buffer.hpp"
#include <cstdint>
#include <memory>

enum class AtariSTVariant { ST, STE, MEGA_ST };

// ============================================================================
// Atari ST default bus state
// ============================================================================

#define ATARI_ST_BUS_DEFAULT_STATE (MC68000::default_bus_state())

// ============================================================================
// Atari ST Manifest
// ============================================================================

template<AtariSTVariant V>
inline constexpr auto kAtariSTManifest = make_manifest(
    // Chips
    Slot<MC68000>{.base_addr = 0x000000, .label = "MC68000"},
    Slot<RAMChip>{.base_addr = 0x000000,
                  .size_bytes = (V == AtariSTVariant::ST) ?
                      atari_st_constants::RAM_SIZE_512K :
                      atari_st_constants::RAM_SIZE_1M,
                  .label = "Main RAM"},
    Slot<ROMChip>{.base_addr = atari_st_constants::TOS_ROM_BASE,
                  .size_bytes = atari_st_constants::TOS_ROM_SIZE,
                  .label = "TOS ROM",
                  .rom = {"tos.img|tos104.img|tos102.img|TOS.IMG"}},
    Slot<ROMChip>{.base_addr = atari_st_constants::CART_ROM_BASE,
                  .size_bytes = atari_st_constants::CART_ROM_SIZE,
                  .label = "Cartridge ROM"},
    Slot<st_shifter_t>{.base_addr = atari_st_constants::SHIFTER_BASE,
                       .addr_mask = 0x007F,
                       .label = "Shifter"},
    Slot<YM2149>{.base_addr = atari_st_constants::PSG_BASE,
                 .addr_mask = 0x0003,
                 .label = "YM2149 PSG"},
    Slot<mk68901_t>{.base_addr = atari_st_constants::MFP_BASE,
                    .addr_mask = 0x003F,
                    .label = "MK68901 MFP"},
    Slot<wd1772_t>{.base_addr = atari_st_constants::FDC_BASE,
                   .addr_mask = 0x0007,
                   .label = "WD1772 FDC"},
    // Ports
    Slot<PortControlDB9>{.name = "Joystick Port 1", .port_number = 1,
                         .default_device = "joystick"},
    Slot<PortControlDB9>{.name = "Joystick Port 2", .port_number = 2,
                         .default_device = "mouse"},
    Slot<PortExpansion>{.name = "Cartridge Port"},
    Slot<PortCompositeVideo>{.name = "Monitor Port", .default_device = "crt_tv"},
    Slot<PortAudioMono>{.name = "Audio Out"}
);

template<AtariSTVariant V>
using AtariSTBusSpec = ManifestBusSpec<kAtariSTManifest<V>, 24, 16>;

template<AtariSTVariant V>
struct AtariSTBoard : Board<AtariSTBusSpec<V>> {
    using ComponentTuple = typename decltype(kAtariSTManifest<V>)::component_tuple;
    ComponentTuple components_;

    // Chip aliases
    MC68000&       cpu      = std::get<0>(components_);
    RAMChip&       ram      = std::get<1>(components_);
    ROMChip&       tos_rom  = std::get<2>(components_);
    ROMChip&       cart_rom = std::get<3>(components_);
    st_shifter_t&  shifter  = std::get<4>(components_);
    YM2149&        psg      = std::get<5>(components_);
    mk68901_t&     mfp      = std::get<6>(components_);
    wd1772_t&      fdc      = std::get<7>(components_);

    // Port aliases
    PortControlDB9&     joy1_port  = std::get<8>(components_);
    PortControlDB9&     joy2_port  = std::get<9>(components_);
    PortExpansion&      cart_port  = std::get<10>(components_);
    PortCompositeVideo& video_port = std::get<11>(components_);
    PortAudioMono&      audio_port = std::get<12>(components_);

    AtariSTBoard() : Board<AtariSTBusSpec<V>>(kAtariSTManifest<V>) {}
};

// ============================================================================
// Atari ST System
// ============================================================================

template<AtariSTVariant V>
class AtariSTSystem : public System {
public:
    AtariSTSystem();
    ~AtariSTSystem() override;

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
    AtariSTBoard<V> board_;

    std::unique_ptr<CompositeVideoPort> video_port_;
    uint32_t audio_sample_rate_ = atari_st_constants::DEFAULT_SAMPLE_RATE;
    std::unique_ptr<AudioPort> audio_port_;

    // ACIA keyboard state (stub: just track key events)
    uint8_t acia_kbd_status_ = 0x02;   // TX empty by default
    uint8_t acia_kbd_data_ = 0;
    uint8_t acia_midi_status_ = 0x02;
    uint8_t acia_midi_data_ = 0;

    bus_state_t m68k_pins_ = ATARI_ST_BUS_DEFAULT_STATE;
    uint32_t frame_counter_ = 0;

    // MC68000 memory callback bridge
    static uint16_t mem_read(void* ctx, uint32_t addr);
    static void mem_write(void* ctx, uint32_t addr, uint16_t data);

    uint8_t read_byte(uint32_t addr) noexcept;
    void write_byte(uint32_t addr, uint8_t data) noexcept;
    bool load_roms();
};
