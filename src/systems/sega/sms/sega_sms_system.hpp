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
#include "core/core_chips.hpp"
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
// Slot 0: Cartridge ROM — up to 512KB at $0000 (banked)
// Slot 1: System RAM   — 8KB at $C000 (mirrored to $E000)

inline constexpr auto kSMSChips = make_chip_manifest(
    Slot<ROMChip>{0x0000, 524288, 0, "Cartridge ROM", 0, 16384, 0, 0, {}},
    Slot<RAMChip>{0xC000,   8192, 0x1FFF, "System RAM", 0, 0, 0, 0, {}},
    // Non-bus chips
    Slot<ZilogZ80A>      {0, 0, 0, "Z80A",          0, 0, 0, 0, {}},
    Slot<SEGA_315_5124>  {0, 0, 0, "315-5124 VDP",  0, 0, 0, 0, {}},
    Slot<sn76489_t>      {0, 0, 0, "SN76489 PSG",   0, 0, 0, 0, {}}
);

using SMSBusSpec  = ManifestBusSpec<kSMSChips, 16, 8>;

// ============================================================================
// SMS Chips — value-typed chips owned by Board
// ============================================================================

struct SMSChips : CoreChips<ZilogZ80A, SEGA_315_5124, sn76489_t> {
    SMSChips() : CoreChips<ZilogZ80A, SEGA_315_5124, sn76489_t>{
        ZilogZ80A{}, SEGA_315_5124{}, sn76489_t{SN76489Variant::SEGA_PSG}, {}} {}
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


private:
    // ── Board + bus ──────────────────────────────────────────────────────
    using Bus       = MemoryBus<SMSBusSpec>;
    using PT        = PackingTraits<SMSBusSpec>;
    using MainBoard = Board<SMSBusSpec, SMSChips>;
    Bus       bus_;
    MainBoard board_{kSMSChips};

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
