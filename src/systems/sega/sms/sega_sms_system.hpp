#pragma once
/*
 * sega_sms_system.hpp — Sega Master System Emulated System
 *
 * The Sega Master System (1986) uses a Sega-customized TMS9918-derived VDP
 * (315-5124) with Mode 4 tile engine, integrated SN76489 PSG, and a Z80A CPU.
 *
 * The SMS has a Sega mapper for cartridge banking, controlled by writes
 * to memory addresses $FFFC-$FFFF.
 */

#include "systems/sega/sms/sega_sms_constants.hpp"
#include "core/system.hpp"
#include "core/system_lines.hpp"
#include "core/board.hpp"
#include "core/signal/video_port.hpp"
#include "core/signal/audio_port.hpp"
#include "chip/cpu/z80/zilog_z80a.hpp"
#include "chip/video/tms9918/sega_315_5124.hpp"
#include "chip/sound/sn76489/sn76489.hpp"
#include "chip/memory/memory_chip.hpp"
#include "utils/ring_buffer.hpp"
#include <cstdint>
#include <memory>

// ============================================================================
// SMS default bus state
// ============================================================================

#define SMS_BUS_DEFAULT_STATE (ZilogZ80A::default_bus_state())

// =============================================================================
// SMS chip manifest
// =============================================================================
//
// Slot 0: Cartridge ROM — up to 512KB at $0000 (banked, 16KB pages)
// Slot 1: System RAM   — 8KB at $C000 (mirrored to $E000)
//
inline constexpr auto kSMSManifest = make_manifest(
    // Chips
    Slot<ZilogZ80A>{.label = "Z80A"},
    Slot<ROMChip>{.base_addr = 0x0000, .size_bytes = 0x00080000, .label = "Cartridge ROM", .bank_size = 16384},
    Slot<RAMChip>{.base_addr = 0xC000, .size_bytes = 0x2000, .addr_mask = 0x1FFF, .label = "System RAM"},
    Slot<SEGA_315_5124>{.base_addr = 0x00BE, .addr_mask = 0x00FE, .label = "315-5124 VDP"},
    Slot<sn76489_t>{.base_addr = 0x007E, .addr_mask = 0x00FE, .label = "SN76489 PSG"},
    // Ports
    Slot<PortControlDB9>{.name = "Controller Port 1", .port_number = 1, .default_device = "joystick"},
    Slot<PortControlDB9>{.name = "Controller Port 2", .port_number = 2, .default_device = "joystick"},
    Slot<PortExpansion>{.name = "Cartridge Slot"},
    Slot<PortCompositeVideo>{.name = "Video Out", .default_device = "crt_tv"},
    Slot<PortAudioMono>{.name = "Audio Out"}
);

inline constexpr size_t kSMSChipCount = decltype(kSMSManifest)::chip_count;

using SMSBusSpec  = ManifestBusSpec<kSMSManifest, 16, 8>;

struct SMSBoard : Board<SMSBusSpec> {
    using ComponentTuple = decltype(kSMSManifest)::component_tuple;
    ComponentTuple components_;

    // Chip aliases
    ZilogZ80A&     z80        = std::get<0>(components_);
    ROMChip&       cart_rom   = std::get<1>(components_);
    RAMChip&       system_ram = std::get<2>(components_);
    SEGA_315_5124& vdp        = std::get<3>(components_);
    sn76489_t&     psg        = std::get<4>(components_);

    // Port aliases
    PortControlDB9&     ctrl1_port     = std::get<5>(components_);
    PortControlDB9&     ctrl2_port     = std::get<6>(components_);
    PortExpansion&      cartridge_port = std::get<7>(components_);
    PortCompositeVideo& video_port     = std::get<8>(components_);
    PortAudioMono&      audio_port     = std::get<9>(components_);

    SMSBoard() : Board(kSMSManifest) {}
};
// ============================================================================
// Sega Master System
// ============================================================================

class SegaSMSSystem : public System {
public:
    SegaSMSSystem();
    ~SegaSMSSystem() override;

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
    using Bus       = MemoryBus<SMSBusSpec>;
    using PT        = PackingTraits<SMSBusSpec>;
    using MainBoard = SMSBoard;
    Bus       bus_;
    MainBoard board_;

    // ── Mapper state ─────────────────────────────────────────────────────
    uint8_t mapper_ctrl_  = 0;
    uint8_t mapper_bank_[3] = {0, 1, 2};  // Default bank mapping

    // ── Video ────────────────────────────────────────────────────────────
    std::unique_ptr<CompositeVideoPort> video_port_;

    // ── Audio ────────────────────────────────────────────────────────────
    uint32_t audio_sample_rate_ = sms_constants::DEFAULT_SAMPLE_RATE;
    std::unique_ptr<AudioPort> audio_port_;

    // ── Joypads ──────────────────────────────────────────────────────────
    uint8_t joypad1_state_ = 0xFF;
    uint8_t joypad2_state_ = 0xFF;

    // ── System state ─────────────────────────────────────────────────────
    bus_state_t pins_         = SMS_BUS_DEFAULT_STATE;
    uint32_t    frame_tstate_counter_ = 0;

    // ── Internal helpers ─────────────────────────────────────────────────
    void configure_bus_memory_map();
    bus_state_t io_tick(bus_state_t pins);
    void update_mapper();
};
