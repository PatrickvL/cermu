#pragma once
/*
 * atari8_system.hpp — Atari 400/800/XL/XE Emulated System
 *
 * The Atari 8-bit family (1979–1992) used a custom chipset: ANTIC (display
 * list coprocessor), GTIA (color/sprite), and POKEY (sound/I/O), with a
 * PIA for joystick/memory banking.
 *
 * Variants:
 *   Atari 800   (1979): 48KB RAM, 2 cartridge slots, 4 joystick ports
 *   Atari 800XL (1983): 64KB RAM, 1 cartridge slot, PBI expansion
 *   Atari 130XE (1985): 128KB RAM (banked), 1 cartridge slot
 */

#include "systems/atari8/atari8_constants.hpp"
#include "core/system.hpp"
#include "core/system_lines.hpp"
#include "core/board.hpp"
#include "core/signal/video_port.hpp"
#include "core/signal/audio_port.hpp"
#include "chip/cpu/fam65xx/mos6502.hpp"
#include "chip/video/antic/antic.hpp"
#include "chip/video/gtia/gtia.hpp"
#include "chip/sound/pokey/c012294.hpp"
#include "chip/io/pia6820.hpp"
#include "chip/memory/memory_chip.hpp"
#include "utils/ring_buffer.hpp"
#include <cstdint>
#include <memory>

enum class Atari8Variant { A800, A800XL, A130XE };

#define ATARI8_BUS_DEFAULT_STATE (MOS6502::default_bus_state())

// ============================================================================
// Atari 8-bit Manifest
// ============================================================================

template<Atari8Variant V>
inline constexpr auto kAtari8Manifest = make_manifest(
    // ── CPU ──────────────────────────────────────────────────────────
    Slot<MOS6502>{.base_addr = 0x0000, .label = "6502C"},

    // ── Memory ───────────────────────────────────────────────────────
    Slot<RAMChip>{.base_addr = 0x0000,
                  .size_bytes = (V == Atari8Variant::A130XE) ? 0x00020000u
                               : (V == Atari8Variant::A800XL) ? 0x00010000u
                               : 0x0000C000u,
                  .label = "RAM"},
    Slot<ROMChip>{.base_addr = 0xA000, .size_bytes = 0x2000,
                  .label = "BASIC ROM",
                  .overlay_group = 1,
                  .rom = {"ataribas.rom|BASIC.ROM|ataribasic.rom"}},
    Slot<ROMChip>{.base_addr = 0xD800, .size_bytes = 0x2800,
                  .label = "OS ROM",
                  .rom = {"atarixl.rom|atariosb.rom|ATARIXL.ROM|ATARIOSB.ROM"}},
    Slot<ROMChip>{.base_addr = 0x8000, .size_bytes = 0x4000,
                  .label = "Cartridge ROM"},

    // ── I/O chips ────────────────────────────────────────────────────
    Slot<antic_t>{.base_addr = atari8_constants::ANTIC_BASE,
                  .addr_mask = 0x000F,
                  .label = "ANTIC"},
    Slot<gtia_t>{.base_addr = atari8_constants::GTIA_BASE,
                 .addr_mask = 0x001F,
                 .label = "GTIA"},
    Slot<AtariPOKEY>{.base_addr = atari8_constants::POKEY_BASE,
                     .addr_mask = 0x000F,
                     .label = "POKEY"},
    Slot<pia6820_t>{.base_addr = atari8_constants::PIA_BASE,
                    .addr_mask = 0x0003,
                    .label = "PIA (Joystick/Banking)"},

    // ── Ports ────────────────────────────────────────────────────────
    Slot<PortControlDB9>{.name = "Controller Port 1", .port_number = 1, .default_device = "joystick"},
    Slot<PortControlDB9>{.name = "Controller Port 2", .port_number = 2, .default_device = "joystick"},
    Slot<PortCassette>{.name = "SIO/Cassette"},
    Slot<PortExpansion>{.name = "Cartridge Slot"},
    Slot<PortCompositeVideo>{.name = "Video Out", .default_device = "crt_tv"},
    Slot<PortAudioMono>{.name = "Audio Out"}
);

template<Atari8Variant V>
using Atari8BusSpec = ManifestBusSpec<kAtari8Manifest<V>, 16, 8>;

// ============================================================================
// Atari 8-bit Board
// ============================================================================

template<Atari8Variant V>
struct Atari8Board : Board<Atari8BusSpec<V>> {
    using ComponentTuple = typename decltype(kAtari8Manifest<V>)::component_tuple;
    ComponentTuple components_;

    MOS6502&     cpu     = std::get<0>(components_);
    RAMChip&     ram     = std::get<1>(components_);
    ROMChip&     basic   = std::get<2>(components_);
    ROMChip&     os_rom  = std::get<3>(components_);
    ROMChip&     cart    = std::get<4>(components_);
    antic_t&     antic   = std::get<5>(components_);
    gtia_t&      gtia    = std::get<6>(components_);
    AtariPOKEY&   pokey   = std::get<7>(components_);
    pia6820_t&   pia     = std::get<8>(components_);

    PortControlDB9&     joy1_port  = std::get<9>(components_);
    PortControlDB9&     joy2_port  = std::get<10>(components_);
    PortCassette&       cassette   = std::get<11>(components_);
    PortExpansion&      cart_port  = std::get<12>(components_);
    PortCompositeVideo& video_port = std::get<13>(components_);
    PortAudioMono&      audio_port = std::get<14>(components_);

    Atari8Board() : Board<Atari8BusSpec<V>>(kAtari8Manifest<V>) {}
};

// ============================================================================
// Atari 8-bit System
// ============================================================================

template<Atari8Variant V>
class Atari8System : public System {
public:
    Atari8System();
    ~Atari8System() override;

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
    using Bus       = MemoryBus<Atari8BusSpec<V>>;
    using PT        = PackingTraits<Atari8BusSpec<V>>;
    using MainBoard = Atari8Board<V>;
    Bus       bus_;
    MainBoard board_;

    std::unique_ptr<CompositeVideoPort> video_port_;
    uint32_t audio_sample_rate_ = atari8_constants::DEFAULT_SAMPLE_RATE;
    std::unique_ptr<AudioPort> audio_port_;

    bus_state_t pins_                = ATARI8_BUS_DEFAULT_STATE;
    uint32_t    frame_cycle_counter_ = 0;

    void configure_bus_memory_map();
    bool load_roms();
};
