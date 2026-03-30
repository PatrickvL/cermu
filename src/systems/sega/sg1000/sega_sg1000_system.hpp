#pragma once
/*
 * sega_sg1000_system.hpp — Sega SG-1000 / SC-3000 Emulated System
 *
 * The SG-1000 (1983) was Sega's first home console.
 * The SC-3000 was the computer variant with a full keyboard.
 * Both share the same Z80 + TMS9918A + SN76489 chipset.
 *
 * Templated on SG1000Variant to share code.
 */

#include "systems/sega/sg1000/sega_sg1000_constants.hpp"
#include "core/system.hpp"
#include "core/system_lines.hpp"
#include "core/board.hpp"
#include "core/signal/video_port.hpp"
#include "core/signal/audio_port.hpp"
#include "core/system_chip_visitors.hpp"
#include "chip/cpu/z80/zilog_z80a.hpp"
#include "chip/video/tms9918/tms9918a.hpp"
#include "chip/sound/sn76489/sn76489.hpp"
#include "chip/memory/memory_chip.hpp"
#include "utils/ring_buffer.hpp"
#include <cstdint>
#include <memory>

// ============================================================================
// SG-1000 Variant Template
// ============================================================================

enum class SG1000Variant { SG1000, SC3000 };

template<SG1000Variant V> struct SG1000VariantTraits;

template<> struct SG1000VariantTraits<SG1000Variant::SG1000> {
    static constexpr const char* name         = "Sega SG-1000";
    static constexpr const char* short_name   = "SG-1000";
    static constexpr const char* description  = "Sega SG-1000 — Z80A, TMS9918A, SN76489 (1983)";
    static constexpr const char* data_folder  = "sega_sg1000";
    static constexpr uint32_t    ram_size     = sg1000_constants::RAM_SIZE_SG1000;
    static constexpr bool        has_keyboard = false;
    static std::vector<const char*> get_aliases() {
        return {"SG1000", "SG-1000"};
    }
};

template<> struct SG1000VariantTraits<SG1000Variant::SC3000> {
    static constexpr const char* name         = "Sega SC-3000";
    static constexpr const char* short_name   = "SC-3000";
    static constexpr const char* description  = "Sega SC-3000 — Z80A, TMS9918A, SN76489, keyboard (1983)";
    static constexpr const char* data_folder  = "sega_sg1000";
    static constexpr uint32_t    ram_size     = sg1000_constants::RAM_SIZE_SC3000;
    static constexpr bool        has_keyboard = true;
    static std::vector<const char*> get_aliases() {
        return {"SC3000", "SC-3000"};
    }
};

// ============================================================================
// SG-1000 default bus state
// ============================================================================

#define SG1000_BUS_DEFAULT_STATE (ZilogZ80A::default_bus_state())

// =============================================================================
// SG-1000 chip declaration — single source of truth
// =============================================================================
//
// Row: X(ctx, type, chip, base, mask, overlay, label, info_label, rom_files)
//
//   Slot 0: RAM      — 64KB at $0000 (effective size set per variant)
//   Slot 1: Cart ROM — 32KB at $0000
//   Slot 2: Z80A     — not bus-mapped
//   Slot 3: TMS9918A — not bus-mapped (I/O port-accessed)
//   Slot 4: SN76489  — not bus-mapped (I/O port-accessed)
//

#define SG1000_FOR_EACH_SYSTEM_CHIP(X, ctx)                                                                \
    X(ctx, ZilogZ80A,   z80,   0x0000,       0, 0, 0, "Z80A",      "Z80A",      nullptr)                  \
    X(ctx, TMS9918A,    vdp,   0x0000,       0, 0, 0, "TMS9918A",  "TMS9918A",  nullptr)                  \
    X(ctx, sn76489_t,   psg,   0x0000,       0, 0, 0, "SN76489",   "SN76489",   nullptr)                  \
    X(ctx, ROMChip,     cart,  0x0000,  0x8000, 0, 0, "Cart ROM",  "Cart ROM",  nullptr)                  \
    X(ctx, RAMChip,     ram,   0x0000, 0x10000, 0, 0, "RAM",       "RAM",       nullptr)

static constexpr size_t kSG1000ChipCount = 0 SG1000_FOR_EACH_SYSTEM_CHIP(CERMU_CHIP_VISITOR_COUNT_ONE, unused);

inline constexpr ChipManifest<kSG1000ChipCount> kSG1000Chips = ChipManifest<kSG1000ChipCount>{{
    SG1000_FOR_EACH_SYSTEM_CHIP(CERMU_CHIP_VISITOR_MANIFEST_ROW, unused)
}};

using SG1000BusSpec = ManifestBusSpec<kSG1000Chips, 16, 8>;

// ============================================================================
// SG-1000 Chips — value-typed chips owned by Board (auto-generated)
// ============================================================================

struct SG1000Chips {
    SG1000_FOR_EACH_SYSTEM_CHIP(CERMU_CHIP_VISITOR_DECLARE_FIELD, unused)
};

// ============================================================================
// Sega SG-1000 / SC-3000 System
// ============================================================================

template<SG1000Variant V>
class SegaSG1000System : public System {
    using Traits = SG1000VariantTraits<V>;

public:
    SegaSG1000System();
    ~SegaSG1000System() override;

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
    using Bus       = MemoryBus<SG1000BusSpec>;
    using PT        = PackingTraits<SG1000BusSpec>;
    using MainBoard = Board<SG1000BusSpec, SG1000Chips>;
    Bus       bus_;
    MainBoard board_{kSG1000Chips};

    // ── Video ────────────────────────────────────────────────────────────
    std::unique_ptr<CompositeVideoPort> video_port_;

    // ── Audio ────────────────────────────────────────────────────────────
    uint32_t audio_sample_rate_ = sg1000_constants::DEFAULT_SAMPLE_RATE;
    std::unique_ptr<AudioPort> audio_port_;

    // ── Joystick ─────────────────────────────────────────────────────────
    uint8_t joypad_state_ = 0xFF;  // Active-low: Up/Down/Left/Right/TL/TR

    // ── System state ─────────────────────────────────────────────────────
    bus_state_t pins_         = SG1000_BUS_DEFAULT_STATE;
    uint32_t    frame_tstate_counter_ = 0;

    // ── Internal helpers ─────────────────────────────────────────────────
    void configure_bus_memory_map();
    bus_state_t io_tick(bus_state_t pins);
};
