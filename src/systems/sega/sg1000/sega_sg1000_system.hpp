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
#include "core/standard_chips.hpp"
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
// SG-1000 chip manifests
// =============================================================================
//
// SG-1000:
//   Slot 0: Cartridge ROM — up to 48KB at $0000
//   Slot 1: RAM           — 1KB at $C000 (mirrored to $FFFF)
//
// SC-3000:
//   Slot 0: Cartridge ROM — up to 48KB at $0000 (or BASIC ROM)
//   Slot 1: RAM           — 8KB at $C000

inline constexpr auto kSG1000Chips = make_chip_manifest(
    Slot<RAMChip>{0x0000, 65536, 0, "RAM"},
    Slot<ROMChip>{0x0000, 32768, 0, "Cartridge ROM"},
    // Non-bus chips
    Slot<ZilogZ80A>  {0, 0, 0, "Z80A"},
    Slot<TMS9918A>   {0, 0, 0, "TMS9918A"},
    Slot<sn76489_t>  {0, 0, 0, "SN76489"}
);

inline constexpr auto kSC3000Chips = make_chip_manifest(
    Slot<RAMChip>{0x0000, 65536, 0, "RAM"},
    Slot<ROMChip>{0x0000, 32768, 0, "Cartridge/BASIC ROM"},
    // Non-bus chips
    Slot<ZilogZ80A>  {0, 0, 0, "Z80A"},
    Slot<TMS9918A>   {0, 0, 0, "TMS9918A"},
    Slot<sn76489_t>  {0, 0, 0, "SN76489"}
);

// BusTraits
template<SG1000Variant V> struct SG1000BusTraits;

template<> struct SG1000BusTraits<SG1000Variant::SG1000> {
    static constexpr const auto& kManifest = kSG1000Chips;
    using Spec = ManifestBusSpec<kSG1000Chips, 16, 8>;
};

template<> struct SG1000BusTraits<SG1000Variant::SC3000> {
    static constexpr const auto& kManifest = kSC3000Chips;
    using Spec = ManifestBusSpec<kSC3000Chips, 16, 8>;
};

// ============================================================================
// SG-1000 ChipSet — value-typed chips owned by Board
// ============================================================================

struct SG1000Chips : StandardChips<ZilogZ80A, TMS9918A, sn76489_t> {};

// ============================================================================
// Sega SG-1000 / SC-3000 System
// ============================================================================

template<SG1000Variant V>
class SegaSG1000System : public System {
    using Traits = SG1000VariantTraits<V>;
    using BT     = SG1000BusTraits<V>;

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


private:
    // ── Board + bus ──────────────────────────────────────────────────────
    using Bus       = MemoryBus<typename BT::Spec>;
    using PT        = PackingTraits<typename BT::Spec>;
    using MainBoard = Board<typename BT::Spec, SG1000Chips>;
    Bus       bus_;
    MainBoard board_{BT::kManifest};

    // ── Display ──────────────────────────────────────────────────────────
    IndexedFrameBuffer display_;
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
