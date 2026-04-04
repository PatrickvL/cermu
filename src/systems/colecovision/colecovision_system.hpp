#pragma once
/*
 * colecovision_system.hpp — ColecoVision Emulated System
 *
 * The ColecoVision (1982) was a home video game console by Coleco.
 * It shared the same TMS9918A+SN76489 chipset as the SG-1000 and MSX1,
 * making it architecturally similar but with different I/O decoding
 * and a unique controller design (12-key keypad + joystick).
 */

#include "systems/colecovision/colecovision_constants.hpp"
#include "core/system.hpp"
#include "core/system_lines.hpp"
#include "core/board.hpp"
#include "core/signal/video_port.hpp"
#include "core/signal/audio_port.hpp"
#include "chip/cpu/z80/zilog_z80a.hpp"
#include "chip/video/tms9918/tms9918a.hpp"
#include "chip/sound/sn76489/sn76489.hpp"
#include "chip/memory/memory_chip.hpp"
#include "utils/ring_buffer.hpp"
#include <cstdint>
#include <memory>

// ============================================================================
// ColecoVision default bus state
// ============================================================================

#define COLECO_BUS_DEFAULT_STATE (ZilogZ80A::default_bus_state())

inline constexpr auto kColecoManifest = make_manifest(
    // Chips
    Slot<ZilogZ80A>{.base_addr = 0x0000, .label = "Z80A"},
    Slot<ROMChip>{.base_addr = 0x0000, .size_bytes = 0x2000, .label = "BIOS ROM", .rom = {"coleco.rom|colecovision.rom|COLECO.ROM"}},
    Slot<RAMChip>{.base_addr = 0x6000, .size_bytes = 0x0400, .label = "RAM"},
    Slot<ROMChip>{.base_addr = 0x8000, .size_bytes = 0x8000, .label = "Cart ROM"},
    Slot<TMS9918A>{.base_addr = 0x00BE, .label = "TMS9918A"},
    Slot<sn76489_t>{.base_addr = 0x00FF, .label = "SN76489"},
    // Ports
    Slot<PortControlDB9>{.name = "Controller Port 1", .port_number = 1, .default_device = "joystick"},
    Slot<PortControlDB9>{.name = "Controller Port 2", .port_number = 2, .default_device = "joystick"},
    Slot<PortExpansion>{.name = "Cartridge Slot"},
    Slot<PortCompositeVideo>{.name = "Video Out", .default_device = "crt_tv"},
    Slot<PortAudioMono>{.name = "Audio Out"}
);

inline constexpr size_t kColecoChipCount = decltype(kColecoManifest)::chip_count;
using ColecoBusSpec = ManifestBusSpec<kColecoManifest, 16, 8>;

struct ColecoBoard : Board<ColecoBusSpec, NoChips> {
    using ComponentTuple = decltype(kColecoManifest)::component_tuple;
    ComponentTuple components_;

    // Chip aliases
    ZilogZ80A& z80  = std::get<0>(components_);
    ROMChip&   bios = std::get<1>(components_);
    RAMChip&   ram  = std::get<2>(components_);
    ROMChip&   cart = std::get<3>(components_);
    TMS9918A&  vdp  = std::get<4>(components_);
    sn76489_t& psg  = std::get<5>(components_);

    // Port aliases
    PortControlDB9&     ctrl1_port     = std::get<6>(components_);
    PortControlDB9&     ctrl2_port     = std::get<7>(components_);
    PortExpansion&      cartridge_port = std::get<8>(components_);
    PortCompositeVideo& video_port     = std::get<9>(components_);
    PortAudioMono&      audio_port     = std::get<10>(components_);

    ColecoBoard() : Board(kColecoManifest) {}
};
// ============================================================================
// ColecoVision System
// ============================================================================

class ColecoVisionSystem : public System {
public:
    ColecoVisionSystem();
    ~ColecoVisionSystem() override;

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
    // ── Board + bus (chips live inside board_) ────────────────────────────
    using Bus       = MemoryBus<ColecoBusSpec>;
    using PT        = PackingTraits<ColecoBusSpec>;
    using MainBoard = ColecoBoard;
    Bus       bus_;
    MainBoard board_;

    // ── Video ────────────────────────────────────────────────────────────
    std::unique_ptr<CompositeVideoPort> video_port_;

    // ── Audio ────────────────────────────────────────────────────────────
    uint32_t audio_sample_rate_ = coleco_constants::DEFAULT_SAMPLE_RATE;
    std::unique_ptr<AudioPort> audio_port_;

    // ── Controllers ──────────────────────────────────────────────────────
    uint8_t ctrl_mode_ = 0;          // 0 or 1, selected by I/O write
    uint8_t ctrl1_joystick_ = 0x7F;  // Joystick state (active-low)
    uint8_t ctrl1_keypad_   = 0x0F;  // Keypad state

    // ── System state ─────────────────────────────────────────────────────
    bus_state_t pins_         = COLECO_BUS_DEFAULT_STATE;
    uint32_t    frame_tstate_counter_ = 0;

    // ── Internal helpers ─────────────────────────────────────────────────
    void configure_bus_memory_map();
    bus_state_t io_tick(bus_state_t pins);
    bool load_roms();
};
